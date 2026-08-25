#ifndef ABISSO_DUNGEON_H
#define ABISSO_DUNGEON_H

#include "common.h"

/* T_WALL / T_FLOOR / T_STAIRS dell'originale */
#define T_WALL   0
#define T_FLOOR  1
#define T_STAIRS 2

typedef struct { int x,y,w,h,arena; } Room;

typedef struct {
    int x, y;
    int hasBossChest;      /* il forziere del drago */
} ChestSpot;

typedef struct {
    int depth;
    int w, h;
    unsigned char* grid;        /* w*h */
    unsigned char* doors;       /* w*h: celle porta */
    Room rooms[40]; int roomCount;
    float spawnX, spawnY;
    int stairsX, stairsY;
    ChestSpot chests[64]; int chestCount;
    int opened[64];             /* world.chestsOpened */
    struct { int x,y; } monSpots[80]; int monSpotCount;
    struct { int x,y; int gem; } treasure[64]; int treasureCount;
    struct { int x,y; } powerupSpots[8]; int powerupCount;
    struct { int x,y; int mana; } potionSpots[24]; int potionCount;
    /* safeZone = stanza d'ingresso allargata (x-0.5 .. w+0.5) */
    float safeX, safeY, safeW, safeH;
    int merchantX, merchantY;
    struct { int x,y; } torches[96]; int torchCount;
    Room bossRoom; int hasBossRoom;
    int bossType;
    int bossChestX, bossChestY;  /* id 'cboss<depth>' */
    struct { int x,y; } gates[64]; int gateCount;
} Layout;

Layout* generateDepth(int depth);          /* genera e conserva in un buffer interno */
void layoutFree(void);

int  isWalkableTile(const Layout* L, int tx, int ty);
int  insideZone(float zx,float zy,float zw,float zh,float x,float y);
int  canOccupy(const Layout* L, float x, float y, float r, const float* excl);
void tryMoveEntity(const Layout* L, float* x, float* y, float speed,
                   float dx, float dy, float dt, float radius, const float* excl);
int  isWallSkin(const Layout* L, int x, int y);
int  computeWallMask(const Layout* L, int x, int y);

#endif
