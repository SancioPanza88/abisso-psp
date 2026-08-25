#include "dungeon.h"
#include "data.h"

/* =====================================================================
   generateDepth: traduzione 1:1 della generazione originale.
   Stesse formule: w = clamp(78+d*3,78,130), h = clamp(44+d*2,44,74),
   maxRooms = 11+floor(d*0.7), corridoi orizzontale/verticale a caso,
   porte = pavimento stretto fra due muri opposti, BFS per le scale,
   tana-arena del boss su ogni multiplo di 5.
   ===================================================================== */

static Layout s_layout;
unsigned int g_worldSeed = 1;   /* seed di mondo (fa da ROOM_CODE su PSP) */

static void carveCorridor(unsigned char* grid,int w,int h,Rng* rng,
                          float ax,float ay,float bx,float by){
    /* a/b sono centri cella (interi +0.0): come l'originale lavora sugli interi */
    int x=(int)ax, y=(int)ay; int tx=(int)bx, ty=(int)by;
    int horizFirst = rng_next(rng)<0.5f;
    if (horizFirst){
        while (x!=tx){ grid[y*w+x]=T_FLOOR; x += x<tx?1:-1; }
        while (y!=ty){ grid[y*w+x]=T_FLOOR; y += y<ty?1:-1; }
    } else {
        while (y!=ty){ grid[y*w+x]=T_FLOOR; y += y<ty?1:-1; }
        while (x!=tx){ grid[y*w+x]=T_FLOOR; x += x<tx?1:-1; }
    }
    grid[ty*w+tx]=T_FLOOR;
}

static void centerOf(const Room* r, int* cx, int* cy){
    *cx = r->x + r->w/2;
    *cy = r->y + r->h/2;
}

static void bfsDistances(const unsigned char* grid,int w,int h,
                         int sx,int sy, short* dist){
    /* coda statica: la mappa e' al massimo 130x74 */
    static short qx[130*74], qy[130*74];
    int head=0, tail=0, i;
    for (i=0;i<w*h;++i) dist[i]=-1;
    if (grid[sy*w+sx]==T_WALL) return;
    qx[tail]=sx; qy[tail]=sy; ++tail;
    dist[sy*w+sx]=0;
    while (head<tail){
        int cx=qx[head], cy=qy[head]; ++head;
        int d = dist[cy*w+cx];
        const int nb[4][2]={{cx+1,cy},{cx-1,cy},{cx,cy+1},{cx,cy-1}};
        for (i=0;i<4;++i){
            int nx=nb[i][0], ny=nb[i][1];
            if (nx<0||ny<0||nx>=w||ny>=h) continue;
            if (grid[ny*w+nx]==T_WALL) continue;
            if (dist[ny*w+nx]!=-1) continue;
            dist[ny*w+nx]=(short)(d+1);
            qx[tail]=nx; qy[tail]=ny; ++tail;
        }
    }
}

