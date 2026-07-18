/* game_init (sub_3c7df259) — TODO: real translation. Stub for now. */
#include <string.h>
#include <stdlib.h>
#include "dh.h"
#include "data.h"
void game_init(void){
    unsigned char*b1=malloc(sizeof g_text1),*b2=malloc(sizeof g_text2);
    memcpy(b1,g_text1,sizeof g_text1); memcpy(b2,g_text2,sizeof g_text2);
    pp(0x910ec)=b1; pp(0x910f0)=b2;
}
