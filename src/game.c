#include "game.h"
#include "gfx.h"
#include "audio.h"
#include "ui.h"
#include "atlas_data.h"
#include "net.h"
#include <pspuser.h>
#include <string.h>

/* =====================================================================
   Simulazione — traduzione 1:1 dall'index.html originale.
   Ogni funzione corrisponde alla controparte JS citata nel commento.
   ===================================================================== */

#define C_HEX(rr,gg,bb) COL(0x##rr,0x##gg,0x##bb,255)
#define DEPTH_FADE_TIME 0.55f
#define FOV_RADIUS 7.4f
#define DOWNED_BLEEDOUT 22.f

World g_world;
Player g_me;
float g_torchClock=0.f, g_animClock=0.f;
float g_shakeTrauma=0.f, g_hitstopTimer=0.f, g_dmgFlashTimer=0.f;
float g_critFlashTimer=0.f, g_bossDeathFlashTimer=0.f, g_depthFadeTimer=0.f;
int   g_minimapVisible=1;
int   g_recordGold=0, g_recordDepth=1;

static float s_lastDepthChangeT=-9999.f;
static float s_respawnTick=10.f, s_powerupTick=20.f;
static char s_toast[128]; static float s_toastT=0;
static char s_bannerText[96]; unsigned int s_bannerColor; static float s_bannerT=0;
#define LOG_N 4
static char s_log[LOG_N][112]; static unsigned int s_logCol[LOG_N]; static int s_logHead=0;

/* pool globali degli effetti (definiti qui, dichiarati in game.c) */
Part       g_parts[PART_CAP];
Proj       g_projs[PROJ_CAP];
FloatText  g_floats[FLOAT_CAP];
Shockwave  g_shocks[16];
SpellFlash g_flashes[24];

/* PRNG globale in stile Math.random() */
static unsigned int s_randState=0;
void gameRandSeed(unsigned int s){ s_randState = s? s : 1u; }
float frand(void){
    if (!s_randState) s_randState = 0x9E3779B9u;
    s_randState = s_randState*1664525u + 1013904223u;
    return (float)(s_randState / 4294967296.0);
}

extern unsigned int g_worldSeed; /* definita in dungeon.c: seed di mondo */

/* ------------------------------------------------------------------ */
/* Toast, banner, log                                                  */
/* ------------------------------------------------------------------ */
void showToast(const char* text){ snprintf(s_toast,sizeof s_toast,"%s",text); s_toastT=2.2f; }
void showLootBanner(const char* icon,const char* text,unsigned int color){
    (void)icon;
    snprintf(s_bannerText,sizeof s_bannerText,"%s",text);
    s_bannerColor=color; s_bannerT=2.6f;
}
void logLine(const char* text,unsigned int color){
    snprintf(s_log[s_logHead],sizeof s_log[0],"%s",text);
    s_logCol[s_logHead]=color;
    s_logHead=(s_logHead+1)%LOG_N;
}
const char* currentToast(void){ return s_toastT>0.f ? s_toast : 0; }
float toastRemaining(void){ return s_toastT; }
const char* currentBanner(int* isIconLine){ *isIconLine=0; return s_bannerT>0.f ? s_bannerText : 0; }
float bannerRemaining(void){ return s_bannerT; }
unsigned int bannerColor(void){ return s_bannerColor; }
const char* logLineAt(int back){
    int idx=((s_logHead-1-back)%LOG_N+LOG_N)%LOG_N;
    return s_log[idx][0]? s_log[idx] : 0;
}
unsigned int logColorAt(int back){
    int idx=((s_logHead-1-back)%LOG_N+LOG_N)%LOG_N;
    return s_logCol[idx];
}

/* ------------------------------------------------------------------ */
/* Record permadeath: ms0:/ABISSO/RECORD.BIN                           */
/* ------------------------------------------------------------------ */
static void loadRecord(void){
    FILE* f=fopen("ms0:/ABISSO/RECORD.BIN","rb");
    unsigned int v[2];
    g_recordGold=0; g_recordDepth=1;
    if (!f) return;
    if (fread(v,4,2,f)==2){ g_recordGold=(int)v[0]; g_recordDepth=(int)v[1]; }
    fclose(f);
    if (g_recordDepth<1) g_recordDepth=1;
    if (g_recordGold<0) g_recordGold=0;
}
static void saveRecord(void){
    FILE* f;
    sceIoMkdir("ms0:/ABISSO",0777);
    f=fopen("ms0:/ABISSO/RECORD.BIN","wb");
    if (!f) return;
    { unsigned int v[2];
      v[0]=(unsigned int)g_recordGold; v[1]=(unsigned int)g_recordDepth;
      fwrite(v,4,2,f); }
    fclose(f);
}
static void updateRecordIfBetter(void){
    int changed=0;
    if (g_me.gold>g_recordGold){ g_recordGold=g_me.gold; changed=1; }
    if (g_world.depth>g_recordDepth){
        int isNewBest = (g_recordDepth>1);
        char buf[80];
        g_recordDepth=g_world.depth; changed=1;
        if (isNewBest){
            snprintf(buf,sizeof buf,"NUOVO RECORD - Piano %d",g_world.depth);
            showLootBanner("*",buf,C_HEX(FF,D3,5C));
        }
    }
    if (changed) saveRecord();
}

/* ------------------------------------------------------------------ */
/* Effetti visivi (particles, floats, shockwaves, flashes)             */
/* ------------------------------------------------------------------ */
static void addShake(float a){ if (a>g_shakeTrauma) g_shakeTrauma=a; else g_shakeTrauma+=a*0.3f; }
static void addHitstop(float t){ g_hitstopTimer+=t; }
static void flashDamage(void){ g_dmgFlashTimer=0.35f; }
static void addCritFlash(void){ g_critFlashTimer=0.12f; }

static Part* allocPart(void){
    int i;
    for (i=0;i<PART_CAP;++i) if (!g_parts[i].used) return &g_parts[i];
    return 0;
}
static void pfxBurst(float x,float y,int n,const unsigned int* colors,int ncol,
                     float spMin,float spMax,float szMin,float szMax,
                     float lfMin,float lfMax,int type,float grav){
    int k;
    for (k=0;k<n;++k){
        Part* p=allocPart();
        float ang,spd;
        if (!p) return;
        ang=frand()*2.f*M_PI;
        spd=spMin+(spMax-spMin)*frand();
        p->used=1; p->x=x; p->y=y;
        p->vx=cosf(ang)*spd; p->vy=sinf(ang)*spd;
        p->life=p->maxLife=lfMin+(lfMax-lfMin)*frand();
        p->size=szMin+(szMax-szMin)*frand();
        p->color=colors[(int)(frand()*ncol)%ncol];
        p->grav=grav; p->drag=(type==1)?0.94f:0.99f; p->type=type;
    }
}
static void hitBurst(float x,float y,unsigned int col){
    unsigned int cs[2]={col,0xFFFFFFFFu};
    pfxBurst(x,y,8,cs,2,1.f,3.5f,1.5f,3.2f,0.15f,0.35f,0,0);
}
static void killBurst(float x,float y,unsigned int col){
    unsigned int cs[2]={col,0xFFFFFFFFu};
    unsigned int sm[1]={C_HEX(33,33,33)};
    pfxBurst(x,y,18,cs,2,1.5f,5.f,2.f,4.5f,0.25f,0.55f,0,0);
    pfxBurst(x,y,6,sm,1,0.5f,2.f,3.f,6.f,0.4f,0.8f,1,-0.4f);
}
static void smokeBurst(float x,float y,unsigned int col,int n){
    unsigned int cs[1]={col};
    pfxBurst(x,y,n,cs,1,0.5f,2.f,3.f,6.f,0.4f,0.9f,1,-0.5f);
}
static void healRise(float x,float y,unsigned int col){
    unsigned int cs[1]={col};
    pfxBurst(x,y,10,cs,1,0.4f,1.2f,2.f,3.5f,0.35f,0.7f,0,-1.6f);
}
static void stepDust(float x,float y,float dx,float dy){
    unsigned int cs[1]={C_HEX(8A,7A,5C)};
    pfxBurst(x-dx*0.3f,y-dy*0.3f+0.25f,3,cs,1,0.2f,0.8f,1.5f,3.f,0.2f,0.45f,1,-0.2f);
}
static Shockwave* allocShock(void){
    int i; for(i=0;i<16;++i) if(!g_shocks[i].used) return &g_shocks[i]; return 0;
}
static void pushShockwave(float x,float y,float life,unsigned int col,float r){
    Shockwave* w=allocShock();
    if (!w) return;
    w->used=1; w->x=x; w->y=y; w->life=w->maxLife=life; w->color=col; w->r=r;
}
static SpellFlash* allocFlash(void){
    int i; for(i=0;i<24;++i) if(!g_flashes[i].used) return &g_flashes[i]; return 0;
}
static void spellFlash(float x,float y,unsigned int color,float intensity){
    SpellFlash* f=allocFlash();
    if (!f) return;
    f->used=1; f->x=x; f->y=y; f->color=color; f->intensity=intensity;
}
static FloatText* allocFloat(void){
    int i; for(i=0;i<FLOAT_CAP;++i) if(!g_floats[i].used) return &g_floats[i]; return 0;
}
static void spawnFloatText(float x,float y,const char* txt,unsigned int color){
    FloatText* f=allocFloat();
    if (!f) return;
    f->used=1; f->x=x; f->y=y; f->vy=-0.8f; f->life=1.1f; f->color=color;
    snprintf(f->txt,sizeof f->txt,"%s",txt);
}

/* ------------------------------------------------------------------ */
/* Equipaggiamento                                                     */
/* ------------------------------------------------------------------ */
EquipBonus computeEquipBonus(void){
    EquipBonus b={0,0,0,0};
    int s,i;
    for (s=0;s<EQ_SLOT_COUNT;++s){
        const EquipItem* it=&g_me.equip[s];
        for (i=0;i<it->statCount;++i){
            switch (it->statKey[i]){
                case ST_HP:        b.hp+=it->statVal[i]; break;
                case ST_DMG_PCT:   b.dmgPct+=it->statVal[i]; break;
                case ST_SPEED_PCT: b.speedPct+=it->statVal[i]; break;
                default:           b.armorPct+=it->statVal[i]; break;
            }
        }
    }
    return b;
}
static void applyClassStats(int cls){
    const ClassDef* c=&CLASSES[cls];
    EquipBonus eq=computeEquipBonus();
    g_me.cls=cls;
    g_me.maxHp=(float)(c->hp+eq.hp); g_me.hp=g_me.maxHp;
    g_me.maxMp=(float)c->maxMp;      g_me.mp=(float)c->maxMp;
    g_me.speed=c->speed;
}
static void statFormatList(const EquipItem* item,char* out,int outSz){
    int i,pos=0;
    out[0]=0;
    for (i=0;i<item->statCount;++i){
        const char* s=statFormat(item->statKey[i],item->statVal[i]);
        if (i) pos+=snprintf(out+pos,outSz-pos,", ");
        pos+=snprintf(out+pos,outSz-pos,"%s",s);
        if (pos>=outSz-1) break;
    }
}
static void tryEquipOrSalvage(const EquipItem* item){
    EquipItem* current=&g_me.equip[item->slot];
    int newPower=itemPower(item);
    char buf[128], stbuf[64];
    if (!current->statCount || newPower>=itemPower(current)){
        EquipBonus oldB=computeEquipBonus();
        *current=*item;
        {   EquipBonus newB=computeEquipBonus();
            int hpDelta=newB.hp-oldB.hp;
            g_me.maxHp+=(float)hpDelta;
            g_me.hp=fminf(g_me.maxHp,g_me.hp+(hpDelta>0?hpDelta:0));
        }
        statFormatList(item,stbuf,sizeof stbuf);
        snprintf(buf,sizeof buf,"Hai equipaggiato: %s %s (%s).",
                 EQUIP_SLOTS[item->slot].name,RARITIES[item->rarity].name,stbuf);
        spawnFloatText(g_me.x,g_me.y-0.7f,RARITIES[item->rarity].name,RARITIES[item->rarity].color);
        logLine(buf,RARITIES[item->rarity].color);
        if (item->rarity>=RAR_EPICO){
            showLootBanner("*",EQUIP_SLOTS[item->slot].name,RARITIES[item->rarity].color);
            spellFlash(g_me.x,g_me.y,RARITIES[item->rarity].color,4);
            addShake(0.2f);
            sfxPlay(SFX_GEM);
        }
    } else {
        int salvage=3+(int)(newPower*0.6f);
        char fb[40];
        g_me.gold+=salvage;
        snprintf(fb,sizeof fb,"+%d oro",salvage);
        spawnFloatText(g_me.x,g_me.y-0.7f,fb,C_HEX(D4,AF,37));
        logLine("Oggetto scomposto in oro (avevi gia' di meglio).",C_HEX(8A,7D,68));
    }
}

/* ------------------------------------------------------------------ */
/* Mostri: creazione                                                   */
/* ------------------------------------------------------------------ */
static int scaledStat(int base,int depth,float factor){
    return (int)(base*(1.f+factor*(depth-1))+0.5f);
}
static int pickMonsterType(Rng* rng,int depth){
    int keys[32],weights[32],n=0,i;
    float total=0.f,r;
    for (i=0;i<MONSTER_COUNT;++i)
        if (!monType(i)->boss){ keys[n]=i; weights[n]=monWeight(i,depth); total+=weights[n]; ++n; }
    if (total<=0.f) return MT_RATTO;
    r=rng_next(rng)*total;
    for (i=0;i<n;++i){ r-=weights[i]; if (r<=0) return keys[i]; }
    return keys[n-1];
}
static Monster* makeMonster(int type,float x,float y,int depth,int affix){
    const MonsterType* t=monType(type);
    Monster* m=0; int i;
    float hp;
    for (i=0;i<MONSTER_CAP;++i)
        if (g_world.monsters[i].type<0){ m=&g_world.monsters[i]; break; }
    if (!m) return 0;
    memset(m,0,sizeof *m);
    hp=(float)scaledStat(t->hp,depth,0.16f);
    m->type=type;
    m->id=g_world.nextMonsterId++;
    m->x=x+0.5f; m->y=y+0.5f; m->rx=m->x; m->ry=m->y;
    m->hp=m->maxHp=hp;
    m->dmg=scaledStat(t->dmg,depth,0.11f);
    m->speed=t->speed * ((affix==AFFIX_VELOCE)?1.6f:1.f);
    m->aggro=t->aggro;
    m->fx=0.f; m->fy=1.f;
    m->splitLeft=t->split?1:0;
    m->affix=t->boss?AFFIX_NONE:affix;
    m->bmMove[0]=0;
    return m;
}

