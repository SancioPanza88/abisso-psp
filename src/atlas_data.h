/* Generato da tools/build_atlas.ps1 - atlas degli asset originali di Abisso 2.0 */
#ifndef ABISSO_ATLAS_DATA_H
#define ABISSO_ATLAS_DATA_H

#define ATLAS_W 1024
#define ATLAS_H 512

typedef struct { unsigned short u,v,w,h; } ARect;

enum {
    AR_FLOOR_DIRT = 0,
    AR_FLOOR_STONE = 1,
    AR_WALL_BRICK = 2,
    AR_WALL_STONE = 3,
    AR_STAIRS = 4,
    AR_TORCH = 5,
    AR_MERCHANT = 6,
    AR_CHEST_CLOSED = 7,
    AR_CHEST_OPEN = 8,
    AR_HERO_GUERRIERO = 9,
    AR_HERO_LADRO = 10,
    AR_HERO_MAGO = 11,
    AR_HERO_RANGER = 12,
    AR_HERO_PROF = 13,
    AR_HERO_PALADINO = 14,
    AR_HERO_NEGROMANTE = 15,
    AR_HERO_BARDO = 16,
    AR_HERO_MONACO = 17,
    AR_MON_RATTO = 18,
    AR_MON_PIPISTRELLO = 19,
    AR_MON_GOBLIN = 20,
    AR_MON_SCHELETRO = 21,
    AR_MON_MELMA = 22,
    AR_MON_GELATINA = 23,
    AR_MON_ZOMBIE = 24,
    AR_MON_RAGNO = 25,
    AR_MON_SPETTRO = 26,
    AR_MON_DRAGO = 27,
    AR_MON_ORCO = 28,
    AR_MON_SERPENTE = 29,
    AR_MON_ARPIA = 30,
    AR_MON_CAVALIERE = 31,
    AR_MON_CAVALIERE_ALT = 32,
    AR_MON_CULTISTA = 33,
    AR_MON_GOLEM = 34,
    AR_MON_MANTIDE = 35,
    AR_MON_SCIAMANO = 36,
    AR_BOSS_GOLEM = 37,
    AR_BOSS_LICH = 38,
    AR_BOSS_MELME = 39,
    AR_BOSS_RAGNO = 40,
    AR_BOSS_RATTI = 41,
    AR_ICON_GOLD = 42,
    AR_ICON_GEM_BLUE = 43,
    AR_ICON_POTION_HP = 44,
    AR_ICON_POTION_MANA = 45,
    AR_PW_FURIA = 46,
    AR_PW_SHIELD = 47,
    AR_PW_HASTE = 48,
    AR_PW_FOCUS = 49,
    AR_ICON_LIGHTNING = 50,
    AR_EQUIP_HELM = 51,
    AR_EQUIP_NECKLACE = 52,
    AR_EQUIP_ARMOR = 53,
    AR_EQUIP_RING = 54,
    AR_EQUIP_GREAVES = 55,
    AR_GLOW = 56
};

extern const ARect g_atlasRects[57];
int atlasLoad(void);   /* decomprime atlas.rle nel buffer texture */
unsigned int* atlasPixels(void);

#endif
