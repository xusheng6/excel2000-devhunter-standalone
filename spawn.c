/* Dev Hunter — enemy-spawn / lane-pattern / barrier-row generation.
 *
 * Faithful translation of game_tick (sub_3c7dcb73), re/hlil.txt 1794-2051,
 * cross-checked against re/asm.txt 3c7ddb5a-3c7ddce5.  Both functions run
 * inside the road-scroll loop; `edi_10 = (-i32(0xdc)) & 0x3f` is the current
 * road-ring row index and `eax_59 = i32(0x9a7e8)` is the barrier countdown.
 *
 * spawn_pattern : hlil 1795-1970 (the `i32(0x9a6c4) s< 1 && i32(0xd4) != 0`
 *                 guard body — the caller has already tested the guard and
 *                 done `i32(0x9a6c4) -= 1`).  Picks a fresh lane/enemy pattern.
 * barrier_row   : hlil 1972-2051 (the whole `else`/eax_59!=0 branch, including
 *                 the leading `i32(0x9a7e8) = eax_59 - 1`).  Paints one
 *                 lane-transition / barrier row.
 *
 * State layout (best inference):
 *   0x9a6cc[6]  current per-lane occupancy (bitmask of active sub-lanes)
 *   0x9a6ec[6]  target  per-lane occupancy for the pattern being spawned
 *   0x9a710/0x9a714  lane-target pairs, stride 8 bytes: [-1]=source lane,
 *                    [0]=dest lane (0x9a710 first, 0x9a714 second of each pair)
 *   0x9a744/0x9a748/0x9a758/0x9a75c/0x9a770  barrier grid cells (nibble flags)
 *   0x9946c+     enemy records (base 0x9945c, stride 0x28): [0]=lane idx,
 *                [1]=target-y (float), [2]=timer/health (float, >0 = alive)
 */
#include <string.h>
#include "dh.h"

