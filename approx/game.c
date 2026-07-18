/* Dev Hunter — faithful reconstruction of the gameplay & look.
 * Matches the original's high near-top-down camera: a wide, mildly-tapering
 * multi-lane road, soft drifting terrain shadows, developer names painted on the
 * tarmac (white names / yellow section headers, from the decrypted credits),
 * textured grass, and top-down car sprites.  Recovered 256-colour palette.
 */
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "dh.h"
#include "data.h"

Game G;
unsigned char *g_strip = NULL;      /* road-decal strip: 0 empty,1 name,2 header */
int g_strip_used = 0;

/* ---- palette indices (into the recovered game palette) ---- */
#define C_ROAD_HI   34              /* brightest asphalt (grayscale idx*4) */
#define C_SHADE_RNG 21              /* how dark the terrain shadow goes */
#define C_LANE      60              /* dashed lane marking (white) */
#define C_NAME_W    61              /* dev name (white) */
#define C_NAME_Y    124             /* section header (yellow) */
#define C_GRASS     70              /* base grass green */

/* ---- projection (trapezoidal, high camera) ---- */
#define VY       (-720.0f)          /* vanishing point above the screen */
#define HW_BOT   210.0f             /* road half-width at the bottom row */
#define DZ       24000.0f           /* depth scale (worldZ = scroll + DZ/(y-VY)) */
#define CX       320.0f
#define P_DEPTH  20.0f              /* world depth of the player's row */
#define NAME_DENSITY 22.0f

/* terrain shadow field (soft, tiled along the road) */
#define NZ 256
#define NX 64
static float g_field[NZ*NX];

double dh_rand(void){
    double t=g_rand_seed*9821.0+0.211327;
    double r=t-(double)(int)t; g_rand_seed=r; return r;
}

/* smooth value-noise terrain, in the spirit of sub_3c7df259's road generator */
static void build_field(void){
    static float a[NZ*NX], b[NZ*NX]; int z,x,it;
    for(z=0;z<NZ;z++) for(x=0;x<NX;x++)
        a[z*NX+x] = ((z&7)||(x&7)) ? -1.0f : (float)dh_rand();
    for(z=0;z<NZ;z++) for(x=0;x<NX;x++) if(a[z*NX+x]<0.0f){
        int z0=z&~7,x0=x&~7,z1=(z0+8)&(NZ-1),x1=(x0+8); float tz=(z-z0)/8.0f,tx;
        float v00,v01,v10,v11; if(x1>=NX)x1=NX-1;
        tx=(x-x0)/8.0f;
        v00=a[z0*NX+x0]; if(v00<0)v00=0; v01=a[z0*NX+x1]; if(v01<0)v01=0;
        v10=a[z1*NX+x0]; if(v10<0)v10=0; v11=a[z1*NX+x1]; if(v11<0)v11=0;
        a[z*NX+x]=(v00*(1-tx)+v01*tx)*(1-tz)+(v10*(1-tx)+v11*tx)*tz;
    }
    memcpy(b,a,sizeof a);
    for(it=0;it<3;it++){
        for(z=0;z<NZ;z++) for(x=0;x<NX;x++){
            float s=b[z*NX+x]*4
                + b[((z+1)&(NZ-1))*NX+x] + b[((z-1)&(NZ-1))*NX+x]
                + b[z*NX+(x<NX-1?x+1:x)] + b[z*NX+(x>0?x-1:x)];
            a[z*NX+x]=s/8.0f;
        }
        memcpy(b,a,sizeof a);
    }
    memcpy(g_field,a,sizeof g_field);
}
static float field_at(float wz,float ax){         /* ax in [0,1] across road */
    float fz=wz*2.4f; int z0,x0; float tz,tx,v0,v1;
    fz -= floorf(fz/NZ)*NZ;
    { float fx=ax*(NX-1); x0=(int)fx; tx=fx-x0; if(x0>=NX-1){x0=NX-2;tx=1;}
      z0=(int)fz; tz=fz-z0; if(z0>=NZ)z0=0;
      { int z1=(z0+1)&(NZ-1);
        v0=g_field[z0*NX+x0]*(1-tx)+g_field[z0*NX+x0+1]*tx;
        v1=g_field[z1*NX+x0]*(1-tx)+g_field[z1*NX+x0+1]*tx;
        return v0*(1-tz)+v1*tz; } }
}

