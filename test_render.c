/* Verify render.c against real captured state:
 *   load verify/frame0_this.bin into S (at the SAME address 0x5e0000 the game
 *   used, so stored this-relative pointers stay valid), run game_render, and
 *   dump the result to compare with verify/frame0_fb.bin. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "dh.h"
#include "data.h"

unsigned char *S=NULL, *g_fb=NULL; double g_rand_seed=0.0;
unsigned int dh_time(void){ return 0; }

int main(int argc,char**argv){
    unsigned int base=0x5e0000; FILE*f; long n; int i; int delta;
    const char*thisf = argc>1?argv[1]:"verify/frame0_this.bin";
    const char*outf  = argc>2?argv[2]:"verify/mine0.raw";
    /* origin base the state was captured at (this-relative pointers use it) */
    if(argc>3) base=(unsigned int)strtoul(argv[3],0,16);
    S=(unsigned char*)malloc(0x100000); memset(S,0,0x100000);
    f=fopen(thisf,"rb"); if(!f){printf("no %s\n",thisf);return 1;}
    fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
    fread(S,1,n,f); fclose(f);
    /* relocate in-buffer pointers: any dword in [base, base+STATE) -> our S */
    delta=(int)((unsigned int)(INT_PTR)S - base);
    for(i=0;i+4<=(int)n;i+=4){ unsigned int v=*(unsigned int*)(S+i);
        if(v>=base && v<base+0x9a96cu) *(unsigned int*)(S+i)=v+delta; }
    fprintf(stderr,"S=%p base=%x delta=%d; 0x8d0ec[0]=%x\n",(void*)S,base,delta,IA(0x8d0ec,0));
    g_fb=(unsigned char*)malloc(640*480); memset(g_fb,0,640*480);
    i32(0x20)=640; i32(0x34)=(int)(INT_PTR)g_fb;   /* redirect surface to our fb */
    fprintf(stderr,"loaded %ld bytes; 0x8d0ec[0]=%x tex-prototype base? rendering...\n",n,IA(0x8d0ec,0));
    game_render(0);
    fprintf(stderr,"render ok\n");
    f=fopen(outf,"wb"); fwrite(g_fb,1,640*480,f); fclose(f);   /* raw 8-bit indices */
    printf("wrote %s\n",outf); (void)i; return 0;
}
