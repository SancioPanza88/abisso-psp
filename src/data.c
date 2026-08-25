#include "data.h"

/* =====================================================================
   Dati 1:1 dall'originale (sezione "Classi eroe" e "Tipi di mostro")
   ===================================================================== */

#define C_HEX(rr,gg,bb) COL(0x##rr,0x##gg,0x##bb,255)

const ClassDef CLASSES[CLASS_COUNT] = {
    /* guerriero: hp16 speed3.6 cd0.5 range1.55 arc1.45 dmg[3,6] */
    { "guerriero", "Guerriero", "Tanto in salute, colpisce da vicino.",
      16, 0, 0, 3.6f, 0.5f, 1.55f, 1.45f, 3, 6, 0, 0.f,
      0.f, C_HEX(dc,dc,e0), C_HEX(dc,dc,e0), C_HEX(dc,dc,e0), C_HEX(ff,d9,8a),
      AS_SLASH, 0.26f,
      { "Carica", "Scatta in avanti travolgendo i nemici sul percorso", AB_CHARGE, 11.f, 0, 0 } },
    /* ladro: hp10 speed4.6 cd0.32 range1.35 arc1.15 dmg[2,4] crit0.28 */
    { "ladro", "Ladro", "Veloce e scaltro, colpi rapidi e critici.",
      10, 0, 0, 4.6f, 0.32f, 1.35f, 1.15f, 2, 4, 0, 0.28f,
      0.f, C_HEX(d8,e6,ff), C_HEX(d8,e6,ff), C_HEX(d8,e6,ff), 0xFFFFFFFFu,
      AS_DOUBLE, 0.15f,
      { "Passo Furtivo", "Balza sul nemico piu' vicino con un colpo critico garantito", AB_SHADOWSTEP, 9.f, 0, 0 } },
    /* mago: hp9 mp14 cost2 speed3.2 cd0.75 range8 dmg[4,8] ranged projSpeed11 #8fd1ff */
    { "mago", "Mago", "Fragile, scaglia energia a distanza consumando mana.",
      9, 14, 2, 3.2f, 0.75f, 8.f, 0.f, 4, 8, 1, 0.f,
      11.f, C_HEX(8f,d1,ff), 0, C_HEX(8f,d1,ff), C_HEX(df,f6,ff),
      AS_CAST, 0.32f,
      { "Onda d'Urto", "Esplosione di energia arcana intorno a te", AB_SHOCKWAVE, 13.f, 6, 0 } },
    /* ranger: hp11 speed3.9 cd0.42 range9 dmg[2,5] ranged projSpeed15 #d8e6b0 */
    { "ranger", "Ranger", "Frecce veloci, buona mobilita'.",
      11, 0, 0, 3.9f, 0.42f, 9.f, 0.f, 2, 5, 1, 0.f,
      15.f, C_HEX(d8,e6,b0), 0, C_HEX(d8,e6,b0), C_HEX(f4,ff,e0),
      AS_BOW, 0.2f,
      { "Raffica", "Scaglia una raffica di frecce a ventaglio", AB_VOLLEY, 10.f, 0, 0 } },
    /* prof: hp22 speed3.6 cd0.55 range8.5 dmg[5,9] ranged projSpeed14 #7df9ff charge0.8 */
    { "prof", "Prof", "Scienziato con fucile al plasma: piu' salute e piu' danno.",
      22, 0, 0, 3.6f, 0.55f, 8.5f, 0.f, 5, 9, 1, 0.f,
      14.f, C_HEX(7d,f9,ff), 0, C_HEX(7d,f9,ff), C_HEX(e8,fe,ff),
      AS_PLASMA, 0.28f,
      { "Colpo Caricato", "Carica il fucile per 0.8s e spara un colpo devastante (~2x danno)", AB_CHARGEDSHOT, 10.f, 0, 0.8f } },
    /* paladino: hp18 speed3.3 cd0.6 range1.5 arc1.3 dmg[3,5] */
    { "paladino", "Paladino", "Tank sacro: tanto in salute, colpisce con la mazza dorata.",
      18, 0, 0, 3.3f, 0.6f, 1.5f, 1.3f, 3, 5, 0, 0.f,
      0.f, C_HEX(ff,d9,8a), C_HEX(ff,d9,8a), C_HEX(ff,d9,8a), C_HEX(ff,f3,c4),
      AS_OVERHEAD, 0.3f,
      { "Muro Sacro", "Scudo divino: -50% danno subito per 5s", AB_HOLYSHIELD, 14.f, 0, 0 } },
    /* negromante: hp10 mp14 cost2 speed3.2 cd0.7 range8 dmg[4,7] ranged projSpeed11 #8fe07b */
    { "negromante", "Negromante", "Scaglia scintille d'anima verde consumando mana.",
      10, 14, 2, 3.2f, 0.7f, 8.f, 0.f, 4, 7, 1, 0.f,
      11.f, C_HEX(8f,e0,7b), 0, C_HEX(8f,e0,7b), C_HEX(c9,f5,c0),
      AS_CAST, 0.32f,
      { "Drenaggio d'Anima", "Esplosione necromantica: danneggia i nemici vicini e ti cura", AB_SOULDRAIN, 13.f, 6, 0 } },
    /* bardo: hp12 speed4.0 cd0.35 range1.4 arc1.1 dmg[2,4] crit0.15 */
    { "bardo", "Bardo", "Rapier veloce e colpi critici, ispira i compagni.",
      12, 0, 0, 4.0f, 0.35f, 1.4f, 1.1f, 2, 4, 0, 0.15f,
      0.f, C_HEX(ff,cf,5c), C_HEX(ff,cf,5c), C_HEX(ff,cf,5c), C_HEX(ff,f0,c0),
      AS_STAB, 0.18f,
      { "Canto d'Ispirazione", "+40% danno per 8s", AB_SONG, 12.f, 0, 0 } },
    /* monaco: hp14 speed4.4 cd0.3 range1.3 arc1.25 dmg[3,5] crit0.2 */
    { "monaco", "Monaco", "Pugni fulminei: velocissimo e preciso.",
      14, 0, 0, 4.4f, 0.3f, 1.3f, 1.25f, 3, 5, 0, 0.2f,
      0.f, C_HEX(ff,ca,7a), C_HEX(ff,ca,7a), C_HEX(ff,ca,7a), C_HEX(ff,e8,c0),
      AS_SPIN, 0.14f,
      { "Onda di Chi", "Proietta un'onda di energia che perfora tutti i nemici sul percorso", AB_CHIWAVE, 8.f, 0, 0 } },
};

