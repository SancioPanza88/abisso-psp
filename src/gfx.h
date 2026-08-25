#ifndef ABISSO_GFX_H
#define ABISSO_GFX_H

#include <psptypes.h>
#include "atlas_data.h"

/* Renderer 2D su GE: quadi testurizzati dall'atlas degli asset originali,
   con tint/luminosita' nel colore vertice (GU_TFX_MODULATE) come il
   globalAlpha/globalCompositeOperation del canvas HTML. */

int  gfxInit(void);
void gfxShutdown(void);

void gfxFrameStart(float camX, float camY, unsigned int clearColor);
void gfxFrameEnd(void);

/* coordinate mondo -> schermo (usa la camera corrente) */
void gfxWorldToScreen(float wx, float wy, float* sx, float* sy);
float gfxCamX(void);
float gfxCamY(void);

/* solidi in spazio schermo */
void gfxQuad(float x, float y, float w, float h, unsigned int col);
void gfxQuadScreenSpace(int enable); /* internamente gestito: le UI usano lo schermo */

/* sprite dell'atlas centrato in (cx,cy), lato `size` pixel */
void gfxDrawSprite(int atlasId, float cx, float cy, float size,
                   float alpha, int flipX, float flash01);
void gfxDrawSpriteRect(const ARect* r, float cx, float cy, float dw, float dh,
                       float alpha, int flipX, float flash01);

/* bagliore additivo radiale (radialGradient del canvas) */
void gfxGlow(float cx, float cy, float radius, unsigned int rgb, float intensity);

/* testo (font 5x7 incorporato, solo UI) */
void gfxText(float x, float y, const char* s, unsigned int col, float scale);
float gfxTextW(const char* s, float scale);

/* utility colore */
unsigned int scaleColRGB(unsigned int c, float f);
unsigned int withAlpha(unsigned int c, float alpha);

#endif