/* ------------------------------------------------------------------ */
/* Mondo                                                               */
/* ------------------------------------------------------------------ */
static void resetFog(void){
    memset(g_world.visited,0,sizeof g_world.visited);
    memset(g_world.visible,0,sizeof g_world.visible);
    g_world.lastTileX=-1; g_world.lastTileY=-1;
}
static void placeAtSpawn(void){
    g_me.x=g_world.layout->spawnX+0.5f;
    g_me.y=g_world.layout->spawnY+0.5f;
}
static void applyBossGates(int sealed){
    Layout* L=g_world.layout;
    int i;
    if (!L || !L->hasBossRoom) return;
    for (i=0;i<L->gateCount;++i)
        L->grid[L->gates[i].y*L->w+L->gates[i].x] = sealed?T_WALL:T_FLOOR;
}
static void makeInitialItems(Layout* L,int depth,Rng* rng){
    int i;
    for (i=0;i<L->treasureCount && g_world.itemCount<ITEM_CAP;++i){
        Item* it=&g_world.items[g_world.itemCount++];
        it->used=1; it->kind=L->treasure[i].gem?IK_GEM:IK_GOLD;
        it->x=(float)L->treasure[i].x+0.5f; it->y=(float)L->treasure[i].y+0.5f;
        it->amount = L->treasure[i].gem ? 15+(int)(frand()*10*depth)
                                        : 3+(int)(frand()*6*depth);
    }
    if (L->powerupCount>0 && g_world.itemCount<ITEM_CAP){
        Item* it=&g_world.items[g_world.itemCount++];
        int si=(int)(rng_next(rng)*L->powerupCount)%L->powerupCount;
        it->used=1; it->kind=IK_POWER;
        it->x=(float)L->powerupSpots[si].x+0.5f;
        it->y=(float)L->powerupSpots[si].y+0.5f;
        it->buff=(int)(rng_next(rng)*PW_COUNT)%PW_COUNT;
    }
    for (i=0;i<L->potionCount && g_world.itemCount<ITEM_CAP;++i){
        Item* it=&g_world.items[g_world.itemCount++];
        it->used=1; it->kind=L->potionSpots[i].mana?IK_MANAPOT:IK_POTION;
        it->x=(float)L->potionSpots[i].x+0.5f;
        it->y=(float)L->potionSpots[i].y+0.5f;
        it->amount=1;
    }
}

static void clearMonstersAndItems(void){
    int i;
    for (i=0;i<MONSTER_CAP;++i) g_world.monsters[i].type=-1;
    for (i=0;i<ITEM_CAP;++i) g_world.items[i].used=0;
    g_world.monsterCount=0; g_world.itemCount=0;
}
static void populateDepth(int depth){
    Layout* L=g_world.layout;
    Rng dynRng;
    char seedStr[80];
    int i;
    clearMonstersAndItems();
    snprintf(seedStr,sizeof seedStr,"%u::%u::dyn::%d",g_worldSeed,g_worldSeed,depth);
    rng_seed(&dynRng,hashStr(seedStr));
    for (i=0;i<L->monSpotCount;++i){
        Rng affixRng;
        char aStr[32];
        int type=pickMonsterType(&dynRng,depth), affix;
        float chance=pickAffixChance(depth);
        snprintf(aStr,sizeof aStr,"%d_%d::affix",depth,i);
        rng_seed(&affixRng,hashStr(aStr));
        if (rng_next(&affixRng) > chance) affix=AFFIX_NONE;
        else affix=(int)(rng_next(&affixRng)*AFFIX_COUNT)%AFFIX_COUNT;
        makeMonster(type,(float)L->monSpots[i].x,(float)L->monSpots[i].y,depth,affix);
    }
    if (L->hasBossRoom)
        makeMonster(L->bossType,(float)L->bossRoom.x+L->bossRoom.w/2,
                    (float)L->bossRoom.y+L->bossRoom.h/2,depth,AFFIX_NONE);
}
static void onDepthArrival(int depth){
    char buf[96];
    resetFog();
    placeAtSpawn();
    { int i; for (i=0;i<PROJ_CAP;++i) g_projs[i].used=0; }
    snprintf(buf,sizeof buf,"Piano %d",depth);
    showToast(buf);
    logLine(buf,C_HEX(E8,A1,3D));
    g_depthFadeTimer=DEPTH_FADE_TIME;
    if (isBossFloor(depth)){
        char b2[128];
        snprintf(b2,sizeof b2,"Piano del Boss: %s e' in fondo alla mappa.",
                 currentBossName(depth));
        showToast(b2);
        logLine(b2,C_HEX(E8,A1,3D));
        sfxPlay(SFX_BOSS_ROAR);
        addShake(0.25f);
    }
}
void gameNewRun(int cls){
    memset(&g_world,0,sizeof g_world);
    memset(&g_me,0,sizeof g_me);
    memset(g_parts,0,sizeof g_parts);
    memset(g_projs,0,sizeof g_projs);
    memset(g_floats,0,sizeof g_floats);
    memset(g_shocks,0,sizeof g_shocks);
    memset(g_flashes,0,sizeof g_flashes);
    g_torchClock=g_animClock=0;
    g_shakeTrauma=g_hitstopTimer=g_dmgFlashTimer=0;
    g_critFlashTimer=g_bossDeathFlashTimer=g_depthFadeTimer=0;
    loadRecord();
    {   union { unsigned int u; unsigned char b[4]; } t;
        unsigned int tick=(unsigned int)sceKernelGetSystemTimeWide();
        t.u=tick^hashStr(__TIME__);
        g_worldSeed = t.b[0]|((unsigned int)t.b[1]<<8)|((unsigned int)t.b[2]<<16)|((unsigned int)t.b[3]<<24);
        if (!g_worldSeed) g_worldSeed=1;
    }

    g_world.depth=1;
    g_world.layout=generateDepth(1);
    g_world.bossActive=0; g_world.bossDead=0;
    g_world.nextMonsterId=1; g_world.nextItemId=1;
    populateDepth(1);
    {   const ClassDef* c=&CLASSES[cls];
        g_me.name=c->key;
        g_me.potions=1;
        g_me.facingX=0; g_me.facingY=1;
        applyClassStats(cls);
    }
    resetFog();
    placeAtSpawn();
    s_lastDepthChangeT=g_torchClock;
    onDepthArrival(1);
    musicSetDepth(1);
}

/* ------------------------------------------------------------------ */
/* FOV: raggi Bresenham (castRay / updateFOV)                          */
/* ------------------------------------------------------------------ */
static void castRay(int x0,int y0,int x1,int y1,const Layout* L){
    int dx=(x1>x0?x1-x0:x0-x1), dy=-(y1>y0?y1-y0:y0-y1);
    int sx=x0<x1?1:-1, sy=y0<y1?1:-1;
    int err=dx+dy, x=x0, y=y0;
    for (;;){
        if (x>=0 && y>=0 && x<L->w && y<L->h){
            g_world.visible[y*L->w+x]=1;
            g_world.visited[y*L->w+x]=1;
            if (L->grid[y*L->w+x]==T_WALL) break;
        } else break;
        if (x==x1 && y==y1) break;
        {   int e2=2*err;
            if (e2>=dy){ err+=dy; x+=sx; }
            if (e2<=dx){ err+=dx; y+=sy; }
        }
    }
}
static void updateFOV(void){
    const Layout* L=g_world.layout;
    int tx=(int)g_me.x, ty=(int)g_me.y;
    int R=(int)ceilf(FOV_RADIUS), yy, xx;
    if (!L) return;
    if (tx==g_world.lastTileX && ty==g_world.lastTileY) return;
    g_world.lastTileX=tx; g_world.lastTileY=ty;
    memset(g_world.visible,0,L->w*L->h);
    for (yy=ty-R; yy<=ty+R; ++yy)
        for (xx=tx-R; xx<=tx+R; ++xx){
            if (xx<0||yy<0||xx>=L->w||yy>=L->h) continue;
            if ((xx-tx)*(xx-tx)+(yy-ty)*(yy-ty) > FOV_RADIUS*FOV_RADIUS) continue;
            castRay(tx,ty,xx,yy,L);
        }
}

/* ------------------------------------------------------------------ */
/* Combattimento del giocatore                                         */
/* ------------------------------------------------------------------ */
static Monster* findNearestMonster(float maxR){
    Monster* best=0; float bd=1e30f; int i;
    for (i=0;i<MONSTER_CAP;++i){
        Monster* m=&g_world.monsters[i];
        float d;
        if (m->type<0) continue;
        d=dist2f(g_me.x,g_me.y,m->rx,m->ry);
        if (d<maxR*maxR && d<bd){ bd=d; best=m; }
    }
    return best;
}
static void rollDamage(const ClassDef* c,float* amount,int* crit){
    float dmg=(float)(c->dmgMin + frand()*(c->dmgMax-c->dmgMin));
    EquipBonus eq=computeEquipBonus();
    *crit = (c->crit>0.f && frand()<c->crit);
    if (*crit) dmg*=1.7f;
    dmg *= (1.f+eq.dmgPct/100.f);
    *amount=dmg;
}
static void hostApplyMonsterDamage(Monster* m,float amount);

void performAttack(void){
    const ClassDef* c=&CLASSES[g_me.cls];
    EquipBonus eq;
    float autoRange,dmgMult;
    Monster* nearest;
    if (g_me.dead||g_me.downed) return;
    if (g_me.atkTimer>0) return;
    if (c->maxMp>0 && c->manaCost && g_me.mp<c->manaCost){ showToast("Mana insufficiente"); return; }
    g_me.atkTimer=c->atkCooldown*(g_me.buffs[PW_FOCUS]>0?0.5f:1.f);
    g_me.anim=1; g_me.animTimer=c->atkDur;
    if (c->maxMp>0 && c->manaCost) g_me.mp-=(float)c->manaCost;

    /* auto-mira come l'originale */
    autoRange = c->ranged ? c->range : (c->range+0.7f>2.1f ? c->range+0.7f : 2.1f);
    nearest=findNearestMonster(autoRange);
    if (nearest){
        float ndx=nearest->rx-g_me.x, ndy=nearest->ry-g_me.y;
        float nd=sqrtf(ndx*ndx+ndy*ndy); if (nd<0.001f) nd=1;
        g_me.facingX=ndx/nd; g_me.facingY=ndy/nd;
    }

    dmgMult=g_me.buffs[PW_RAGE]>0?1.4f:1.f;
    if (!c->ranged){
        float fx=g_me.facingX, fy=g_me.facingY;
        int i;
        sfxPlay(SFX_SWING);
        pfxBurst(g_me.x+fx*0.4f,g_me.y+fy*0.4f,5,(const unsigned int[]){c->fxCol1,c->fxCol2},2,
                 0.8f,2.4f,2.f,4.f,0.12f,0.28f,0,0);
        for (i=0;i<MONSTER_CAP;++i){
            Monster* m=&g_world.monsters[i];
            float dx,dy,d,nx,ny,dot,amount; int crit;
            char buf[16];
            if (m->type<0) continue;
            dx=m->rx-g_me.x; dy=m->ry-g_me.y;
            d=sqrtf(dx*dx+dy*dy);
            if (d>c->range) continue;
            nx=dx/(d>0.001f?d:1); ny=dy/(d>0.001f?d:1);
            dot=nx*fx+ny*fy;
            if (dot<cosf(c->arc)) continue;
            rollDamage(c,&amount,&crit);
            amount=(float)(int)(amount*dmgMult+0.5f);
            hostApplyMonsterDamage(m,amount);
            hitBurst(m->rx,m->ry,crit?C_HEX(FF,E0,8A):0xFFFFFFFFu);
            pfxBurst(m->rx,m->ry,6,(const unsigned int[]){c->fxCol1,c->fxCol2},2,
                     1.f,3.2f,2.f,4.f,0.15f,0.35f,0,0);
            if (crit){ sfxPlay(SFX_CRIT); addShake(0.32f); addHitstop(0.07f); addCritFlash(); }
            else     { sfxPlay(SFX_HIT);  addShake(0.10f); addHitstop(0.025f); }
            snprintf(buf,sizeof buf,"%d%s",(int)amount,crit?"!":"");
            spawnFloatText(m->rx,m->ry-0.5f,buf,crit?C_HEX(FF,CF,5C):C_HEX(F2,F2,F2));
        }
    } else {
        Proj* p=0; int i;
        if (g_me.cls==CL_MAGO || g_me.cls==CL_NEGROMANTE) sfxPlay(SFX_SHOOT_MAGE);
        else if (g_me.cls==CL_PROF) sfxPlay(SFX_SHOOT_PLASMA);
        else sfxPlay(SFX_SHOOT_RANGER);
        pfxBurst(g_me.x+g_me.facingX*0.5f,g_me.y+g_me.facingY*0.5f,8,
                 (const unsigned int[]){c->fxCol1,c->fxCol2},2,1.5f,4.f,2.5f,5.f,0.12f,0.3f,0,0);
        spellFlash(g_me.x+g_me.facingX*0.5f,g_me.y+g_me.facingY*0.5f,c->projColor,2);
        for (i=0;i<PROJ_CAP;++i) if (!g_projs[i].used){ p=&g_projs[i]; break; }
        if (p){
            p->used=1; p->x=g_me.x; p->y=g_me.y;
            p->vx=g_me.facingX*c->projSpeed; p->vy=g_me.facingY*c->projSpeed;
            p->life=1.6f; p->color=c->projColor;
            p->foreign=0; p->boss=0; p->pierce=0;
            p->dmgMult=dmgMult; p->hitCount=0;
        }
    }
}

