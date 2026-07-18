/* Dev Hunter — entity update loops (part of game_tick = sub_3c7dcb73).
 *
 * Faithful translation of re/hlil.txt lines 1250-1686:
 *   (1) enemy update   — g->enemy[0..6]   (stride 0x28)
 *   (2) bullet update  — g->bullet[0..19]  (stride 0x14)
 *   (3) debris update  — g->debris[0..199] (stride 0x14)
 *
 * var_10 = frame dt ; var_18 = 1 - dt*0.04  (both computed in game_tick).
 *
 * Note the game's field naming is its own: for a car, `y` is the lateral
 * (steer) coordinate integrated by `vy`, and `x` is the forward coordinate
 * integrated by `vx` (the HLIL cursor edi_6 was based at enemy.vx).
 */
#include <math.h>
#include "devhunter.h"

void entity_update(float var_10, float var_18)
{
    /* ---------------- (1) enemies ---------------------------------------- */
    for (int i = 0; i < 7; i++) {
        Enemy &en = g->enemy[i];
        if (!(en.timer > 0.0f)) continue;               /* inactive slot */

        /* movement (1262-1265) */
        en.x = (float)((double)var_10 * (double)en.vx + (double)en.x);
        float ny = (float)((double)var_10 * (double)en.vy + (double)en.y);
        en.y = ny;

        if (en.timer > 20.0f) {                         /* homing (1278-1285) */
            if (!(en.target_y == ny))
                en.vy = (float)((((double)en.target_y - (double)ny)
                                 * 0.100000001f + (double)en.vy) * 0.5f);
        } else {                                        /* decay (1287-1289) */
            en.timer = (float)((double)en.timer - (double)var_10);
            en.vx = (float)((double)var_18 * (double)en.vx);
            en.vy = (float)((double)var_18 * (double)en.vy);
        }

        /* despawn if forward pos outside [E-0x41, E+0xc] (1291-1315) */
        {
            int E = g->frame;
            int cond0 = ((double)(E + 0xc) < (double)en.x);
            int gt2   = 0;
            if (!cond0)
                gt2 = ((double)(E - 0x41) > (double)en.x);
            if (cond0 || gt2)
                en.timer = 0.0f;
        }

        /* proximity lerp toward player (1317-1395) */
        if (en.timer > 20.0f) {
            if (dh_fabs((double)en.x - (double)(g->frame - 0x1a)) < 5.0) {
                if (dh_fabs((double)en.y - (double)g->speed) < 40.0) {
                    double mx, mn, d14, d15;
                    en.vy = g->steer;
                    if (!(g->throttle < en.vx)) mx = g->throttle; else mx = en.vx;
                    if (g->throttle <= en.vx)   mn = g->throttle; else mn = en.vx;
                    d14 = (double)(float)mx - mn;
                    if (!((double)(g->frame - 0x1a) < (double)en.x)) {
                        en.vx = (float)(d14 * 0.100000001f + mn);
                        d15 = d14 * 0.899999976f;
                    } else {
                        en.vx = (float)(d14 * 0.899999976f + mn);
                        d15 = d14 * 0.100000001f;
                    }
                    g->throttle = (float)(d15 + mn);
                }
            }
        }

        /* cross-enemy separation (1397-1493) */
        for (int j = 0; j < 7; j++) {
            if (i == j) continue;
            Enemy &ot = g->enemy[j];
            if (!(en.timer > 20.0f)) continue;
            if (!(ot.timer > 20.0f)) continue;
            if (!(dh_fabs((double)en.x - (double)ot.x) < 5.0)) continue;
            if (!(dh_fabs((double)en.y - (double)ot.y) < 40.0)) continue;
            double mx, mn, d21, d22;
            if (!(ot.vx < en.vx)) mx = ot.vx; else mx = en.vx;
            if (ot.vx <= en.vx)   mn = ot.vx; else mn = en.vx;
            d21 = (double)(float)mx - mn;
            if ((double)en.x <= (double)ot.x) {
                en.vx = (float)(d21 * 0.100000001f + mn);
                d22 = d21 * 0.899999976f;
            } else {
                en.vx = (float)(d21 * 0.899999976f + mn);
                d22 = d21 * 0.100000001f;
            }
            ot.vx = (float)(d22 + mn);
        }

        /* grid-cell lookup -> collision with map / bullets (1495-1544) */
        {
            int col = FTOL((double)en.y * 0.050000000000000003 + 31.5);
            int E   = g->frame;
            int row = FTOL(((64.0 - (double)en.x + (double)E
                             - (double)g->scroll_row) + 2.0));
            int cell = col + ((row & 0x3f) << 6);

            if (g->texptr[cell] == g->proto(TEX_GRASS)) {
                if (en.timer > 20.0f) {
                    int spawn;
                    if ((double)(E - 0x1a) <= (double)en.x)
                        spawn = 1;
                    else
                        spawn = !(en.target_y == en.y);
                    if (spawn) {
                        en.vy    = (float)((double)en.vy * 0.25);
                        en.timer = 20.0f;
                        spawn_burst(en.y, (float)((double)en.x - 3.0f),
                                    en.vy, en.vx);
                    } else {
                        en.timer = 0.0f;
                    }
                }
            }

            /* wall bounce : flip target-y (1546-1575) */
            if (en.timer > 20.0f
                && (g->elevation[cell] > 0x1400 || g->elevation[cell + 1] > 0x1400)) {
                if ((double)(E - 0x1a) > (double)en.x) {
                    if (!(en.y < 0.0f))
                        en.target_y = 300.0f;
                    else
                        en.target_y = -300.0f;
                }
            }
        }
    }

    /* ---------------- (2) bullets --------------------------------------- */
    for (int bi = 0; bi < 0x14; bi++) {
        Bullet &bl = g->bullet[bi];
        if (!(bl.life > 0.0f)) continue;                /* life > 0 */

        bl.y    = (float)((double)var_10 * (double)bl.vy + (double)bl.y);
        bl.x    = (float)((double)var_10 * (double)bl.vx + (double)bl.x);
        bl.life = (float)((double)bl.life - (double)var_10);

        for (int j = 0; j < 7; j++) {
            Enemy &en = g->enemy[j];
            if (!(en.timer > 20.0f)) continue;          /* enemy alive */
            if (!(dh_fabs((double)en.y - (double)bl.x) < 40.0)) continue;
            if (!(dh_fabs((double)en.x - (double)bl.y) < 10.0)) continue;
            en.timer = (float)((double)en.timer - 20.0f);
            bl.life  = 0.0f;
            if (en.timer <= 20.0f)                       /* killed -> burst */
                spawn_burst(en.y, (float)((double)en.x - 3.0f), en.vy, en.vx);
        }
    }

    /* ---------------- (3) debris ---------------------------------------- */
    for (int di = 0; di < 0xc8; di++) {
        Debris &d = g->debris[di];
        if (!(d.life > 0.0f)) continue;
        d.y    = (float)((double)var_10 * (double)d.vy + (double)d.y);
        d.x    = (float)((double)var_10 * (double)d.vx + (double)d.x);
        d.vy   = (float)((double)var_18 * (double)d.vy);
        d.vx   = (float)((double)var_18 * (double)d.vx);
        d.life = (float)((double)d.life - (double)var_10);
    }
}
