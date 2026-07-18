/* game_tick — sub_3c7dcb73, translated from hlil (re/hlil.txt 981-2473) + asm.
 * Per frame: dt + day/night ambient, player physics, [enemies/bullets/debris -
 * TODO], road scroll + dev-name painting + elevation, [glow - TODO], then render.
 * FP compares decoded as: test_bit(x,0)=a<b, test_bit(x,6)=a==b.
 */
#include <string.h>
#include "dh.h"

void game_tick(void){
    float var_10;   /* dt (ms * 0.02) */
    float var_18;   /* 1 - dt*0.04 (velocity decay) */
    int   var_14;   /* ambient shade (day/night) */
    int   i;

    /* ---- time delta ---- */
    { int eax = (int)dh_time();
      int dt  = eax - i32(0xb4);
      i32(0xb4) = eax;
      var_10 = (float)dt * 0.0199999996f;
      var_18 = 1.0f - var_10*0.04f; }

    /* ---- day/night ambient from timer 0x9a6b4 ---- */
    { float x = f32(0x9a6b4);
      if      (x < 1000.0f) var_14 = 0;
      else if (x < 2000.0f) var_14 = FTOL((double)(x-1000.0f)*0.0199999996*256.0);
      else if (x < 2600.0f) var_14 = 0x1400;
      else                  var_14 = FTOL((3600.0-(double)x)*0.0199999996*256.0); }
    /* advance the timer, wrap the day/night cycle at 3600 */
    { float nx = var_10 + f32(0x9a6b4);
      f32(0x9a6b4) = nx;
      if (nx > 3600.0f){ f32(0x9a6b4)=nx-3600.0f; f32(0x9a6b8)-=3600.0f; f32(0x9a6bc)-=3600.0f; } }

    /* fill lighting grid with ambient */
    for(i=0;i<0x1000;i++) IA(0x890ec, i) = var_14;

    /* ---- player physics ---- */
    {
      if (f32(0x9a6c0) != 0.0f){                    /* crash recovery */
          float r = f32(0x9a6c0) - var_10;
          f32(0x9a6c0)=r; f32(0xbc)=0.0f; f32(0xb8)=0.0f;
          if (r <= 0.0f){ f32(0xc0)=0.0f; f32(0x9a6c0)=0.0f; }
      } else {
          int gas=i32(0xa4), brake=i32(0xa8);
          if (gas)  { float v=var_10*4.0f + f32(0xb8); f32(0xb8)=v; if(v<-10.0f) f32(0xb8)=-10.0f; }
          if (brake){ float v=f32(0xb8) - var_10*4.0f; f32(0xb8)=v; if(v>10.0f)  f32(0xb8)= 10.0f; }
          if (!gas && !brake) f32(0xb8) = (1.0f - var_10*0.2f)*f32(0xb8);
          if (i32(0xac)){ float v=var_10*0.200000003f + f32(0xbc); f32(0xbc)=v; if(v>2.6f) f32(0xbc)=2.6f; }
          if (i32(0xb0)){ float v=f32(0xbc) - var_10*0.200000003f; f32(0xbc)=v; if(v<0.0f) f32(0xbc)=0.0f; }
      }
      f32(0xc0) = f32(0xbc)*0.217391312f * f32(0xb8) * var_10 + f32(0xc0);
      (void)var_18;
      { float d = var_10 * f32(0xbc);
        f32(0xc4) = d + f32(0xc4);
        f32(0xa0) = d + f32(0xa0); }
      /* barrier crash check (only when not already recovering) */
      if (f32(0x9a6c0) == 0.0f){
          int row = FTOL(66.0 - (double)i32(0xdc) - (double)f32(0xa0) + 28.0);
          int col = FTOL((double)f32(0xa0)*0.05 + 31.5);
          if (PA(0x8d0ec, ((row&0x3f)<<6)+col) == S+0x20e8){
              int e0 = i32(0xe0);
              f32(0xbc)=0.0f; f32(0x9a6c0)=20.0f; f32(0xb8)=0.0f;
              spawn_burst(f32(0xc0), (float)(e0-0x1c), 0.0f, 0.0f);
          }
      }
      (void)var_18;
    }

    /* ---- enemy / bullet / debris updates ---- */
    entity_update(var_10, var_18);

    f32(0x9c) = f32(0xc0);   /* camera lateral = speed (steering curves the road) */

    /* ---- road scroll: emit new rows as the odometer (0xa0) advances ---- */
    while(f32(0xa0) >= 1.0f){
        int eax_59, edi_10, i_4;
        f32(0xa0) -= 1.0f;
        i32(0xdc) += 1; i32(0xe0) += 1;
        if (i32(0xdc) > 0x40) i32(0xdc) -= 0x40;
        eax_59 = i32(0x9a7e8);
        edi_10 = (-i32(0xdc)) & 0x3f;

        if (eax_59 == 0){
            /* paint the road surface for this row: grass / road / dashed lanes */
            for(i_4=0;i_4<0x40;i_4++){
                int eax_114 = (i_4-0x13) >> 2;
                unsigned char *cell;
                if (i_4 < 0x13 || i_4 > 0x2b) cell = S+0x20e8;
                else if (((i_4+1)&3) != 0){
                    cell = S+0xe8;
                    if (IA(0x9a6cc, eax_114) == 0) cell = S+0x20e8;
                } else {
                    int edx_18 = IA(0x9a6c8, eax_114);
                    if (edx_18 != 0){
                        if (IA(0x9a6cc, eax_114) == 0) cell = S+0x10e8;
                        else cell = ((i32(0xe0) % 6) < 3) ? S+0x10e8 : S+0xe8;
                    } else if (IA(0x9a6cc, eax_114) == edx_18) cell = S+0x20e8;
                    else {
                        if (edx_18 != 0 && IA(0x9a6cc, eax_114) != 0)
                            cell = ((i32(0xe0) % 6) < 3) ? S+0x10e8 : S+0xe8;
                        else cell = S+0x10e8;
                    }
                }
                PA(0x8d0ec, (edi_10<<6)+i_4) = cell;
            }
            /* paint the current dev-name word onto the road */
            { int edx_24 = 0;
              unsigned char *ecx_21 = PA(0xc8, i32(0xd0));
              int eax_120 = i32(0x910ec);
              unsigned char i_5;
              while((i_5 = *PA(0xc8, i32(0xd0))) != 0x2d){
                  if (i_5 != 0x20){
                      int eax_122 = i32(0xd4) + ((int)(signed char)i_5 << 1);
                      unsigned char *cell = (i_5 < 0x61)
                          ? (unsigned char*)((eax_122<<0xc) + (int)(INT_PTR)S - 0x30f18)
                          : (unsigned char*)((eax_122<<0xc) + (int)(INT_PTR)S - 0xa4f18);
                      PA(0x8d0ec, (edi_10<<6) + i32(0xd8) + edx_24) = cell;
                  }
                  edx_24++;
                  PA(0xc8, i32(0xd0)) = PA(0xc8, i32(0xd0)) + 1;
              }
              /* advance / repeat the word, handle control chars # / ! */
              { int eax_129 = i32(0xd0);
                int t = i32(0xd4) ^ 1;
                i32(0xd4) = t;
                if (t != 0){
                    PA(0xc8, eax_129) = PA(0xc8, eax_129) + 1;
                    { unsigned char c = *PA(0xc8, i32(0xd0));
                      if (c == 0x23) PA(0xc8, i32(0xd0)) = (unsigned char*)(eax_120 + 0xfa);
                      if (c == 0x2f){ PA(0xc8, i32(0xd0)) = pp(0x910f0); i32(0xd0) ^= 1; }
                      if (c == 0x21){ PA(0xc8, i32(0xd0)) = PA(0xc8, i32(0xd0))+1; i32(0xd0) ^= 1; } }
                } else {
                    PA(0xc8, eax_129) = ecx_21;
                }
              }
            }
            /* spawn countdown (enemy lane setup: TODO) */
            i32(0x9a6c4) -= 1;
        } else {
            i32(0x9a7e8) = eax_59 - 1;
            /* barrier/lane-transition row generation: TODO */
        }
        /* elevation generation for the new row: TODO (uses embedded values for now) */
    }

    /* ---- (glow/headlight splat + entity billboards: TODO) ---- */

    game_render(var_14);
}
