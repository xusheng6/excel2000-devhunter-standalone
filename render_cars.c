/* render_cars.c — continuation of game_render (sub_3c7de915) tail.
 *
 * Faithful translation of hlil.txt 3178-3562 / asm.txt 3040-3528: after the road
 * mesh (render.c) this draws the 7 enemy cars + the player car (projected 16-vertex
 * models, triangulated and filled via raster_span / raster_tri), then the bullets
 * and debris as plotted pixels.  The DirectDraw lock/unlock/flip that follows in the
 * DLL (asm 3c7df1b3..) is intentionally omitted — fb is already provided.
 *
 * The FP-stack vertex projection was traced from the ASM (asm 3040-3166); the HLIL
 * garbles the x87 stack loads as float.t(0x3c7ad08f).
 *
 * arg1 of game_render == the ambient car-shade base (asm [ebp-0x18]/HLIL var_1c_3).
 * render_cars()'s signature is fixed to (fb,pitch), so that value is taken from the
 * file-scope global below; game_render should set it before calling (defaults to 0,
 * which is what the road part already assumes since it ignores `shade`).
 */
#include <math.h>
#include "dh.h"
#include "data.h"          /* g_tab + TAB(absoluteAddr) : car vertex & mesh tables */

/* ---- projection constants (qwords in the DLL) ---- */
#define COS10   0.98480775301220802    /* 0x3c7ad298 */
#define SIN10   0.17364817766693041    /* 0x3c7ad280 */
#define K3473   3.4729635533386071     /* 0x3c7ad290 (bullet/debris denom bias) */
#define K1969   19.696155060244161     /* 0x3c7ad278 (bullet/debris num  bias) */
#define FOCAL   864.0                  /* 0x3c7ad270 */
#define XFOCAL  (-48.0)                /* 0x3c7ad250 */
#define MULY    0.10000000000000001    /* 0x3c74feb0 */
#define MULX    0.125                  /* 0x3c7ad260 */
#define BASE20  20.0                   /* 0x3c7ad258 */
#define C86     86.0                   /* 0x3c7ad288 */
#define PLAYEROFF 28.0                 /* 0x3c7ad268 */
#define RECTHRESH 20.0                 /* 0x3c7ad1a8 */
#define DBGCOL  (-0.75)                /* 0x3c7ad248 */

/* ---- DLL table addresses (indexed through TAB / g_tab) ---- */
#define VTAB        0x3c7acf72u         /* 16-vertex car models, stride 0x30/pose  */
#define MESH1       0x3c7ad062u         /* 7-byte trapezoid records (raster_span)   */
#define MESH1_END   0x3c7ad08cu
#define MESH2       0x3c7ad08fu         /* 0xa-byte triangle records (raster_tri)   */
#define MESH2_END   0x3c7ad107u
#define MESH2_BODY  0x3c7ad0a3u         /* force-body-texture threshold             */

#define SB(a)  ((int)(signed char)TAB(a))   /* movsx byte [a] */

int g_render_shade = 0;    /* game_render arg1 (ambient car-shade base) */