/* ---- MONSTER_TYPES (valori identici all'originale) ----
   wKind: 0 decrescente(start) | 1 costante(wLow) | 2 soglia(minDepth,wLow,wHigh) | 3 boss */
static const MonsterType MTAB[MONSTER_COUNT] = {
    { 'r', "ratto",          C_HEX(a9,85,5c),  7,  2, 2.6f, 5.5f, 1,3,   0,10,0,0,0,   0,0,0,0,0,0,0,0,0,0,0, 0.f },
    { 'b', "pipistrello",    C_HEX(9b,7f,b0),  8,  2, 2.9f, 6.5f, 1,3,   0, 9,0,0,0,   1,0,0,0,0,0,0,0,0,0,0, 0.f },
    { 'g', "goblin",         C_HEX(7f,ae,63), 14,  3, 2.3f, 6.0f, 2,5,   1, 8,0,0,0,   0,0,0,0,0,0,0,0,0,0,0, 0.f },
    { 'j', "melma",          C_HEX(5f,bf,8f),  9,  2, 1.4f, 4.5f, 1,2,   2, 0,2,6,2,   0,0,0,0,0,0,0,0,0,0,0, 0.f },
    { 'J', "gelatina",       C_HEX(37,a0,6a), 22,  3, 1.2f, 4.5f, 2,4,   2, 0,1,5,2,   0,1,0,0,0,0,0,0,0,0,0, 0.f },
    { 's', "scheletro",      C_HEX(cf,ca,bb), 20,  4, 1.9f, 6.0f, 3,6,   2, 0,2,9,2,   0,0,0,0,0,0,0,0,0,0,0, 0.f },
    { 'o', "orco",           C_HEX(c0,7a,3a), 32,  6, 2.0f, 6.5f, 4,9,   2, 0,1,8,3,   0,0,0,0,0,0,0,0,0,0,0, 0.f },
    { 'z', "zombie",         C_HEX(7d,8a,5b), 26,  5, 1.3f, 5.0f, 3,6,   2, 0,1,7,3,   0,0,0,0,0,0,0,0,0,0,0, 0.f },
    { 'S', "ragno gigante",  C_HEX(8a,3a,5c), 24,  5, 3.0f, 7.5f, 4,8,   2, 0,0,7,4,   0,0,1,0,0,0,0,0,0,0,0, 0.f },
    { 'W', "spettro",        C_HEX(7f,b0,c9), 28,  6, 2.7f, 8.0f, 5,10,  2, 0,0,6,5,   0,0,0,1,0,0,0,0,0,0,0, 0.f },
    { 'D', "drago minore",   C_HEX(e8,a1,3d),120, 11, 2.3f,11.0f,40,70,  3, 0,0,0,0,   0,0,0,0,0,0,0,0,0,1,0, 0.f },
    { 'k', "serpente",       C_HEX(7f,ae,63), 10,  2, 2.5f, 5.5f, 1,3,   1, 7,0,0,0,   0,0,1,0,1,0,0,0,0,0,0, 0.f },
    { 'h', "arpia",          C_HEX(b0,c8,e8),  9,  2, 2.9f, 6.5f, 1,3,   0,11,0,0,0,   1,0,0,0,0,1,0,0,0,0,0, 0.f },
    { 'C', "cavaliere caduto",C_HEX(c8,c2,b4),26,  5, 1.8f, 6.0f, 3,6,   2, 0,1,8,2,   0,0,0,0,0,0,1,0,0,0,0, 0.f },
    { 'c', "cultista",       C_HEX(b0,7f,d1), 18,  4, 2.0f, 6.5f, 3,6,   2, 0,1,6,3,   0,0,0,0,0,0,0,0,0,0,1, 3.0f },
    { 'm', "mantide abissale",C_HEX(c9,6f,4a),22,  4, 3.2f, 7.5f, 4,8,   2, 0,0,6,4,   0,0,0,0,0,0,0,1,0,0,0, 0.f },
    { 'q', "sciamano goblin",C_HEX(7f,ae,63), 20,  4, 1.9f, 7.0f, 3,7,   2, 0,0,5,4,   0,0,0,0,0,0,0,0,0,0,1, 2.6f },
    { 'G', "golem di roccia",C_HEX(8a,8f,98), 40,  7, 1.4f, 5.0f, 6,12,  2, 0,0,5,4,   0,0,0,0,0,0,0,0,1,0,0, 0.f },
    { 'X', "Golem di Pietra",        C_HEX(c9,b2,8a),150, 12, 1.6f,12.0f,220,300, 3,0,0,0,0, 0,0,0,0,1,0,0,0,1,1,0, 0.f },
    { 'L', "Lich Signore dei Nonmorti",C_HEX(8a,6c,ff),135, 11, 2.1f,12.0f,200,280,3,0,0,0,0, 0,0,0,0,0,0,0,0,0,1,0, 0.f },
    { 'M', "Regina delle Melme",     C_HEX(7d,ff,9a),130, 10, 1.5f,10.0f,190,260,3,0,0,0,0, 0,0,0,0,0,0,0,0,0,1,0, 0.f },
    { 'R', "Re Ragno",               C_HEX(c0,50,3a),140, 12, 2.6f,12.0f,210,290,3,0,0,0,0, 0,0,1,0,0,0,0,0,0,1,0, 0.f },
    { 'K', "Re dei ratti",           C_HEX(c9,a8,64),120, 11, 2.9f,12.0f,200,280,3,0,0,0,0, 0,0,0,0,0,0,0,0,0,1,0, 0.f },
};