/* hlil 1795-1970 : pick a new lane / enemy pattern. */
void spawn_pattern(int edi_10)
{
    (void)edi_10;   /* row index is unused by this branch */

    /* reset spawn countdown, choose pattern row eax_131 in [0,6) */
    i32(0x9a6c4) = 0x28 - FTOL(dh_rand() * -80.0);
    int eax_131 = FTOL(dh_rand() * 6.0);
    u8(0x9a7fc) = (unsigned char)eax_131;

    /* target occupancy 0x9a6ec[0..5] from the chosen pattern-row bitmask */
    {
        int i_6 = 1;
        int *edx_25 = (int *)(S + 0x9a6ec);
        do {
            int edi_17 = (1 << (i_6 & 0xff)) & (int)(signed char)u8(0x9a7ec + eax_131);
            i_6 += 1;
            *edx_25 = edi_17;
            edx_25 = &edx_25[1];
        } while (i_6 < 7);
    }

    /* build the lane-target pairs 0x9a714 and the max transition distance */
    int var_8_1 = 0;
    {
        int i_7 = 1;
        int *ebx_16 = (int *)(S + 0x9a714);
        int *var_2c_19 = (int *)(S + 0x9a6cc);
        i32(0xd8) = ((int)(signed char)u8(0x9a7f4 + eax_131) << 2) + 0x14;
        do {
            ebx_16[-1] = i_7;
            ebx_16[0]  = i_7;
            int eax_136 = *var_2c_19;
            if (eax_136 != var_2c_19[8]) {
                if (eax_136 == 0) {
                    /* lane starts: snap dest to nearest currently-occupied lane */
                    int var_20_2 = 0xa;
                    int j_2 = 1;
                    int *var_24_5 = (int *)(S + 0x9a6cc);
                    do {
                        if (*var_24_5 != 0) {
                            int d = dh_iabs(j_2 - i_7);
                            if (d < var_20_2) { var_20_2 = d; ebx_16[0] = j_2; }
                        }
                        var_24_5 = &var_24_5[1];
                        j_2 += 1;
                    } while (j_2 < 7);
                    if (var_8_1 <= var_20_2) var_8_1 = var_20_2;
                } else {
                    /* lane ends: snap source to nearest target-occupied lane */
                    int var_20_1 = 0xa;
                    int j_3 = 1;
                    int *var_24_4 = (int *)(S + 0x9a6ec);
                    do {
                        if (*var_24_4 != 0) {
                            int d = dh_iabs(j_3 - i_7);
                            if (d < var_20_1) { var_20_1 = d; ebx_16[-1] = j_3; }
                        }
                        var_24_4 = &var_24_4[1];
                        j_3 += 1;
                    } while (j_3 < 7);
                    if (var_8_1 <= var_20_1) var_8_1 = var_20_1;
                }
            }
            i_7 += 1;
            var_2c_19 = &var_2c_19[1];
            ebx_16 = &ebx_16[2];
        } while (i_7 < 7);
    }

    /* re-home the 7 live enemies onto their remapped lane target-y */
    {
        int *ecx_30 = (int *)(S + 0x9946c);
        int i_36 = 7;
        int i_8;
        do {
            if (((float *)ecx_30)[2] > 0.0f) {
                int eax_157 = *(int *)(S + ((*ecx_30) << 3) + 0x9a710) - 1;
                *ecx_30 = eax_157;
                ((float *)ecx_30)[1] = (float)((double)(eax_157 * 0x54) - 210.0);
            }
            ecx_30 = &ecx_30[0xa];
            i_8 = i_36;
            i_36 -= 1;
        } while (i_8 != 1);
    }

    /* schedule the barrier row and clear the barrier grid */
    i32(0x9a7e8) = var_8_1 << 2;
    memset(S + 0x9a748, 0, 0xa0);

    /* fill the barrier grid describing how each lane merges/splits */
    {
        int edx_29 = 1;
        int *var_c    = (int *)(S + 0x9a770);
        int *var_20_3 = (int *)(S + 0x9a6cc);
        int i_9 = 5;
        int *eax_161  = (int *)(S + 0x9a714);
        int *var_28_5 = (int *)(S + (var_8_1 << 2) + 0x9a744);
        do {
            int ecx_34 = eax_161[-1];
            if (ecx_34 != edx_29 || eax_161[0] != edx_29) {
                int j_4 = 0;
                if (*var_20_3 == 0) {
                    int ecx_45 = eax_161[0];
                    if (ecx_45 <= edx_29) {
                        if (edx_29 - ecx_45 > 0) {
                            int *ecx_49 = &var_c[-0xa];
                            do {
                                ecx_49[5] |= 3;
                                ecx_49[0] |= 0xc;
                                j_4 += 1;
                                ecx_49 -= 4;            /* -0x10 bytes */
                            } while (j_4 < edx_29 - eax_161[0]);
                        }
                    } else if (ecx_45 - edx_29 > 0) {
                        int *ecx_47 = var_c;
                        do {
                            ecx_47[-5] |= 6;
                            ecx_47[0]  |= 9;
                            j_4 += 1;
                            ecx_47 = &ecx_47[6];
                        } while (j_4 < eax_161[0] - edx_29);
                    }
                } else if (ecx_34 <= edx_29) {
                    if (edx_29 - ecx_34 > 0) {
                        int *ecx_44 = var_28_5;
                        do {
                            ecx_44[5] |= 9;
                            ecx_44[0] |= 6;
                            j_4 += 1;
                            ecx_44 -= 6;                /* -0x18 bytes */
                        } while (j_4 < edx_29 - eax_161[-1]);
                    }
                } else if (ecx_34 - edx_29 > 0) {
                    int i_27 = i_9;
                    do {
                        int ecx_40 = i_27 - j_4;
                        i_27 += 5;
                        int ecx_41 = ecx_40 + var_8_1;
                        IA(0x9a744, ecx_41) |= 0xc;
                        IA(0x9a758, ecx_41) |= 3;
                        j_4 += 1;
                    } while (j_4 < eax_161[-1] - edx_29);
                }
            } else {
                /* lane straight-through: mark whole run occupied/empty */
                int *ecx_36 = &var_c[-5];
                int j_12 = 5;
                int j_5;
                do {
                    int ebx_19 = *var_20_3;
                    *ecx_36 = (ebx_19 != 0) ? 0xf : 0;
                    ecx_36 = &ecx_36[1];
                    j_5 = j_12;
                    j_12 -= 1;
                } while (j_5 != 1);
            }
            i_9 += 5;
            var_20_3 = &var_20_3[1];
            var_28_5 = &var_28_5[5];
            var_c = &var_c[5];
            edx_29 += 1;
            eax_161 = &eax_161[2];
        } while (i_9 < 0x23);
    }
}

