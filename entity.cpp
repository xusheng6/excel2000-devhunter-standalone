/* Dev Hunter — entity update loops (part of game_tick = sub_3c7dcb73).
 *
 * Faithful translation of re/hlil.txt lines 1250-1686:
 *   (1) enemy update      @ 0x9945c, 7 entries, stride 0x28 (0xa floats)
 *   (2) bullet update     @ 0x99578, 0x14 entries, stride 0x14 (5 floats)
 *   (3) debris update     @ 0x9970c, 0xc8 entries, stride 0x14 (5 floats)
 *
 * var_10 = frame dt ; var_18 = 1 - dt*0.04  (both computed in game_tick).
 * In the HLIL arg1 is the game object; here it is the global base pointer S,
 * so *(arg1 + 0xNN) becomes a macro on that offset.  A local `float*` mirrors
 * each HLIL cursor (edi_6/ebx_1/edi_7/i_13) so `edi_6[k]` maps 1:1.
 *
 * Enemy struct (stride 0x28, base 0x9945c = edi_6[0]):
 *   edi_6[-2] 0x99454  ax   (unused here)
 *   edi_6[-1] 0x99458  vy
 *   edi_6[ 0] 0x9945c  vx
 *   edi_6[ 1] 0x99460  y  (integrated by vy)
 *   edi_6[ 2] 0x99464  x  (integrated by vx)
 *   edi_6[ 5] 0x99470  target-y (homing goal, set to +/-300)
 *   edi_6[ 6] 0x99474  timer/health (>20 = alive/active, decays otherwise)
 */
#include <math.h>
#include "devhunter.h"