/* -------- dev-name road strip via GDI -------- */
static void build_strip(void){
    HDC dc; HBITMAP bmp,oldb; BITMAPINFO bi; unsigned int*px;
    HFONT fnt,oldf; int x,y,band,i; const char*t=g_text1; int tlen=(int)sizeof g_text1;
    g_strip=(unsigned char*)calloc(STRIP_W,STRIP_H);
    dc=CreateCompatibleDC(NULL);
    memset(&bi,0,sizeof bi);
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=STRIP_W; bi.bmiHeader.biHeight=-STRIP_H;
    bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
    bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&px,NULL,0);
    oldb=(HBITMAP)SelectObject(dc,bmp);
    { RECT r; r.left=0;r.top=0;r.right=STRIP_W;r.bottom=STRIP_H; FillRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH)); }
    fnt=CreateFont(22,0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_SWISS,"Arial");
    oldf=(HFONT)SelectObject(dc,fnt);
    SetBkMode(dc,TRANSPARENT); SetTextAlign(dc,TA_CENTER|TA_TOP);

    band=0; i=0;
    while(i<tlen && band<STRIP_H-32){
        char word[64]; int wl=0,upper=0;
        while(i<tlen){
            char ch=t[i++];
            if(ch=='-'||ch==0) break;
            if(ch=='!'||ch=='#'||ch=='/'){ wl=0; break; }
            if(ch>='A'&&ch<='Z') upper=1;
            if(ch>='a'&&ch<='z') ch=(char)(ch-'a'+'A');   /* game draws uppercase */
            if(wl<63) word[wl++]=ch;
        }
        if(wl>0){
            word[wl]=0;
            SetTextColor(dc,upper?RGB(255,255,0):RGB(255,255,255));
            TextOutA(dc,STRIP_W/2,band+2,word,wl);
            band+=54;
        }
    }
    g_strip_used=band?band:STRIP_H;
    GdiFlush();
    /* vertical flip so decals read upright to the approaching driver; classify
     * colour by luminance (yellow header vs white name). */
    for(y=0;y<STRIP_H;y++) for(x=0;x<STRIP_W;x++){
        unsigned int p=px[(STRIP_H-1-y)*STRIP_W+x];
        int r=(p>>16)&0xff,g=(p>>8)&0xff,b=p&0xff;
        g_strip[y*STRIP_W+x] = (r+g+b<160)?0 : (b<80?2:1);
    }
    SelectObject(dc,oldf); DeleteObject(fnt);
    SelectObject(dc,oldb); DeleteObject(bmp); DeleteDC(dc);
}

static void reset_game(void){
    memset(G.enemy,0,sizeof G.enemy); memset(G.bullet,0,sizeof G.bullet);
    memset(G.debris,0,sizeof G.debris); memset(G.wall,0,sizeof G.wall);
    G.speed=0; G.posx=0; G.steer=0; G.scroll=0;
    G.fire_cd=0; G.wpn_cd=0; G.spawn_timer=1.0f; G.wall_timer=3.0f; G.recover=0;
    G.score=0; G.hitflash=0; G.frame=0;
}
void game_init(void){
    memset(&G,0,sizeof G); g_rand_seed=0.0;
    build_field(); build_strip(); reset_game(); G.started=1;
}

/* -------- entities -------- */
static const float LANE[5]={-0.72f,-0.36f,0.0f,0.36f,0.72f};
static void spawn_enemy(void){
    int i; for(i=0;i<MAX_ENEMY;i++) if(!G.enemy[i].active){
        Enemy*e=&G.enemy[i]; int cc[5]={152,70,232,186,214};
        e->active=1; e->z=G.scroll+34.0f+(float)dh_rand()*6.0f;
        e->x=LANE[(int)(dh_rand()*5)%5]+(float)(dh_rand()-0.5)*0.06f;
        e->vx=(float)((dh_rand()-0.5)*0.05); e->color=cc[(int)(dh_rand()*5)%5];
        e->hp=1; return;
    }
}
static void fire_bullet(float x){ int i; for(i=0;i<MAX_BULLET;i++) if(!G.bullet[i].active){
    G.bullet[i].active=1; G.bullet[i].z=G.scroll+P_DEPTH+1.0f; G.bullet[i].x=x; return; } }
static void spawn_debris(float z,float x){ int i,n=0; for(i=0;i<MAX_DEBRIS&&n<20;i++) if(!G.debris[i].active){
    double a=dh_rand()*6.2834,m=dh_rand(); Debris*d=&G.debris[i];
    d->active=1; d->z=z; d->x=x; d->vz=(float)(cos(a)*m*3.0); d->vx=(float)(sin(a)*m*0.8);
    d->life=0.5f+(float)dh_rand()*0.4f; n++; } }
