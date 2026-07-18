/* Dev Hunter — FAITHFUL port (translating MSOWC.DLL 1:1).
 *
 * The game's C++ `this` object is a flat ~0x9a96c byte buffer.  We keep it as a
 * raw byte array `S` and access every field through the exact DLL offset, so
 * each `*(arg1 + 0xNNN)` in the disassembly maps to a macro here.  MUST be built
 * 32-bit (the game stores `this`-relative 4-byte pointers inside its fields).
 *
 * Function name map (DLL base 0x3c6d0000):
 *   game_init   sub_3c7df259    game_tick  sub_3c7dcb73    game_render sub_3c7de915
 *   raster_span sub_3c7de66f    raster_tri sub_3c7de4a2
 *   spawn_burst sub_3c7de3b7    splat      sub_3c7dca79
 *   dh_rand     sub_3c8d36e0     (ftol = sub_3c6db234)
 */
#ifndef DH_H
#define DH_H
#include <windows.h>

#define SCR_W 640
#define SCR_H 480
#define FB_PITCH 640
#define STATE_SIZE 0x100000u

extern unsigned char *S;            /* the game state (this) */
extern unsigned char *g_fb;         /* 8-bit palette-index framebuffer (surface) */
extern double g_rand_seed;          /* data_3c924290, init 0.0 */

/* ---- field accessors (offset-exact) ---- */
#define f32(o)  (*(float*)(S+(o)))
#define i32(o)  (*(int*)(S+(o)))
#define u32(o)  (*(unsigned int*)(S+(o)))
#define u8(o)   (*(unsigned char*)(S+(o)))
#define i8(o)   (*(signed char*)(S+(o)))
#define i16(o)  (*(short*)(S+(o)))
#define u16(o)  (*(unsigned short*)(S+(o)))
#define pp(o)   (*(unsigned char**)(S+(o)))         /* pointer-into-self field */
#define IA(b,i) (*(int*)(S+(b)+((i)<<2)))           /* dword array elem */
#define PA(b,i) (*(unsigned char**)(S+(b)+((i)<<2)))

#define FTOL(x) ((int)(x))                          /* sub_3c6db234: trunc toward 0 */

/* ---- helpers (helpers.c) ---- */
double dh_rand(void);                               /* sub_3c8d36e0 */
int    dh_iabs(int v);                              /* sub_3c7897e4 */
double dh_fabs(double v);                           /* sub_3c8f74d4 */
int    raster_span(unsigned char*fb,int a3,int a4,int a5,int a6,int a7,int a8,
                   int a9,int a10,int a11,int a12,int a13);  /* sub_3c7de66f */
int    raster_tri(unsigned char*fb,int a3);         /* sub_3c7de4a2 */
int    spawn_burst(float x,float y,float vx,float vy); /* sub_3c7de3b7 */
int    splat(double sx,double sy,int amt);          /* sub_3c7dca79 */

/* ---- core (init.c/tick.c/render.c) ---- */
void   game_init(void);                             /* sub_3c7df259 */
void   game_tick(void);                             /* sub_3c7dcb73 */
void   game_render(int shade);                      /* sub_3c7de915 */
void   entity_update(float var_10, float var_18);   /* tick entity loops (entity.c) */
void   spawn_pattern(int edi_10);                   /* tick lane/enemy pattern (spawn.c) */
void   barrier_row(int edi_10, int eax_59);         /* tick lane-transition row (spawn.c) */
void   render_cars(unsigned char *fb, int pitch);   /* car/bullet/debris draw (render_cars.c) */
extern int g_render_shade;                          /* game_render arg1 for render_cars */
void   glow_pass(int var_14);                       /* headlight/entity glow splats (glow.c) */
extern float glow_var_10;                            /* dt passthrough for glow_pass */

unsigned int dh_time(void);

#endif
