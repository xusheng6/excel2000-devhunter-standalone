/* Dev Hunter — the game object as a C++ class, matching the original.
 *
 * MSOWC.DLL's Dev Hunter is C++: a large `this` object (thiscall) whose fields
 * are the offsets we reverse-engineered and whose methods are game_init/tick/
 * render/...  This class reproduces that: member VARIABLES at the exact original
 * offsets (so init_state.bin still loads and the internal this-relative pointers
 * stay valid) and member FUNCTIONS for the game code.  Every field offset is
 * checked with static_assert, so a layout mistake cannot compile.
 *
 * MUST be built 32-bit (the object stores 4-byte this-relative pointers in its
 * own fields).  Non-virtual methods don't change layout, so sizeof/offsets equal
 * the original C++ class.
 */
#ifndef DEVHUNTER_H
#define DEVHUNTER_H
#include <windows.h>
#include <stddef.h>

#define SCR_W 640
#define SCR_H 480
#define FB_PITCH 640
#define STATE_SIZE 0x9a96cu

/* ---- entity records (see offsets below) ---- */
struct Enemy {          /* stride 0x28; array starts at 0x99454 */
    float slot0;        /* +0x00  also holds the PREVIOUS enemy's `pose` */
    float vy;           /* +0x04 */
    float vx;           /* +0x08  (the DLL code's "edi_6" base points here) */
    float y;            /* +0x0c */
    float x;            /* +0x10 */
    float f3;           /* +0x14 */
    int   lane;         /* +0x18 */
    float target_y;     /* +0x1c  homing goal (+/-300 on wall bounce) */
    float timer;        /* +0x20  alive if > 20, decays otherwise, 0 = dead */
    int   color;        /* +0x24  colour-scheme index */
    /* `pose` (would be +0x28) lands in the next Enemy's slot0 — see poseOf() */
};

struct Bullet {         /* stride 0x14; array-of-5-floats at 0x99574 */
    float dvx;          /* +0x00 (0x99574) */
    float x;            /* +0x04 (0x99578) */
    float y;            /* +0x08 (0x9957c) */
    float life;         /* +0x0c (0x99580) */
    float f4;           /* +0x10 (0x99584) */
};

struct Debris {         /* stride 0x14; array-of-5-floats at 0x99704 */
    float vx;           /* +0x00 (0x99704) */
    float vy;           /* +0x04 (0x99708) */
    float x;            /* +0x08 (0x9970c) */
    float y;            /* +0x0c (0x99710) */
    float life;         /* +0x10 (0x99714) */
};

/* road-cell texture prototype offsets (pointers stored in texptr[] point here) */
#define TEX_PLAIN  0xe8u     /* plain road            */
#define TEX_LANE   0x10e8u   /* lane marking          */
#define TEX_GRASS  0x20e8u   /* grass / lane-end barrier (crash cell) */

struct DevHunter {
    /* ---- 0x00: COM / DirectDraw / timing (mostly unused by the port) ---- */
    void *ddraw_create;              /* 0x00 */
    void *time_get, *time_begin, *time_end; /* 0x04,08,0c */
    char  _lock_a[0x10];             /* 0x10  DDSURFACEDESC head */
    int   surf_pitch;                /* 0x20  lPitch  (== FB_PITCH) */
    char  _lock_b[0x10];             /* 0x24 */
    unsigned char *surf;             /* 0x34  lpSurface (== g_fb) */
    char  _hdr[0x64];                /* 0x38 -> 0x9c */

    /* ---- camera / player physics ---- */
    float camX;                      /* 0x9c  lateral camera (= speed) */
    float camY;                      /* 0xa0  forward odometer (consumed by scroll) */
    int   key_gas, key_brake;        /* 0xa4,0xa8  right / left */
    int   key_up, key_down;          /* 0xac,0xb0 */
    int   frame_timer;               /* 0xb4  last timeGetTime */
    float steer;                     /* 0xb8  [-10,10]  (gas/brake) */
    float throttle;                  /* 0xbc  [0,2.6]   (up/down) */
    float speed;                     /* 0xc0  = camX */
    float lateral;                   /* 0xc4 */
    unsigned char *text_cur[2];      /* 0xc8,0xcc  credit/taunt stream cursors */
    int   text_stream;               /* 0xd0  which stream (0/1) */
    int   text_toggle;               /* 0xd4  repeat/advance word */
    int   text_col;                  /* 0xd8  starting road column for the word */
    int   scroll_row;                /* 0xdc  road ring row (mod 0x40) */
    int   frame;                     /* 0xe0  frame / distance counter */
    char  _pad_e4[4];                /* 0xe4 */

