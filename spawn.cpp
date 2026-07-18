/* Dev Hunter — enemy-spawn / lane-pattern / barrier-row generation.
 *
 * Faithful translation of game_tick (sub_3c7dcb73), re/hlil.txt 1794-2051,
 * cross-checked against re/asm.txt 3c7ddb5a-3c7ddce5.  Both functions run
 * inside the road-scroll loop; `edi_10 = (-g->scroll_row) & 0x3f` is the current
 * road-ring row index and `eax_59 = g->barrier_phase` is the barrier countdown.
 *
 * spawn_pattern : hlil 1795-1970.  Picks a fresh lane/enemy pattern.
 * barrier_row   : hlil 1972-2051.  Paints one lane-transition / barrier row.
 *
 * The lane state:
 *   g->lane_cur[6]   current per-lane occupancy (0 = empty)
 *   g->lane_tgt[6]   target  per-lane occupancy for the pattern being spawned
 *   g->lane_pair[]   per-lane {source,dest} pairs, stride 2 ints
 *   g->barrier_grid[] raw nibble grid describing each lane's merge/split; the
 *                    cursor walks below index it with small +/- offsets (they
 *                    mirror the DLL's flat-buffer pointer arithmetic exactly).
 */
#include <string.h>
#include "devhunter.h"

/* hlil 1795-1970 : pick a new lane / enemy pattern. */
void spawn_pattern(int edi_10)
{
    (void)edi_10;   /* row index is unused by this branch */

    /* reset spawn countdown, choose pattern row eax_131 in [0,6) */
    g->spawn_count = 0x28 - FTOL(dh_rand() * -80.0);
    int eax_131 = FTOL(dh_rand() * 6.0);
    g->spawn_pat = (unsigned char)eax_131;

    /* target occupancy from the chosen pattern-row bitmask */
    for (int k = 0; k < 6; k++)
        g->lane_tgt[k] = (1 << ((k + 1) & 0xff)) & (int)(signed char)g->spawn_tab[eax_131];

    /* build the lane {source,dest} pairs and the max transition distance */
    int var_8_1 = 0;
    g->text_col = ((int)(signed char)g->spawn_tab[eax_131 + 8] << 2) + 0x14;
    for (int k = 0; k < 6; k++) {
        int   lane = k + 1;
        int  *pair_src = &g->lane_pair[2*k];       /* ebx_16[-1] */
        int  *pair_dst = &g->lane_pair[2*k + 1];   /* ebx_16[ 0] */
        *pair_src = lane;
        *pair_dst = lane;
        int cur = g->lane_cur[k];
        if (cur != g->lane_tgt[k]) {
            if (cur == 0) {
                /* lane starts: snap dest to nearest currently-occupied lane */
                int best = 0xa;
                for (int j = 1; j < 7; j++) {
                    if (g->lane_cur[j-1] != 0) {
                        int d = dh_iabs(j - lane);
                        if (d < best) { best = d; *pair_dst = j; }
                    }
                }
                if (var_8_1 <= best) var_8_1 = best;
            } else {
                /* lane ends: snap source to nearest target-occupied lane */
                int best = 0xa;
                for (int j = 1; j < 7; j++) {
                    if (g->lane_tgt[j-1] != 0) {
                        int d = dh_iabs(j - lane);
                        if (d < best) { best = d; *pair_src = j; }
                    }
                }
                if (var_8_1 <= best) var_8_1 = best;
            }
        }
    }

    /* re-home the 7 live enemies onto their remapped lane target-y */
    for (int k = 0; k < 7; k++) {
        Enemy &en = g->enemy[k];
        if (en.timer > 0.0f) {
            int nl = g->lane_pair[en.lane * 2] - 1;
            en.lane = nl;
            en.target_y = (float)((double)(nl * 0x54) - 210.0);
        }
    }

    /* schedule the barrier row and clear the barrier grid */
    g->barrier_phase = var_8_1 << 2;
    memset(&g->barrier_grid[1], 0, 0xa0);

    /* fill the barrier grid describing how each lane merges/splits */
    {
        int edx_29 = 1;
        int *var_c    = &g->barrier_grid[11];              /* 0x9a770 */
        int *var_20_3 = &g->lane_cur[0];
        int i_9 = 5;
        int *eax_161  = &g->lane_pair[1];                  /* 0x9a714 */
        int *var_28_5 = &g->barrier_grid[var_8_1];         /* 0x9a744 + dist */
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
                        g->barrier_grid[ecx_41]     |= 0xc;
                        g->barrier_grid[ecx_41 + 5] |= 3;
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
    g->barrier_phase = eax_59 - 1;
    int temp0_1 = (eax_59 - 1) / 4;

    /* build the 6 lane cell-texture columns for this barrier row */
    {
        int i_10 = 1;
        int *ebx_5 = &g->barrier_grid[6 + temp0_1];        /* 0x9a75c + phase/4 */
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
            /* write animated cell textures into the texptr grid (as ints) */
            int *edx_10 = (int *)(g->texptr + 0xf + eax_63 + (((edi_10 << 4) + i_10) << 2));
            do {
                int eax_67 = (g->barrier_phase & 3) + j_6;
                j_6 += 6;
                *edx_10 = g->colorscheme[edi_11 + eax_67 * 6];
                edx_10 = &edx_10[1];
            } while (j_6 < 0x1e);
            i_10 += 1;
            ebx_5 = &ebx_5[5];
        } while (i_10 < 7);
    }

    /* paint the road cells: grass outside, barrier / gap inside the lane band */
    for (int i_11 = 0; i_11 < 0x40; i_11++) {
        if (i_11 < 0x13 || i_11 > 0x2b) {
            g->texptr[(edi_10 << 6) + i_11] = g->proto(TEX_GRASS);
        } else if (((i_11 + 1) & 3) == 0) {
            int eax_73 = ((i_11 - 0x13) >> 2) * 5 + temp0_1;
            int ecx_8  = g->barrier_grid[eax_73 + 1];      /* 0x9a748 + eax_73 */
            int eax_74 = g->barrier_grid[eax_73 + 6];      /* 0x9a75c + eax_73 */
            if ((ecx_8 == 0xf || ecx_8 == 0) && (eax_74 == 0xf || eax_74 == 0)) {
                if (ecx_8 != 0 || eax_74 != 0) {
                    int edx_11 = (eax_74 == 0xf) ? 1 : 0;
                    int eax_75 = (ecx_8 == 0xf) ? 1 : 0;
                    unsigned char *eax_79;
                    if (edx_11 != eax_75 || (g->frame % 6) < 3) eax_79 = g->proto(TEX_LANE);
                    else                                        eax_79 = g->proto(TEX_PLAIN);
                    g->texptr[(edi_10 << 6) + i_11] = eax_79;
                } else {
                    g->texptr[(edi_10 << 6) + i_11] = g->proto(TEX_GRASS);
                }
            }
        }
    }

    /* commit the transition: current occupancy <- target occupancy */
    for (int k = 0; k < 6; k++)
        g->lane_cur[k] = g->lane_tgt[k];
}
