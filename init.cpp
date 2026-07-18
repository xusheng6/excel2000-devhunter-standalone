/* game_init — loads the exact captured output of sub_3c7df259.
 *
 * init is a deterministic one-time setup (RNG seed is always 0.0), so its output
 * is identical every run.  Rather than re-derive its ~600 lines of terrain / LUT
 * / GDI-letter-atlas generation, we load a snapshot of the game state taken right
 * after the real init ran (init_state.bin, captured via the BN debugger) and
 * relocate its this-relative pointers into our buffer.  Byte-exact and standalone.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "devhunter.h"
#include "data.h"

#define INIT_BASE 0x6a0000u          /* THIS address when init_state.bin was captured (post-init) */

void game_init(void){
    FILE *f = fopen("init_state.bin","rb");
    long n; int i; unsigned int delta;
    unsigned char *b1, *b2;
    if(!f){ MessageBoxA(0,"init_state.bin not found","Dev Hunter",0); ExitProcess(1); }
    fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
    fread(S,1,n,f); fclose(f);
    /* relocate in-buffer this-relative pointers [INIT_BASE, INIT_BASE+state) -> S */
    delta = (unsigned int)(INT_PTR)S - INIT_BASE;
    for(i=0;i+4<=(int)n;i+=4){ unsigned int v=*(unsigned int*)(S+i);
        if(v>=INIT_BASE && v<INIT_BASE+0x9a96cu) *(unsigned int*)(S+i)=v+delta; }
    /* the two text streams are separate heap allocations in the original; recreate
     * them from the decrypted credits and re-point the buffer + cursor fields. */
    b1=(unsigned char*)malloc(sizeof g_text1); memcpy(b1,g_text1,sizeof g_text1);
    b2=(unsigned char*)malloc(sizeof g_text2); memcpy(b2,g_text2,sizeof g_text2);
    pp(0x910ec)=b1; pp(0x910f0)=b2;
    pp(0xc8)=b1; pp(0xcc)=b2;          /* stream cursors at start */
    i32(0xd0)=0; i32(0xd4)=1; i32(0xd8)=0x14;
    /* our 8-bit framebuffer replaces the DirectDraw surface; make a letter font */
    i32(0x20)=FB_PITCH;
    i32(0x34)=(int)(INT_PTR)g_fb;
    i32(0x9a920)=(int)(INT_PTR)CreateFont(64,16,0,0,FW_BLACK,0,0,0,ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Tahoma");
}