Layout* generateDepth(int depth){
    Layout* L=&s_layout;
    Rng rng; Rng dynRngUnused;
    unsigned char *grid, *oldGrid=s_layout.grid, *oldDoors=s_layout.doors;
    Room* rooms;
    int maxRooms, attempts=0, i, x, y;
    int spawnX,spawnY, stairsX=0,stairsY=0;
    int merchantX, merchantY;
    int numChests, chestTries=0;
    int numTreasure, tTries=0;
    int numPotionSpots, poTries=0;
    static short distMap[130*74];

    (void)dynRngUnused;

    /* reset completo dello stato del piano (porte/forzieri/cancelli inclusi) */
    memset(&s_layout,0,sizeof s_layout);
    free(oldGrid); free(oldDoors);
    L->depth = depth;
    L->w = (int)(78 + depth*3.0f); if (L->w<78) L->w=78; if (L->w>130) L->w=130;
    L->h = (int)(44 + depth*2.0f); if (L->h<44) L->h=44; if (L->h>74) L->h=74;
    grid = (unsigned char*)calloc(L->w*L->h,1);
    L->doors = (unsigned char*)calloc(L->w*L->h,1);
    L->grid = grid;
    rooms = L->rooms;

    /* seed deterministico: hashStr(ROOM_CODE+'::'+seed+'::layout::'+depth).
       Su PSP non c'e' stanza condivisa: il seed di mondo fa da ROOM_CODE. */
    {
        char seedStr[64];
        snprintf(seedStr,sizeof seedStr,"%u::%u::layout::%d",g_worldSeed,g_worldSeed,depth);
        rng_seed(&rng, hashStr(seedStr));
    }

    maxRooms = 11 + (int)(depth*0.7f);
    while (L->roomCount<maxRooms && attempts<500){
        int rw = 4 + (int)(rng_next(&rng)*7);
        int rh = 3 + (int)(rng_next(&rng)*5);
        int rx = 1 + (int)(rng_next(&rng)*(L->w-rw-2));
        int ry = 1 + (int)(rng_next(&rng)*(L->h-rh-2));
        int ok=1, ri;
        for (ri=0; ri<L->roomCount; ++ri){
            const Room* r=&rooms[ri];
            if (rx-1 < r->x+r->w+1 && rx+rw+1 > r->x-1 &&
                ry-1 < r->y+r->h+1 && ry+rh+1 > r->y-1){ ok=0; break; }
        }
        if (!ok) continue;
        { Room* nr=&rooms[L->roomCount++];
          nr->x=rx; nr->y=ry; nr->w=rw; nr->h=rh; nr->arena=0; }
        for (y=ry;y<ry+rh;++y)
            for (x=rx;x<rx+rw;++x)
                grid[y*L->w+x]=T_FLOOR;
    }
    if (L->roomCount==0){
        Room* nr=&rooms[0];
        nr->x=(L->w>>1)-4; nr->y=(L->h>>1)-3; nr->w=8; nr->h=6; nr->arena=0;
        L->roomCount=1;
        for (y=nr->y;y<nr->y+nr->h;++y)
            for (x=nr->x;x<nr->x+nr->w;++x)
                grid[y*L->w+x]=T_FLOOR;
    }
    for (i=1;i<L->roomCount;++i){
        int ax,ay,bx,by;
        centerOf(&rooms[i-1],&ax,&ay); centerOf(&rooms[i],&bx,&by);
        carveCorridor(grid,L->w,L->h,&rng,(float)ax,(float)ay,(float)bx,(float)by);
    }
    {   /* extraLoops = floor(roomCount*0.3) */
        int extraLoops=(int)(L->roomCount*0.3f), k;
        for (k=0;k<extraLoops;++k){
            int ai=(int)(rng_next(&rng)*L->roomCount)%L->roomCount;
            int bi=(int)(rng_next(&rng)*L->roomCount)%L->roomCount;
            int ax,ay,bx,by;
            if (ai!=bi){
                centerOf(&rooms[ai],&ax,&ay); centerOf(&rooms[bi],&bx,&by);
                carveCorridor(grid,L->w,L->h,&rng,(float)ax,(float)ay,(float)bx,(float)by);
            }
        }
    }

    /* ---- Tana del boss (piani multipli di 5) ---- */
    if (depth>=BOSS_FLOOR_STRIDE && (depth%BOSS_FLOOR_STRIDE)==0){
        int tries;
        for (tries=0;tries<100;++tries){
            int bw = 12+(int)(rng_next(&rng)*6), bh = 8+(int)(rng_next(&rng)*5);
            int bx = 3+(int)(rng_next(&rng)*(L->w-bw-6)), by = 3+(int)(rng_next(&rng)*(L->h-bh-6));
            int ok=1, ri, yy, xx;
            int yy2, xx2;
            for (ri=0; ri<L->roomCount; ++ri){
                const Room* r=&rooms[ri];
                if (bx-2 < r->x+r->w+2 && bx+bw+2 > r->x-2 &&
                    by-2 < r->y+r->h+2 && by+bh+2 > r->y-2){ ok=0; break; }
            }
            if (!ok) continue;
            /* interno vuoto */
            for (yy=by; yy<by+bh && ok; ++yy)
                for (xx=bx; xx<bx+bw; ++xx)
                    if (grid[yy*L->w+xx]!=T_WALL){ ok=0; break; }
            if (!ok) continue;
            /* anello esterno vuoto */
            for (yy=by-1; yy<=by+bh && ok; ++yy)
                for (xx=bx-1; xx<=bx+bw; ++xx){
                    int inside = (xx>=bx && xx<bx+bw && yy>=by && yy<by+bh);
                    if (xx<0||yy<0||xx>=L->w||yy>=L->h) continue;
                    if (!inside && grid[yy*L->w+xx]!=T_WALL){ ok=0; break; }
                }
            if (!ok) continue;
            for (yy=by; yy<by+bh; ++yy)
                for (xx=bx; xx<bx+bw; ++xx)
                    grid[yy*L->w+xx]=T_FLOOR;
            L->hasBossRoom=1;
            L->bossRoom.x=bx; L->bossRoom.y=by; L->bossRoom.w=bw; L->bossRoom.h=bh; L->bossRoom.arena=1;
            L->bossType=bossTypeForDepth(depth);
            {   /* corridoio d'ingresso verso la stanza piu' vicina */
                int nearIdx=0; long nearD=0x7FFFFFFF;
                int bcx=bx+bw/2, bcy=by+bh/2, ri2;
                for (ri2=0; ri2<L->roomCount; ++ri2){
                    int cx,cy; long d;
                    centerOf(&rooms[ri2],&cx,&cy);
                    d=(long)(cx>bcx?cx-bcx:bcx-cx)+(cy>bcy?cy-bcy:bcy-cy);
                    if (d<nearD){ nearD=d; nearIdx=ri2; }
                }
                {   int cx,cy; centerOf(&rooms[nearIdx],&cx,&cy);
                    carveCorridor(grid,L->w,L->h,&rng,(float)bcx,(float)bcy,(float)cx,(float)cy); }
            }
            {   /* cancelli: celle di pavimento adiacenti al bordo della tana */
                L->gateCount=0;
                for (yy2=by-1; yy2<=by+bh && L->gateCount<64; ++yy2)
                    for (xx2=bx-1; xx2<=bx+bw && L->gateCount<64; ++xx2){
                        int inside = (xx2>=bx && xx2<bx+bw && yy2>=by && yy2<by+bh);
                        if (inside || xx2<0 || yy2<0 || xx2>=L->w || yy2>=L->h) continue;
                        if (grid[yy2*L->w+xx2]==T_FLOOR){
                            L->gates[L->gateCount].x=xx2;
                            L->gates[L->gateCount].y=yy2;
                            ++L->gateCount;
                        }
                    }
            }
            {   /* forziere del drago contro una parete della tana */
                int fx = bx+1 + (int)(rng_next(&rng)*(bw-2>0?bw-2:1));
                int py = by+1;
                if (by+4 >= by+bh/2) py = by+bh-2;
                L->bossChestX=fx; L->bossChestY=py;
            }
            break;
        }
    }

    /* ---- Porte: pavimento stretto fra due muri opposti ---- */
    for (y=1;y<L->h-1;++y){
        for (x=1;x<L->w-1;++x){
            int N,S,E,W_;
            if (grid[y*L->w+x]!=T_FLOOR) continue;
            N=grid[(y-1)*L->w+x]; S=grid[(y+1)*L->w+x];
            E=grid[y*L->w+x+1]; W_=grid[y*L->w+x-1];
            if ((N==T_WALL&&S==T_WALL&&E==T_FLOOR&&W_==T_FLOOR)||
                (E==T_WALL&&W_==T_WALL&&N==T_FLOOR&&S==T_FLOOR))
                L->doors[y*L->w+x]=1;
        }
    }

    /* ---- Spawn, zona sicura, mercante ---- */
    { int cx,cy; centerOf(&rooms[0],&cx,&cy); spawnX=cx; spawnY=cy; }
    L->safeX = rooms[0].x-0.5f; L->safeY = rooms[0].y-0.5f;
    L->safeW = rooms[0].w+1.f;  L->safeH = rooms[0].h+1.f;
    merchantX = rooms[0].x + (rooms[0].w-2>1?rooms[0].w-2:1);
    merchantY = rooms[0].y + rooms[0].h/2;
    if (merchantX==spawnX && merchantY==spawnY) merchantX = rooms[0].x+1;
    L->merchantX=merchantX; L->merchantY=merchantY;

    /* ---- Scale nella stanza raggiungibile piu' lontana ---- */
    bfsDistances(grid,L->w,L->h,spawnX,spawnY,distMap);
    {
        int bestDist=-1, ri;
        stairsX=spawnX; stairsY=spawnY;
        for (ri=0; ri<L->roomCount; ++ri){
            int cx,cy,d;
            if (rooms[ri].arena) continue;
            centerOf(&rooms[ri],&cx,&cy);
            d=distMap[cy*L->w+cx];
            if (d>bestDist){ bestDist=d; stairsX=cx; stairsY=cy; }
        }
    }
    grid[stairsY*L->w+stairsX]=T_STAIRS;
    L->stairsX=stairsX; L->stairsY=stairsY;

    /* ---- Forzieri ---- */
    if (L->hasBossRoom){
        L->chests[L->chestCount].x=L->bossChestX;
        L->chests[L->chestCount].y=L->bossChestY;
        L->chests[L->chestCount].hasBossChest=1;
        L->chestCount++;
    }
    numChests = 3 + depth/2;
    while (L->chestCount<numChests && chestTries<300){
        int ri = 1 + (int)(rng_next(&rng)*(L->roomCount-1));
        const Room* room;
        if (ri>=L->roomCount) ri=L->roomCount-1;
        room=&rooms[ri];
        ++chestTries;
        if (room->arena) continue;
        x = room->x+1 + (int)(rng_next(&rng)*(room->w-2>1?room->w-2:1));
        y = room->y+1 + (int)(rng_next(&rng)*(room->h-2>1?room->h-2:1));
        if (grid[y*L->w+x]!=T_FLOOR) continue;
        if (x==spawnX && y==spawnY) continue;
        if (x==merchantX && y==merchantY) continue;
        { int dup=0,ci; for (ci=0;ci<L->chestCount;++ci)
            if (L->chests[ci].x==x&&L->chests[ci].y==y){dup=1;break;}
          if (dup) continue; }
        L->chests[L->chestCount].x=x; L->chests[L->chestCount].y=y;
        L->chests[L->chestCount].hasBossChest=0;
        L->chestCount++;
    }

    /* ---- Spot di spawn dei mostri ---- */
    for (i=1;i<L->roomCount;++i){
        const Room* room=&rooms[i];
        int count = 1 + (int)(rng_next(&rng)*3), k;
        if (room->arena) continue;
        for (k=0;k<count;++k){
            x = room->x+1 + (int)(rng_next(&rng)*(room->w-2>1?room->w-2:1));
            y = room->y+1 + (int)(rng_next(&rng)*(room->h-2>1?room->h-2:1));
            if (grid[y*L->w+x]!=T_FLOOR) continue;
            if (L->monSpotCount<80){
                L->monSpots[L->monSpotCount].x=x; L->monSpots[L->monSpotCount].y=y;
                ++L->monSpotCount;
            }
        }
    }

    /* ---- Tesori sparsi ---- */
    numTreasure = 4 + depth/2;
    while (L->treasureCount<numTreasure && tTries<300){
        int ri=(int)(rng_next(&rng)*L->roomCount)%L->roomCount;
        const Room* room=&rooms[ri];
        ++tTries;
        if (room->arena) continue;
        x = room->x+1 + (int)(rng_next(&rng)*(room->w-2>1?room->w-2:1));
        y = room->y+1 + (int)(rng_next(&rng)*(room->h-2>1?room->h-2:1));
        if (grid[y*L->w+x]!=T_FLOOR) continue;
        if (x==spawnX && y==spawnY) continue;
        if (x==merchantX && y==merchantY) continue;
        { int dup=0,ci; for (ci=0;ci<L->chestCount;++ci)
            if (L->chests[ci].x==x&&L->chests[ci].y==y){dup=1;break;}
          if (dup) continue;
          for (ci=0;ci<L->treasureCount;++ci)
            if (L->treasure[ci].x==x&&L->treasure[ci].y==y){dup=1;break;}
          if (dup) continue; }
        L->treasure[L->treasureCount].x=x; L->treasure[L->treasureCount].y=y;
        L->treasure[L->treasureCount].gem = rng_next(&rng)<0.18f;
        ++L->treasureCount;
    }

    /* ---- Spot potenziamenti ---- */
    {
        int pTries=0;
        while (L->powerupCount<3 && pTries<200){
            int ri=1+(int)(rng_next(&rng)*(L->roomCount-1));
            const Room* room;
            if (ri>=L->roomCount) ri=L->roomCount-1;
            room=&rooms[ri];
            ++pTries;
            if (room->arena) continue;
            x = room->x+1 + (int)(rng_next(&rng)*(room->w-2>1?room->w-2:1));
            y = room->y+1 + (int)(rng_next(&rng)*(room->h-2>1?room->h-2:1));
            if (grid[y*L->w+x]!=T_FLOOR) continue;
            { int dup=0,ci; for (ci=0;ci<L->powerupCount;++ci)
                if (L->powerupSpots[ci].x==x&&L->powerupSpots[ci].y==y){dup=1;break;}
              if (dup) continue; }
            L->powerupSpots[L->powerupCount].x=x;
            L->powerupSpots[L->powerupCount].y=y;
            ++L->powerupCount;
        }
    }

    /* ---- Pozioni garantite ---- */
    numPotionSpots = 3 + depth/3;
    poTries=0;
    while (L->potionCount<numPotionSpots && poTries<250){
        int ri=(int)(rng_next(&rng)*L->roomCount)%L->roomCount;
        const Room* room=&rooms[ri];
        ++poTries;
        if (room->arena) continue;
        x = room->x+1 + (int)(rng_next(&rng)*(room->w-2>1?room->w-2:1));
        y = room->y+1 + (int)(rng_next(&rng)*(room->h-2>1?room->h-2:1));
        if (grid[y*L->w+x]!=T_FLOOR) continue;
        if (x==merchantX && y==merchantY) continue;
        { int dup=0,ci;
          for (ci=0;ci<L->potionCount;++ci)
            if (L->potionSpots[ci].x==x&&L->potionSpots[ci].y==y){dup=1;break;}
          if (dup) continue;
          for (ci=0;ci<L->chestCount;++ci)
            if (L->chests[ci].x==x&&L->chests[ci].y==y){dup=1;break;}
          if (dup) continue;
          for (ci=0;ci<L->treasureCount;++ci)
            if (L->treasure[ci].x==x&&L->treasure[ci].y==y){dup=1;break;}
          if (dup) continue; }
        L->potionSpots[L->potionCount].x=x;
        L->potionSpots[L->potionCount].y=y;
        L->potionSpots[L->potionCount].mana = rng_next(&rng)<0.45f;
        ++L->potionCount;
    }

    /* ---- Torce sui muri delle stanze ---- */
    for (i=0;i<L->roomCount;++i){
        const Room* room=&rooms[i];
        int n = (i==0)?3 : 1+(int)(rng_next(&rng)*3);
        int placed=0, tries=0;
        int x0=room->x-1, x1=room->x+room->w, y0=room->y-1, y1=room->y+room->h;
        if (room->arena) continue;
        while (placed<n && tries<60){
            int edge=(int)(rng_next(&rng)*4), tx, ty;
            ++tries;
            switch (edge){
                case 0: tx=x0; ty=y0+1+(int)(rng_next(&rng)*(room->h-2>1?room->h-2:1)); break;
                case 1: tx=x1; ty=y0+1+(int)(rng_next(&rng)*(room->h-2>1?room->h-2:1)); break;
                case 2: ty=y0; tx=x0+1+(int)(rng_next(&rng)*(room->w-2>1?room->w-2:1)); break;
                default:ty=y1; tx=x0+1+(int)(rng_next(&rng)*(room->w-2>1?room->w-2:1)); break;
            }
            if (tx<1||ty<1||tx>=L->w-1||ty>=L->h-1) continue;
            if (grid[ty*L->w+tx]!=T_WALL) continue;
            { int dup=0,ti; for (ti=0;ti<L->torchCount;++ti)
                if (L->torches[ti].x==tx&&L->torches[ti].y==ty){dup=1;break;}
              if (dup) continue; }
            if (L->torchCount<96){
                L->torches[L->torchCount].x=tx; L->torches[L->torchCount].y=ty;
                ++L->torchCount;
            }
            ++placed;
        }
    }

    L->spawnX=(float)spawnX; L->spawnY=(float)spawnY;
    return L;
}

