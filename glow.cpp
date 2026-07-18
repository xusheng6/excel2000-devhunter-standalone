/* Dev Hunter — glow / headlight lighting pass (part of game_tick = sub_3c7dcb73).
 *
 * Faithful translation of re/hlil.txt lines 2110-2407.  Runs after the road
 * scroll loop and before the bullet-fire code.  It splats light into the road
 * lighting grid (field 0x910f4, normally pointing at the 64x64 grid 0x890ec)
 * for the player headlight cone, enemy/debris glows, a dusk fill pass, and the
 * muzzle glow — mostly visible in the night section.
 *
 * FP compares decoded (see MEMORY / dh.h):  the status word after fcom/fstsw is
 *   X = (a<b)<<8 | unord<<0xa | (a==b)<<0xe [|0x3800/0x3000]
 * and `X:1.b` is the *high* byte (bits 8-15), so test_bit(,0)=C0=(a<b) and
 * test_bit(,6)=C3=(a==b).  The 0x3800/0x3000 fill only sets C1/C2-area bits that
 * neither test looks at, so they are ignored.  Thus:
 *     test_bit(,6)                      -> a == b
 *     !test_bit(,6) && !test_bit(,0)    -> a >  b
 *     test_bit(,6) || test_bit(,0)      -> a <= b
 *
 * splat(X,Y,amt) == sub_3c7dca79(arg1,X,Y,amt): bilinear-accumulates amt into the
 * grid pointed at by field 0x910f4.  X,Y are the var_60/var_58 `.q` doubles.
 *
 * NOTE on var_10: the 0x9a6ac muzzle pass (hlil line 2352) needs var_10, the
 * per-frame dt computed in game_tick.  The requested signature is
 * `void glow_pass(int var_14)` (no var_10), and other files must not be touched,
 * so the caller passes it through the file-scope `glow_var_10` below (set it to
 * game_tick's var_10 immediately before calling glow_pass).  See summary.
 */
#include <math.h>
#include "devhunter.h"

/* frame dt (game_tick var_10); caller sets this before glow_pass(). */
float glow_var_10 = 0.0f;