/* ---- Abilita' di classe (useClassAbility / ability*) ---- */
static void abilityCharge(void){
    const ClassDef* c=&CLASSES[CL_GUERRIERO];
    const float dist=4.5f; const int steps=18;
    float fx=g_me.facingX, fy=g_me.facingY;
    int hitIds[MONSTER_CAP]; int hitCount=0;
    int i,reached=0;
    unsigned int dust[1]={C_HEX(8A,7A,5C)};
    for (i=1;i<=steps;++i){
        float nx=g_me.x+fx*dist*(i/(float)steps);
        float ny=g_me.y+fy*dist*(i/(float)steps);
        int j;
        if (!canOccupy(g_world.layout,nx,ny,0.24f,NULL)) break;
        reached=i;
        if (i%4==0) pfxBurst(nx,ny+0.3f,2,dust,1,0.3f,0.8f,2.f,4.f,0.3f,0.6f,1,0);
        for (j=0;j<MONSTER_CAP;++j){
            Monster* m=&g_world.monsters[j];
            float amount; int crit,k,already=0;
            char buf[16];
            if (m->type<0) continue;
            for (k=0;k<hitCount;++k) if (hitIds[k]==m->id){ already=1; break; }
            if (already) continue;
            if (dist2f(nx,ny,m->rx,m->ry)<0.7f*0.7f){
                hitIds[hitCount++]=m->id;
                rollDamage(c,&amount,&crit);
                amount=(float)(int)(amount*1.3f+0.5f);
                hostApplyMonsterDamage(m,amount);
                hitBurst(m->rx,m->ry,C_HEX(DC,DC,E0));
                sfxPlay(SFX_HIT);
                snprintf(buf,sizeof buf,"%d",(int)amount);
                spawnFloatText(m->rx,m->ry-0.5f,buf,C_HEX(FF,CF,5C));
            }
        }
    }
    if (reached>0){
        g_me.x+=fx*dist*(reached/(float)steps);
        g_me.y+=fy*dist*(reached/(float)steps);
    }
    if (g_me.invulnTimer<0.35f) g_me.invulnTimer=0.35f;
    addShake(0.28f); addHitstop(0.05f);
    spellFlash(g_me.x,g_me.y,C_HEX(DC,DC,E0),2.5f);
    spawnFloatText(g_me.x,g_me.y-0.8f,"CARICA!",C_HEX(DC,DC,E0));
}
static void abilityShadowStep(void){
    Monster* target=0; float bd=1e30f;
    int i;
    float ox=g_me.x, oy=g_me.y;
    for (i=0;i<MONSTER_CAP;++i){
        Monster* m=&g_world.monsters[i];
        float d;
        if (m->type<0) continue;
        d=dist2f(g_me.x,g_me.y,m->rx,m->ry);
        if (d<36.f && d<bd){ bd=d; target=m; }
    }
    if (target){
        float dx=target->rx-g_me.x, dy=target->ry-g_me.y;
        float d=sqrtf(bd); if (d<0.001f) d=1;
        {
            float spotX=target->rx-(dx/d)*0.85f, spotY=target->ry-(dy/d)*0.85f;
            if (canOccupy(g_world.layout,spotX,spotY,0.24f,NULL)){ g_me.x=spotX; g_me.y=spotY; }
        }
        g_me.facingX=dx/d; g_me.facingY=dy/d;
        {
            const ClassDef* lad=&CLASSES[CL_LADRO];
            float amount=(float)((lad->dmgMin+lad->dmgMax)/2)*1.9f;
            char buf[16];
            amount=(float)(int)(amount+0.5f);
            hostApplyMonsterDamage(target,amount);
            hitBurst(target->rx,target->ry,C_HEX(FF,E0,8A));
            sfxPlay(SFX_CRIT);
            addCritFlash(); addShake(0.24f); addHitstop(0.06f);
            snprintf(buf,sizeof buf,"%d!",(int)amount);
            spawnFloatText(target->rx,target->ry-0.5f,buf,C_HEX(FF,CF,5C));
        }
    } else {
        float fx=g_me.facingX, fy=g_me.facingY;
        if (canOccupy(g_world.layout,g_me.x+fx*2,g_me.y+fy*2,0.24f,NULL)){
            g_me.x+=fx*2; g_me.y+=fy*2;
        }
    }
    if (g_me.invulnTimer<1.4f) g_me.invulnTimer=1.4f;
    smokeBurst(ox,oy,C_HEX(4A,3A,7A),14);
    smokeBurst(g_me.x,g_me.y,C_HEX(4A,3A,7A),14);
    spellFlash(ox,oy,C_HEX(8A,5C,FF),3);
    spellFlash(g_me.x,g_me.y,C_HEX(8A,5C,FF),3);
    spawnFloatText(g_me.x,g_me.y-0.8f,"PASSO FURTIVO",C_HEX(D8,E6,FF));
}
static void abilityShockwave(void){
    const ClassDef* c=&CLASSES[CL_MAGO];
    const float radius=2.3f;
    float dmgMult=g_me.buffs[PW_RAGE]>0?1.4f:1.f;
    int i,hit=0;
    char buf[64];
    for (i=0;i<MONSTER_CAP;++i){
        Monster* m=&g_world.monsters[i];
        float amount; int crit; char fb[16];
        if (m->type<0) continue;
        if (dist2f(g_me.x,g_me.y,m->rx,m->ry)<radius*radius){
            rollDamage(c,&amount,&crit);
            amount=(float)(int)(amount*1.2f*dmgMult+0.5f);
            hostApplyMonsterDamage(m,amount);
            hitBurst(m->rx,m->ry,C_HEX(8F,D1,FF));
            sfxPlay(SFX_HIT);
            snprintf(fb,sizeof fb,"%d",(int)amount);
            spawnFloatText(m->rx,m->ry-0.5f,fb,C_HEX(8F,D1,FF));
            ++hit;
        }
    }
    pushShockwave(g_me.x,g_me.y,0.45f,C_HEX(8F,D1,FF),0);
    sfxPlay(SFX_BOOM);
    addShake(0.55f); addHitstop(0.09f);
    spellFlash(g_me.x,g_me.y,C_HEX(8F,D1,FF),4.5f);
    pfxBurst(g_me.x,g_me.y,22,(const unsigned int[]){C_HEX(8F,D1,FF),C_HEX(CF,E9,FF)},2,
             2.f,7.f,2.f,4.5f,0.3f,0.7f,0,0);
    spawnFloatText(g_me.x,g_me.y-0.8f,"ONDA D'URTO",C_HEX(8F,D1,FF));
    snprintf(buf,sizeof buf,"Onda d'urto: colpiti %d nemici.",hit);
    logLine(buf,C_HEX(8F,D1,FF));
}
static void abilityVolley(void){
    const ClassDef* c=&CLASSES[CL_RANGER];
    float baseAngle=atan2f(g_me.facingY,g_me.facingX);
    static const float spread[5]={-0.42f,-0.21f,0.f,0.21f,0.42f};
    int k;
    sfxPlay(SFX_SHOOT_RANGER);
    spellFlash(g_me.x,g_me.y,C_HEX(D8,E6,B0),2.2f);
    for (k=0;k<5;++k){
        float ang=baseAngle+spread[k];
        Proj* p=0; int i;
        for (i=0;i<PROJ_CAP;++i) if (!g_projs[i].used){ p=&g_projs[i]; break; }
        if (!p) break;
        p->used=1; p->x=g_me.x; p->y=g_me.y;
        p->vx=cosf(ang)*c->projSpeed; p->vy=sinf(ang)*c->projSpeed;
        p->life=1.6f; p->color=c->projColor;
        p->foreign=0; p->boss=0; p->pierce=0; p->dmgMult=0.7f; p->hitCount=0;
    }
    spawnFloatText(g_me.x,g_me.y-0.8f,"RAFFICA!",C_HEX(D8,E6,B0));
}
static void fireChargedShot(void){
    const ClassDef* c=&CLASSES[CL_PROF];
    float dmgMult=(g_me.buffs[PW_RAGE]>0?1.4f:1.f)*2;
    float speed=c->projSpeed*1.35f;
    Proj* p=0; int i;
    for (i=0;i<PROJ_CAP;++i) if (!g_projs[i].used){ p=&g_projs[i]; break; }
    if (!p) return;
    p->used=1; p->x=g_me.x; p->y=g_me.y;
    p->vx=g_me.facingX*speed; p->vy=g_me.facingY*speed;
    p->life=2.4f; p->color=C_HEX(BD,F3,FF);
    p->foreign=0; p->boss=0; p->pierce=0; p->dmgMult=dmgMult; p->hitCount=0;
    sfxPlay(SFX_SHOOT_PLASMA);
    addShake(0.4f); addHitstop(0.07f);
    spellFlash(g_me.x,g_me.y,C_HEX(7D,F9,FF),3.2f);
    pfxBurst(g_me.x+g_me.facingX*0.6f,g_me.y+g_me.facingY*0.6f,14,
             (const unsigned int[]){C_HEX(7D,F9,FF),C_HEX(EA,FF,FF)},2,
             1.5f,4.f,2.f,4.f,0.2f,0.5f,0,0);
}
static void startChargedShot(float chargeTime){
    g_me.pendingCharge=chargeTime;
    sfxPlay(SFX_SHOOT_PLASMA);
    spawnFloatText(g_me.x,g_me.y-0.9f,"CARICA...",C_HEX(7D,F9,FF));
    pfxBurst(g_me.x,g_me.y-0.2f,6,(const unsigned int[]){C_HEX(7D,F9,FF),C_HEX(CF,EF,FF)},2,
             0.4f,1.2f,1.5f,3.f,0.25f,0.5f,0,0);
}
static void abilityHolyShield(void){
    g_me.buffs[PW_SHIELD]=5.f;
    spellFlash(g_me.x,g_me.y,C_HEX(FF,D9,8A),2.8f);
    spawnFloatText(g_me.x,g_me.y-0.8f,"MURO SACRO",C_HEX(FF,D9,8A));
    logLine("Muro Sacro: danno subito dimezzato per 5s.",C_HEX(FF,D9,8A));
}
static void abilitySoulDrain(void){
    const ClassDef* c=&CLASSES[CL_NEGROMANTE];
    const float radius=2.4f;
    float dmgMult=g_me.buffs[PW_RAGE]>0?1.4f:1.f;
    int i,hit=0; float drained=0.f;
    char buf[80],fb[16];
    for (i=0;i<MONSTER_CAP;++i){
        Monster* m=&g_world.monsters[i];
        float amount; int crit;
        if (m->type<0) continue;
        if (dist2f(g_me.x,g_me.y,m->rx,m->ry)<radius*radius){
            rollDamage(c,&amount,&crit);
            amount=(float)(int)(amount*1.2f*dmgMult+0.5f);
            hostApplyMonsterDamage(m,amount);
            hitBurst(m->rx,m->ry,C_HEX(8F,E0,7B));
            sfxPlay(SFX_HIT);
            snprintf(fb,sizeof fb,"%d",(int)amount);
            spawnFloatText(m->rx,m->ry-0.5f,fb,C_HEX(8F,E0,7B));
            ++hit; drained+=amount;
        }
    }
    if (drained>0.f){
        int heal=(int)(drained*0.5f+0.5f); if (heal<1) heal=1;
        g_me.hp=fminf(g_me.maxHp,g_me.hp+heal);
        healRise(g_me.x,g_me.y-0.3f,C_HEX(8F,E0,7B));
        snprintf(fb,sizeof fb,"+%d",heal);
        spawnFloatText(g_me.x,g_me.y-0.7f,fb,C_HEX(8F,E0,7B));
    }
    pushShockwave(g_me.x,g_me.y,0.45f,C_HEX(8F,E0,7B),0);
    sfxPlay(SFX_BOOM);
    addShake(0.5f); addHitstop(0.07f);
    spellFlash(g_me.x,g_me.y,C_HEX(8F,E0,7B),4);
    pfxBurst(g_me.x,g_me.y,18,(const unsigned int[]){C_HEX(8F,E0,7B),C_HEX(C9,FF,D6)},2,
             1.5f,6.f,2.f,4.5f,0.3f,0.7f,0,0);
    spawnFloatText(g_me.x,g_me.y-0.8f,"DRENAGGIO D'ANIMA",C_HEX(8F,E0,7B));
    snprintf(buf,sizeof buf,"Drenaggio d'Anima: %d colpiti, %d drenati.",hit,(int)drained);
    logLine(buf,C_HEX(8F,E0,7B));
}
static void abilityInspiringSong(void){
    g_me.buffs[PW_RAGE]=8.f;
    spellFlash(g_me.x,g_me.y,C_HEX(FF,CF,5C),2.8f);
    pfxBurst(g_me.x,g_me.y-0.3f,10,(const unsigned int[]){C_HEX(FF,CF,5C),C_HEX(FF,E9,B0)},2,
             0.5f,1.6f,1.5f,3.f,0.4f,0.8f,0,0);
    spawnFloatText(g_me.x,g_me.y-0.8f,"CANTO D'ISPIRAZIONE",C_HEX(FF,CF,5C));
    logLine("Canto d'Ispirazione: +40% danno per 8s.",C_HEX(FF,CF,5C));
}
static void abilityChiWave(void){
    const float speed=13.f;
    float dmgMult=(g_me.buffs[PW_RAGE]>0?1.4f:1.f)*1.5f;
    Proj* p=0; int i;
    for (i=0;i<PROJ_CAP;++i) if (!g_projs[i].used){ p=&g_projs[i]; break; }
    if (!p) return;
    p->used=1; p->x=g_me.x; p->y=g_me.y;
    p->vx=g_me.facingX*speed; p->vy=g_me.facingY*speed;
    p->life=0.9f; p->color=C_HEX(FF,CA,7A);
    p->foreign=0; p->boss=0; p->pierce=1; p->dmgMult=dmgMult; p->hitCount=0;
    sfxPlay(SFX_SHOOT_PLASMA);
    addShake(0.18f);
    spellFlash(g_me.x,g_me.y,C_HEX(FF,CA,7A),2);
    spawnFloatText(g_me.x,g_me.y-0.8f,"ONDA DI CHI",C_HEX(FF,CA,7A));
}
void useClassAbility(void){
    const ClassDef* c=&CLASSES[g_me.cls];
    const AbilityDef* ab=&c->ability;
    if (g_me.dead||g_me.downed) return;
    if (g_me.abilityTimer>0){
        char buf[48];
        snprintf(buf,sizeof buf,"Abilita' in recupero (%ds)",(int)ceilf(g_me.abilityTimer));
        showToast(buf);
        return;
    }
    if (ab->manaCost && g_me.mp<ab->manaCost){ showToast("Mana insufficiente"); return; }
    switch (g_me.cls){
        case CL_GUERRIERO:  abilityCharge(); break;
        case CL_LADRO:      abilityShadowStep(); break;
        case CL_MAGO:       abilityShockwave(); break;
        case CL_RANGER:     abilityVolley(); break;
        case CL_PROF:       startChargedShot(ab->chargeTime>0?ab->chargeTime:0.8f); break;
        case CL_PALADINO:   abilityHolyShield(); break;
        case CL_NEGROMANTE: abilitySoulDrain(); break;
        case CL_BARDO:      abilityInspiringSong(); break;
        case CL_MONACO:     abilityChiWave(); break;
        default: break;
    }
    sfxPlay(SFX_ABILITY);
    g_me.abilityTimer=ab->cooldown;
    if (ab->manaCost) g_me.mp-=(float)ab->manaCost;
}

void drinkPotion(void){
    int heal;
    char buf[24];
    if (g_me.dead||g_me.downed) return;
    if (g_me.potions<=0) return;
    if (g_me.hp>=g_me.maxHp){ showToast("Sei gia' in piena salute"); return; }
    g_me.potions--;
    heal=(int)(g_me.maxHp*0.55f)+2;
    g_me.hp=fminf(g_me.maxHp,g_me.hp+heal);
    sfxPlay(SFX_POTION);
    healRise(g_me.x,g_me.y-0.3f,C_HEX(7F,AE,63));
    snprintf(buf,sizeof buf,"+%d",heal);
    spawnFloatText(g_me.x,g_me.y-0.7f,buf,C_HEX(7F,AE,63));
}
void drinkManaPotion(void){
    int restore;
    char buf[24];
    if (g_me.dead||g_me.downed) return;
    if (CLASSES[g_me.cls].maxMp<=0){ showToast("La tua classe non usa mana"); return; }
    if (g_me.manaPotions<=0) return;
    if (g_me.mp>=g_me.maxMp){ showToast("Mana gia' pieno"); return; }
    g_me.manaPotions--;
    restore=(int)(g_me.maxMp*0.6f)+1;
    g_me.mp=fminf(g_me.maxMp,g_me.mp+restore);
    sfxPlay(SFX_MANA);
    healRise(g_me.x,g_me.y-0.3f,C_HEX(5F,A0,C9));
    snprintf(buf,sizeof buf,"+%d",restore);
    spawnFloatText(g_me.x,g_me.y-0.7f,buf,C_HEX(5F,A0,C9));
}