    /* ---- 0xe8: cell-texture prototypes + 3D letter tiles (0x1000-byte banks) ---- */
    unsigned char tex[0x850ec - 0xe8]; /* 0xe8 -> 0x850ec */

    /* ---- 64x64 road grids ---- */
    int   elevation[0x1000];         /* 0x850ec  road height -> shadow */
    int   lighting[0x1000];          /* 0x890ec  ambient + glow */
    unsigned char *texptr[0x1000];   /* 0x8d0ec  per-cell texture pointer */

    unsigned char *text_buf[2];      /* 0x910ec,0x910f0  decrypted credit/taunt buffers */
    int  *splat_target;              /* 0x910f4  glow-splat destination grid */
    int   lut[(0x990f8 - 0x910f8)/4];/* 0x910f8  texel+shade -> palette LUT */
    int   colorscheme[(0x99454 - 0x990f8)/4]; /* 0x990f8 */

    /* ---- entities ---- */
    Enemy  enemy[7];                 /* 0x99454 (7 enemies) */
    char   _e_gap[0x99574 - (0x99454 + 7*sizeof(Enemy))]; /* holds enemy[6].pose too */
    Bullet bullet[0x14];             /* 0x99574 (ends exactly at debris) */
    Debris debris[0xc8];             /* 0x99704 (ends exactly at debris_idx) */

    /* ---- game state block ---- */
    int   debris_idx;                /* 0x9a6a4  debris write index */
    int   fx_names;                  /* 0x9a6a8  'H' name overlay toggle */
    int   fx_weapon;                 /* 0x9a6ac  'O' weapon */
    int   fx_fire;                   /* 0x9a6b0  Space fire */
    float daynight;                  /* 0x9a6b4  day/night cycle timer */
    float spawn_at;                  /* 0x9a6b8  last enemy-spawn time */
    float fire_at;                   /* 0x9a6bc  last fire time */
    float recover;                   /* 0x9a6c0  crash recovery ( >0 = spun out ) */
    int   spawn_count;               /* 0x9a6c4  spawn countdown */
    int   _f9a6c8;                   /* 0x9a6c8 */
    int   lane_cur[6];               /* 0x9a6cc  current per-lane occupancy */
    char  _p1[8];
    int   lane_tgt[6];               /* 0x9a6ec  target  per-lane occupancy */
    char  _p2[0xc];
    int   lane_pair[6*2];            /* 0x9a710/0x9a714  lane source/dest pairs */
    char  _p3[4];
    int   barrier_grid[(0x9a7e8-0x9a744)/4]; /* 0x9a744  barrier cells */
    int   barrier_phase;             /* 0x9a7e8  transition rows remaining */
    unsigned char spawn_tab[0x9a7fc-0x9a7ec]; /* 0x9a7ec  spawn parameter bytes */
    unsigned char spawn_pat;         /* 0x9a7fc  current pattern index */
    char  _p4[0x9a80c-0x9a7fd];
    int   f_9a80c;                   /* 0x9a80c */
    char  _p5[0x9a920-0x9a810];      /* HMODULEs / ddraw surfaces / atom */
    int   letter_font;               /* 0x9a920  HFONT for the letter atlas */
    char  _p6[0x9a948-0x9a924];      /* ddraw palette / surfaces */
    int   tri[9];                    /* 0x9a948..0x9a968  raster triangle verts (ends 0x9a96c) */

    /* helpers (non-virtual methods don't change the layout) */
    int   poseOf(int i) { return *(int*)((char*)&enemy[i] + 0x28); }  /* pose is packed
                             into the next enemy's leading slot (compiler packing) */
    void  setPose(int i, int v) { *(int*)((char*)&enemy[i] + 0x28) = v; }
    unsigned char *proto(unsigned int off) { return (unsigned char*)this + off; }
};

