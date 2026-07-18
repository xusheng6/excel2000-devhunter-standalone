/* Dev Hunter faithful port — Win32/GDI harness.
 * Replaces the DLL's loader (func2) + DirectDraw surface: an 8-bit framebuffer
 * the translated renderer writes into, blitted through the recovered palette.
 * Build 32-bit.  Input wndproc = sub_3c7dfc6b (writes state offsets directly).
 */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include "devhunter.h"
#include "data.h"

unsigned char *S = NULL;
DevHunter    *g = NULL;   /* same memory as S — the readable, named-field view */
unsigned char *g_fb = NULL;
double g_rand_seed = 0.0;

static HWND     g_hwnd;
static HDC      g_memdc;
static HBITMAP  g_dib;
static unsigned int *g_dibpix;
static unsigned int g_pal32[256];

unsigned int dh_time(void){ return timeGetTime(); }

static void build_pal32(void){
    int i; for(i=0;i<256;i++)
        g_pal32[i]=(g_palette[i][0]<<16)|(g_palette[i][1]<<8)|g_palette[i][2];
}
static void present(void){
    int n=SCR_W*SCR_H,i; unsigned char*s=g_fb; unsigned int*d=g_dibpix;
    for(i=0;i<n;i++) d[i]=g_pal32[s[i]];
    { HDC h=GetDC(g_hwnd); BitBlt(h,0,0,SCR_W,SCR_H,g_memdc,0,0,SRCCOPY); ReleaseDC(g_hwnd,h); }
}
static void dump_ppm(const char*p){
    FILE*f=fopen(p,"wb"); int i,n=SCR_W*SCR_H;
    fprintf(f,"P6\n%d %d\n255\n",SCR_W,SCR_H);
    for(i=0;i<n;i++){unsigned char c=g_fb[i];fputc(g_palette[c][0],f);fputc(g_palette[c][1],f);fputc(g_palette[c][2],f);}
    fclose(f);
}

/* sub_3c7dfc6b — input maps keys to state flags */
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_KEYDOWN:
        switch(wp){
        case VK_DOWN:  g->key_down=1; break;
        case VK_RIGHT: g->key_gas=1; break;
        case VK_SPACE: g->fx_fire=1; break;
        case VK_LEFT:  g->key_brake=1; break;
        case VK_UP:    g->key_up=1; break;
        case 'H':      g->fx_names ^= 1; break;
        case 'O':      g->fx_weapon=1; break;
        case VK_ESCAPE: case VK_F12: PostMessage(h,WM_CLOSE,0,0); break;
        } return 0;
    case WM_KEYUP:
        switch(wp){
        case VK_SPACE: g->fx_fire=0; break;
        case VK_LEFT:  g->key_brake=0; break;
        case VK_UP:    g->key_up=0; break;
        case VK_RIGHT: g->key_gas=0; break;
        case VK_DOWN:  g->key_down=0; break;
        case 'O':      g->fx_weapon=0; break;
        } return 0;
    case WM_SETCURSOR: SetCursor(NULL); return 1;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(h,m,wp,lp);
}

static void make_window(void){
    WNDCLASS wc; RECT r; DWORD st; BITMAPINFO bi; HDC wdc;
    memset(&wc,0,sizeof wc);
    wc.lpfnWndProc=WndProc; wc.hInstance=GetModuleHandle(NULL);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName="DevHunter"; RegisterClass(&wc);
    st=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    r.left=0;r.top=0;r.right=SCR_W;r.bottom=SCR_H; AdjustWindowRect(&r,st,FALSE);
    g_hwnd=CreateWindow("DevHunter","Dev Hunter",st,CW_USEDEFAULT,CW_USEDEFAULT,
        r.right-r.left,r.bottom-r.top,NULL,NULL,wc.hInstance,NULL);
    wdc=GetDC(g_hwnd);
    memset(&bi,0,sizeof bi);
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=SCR_W; bi.bmiHeader.biHeight=-SCR_H;
    bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
    g_memdc=CreateCompatibleDC(wdc);
    g_dib=CreateDIBSection(wdc,&bi,DIB_RGB_COLORS,(void**)&g_dibpix,NULL,0);
    SelectObject(g_memdc,g_dib);
    ReleaseDC(g_hwnd,wdc);
    ShowWindow(g_hwnd,SW_SHOW); UpdateWindow(g_hwnd);
}

/* mimic func1/setmode: allocate state, wire surface + letter font, then init */
static void boot(void){
    S = (unsigned char*)VirtualAlloc(NULL,STATE_SIZE,MEM_COMMIT,PAGE_READWRITE);
    g = (DevHunter*)S;              /* the class overlays the raw buffer */
    memset(S,0,STATE_SIZE);
    g_fb = (unsigned char*)malloc(FB_PITCH*SCR_H);
    memset(g_fb,0,FB_PITCH*SCR_H);
    build_pal32();
    /* letter atlas font (setmode created Tahoma 64 black) */
    g->letter_font = (int)(INT_PTR)CreateFont(64,16,0,0,FW_BLACK,0,0,0,ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Tahoma");
    /* surface = our 8-bit framebuffer */
    g->surf_pitch = FB_PITCH;
    g->surf = g_fb;
    game_init();
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show){
    MSG msg;
    (void)hi;(void)hp;(void)show;

    if(cmd && strstr(cmd,"--shot")){
        int frames=1,i; char outp[260]="shot.ppm";
        sscanf(cmd,"%*s %d %259s",&frames,outp);
        make_window(); timeBeginPeriod(1); boot();
        g->key_up=1;   /* simulate accelerator so the road scrolls */
        for(i=0;i<frames;i++){ g->frame_timer=dh_time()-20; game_tick(); }
        dump_ppm(outp); return 0;
    }

    make_window(); timeBeginPeriod(1); boot();
    g->frame_timer=dh_time();
    for(;;){
        while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){ timeEndPeriod(1); return 0; }
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        game_tick();
        present();
        Sleep(1);
    }
}
