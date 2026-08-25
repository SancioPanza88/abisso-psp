#ifndef ABISSO_DATA_H
#define ABISSO_DATA_H

#include "common.h"

/* =====================================================================
   Tabelle dati tradotte 1:1 dall'originale Abisso 2.0 (index.html)
   ===================================================================== */

typedef enum {
    CL_GUERRIERO=0, CL_LADRO, CL_MAGO, CL_RANGER, CL_PROF,
    CL_PALADINO, CL_NEGROMANTE, CL_BARDO, CL_MONACO,
    CLASS_COUNT
} ClassId;

typedef enum {
    AB_CHARGE=0, AB_SHADOWSTEP, AB_SHOCKWAVE, AB_VOLLEY, AB_CHARGEDSHOT,
    AB_HOLYSHIELD, AB_SOULDRAIN, AB_SONG, AB_CHIWAVE
} AbilityKind;

typedef enum { AS_SLASH=0, AS_DOUBLE, AS_CAST, AS_BOW, AS_OVERHEAD, AS_STAB, AS_SPIN, AS_PLASMA } AtkStyle;

typedef struct {
    const char* name;
    const char* desc;
    AbilityKind kind;
    float cooldown;
    int   manaCost;
    float chargeTime;   /* solo Colpo Caricato del Prof */
} AbilityDef;

typedef struct {
    const char* key;
    const char* name;
    const char* desc;
    int   hp;
    int   maxMp;
    int   manaCost;
    float speed;
    float atkCooldown;
    float range;
    float arc;          /* melee only */
    int   dmgMin, dmgMax;
    int   ranged;
    float crit;
    float projSpeed;
    unsigned int projColor;
    unsigned int meleeColor;
    unsigned int fxCol1, fxCol2;
    AtkStyle style;
    float atkDur;
    AbilityDef ability;
} ClassDef;

extern const ClassDef CLASSES[CLASS_COUNT];

/* ---- Tipi di mostro (MONSTER_TYPES) ---- */
typedef struct {
    char ch;
    const char* name;
    unsigned int color;
    int   hp;
    int   dmg;
    float speed;
    float aggro;
    int   goldMin, goldMax;
    /* weight(depth) dell'originale:
       0 = decrescente lineare (max(0,start-d))  1 = costante
       2 = a soglia (wLow prima di minDepth, wHigh dopo)  3 = zero (boss) */
    int   wKind, wStart, wLow, wHigh, wMinDepth;
    int erratic, split, poison, lifesteal, dash, swoop, cone, doubleAtk, stomp, boss, ranged;
    int beam;           /* sciamano */
    float range;        /* gittata attacco a distanza */
} MonsterType;

enum {
    MT_RATTO=0, MT_PIPISTRELLO, MT_GOBLIN, MT_MELMA, MT_GELATINA, MT_SCHELETRO,
    MT_ORCO, MT_ZOMBIE, MT_RAGNO, MT_SPETTRO, MT_DRAGO,
    MT_SERPENTE, MT_ARPIA, MT_CAVALIERE, MT_CULTISTA, MT_MANTIDE, MT_SCIAMANO, MT_GOLEM_ROCCIA,
    MT_BOSS_GOLEM, MT_BOSS_LICH, MT_BOSS_MELME, MT_BOSS_RAGNO, MT_BOSS_RATTI,
    MONSTER_COUNT
};
#define MONSTER_MAX_WEIGHT_KINDS 4
int monWeight(int type, int depth);
const MonsterType* monType(int type);
int monTypeByChar(char c);

/* Affissi (MONSTER_AFFIXES) */
typedef struct { const char* name; unsigned int color; float speedMult; float explodeRadius; float regenPerSec; } AffixDef;
enum { AFFIX_NONE=-1, AFFIX_VELOCE=0, AFFIX_ESPLOSIVO, AFFIX_RIGENERANTE, AFFIX_COUNT };
extern const AffixDef AFFIXES[AFFIX_COUNT];
float pickAffixChance(int depth);

/* Potenziamenti (POWERUP_TYPES) */
typedef struct { const char* name; const char* desc; unsigned int color; float duration; } PowerDef;
enum { PW_RAGE=0, PW_SHIELD, PW_HASTE, PW_FOCUS, PW_COUNT };
extern const PowerDef POWERS[PW_COUNT];

/* Equipaggiamento (EQUIP_INFO / STAT_POOL / RARITY_TIERS) */
typedef struct { const char* name; } EquipSlotInfo;
enum { EQ_HELM=0, EQ_NECKLACE, EQ_ARMOR, EQ_RING, EQ_GREAVES, EQ_SLOT_COUNT };
extern const EquipSlotInfo EQUIP_SLOTS[EQ_SLOT_COUNT];

enum { ST_HP=0, ST_DMG_PCT, ST_SPEED_PCT, ST_ARMOR_PCT, ST_COUNT };
const char* statFormat(int statKey, int value);
int  statCalc(int statKey, int depth);

enum { RAR_COMUNE=0, RAR_RARO, RAR_EPICO, RAR_LEGGEN, RARITY_COUNT };
typedef struct { const char* key; const char* name; unsigned int color; int weight; float mult; int statCount; } RarityTier;
extern const RarityTier RARITIES[RARITY_COUNT];

/* ---- Boss (BOSS / BOSS_VARIANTS) ---- */
typedef struct {
    int   noFly, noDive, stomp, summon, charge, web, poisonHit, spit;
    float stompR, breathCd, fbSpeed, fbR, fbDmg, fbWind, breathDist;
    int   summonType, summonN;
    unsigned int fbColor, breathColor, aura;
} BossVariant;
void bossVariantFor(int monsterType, BossVariant* out);
extern const struct BossBase {
    float wind, breathWind, breathBurn, breathDist, breathArcCos, breathDmg, breathCd;
    float fbWind, fbSpeed, fbR, fbDmg, fbCd;
    float flyCdMin, flyCdMax, flyDur, flySpeedMul, diveHover, diveR, diveDmg, diveRecover;
} BOSS;

/* Piani dei boss */
#define BOSS_FLOOR_STRIDE 5
int isBossFloor(int depth);
int bossTypeForDepth(int depth);
const char* currentBossName(int depth);

/* Nomi per il soprannome casuale dell'eroe */
extern const char* const FIRST_NAMES[25];
const char* randomNickname(void);

/* Utility danno/equip usate dal gioco */
typedef struct { int slot; int rarity; int statKey[3]; int statVal[3]; int statCount; } EquipItem;
EquipItem makeEquipItem(int depth, Rng* rng);
EquipItem makePremiumEquip(int depth, Rng* rng);
int itemPower(const EquipItem* e);

#endif
