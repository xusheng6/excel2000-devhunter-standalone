/* Dev Hunter — standalone reimplementation of the Excel 2000 MSOWC.DLL easter
 * egg.  Faithful ("close enough") pseudo-3D racer: recovered palette, decrypted
 * developer-name credits, the game's float LCG, and the original's 10-degree /
 * focal-864 road projection.  Win32 + GDI, 8-bit framebuffer -> palette -> DIB.
 */
#ifndef DH_H
#define DH_H
#include <windows.h>

#define SCR_W 640
#define SCR_H 480
#define FB_PITCH 640

extern unsigned char *g_fb;        /* 8-bit palette-index framebuffer */
extern double g_rand_seed;         /* LCG seed (0.0), sub_3c8d36e0 */
double dh_rand(void);              /* float in [0,1) */
unsigned int dh_time(void);

/* ---- game state ---- */
#define MAX_ENEMY 8
#define MAX_BULLET 48
#define MAX_DEBRIS 220

typedef struct { int active; float z, x, vx; int color, hp; } Enemy;
typedef struct { int active; float z, x; } Bullet;
typedef struct { int active; float z, x, vz, vx, life; } Debris;

#define MAX_WALL 12
typedef struct { int active; float z, x, len; } Wall;   /* lane-end barrier */

typedef struct {
    /* input flags (set by wndproc) */
    int k_gas, k_brake, k_left, k_right, k_fire, k_weapon, k_names;
    /* player / camera */
    float speed;        /* forward speed */
    float posx;         /* lateral world position (0 = centre) */
    float steer;        /* current steer */
    float scroll;       /* world distance travelled */
    float fire_cd;      /* fire cooldown */
    float wpn_cd;
    float recover;      /* >0 : crash recovery (invulnerable, stopped) */
    /* entities */
    Enemy  enemy[MAX_ENEMY];
    Bullet bullet[MAX_BULLET];
    Debris debris[MAX_DEBRIS];
    Wall   wall[MAX_WALL];
    float  spawn_timer;
    float  wall_timer;
    int    score, hitflash;
    int    frame;
    unsigned int last_ms;
    int    started;
} Game;
extern Game G;

void game_init(void);
void game_tick(float dt);
void game_render(void);

/* dev-name road texture strip (built in game_init from the decrypted credits) */
#define STRIP_W 200
#define STRIP_H 2048
extern unsigned char *g_strip;     /* STRIP_W x STRIP_H, 0 = empty, else letter */
extern int g_strip_used;

#endif
