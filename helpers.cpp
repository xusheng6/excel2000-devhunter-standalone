/* Dev Hunter helpers — leaf functions of MSOWC.DLL, translated 1:1. */
#include <math.h>
#include "devhunter.h"

/* sub_3c8d36e0: float LCG in [0,1) */
double dh_rand(void){
    double t = g_rand_seed*9821.0 + 0.211327;      /* 0.21132699999999999 */
    double r = t - (double)FTOL(t);
    g_rand_seed = r;
    return r;
}
int    dh_iabs(int v){ return v<0?-v:v; }          /* sub_3c7897e4 */
double dh_fabs(double v){ return v<0?-v:v; }       /* sub_3c8f74d4 */

/* sub_3c7de3b7: emit 66 explosion particles at (ax,ay), base velocity (avx,avy) */
int spawn_burst(float ax, float ay, float avx, float avy){
    int i_1 = 0x42, i, ret = 0;
    do {
        int idx = i32(0x9a6a4);
        double ang = dh_rand()*6.2834;
        double mag = dh_rand();
        double s   = sin(ang);
        double c   = cos(ang);
        double r13 = c*mag*0.375;
        float  r5;
        *(float*)(S + (idx + 0x7ac1)*0x14) = (float)(dh_rand() + 20.0);
        *(float*)(S + idx*0x14 + 0x99704) = (float)(avx + s*mag*6.0);
        r5 = (float)(avy + r13);
        *(float*)(S + idx*0x14 + 0x99708) = r5;
        *(float*)(S + idx*0x14 + 0x9970c) = (float)(r5 + ax);
        *(float*)(S + idx*0x14 + 0x99710) = (float)(r13 + ay);
        i = i_1; i_1--;
        i32(0x9a6a4) = (idx+1) % 0xc8;
        ret = (idx+1) / 0xc8;
    } while (i != 1);
    return ret;
}

/* sub_3c7dca79: bilinear splat of `amt` into the 64x64 int grid at field 0x910f4 */
int splat(double x, double y, int amt){
    int ix  = FTOL(x);
    int iy  = FTOL(y);
    int xf  = FTOL((x-(double)ix)*256.0);
    int yf  = FTOL((y-(double)iy)*256.0);
    int row = (iy&0x3f)<<6;
    int col = ix&0x3f;
    int col1= (ix+1)&0x3f;
    int row1= ((iy+1)&0x3f)<<6;
    int r;
    unsigned char *base = pp(0x910f4);
    *(int*)(base + ((row +col )<<2)) += ((0x100-yf)*(0x100-xf)*amt) >> 0x10;
    *(int*)(pp(0x910f4) + ((row +col1)<<2)) += ((0x100-yf)*xf*amt) >> 0x10;
    *(int*)(base + ((col +row1)<<2)) += (yf*(0x100-xf)*amt) >> 0x10;
    r = (yf*xf*amt) >> 0x10;
    *(int*)(base + ((col1+row1)<<2)) += r;
    return r;
}