const MonsterType* monType(int type){ return &MTAB[type]; }

int monTypeByChar(char c){
    int i;
    for (i=0;i<MONSTER_COUNT;++i) if (MTAB[i].ch==c) return i;
    return MT_RATTO;
}

int monWeight(int type, int depth){
    const MonsterType* t = &MTAB[type];
    switch (t->wKind){
        case 0: { int w = t->wStart - depth; return w>0?w:0; }
        case 1: return t->wLow;
        case 2: return depth>=t->wMinDepth ? t->wHigh : t->wLow;
        default: return 0;
    }
}

/* ---- Affissi ---- */
const AffixDef AFFIXES[AFFIX_COUNT] = {
    { "Veloce",      C_HEX(ff,e0,66), 1.6f, 0.f,   0.f    },
    { "Esplosivo",   C_HEX(ff,6b,4a), 1.f,  1.8f,  0.f    },
    { "Rigenerante", C_HEX(7f,ae,63), 1.f,  0.f,   0.045f },
};
float pickAffixChance(int depth){
    float c = 0.1f + depth*0.018f;           /* Math.min(0.4, ...) */
    return c>0.4f?0.4f:c;
}

/* ---- Potenziamenti ---- */
const PowerDef POWERS[PW_COUNT] = {
    { "Furia",          "+40% danno",        C_HEX(e0,5b,4f), 12.f },
    { "Scudo",          "-50% danno subito", C_HEX(5f,a0,c9), 10.f },
    { "Fretta",         "+40% velocita'",    C_HEX(d4,af,37), 12.f },
    { "Concentrazione", "-50% recupero attacco", C_HEX(8f,d1,ff), 10.f },
};