void glow_pass(int var_14)
{
    /* =====================================================================
     * (1) headlight cone  (hlil 2110-2152) — gated by 0x9a6a8 and no crash
     * ===================================================================== */
    if (g->fx_names != 0) {
        if (g->recover == 0.0f) {                    /* recovery timer == 0 */
            double x87_r7_188 = (double)g->speed * 0.05;      /* camX*0.05 */
            int i_13 = 1;
            int i_32 = 1;
            double var_38_1;

            g->splat_target = (unsigned char*)g->lighting;                 /* splat into lighting grid */
            var_38_1 = 66.0 - (double)g->scroll_row - (double)g->camY + 26.0;

            do {
                int eax_164 = -i_13;                   /* neg.d(i_13) */
                int j_7 = eax_164 - 1;
                if (eax_164 - 1 < i_13 + 1) {
                    do {
                        int ecx_50 = (j_7 == eax_164 - 1 || j_7 == i_13 + 1) ? 2 : 0;
                        double var_58_7 = var_38_1 - (float)i_32;
                        double var_60_1 = (float)j_7 * 0.5f + (x87_r7_188 + 31.5);
                        int amt = (int)((long long)((eax_164 + 0x14) * var_14) / -20)
                                  >> ecx_50;
                        splat(var_60_1, var_58_7, amt);
                        j_7 += 1;
                    } while (j_7 < i_13 + 1);
                }
                i_13 += 1;
                i_32 = i_13;
            } while (i_13 < 0x13);
        }
    }

    /* splat target = lighting grid for the enemy/debris/dusk passes */
    g->splat_target = (unsigned char*)g->lighting;

    /* =====================================================================
     * (2) enemy glow  (hlil 2159-2234) — one splat cluster per live enemy.
     *     Loop count = (var_14>>8)/2 - 3 (round toward zero), so it scales
     *     with the ambient shade (0 by day, up to 7 enemies at full night).
     * ===================================================================== */
    {
        int eax_174 = var_14 >> 8;
        int edx_32  = eax_174 >> 31;                   /* sign of eax_174 */
        int count   = ((eax_174 - edx_32) >> 1) - 3;   /* live-enemy glows, scales w/ dusk */
        if (count > 0) {
            for (int k = 0; k < count; k++) {
                Enemy &en = g->enemy[k];
                if (en.timer > 20.0f) {                /* enemy alive */
                    int edi_20 = g->scroll_row;
                    double var_40_2 = (double)en.y * 0.05 + 31.5;
                    double x87_r7_204 = (double)edi_20;
                    double x87_r6_33 = 64.0 - (double)en.x
                                       + (double)g->frame - x87_r7_204;
                    double var_38_2 = x87_r6_33;
                    double x87_r6_34 = x87_r6_33 + x87_r7_204;   /* 64 - x + E */
                    int edi_21;
                    int i_13, i_33;

                    if (!(x87_r6_34 < 80.0))
                        edi_21 = 0x13;
                    else
                        edi_21 = FTOL(var_38_2) + edi_20 - 0x3c;

                    i_13 = 1;
                    i_33 = 1;
                    if (edi_21 > 1) {
                        do {
                            int eax_181 = -i_13;
                            int j_8 = eax_181 - 1;
                            if (eax_181 - 1 < i_13 + 1) {
                                do {
                                    int ecx_52 = (j_8 == eax_181 - 1 || j_8 == i_13 + 1) ? 2 : 0;
                                    double var_58_8 = var_38_2 - (float)i_33;
                                    double var_60_2 = (float)j_8 * 0.5f + var_40_2;
                                    int amt = (int)((long long)((eax_181 + 0x14) * var_14) / -20)
                                              >> ecx_52;
                                    splat(var_60_2, var_58_8, amt);
                                    j_8 += 1;
                                } while (j_8 < i_13 + 1);
                            }
                            i_13 += 1;
                            i_33 = i_13;
                        } while (i_13 < edi_21);
                    }
                }
            }
        }
    }

    /* =====================================================================
     * (3) debris glow  (hlil 2236-2262) — unconditional; 0x64 iterations,
     *     stride 0xa floats (0x28 bytes = every other debris entry).
     * ===================================================================== */
    {
        /* every other debris entry (stride 0x28 bytes = 2 records), 0x64 of them */
        for (int k = 0; k < 0x64; k++) {
            Debris &db = g->debris[2*k];
            if (db.life > 0.0f) {                      /* life > 0 */
                double i_23 = (double)g->frame + 64.0
                              - (double)db.y - (double)g->scroll_row;
                double i_19 = (double)db.x * 0.05 + 31.5;
                splat(i_19, i_23, -var_14);            /* neg.d(var_14) */
            }
        }
    }

    /* =====================================================================
     * (4) dusk fill pass  (hlil 2264-2339) — only while shade < 0x1400.
     *     8 slots: slots 0-6 are the live enemies, slot 7 is the player.
     *     Each emits a 7x(i_31-1) splat block into the lighting grid.
     * ===================================================================== */
    if (var_14 < 0x1400) {
        g->splat_target = (unsigned char*)g->lighting;

        for (int i_16 = 0; i_16 < 8; i_16++) {
            double var_40_3 = 0.0, var_38_3 = 0.0;
            int i_31 = 0;
            int do_splat = 0;

            if (i_16 >= 7) {                            /* slot 7: player glow */
                var_40_3 = (double)g->speed * 0.05 + 31.5;
                var_38_3 = 66.0 - (double)g->scroll_row - (double)g->camY + 26.0;
                i_31 = 6;                               /* label_3c7de3a3 */
                do_splat = 1;
            } else {                                    /* slots 0-6: enemies */
                Enemy &en = g->enemy[i_16];
                if (en.timer > 0.0f) {                  /* timer > 0 */
                    int edi_24 = g->scroll_row;
                    double x87_r7_223, x87_r6_40, x87_r6_41;
                    var_40_3 = (double)en.y * 0.05 + 31.5;
                    x87_r7_223 = (double)edi_24;
                    x87_r6_40 = 64.0 - (double)en.x
                                + (double)g->frame - x87_r7_223;
                    var_38_3 = x87_r6_40;
                    x87_r6_41 = x87_r6_40 + x87_r7_223; /* 64 - x + E */

                    if (x87_r6_41 <= 110.0) {
                        i_31 = 6;                       /* label_3c7de3a3 */
                    } else {
                        int t = 0x72 - FTOL(var_38_3) - edi_24;
                        i_31 = (t >= 0) ? t : 0;        /* nullptr -> 0 */
                    }
                    do_splat = 1;
                }
            }

            if (do_splat) {                             /* label_3c7ddfe8 */
                int var_c_1 = 1;
                if (i_31 > 1) {
                    do {
                        int j_9;
                        for (j_9 = -4; j_9 < 3; j_9 += 1) {
                            int ecx_56 = (j_9 == 8 || j_9 == 2) ? 2 : 0;
                            double var_58_9 = (float)var_c_1 + var_38_3;
                            double var_60_3 = (float)j_9 * 0.5f + var_40_3;
                            int amt = (int)((long long)(0x1e00 - var_14) / 4) >> ecx_56;
                            splat(var_60_3, var_58_9, amt);
                        }
                        var_c_1 += 1;
                    } while (var_c_1 < i_31);
                }
            }

        }
    }

    /* =====================================================================
     * (5) muzzle glow  (hlil 2341-2407) — gated by 0x9a6ac and no crash.
     *     Splats into the *elevation* grid 0x850ec (not 0x890ec); a 2x3 block
     *     of dots just ahead of the car, intensity = FTOL(dt * 5120).
     * ===================================================================== */
    if (g->fx_weapon != 0) {
        if (g->recover == 0.0f) {
            int eax_202;
            double bx = (double)g->speed * 0.05;      /* camX*0.05 */
            double ry = 66.0 - (double)g->scroll_row + (double)g->camY;
            g->splat_target = (unsigned char*)g->elevation;    /* target = elevation grid */
            eax_202 = FTOL(glow_var_10 * 5120.0f);     /* dt * 5120 */

            splat(bx + 31.0, ry + 27.0, eax_202);
            splat(bx + 31.0, ry + 28.0, eax_202);
            splat(bx + 31.0, ry + 29.0, eax_202);
            splat(bx + 31.5, ry + 27.0, eax_202);
            splat(bx + 31.5, ry + 28.0, eax_202);
            splat(bx + 31.5, ry + 29.0, eax_202);
        }
    }
}
