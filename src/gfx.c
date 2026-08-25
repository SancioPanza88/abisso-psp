#include "gfx.h"
#include "common.h"
#include <pspuser.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <psputils.h>

/* =====================================================================
   Renderer: pipeline GU identica ai sample ufficiali pspsdk (gu/blit).
   ===================================================================== */

#define BUF_WIDTH  512
#define LIST_SIZE  262144

static unsigned int __attribute__((aligned(16))) s_list[LIST_SIZE];

/* ---- atlas (asset originali, RLE incorporato via bin2o) ---- */
extern const unsigned char atlas_rle_start[];
extern const unsigned char atlas_rle_end[];
extern const unsigned int  atlas_rle_size;
static unsigned int __attribute__((aligned(16))) s_atlasPix[ATLAS_W*ATLAS_H];

int atlasLoad(void){
    /* decompressione RLE di assets/atlas.rle: coppie [u16 run][u32 pixel ABGR] */
    const unsigned char* p = atlas_rle_start;
    unsigned int total = ATLAS_W*ATLAS_H, i = 0;
    while (i < total){
        unsigned short run; unsigned int px;
        run  = (unsigned short)(p[0] | (p[1]<<8));
        px   = (unsigned int)p[2] | ((unsigned int)p[3]<<8) |
               ((unsigned int)p[4]<<16) | ((unsigned int)p[5]<<24);
        p += 6;
        {
            unsigned int k;
            for (k=0; k<run && i<total; ++k) s_atlasPix[i++] = px;
        }
    }
    sceKernelDcacheWritebackRange(s_atlasPix, sizeof(s_atlasPix));
    return (int)(atlas_rle_end - atlas_rle_start);
}
unsigned int* atlasPixels(void){ return s_atlasPix; }

/* ---- font 5x7 classico (solo UI; il gioco usa gli sprite originali) ---- */
static const unsigned char FONT5X7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x20,0x20,0x20,0x20},
    {0x7E,0x04,0x08,0x04,0x7E},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},{0x08,0x04,0x08,0x10,0x08}
};
#define FONT_TW 128
#define FONT_TH 64
static unsigned int __attribute__((aligned(16))) s_fontPix[FONT_TW*FONT_TH];

/* ---- stato ---- */
typedef struct { float u,v; unsigned int c; float x,y,z; } VT;
typedef struct { unsigned int c; float x,y,z; } VC;

#define VTYPE_TEX (GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D)
#define VTYPE_COL (GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D)

static float s_camX=0.f, s_camY=0.f;
static int   s_blendAdd = -1;
static int   s_texBound = -1;   /* 0=atlas 1=font -1=none */
static unsigned int s_clearCol = COL(10,9,6,255);

void gfxQuadScreenSpace(int enable){ (void)enable; }

unsigned int scaleColRGB(unsigned int c, float f){
    unsigned int r=(unsigned int)(COL_R(c)*f), g=(unsigned int)(COL_G(c)*f),
                 b=(unsigned int)(COL_B(c)*f);
    if (r>255) r=255; if (g>255) g=255; if (b>255) b=255;
    return (c & 0xFF000000u) | (b<<16) | (g<<8) | r;
}
unsigned int withAlpha(unsigned int c, float alpha){
    unsigned int a=(unsigned int)(alpha*255.f); if (a>255) a=255;
    return (a<<24) | (c & 0x00FFFFFFu);
}

static void setBlend(int add){
    if (s_blendAdd==add) return;
    if (add) sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_FIX,0,0xFFFFFF);
    else     sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    s_blendAdd=add;
}
void gfxBindAtlas(int on){
    if (s_texBound==on) return;
    if (on){
        sceGuTexMode(GU_PSM_8888,0,0,0);
        sceGuTexImage(0,ATLAS_W,ATLAS_H,ATLAS_W,s_atlasPix);
    } else {
        sceGuTexMode(GU_PSM_8888,0,0,0);
        sceGuTexImage(0,FONT_TW,FONT_TH,FONT_TW,s_fontPix);
    }
    s_texBound=on;
}

int gfxInit(void){
    memset(s_atlasPix,0,sizeof(s_atlasPix));
    if (atlasLoad() <= 0) return 0;

    /* baking del font */
    {
        int idx,row,col,x,y;
        unsigned int* pix = s_fontPix;
        memset(pix,0,sizeof(s_fontPix));
        for (idx=0; idx<95; ++idx){
            int gx=(idx%16)*6, gy=(idx/16)*8;
            for (col=0; col<5; ++col){
                unsigned char bits=FONT5X7[idx][col];
                for (row=0; row<7; ++row)
                    if (bits & (1<<row)){
                        x=gx+col; y=gy+row;
                        pix[y*FONT_TW+x]=0xFFFFFFFFu;
                    }
            }
        }
        sceKernelDcacheWritebackRange(s_fontPix,sizeof(s_fontPix));
    }

    /* framebuffer VRAM: fbp0 @0, fbp1 @fsz, zbp @2*fsz (8888/8888/4444) */
    sceGuInit();
    sceGuStart(GU_DIRECT,s_list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
    sceGuDispBuffer(SCR_W,SCR_H,(void*)(BUF_WIDTH*SCR_H*4),BUF_WIDTH);
    sceGuDepthBuffer((void*)(BUF_WIDTH*SCR_H*8),BUF_WIDTH);
    sceGuOffset(2048-(SCR_W/2),2048-(SCR_H/2));
    sceGuViewport(2048,2048,SCR_W,SCR_H);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SCR_W,SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    s_blendAdd=0;
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER,0,0xFF);
    sceGuTexMode(GU_PSM_8888,0,0,0);
    sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST,GU_NEAREST);
    sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    sceGuClearColor(s_clearCol);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceGuDisplay(1);

    s_texBound=-1;
    gfxBindAtlas(1);
    return 1;
}