/* ---- Equipaggiamento ---- */
const EquipSlotInfo EQUIP_SLOTS[EQ_SLOT_COUNT] = {
    { "Elmo" }, { "Collana" }, { "Armatura" }, { "Anello" }, { "Gambali" }
};
const RarityTier RARITIES[RARITY_COUNT] = {
    { "comune",      "Comune",      C_HEX(b8,b8,b8), 100, 1.0f, 1 },
    { "raro",        "Raro",        C_HEX(5f,a0,c9),  32, 1.35f, 1 },
    { "epico",       "Epico",       C_HEX(b0,6b,f2),   9, 1.6f,  2 },
    { "leggendario", "Leggendario", C_HEX(ff,9d,2e),   2, 2.2f,  3 },
};

const char* statFormat(int statKey, int value){
    static char buf[32];
    switch (statKey){
        case ST_HP:        snprintf(buf,sizeof buf,"+%d salute",value); break;
        case ST_DMG_PCT:   snprintf(buf,sizeof buf,"+%d%% danno",value); break;
        case ST_SPEED_PCT: snprintf(buf,sizeof buf,"+%d%% velocita'",value); break;
        default:           snprintf(buf,sizeof buf,"-%d%% danno subito",value); break;
    }
    return buf;
}
int statCalc(int statKey, int depth){
    switch (statKey){
        case ST_HP:        return 4 + (int)(depth*1.6f);
        case ST_DMG_PCT:   return 5 + (int)(depth*1.1f);
        case ST_SPEED_PCT: return 3 + (int)(depth*0.5f);
        default:           return 4 + (int)(depth*0.9f);
    }
}

EquipItem makeEquipItem(int depth, Rng* rng){
    /* pickRarity: il peso delle rarita' alte cresce con la profondita' */
    float weights[RARITY_COUNT]; float total=0.f, roll; int i;
    EquipItem e;
    for (i=0;i<RARITY_COUNT;++i){
        weights[i] = (i==RAR_COMUNE) ? (float)RARITIES[i].weight
                                     : RARITIES[i].weight*(1.f+depth*0.045f);
        total += weights[i];
    }
    roll = rng_next(rng)*total;
    for (i=0;i<RARITY_COUNT;++i){ roll -= weights[i]; if (roll<=0) break; }
    if (i>RARITY_COUNT-1) i=RARITY_COUNT-1;
    e.slot = (int)(rng_next(rng)*EQ_SLOT_COUNT) % EQ_SLOT_COUNT;
    e.rarity = i;
    e.statCount = RARITIES[i].statCount;
    {
        const RarityTier* tier=&RARITIES[i];
        int k;
        int chosen[ST_COUNT];
        for (k=0;k<ST_COUNT;++k) chosen[k]=0;
        for (k=0;k<e.statCount && k<ST_COUNT;++k){
            int idx;
            do { idx = (int)(rng_next(rng)*ST_COUNT) % ST_COUNT; } while (chosen[idx]);
            chosen[idx]=1;
            e.statKey[k]=idx;
            e.statVal[k]=(int)(statCalc(idx,depth)*tier->mult + 0.5f);
        }
    }
    return e;
}

EquipItem makePremiumEquip(int depth, Rng* rng){
    /* premio del forziere del boss: solo epico/leggendario (45% leggendario) */
    EquipItem e; const RarityTier* tier; int k, chosen[ST_COUNT];
    e.slot = (int)(rng_next(rng)*EQ_SLOT_COUNT) % EQ_SLOT_COUNT;
    e.rarity = rng_next(rng)<0.45f ? RAR_LEGGEN : RAR_EPICO;
    tier = &RARITIES[e.rarity];
    e.statCount = tier->statCount;
    for (k=0;k<ST_COUNT;++k) chosen[k]=0;
    for (k=0;k<e.statCount && k<ST_COUNT;++k){
        int idx;
        do { idx = (int)(rng_next(rng)*ST_COUNT) % ST_COUNT; } while (chosen[idx]);
        chosen[idx]=1;
        e.statKey[k]=idx;
        e.statVal[k]=(int)(statCalc(idx,depth)*tier->mult + 0.5f);
    }
    return e;
}

int itemPower(const EquipItem* e){
    int i,sum=0;
    for (i=0;i<e->statCount;++i) sum += e->statVal[i];
    return sum;
}