/* hlil 1972-2051 : paint one lane-transition / barrier row. */
void barrier_row(int edi_10, int eax_59)
{
    i32(0x9a7e8) = eax_59 - 1;
    int temp0_1 = (eax_59 - 1) / 4;

    /* build the 6 lane cell-texture columns for this barrier row */
    {
        int i_10 = 1;
        int *ebx_5 = (int *)(S + (temp0_1 << 2) + 0x9a75c);
        do {
            int ecx_4 = *ebx_5;
            int eax_63 = 0;
            int edi_11;
            if (ecx_4 == 0) {
                edi_11 = 0;
                eax_63 = 1;
            } else {
                edi_11 = 3;
                if (ecx_4 == 3)      edi_11 = 1;
                else if (ecx_4 == 6) edi_11 = 2;
                else if (ecx_4 != 9) edi_11 = (ecx_4 != 0xc ? 1 : 0) + 4;
            }
            int j_6 = eax_63 * 6;
            int *edx_10 = (int *)(S + ((eax_63 + (((edi_10 << 4) + i_10) << 2)) << 2) + 0x8d128);
            do {
                int eax_67 = (i32(0x9a7e8) & 3) + j_6;
                j_6 += 6;
                *edx_10 = IA(0x990f8, edi_11 + eax_67 * 6);
                edx_10 = &edx_10[1];
            } while (j_6 < 0x1e);
            i_10 += 1;
            ebx_5 = &ebx_5[5];
        } while (i_10 < 7);
    }

    /* paint the road cells: grass outside, barrier / gap inside the lane band */
    for (int i_11 = 0; i_11 < 0x40; i_11++) {
        if (i_11 < 0x13 || i_11 > 0x2b) {
            PA(0x8d0ec, (edi_10 << 6) + i_11) = S + 0x20e8;
        } else if (((i_11 + 1) & 3) == 0) {
            int eax_73 = ((i_11 - 0x13) >> 2) * 5 + temp0_1;
            int ecx_8  = IA(0x9a748, eax_73);
            int eax_74 = IA(0x9a75c, eax_73);
            if ((ecx_8 == 0xf || ecx_8 == 0) && (eax_74 == 0xf || eax_74 == 0)) {
                if (ecx_8 != 0 || eax_74 != 0) {
                    int edx_11 = (eax_74 == 0xf) ? 1 : 0;
                    int eax_75 = (ecx_8 == 0xf) ? 1 : 0;
                    unsigned char *eax_79;
                    if (edx_11 != eax_75 || (i32(0xe0) % 6) < 3) eax_79 = S + 0x10e8;
                    else                                         eax_79 = S + 0xe8;
                    PA(0x8d0ec, (edi_10 << 6) + i_11) = eax_79;
                } else {
                    PA(0x8d0ec, (edi_10 << 6) + i_11) = S + 0x20e8;
                }
            }
        }
    }

    /* commit the transition: current occupancy 0x9a6cc <- target 0x9a6ec */
    {
        int *eax_80 = (int *)(S + 0x9a6cc);
        int i_34 = 6;
        int i_12;
        do {
            *eax_80 = eax_80[8];
            eax_80 = &eax_80[1];
            i_12 = i_34;
            i_34 -= 1;
        } while (i_12 != 1);
    }
}