/* ------------------------------------------------------------------ */
/* Danni al giocatore (applyMonsterHitToMe) e morte                    */
/* ------------------------------------------------------------------ */
static void handleDowned(void);
static void handleLocalDeath(void);
static void hostAdvanceDepth(void);
static void applyMonsterHitToMe(float amount,int poison);

/* Danno ricevuto da un mostro via rete (MHIT dall'host). */
void netHitFromRemote(float amount,int poison){
    applyMonsterHitToMe(amount,poison);
}

static void applyMonsterHitToMe(float amount,int poison){
    float finalAmount;
    if (g_me.dead || g_me.invulnTimer>0) return;
    g_me.invulnTimer=0.45f;
    {   /* direzione del colpo: verso il mostro piu' vicino */
        Monster* best=0; float bd=1e30f; int i;
        for (i=0;i<MONSTER_CAP;++i){
            Monster* m=&g_world.monsters[i];
            float d;
            if (m->type<0) continue;
            d=dist2f(g_me.x,g_me.y,m->rx,m->ry);
            if (d<bd){ bd=d; best=m; }
        }
        if (best){
            float ddx=g_me.x-best->rx, ddy=g_me.y-best->ry;
            float dl=sqrtf(ddx*ddx+ddy*ddy); if (dl<0.001f) dl=1;
            g_me.lastHitX=ddx/dl; g_me.lastHitY=ddy/dl;
        }
    }
    if (g_me.downed){ flashDamage(); handleLocalDeath(); return; }
    finalAmount = g_me.buffs[PW_SHIELD]>0 ? amount*0.5f : amount;
    {
        EquipBonus eq=computeEquipBonus();
        finalAmount *= fmaxf(0.2f, 1.f-eq.armorPct/100.f);
    }
    finalAmount=(float)(int)(finalAmount+0.5f);
    if (finalAmount<1) finalAmount=1;
    g_me.hp=fmaxf(0.f,g_me.hp-finalAmount);
    flashDamage();
    sfxPlay(SFX_HURT);
    addShake(0.22f);
    if (poison) g_me.poisonTimer=3.f;
    if (g_me.hp<=0) handleDowned();
}
static void handleDowned(void){
    g_me.downed=1;
    g_me.downedTimer=DOWNED_BLEEDOUT;
    sfxPlay(SFX_DEATH);
    addShake(0.3f);
    showToast("Sei a terra! Nessuno puo' aiutarti...");
    logLine("Sei a terra e hai bisogno di soccorso!",C_HEX(C1,44,3A));
}
static void handleLocalDeath(void){
    g_me.dead=1; g_me.downed=0;
    g_me.respawnTimer=3.2f;
    updateRecordIfBetter();
    sfxPlay(SFX_DEATH);
    addShake(0.45f);
    logLine("Sei caduto nell'oscurita'... hai perso tutto.",C_HEX(C1,44,3A));
}
static void handleRespawn(void){
    int s;
    g_me.dead=0; g_me.downed=0; g_me.downedTimer=0;
    g_me.gold=0; g_me.potions=0; g_me.manaPotions=0;
    for (s=0;s<EQ_SLOT_COUNT;++s){ g_me.equip[s].statCount=0; }
    {   int k; for (k=0;k<PW_COUNT;++k) g_me.buffs[k]=0; }
    applyClassStats(g_me.cls);
    placeAtSpawn();
    if (g_world.layout->hasBossRoom && g_world.bossActive && !g_world.bossDead){
        const Room* br=&g_world.layout->bossRoom;
        g_me.x=(float)br->x+1.5f; g_me.y=(float)br->y+br->h-1.5f;
    }
}

/* ------------------------------------------------------------------ */
/* Danni ai mostri / uccisioni (hostApplyMonsterDamage/hostKillMonster)*/
/* ------------------------------------------------------------------ */
static void hostKillMonster(Monster* m){
    const MonsterType* t=monType(m->type);
    Layout* L=g_world.layout;
    if (t->boss){
        g_world.bossDead=1; g_world.bossActive=0;
        applyBossGates(0);
        { char buf[96];
          snprintf(buf,sizeof buf,"%s e' stato sconfitto!",t->name);
          showToast(buf);
          logLine("La tana del boss si e' aperta: il forziere leggendario e' tuo.",
                  C_HEX(FF,D2,7A)); }
    }
    if (dist2f(m->x,m->y,g_me.x,g_me.y)<121.f){
        if (t->boss){
            int k;
            for (k=0;k<3;++k)
                killBurst(m->x+(frand()-0.5f)*0.8f,m->y+(frand()-0.5f)*0.8f,t->color);
            for (k=0;k<20;++k){
                Part* p=allocPart();
                if (!p) break;
                {   float ang=frand()*2.f*M_PI, d=frand()*2.f;
                    p->used=1; p->x=m->x+cosf(ang)*d; p->y=m->y+sinf(ang)*d;
                    p->vx=cosf(ang)*(2+frand()*5); p->vy=sinf(ang)*(2+frand()*5);
                    p->life=p->maxLife=0.5f+frand()*0.7f; p->size=1.5f+frand()*2.f;
                    p->color=C_HEX(FF,D7,00); p->grav=2; p->drag=0.99f; p->type=0;
                }
            }
            addShake(1.2f); addHitstop(0.22f);
            sfxPlay(SFX_BOSS_KILL);
            g_bossDeathFlashTimer=0.35f;
        } else {
            killBurst(m->x,m->y,t->color);
            addShake(0.4f); addHitstop(0.045f);
            sfxPlay(SFX_KILL);
        }
    }
    /* affisso Esplosivo */
    if (m->affix==AFFIX_ESPLOSIVO && dist2f(m->x,m->y,g_me.x,g_me.y)<121.f){
        float r=AFFIXES[AFFIX_ESPLOSIVO].explodeRadius;
        unsigned int cs[1]={C_HEX(FF,6B,4A)};
        pfxBurst(m->x,m->y,18,cs,1,(float)(0),r*1.6f,2.f,4.f,0.4f,0.8f,0,0.3f);
        if (dist2f(m->x,m->y,g_me.x,g_me.y)<(r+0.3f)*(r+0.3f)){
            float dmg=3+(float)(int)(L?g_world.depth*0.8f:0);
            g_me.hp=fmaxf(0.f,g_me.hp-dmg);
            flashDamage(); sfxPlay(SFX_HURT);
            if (g_me.hp<=0) handleDowned();
        }
    }
    /* drop */
    {
        Item* it=0; int i;
        int gold=t->goldMin + (int)(frand()*(t->goldMax-t->goldMin+1));
        #define ADD_ITEM(kindVar) \
            for (i=0;i<ITEM_CAP;++i) if (!g_world.items[i].used){ it=&g_world.items[i]; break; } \
            if (it){ memset(it,0,sizeof *it); it->used=1; it->kindVar; \
                     it->x=(float)(int)m->x+0.5f; it->y=(float)(int)m->y+0.5f; }
        ADD_ITEM(kind=IK_GOLD); if(it) it->amount=gold;
        it=0;
        if (frand()<0.16f){ ADD_ITEM(kind=IK_POTION); it->amount=1; }
        it=0;
        if (frand()<0.11f){ ADD_ITEM(kind=IK_MANAPOT); it->amount=1; }
        it=0;
        if (frand() < (t->boss?0.95f:0.07f)){
            Rng rr; rng_seed(&rr,(unsigned int)(frand()*4294967296.f));
            ADD_ITEM(kind=IK_EQUIP);
            it->eq = t->boss ? makePremiumEquip(g_world.depth,&rr) : makeEquipItem(g_world.depth,&rr);
        }
        #undef ADD_ITEM
    }
    /* scissione della gelatina */
    if (monType(m->type)->split && m->splitLeft>0){
        int k;
        for (k=0;k<2;++k){
            Monster* nm=makeMonster(MT_MELMA,(float)(int)m->x,(float)(int)m->y,g_world.depth,AFFIX_NONE);
            if (!nm) break;
            nm->x+=(k?0.3f:-0.3f); nm->rx=nm->x;
            nm->hp=(float)(int)(nm->hp*0.55f); nm->maxHp=nm->hp; nm->splitLeft=0;
        }
    }
    m->type=-1; /* rimozione */
}
static void hostApplyMonsterDamage(Monster* m,float amount){
    if (m->type<0 || m->hp<=0) return;
    if (monType(m->type)->boss && g_world.layout && g_world.layout->hasBossRoom &&
        !g_world.bossActive) return;  /* boss dormiente invulnerabile */
    m->hitFlashAt=g_torchClock;
    m->hp-=amount;
    if (m->hp<=0) hostKillMonster(m);
}

/* ------------------------------------------------------------------ */
/* Forzieri, mercante, raccolte                                        */
/* ------------------------------------------------------------------ */
static void resolveChestOpen(int chestIdx){
    Layout* L=g_world.layout;
    ChestSpot* c=&L->chests[chestIdx];
    int isBossChest=c->hasBossChest;
    int gold,potion,manaPotion;
    EquipItem eq; int hasEq=0;
    char buf[80], fb[32];
    Rng rr; rng_seed(&rr,(unsigned int)(frand()*4294967296.f));
    if (isBossChest && !g_world.bossDead){
        showToast("Il forziere del drago e' sigillato: solo la sua fine lo aprira'.");
        return;
    }
    if (L->opened[chestIdx]) return;
    L->opened[chestIdx]=1;
    if (isBossChest){
        gold=60+(int)(frand()*25*g_world.depth);
        eq=makePremiumEquip(g_world.depth,&rr); hasEq=1;
        potion=1; manaPotion=1;
    } else {
        gold=6+(int)(frand()*9*g_world.depth);
        potion=frand()<0.32f; manaPotion=frand()<0.24f;
        hasEq=frand()<0.20f;
        if (hasEq) eq=makeEquipItem(g_world.depth,&rr);
    }
    g_me.gold+=gold;
    if (potion) g_me.potions++;
    if (manaPotion) g_me.manaPotions++;
    sfxPlay(SFX_CHEST);
    snprintf(fb,sizeof fb,"+%d oro",gold);
    spawnFloatText(g_me.x,g_me.y-0.7f,fb,C_HEX(D4,AF,37));
    snprintf(buf,sizeof buf,"Hai aperto un forziere: +%d oro.",gold);
    logLine(buf,C_HEX(E8,A1,3D));
    if (hasEq) tryEquipOrSalvage(&eq);
}
static void applyPickup(Item* it){
    char buf[40];
    switch (it->kind){
        case IK_GOLD:
            g_me.gold+=it->amount; sfxPlay(SFX_PICKUP);
            snprintf(buf,sizeof buf,"+%d oro",it->amount);
            spawnFloatText(g_me.x,g_me.y-0.7f,buf,C_HEX(D4,AF,37));
            break;
        case IK_GEM:
            g_me.gold+=it->amount; sfxPlay(SFX_GEM);
            snprintf(buf,sizeof buf,"+%d oro",it->amount);
            spawnFloatText(g_me.x,g_me.y-0.7f,buf,C_HEX(8F,D1,FF));
            logLine("Hai trovato una gemma preziosa!",C_HEX(8F,D1,FF));
            break;
        case IK_POWER:
            g_me.buffs[it->buff]=POWERS[it->buff].duration;
            sfxPlay(SFX_POWER);
            healRise(g_me.x,g_me.y-0.3f,POWERS[it->buff].color);
            spawnFloatText(g_me.x,g_me.y-0.7f,POWERS[it->buff].name,POWERS[it->buff].color);
            logLine(POWERS[it->buff].desc,POWERS[it->buff].color);
            break;
        case IK_EQUIP:
            sfxPlay(SFX_GEM);
            tryEquipOrSalvage(&it->eq);
            break;
        case IK_MANAPOT:
            g_me.manaPotions++; sfxPlay(SFX_POTION);
            spawnFloatText(g_me.x,g_me.y-0.7f,"+pozione di mana",C_HEX(5F,A0,C9));
            break;
        default:
            g_me.potions++; sfxPlay(SFX_POTION);
            spawnFloatText(g_me.x,g_me.y-0.7f,"+pozione",C_HEX(C1,44,3A));
            break;
    }
    it->used=0;
}
static void checkItemPickup(void){
    int i;
    for (i=0;i<ITEM_CAP;++i){
        Item* it=&g_world.items[i];
        if (!it->used) continue;
        if (dist2f(g_me.x,g_me.y,it->x,it->y)<0.62f*0.62f){ applyPickup(it); }
    }
}
void buyFromMerchant(int kind){
    /* merchantPrices: potion/manapotion 12+d*2, equip 45+d*8, power 20+d*4 */
    int d=g_world.depth, cost;
    char buf[64];
    switch (kind){
        case IK_POTION: cost=12+d*2; break;
        case IK_MANAPOT: cost=12+d*2; break;
        case IK_EQUIP: cost=45+d*8; break;
        case IK_POWER: cost=20+d*4; break;
        default: return;
    }
    if (g_me.gold<cost){ showToast("Non hai abbastanza oro"); return; }
    g_me.gold-=cost;
    if (kind==IK_POTION){
        g_me.potions++;
        logLine("Comprata una pozione di salute dal mercante.",C_HEX(7F,AE,63));
    } else if (kind==IK_MANAPOT){
        g_me.manaPotions++;
        logLine("Comprata una pozione di mana dal mercante.",C_HEX(7F,AE,63));
    } else if (kind==IK_POWER){
        int buff=(int)(frand()*PW_COUNT)%PW_COUNT;
        g_me.buffs[buff]=POWERS[buff].duration;
        spawnFloatText(g_me.x,g_me.y-0.7f,POWERS[buff].name,POWERS[buff].color);
        logLine(POWERS[buff].desc,C_HEX(7F,AE,63));
    } else {
        Rng rr; rng_seed(&rr,(unsigned int)(frand()*4294967296.f));
        { EquipItem eq=makeEquipItem(g_world.depth,&rr);
          tryEquipOrSalvage(&eq); }
    }
    (void)buf;
}

void tryInteract(void){
    Layout* L=g_world.layout;
    int tx,ty,i;
    float mdx,mdy;
    if (g_me.dead||g_me.downed) return;
    tx=(int)g_me.x; ty=(int)g_me.y;
    if (L->grid[ty*L->w+tx]==T_STAIRS){
        /* guardia 4000ms come hostAdvanceDepth */
        if (g_torchClock-s_lastDepthChangeT>=4.f) hostAdvanceDepth();
        else showToast("Le scale ti risucchiano piu' a fondo...");
        return;
    }
    mdx=g_me.x-(L->merchantX+0.5f); mdy=g_me.y-(L->merchantY+0.5f);
    if (mdx*mdx+mdy*mdy < 2.1f*2.1f){ uiOpenMerchant(); return; }
    for (i=0;i<L->chestCount;++i){
        float cx=g_me.x-(L->chests[i].x+0.5f), cy=g_me.y-(L->chests[i].y+0.5f);
        if (!L->opened[i] && cx*cx+cy*cy<2.1f*2.1f){ resolveChestOpen(i); return; }
    }
}