/* a lane-end barrier: a wall blocking one lane, running along the road */
static void spawn_wall(void){ int i; for(i=0;i<MAX_WALL;i++) if(!G.wall[i].active){
    Wall*w=&G.wall[i]; w->active=1; w->z=G.scroll+38.0f+(float)dh_rand()*6.0f;
    w->x=LANE[(int)(dh_rand()*5)%5]; w->len=4.0f+(float)dh_rand()*8.0f; return; } }

void game_tick(float dt){
    int i,j; G.frame++;
    if(G.recover>0) G.recover-=dt;

    if(G.recover<=0){
        if(G.k_gas)   G.speed+=9.0f*dt;
        if(G.k_brake) G.speed-=14.0f*dt;
    }
    G.speed-=G.speed*0.4f*dt;
    if(G.speed<0)G.speed=0; if(G.speed>16.0f)G.speed=16.0f;
    { float s=(G.recover>0)?0.0f:((G.k_left?-1.0f:0)+(G.k_right?1.0f:0));
      G.steer+=(s-G.steer)*8.0f*dt;
      G.posx+=G.steer*(1.1f+G.speed*0.05f)*dt; }
    if(G.posx<-0.95f){G.posx=-0.95f;} if(G.posx>0.95f){G.posx=0.95f;}
    G.scroll+=G.speed*dt;

    G.fire_cd-=dt; G.wpn_cd-=dt;
    if(G.recover<=0 && G.k_fire&&G.fire_cd<=0){ fire_bullet(G.posx); G.fire_cd=0.16f; }
    if(G.recover<=0 && G.k_weapon&&G.wpn_cd<=0){ fire_bullet(G.posx-0.3f); fire_bullet(G.posx); fire_bullet(G.posx+0.3f); G.wpn_cd=0.4f; }

    for(i=0;i<MAX_BULLET;i++) if(G.bullet[i].active){
        G.bullet[i].z+=26.0f*dt;
        if(G.bullet[i].z-G.scroll>36.0f) G.bullet[i].active=0; }

    /* enemies: sprites only, NO collision with the player (as in the original) */
    G.spawn_timer-=dt;
    if(G.spawn_timer<=0){ spawn_enemy(); G.spawn_timer=0.6f+(float)dh_rand()*0.7f; }
    for(i=0;i<MAX_ENEMY;i++) if(G.enemy[i].active){
        Enemy*e=&G.enemy[i];
        e->x+=e->vx*dt; if(e->x<-0.9f||e->x>0.9f)e->vx=-e->vx;
        if(e->z-G.scroll<16.0f) e->active=0;
    }
    /* lane-end barriers: hitting one is the only way to crash */
    G.wall_timer-=dt;
    if(G.wall_timer<=0){ spawn_wall(); G.wall_timer=2.5f+(float)dh_rand()*3.0f; }
    for(i=0;i<MAX_WALL;i++) if(G.wall[i].active){
        Wall*w=&G.wall[i]; float depth=w->z-G.scroll;
        if(depth<14.0f){ w->active=0; continue; }
        if(G.recover<=0 && depth<P_DEPTH+0.8f && depth>P_DEPTH-1.2f && fabsf(w->x-G.posx)<0.22f){
            G.recover=1.3f; G.hitflash=8; G.speed=0; G.steer=0;   /* CRASH (spin-out) */
            spawn_debris(G.scroll+P_DEPTH,G.posx); spawn_debris(G.scroll+P_DEPTH,G.posx);
        }
    }
    /* bullets destroy enemy cars (the hunt) */
    for(i=0;i<MAX_BULLET;i++) if(G.bullet[i].active)
        for(j=0;j<MAX_ENEMY;j++) if(G.enemy[j].active)
            if(fabsf(G.bullet[i].z-G.enemy[j].z)<1.8f && fabsf(G.bullet[i].x-G.enemy[j].x)<0.22f){
                G.bullet[i].active=0; G.enemy[j].active=0; G.score+=100; spawn_debris(G.enemy[j].z,G.enemy[j].x); break;
            }
    for(i=0;i<MAX_DEBRIS;i++) if(G.debris[i].active){
        Debris*d=&G.debris[i]; d->z+=d->vz*dt; d->x+=d->vx*dt; d->life-=dt;
        if(d->life<=0)d->active=0; }
    if(G.hitflash>0)G.hitflash--;
}

