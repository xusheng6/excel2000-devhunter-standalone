/* game_render — sub_3c7de915, translated from asm (re/asm.txt 2773-3528).
 * Projects a straight flat 0x41x0x41 road mesh (camera pitch 10deg, focal 864,
 * centre (0x140,0xce)) and fills each cell as a shaded/textured trapezoid via
 * raster_span; then cars / bullets / debris.  DirectDraw lock/flip is replaced
 * by writing straight into g_fb (surface base g->surf, pitch g->surf_pitch).
 *
 * this = S ; arg1 = `shade` (the ambient `var_14` computed in tick).
 */
#include <string.h>
#include <math.h>
#include "devhunter.h"

/* projection constants (qwords at 0x3c7ad2xx) */
#define COS10  0.98480775301220802
#define SIN10  0.17364817766693041
#define K3473  3.4729635533386071
#define K1969  19.696155060244161
#define FOCAL  864.0

void game_render(int shade){
    unsigned char *fb = g->surf;       /* g_fb */
    int pitch = g->surf_pitch;
    static int V[0x41*0x41 + 4];        /* projected X grid (var_4360) */
    static int SY[0x41 + 2];            /* per-row screen Y (var_158..) */
    double camX = g->camX, camY = g->camY;
    double leftBase  = (-640.0 - camX) * 48.0;   /* left road edge  */
    double rightBase = ( 640.0 - camX) * 48.0;   /* right road edge */
    int var8 = g->scroll_row;
    int row, col, i_1, edi;

    /* ---- projection: 0x41 rows ---- */
    for(row=0; row<0x41; row++){
        double dist  = (double)((var8 & 0x3f) - 1) + camY;
        double denom = 86.0 - (dist*COS10 + K3473);
        int screenY  = FTOL((dist*SIN10 + K1969)*FOCAL / denom);
        int eax12, eax13, var10, ecx;
        SY[row] = screenY - 0xce;
        eax12 = FTOL(leftBase  / denom);
        eax13 = FTOL(rightBase / denom);
        var10 = 0; ecx = eax13 - eax12;
        for(col=0; col<0x40; col++){
            int sx = var10/0x3f + eax12 + 0x140;
            if(sx < -0x28)      sx = sx - eax12 + eax13;
            else if(sx > 0x2a8) sx = sx - eax13 + eax12;
            V[1 + row*0x41 + col] = sx;
            var10 += ecx;
        }
        var8++;
    }

    /* ---- road mesh raster ---- */
    { int dc = g->scroll_row;
      for(i_1 = -dc; i_1 < 0x31 - dc; i_1++){
        int ebx = i_1 & 0x3f;
        int base = 1 + ebx*0x41;
        for(edi=0; edi<0x3f; edi++){
            int M = base + edi;
            int left = V[M], right = V[M+1];
            if(left < right && left <= 0x27f && right > 0){
                int r0 = ebx*0x40 + edi;
                int r1 = ((ebx+1)&0x3f)*0x40 + edi;
                g->cur_tex = g->texptr[r0];        /* cell texture */
                raster_span(fb, pitch,
                    SY[ebx], SY[ebx+1],             /* y top / bottom */
                    V[M], V[M+1],                    /* x top-left / top-right */
                    V[M+0x41], V[M+0x42],            /* x bottom-left / bottom-right */
                    g->lighting[r0], g->lighting[r0+1],   /* shade top l/r */
                    g->lighting[r1], g->lighting[r1+1]);  /* shade bottom l/r */
            }
        }
      }
    }

    /* car models, bullets, debris (sub_3c7de915 tail) */
    g_render_shade = shade;
    render_cars(fb, pitch);
    (void)camY;
}