/* ------------------------------------------------------------------ */
/* hostAdvanceDepth: discesa al piano successivo                       */
/* ------------------------------------------------------------------ */
static void hostAdvanceDepth(void){
    int newDepth=g_world.depth+1;
    s_lastDepthChangeT=g_torchClock;
    g_world.depth=newDepth;
    layoutFree();
    g_world.layout=generateDepth(newDepth);
    g_world.bossActive=0; g_world.bossDead=0;
    populateDepth(newDepth);
    sfxPlay(SFX_STAIR);
    musicSetDepth(newDepth);
    onDepthArrival(newDepth);
}
void gameDescend(void){ hostAdvanceDepth(); }
void toggleMinimap(void){ g_minimapVisible=!g_minimapVisible; }

/* ------------------------------------------------------------------ */
/* AI dei mostri (updateMonsterAI)                                     */
/* ------------------------------------------------------------------ */
static void updateBossAI(Monster* m,float dt);
typedef struct { float x,y; } PPos;
static int playerAlive(void){ return !g_me.dead && !g_me.downed; }

/* ------------------------------------------------------------------ */
/* Multiplayer: bersaglio dei mostri tra tutti i giocatori (come       */
/* l'originale host-autorevole, dove i mostri inseguono `players[]`).  */
/* ------------------------------------------------------------------ */
static float mpTargetX, mpTargetY;      /* posizione del bersaglio corrente */
static int   mpTargetRemote;            /* 1 se il bersaglio e' un peer remoto */
static int   mpTargetPeerId;            /* peerId del bersaglio remoto */

/* Seleziona il giocatore (locale o remoto) piu' vicino al mostro. */
static int pickPlayerTarget(float mx,float my){
    float bestD2; int have=0;
    mpTargetRemote=0;
    if (playerAlive()){
        bestD2=dist2f(mx,my,g_me.x,g_me.y);
        mpTargetX=g_me.x; mpTargetY=g_me.y;
        have=1;
    } else bestD2=0.f;
    {
        int i;
        for (i=0;i<netPlayerCount();++i){
            NetPlayer* p=netPlayerAt(i);
            if (!p || !p->used || p->isLocal) continue;
            if (p->flags & (NF_DEAD|NF_DOWNED)) continue;
            if (p->depth != g_world.depth) continue;   /* piani diversi */
            {
                float d2=dist2f(mx,my,p->x,p->y);
                if (!have || d2<bestD2){
                    bestD2=d2;
                    mpTargetX=p->x; mpTargetY=p->y;
                    mpTargetRemote=1; mpTargetPeerId=p->peerId;
                    have=1;
                }
            }
        }
    }
    return have;
}

/* Danno al bersaglio: locale -> applyMonsterHitToMe; remoto -> rete (MHIT). */
static void dealTargetHit(float amount,int poison){
    if (mpTargetRemote) netDealMonsterHit(mpTargetPeerId,amount,poison);
    else                applyMonsterHitToMe(amount,poison);
}

static void monBoltVisual(Monster* m,float sp,float life,unsigned int color){
    Proj* p=0; int i;
    float dx=g_me.x-m->x, dy=g_me.y-m->y, l=sqrtf(dx*dx+dy*dy);
    if (l<0.001f) l=1;
    for (i=0;i<PROJ_CAP;++i) if (!g_projs[i].used){ p=&g_projs[i]; break; }
    if (!p) return;
    p->used=1; p->x=m->x; p->y=m->y;
    p->vx=dx/l*sp; p->vy=dy/l*sp;
    p->life=life; p->color=color;
    p->foreign=1; p->boss=0; p->pierce=0; p->dmgMult=0; p->hitCount=0;
}
static void updateMonsterAI(Monster* m,float dt){
    const MonsterType* t=monType(m->type);
    Layout* L=g_world.layout;
    float excl[4];
    float d2p=0.f, dp=0.f, dxn=0.f, dyn=0.f;
    excl[0]=L->safeX; excl[1]=L->safeY; excl[2]=L->safeW; excl[3]=L->safeH;

    /* il boss dormiente resta in tana */
    if (t->boss && L->hasBossRoom && !(g_world.bossActive && !g_world.bossDead)){
        const Room* br=&L->bossRoom;
        m->x=clampf(m->x,(float)br->x+0.45f,(float)br->x+br->w-0.45f);
        m->y=clampf(m->y,(float)br->y+0.45f,(float)br->y+br->h-0.45f);
        m->rx=m->x; m->ry=m->y;
        m->fx=0; m->fy=1;
        return;
    }
    if (t->boss){ updateBossAI(m,dt); return; }

    m->atkCd=fmaxf(0.f,m->atkCd-dt);
    if (m->affix==AFFIX_RIGENERANTE)
        m->hp=fminf(m->maxHp,m->hp+AFFIXES[AFFIX_RIGENERANTE].regenPerSec*dt);

    if (!playerAlive() && !pickPlayerTarget(m->x,m->y)){ m->state=2; }
    else {
        pickPlayerTarget(m->x,m->y);
        d2p=dist2f(m->x,m->y,mpTargetX,mpTargetY);
    }
    if (playerAlive() && d2p < m->aggro*m->aggro){
        dp=sqrtf(d2p); if (dp<0.001f) dp=0.001f;
        dxn=(mpTargetX-m->x)/dp; dyn=(mpTargetY-m->y)/dp;
        m->fx=dxn; m->fy=dyn;
        m->state=1;
        /* ---- attacchi dedicati in corso ---- */
        if (m->dashKind==WF_DASH || m->dashKind==WF_SWOOP){
            float vx=m->dTx-m->x, vy=m->dTy-m->y, l=sqrtf(vx*vx+vy*vy);
            if (l<0.001f) l=0.001f;
            tryMoveEntity(L,&m->x,&m->y,m->speed,vx/l,vy/l,dt,0.27f,excl);
            m->dT+=dt;
            if (!m->dHit && l<((m->dashKind==WF_DASH)?0.55f:0.62f)){
                m->dHit=1;
                applyMonsterHitToMe((float)m->dmg,t->poison);
            }
            if (m->dT>=m->dDur){ m->dashKind=0; m->atkT=0.25f; }
            m->rx=m->x; m->ry=m->y;
            return;
        }
        if (m->boltActive){
            m->bLife-=dt;
            m->x+=m->bVx*dt; m->y+=m->bVy*dt;
            if (m->bLife<=0){
                unsigned int cs[1]={t->color};
                pfxBurst(m->x,m->y,8,cs,1,1.f,3.5f,2.f,5.f,0.2f,0.5f,0,0);
                m->boltActive=0;
                m->rx=m->x; m->ry=m->y;
                return;
            }
            if (mpTargetRemote || (playerAlive() && dist2f(mpTargetX,mpTargetY,m->x,m->y)<=0.45f*0.45f)){
                dealTargetHit((float)m->dmg,t->poison);
                { unsigned int cs[1]={t->color};
                  pfxBurst(mpTargetX,mpTargetY,12,cs,1,1.f,3.5f,2.f,5.f,0.2f,0.5f,0,0); }
                m->boltActive=0;
            }
            m->rx=m->x; m->ry=m->y;
            return;
        }
        if (m->dentT>0){
            m->dentT-=dt;
            if (m->dentT<=0 &&
                dist2f(m->x,m->y,mpTargetX,mpTargetY)<=1.5f*1.5f){
                dealTargetHit((float)m->dmg,0);
                m->atkT=0.25f;
            }
        }
        if (m->winding){
            m->windT-=dt;
            if (m->windT<=0){
                m->winding=0; m->atkT=0.3f;
                switch (m->windFx){
                    case WF_DASH:
                        m->dashKind=WF_DASH;
                        m->dTx=mpTargetX; m->dTy=mpTargetY;
                        m->dT=0; m->dDur=0.24f; m->dSpeed=7.2f; m->dHit=0;
                        break;
                    case WF_SWOOP:
                        m->dashKind=WF_SWOOP;
                        m->dTx=mpTargetX; m->dTy=mpTargetY;
                        m->dT=0; m->dDur=0.30f; m->dSpeed=6.4f; m->dHit=0;
                        break;
                    case WF_STOMP: {
                        const float r=1.8f;
                        pushShockwave(m->x,m->y,0.5f,t->color,0);
                        pfxBurst(m->x,m->y,16,(const unsigned int[]){t->color,C_HEX(E8,E4,DC)},2,
                                 2.f,6.f,2.5f,5.f,0.3f,0.6f,0,0);
                        spellFlash(m->x,m->y,t->color,3);
                        if (dist2f(m->x,m->y,mpTargetX,mpTargetY)<=r*r)
                            dealTargetHit((float)m->dmg,0);
                        break; }
                    case WF_CONE:
                        {
                            float vx=mpTargetX-m->x, vy=mpTargetY-m->y;
                            float dd=sqrtf(vx*vx+vy*vy); if (dd<0.001f) dd=0.001f;
                            if (dd<=1.6f &&
                                (vx/dd)*m->fx+(vy/dd)*m->fy>=0.55f)
                                dealTargetHit((float)(m->dmg+(int)(frand()*2)),0);
                        }
                        {   /* fendente a ventaglio */
                            float a0=atan2f(m->fy,m->fx)-0.55f, a1=atan2f(m->fy,m->fx)+0.55f;
                            int k;
                            for (k=0;k<12;++k){
                                float aa=a0+(a1-a0)*(k/11.f);
                                unsigned int cs[2]={t->color,C_HEX(E8,E8,E8)};
                                pfxBurst(m->x+cosf(aa)*0.9f,m->y+sinf(aa)*0.9f,2,cs,2,
                                         0.6f,1.8f,2.f,3.5f,0.15f,0.3f,0,0);
                            }
                        }
                        break;
                    default: break;
                }
                m->windFx=WF_NONE;
            }
            m->rx=m->x; m->ry=m->y;
            return;
        }
        /* inseguimento oppure attacco */
        {
            float atkDist=t->range>0?t->range:0.85f;
            if (dp>atkDist){
                float vx=dxn, vy=dyn;
                if (t->erratic && frand()<0.03f){
                    vx+=(frand()-0.5f)*1.4f; vy+=(frand()-0.5f)*1.4f;
                }
                tryMoveEntity(L,&m->x,&m->y,m->speed,vx,vy,dt,0.27f,excl);
            } else if (m->atkCd<=0){
                if (t->dash){
                    m->winding=1; m->windT=0.35f; m->windFx=WF_DASH;
                    m->atkCd=2.f+frand()*0.8f;
                    { unsigned int cs[1]={C_HEX(7F,AE,63)};
                      pfxBurst(m->x,m->y,6,cs,1,1.f,3.f,2.f,4.f,0.2f,0.4f,0,0); }
                } else if (t->swoop){
                    m->winding=1; m->windT=0.35f; m->windFx=WF_SWOOP;
                    m->atkCd=2.2f+frand()*0.8f;
                } else if (t->ranged){
                    float sp=t->beam?3.8f:3.2f;
                    float l=sqrtf(dxn*dxn+dyn*dyn); if (l<0.001f) l=1;
                    m->boltActive=1;
                    m->bVx=dxn*sp; m->bVy=dyn*sp;
                    m->bLife=fminf(1.9f,l/sp+0.35f);
                    m->atkCd=2.1f+frand()*0.6f;
                    monBoltVisual(m,sp,m->bLife,t->beam?C_HEX(7F,AE,63):C_HEX(B0,7F,D1));
                } else if (t->doubleAtk){
                    m->atkCd=0.8f+frand()*0.2f;
                    m->dentT=0.22f;
                    m->atkT=0.28f;
                    dealTargetHit((float)m->dmg,0);
                } else if (t->stomp){
                    m->winding=1; m->windT=0.55f; m->windFx=WF_STOMP;
                    m->atkCd=1.3f+frand()*0.4f;
                } else if (t->cone){
                    m->winding=1; m->windT=0.45f; m->windFx=WF_CONE;
                    m->atkCd=1.15f+frand()*0.3f;
                } else {
                    m->atkCd=1.05f+frand()*0.3f;
                    m->atkT=0.28f;
                    dealTargetHit((float)(m->dmg+(int)(frand()*2)),t->poison);
                    if (t->lifesteal) m->hp=fminf(m->maxHp,m->hp+2);
                }
            }
        }
    } else {
        m->state=2;
        m->wanderT-=dt;
        if (m->wanderT<=0){
            float ang=frand()*2.f*M_PI;
            m->wanderT=2+frand()*3;
            m->wtgtX=m->x+cosf(ang)*3;
            m->wtgtY=m->y+sinf(ang)*3;
        }
        {
            float wdx=m->wtgtX-m->x, wdy=m->wtgtY-m->y;
            float wd=sqrtf(wdx*wdx+wdy*wdy);
            if (wd>0.35f){
                tryMoveEntity(L,&m->x,&m->y,m->speed,wdx/wd,wdy/wd,dt*0.55f,0.27f,excl);
                m->fx=wdx/wd; m->fy=wdy/wd;
            }
        }
    }
    m->rx=m->x; m->ry=m->y;
}


/* ------------------------------------------------------------------ */
/* Boss AI (updateBossAI + helper)                                     */
/* ------------------------------------------------------------------ */
static void bossConeDamage(Monster* m,const BossVariant* C){
    float vx,vy,dd;
    if (!playerAlive() && !mpTargetRemote) return;
    vx=mpTargetX-m->x; vy=mpTargetY-m->y;
    dd=sqrtf(vx*vx+vy*vy); if (dd<0.001f) dd=0.001f;
    if (dd>C->breathDist) return;
    if ((vx/dd)*m->fx+(vy/dd)*m->fy < BOSS.breathArcCos) return;
    dealTargetHit((float)((int)BOSS.breathDmg),C->poisonHit);
}
static void bossFireballExplode(Monster* m,const BossVariant* C){
    int hitAny=0;
    if (dist2f(mpTargetX,mpTargetY,m->fbx,m->fby)<=C->fbR*C->fbR){
        dealTargetHit((float)C->fbDmg,C->poisonHit);
        hitAny=1;
    }
    pushShockwave(m->fbx,m->fby,0.45f,C->fbColor,0);
    pfxBurst(m->fbx,m->fby,20,(const unsigned int[]){C->fbColor,C_HEX(FF,D2,3D),0xFFFFFFFFu},3,
             2.f,7.f,2.5f,5.5f,0.25f,0.6f,0,0);
    spellFlash(m->fbx,m->fby,C->fbColor,3);
    if (hitAny){ addShake(0.5f); sfxPlay(SFX_BOOM); addHitstop(0.045f); }
    else sfxPlay(SFX_BOOM);
}
static void pickBossFlyWaypoint(Monster* m,float* wx,float* wy){
    Layout* L=g_world.layout;
    const Room* br=&L->bossRoom;
    int i;
    if (L->hasBossRoom){
        for (i=0;i<40;++i){
            int x=br->x+1+(int)(frand()*(br->w-2>1?br->w-2:1));
            int y=br->y+1+(int)(frand()*(br->h-2>1?br->h-2:1));
            if (L->grid[y*L->w+x]==T_FLOOR &&
                dist2f(x+0.5f,y+0.5f,m->x,m->y)>=2.5f){
                *wx=x+0.5f; *wy=y+0.5f; return;
            }
        }
    }
    for (i=0;i<40;++i){
        int x=(int)m->x+((int)(frand()*7)-3);
        int y=(int)m->y+((int)(frand()*7)-3);
        if (x>=0 && y>=0 && x<L->w && y<L->h && L->grid[y*L->w+x]==T_FLOOR){
            *wx=x+0.5f; *wy=y+0.5f; return;
        }
    }
    *wx=m->x; *wy=m->y;
}
enum { BM_CLAW=0, BM_BREATH, BM_FIREBALL, BM_FLY, BM_STOMP, BM_SUMMON, BM_CHARGE };

