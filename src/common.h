#ifndef ABISSO_COMMON_H
#define ABISSO_COMMON_H

#include <psptypes.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Colore GU: formato ABGR in memoria (0xAABBGGRR come valore). */
#define COL(r,g,b,a) (((unsigned int)(a)<<24)|((unsigned int)(b)<<16)|((unsigned int)(g)<<8)|((unsigned int)(r)))
#define COL_A(c)  (((c)>>24)&0xFF)
#define COL_R(c)  ((c)&0xFF)
#define COL_G(c)  (((c)>>8)&0xFF)
#define COL_B(c)  (((c)>>16)&0xFF)

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

#define TILE_PX   22          /* TILE originale 26/22 a seconda dello schermo; PSP fisso 22 */
#define SCR_W     480
#define SCR_H     272

static inline float clampf(float v,float a,float b){ return v<a?a:(v>b?b:v); }
static inline float dist2f(float ax,float ay,float bx,float by){
    float dx=bx-ax, dy=by-ay; return dx*dx+dy*dy;
}
static inline float lerpf(float a,float b,float t){ return a+(b-a)*t; }
static inline float signf(float v){ return (v>0.f)-(v<0.f); }

/* hashStr del gioco originale (FNV-1a 32bit). */
static inline unsigned int hashStr(const char* s){
    unsigned int h = 0x811c9dc5u;
    for (; *s; ++s){ h ^= (unsigned char)*s; h *= 0x01000193u; }
    return h;
}

/* mulberry32 del gioco originale, tradotta 1:1 (semantica uint32 di JS). */
typedef struct { unsigned int a; } Rng;
static inline void rng_seed(Rng* r, unsigned int seed){ r->a = seed; }
static inline float rng_next(Rng* r){
    unsigned int a = r->a + 0x6D2B79F5u;
    r->a = a;
    unsigned int t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return (float)((t ^ (t >> 14)) / 4294967296.0);
}
/* Math.random(): PRNG globale non deterministico. */
float frand(void);

#endif
