/* game_tick — sub_3c7dcb73, translated from hlil (re/hlil.txt 981-2473) + asm.
 * Per frame: dt + day/night ambient, player physics, enemies/bullets/debris,
 * road scroll + dev-name painting + elevation, glow, then render.
 * FP compares decoded as: test_bit(x,0)=a<b, test_bit(x,6)=a==b.
 */
#include <string.h>
#include "devhunter.h"
#include "data.h"

void game_tick(void){
    float var_10;   /* dt (ms * 0.02) */
    float var_18;   /* 1 - dt*0.04 (velocity decay) */
    int   var_14;   /* ambient shade (day/night) */
    int   i;

    /* ---- time delta ---- */
    { int eax = (int)dh_time();
      int dt  = eax - g->frame_timer;
      g->frame_timer = eax;
      var_10 = (float)dt * 0.0199999996f;
      var_18 = 1.0f - var_10*0.04f; }

    /* ---- day/night ambient from the daynight timer ---- */
    { float x = g->daynight;
      if      (x < 1000.0f) var_14 = 0;
      else if (x < 2000.0f) var_14 = FTOL((double)(x-1000.0f)*0.0199999996*256.0);
      else if (x < 2600.0f) var_14 = 0x1400;
      else                  var_14 = FTOL((3600.0-(double)x)*0.0199999996*256.0); }
    /* advance the timer, wrap the day/night cycle at 3600 */
    { float nx = var_10 + g->daynight;
      g->daynight = nx;
      if (nx > 3600.0f){ g->daynight=nx-3600.0f; g->spawn_at-=3600.0f; g->fire_at-=3600.0f; } }

    /* fill lighting grid with ambient */
    for(i=0;i<0x1000;i++) g->lighting[i] = var_14;

    /* ---- spawn a new enemy car when the spawn timer allows ---- */
    if ((g->daynight - g->spawn_at) > 25.0f){
        int e;
        for(e=0;e<7;e++){
            Enemy &en = g->enemy[e];
            if (en.timer <= 0.0f){                  /* found an inactive slot */
                int pat = (signed char)g->spawn_pat;
                int lane = FTOL(dh_rand() * (double)(signed char)TAB(0x3c7ad10c + pat))
                         + (signed char)TAB(0x3c7ad104 + pat);
                float lanepos = (float)(lane * 0x54) - 210.0f;
                float vx = (float)(dh_rand() * 3.6);
                en.timer    = 100.0f;                /* timer = alive */
                en.lane     = lane;
                en.target_y = lanepos;
                en.y        = lanepos;
                en.vx       = vx;
                en.vy       = 0.0f;
                en.x        = (vx >= g->throttle) ? (float)(g->frame - 0x41)
                                                  : (float)(g->frame + 0xa);
                en.color    = 1 - FTOL(dh_rand() * -6.0);   /* colour variant */
                { double r = dh_rand() * 5.0;
                  if (FTOL(r) == 2 && (r - 2.0) < 0.5) r = 0.5;
                  g->setPose(e, FTOL(r)); }          /* pose */
                break;
            }
        }
        g->spawn_at = g->daynight;
    }

    /* ---- player physics ---- */
    {
      if (g->recover != 0.0f){                    /* crash recovery */
          float r = g->recover - var_10;
          g->recover=r; g->throttle=0.0f; g->steer=0.0f;
          if (r <= 0.0f){ g->speed=0.0f; g->recover=0.0f; }
      } else {
          int gas=g->key_gas, brake=g->key_brake;
          if (gas)  { float v=var_10*4.0f + g->steer; g->steer=v; if(v<-10.0f) g->steer=-10.0f; }
          if (brake){ float v=g->steer - var_10*4.0f; g->steer=v; if(v>10.0f)  g->steer= 10.0f; }
          if (!gas && !brake) g->steer = (1.0f - var_10*0.2f)*g->steer;
          if (g->key_up){ float v=var_10*0.200000003f + g->throttle; g->throttle=v; if(v>2.6f) g->throttle=2.6f; }
          if (g->key_down){ float v=g->throttle - var_10*0.200000003f; g->throttle=v; if(v<0.0f) g->throttle=0.0f; }
      }
      g->speed = g->throttle*0.217391312f * g->steer * var_10 + g->speed;
      (void)var_18;
      { float d = var_10 * g->throttle;
        g->lateral = d + g->lateral;
        g->camY = d + g->camY; }
      /* barrier crash check (only when not already recovering).
       * col uses `speed` — as you steer off-centre the checked column drifts
       * past the road edge into grass/barrier, which crashes you.
       * (The HLIL mislabels this as a0; the x87 ftol pops, leaving speed on top.) */
      if (g->recover == 0.0f){
          int row = FTOL(66.0 - (double)g->scroll_row - (double)g->camY + 28.0);
          int col = FTOL((double)g->speed*0.05 + 31.5);
          if (g->texptr[((row&0x3f)<<6)+col] == g->proto(TEX_GRASS)){
              int e0 = g->frame;
              g->throttle=0.0f; g->recover=20.0f; g->steer=0.0f;
              spawn_burst(g->speed, (float)(e0-0x1c), 0.0f, 0.0f);
          }
      }
      (void)var_18;
    }

    /* ---- enemy / bullet / debris updates ---- */
    entity_update(var_10, var_18);

    g->camX = g->speed;   /* camera lateral = speed (steering curves the road) */

    /* ---- road scroll: emit new rows as the odometer advances ---- */
    while(g->camY >= 1.0f){
        int eax_59, edi_10, i_4;
        g->camY -= 1.0f;
        g->scroll_row += 1; g->frame += 1;
        if (g->scroll_row > 0x40) g->scroll_row -= 0x40;
        eax_59 = g->barrier_phase;
        edi_10 = (-g->scroll_row) & 0x3f;

        if (eax_59 == 0){
            /* paint the road surface for this row: grass / road / dashed lanes */
            for(i_4=0;i_4<0x40;i_4++){
                int eax_114 = (i_4-0x13) >> 2;
                unsigned char *cell;
                if (i_4 < 0x13 || i_4 > 0x2b) cell = g->proto(TEX_GRASS);
                else if (((i_4+1)&3) != 0){
                    cell = g->proto(TEX_PLAIN);
                    if (g->lane_cur[eax_114] == 0) cell = g->proto(TEX_GRASS);
                } else {
                    int edx_18 = g->lane_cur[eax_114 - 1];   /* occupancy one cell back */
                    if (edx_18 != 0){
                        if (g->lane_cur[eax_114] == 0) cell = g->proto(TEX_LANE);
                        else cell = ((g->frame % 6) < 3) ? g->proto(TEX_LANE) : g->proto(TEX_PLAIN);
                    } else if (g->lane_cur[eax_114] == edx_18) cell = g->proto(TEX_GRASS);
                    else {
                        if (edx_18 != 0 && g->lane_cur[eax_114] != 0)
                            cell = ((g->frame % 6) < 3) ? g->proto(TEX_LANE) : g->proto(TEX_PLAIN);
                        else cell = g->proto(TEX_LANE);
                    }
                }
                g->texptr[(edi_10<<6)+i_4] = cell;
            }
            /* paint the current dev-name word onto the road */
            { int edx_24 = 0;
              unsigned char *ecx_21    = g->text_cur[g->text_stream];  /* word start */
              unsigned char *word_base = g->text_buf[0];               /* stream 0 base */
              unsigned char i_5;
              while((i_5 = *g->text_cur[g->text_stream]) != 0x2d){
                  if (i_5 != 0x20){
                      int eax_122 = g->text_toggle + ((int)(signed char)i_5 << 1);
                      /* letter tile in the 3D-letter atlas (upper/lower case bank) */
                      unsigned char *cell = (i_5 < 0x61)
                          ? (unsigned char*)((eax_122<<0xc) + (int)(INT_PTR)S - 0x30f18)
                          : (unsigned char*)((eax_122<<0xc) + (int)(INT_PTR)S - 0xa4f18);
                      g->texptr[(edi_10<<6) + g->text_col + edx_24] = cell;
                  }
                  edx_24++;
                  g->text_cur[g->text_stream]++;
              }
              /* advance / repeat the word, handle control chars - # / ! */
              { int eax_129 = g->text_stream;
                int t = g->text_toggle ^ 1;
                g->text_toggle = t;
                if (t != 0){
                    g->text_cur[eax_129]++;
                    { unsigned char c = *g->text_cur[g->text_stream];
                      if (c == 0x23) g->text_cur[g->text_stream] = word_base + 0xfa;
                      if (c == 0x2f){ g->text_cur[g->text_stream] = g->text_buf[1]; g->text_stream ^= 1; }
                      if (c == 0x21){ g->text_cur[g->text_stream]++; g->text_stream ^= 1; } }
                } else {
                    g->text_cur[eax_129] = ecx_21;
                }
              }
            }
            /* spawn countdown -> pick a new lane/enemy pattern */
            g->spawn_count -= 1;
            if (g->spawn_count < 1 && g->text_toggle != 0) spawn_pattern(edi_10);
        } else {
            barrier_row(edi_10, eax_59);   /* sets barrier_phase = eax_59-1 internally */
        }
        /* elevation (road shadow) generation for the new row */
        { int next_base = ((edi_10+1)&0x3f) << 6;
          int cur_base  = edi_10 << 6;
          int v3;
          for(v3=1; v3<=0x40; v3++){
              int col = v3-1, add = 0;
              int ebx_8 = ( g->elevation[((v3-2)&0x3f) + next_base]
                          + g->elevation[( v3   &0x3f) + next_base]
                          + (g->elevation[next_base + col] << 1) ) >> 2;
              double r = dh_rand() * 10.0;
              if (r < 2.0) add = FTOL(r*512.0);
              else if (r > 7.99) add = FTOL((r-10.0)*512.0);
              if (g->texptr[cur_base+col] == g->proto(TEX_GRASS)) add <<= 2;
              { int e = ebx_8 + add;
                if (e < 0) e = 0;
                if (e > 0x1400) e = 0x1400;
                if (e > 0xa00 && g->texptr[cur_base+col] != g->proto(TEX_GRASS)) e -= 0x100;
                g->elevation[cur_base+col] = e; }
          }
        }
    }

    /* ---- headlight cone + entity/muzzle glow splats ---- */
    glow_var_10 = var_10;
    glow_pass(var_14);

    /* ---- fire bullets on Space, 2-shot spread ---- */
    if (g->fx_fire != 0 && g->recover == 0.0f && (g->daynight - g->fire_at) > 2.0f){
        int i_17;
        for(i_17 = -1; i_17 < 2; i_17 += 2){
            int bi = i32(0x99700);              /* round-robin bullet slot */
            Bullet &bl = g->bullet[bi];
            bl.life = 20.0f;
            bl.x    = (float)i_17*10.0f + g->speed;
            bl.y    = (float)(g->frame - 0x1a);
            *(float*)(S + (bi + 0x7aac)*0x14) = 0.0f;   /* clears the slot's trailing word (game quirk) */
            bl.dvx  = g->throttle + 1.5f;
            i32(0x99700) = (bi + 1) % 0x14;
        }
        g->fire_at = g->daynight;
    }

    /* fold road elevation into the lighting grid */
    { int k; for(k=0;k<0x1000;k++){
        int l = g->lighting[k];
        int e = g->elevation[k];
        if (l < 0) l = 0;
        { int t = l; l = t + e; if (t + e < 0) l = 0; }
        if (l > 0x1e80) l = 0x1e80;
        g->lighting[k] = l;
    } }

    game_render(var_14);
}