/* -------- rendering -------- */
static void px_set(int x,int y,unsigned char c){ if((unsigned)x<SCR_W&&(unsigned)y<SCR_H) g_fb[y*FB_PITCH+x]=c; }
static void fillrect(int x0,int y0,int x1,int y1,unsigned char c){
    int x,y; if(x0<0)x0=0; if(y0<0)y0=0; if(x1>SCR_W)x1=SCR_W; if(y1>SCR_H)y1=SCR_H;
    for(y=y0;y<y1;y++){ unsigned char*r=g_fb+y*FB_PITCH; for(x=x0;x<x1;x++) r[x]=c; } }

/* screen row for a world depth (inverse of worldZ mapping) */
static float depth_to_y(float depth){ return DZ/depth + VY; }
static float y_to_hw(float y){ return HW_BOT*(y-VY)/(480.0f-VY); }
static float curve_at(float wz){ return 46.0f*sinf(wz*0.10f)+22.0f*sinf(wz*0.043f+1.3f); }

/* top-down car sprite pointing up, size w x h at (cx,cy) */
static void draw_car(int cx,int cy,int w,int h,unsigned char body,unsigned char roof,unsigned char win){
    int x0,y0; if(w<4)w=4; if(h<6)h=6; x0=cx-w/2; y0=cy-h/2;
    fillrect(x0+1,y0+h/8,x0+w-1,y0+h,16);                 /* shadow */
    fillrect(x0,y0+h/6,x0+w,y0+h,body);                   /* body */
    fillrect(x0+1,y0,x0+w-1,y0+h/6,body);                 /* nose */
    fillrect(x0+2,y0+h/5,x0+w-2,y0+2*h/5,win);            /* windshield */
    fillrect(x0+2,y0+2*h/5,x0+w-2,y0+3*h/5,roof);         /* roof */
    fillrect(x0+2,y0+3*h/5,x0+w-2,y0+7*h/10,win);         /* rear window */
    px_set(x0+1,y0+h-2,140); px_set(x0+w-2,y0+h-2,140);   /* tail lights */
    fillrect(x0-1,y0+h/6,x0,y0+h/2,16);                   /* wheels */
    fillrect(x0+w,y0+h/6,x0+w+1,y0+h/2,16);
}

/* raised concrete barrier running along a lane (the "lane end") */
static void draw_wall(Wall*w){
    float dN=w->z-G.scroll, dF=w->z+w->len-G.scroll; int yy,yN,yF;
    if(dN<14.0f)dN=14.0f; if(dF>40.0f)dF=40.0f; if(dF<=dN) return;
    yN=(int)depth_to_y(dN); yF=(int)depth_to_y(dF);
    for(yy=yF;yy<=yN;yy++){
        float fy=(float)yy, hw=y_to_hw(fy), depth=DZ/(fy-VY);
        float c=CX+curve_at(G.scroll+depth)*(hw/HW_BOT)+w->x*hw;
        int half=(int)(hw*0.11f)+2, hgt=(int)(hw*0.10f)+3;
        fillrect((int)c-half,yy-hgt,(int)c+half,yy,30);        /* wall face */
        fillrect((int)c-half,yy-hgt-2,(int)c+half,yy-hgt,46);  /* lit top cap */
        fillrect((int)c-half,yy,(int)c+half,yy+1,14);          /* base shadow */
    }
}