void gfxShutdown(void){
    sceGuTerm();
}

void gfxFrameStart(float camX,float camY,unsigned int clearColor){
    s_camX=camX; s_camY=camY;
    if (clearColor!=s_clearCol){
        s_clearCol=clearColor;
        sceGuClearColor(clearColor);
    }
    sceGuStart(GU_DIRECT,s_list);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    s_texBound=-1;
    gfxBindAtlas(1);
    setBlend(0);
}

void gfxFrameEnd(void){
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void gfxWorldToScreen(float wx,float wy,float* sx,float* sy){
    *sx = (wx-s_camX)*(float)TILE_PX;
    *sy = (wy-s_camY)*(float)TILE_PX;
}
float gfxCamX(void){ return s_camX; }
float gfxCamY(void){ return s_camY; }

void gfxQuad(float x,float y,float w,float h,unsigned int col){
    VC* v = (VC*)sceGuGetMemory(sizeof(VC)*2);
    v[0].c=col; v[0].x=x;   v[0].y=y;   v[0].z=0;
    v[1].c=col; v[1].x=x+w; v[1].y=y+h; v[1].z=0;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES,VTYPE_COL,2,0,v);
    sceGuEnable(GU_TEXTURE_2D);
}

void gfxDrawSpriteRect(const ARect* r,float cx,float cy,float dw,float dh,
                       float alpha,int flipX,float flash01){
    VT* v;
    unsigned int col;
    float x0=cx-dw*0.5f, y0=cy-dh*0.5f;
    if (alpha<=0.004f) return;
    col = withAlpha(0xFFFFFFFFu,alpha);
    v = (VT*)sceGuGetMemory(sizeof(VT)*2);
    if (!flipX){
        v[0].u=(float)r->u;        v[0].v=(float)r->v;
        v[1].u=(float)(r->u+r->w); v[1].v=(float)(r->v+r->h);
    } else {
        v[0].u=(float)(r->u+r->w); v[0].v=(float)r->v;
        v[1].u=(float)r->u;        v[1].v=(float)(r->v+r->h);
    }
    v[0].c=col; v[0].x=x0; v[0].y=y0; v[0].z=0;
    v[1].c=col; v[1].x=x0+dw; v[1].y=y0+dh; v[1].z=0;
    gfxBindAtlas(1);
    setBlend(0);
    sceGuDrawArray(GU_SPRITES,VTYPE_TEX,2,0,v);
    if (flash01>0.001f){
        /* flash bianco sul colpo: quad additivo sulla stessa area */
        unsigned int fc = withAlpha(0x00FFFFFFu, flash01*0.85f*alpha);
        v[0].c=fc; v[1].c=fc;
        setBlend(1);
        sceGuDrawArray(GU_SPRITES,VTYPE_TEX,2,0,v);
        setBlend(0);
    }
}

void gfxDrawSprite(int atlasId,float cx,float cy,float size,
                   float alpha,int flipX,float flash01){
    gfxDrawSpriteRect(&g_atlasRects[atlasId],cx,cy,size,size,alpha,flipX,flash01);
}

void gfxGlow(float cx,float cy,float radius,unsigned int rgb,float intensity){
    VT* v;
    unsigned int col;
    const ARect* r=&g_atlasRects[AR_GLOW];
    if (intensity<=0.004f || radius<1.f) return;
    col = withAlpha(rgb,intensity);
    v=(VT*)sceGuGetMemory(sizeof(VT)*2);
    v[0].u=(float)r->u;        v[0].v=(float)r->v;
    v[1].u=(float)(r->u+r->w); v[1].v=(float)(r->v+r->h);
    v[0].c=col; v[0].x=cx-radius; v[0].y=cy-radius; v[0].z=0;
    v[1].c=col; v[1].x=cx+radius; v[1].y=cy+radius; v[1].z=0;
    gfxBindAtlas(1);
    setBlend(1);
    sceGuDrawArray(GU_SPRITES,VTYPE_TEX,2,0,v);
    setBlend(0);
}

void gfxText(float x,float y,const char* s,unsigned int col,float scale){
    VT* base; int n=0,i,len;
    if (!s) return;
    len=(int)strlen(s);
    base=(VT*)sceGuGetMemory(sizeof(VT)*2*(unsigned)len);
    gfxBindAtlas(0);
    setBlend(0);
    for (i=0;i<len;++i){
        unsigned char ch=(unsigned char)s[i];
        int idx; VT* v;
        float gx,gy,u0,v0;
        if (ch==' '){ x+=6.f*scale; continue; }
        if (ch>=32 && ch<127) idx=ch-32; else idx='?'-32;
        gx=(float)((idx%16)*6); gy=(float)((idx/16)*8);
        u0=gx; v0=gy;
        v=&base[n*2];
        v[0].u=u0;      v[0].v=v0;
        v[1].u=u0+5.f;  v[1].v=v0+7.f;
        v[0].c=col; v[0].x=x;       v[0].y=y; v[0].z=0;
        v[1].c=col; v[1].x=x+5.f*scale; v[1].y=y+7.f*scale; v[1].z=0;
        n++;
        x+=6.f*scale;
    }
    if (n>0) sceGuDrawArray(GU_SPRITES,VTYPE_TEX,n*2,0,base);
    gfxBindAtlas(1);
}
float gfxTextW(const char* s,float scale){
    if (!s) return 0;
    return (float)strlen(s)*6.f*scale;
}
