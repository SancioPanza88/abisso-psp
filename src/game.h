#ifndef ABISSO_GAME_H
#define ABISSO_GAME_H

#include "common.h"
#include "data.h"
#include "dungeon.h"

/* =====================================================================
   Stato di gioco: traduzione delle strutture dell'originale
   (me / world / monsters / items / projectiles / particles / ...)
   ===================================================================== */

typedef struct {
    int cls;
    const char* name;
    float x,y;
    float facingX,facingY;
    float hp,maxHp, mp,maxMp;
    int gold, potions, manaPotions;
    float atkTimer, invulnTimer, poisonTimer, abilityTimer;
    int dead, downed;
    float downedTimer, respawnTimer;
    float poisonAcc;
    float speed;              /* ricalcolata ogni frame come l'originale */
    int anim;                 /* 0 none, 1 attack */
    float animTimer;
    float chargeT;            /* Colpo Caricato del Prof */
    float pendingCharge;      /* timer del fuoco differito */
    float buffs[PW_COUNT];
    EquipItem equip[EQ_SLOT_COUNT];
    float lastHitX,lastHitY;
    float prevHpShown;
    int stepAccFlag; float stepAcc;
} Player;

enum { IK_GOLD=0, IK_GEM, IK_POWER, IK_EQUIP, IK_POTION, IK_MANAPOT };
typedef struct {
    int used;
    int kind;
    float x,y;                /* centro cella */
    int amount;
    int buff;
    EquipItem eq;
} Item;

#define MONSTER_CAP 128
#define ITEM_CAP    256
#define PROJ_CAP     96
#define PART_CAP    640
#define FLOAT_CAP     48

enum { WF_NONE=0, WF_DASH, WF_SWOOP, WF_STOMP, WF_CONE };

typedef struct Monster {
    int id, type;
    float x,y,rx,ry;
    float hp,maxHp;
    int dmg;
    float speed, aggro;
    int state;                 /* 0 idle, 1 chase, 2 wander */
    float fx,fy;
    float wanderT, wtgtX, wtgtY;
    float atkCd;
    int splitLeft;
    int affix;
    float hitFlashAt;
    float atkT;
    /* attacchi dedicati */
    int dashKind;              /* WF_DASH / WF_SWOOP attivi */
    float dTx,dTy,dT,dDur,dSpeed; int dHit;
    int boltActive; float bVx,bVy,bLife;
    float dentT;
    int winding; float windT; int windFx;
    /* cervello boss (_bm) */
    char bmMove[12];           /* "", fly, dive, breath, fireball, stomp, summon, charge */
    int   bmPhase;             /* 0 wind/flight, 1 burn/done/rush/recover */
    float bmT, bmTx, bmTy, bmWpX, bmWpY;
    int   bmHasWp, bmHitDone;
    int   fbActive; float fbx,fby,fbvx,fbvy,fbLife;
    float cdB,cdFb,cdFly,cdSummon,cdStomp,cdCharge;
    int fromBoss;
    float alertFlash;
} Monster;

typedef struct Proj {
    int used;
    float x,y,vx,vy,life;
    unsigned int color;
    int atlasId;               /* AR_ICON_* usato come glifo visivo */
    float sizePx;
    int foreign,boss,pierce;
    float dmgMult;
    int hitIds[16]; int hitCount;
} Proj;

typedef struct Part {
    int used;
    float x,y,vx,vy,life,maxLife,size;
    unsigned int color;
    float grav,drag;
    int type;                  /* 0 dot,1 smoke,2 shard */
} Part;

typedef struct FloatText {
    int used;
    float x,y,vy,life;
    char txt[24];
    unsigned int color;
} FloatText;

typedef struct Shockwave { int used; float x,y,life,maxLife,r; unsigned int color; } Shockwave;
typedef struct SpellFlash{ int used; float x,y,intensity; unsigned int color; } SpellFlash;

typedef struct {
    int depth;
    Layout* layout;
    Monster monsters[MONSTER_CAP]; int monsterCount;
    Item items[ITEM_CAP]; int itemCount;
    unsigned char visited[130*74];
    unsigned char visible[130*74];
    int lastTileX,lastTileY;
    int bossActive, bossDead;
    int nextMonsterId, nextItemId;
} World;

/* record locale (permadeath) */
extern int g_recordGold, g_recordDepth;

extern World g_world;
extern Player g_me;
extern float g_torchClock, g_animClock;
extern float g_shakeTrauma, g_hitstopTimer, g_dmgFlashTimer;
extern float g_critFlashTimer, g_bossDeathFlashTimer, g_depthFadeTimer;
extern int g_minimapVisible;

/* input per frame (riempito da main.c) */
typedef struct {
    float mvx,mvy;
    int attackHeld;
    int interactPressed;
} GameInput;

void gameNewRun(int cls);
void gameUpdate(float dt, const GameInput* in);
void gameRenderWorld(void);          /* mondo (tiles, entita', fx, luci) */

void performAttack(void);
void useClassAbility(void);
void drinkPotion(void);
void drinkManaPotion(void);
void tryInteract(void);
void toggleMinimap(void);
void buyFromMerchant(int kind);

/* HUD/log per la ui */
void showToast(const char* text);
void showLootBanner(const char* iconTxt, const char* text, unsigned int color);
void logLine(const char* text, unsigned int color);
const char* currentToast(void);      /* NULL se scaduto */
float toastRemaining(void);
const char* currentBanner(int* isIconLine); /* banner: due righe */
float bannerRemaining(void);
unsigned int bannerColor(void);
const char* logLineAt(int back);     /* 0 = piu' recente */
unsigned int logColorAt(int back);
typedef struct { int hp,dmgPct,speedPct,armorPct; } EquipBonus;
EquipBonus computeEquipBonus(void);
void gameRandSeed(unsigned int s);
Monster* findBossMonster(void);
Item* itemAt(int i);
int monsterAliveCount(void);

#endif