void game_render(void){
    int x,y;
    for(y=0;y<SCR_H;y++){
        float fy=(float)y;
        float depth=DZ/(fy-VY);
        float worldZ=G.scroll+depth;
        float hw=y_to_hw(fy);
        float center=CX + curve_at(worldZ)*(hw/HW_BOT) - G.posx*hw*0.0f; /* fixed road */
        int left=(int)(center-hw), right=(int)(center+hw);
        int zt=((int)(worldZ*2.0f))&1;
        unsigned char*row=g_fb+y*FB_PITCH;
        int v; unsigned char*srow=NULL;
        /* grass (soft blotches from the same terrain field) */
        { int gx; float gz=worldZ*0.6f;
          for(gx=0;gx<SCR_W;gx++){
            float gsh=field_at(gz,(float)gx/(float)SCR_W);
            row[gx]=(unsigned char)(C_GRASS-3+(int)(gsh*6.0f)); } }
        if(right<=left){ continue; }
        /* names sampled per row */
        if(g_strip){ v=((int)(worldZ*NAME_DENSITY))%STRIP_H; if(v<0)v+=STRIP_H; srow=g_strip+v*STRIP_W; }
        { int xx0=left<0?0:left, xx1=right>SCR_W?SCR_W:right;
          float invw=1.0f/(right-left);
          for(x=xx0;x<xx1;x++){
            float ax=(x-left)*invw;                    /* 0..1 across road */
            float sh=field_at(worldZ,ax);              /* terrain shadow 0..1 */
            int idx=C_ROAD_HI-(int)(sh*C_SHADE_RNG);
            /* lane dashes: 5 lanes -> 4 interior lines + edges */
            { float ac=ax*2.0f-1.0f; int li; float best=1;
              for(li=-2;li<=2;li++){ float d=fabsf(ac-li*0.4f); if(d<best)best=d; }
              if(best<0.018f && zt) idx=C_LANE;
              if((ax<0.02f||ax>0.98f)) idx=C_LANE-4;   /* road edge line */
            }
            /* dev names on tarmac (kept off the very edges) */
            if(srow && ax>0.06f && ax<0.94f){ int u=(int)(ax*STRIP_W); if(u>=0&&u<STRIP_W){ unsigned char s=srow[u];
                if(s==1) idx=C_NAME_W; else if(s==2) idx=C_NAME_Y; } }
            row[x]=(unsigned char)idx;
          }
        }
    }

    /* lane-end barriers (draw far ones first) */
    { int i,j; Wall*ord[MAX_WALL]; int n=0;
      for(i=0;i<MAX_WALL;i++) if(G.wall[i].active) ord[n++]=&G.wall[i];
      for(i=1;i<n;i++){Wall*a=ord[i];int b=i;while(b>0&&ord[b-1]->z<a->z){ord[b]=ord[b-1];b--;}ord[b]=a;}
      for(j=0;j<n;j++) draw_wall(ord[j]); }

    /* enemies far->near */
    { int order[MAX_ENEMY],n=0,i,k;
      for(i=0;i<MAX_ENEMY;i++) if(G.enemy[i].active) order[n++]=i;
      for(i=1;i<n;i++){int a=order[i],b=i;while(b>0&&G.enemy[order[b-1]].z<G.enemy[a].z){order[b]=order[b-1];b--;}order[b]=a;}
      for(k=0;k<n;k++){ Enemy*e=&G.enemy[order[k]]; float depth=e->z-G.scroll;
        if(depth<16.0f||depth>40.0f) continue;
        { float fy=depth_to_y(depth); float hw=y_to_hw(fy);
          float cxr=CX+curve_at(e->z)*(hw/HW_BOT);
          int scx=(int)(cxr+e->x*hw), scy=(int)fy;
          int w=(int)(hw*0.16f)+6, h=(int)(hw*0.24f)+9;
          draw_car(scx,scy,w,h,(unsigned char)e->color,(unsigned char)(e->color-4),(unsigned char)(e->color+6)); }
      }
    }
    /* bullets */
    { int i; for(i=0;i<MAX_BULLET;i++) if(G.bullet[i].active){ float depth=G.bullet[i].z-G.scroll;
        if(depth<16.0f||depth>40.0f) continue;
        { float fy=depth_to_y(depth); float hw=y_to_hw(fy);
          int scx=(int)(CX+curve_at(G.bullet[i].z)*(hw/HW_BOT)+G.bullet[i].x*hw), scy=(int)fy;
          fillrect(scx-1,scy-3,scx+2,scy+3,141); } } }
    /* debris */
    { int i; for(i=0;i<MAX_DEBRIS;i++) if(G.debris[i].active){ float depth=G.debris[i].z-G.scroll;
        if(depth<16.0f||depth>40.0f) continue;
        { float fy=depth_to_y(depth); float hw=y_to_hw(fy);
          int scx=(int)(CX+curve_at(G.debris[i].z)*(hw/HW_BOT)+G.debris[i].x*hw), scy=(int)fy;
          px_set(scx,scy,(unsigned char)(130+(G.frame&7))); px_set(scx+1,scy,138); } } }

    /* player car near the bottom centre; steers within the road.
     * blinks while recovering from a crash (spin-out) */
    if(G.recover<=0 || (G.frame&2)){
      float fy=depth_to_y(P_DEPTH); float hw=y_to_hw(fy);
      int scx=(int)(CX+curve_at(G.scroll+P_DEPTH)*(hw/HW_BOT)+G.posx*hw);
      int scy=(int)fy+18; int lean=(int)(G.steer*6.0f);
      draw_car(scx+lean,scy,(int)(hw*0.17f)+7,(int)(hw*0.26f)+11,152,146,158); }

    if(G.hitflash>0 && (G.hitflash&1)) fillrect(0,0,SCR_W,SCR_H,136);
}
