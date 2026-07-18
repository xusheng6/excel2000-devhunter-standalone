/* Win32/GDI harness for Dev Hunter standalone. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include "dh.h"
#include "data.h"

unsigned char *g_fb = NULL;
double g_rand_seed = 0.0;
Game G;

static HWND g_hwnd;
static HDC  g_memdc;
static HBITMAP g_dib;
static unsigned int *g_dibpix;
static unsigned int g_pal32[256];

unsigned int dh_time(void){ return timeGetTime(); }

static void build_pal32(void){
    int i;
    for(i=0;i<256;i++)
        g_pal32[i]=(g_palette[i][0]<<16)|(g_palette[i][1]<<8)|g_palette[i][2];
}
static void present(void){
    int n=SCR_W*SCR_H,i; unsigned char*s=g_fb; unsigned int*d=g_dibpix;
    char hud[128];
    for(i=0;i<n;i++) d[i]=g_pal32[s[i]];
    /* HUD drawn straight onto the DIB via GDI */
    SetBkMode(g_memdc,TRANSPARENT);
    SetTextColor(g_memdc,RGB(255,255,255));
    wsprintf(hud,"SCORE %d",G.score);
    TextOutA(g_memdc,8,6,hud,lstrlenA(hud));
    if(G.recover>0.0f){ SetTextColor(g_memdc,RGB(255,80,60));
        TextOutA(g_memdc,SCR_W/2-30,SCR_H/2-40,"CRASH!",6); }
    SetTextColor(g_memdc,RGB(200,200,120));
    TextOutA(g_memdc,8,SCR_H-20,
        "\x18\x19\x1a\x1b move/gas   SPACE fire   O spread   H names   ESC quit",57);
    { HDC hdc=GetDC(g_hwnd); BitBlt(hdc,0,0,SCR_W,SCR_H,g_memdc,0,0,SRCCOPY); ReleaseDC(g_hwnd,hdc); }
}
static void dump_ppm(const char*path){
    FILE*f=fopen(path,"wb"); int i,n=SCR_W*SCR_H;
    fprintf(f,"P6\n%d %d\n255\n",SCR_W,SCR_H);
    for(i=0;i<n;i++){unsigned char c=g_fb[i];fputc(g_palette[c][0],f);fputc(g_palette[c][1],f);fputc(g_palette[c][2],f);}
    fclose(f);
}

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM wp,LPARAM lp){
    switch(m){
    case WM_KEYDOWN:
        switch(wp){
        case VK_RIGHT: G.k_right=1; break;
        case VK_LEFT:  G.k_left =1; break;
        case VK_UP:    G.k_gas  =1; break;
        case VK_DOWN:  G.k_brake=1; break;
        case VK_SPACE: G.k_fire =1; break;
        case 'O':      G.k_weapon=1; break;
        case 'H':      G.k_names ^= 1; break;
        case VK_ESCAPE: case VK_F12: PostMessage(h,WM_CLOSE,0,0); break;
        } return 0;
    case WM_KEYUP:
        switch(wp){
        case VK_RIGHT: G.k_right=0; break;
        case VK_LEFT:  G.k_left =0; break;
        case VK_UP:    G.k_gas  =0; break;
        case VK_DOWN:  G.k_brake=0; break;
        case VK_SPACE: G.k_fire =0; break;
        case 'O':      G.k_weapon=0; break;
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

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show){
    MSG msg; unsigned int last;
    (void)hi;(void)hp;(void)show;
    g_fb=(unsigned char*)malloc(FB_PITCH*SCR_H);
    build_pal32();

    if(cmd && strstr(cmd,"--shot")){
        int frames=1,i; char outp[260]="shot.ppm";
        sscanf(cmd,"%*s %d %259s",&frames,outp);
        make_window();
        game_init();
        for(i=0;i<frames;i++){
            G.k_gas=1;                       /* simulate driving for the dump */
            G.k_fire = (i&15)<2;
            if((i%50)<10) G.k_right=1; else G.k_right=0;
            game_tick(1.0f/30.0f);
        }
        game_render();
        dump_ppm(outp);
        return 0;
    }

    make_window();
    timeBeginPeriod(1);
    game_init();
    last=dh_time();
    for(;;){
        unsigned int now; float dt;
        while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){ timeEndPeriod(1); return 0; }
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        now=dh_time(); dt=(now-last)/1000.0f; last=now;
        if(dt>0.1f) dt=0.1f;
        if(dt<=0.0f) dt=0.001f;
        game_tick(dt);
        game_render();
        present();
        Sleep(1);
    }
}