/* sub_3c7de66f: textured/shaded trapezoid fill (4x unroll collapsed). */
int raster_span(unsigned char *arg2, int arg3, int arg4, int arg5, int arg6,
                int arg7, int arg8, int arg9, int arg10, int arg11,
                int arg12, int arg13){
    unsigned char *tex = pp(0x850e8);
    int var8  = arg10<<2, ebx = arg4, var10 = arg11<<2;
    int esi1  = arg5-ebx, var14 = 0;
    int temp0, temp0_1, temp0_2, ecx1, edx4, result;
    if (esi1==0) return 0;
    temp0   = ((arg12-arg10)<<2)/esi1;
    temp0_1 = ((arg13-arg11)<<2)/esi1;
    temp0_2 = 0x8000/esi1;
    ecx1 = esi1;
    if (arg5>0x1e0) ecx1 = ecx1-arg5+0x1e0;
    edx4 = 0;
    if (ebx<0){ var8-=temp0*ebx; var10-=temp0_1*ebx; edx4=-ebx; var14=-(temp0_2*ebx); ebx=0; }
    result = arg3*ebx; arg2 += result;
    if (edx4<ecx1){
        int result_1 = arg9-arg7, eax26 = arg8-arg6;
        int edi2 = result_1*edx4, eax27 = eax26*edx4;
        int rows = ecx1-edx4;
        for(;;){
            int ebx1=var8, ecx5=eax27/esi1+arg6, esi2=0;
            int eax33=edi2/esi1+arg7, arg4v=eax33, edi4=eax33-ecx5;
            int eax41=(var14>>5)&0xffffffe0, temp0_5,temp0_6,k; /* asm: and al,0xe0 (low byte only) */
            if(edi4!=0){ temp0_5=(var10-ebx1)/edi4; temp0_6=0x8000/edi4; } else { temp0_5=temp0_6=0; }
            if(ecx5<0){ edi4=arg4v; ebx1-=temp0_5*ecx5; esi2=-(temp0_6*ecx5); ecx5=0; }
            if(arg4v>0x280){ arg4v=0x280; edi4=0x280-ecx5; }
            { unsigned char*row=arg2+ecx5;
              for(k=0;k<edi4;k++){
                int texval=*(int*)(tex+(((esi2>>0xa)+eax41)<<2));
                int idx=texval+((ebx1>>2)&0x1f00);
                *row++ = (unsigned char)IA(0x910f8, idx);
                ebx1+=temp0_5; esi2+=temp0_6; } }
            arg2+=arg3; var8+=temp0; var10+=temp0_1; var14+=temp0_2;
            eax27+=eax26; edi2+=result_1;
            if(--rows==0) break;
        }
    }
    return result;
}

/* sub_3c7de4a2: y-sort three verts (0x9a948..0x9a968) -> up to two spans */
int raster_tri(unsigned char *arg2, int arg3){
    int ebx=i32(0x9a954), var20=i32(0x9a948), edx=i32(0x9a950), result_1=i32(0x9a94c);
    int var28=edx, var18=i32(0x9a958), eax_3=i32(0x9a95c), var14=eax_3, result;
    if (ebx>eax_3){
        int edx_1=edx^var18, ebx_1=ebx^eax_3, eax_4;
        i32(0x9a964)^=i32(0x9a968); eax_4=eax_3^ebx_1; var18^=edx_1; edx=edx_1^var18;
        var14=eax_4; ebx=ebx_1^eax_4; i32(0x9a968)^=i32(0x9a964); i32(0x9a964)^=i32(0x9a968); var28=edx;
    }
    if (result_1>ebx){
        int var_1c=result_1^ebx, var20_1;
        i32(0x9a960)^=i32(0x9a964); var20_1=var20^edx; ebx^=var_1c; edx^=var20_1;
        result_1=var_1c^ebx; i32(0x9a964)^=i32(0x9a960); var20=var20_1^edx; i32(0x9a960)^=i32(0x9a964); var28=edx;
    }
    if (ebx>var14){
        int edx_2=edx^var18, ebx_2;
        i32(0x9a964)^=i32(0x9a968); ebx_2=ebx^var14; var18^=edx_2; edx=edx_2^var18;
        var14^=ebx_2; ebx=ebx_2^var14; i32(0x9a968)^=i32(0x9a964); var28=edx; i32(0x9a964)^=i32(0x9a968);
    }
    result=result_1;
    if (var14!=result){
        int ecx_7=ebx-result, edi_8=var14-result, var_c_1=edx, var_8_1=i32(0x9a964), ecx_8=i32(0x9a960);
        int edi_10, result_2;
        if(edi_8==0) return result;
        edi_10=((var18-var20)*ecx_7)/edi_8+var20;
        result=((i32(0x9a968)-i32(0x9a960))*ecx_7)/edi_8+ecx_8; result_2=result;
        if (var28>edi_10){
            int var_8_2=var_8_1^result, edx_8=edi_10^var28;
            result^=var_8_2; edi_10^=edx_8; result_2=result; var_8_1=var_8_2^result; var_c_1=edx_8^edi_10;
        }
        if (ebx!=result_1){
            raster_span(arg2,arg3,result_1,ebx,var20,var20+1,var_c_1,edi_10+1,ecx_8,ecx_8,var_8_1,result);
            result=result_2;
        }
        if (var14!=ebx){
            int ecx_11=i32(0x9a968);
            return raster_span(arg2,arg3,ebx,var14,var_c_1,edi_10+1,var18,var18+1,var_8_1,result,ecx_11,ecx_11);
        }
    }
    return result;
}