/* every field pinned to its exact original offset */
#define CK(field,off) static_assert(offsetof(DevHunter,field)==(off), #field)
CK(surf_pitch,0x20); CK(surf,0x34); CK(camX,0x9c); CK(camY,0xa0);
CK(key_gas,0xa4); CK(key_down,0xb0); CK(frame_timer,0xb4);
CK(steer,0xb8); CK(throttle,0xbc); CK(speed,0xc0); CK(lateral,0xc4);
CK(text_cur,0xc8); CK(text_stream,0xd0); CK(scroll_row,0xdc); CK(frame,0xe0);
CK(tex,0xe8); CK(elevation,0x850ec); CK(lighting,0x890ec); CK(texptr,0x8d0ec);
CK(text_buf,0x910ec); CK(splat_target,0x910f4); CK(lut,0x910f8);
CK(enemy,0x99454); CK(bullet,0x99574); CK(debris,0x99704);
CK(debris_idx,0x9a6a4); CK(fx_names,0x9a6a8); CK(fx_fire,0x9a6b0);
CK(daynight,0x9a6b4); CK(recover,0x9a6c0); CK(spawn_count,0x9a6c4);
CK(lane_cur,0x9a6cc); CK(lane_tgt,0x9a6ec); CK(lane_pair,0x9a710);
CK(barrier_grid,0x9a744); CK(barrier_phase,0x9a7e8); CK(spawn_tab,0x9a7ec);
CK(spawn_pat,0x9a7fc); CK(letter_font,0x9a920); CK(tri,0x9a948);
#undef CK

/* the singleton game object (allocated in main; init_state.bin loads over it) */
extern DevHunter *g;
extern unsigned char *g_fb;
extern double g_rand_seed;

/* ---------------------------------------------------------------------------
 * Legacy raw-buffer bridge.
 *
 * The port was first written against a flat byte buffer `S` with offset macros
 * (`i32(0x34)`, `f32(0xc0)`, ...).  `S` now simply aliases the class object
 * (`S == (unsigned char*)g`), so the two views are the same memory.  Named
 * fields (`g->surf`) are the readable form; these macros remain only for the
 * handful of deep texture-bank / computed-array accesses that don't correspond
 * to a single named field.
 * ------------------------------------------------------------------------- */
extern unsigned char *S;
#define f32(o)  (*(float*)(S+(o)))
#define i32(o)  (*(int*)(S+(o)))
#define u32(o)  (*(unsigned int*)(S+(o)))
#define u8(o)   (*(unsigned char*)(S+(o)))
#define i8(o)   (*(signed char*)(S+(o)))
#define i16(o)  (*(short*)(S+(o)))
#define u16(o)  (*(unsigned short*)(S+(o)))
#define pp(o)   (*(unsigned char**)(S+(o)))
#define IA(b,i) (*(int*)(S+(b)+((i)<<2)))
#define PA(b,i) (*(unsigned char**)(S+(b)+((i)<<2)))
#define FTOL(x) ((int)(x))          /* sub_3c6db234: truncate toward 0 */

/* ---- helpers (helpers.cpp) ---- */
double dh_rand(void);               /* sub_3c8d36e0 */
int    dh_iabs(int v);              /* sub_3c7897e4 */
double dh_fabs(double v);           /* sub_3c8f74d4 */
int    raster_span(unsigned char*fb,int a3,int a4,int a5,int a6,int a7,int a8,
                   int a9,int a10,int a11,int a12,int a13);  /* sub_3c7de66f */
int    raster_tri(unsigned char*fb,int a3);         /* sub_3c7de4a2 */
int    spawn_burst(float x,float y,float vx,float vy); /* sub_3c7de3b7 */
int    splat(double sx,double sy,int amt);          /* sub_3c7dca79 */
unsigned int dh_time(void);

/* ---- core (init/tick/render/...) ---- */
void   game_init(void);             /* init.cpp (loads init_state.bin) */
void   game_tick(void);             /* tick.cpp */
void   game_render(int shade);      /* render.cpp */
void   entity_update(float dt, float decay);       /* entity.cpp */
void   spawn_pattern(int row);                     /* spawn.cpp */
void   barrier_row(int row, int phase);            /* spawn.cpp */
void   render_cars(unsigned char *fb, int pitch);  /* render_cars.cpp */
extern int   g_render_shade;        /* game_render arg passed to render_cars */
void   glow_pass(int shade);        /* glow.cpp */
extern float glow_var_10;           /* dt passthrough for glow_pass */

#endif