void render_cars(unsigned char *fb, int pitch)   /* fb = pp(0x34), pitch = i32(0x20) */
{
    double camX = f32(0x9c), camY = f32(0xa0);
    int    e0    = i32(0xe0);
    int    shade = g_render_shade;                 /* arg1 */
    int    edi_2;

    /* ================= per-entity loop: edi_2 = 0..6 enemies, 7 = player ========= */
    for (edi_2 = 0; edi_2 < 8; edi_2++) {
        unsigned char *body, *window, *roof;
        double CARX, CARbaseY, dist14;
        int    var_1c_3 = shade;                   /* this car's shade base */
        int    pose;                               /* var_20_3 */
        int    V[16][4];                           /* var_150: {Xleft,Yscr,Xright,Yscr} */
        unsigned int ebx, i_3;
        int    v;

        if (edi_2 >= 7) {
            /* ---- player (asm 3c7df1f0) ---- */
            CARX     = f32(0xc0);                   /* fld [esi+0xc0] -> CARX slot */
            body     = S + 0x70e8;
            window   = S + 0xe0e8;
            roof     = S + 0x150e8;
            CARbaseY = (double)e0 + camY - PLAYEROFF;
            dist14   = (double)e0 + camY - CARbaseY;    /* == 28.0 */
            /* player recovery flash uses field 0x9a6c0, flash when != 0 */
            if (f32(0x9a6c0) != 0.0f)
                var_1c_3 = ((0x1e - FTOL((double)f32(0x9a6c0))) << 8) + shade;
            pose = 0;
        } else {
            /* ---- enemy (asm 3c7deb94) ---- */
            int cbase = 0x9945c + edi_2 * 0x28;
            unsigned char *bank;
            CARX     = f32(cbase + 4);              /* 0x99460 */
            CARbaseY = f32(cbase + 8);              /* 0x99464 */
            /* skip inactive car: field 0x99474 <= 0 (fcomp;jbe) */
            if (!(f32(cbase + 0x18) > 0.0f))
                continue;
            bank     = S + (i32(cbase + 0x1c) << 0xc);   /* (color<<12)+S, 0x99478 */
            body     = bank + 0x70e8;
            window   = bank + 0xe0e8;
            roof     = bank + 0x150e8;
            dist14   = (double)e0 + camY - CARbaseY;
            /* enemy recovery flash: field 0x99474 < 20 */
            if (f32(cbase + 0x18) < RECTHRESH)
                var_1c_3 = ((0x1e - FTOL((double)f32(cbase + 0x18))) << 8) + shade;
            pose = i32(cbase + 0x20);               /* 0x9947c */
        }

        /* ---- project the 16 model vertices (asm 3c7dec87..3c7ded51) ---- */
        ebx = VTAB + (unsigned)pose * 0x30;
        for (v = 0; v < 16; v++) {
            int    b0 = SB(ebx - 2);                /* movsx [ebx-2] */
            int    b1 = SB(ebx - 1);                /* movsx [ebx-1] */
            int    b2 = SB(ebx);                    /* movsx [ebx]   */
            double varc  = (double)b1 * MULX + dist14;              /* b1*0.125 + dist14 */
            double t     = BASE20 - (double)b2 * MULY;             /* 20 - b2*0.1       */
            double denom = C86 - (t * SIN10 + varc * COS10);
            int Xleft  = 0x140 - FTOL(((double)b0  + CARX - camX) * XFOCAL / denom);
            int Yscr   = FTOL((t * COS10 + varc * SIN10) * FOCAL / denom) - 0xce;
            int Xright = 0x140 - FTOL(((double)(-b0) + CARX - camX) * XFOCAL / denom);
            V[v][0] = Xleft;                        /* [edi-4] screenX left  */
            V[v][1] = Yscr;                         /* [edi]   screenY       */
            V[v][2] = Xright;                       /* [edi+4] screenX right */
            V[v][3] = Yscr;                         /* [edi+8] screenY copy  */
            ebx += 3;
        }

        /* ---- mesh pass 1: 7-byte trapezoid records -> raster_span (asm 3c7ded5b) --- */
        for (i_3 = MESH1; i_3 < MESH1_END; i_3 += 7) {
            int v0 = SB(i_3 - 2), v1 = SB(i_3 - 1);
            int texsel = SB(i_3 + 4);
            int s0, s1, s2, s3;

            if      (texsel == 0) pp(0x850e8) = body;
            else if (texsel == 1) pp(0x850e8) = window;
            else if (texsel == 2) pp(0x850e8) = roof;

            if (V[v0][1] == V[v1][1])               /* Yscr(v0)==Yscr(v1): degenerate */
                continue;

            s0 = (SB(i_3 + 0) << 8) + var_1c_3;  if (s0 > 0x1e80) s0 = 0x1e80;
            s1 = (SB(i_3 + 1) << 8) + var_1c_3;  if (s1 > 0x1e80) s1 = 0x1e80;
            s2 = (SB(i_3 + 2) << 8) + var_1c_3;  if (s2 > 0x1e80) s2 = 0x1e80;
            s3 = (SB(i_3 + 3) << 8) + var_1c_3;  if (s3 > 0x1e80) s3 = 0x1e80;

            raster_span(fb, pitch,
                        V[v0][1], V[v1][1],         /* y top / bottom       */
                        V[v0][0], V[v0][2] + 1,     /* Xleft(v0), Xright(v0)+1 */
                        V[v1][0], V[v1][2] + 1,     /* Xleft(v1), Xright(v1)+1 */
                        s0, s1, s2, s3);
        }

        /* ---- mesh pass 2: 0xa-byte records, two mirrored triangles (asm 3c7dee4e) - */
        for (i_3 = MESH2; i_3 < MESH2_END; i_3 += 0xa) {
            int v0 = SB(i_3 - 3), v1 = SB(i_3 - 2), v2 = SB(i_3 - 1);
            int texsel2 = SB(i_3 + 6);
            int cross, sh0, sh1, sh2;

            if      (texsel2 == 0) pp(0x850e8) = body;
            else if (texsel2 == 1) pp(0x850e8) = S + 0x1c0e8;

            if ((edi_2 == 7 || pose == 3) && i_3 < MESH2_BODY)
                pp(0x850e8) = body;

            /* triangle A: left side (Xleft, Yscr) */
            i32(0x9a948) = V[v0][0]; i32(0x9a94c) = V[v0][1];
            i32(0x9a950) = V[v1][0]; i32(0x9a954) = V[v1][1];
            i32(0x9a958) = V[v2][0]; i32(0x9a95c) = V[v2][1];
            cross = (i32(0x9a95c) - i32(0x9a954)) * (i32(0x9a948) - i32(0x9a950))
                  - (i32(0x9a958) - i32(0x9a950)) * (i32(0x9a94c) - i32(0x9a954));
            if (cross <= 0) {                       /* test edx; jg skip -> fill if <=0 */
                sh0 = (SB(i_3 + 0) << 8) + var_1c_3; if (sh0 > 0x1e80) sh0 = 0x1e80;
                sh1 = (SB(i_3 + 1) << 8) + var_1c_3; if (sh1 > 0x1e80) sh1 = 0x1e80;
                sh2 = (SB(i_3 + 2) << 8) + var_1c_3; if (sh2 > 0x1e80) sh2 = 0x1e80;
                i32(0x9a960) = sh0; i32(0x9a964) = sh1; i32(0x9a968) = sh2;
                raster_tri(fb, pitch);
            }

            /* triangle B: right side (Xright, Yscr copy) */
            i32(0x9a948) = V[v0][2]; i32(0x9a94c) = V[v0][3];
            i32(0x9a950) = V[v1][2]; i32(0x9a954) = V[v1][3];
            i32(0x9a958) = V[v2][2]; i32(0x9a95c) = V[v2][3];
            cross = (i32(0x9a95c) - i32(0x9a954)) * (i32(0x9a948) - i32(0x9a950))
                  - (i32(0x9a958) - i32(0x9a950)) * (i32(0x9a94c) - i32(0x9a954));
            if (cross >= 0) {                       /* js skip -> fill if >=0 */
                sh0 = (SB(i_3 + 3) << 8) + var_1c_3; if (sh0 > 0x1e80) sh0 = 0x1e80;
                sh1 = (SB(i_3 + 4) << 8) + var_1c_3; if (sh1 > 0x1e80) sh1 = 0x1e80;
                sh2 = (SB(i_3 + 5) << 8) + var_1c_3; if (sh2 > 0x1e80) sh2 = 0x1e80;
                i32(0x9a960) = sh0; i32(0x9a964) = sh1; i32(0x9a968) = sh2;
                raster_tri(fb, pitch);
            }
        }
    }

    /* ================= bullets: array 0x9957c, 0x14 entries, stride 0x14 ========= */
    {
        unsigned int b;
        int k;
        for (k = 0, b = 0x9957c; k < 0x14; k++, b += 0x14) {
            double zc, denom;
            int Yscr, Xscr;
            if (!(f32(b + 4) > 0.0f))               /* alive test (fcomp;jbe) */
                continue;
            zc    = (double)e0 - (double)f32(b) + camY;          /* e0 - Y + camY */
            denom = C86 - (zc * COS10 + K3473);
            Yscr  = FTOL((zc * SIN10 + K1969) * FOCAL / denom) - 0xce;
            Xscr  = 0x140 - FTOL(((double)f32(b - 4) - camX) * XFOCAL / denom);
            i32(0x9a80c) = Yscr;                    /* stored unconditionally */
            if (Yscr > -1 && Yscr < 0x1e0) {        /* note: no X bound check for bullets */
                unsigned char *p = fb + pitch * Yscr + Xscr;
                p[0] = 0x88;
                p[1] = 0x8c;
            }
        }
    }

    /* ================= debris: array 0x99714, 0xc8 entries, stride 0x14 ========== */
    {
        unsigned int d;
        int k;
        for (k = 0, d = 0x99714; k < 0xc8; k++, d += 0x14) {
            double zc, denom;
            int Yscr, Xscr;
            if (!(f32(d) > 0.0f))                   /* alive test */
                continue;
            zc    = (double)e0 - (double)f32(d - 4) + camY;
            denom = C86 - (zc * COS10 + K3473);
            Yscr  = FTOL((zc * SIN10 + K1969) * FOCAL / denom) - 0xce;
            Xscr  = 0x140 - FTOL(((double)f32(d - 8) - camX) * XFOCAL / denom);
            if (Xscr > 0 && Xscr < 0x27f && Yscr > -1 && Yscr < 0x1de) {
                int cval = FTOL((double)f32(d) * DBGCOL);        /* fld [edi]*-0.75 */
                int cl   = (0x80 - cval) & 0xff;                 /* cl */
                int ch   = (((0x80 - cval) >> 8) & 0xff) | 0x88; /* or ch,0x88 */
                unsigned char *p = fb + pitch * Yscr + Xscr;
                p[0]         = (unsigned char)cl;   /* mov word [eax],cx  */
                p[1]         = (unsigned char)ch;
                p[pitch]     = (unsigned char)cl;   /* mov word [edx+eax],cx (next row) */
                p[pitch + 1] = (unsigned char)ch;
            }
        }
    }
}