/* ---- Boss ---- */
const struct BossBase BOSS = {
    0.5f,   /* wind */
    0.55f,  /* breathWind */
    0.95f,  /* breathBurn */
    3.2f,   /* breathDist */
    0.70710678f, /* breathArcCos cos(pi/4) */
    10,     /* breathDmg */
    4.2f,   /* breathCd */
    0.62f,  /* fbWind */
    3.6f,   /* fbSpeed */
    1.4f,   /* fbR */
    13,     /* fbDmg */
    6.4f,   /* fbCd */
    8.5f, 11.f, 5.0f, 1.5f, 1.3f, 2.0f, 17, 1.25f
};

void bossVariantFor(int monsterType, BossVariant* out){
    BossVariant v;
    v.noFly=0; v.noDive=0; v.stomp=0; v.summon=0; v.charge=0; v.web=0; v.poisonHit=0; v.spit=0;
    v.stompR=2.2f; v.breathCd=BOSS.breathCd; v.fbSpeed=BOSS.fbSpeed; v.fbR=BOSS.fbR;
    v.fbDmg=(int)BOSS.fbDmg; v.fbWind=BOSS.fbWind; v.breathDist=BOSS.breathDist;
    v.summonType=-1; v.summonN=2;
    v.fbColor=C_HEX(ff,8a,3d); v.breathColor=C_HEX(ff,7a,2d); v.aura=0;
    switch (monsterType){
        case MT_DRAGO: default:
            break;
        case MT_BOSS_GOLEM: /* X Golem di Pietra: pestone + clava */
            v.noFly=1; v.noDive=1; v.stomp=1; v.stompR=2.2f; v.breathCd=0;
            v.fbColor=C_HEX(c9,b2,8a); v.fbSpeed=2.8f; v.fbR=1.7f; v.fbDmg=14;
            v.aura=C_HEX(c9,b2,8a);
            break;
        case MT_BOSS_LICH: /* L: evoca scheletri */
            v.noFly=1; v.summon=1; v.summonType=MT_SCHELETRO; v.summonN=2; v.breathCd=0;
            v.fbColor=C_HEX(8a,6c,ff); v.fbSpeed=4.2f; v.fbWind=0.85f; v.fbR=1.7f; v.fbDmg=15;
            v.aura=C_HEX(8a,6c,ff);
            break;
        case MT_BOSS_MELME: /* M: scioglie e rigenera, sputa melme */
            v.noFly=1; v.summon=1; v.summonType=MT_MELMA; v.summonN=2; v.breathCd=0; v.spit=1;
            v.fbColor=C_HEX(7d,ff,9a); v.fbSpeed=2.6f; v.fbR=1.6f; v.fbDmg=10;
            v.aura=C_HEX(7d,ff,9a);
            break;
        case MT_BOSS_RAGNO: /* R: tela velenosa */
            v.noFly=1; v.noDive=1; v.web=1; v.fbColor=C_HEX(e0,d8,cc); v.fbSpeed=3.2f;
            v.fbR=2.0f; v.fbDmg=11; v.poisonHit=1; v.breathCd=0;
            v.aura=C_HEX(c0,50,3a);
            break;
        case MT_BOSS_RATTI: /* K: carica e schiera ratti */
            v.noFly=1; v.noDive=1; v.summon=1; v.summonType=MT_RATTO; v.summonN=3;
            v.charge=1; v.fbColor=C_HEX(c9,a8,64); v.fbSpeed=3.0f; v.fbR=1.5f; v.fbDmg=9;
            v.breathColor=C_HEX(c9,a8,64); v.breathCd=5.2f; v.breathDist=2.6f;
            v.aura=C_HEX(c9,a8,64);
            break;
    }
    *out = v;
}

int isBossFloor(int depth){ return depth>=5 && depth%BOSS_FLOOR_STRIDE==0; }
int bossTypeForDepth(int depth){
    static const int bt[6]={MT_DRAGO,MT_BOSS_GOLEM,MT_BOSS_LICH,MT_BOSS_MELME,MT_BOSS_RAGNO,MT_BOSS_RATTI};
    int idx=((depth/BOSS_FLOOR_STRIDE)-1)%6;
    if (idx<0) idx+=6;
    return bt[idx];
}
const char* currentBossName(int depth){ return monType(bossTypeForDepth(depth))->name; }

/* ---- Nomi ---- */
const char* const FIRST_NAMES[25] = {
    "Aldo","Bruno","Cesare","Dario","Elio","Fosco","Gino","Ivo","Luca","Marco",
    "Nino","Orso","Piero","Renzo","Sandro","Tullio","Ugo","Vito","Wanda","Ilaria",
    "Lucia","Nadia","Rosa","Silvia","Vera"
};

const char* randomNickname(void){
    static char nick[24];
    snprintf(nick,sizeof nick,"%s%d", FIRST_NAMES[(int)(frand()*25)%25], (int)(100+frand()*899));
    return nick;
}