void entity_update(float var_10, float var_18)
{
    /* ---------------- (1) enemies : 0x9945c, i_1 = 0..6, stride 0x28 ------ */
    int i_1 = 0;
    do {
        float *edi_6 = (float *)(S + 0x9945c + i_1 * 0x28);

        if (edi_6[6] > 0.0f) {                              /* line 1261: >0  */
            float var_2c_3;

            /* movement (1262-1265) */
            edi_6[2] = (float)((double)var_10 * (double)edi_6[0] + (double)edi_6[2]);
            var_2c_3 = (float)((double)var_10 * (double)edi_6[-1] + (double)edi_6[1]);
            edi_6[1] = var_2c_3;

            if (edi_6[6] > 20.0f) {                         /* line 1273: >20 */
                /* homing toward edi_6[5] (1278-1285) */
                if (!(edi_6[5] == var_2c_3))
                    edi_6[-1] = (float)((((double)edi_6[5] - (double)var_2c_3)
                                         * 0.100000001f + (double)edi_6[-1]) * 0.5f);
            } else {                                        /* decay (1287-1289) */
                edi_6[6] = (float)((double)edi_6[6] - (double)var_10);
                edi_6[0]  = (float)((double)var_18 * (double)edi_6[0]);
                edi_6[-1] = (float)((double)var_18 * (double)edi_6[-1]);
            }

            /* despawn if x outside [E-0x41, E+0xc] (1291-1315) */
            {
                int E = i32(0xe0);
                int cond0 = ((double)(E + 0xc) < (double)edi_6[2]);   /* E+12 < x */
                int gt2   = 0;
                if (!cond0)
                    gt2 = ((double)(E - 0x41) > (double)edi_6[2]);    /* E-0x41 > x */
                if (cond0 || gt2)
                    edi_6[6] = 0.0f;
            }

            /* proximity lerp toward player (1317-1395) */
            if (edi_6[6] > 20.0f) {
                if (dh_fabs((double)edi_6[2] - (double)(i32(0xe0) - 0x1a)) < 5.0) {
                    if (dh_fabs((double)edi_6[1] - (double)f32(0xc0)) < 40.0) {
                        double mx, mn, d14, d15;

                        edi_6[-1] = f32(0xb8);
                        if (!(f32(0xbc) < edi_6[0])) mx = f32(0xbc); else mx = edi_6[0];  /* max */
                        if (f32(0xbc) <= edi_6[0])   mn = f32(0xbc); else mn = edi_6[0];  /* min */
                        d14 = (double)(float)mx - mn;
                        if (!((double)(i32(0xe0) - 0x1a) < (double)edi_6[2])) {
                            edi_6[0] = (float)(d14 * 0.100000001f + mn);
                            d15 = d14 * 0.899999976f;
                        } else {
                            edi_6[0] = (float)(d14 * 0.899999976f + mn);
                            d15 = d14 * 0.100000001f;
                        }
                        f32(0xbc) = (float)(d15 + mn);
                    }
                }
            }

            /* cross-enemy separation (1397-1493) : j = 0..6, stride 0x28 */
            {
                int j = 0;
                float *ebx_1 = (float *)(S + 0x9945c);
                do {
                    if (i_1 != j) {
                        if (edi_6[6] > 20.0f) {
                            if (ebx_1[6] > 20.0f) {
                                if (dh_fabs((double)edi_6[2] - (double)ebx_1[2]) < 5.0) {
                                    if (dh_fabs((double)edi_6[1] - (double)ebx_1[1]) < 40.0) {
                                        double mx, mn, d21, d22;

                                        if (!(ebx_1[0] < edi_6[0])) mx = ebx_1[0]; else mx = edi_6[0]; /* max */
                                        if (ebx_1[0] <= edi_6[0])   mn = ebx_1[0]; else mn = edi_6[0]; /* min */
                                        d21 = (double)(float)mx - mn;
                                        if ((double)edi_6[2] <= (double)ebx_1[2]) {
                                            edi_6[0] = (float)(d21 * 0.100000001f + mn);
                                            d22 = d21 * 0.899999976f;
                                        } else {
                                            edi_6[0] = (float)(d21 * 0.899999976f + mn);
                                            d22 = d21 * 0.100000001f;
                                        }
                                        ebx_1[0] = (float)(d22 + mn);
                                    }
                                }
                            }
                        }
                    }
                    j += 1;
                    ebx_1 += 0xa;
                } while (j < 7);
            }

            /* grid-cell lookup -> collision with map / bullets (1495-1544) */
            {
                int eax_35 = FTOL((double)edi_6[1] * 0.050000000000000003 + 31.5);
                int eax_36 = i32(0xe0);
                int eax_37 = FTOL(((64.0 - (double)edi_6[2] + (double)eax_36
                                    - (double)i32(0xdc)) + 2.0));
                int ebx_3 = eax_35 + ((eax_37 & 0x3f) << 6);

                if (PA(0x8d0ec, ebx_3) == S + 0x20e8) {
                    if (edi_6[6] > 20.0f) {
                        int spawn;
                        if ((double)(eax_36 - 0x1a) <= (double)edi_6[2])
                            spawn = 1;                       /* (E-0x1a) <= x */
                        else
                            spawn = !(edi_6[5] == edi_6[1]);
                        if (spawn) {
                            edi_6[-1] = (float)((double)edi_6[-1] * 0.25);
                            edi_6[6]  = 20.0f;               /* 0x41a00000 */
                            spawn_burst(edi_6[1], (float)((double)edi_6[2] - 3.0f),
                                        edi_6[-1], edi_6[0]);
                        } else {
                            edi_6[6] = 0.0f;
                        }
                    }
                }

                /* wall bounce : flip target-y (1546-1575) */
                if (edi_6[6] > 20.0f
                    && (IA(0x850ec, ebx_3) > 0x1400 || IA(0x850f0, ebx_3) > 0x1400)) {
                    if ((double)(eax_36 - 0x1a) > (double)edi_6[2]) {
                        if (!(edi_6[1] < 0.0f))
                            edi_6[5] = 300.0f;               /* 0x43960000 */
                        else
                            edi_6[5] = -300.0f;              /* 0xc3960000 */
                    }
                }
            }
        }

        i_1 += 1;
    } while (i_1 < 7);

    /* ---------------- (2) bullets : 0x99578, 0x14 down, stride 0x14 ------- */
    {
        float *edi_7 = (float *)(S + 0x99578);
        int i_29 = 0x14;
        int i_2;
        do {
            if (edi_7[2] > 0.0f) {                          /* life > 0 */
                float *ebx_4 = (float *)(S + 0x99474);      /* enemy timer field */
                int j_11 = 7;
                int j_1;

                edi_7[1] = (float)((double)var_10 * (double)edi_7[-1] + (double)edi_7[1]);
                edi_7[0] = (float)((double)var_10 * (double)edi_7[-2] + (double)edi_7[0]);
                edi_7[2] = (float)((double)edi_7[2] - (double)var_10);

                do {
                    if (ebx_4[0] > 20.0f) {                 /* enemy alive */
                        if (dh_fabs((double)ebx_4[-5] - (double)edi_7[0]) < 40.0) {
                            if (dh_fabs((double)ebx_4[-4] - (double)edi_7[1]) < 10.0) {
                                ebx_4[0] = (float)((double)ebx_4[0] - 20.0f);
                                edi_7[2] = 0.0f;
                                if (ebx_4[0] <= 20.0f)      /* killed -> burst */
                                    spawn_burst(ebx_4[-5],
                                                (float)((double)ebx_4[-4] - 3.0f),
                                                ebx_4[-7], ebx_4[-6]);
                            }
                        }
                    }
                    ebx_4 += 0xa;
                    j_1 = j_11;
                    j_11 -= 1;
                } while (j_1 != 1);
            }
            edi_7 += 5;
            i_2 = i_29;
            i_29 -= 1;
        } while (i_2 != 1);
    }

    /* ---------------- (3) debris : 0x9970c, 0xc8 down, stride 0x14 -------- */
    {
        float *i_13 = (float *)(S + 0x9970c);
        int i_35 = 0xc8;
        int i_3;
        do {
            if (i_13[2] > 0.0f) {
                i_13[1]  = (float)((double)var_10 * (double)i_13[-1] + (double)i_13[1]);
                i_13[0]  = (float)((double)var_10 * (double)i_13[-2] + (double)i_13[0]);
                i_13[-1] = (float)((double)var_18 * (double)i_13[-1]);
                i_13[-2] = (float)((double)var_18 * (double)i_13[-2]);
                i_13[2]  = (float)((double)i_13[2] - (double)var_10);
            }
            i_13 += 5;
            i_3 = i_35;
            i_35 -= 1;
        } while (i_3 != 1);
    }
}