static int pickBossMove(Monster* m,float d,const BossVariant* C){
    if (C->stomp && d>=1.5f && d<=3.6f && m->cdStomp<=0 && frand()<0.4f) return BM_STOMP;
    if (C->charge && d>=2.8f && d<=9.f && m->cdCharge<=0 && frand()<0.45f) return BM_CHARGE;
    if (C->summon && m->cdSummon<=0 && frand()<0.3f) return BM_SUMMON;
    if (C->breathCd!=0.f && d>=1.05f && d<=4.2f && m->cdB<=0 && frand()<0.4f) return BM_BREATH;
    if (C->fbDmg>0 && d>=2.2f && d<=10.f && m->cdFb<=0 && frand()<0.42f) return BM_FIREBALL;
    if (!C->noFly && d>=3.2f && m->cdFly<=0 && frand()<0.5f) return BM_FLY;
    return BM_CLAW;
}

static void updateBossAI(Monster* m,float dt){
    Layout* L=g_world.layout;
    const MonsterType* t=monType(m->type);
    BossVariant C;
    float excl[4];
    float d, dxn, dyn;
    int move;

    bossVariantFor(m->type,&C);
    m->cdB=fmaxf(0,m->cdB-dt);
    m->cdFb=fmaxf(0,m->cdFb-dt);
    m->cdFly=fmaxf(0,m->cdFly-dt);
    m->cdSummon=fmaxf(0,m->cdSummon-dt);
    m->cdStomp=fmaxf(0,m->cdStomp-dt);
    m->cdCharge=fmaxf(0,m->cdCharge-dt);

    excl[0]=L->safeX; excl[1]=L->safeY; excl[2]=L->safeW; excl[3]=L->safeH;

    /* bossTarget: il giocatore (locale o remoto piu' vicino) */
    if (!playerAlive() && !pickPlayerTarget(m->x,m->y)){
        m->state=0;
        return;
    }
    pickPlayerTarget(m->x,m->y);
    dxn=mpTargetX-m->x; dyn=mpTargetY-m->y;
    d=sqrtf(dxn*dxn+dyn*dyn); if (d<0.001f) d=0.001f;
    dxn/=d; dyn/=d;
    m->fx=dxn; m->fy=dyn;
    move = m->bmMove[0] ? -1 : pickBossMove(m,d,&C);
    if (m->bmMove[0]==0){
        switch (move){
            case BM_BREATH:
                m->cdB=C.breathCd+frand()*1.2f;
                strcpy(m->bmMove,"breath"); m->bmPhase=0; m->winding=1;
                m->windT=BOSS.breathWind;
                sfxPlay(SFX_ABILITY);
                break;
            case BM_FIREBALL:
                m->cdFb=BOSS.fbCd+frand();
                strcpy(m->bmMove,"fireball"); m->bmPhase=0; m->winding=1; m->windT=C.fbWind>0?C.fbWind:BOSS.fbWind;
                break;
            case BM_FLY:
                m->cdFly=BOSS.flyCdMin+frand()*(BOSS.flyCdMax-BOSS.flyCdMin);
                strcpy(m->bmMove,"fly"); m->bmPhase=0; m->bmT=BOSS.flyDur; m->bmHasWp=0;
                smokeBurst(m->x,m->y,C_HEX(6A,5C,46),14);
                sfxPlay(SFX_BOSS_ROAR);
                break;
            case BM_STOMP:
                m->cdStomp=5.5f+frand()*2;
                strcpy(m->bmMove,"stomp"); m->bmPhase=0; m->winding=1; m->windT=BOSS.wind;
                break;
            case BM_SUMMON:
                m->cdSummon=12+frand()*4;
                strcpy(m->bmMove,"summon"); m->bmPhase=0; m->winding=1; m->windT=BOSS.wind+0.2f;
                break;
            case BM_CHARGE:
                m->cdCharge=8+frand()*3;
                strcpy(m->bmMove,"charge"); m->bmPhase=0; m->winding=1; m->windT=BOSS.wind;
                break;
            default:
                m->bmT=0.5f+frand()*0.3f;
                break;
        }
        if (m->bmMove[0]) { m->rx=m->x; m->ry=m->y; return; }
    }

    if (strcmp(m->bmMove,"fly")==0){
        m->bmT-=dt;
        {
            int aim=m->bmT<BOSS.flyDur*0.35f;
            if (aim){ m->bmWpX=mpTargetX; m->bmWpY=mpTargetY; m->bmHasWp=1; }
            else if (!m->bmHasWp || dist2f(m->x,m->y,m->bmWpX,m->bmWpY)<1.1f)
                pickBossFlyWaypoint(m,&m->bmWpX,&m->bmWpY), m->bmHasWp=1;
        }
        m->speed=t->speed*BOSS.flySpeedMul;
        if (d>0.9f){
            float mxv=m->bmWpX-m->x, myv=m->bmWpY-m->y;
            float ml=sqrtf(mxv*mxv+myv*myv); if (ml<1) ml=1;
            tryMoveEntity(L,&m->x,&m->y,m->speed,mxv/ml,myv/ml,dt,0.27f,excl);
        }
        if (m->bmT<=0){
            smokeBurst(m->x,m->y,C_HEX(6A,5C,46),16);
            if (d<=8){
                strcpy(m->bmMove,"dive"); m->bmPhase=0; m->bmT=BOSS.diveHover;
                m->bmTx=mpTargetX; m->bmTy=mpTargetY;
                m->winding=1; m->windT=BOSS.diveHover;
            } else { m->bmMove[0]=0; m->bmT=0.5f; }
        }
    } else if (strcmp(m->bmMove,"dive")==0){
        if (m->bmPhase==0){
            m->windT-=dt;
            if (dist2f(m->x,m->y,m->bmTx,m->bmTy)>0.04f){
                float svx=m->bmTx-m->x, svy=m->bmTy-m->y;
                float sl=sqrtf(svx*svx+svy*svy); if (sl<0.001f) sl=0.001f;
                {   float step=fminf(sl,dt*9.5f);
                    if (canOccupy(L,m->x+(svx/sl)*step,m->y,0.27f,NULL)) m->x+=(svx/sl)*step;
                    if (canOccupy(L,m->x,m->y+(svy/sl)*step,0.27f,NULL)) m->y+=(svy/sl)*step; }
            }
            if (m->windT<=0){
                m->winding=0;
                pfxBurst(m->bmTx,m->bmTy,22,(const unsigned int[]){C_HEX(FF,7A,2D),C_HEX(FF,D2,3D),0xFFFFFFFFu},3,
                         2.5f,7.5f,3.f,6.f,0.3f,0.65f,0,0);
                pushShockwave(m->bmTx,m->bmTy,0.5f,C_HEX(FF,7A,2D),0);
                spellFlash(m->bmTx,m->bmTy,C_HEX(FF,8A,3D),3.5f);
                if (dist2f(mpTargetX,mpTargetY,m->bmTx,m->bmTy)<=BOSS.diveR*BOSS.diveR){
                    dealTargetHit(BOSS.diveDmg,0);
                    addShake(0.85f); sfxPlay(SFX_BOSS_ROAR);
                } else { addShake(0.4f); sfxPlay(SFX_BOOM); }
                m->bmPhase=1; m->bmT=BOSS.diveRecover;
            }
        } else {
            m->bmT-=dt;
            if (m->bmT<=0){ m->bmMove[0]=0; m->bmT=0.5f; }
        }
    } else if (strcmp(m->bmMove,"breath")==0){
        if (m->bmPhase==0){
            m->windT-=dt;
            if (m->windT<=0){
                unsigned int c1=C.breathColor, c2=C.fbColor;
                float a=atan2f(m->fy,m->fx);
                int k;
                m->winding=0; m->atkT=0.4f;
                m->bmPhase=1; m->bmT=BOSS.breathBurn;
                m->bmHitDone=0;
                for (k=0;k<26;++k){
                    float rr=frand()*C.breathDist;
                    float off=(frand()-0.5f)*1.15f;
                    unsigned int cs[2]={c1,c2};
                    pfxBurst(m->x+cosf(a)*rr-sinf(a)*off,m->y+sinf(a)*rr+cosf(a)*off,
                             1,cs,2,0.5f,1.6f,2.5f,5.f,0.3f,0.6f,0,-0.6f);
                }
                pfxBurst(m->x,m->y,10,(const unsigned int[]){c1,c2},2,1.5f,4.f,3.f,6.f,0.2f,0.45f,0,0);
                spellFlash(m->x,m->y,c1,3.5f);
                sfxPlay(SFX_BOOM); addShake(0.35f);
                bossConeDamage(m,&C);
            }
        } else {
            if (!m->bmHitDone && m->bmT<=BOSS.breathBurn*0.55f){ m->bmHitDone=1; bossConeDamage(m,&C); }
            m->bmT-=dt;
            if (m->bmT<=0){ m->bmMove[0]=0; m->bmT=0.5f; }
        }
    } else if (strcmp(m->bmMove,"fireball")==0){
        if (m->bmPhase==0){
            m->windT-=dt;
            if (m->windT<=0){
                m->winding=0; m->atkT=0.4f;
                m->fbx=m->x+dxn*0.7f; m->fby=m->y+dyn*0.7f;
                m->fbvx=dxn*C.fbSpeed; m->fbvy=dyn*C.fbSpeed;
                m->fbLife=fmaxf(0.65f,fminf(1.9f,d/C.fbSpeed+0.15f));
                m->fbActive=1;
                m->bmPhase=2;
                sfxPlay(SFX_ABILITY);
            }
        } else if (m->bmPhase==2){
            int burst=0;
            m->fbLife-=dt;
            m->fbx+=m->fbvx*dt; m->fby+=m->fbvy*dt;
            if (dist2f(mpTargetX,mpTargetY,m->fbx,m->fby)<=(C.fbR+0.35f)*(C.fbR+0.35f)){
                bossFireballExplode(m,&C); burst=1;
            }
            if (!burst && m->fbLife<=0){ bossFireballExplode(m,&C); burst=1; }
            if (burst){ m->fbActive=0; m->bmPhase=1; m->bmT=0.4f; }
        } else {
            m->bmT-=dt;
            if (m->bmT<=0){ m->bmMove[0]=0; m->bmT=0.5f; }
        }
    } else if (strcmp(m->bmMove,"stomp")==0){
        if (m->bmPhase==0){
            m->windT-=dt;
            if (m->windT<=0){
                float r=C.stompR;
                m->winding=0; m->atkT=0.4f;
                pushShockwave(m->x,m->y,0.55f,C.fbColor,r);
                pfxBurst(m->x,m->y,18,(const unsigned int[]){C.fbColor,C_HEX(E8,E4,DC)},2,
                         2.f,6.5f,2.5f,5.5f,0.3f,0.65f,0,0);
                spellFlash(m->x,m->y,C.fbColor,3.4f);
                if (dist2f(m->x,m->y,mpTargetX,mpTargetY)<=r*r)
                    dealTargetHit((float)(m->dmg+2),0);
                addShake(0.55f); sfxPlay(SFX_BOOM);
                m->bmPhase=1; m->bmT=0.5f;
            }
        } else {
            m->bmT-=dt;
            if (m->bmT<=0){ m->bmMove[0]=0; m->bmT=0.5f; }
        }
    } else if (strcmp(m->bmMove,"summon")==0){
        if (m->bmPhase==0){
            m->windT-=dt;
            if (m->windT<=0){
                int activeMin=0,i,spawned=0,guard=0;
                m->winding=0; m->atkT=0.4f;
                for (i=0;i<MONSTER_CAP;++i)
                    if (g_world.monsters[i].type>=0 &&
                        g_world.monsters[i].fromBoss==m->id) ++activeMin;
                while (spawned<C.summonN-activeMin && guard++<60){
                    int x=(int)m->x+((int)(frand()*9)-4);
                    int y=(int)m->y+((int)(frand()*9)-4);
                    Monster* nm;
                    if (x<0||y<0||x>=L->w||y>=L->h || L->grid[y*L->w+x]!=T_FLOOR) continue;
                    nm=makeMonster(C.summonType,(float)x,(float)y,g_world.depth,AFFIX_NONE);
                    if (!nm) break;
                    nm->maxHp=fmaxf(1,(float)(int)(nm->maxHp*0.45f)); nm->hp=nm->maxHp;
                    nm->dmg=fmaxf(1,(int)(nm->dmg*0.7f));
                    nm->fromBoss=m->id;
                    pfxBurst(nm->x,nm->y,12,(const unsigned int[]){C.aura,0xFFFFFFFFu},2,
                             1.f,3.5f,2.5f,5.f,0.25f,0.55f,0,0);
                    ++spawned;
                }
                sfxPlay(SFX_ABILITY);
                m->bmPhase=1; m->bmT=0.6f;
            }
        } else {
            m->bmT-=dt;
            if (m->bmT<=0){ m->bmMove[0]=0; m->bmT=0.5f; }
        }
    } else if (strcmp(m->bmMove,"charge")==0){
        if (m->bmPhase==0){
            m->windT-=dt;
            if (m->windT<=0){
                m->winding=0;
                m->bmPhase=1; m->bmT=1.15f; m->bmHitDone=0;
                m->bmTx=mpTargetX; m->bmTy=mpTargetY;
                sfxPlay(SFX_BOSS_ROAR);
            }
        } else {
            m->bmT-=dt;
            {
                float cvx=m->bmTx-m->x, cvy=m->bmTy-m->y;
                float cl=sqrtf(cvx*cvx+cvy*cvy); if (cl<0.001f) cl=0.001f;
                tryMoveEntity(L,&m->x,&m->y,6.f,cvx/cl,cvy/cl,dt,0.27f,excl);
                if (!m->bmHitDone && cl<0.95f){
                    m->bmHitDone=1;
                    dealTargetHit((float)(m->dmg+2),0);
                    pushShockwave(m->x,m->y,0.45f,C.fbColor,0);
                    pfxBurst(m->x,m->y,20,(const unsigned int[]){C.fbColor,C_HEX(FF,D2,3D),0xFFFFFFFFu},3,
                             2.f,7.f,2.5f,5.5f,0.25f,0.6f,0,0);
                }
                if (m->bmT<=0 || (m->bmHitDone && cl<0.6f)){ m->bmMove[0]=0; m->bmT=0.6f; }
            }
        }
    } else {
        /* inseguimento + artiglio */
        if (d>1.05f){
            tryMoveEntity(L,&m->x,&m->y,t->speed,dxn,dyn,dt,0.27f,excl);
        } else if (m->atkCd<=0){
            if (!m->winding){ m->winding=1; m->windT=BOSS.wind; }
            else {
                m->windT-=dt;
                if (m->windT<=0){
                    m->winding=0; m->atkT=0.32f;
                    dealTargetHit((float)(m->dmg+(int)(frand()*2)),0);
                    m->atkCd=1.05f+frand()*0.35f;
                }
            }
        }
        m->bmT-=dt;
        if (m->bmT<=0) m->bmMove[0]=0;
    }
    m->rx=m->x; m->ry=m->y;
}