void layoutFree(void){
    if (s_layout.grid){ free(s_layout.grid); s_layout.grid=NULL; }
    if (s_layout.doors){ free(s_layout.doors); s_layout.doors=NULL; }
}

/* ---- Movimento generico (canOccupy / tryMoveEntity) 1:1 ---- */
int isWalkableTile(const Layout* L, int tx, int ty){
    if (tx<0||ty<0||tx>=L->w||ty>=L->h) return 0;
    return L->grid[ty*L->w+tx]!=T_WALL;
}
int insideZone(float zx,float zy,float zw,float zh,float x,float y){
    return (x>=zx && x<zx+zw && y>=zy && y<zy+zh);
}
static int insideExcl(const float* excl, float x, float y){
    if (!excl) return 0;
    return insideZone(excl[0],excl[1],excl[2],excl[3],x,y);
}
int canOccupy(const Layout* L, float x, float y, float r, const float* excl){
    const float pts[5][2]={{x-r,y-r},{x+r,y-r},{x-r,y+r},{x+r,y+r},{x,y}};
    int i;
    for (i=0;i<5;++i)
        if (!isWalkableTile(L,(int)pts[i][0],(int)pts[i][1])) return 0;
    if (insideExcl(excl,x,y)) return 0;
    return 1;
}
void tryMoveEntity(const Layout* L, float* x, float* y, float speed,
                   float dx, float dy, float dt, float radius, const float* excl){
    float step=speed*dt;
    float nx=*x+dx*step, ny=*y+dy*step;
    int movedX, movedY;
    movedX = canOccupy(L,nx,*y,radius,excl);
    if (movedX) *x=nx;
    movedY = canOccupy(L,*x,ny,radius,excl);
    if (movedY) *y=ny;
    /* assistenza d'ingresso nei corridoi (identica all'originale) */
    if (dx!=0.f && dy!=0.f){
        if (!movedX){
            float targetY=(float)((int)(*y))+0.5f;
            float diff=targetY-*y;
            float nudged=*y+signf(diff)*(fabsf(diff)<step*1.6f?fabsf(diff):step*1.6f);
            if (canOccupy(L,*x,nudged,radius,excl)) *y=nudged;
        }
        if (!movedY){
            float targetX=(float)((int)(*x))+0.5f;
            float diff=targetX-*x;
            float nudged=*x+signf(diff)*(fabsf(diff)<step*1.6f?fabsf(diff):step*1.6f);
            if (canOccupy(L,nudged,*y,radius,excl)) *x=nudged;
        }
    }
}

/* ---- Auto-tile dei muri (computeWallMask) ---- */
static int isFloorish(const Layout* L, int x, int y){
    if (x<0||y<0||x>=L->w||y>=L->h) return 0;
    return L->grid[y*L->w+x]!=T_WALL;
}
int isWallSkin(const Layout* L, int x, int y){
    if (x<0||y<0||x>=L->w||y>=L->h) return 0;
    if (L->grid[y*L->w+x]!=T_WALL) return 0;
    return isFloorish(L,x-1,y)||isFloorish(L,x+1,y)||isFloorish(L,x,y-1)||isFloorish(L,x,y+1);
}
int computeWallMask(const Layout* L, int x, int y){
    int mask=0;
    if (isWallSkin(L,x,y-1)) mask|=1;
    if (isWallSkin(L,x+1,y)) mask|=2;
    if (isWallSkin(L,x,y+1)) mask|=4;
    if (isWallSkin(L,x-1,y)) mask|=8;
    return mask;
}