/* ------------------------------------------------------------------ */
/* hostRespawnTick: respawn mostri e power-up                          */
/* ------------------------------------------------------------------ */
static void hostRespawnTick(float dt){
    Layout* L=g_world.layout;
    int cap=g_world.depth*2+9; if (cap>30) cap=30;
    s_respawnTick-=dt;
    if (s_respawnTick<=0){
        int alive=0,i,count=0;
        s_respawnTick=13+frand()*11;
        for (i=0;i<MONSTER_CAP;++i) if (g_world.monsters[i].type>=0) ++alive;
        for (i=0;i<L->monSpotCount;++i) ++count;
        if (alive<cap && count>0){
            int si=(int)(frand()*count)%count;
            Rng rr; rng_seed(&rr,(unsigned int)(frand()*4294967296.f));
            makeMonster(pickMonsterType(&rr,g_world.depth),
                        (float)L->monSpots[si].x,(float)L->monSpots[si].y,
                        g_world.depth,AFFIX_NONE);
        }
    }
    s_powerupTick-=dt;
    if (s_powerupTick<=0){
        int hasPower=0,i;
        s_powerupTick=30+frand()*20;
        for (i=0;i<g_world.itemCount;++i)
            if (g_world.items[i].used && g_world.items[i].kind==IK_POWER) hasPower=1;
        if (!hasPower && L->powerupCount>0){
            Item* it=0;
            for (i=0;i<ITEM_CAP;++i) if (!g_world.items[i].used){ it=&g_world.items[i]; break; }
            if (it){
                int si=(int)(frand()*L->powerupCount)%L->powerupCount;
                memset(it,0,sizeof *it);
                it->used=1; it->kind=IK_POWER;
                it->x=(float)L->powerupSpots[si].x+0.5f;
                it->y=(float)L->powerupSpots[si].y+0.5f;
                it->buff=(int)(frand()*PW_COUNT)%PW_COUNT;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Proiettili e aggiornamenti effetti                                  */
/* ------------------------------------------------------------------ */
static void updateProjectiles(float dt){
    int i,j;
    for (i=0;i<PROJ_CAP;++i){
        Proj* pr=&g_projs[i];
        float prevX,prevY;
        if (!pr->used) continue;
        prevX=pr->x; prevY=pr->y;
        pr->x+=pr->vx*dt; pr->y+=pr->vy*dt;
        pr->life-=dt;
        if (!pr->foreign){
            int hit=0;
            for (j=0;j<MONSTER_CAP;++j){
                Monster* m=&g_world.monsters[j];
                float amount; int crit,k,skip=0;
                char buf[16];
                if (m->type<0) continue;
                if (dist2f(pr->x,pr->y,m->rx,m->ry)>=0.42f*0.42f) continue;
                if (pr->pierce){
                    for (k=0;k<pr->hitCount;++k)
                        if (pr->hitIds[k]==m->id){ skip=1; break; }
                    if (skip) continue;
                }
                rollDamage(&CLASSES[g_me.cls],&amount,&crit);
                amount=(float)(int)(amount*(pr->dmgMult)+0.5f);
                hostApplyMonsterDamage(m,amount);
                hitBurst(m->rx,m->ry,crit?C_HEX(FF,E0,8A):pr->color);
                if (crit){ sfxPlay(SFX_CRIT); addShake(0.28f); addHitstop(0.06f); addCritFlash(); }
                else     { sfxPlay(SFX_HIT);  addShake(0.09f); addHitstop(0.02f); }
                snprintf(buf,sizeof buf,"%d%s",(int)amount,crit?"!":"");
                spawnFloatText(m->rx,m->ry-0.5f,buf,crit?C_HEX(FF,CF,5C):C_HEX(F2,F2,F2));
                if (pr->pierce){
                    if (pr->hitCount<16) pr->hitIds[pr->hitCount++]=m->id;
                } else { hit=1; break; }
            }
            {
                int wallHit=!canOccupy(g_world.layout,pr->x,pr->y,0.15f,NULL);
                int wallMid=!wallHit && !canOccupy(g_world.layout,(prevX+pr->x)*0.5f,(prevY+pr->y)*0.5f,0.15f,NULL);
                if (hit||wallHit||wallMid){
                    if (!hit){
                        float wx=wallMid?(prevX+pr->x)*0.5f:prevX;
                        float wy=wallMid?(prevY+pr->y)*0.5f:prevY;
                        unsigned int cs[3]={C_HEX(D4,C4,A0),C_HEX(A8,90,70),C_HEX(6B,5A,44)};
                        unsigned int sm[1]={C_HEX(8A,7E,6A)};
                        pfxBurst(wx,wy,5,cs,3,1.5f,4.f,1.5f,4.f,0.25f,0.5f,2,6);
                        pfxBurst(wx,wy,3,sm,1,0.5f,1.5f,3.f,6.f,0.4f,0.8f,1,-0.4f);
                    }
                    pr->used=0; continue;
                }
            }
        }
        if (pr->life<=0) pr->used=0;
    }
}
static void updateFx(float dt){
    int i;
    for (i=0;i<PART_CAP;++i){
        Part* p=&g_parts[i];
        if (!p->used) continue;
        p->life-=dt;
        if (p->life<=0){ p->used=0; continue; }
        p->vy+=p->grav*dt;
        p->vx*=p->drag; p->vy*=p->drag;
        p->x+=p->vx*dt; p->y+=p->vy*dt;
    }
    for (i=0;i<FLOAT_CAP;++i){
        FloatText* f=&g_floats[i];
        if (!f->used) continue;
        f->y+=f->vy*dt; f->life-=dt*0.85f;
        if (f->life<=0) f->used=0;
    }
    for (i=0;i<16;++i){
        Shockwave* w=&g_shocks[i];
        if (!w->used) continue;
        w->life-=dt;
        if (w->life<=0) w->used=0;
    }
    for (i=0;i<24;++i){
        SpellFlash* f=&g_flashes[i];
        if (!f->used) continue;
        f->intensity-=dt*4.f;
        if (f->intensity<=0) f->used=0;
    }
    g_shakeTrauma=fmaxf(0,g_shakeTrauma-dt*2.6f);
    g_dmgFlashTimer=fmaxf(0,g_dmgFlashTimer-dt);
    g_critFlashTimer=fmaxf(0,g_critFlashTimer-dt);
    g_bossDeathFlashTimer=fmaxf(0,g_bossDeathFlashTimer-dt);
    g_depthFadeTimer=fmaxf(0,g_depthFadeTimer-dt);
    s_toastT=fmaxf(0,s_toastT-dt);
    s_bannerT=fmaxf(0,s_bannerT-dt);
    g_torchClock+=dt;
    g_animClock+=dt;
}

/* ------------------------------------------------------------------ */
/* gameUpdate: loop di aggiornamento (updateLocalPlayer + hostSimTick) */
/* ------------------------------------------------------------------ */
void gameUpdate(float dt,const GameInput* in){
    const ClassDef* c=&CLASSES[g_me.cls];
    Layout* L=g_world.layout;
    int k;

    if (g_hitstopTimer>0){
        g_hitstopTimer=fmaxf(0,g_hitstopTimer-dt);
        dt*=0.06f;
    }

    updateFx(dt);

    if (g_me.dead){
        g_me.respawnTimer-=dt;
        if (g_me.respawnTimer<=0) handleRespawn();
        return;
    }
    if (g_me.downed){
        g_me.downedTimer-=dt;
        if (g_me.downedTimer<=0) handleLocalDeath();
        updateFOV();
        return;
    }

    g_me.atkTimer=fmaxf(0,g_me.atkTimer-dt);
    g_me.abilityTimer=fmaxf(0,g_me.abilityTimer-dt);
    g_me.invulnTimer=fmaxf(0,g_me.invulnTimer-dt);
    g_me.animTimer=fmaxf(0,g_me.animTimer-dt);
    if (g_me.animTimer<=0) g_me.anim=0;
    for (k=0;k<PW_COUNT;++k) g_me.buffs[k]=fmaxf(0,g_me.buffs[k]-dt);
    if (g_me.pendingCharge>0){
        g_me.pendingCharge-=dt;
        if (g_me.pendingCharge<=0){ g_me.pendingCharge=0; fireChargedShot(); }
    }
    if (g_me.poisonTimer>0){
        g_me.poisonTimer-=dt;
        g_me.poisonAcc+=dt;
        if (g_me.poisonAcc>1.f){
            g_me.poisonAcc-=1.f;
            g_me.hp=fmaxf(0.f,g_me.hp-1);
            if (g_me.hp<=0) handleDowned();
        }
    }
    if (c->maxMp>0 && g_me.mp<g_me.maxMp) g_me.mp=fminf(g_me.maxMp,g_me.mp+dt*0.8f);

    /* movimento */
    g_me.speed=c->speed*(g_me.buffs[PW_HASTE]>0?1.4f:1.f)*(1.f+computeEquipBonus().speedPct/100.f);
    {
        float mvx=in->mvx, mvy=in->mvy;
        float len=sqrtf(mvx*mvx+mvy*mvy);
        if (len>1.f){ mvx/=len; mvy/=len; }
        if (len>0.01f){
            float stepAccBak=0;
            g_me.facingX=mvx/len; g_me.facingY=mvy/len;
            tryMoveEntity(L,&g_me.x,&g_me.y,g_me.speed,mvx/len,mvy/len,dt,0.24f,NULL);
            g_me.stepAcc+=dt;
            if (g_me.stepAcc>0.26f){
                g_me.stepAcc=0;
                sfxPlay(SFX_STEP);
                stepDust(g_me.x,g_me.y,g_me.facingX,g_me.facingY);
            }
            (void)stepAccBak;
        }
    }

    if (in->attackHeld) performAttack();

    updateFOV();
    checkItemPickup();

    /* ---- simulazione mondo (hostSimTick) ---- */
    {   Room* br=L->hasBossRoom?&L->bossRoom:0;
        if (br && !g_world.bossActive && !g_world.bossDead &&
            playerAlive() &&
            g_me.x>=br->x && g_me.x<br->x+br->w &&
            g_me.y>=br->y && g_me.y<br->y+br->h){
            char buf[96];
            g_world.bossActive=1; g_world.bossDead=0;
            applyBossGates(1);
            snprintf(buf,sizeof buf,"L'arena si e' sigillata: %s e' scatenato!",
                     currentBossName(g_world.depth));
            showToast(buf);
            logLine(buf,C_HEX(E8,A1,3D));
            addShake(1.1f);
            sfxPlay(SFX_BOSS_ROAR);
            {   /* il giocatore entra nell'arena ai piedi della tana */
                g_me.x=(float)br->x+1.5f; g_me.y=(float)br->y+br->h-1.5f;
            }
        }
    }
    {   int i;
        for (i=0;i<MONSTER_CAP;++i)
            if (g_world.monsters[i].type>=0)
                updateMonsterAI(&g_world.monsters[i],dt);
    }
    hostRespawnTick(dt);
}
/* ------------------------------------------------------------------ */
/* Rendering del mondo (render() dell'originale, vista topdown)        */
/* ------------------------------------------------------------------ */
Monster* findBossMonster(void){
    int i;
    for (i=0;i<MONSTER_CAP;++i)
        if (g_world.monsters[i].type>=0 && monType(g_world.monsters[i].type)->boss)
            return &g_world.monsters[i];
    return 0;
}
Item* itemAt(int i){ return &g_world.items[i]; }
int monsterAliveCount(void){
    int i,n=0;
    for (i=0;i<MONSTER_CAP;++i) if (g_world.monsters[i].type>=0) ++n;
    return n;
}

static const int MON_SPRITE[MONSTER_COUNT] = {
    AR_MON_RATTO, AR_MON_PIPISTRELLO, AR_MON_GOBLIN, AR_MON_MELMA,
    AR_MON_GELATINA, AR_MON_SCHELETRO, AR_MON_ORCO, AR_MON_ZOMBIE,
    AR_MON_RAGNO, AR_MON_SPETTRO, AR_MON_DRAGO,
    AR_MON_SERPENTE, AR_MON_ARPIA, AR_MON_CAVALIERE, AR_MON_CULTISTA,
    AR_MON_MANTIDE, AR_MON_SCIAMANO, AR_MON_GOLEM,
    AR_BOSS_GOLEM, AR_BOSS_LICH, AR_BOSS_MELME, AR_BOSS_RAGNO, AR_BOSS_RATTI
};
/* dimensioni relative alla cella, come drawSprite nell'originale */
static float monSpriteScale(int type){
    switch (type){
        case MT_RATTO: case MT_PIPISTRELLO: case MT_SERPENTE: return 0.55f;
        case MT_MELMA: case MT_GOBLIN: case MT_ARPIA: case MT_CULTISTA: return 0.65f;
        case MT_GELATINA: case MT_SCHELETRO: case MT_MANTIDE: case MT_ZOMBIE:
        case MT_SCIAMANO: return 0.75f;
        case MT_ORCO: case MT_RAGNO: case MT_CAVALIERE: case MT_SPETTRO:
        case MT_GOLEM_ROCCIA: return 0.85f;
        default: return 1.35f; /* boss */
    }
}
static const int HERO_SPRITE[CLASS_COUNT] = {
    AR_HERO_GUERRIERO, AR_HERO_LADRO, AR_HERO_MAGO, AR_HERO_RANGER,
    AR_HERO_PROF, AR_HERO_PALADINO, AR_HERO_NEGROMANTE, AR_HERO_BARDO,
    AR_HERO_MONACO
};

void gameRenderWorld(void){
    Layout* L=g_world.layout;
    int x0,y0,x1,y1,tx,ty,i;
    float flick,camX,camY,viewCols,viewRows;
    float bgR,bgG,bgB; unsigned int bgCol;
    float shakeX=0,shakeY=0;

    if (!L) return;

    /* sfondo che vira col piano (identico all'originale) */
    {
        int dp=g_world.depth<30?g_world.depth:30;
        bgR=(float)(int)(10+dp*0.55f); bgG=(float)(int)(9+dp*0.35f); bgB=(float)(int)(6+dp*1.1f);
        if (bgR>255)bgR=255; if(bgG>255)bgG=255; if(bgB>255)bgB=255;
        bgCol=COL((int)bgR,(int)bgG,(int)bgB,255);
    }
    /* screen shake: trauma quadratico */
    if (g_shakeTrauma>0.001f){
        float tr=g_shakeTrauma*g_shakeTrauma;
        shakeX=(frand()-0.5f)*22.f*tr;
        shakeY=(frand()-0.5f)*22.f*tr;
    }
    viewCols=(float)SCR_W/TILE_PX; viewRows=(float)SCR_H/TILE_PX;
    camX=g_me.x-viewCols/2+shakeX/TILE_PX;
    camY=g_me.y-viewRows/2+shakeY/TILE_PX;
    gfxFrameStart(camX,camY,bgCol);

    flick=0.9f+0.1f*sinf(g_torchClock*3.1f)+0.04f*sinf(g_torchClock*7.7f);
    x0=(int)camX-1; y0=(int)camY-1;
    x1=(int)(camX+viewCols)+2; y1=(int)(camY+viewRows)+2;

    /* ---- tiles ---- */
    for (ty=y0;ty<=y1;++ty){
        if (ty<0||ty>=L->h) continue;
        for (tx=x0;tx<=x1;++tx){
            float sx,sy,alpha,tileLight;
            int tval;
            if (tx<0||tx>=L->w) continue;
            if (!g_world.visible[ty*L->w+tx] && !g_world.visited[ty*L->w+tx]) continue;
            gfxWorldToScreen((float)tx,(float)ty,&sx,&sy);
            tileLight = g_world.visible[ty*L->w+tx] ? flick : 0.32f;
            tval=L->grid[ty*L->w+tx];
            alpha=tileLight;
            if (tval==T_WALL){
                if (!isWallSkin(L,tx,ty)) continue;   /* i muri interni restano oscurita' */
                gfxDrawSprite(AR_WALL_BRICK,sx+TILE_PX/2,sy+TILE_PX/2,TILE_PX+1.f,alpha,
                              ((computeWallMask(L,tx,ty)&8)!=0),0);
                /* coppietto superiore piu' scuro quando il muro ha cielo sopra */
                if (isWallSkin(L,tx,ty-1))
                    gfxQuad(sx,sy,TILE_PX,TILE_PX*0.28f,COL(0,0,0,(unsigned char)(70*alpha)));
            } else {
                const ARect* fr=&g_atlasRects[((tx+ty)&1)?AR_FLOOR_STONE:AR_FLOOR_DIRT];
                gfxDrawSpriteRect(fr,sx+TILE_PX/2,sy+TILE_PX/2,TILE_PX+1.f,TILE_PX+1.f,alpha,0,0);
                if (tval==T_STAIRS){
                    float stPulse=0.55f+0.45f*sinf(g_torchClock*2.4f);
                    gfxDrawSprite(AR_STAIRS,sx+TILE_PX/2,sy+TILE_PX/2,TILE_PX*0.8f,flick,0,0);
                    gfxGlow(sx+TILE_PX/2,sy+TILE_PX/2,TILE_PX*0.9f,C_HEX(E8,A1,3D),0.20f*stPulse*flick);
                    gfxGlow(sx+TILE_PX/2,sy-TILE_PX,TILE_PX*0.55f,C_HEX(E8,A1,3D),0.10f*stPulse*flick);
                    if (frand()<0.02f){
                        Part* p=allocPart();
                        if (p){
                            p->used=1;
                            p->x=tx+0.5f+(frand()-0.5f)*0.7f; p->y=ty+1;
                            p->vx=(frand()-0.5f)*0.15f; p->vy=-0.5f-frand()*0.3f;
                            p->life=p->maxLife=0.9f+frand()*0.5f; p->size=1.5f;
                            p->color=C_HEX(F0,C8,78); p->grav=0; p->drag=0.99f; p->type=0;
                        }
                    }
                }
            }
        }
    }

    /* ---- torce ---- */
    for (i=0;i<L->torchCount;++i){
        float sx,sy;
        int txx=L->torches[i].x, tyy=L->torches[i].y;
        if (txx<x0-1||txx>x1+1||tyy<y0-1||tyy>y1+1) continue;
        if (!g_world.visited[tyy*L->w+txx]) continue;
        gfxWorldToScreen((float)txx+0.5f,(float)tyy+0.72f,&sx,&sy);
        if (g_world.visible[tyy*L->w+txx]){
            float fl=0.75f+0.25f*sinf(g_torchClock*6.3f+i*1.7f);
            gfxDrawSprite(AR_TORCH,sx,sy,TILE_PX*1.05f,flick,0,0);
            gfxGlow(sx,sy,TILE_PX*(1.4f+0.25f*fl),C_HEX(FF,B4,6E),0.30f*fl);
        } else {
            gfxDrawSprite(AR_TORCH,sx,sy,TILE_PX*1.05f,0.32f,0,0);
        }
    }

    /* ---- oggetti a terra ---- */
    for (i=0;i<ITEM_CAP;++i){
        Item* it=&g_world.items[i];
        float sx,sy; int vis;
        if (!it->used) continue;
        tx=(int)it->x; ty=(int)it->y;
        if (tx<x0||tx>x1||ty<y0||ty>y1) continue;
        vis=g_world.visible[ty*L->w+tx];
        if (!vis && !g_world.visited[ty*L->w+tx]) continue;
        gfxWorldToScreen(it->x,it->y,&sx,&sy);
        {
            float bob=sinf(g_torchClock*3.f+i)*1.5f;
            float a=vis?flick:0.32f;
            int spr=AR_ICON_GOLD;
            float size=TILE_PX*0.42f;
            switch (it->kind){
                case IK_GOLD:   spr=AR_ICON_GOLD; break;
                case IK_GEM:    spr=AR_ICON_GEM_BLUE; break;
                case IK_POTION: spr=AR_ICON_POTION_HP; break;
                case IK_MANAPOT:spr=AR_ICON_POTION_MANA; break;
                case IK_POWER:  spr=AR_PW_FURIA+it->buff; size=TILE_PX*0.5f; break;
                case IK_EQUIP:
                    spr=AR_EQUIP_HELM+it->eq.slot; size=TILE_PX*0.5f;
                    gfxGlow(sx,sy+bob,TILE_PX*0.5f,RARITIES[it->eq.rarity].color,0.35f*a);
                    break;
            }
            gfxDrawSprite(spr,sx,sy+bob,size,a,0,0);
        }
    }

    /* ---- forzieri e mercante ---- */
    for (i=0;i<L->chestCount;++i){
        float sx,sy; int vis;
        tx=L->chests[i].x; ty=L->chests[i].y;
        if (tx<x0||tx>x1||ty<y0||ty>y1) continue;
        vis=g_world.visible[ty*L->w+tx];
        if (!vis && !g_world.visited[ty*L->w+tx]) continue;
        gfxWorldToScreen((float)tx+0.5f,(float)ty+0.5f,&sx,&sy);
        gfxDrawSprite(L->opened[i]?AR_CHEST_OPEN:AR_CHEST_CLOSED,
                      sx,sy,TILE_PX*0.72f,vis?flick:0.32f,0,0);
    }
    {
        float sx,sy; int vis;
        tx=L->merchantX; ty=L->merchantY;
        if (tx>=x0&&tx<=x1&&ty>=y0&&ty<=y1){
            vis=g_world.visible[ty*L->w+tx];
            if (vis || g_world.visited[ty*L->w+tx]){
                gfxWorldToScreen((float)tx+0.5f,(float)ty+0.45f,&sx,&sy);
                gfxDrawSprite(AR_MERCHANT,sx,sy,TILE_PX*1.15f,vis?flick:0.32f,0,0);
            }
        }
    }

    /* ---- entita' ordinate per Y ---- */
    {   /* raccolta indici + y per un sort semplice (insertion sort sui mostri) */
        struct EntRef { Monster* m; int isPlayer; float y; };
        static struct EntRef ents[MONSTER_CAP+1];
        int n=0,j,k;
        for (i=0;i<MONSTER_CAP;++i){
            Monster* m=&g_world.monsters[i];
            if (m->type>=0){
                int mtx=(int)m->ry, mty=(int)m->ry;
                (void)mtx;
                if (!g_world.visible[mty*L->w+(int)m->rx]) continue;
                ents[n].m=m; ents[n].isPlayer=0; ents[n].y=m->ry; ++n;
            }
        }
        ents[n].m=0; ents[n].isPlayer=1; ents[n].y=g_me.y; ++n;
        for (j=1;j<n;++j){
            struct EntRef key=ents[j];
            k=j-1;
            while (k>=0 && ents[k].y>key.y){ ents[k+1]=ents[k]; --k; }
            ents[k+1]=key;
        }
        for (j=0;j<n;++j){
            float sx,sy;
            if (ents[j].isPlayer){
                const ARect* r=&g_atlasRects[HERO_SPRITE[g_me.cls]];
                float bob=(g_me.anim? sinf(g_torchClock*30)*2 : sinf(g_torchClock*2.2f)*1.2f);
                float lungeX=0,lungeY=0;
                if (g_me.anim){
                    lungeX=g_me.facingX*TILE_PX*0.12f;
                    lungeY=g_me.facingY*TILE_PX*0.12f;
                }
                gfxWorldToScreen(g_me.x,g_me.y,&sx,&sy);
                /* alone caldo del giocatore */
                gfxGlow(sx,sy,TILE_PX*1.6f,C_HEX(FF,BA,6E),0.10f*flick);
                gfxDrawSpriteRect(r,sx+lungeX,sy+bob,TILE_PX*0.95f,TILE_PX*0.95f,
                                  flick,g_me.facingX<0,
                                  g_me.invulnTimer>0.3f?0.6f:0);
                if (g_me.chargeT>0 || g_me.pendingCharge>0)
                    gfxGlow(sx,sy,TILE_PX*1.1f,C_HEX(7D,F9,FF),0.4f);
                if (g_me.buffs[PW_SHIELD]>0)
                    gfxGlow(sx,sy,TILE_PX*0.95f,C_HEX(5F,A0,C9),0.35f);
            } else {
                Monster* m=ents[j].m;
                const MonsterType* t=monType(m->type);
                float light=g_world.visible[(int)m->ry*L->w+(int)m->rx]?flick:0.32f;
                float flash=m->hitFlashAt>0 ? fmaxf(0,1-(g_torchClock-m->hitFlashAt)/0.12f) : 0;
                float bob=sinf(g_torchClock*2.6f+m->id)*1.4f;
                float scale=monSpriteScale(m->type)*(t->boss?1.f:1.f);
                float windPush=m->winding?-TILE_PX*0.08f:0;
                gfxWorldToScreen(m->rx,m->ry,&sx,&sy);
                if (t->boss) gfxGlow(sx,sy,TILE_PX*1.5f,t->color,0.18f*light);
                gfxDrawSprite(MON_SPRITE[m->type],
                              sx+m->fx*windPush,sy+bob+m->fy*windPush,
                              TILE_PX*scale,light,m->fx<0,flash);
                /* barra HP se danneggiato */
                if (m->hp<m->maxHp){
                    float bw=TILE_PX*0.8f;
                    gfxQuad(sx-bw/2,sy-TILE_PX*scale/2-6,bw,3,COL(20,16,12,200));
                    gfxQuad(sx-bw/2,sy-TILE_PX*scale/2-6,bw*(m->hp/m->maxHp),3,
                            COL(196,68,58,230));
                }
                if (m->affix==AFFIX_VELOCE)
                    gfxDrawSprite(AR_ICON_LIGHTNING,sx+TILE_PX*0.3f,sy-TILE_PX*scale/2-8,
                                  TILE_PX*0.28f,light,0,0);
            }
        }
    }

    /* proiettili */
    for (i=0;i<PROJ_CAP;++i){
        Proj* pr=&g_projs[i];
        float sx,sy;
        if (!pr->used) continue;
        gfxWorldToScreen(pr->x,pr->y,&sx,&sy);
        gfxGlow(sx,sy,pr->boss?TILE_PX*0.85f:TILE_PX*0.5f,pr->color,0.65f);
        gfxQuad(sx-2,sy-2,4,4,withAlpha(0xFFFFFF,0.9f));
    }

    /* particelle */
    for (i=0;i<PART_CAP;++i){
        Part* p=&g_parts[i];
        float sx,sy,a;
        if (!p->used) continue;
        a=p->life/p->maxLife;
        gfxWorldToScreen(p->x,p->y,&sx,&sy);
        if (p->type==1) gfxGlow(sx,sy,p->size*2.2f,p->color,0.30f*a);
        else gfxQuad(sx-p->size/2,sy-p->size/2,p->size,p->size,withAlpha(p->color,a));
    }

    /* shockwaves */
    for (i=0;i<16;++i){
        Shockwave* w=&g_shocks[i];
        float sx,sy,prog,radius;
        if (!w->used) continue;
        prog=1-w->life/w->maxLife;
        radius=w->r>0? w->r : 1.2f;
        gfxWorldToScreen(w->x,w->y,&sx,&sy);
        radius*=TILE_PX*(0.3f+prog*1.1f);
        gfxGlow(sx,sy,radius,w->color,0.5f*(1-prog));
    }

    /* spell flashes */
    for (i=0;i<24;++i){
        SpellFlash* f=&g_flashes[i];
        float sx,sy;
        if (!f->used) continue;
        gfxWorldToScreen(f->x,f->y,&sx,&sy);
        gfxGlow(sx,sy,TILE_PX*1.2f*f->intensity*0.4f+TILE_PX*0.4f,f->color,
                fminf(0.8f,f->intensity*0.25f));
    }

    /* testi flottanti */
    for (i=0;i<FLOAT_CAP;++i){
        FloatText* ft=&g_floats[i];
        float sx,sy,w;
        if (!ft->used) continue;
        gfxWorldToScreen(ft->x,ft->y,&sx,&sy);
        w=gfxTextW(ft->txt,1);
        gfxText(sx-w/2,sy,ft->txt,withAlpha(ft->color,fminf(1,ft->life*2)),1);
    }

    /* flash danno rosso direzionale */
    if (g_dmgFlashTimer>0){
        float a=g_dmgFlashTimer*0.55f;
        float bias=g_me.lastHitX;  /* -1..1: rosso dal lato del colpo */
        if (bias<0) gfxQuad(0,0,SCR_W*0.5f,SCR_H,withAlpha(C_HEX(C1,44,3A),a));
        else        gfxQuad(SCR_W*0.5f,0,SCR_W*0.5f,SCR_H,withAlpha(C_HEX(C1,44,3A),a));
        gfxQuad(0,0,SCR_W,SCR_H,withAlpha(C_HEX(C1,44,3A),a*0.4f));
    }
    if (g_critFlashTimer>0)
        gfxQuad(0,0,SCR_W,SCR_H,withAlpha(0xFFFFFF,g_critFlashTimer*1.2f));
    if (g_bossDeathFlashTimer>0)
        gfxQuad(0,0,SCR_W,SCR_H,withAlpha(C_HEX(FF,D2,3D),g_bossDeathFlashTimer*1.1f));

    /* dissolvenza cambio piano */
    if (g_depthFadeTimer>0)
        gfxQuad(0,0,SCR_W,SCR_H,withAlpha(C_HEX(02,01,01),g_depthFadeTimer/DEPTH_FADE_TIME));

    /* vignetta semplice */
    {
        int e=26;
        gfxQuad(0,0,SCR_W,e,COL(2,1,1,60));
        gfxQuad(0,SCR_H-e,SCR_W,e,COL(2,1,1,60));
        gfxQuad(0,0,e,SCR_H,COL(2,1,1,60));
        gfxQuad(SCR_W-e,0,e,SCR_H,COL(2,1,1,60));
    }
}