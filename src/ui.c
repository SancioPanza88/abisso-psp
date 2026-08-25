#include "ui.h"
#include "game.h"
#include "gfx.h"
#include "data.h"
#include "audio.h"
#include "atlas_data.h"
#include <string.h>

#define C_HEX(rr,gg,bb) COL(0x##rr,0x##gg,0x##bb,255)
#define LOG_SHOWN 3

static int s_merchantOpen=0;
static int s_merchantSel=0;
static float s_prevHpShown=-1;

/* ------------------------------------------------------------------ */
/* Schermata titolo                                                    */
/* ------------------------------------------------------------------ */
void uiRenderTitle(float blinkT){
    const char* sub="Roguelike in tempo reale - port PSP";
    float w;
    w=gfxTextW("ABISSO",6);
    gfxText((SCR_W-w)/2,60,"ABISSO",C_HEX(E8,A1,3D),6);
    w=gfxTextW(sub,1);
    gfxText((SCR_W-w)/2,110,sub,C_HEX(9C,8E,77),1);
    if (fmodf(blinkT,1.f)<0.6f){
        w=gfxTextW("PREMI START",2);
        gfxText((SCR_W-w)/2,150,"PREMI START",C_HEX(E8,DC,C5),2);
    }
    {
        const char* l1="ANALOG: muovi   X: attacca   QUADRO: interagisci";
        const char* l2="CERCHIO: abilita'   TRIANGOLO: pozione   L: mana";
        const char* l3="SELECT: mappa   START: pausa";
        w=gfxTextW(l1,1); gfxText((SCR_W-w)/2,200,l1,C_HEX(9C,8E,77),1);
        w=gfxTextW(l2,1); gfxText((SCR_W-w)/2,214,l2,C_HEX(9C,8E,77),1);
        w=gfxTextW(l3,1); gfxText((SCR_W-w)/2,228,l3,C_HEX(9C,8E,77),1);
    }
}

/* ------------------------------------------------------------------ */
/* Selezione classe                                                    */
/* ------------------------------------------------------------------ */
void uiRenderClassSelect(int sel){
    char buf[160];
    float w;
    const ClassDef* c=&CLASSES[sel];
    w=gfxTextW("SCEGLI LA TUA CLASSE",2);
    gfxText((SCR_W-w)/2,18,"SCEGLI LA TUA CLASSE",C_HEX(E8,A1,3D),2);
    snprintf(buf,sizeof buf,"%d / %d",sel+1,CLASS_COUNT);
    w=gfxTextW(buf,1);
    gfxText(SCR_W-10-w,24,buf,C_HEX(9C,8E,77),1);
    /* ritratto grande */
    gfxDrawSprite(AR_HERO_GUERRIERO+sel,SCR_W/2,92,TILE_PX*3.2f,1,0,0);
    w=gfxTextW(c->name,2);
    gfxText((SCR_W-w)/2,132,c->name,C_HEX(E8,DC,C5),2);
    w=gfxTextW(c->desc,1);
    gfxText((SCR_W-w)/2,156,c->desc,C_HEX(9C,8E,77),1);
    snprintf(buf,sizeof buf,"HP %d  Velocita' %.1f  Danno %d-%d%s",
             c->hp,c->speed,c->dmgMin,c->dmgMax,
             c->ranged?"  a distanza":"  mischia");
    w=gfxTextW(buf,1);
    gfxText((SCR_W-w)/2,172,buf,C_HEX(9C,8E,77),1);
    /* abilita' */
    snprintf(buf,sizeof buf,"Abilita': %s (recupero %ds)",c->ability.name,(int)c->ability.cooldown);
    w=gfxTextW(buf,1);
    gfxText((SCR_W-w)/2,196,buf,C_HEX(7F,AE,63),1);
    if (strlen(c->ability.desc)<52){
        w=gfxTextW(c->ability.desc,1);
        gfxText((SCR_W-w)/2,210,c->ability.desc,C_HEX(9C,8E,77),1);
    }
    w=gfxTextW("X: scendi nell'abisso",1);
    gfxText((SCR_W-w)/2,240,"X: scendi nell'abisso",C_HEX(E8,A1,3D),1);
}

/* ------------------------------------------------------------------ */
/* HUD di gioco                                                        */
/* ------------------------------------------------------------------ */
static void drawBar(float x,float y,float w,float h,float pct,unsigned int fillCol){
    gfxQuad(x-1,y-1,w+2,h+2,COL(8,6,4,220));
    gfxQuad(x,y,w,h,COL(30,24,16,230));
    if (pct>0) gfxQuad(x,y,w*pct,h,fillCol);
}
void uiRenderGameHud(void){
    char buf[128];
    const ClassDef* c=&CLASSES[g_me.cls];
    float hpPct=g_me.maxHp>0?g_me.hp/g_me.maxHp:0;

    /* --- barra HP con flash --- */
    if (s_prevHpShown<0) s_prevHpShown=g_me.hp;
    {
        unsigned int col = C_HEX(C1,44,3A);
        if (g_me.hp<s_prevHpShown-0.01f) col=C_HEX(E0,5B,4F);
        else if (g_me.hp>s_prevHpShown+0.01f) col=C_HEX(7F,AE,63);
        drawBar(10,12,120,10,hpPct,col);
        s_prevHpShown=g_me.hp;
    }
    snprintf(buf,sizeof buf,"%d/%d",(int)fmaxf(0,g_me.hp),(int)g_me.maxHp);
    gfxText(136,13,buf,C_HEX(E8,DC,C5),1);
    /* mana */
    if (c->maxMp>0){
        drawBar(10,26,90,6,g_me.maxMp>0?g_me.mp/g_me.maxMp:0,C_HEX(5F,A0,C9));
        snprintf(buf,sizeof buf,"%d/%d",(int)g_me.mp,(int)g_me.maxMp);
        gfxText(104,25,buf,C_HEX(E8,DC,C5),1);
    }
    /* oro e pozioni */
    gfxDrawSprite(AR_ICON_GOLD,16,46,TILE_PX*0.5f,1,0,0);
    snprintf(buf,sizeof buf,"%d",g_me.gold);
    gfxText(26,42,buf,C_HEX(D4,AF,37),1);
    gfxDrawSprite(AR_ICON_POTION_HP,70,46,TILE_PX*0.5f,1,0,0);
    snprintf(buf,sizeof buf,"%d",g_me.potions);
    gfxText(80,42,buf,C_HEX(E8,DC,C5),1);
    if (c->maxMp>0){
        gfxDrawSprite(AR_ICON_POTION_MANA,108,46,TILE_PX*0.5f,1,0,0);
        snprintf(buf,sizeof buf,"%d",g_me.manaPotions);
        gfxText(118,42,buf,C_HEX(E8,DC,C5),1);
    }
    /* etichetta piano + nemici + boss */
    {
        Layout* L=g_world.layout;
        snprintf(buf,sizeof buf,"Piano %d · %d nemici",g_world.depth,monsterAliveCount());
        if (isBossFloor(g_world.depth)){
            if (findBossMonster() && !g_world.bossDead)
                snprintf(buf+strlen(buf),sizeof buf-strlen(buf),
                         " · Boss: %s",currentBossName(g_world.depth));
            else
                snprintf(buf+strlen(buf),sizeof buf-strlen(buf)," · Boss sconfitto!");
        }
        (void)L;
    }
    { float w=gfxTextW(buf,1); gfxText((SCR_W-w)/2,8,buf,C_HEX(E8,A1,3D),1); }

    /* buff attivi */
    {
        int k; float bx=10;
        for (k=0;k<PW_COUNT;++k){
            if (g_me.buffs[k]>0){
                snprintf(buf,sizeof buf,"%s %ds",POWERS[k].name,(int)ceilf(g_me.buffs[k]));
                gfxText(bx,58,buf,POWERS[k].color,1);
                bx+=gfxTextW(buf,1)+8;
            }
        }
    }

    /* box abilita' in basso a destra */
    {
        float x=SCR_W-64,y=SCR_H-40;
        gfxQuad(x,y,54,30,COL(8,6,4,200));
        snprintf(buf,sizeof buf,"%s",c->ability.name);
        if (gfxTextW(buf,1)>52){ buf[10]=0; }
        gfxText(x+3,y+3,buf,C_HEX(E8,DC,C5),1);
        if (g_me.abilityTimer>0)
            snprintf(buf,sizeof buf,"CD %ds",(int)ceilf(g_me.abilityTimer));
        else if (c->maxMp>0 && c->manaCost)
            snprintf(buf,sizeof buf,"costo %d mp",c->manaCost);
        else
            snprintf(buf,sizeof buf,"pronta");
        gfxText(x+3,y+15,buf,g_me.abilityTimer>0?C_HEX(9C,8E,77):C_HEX(7F,AE,63),1);
    }
    /* suggerimento comandi in basso a sinistra */
    gfxText(8,SCR_H-14,"X attacca  [] interagisce  O abilita  /\\ pozione",
            COL(156,142,119,180),1);

    /* barra del boss */
    {
        Monster* boss=findBossMonster();
        if (boss && g_world.bossActive && !g_world.bossDead){
            const MonsterType* t=monType(boss->type);
            float pct=boss->hp/boss->maxHp;
            snprintf(buf,sizeof buf,"%s",t->name);
            { float w=gfxTextW(buf,1); gfxText((SCR_W-w)/2,SCR_H-34,buf,t->color,1); }
            drawBar(SCR_W/2-90,SCR_H-24,180,8,pct,C_HEX(E0,5B,4F));
        }
    }

    /* minimappa (drawMinimap) */
    if (g_minimapVisible && g_world.layout){
        Layout* L=g_world.layout;
        const int MW=140,MH=96;
        const int MX=SCR_W-MW-6, MY=8;
        float scale=MW/(float)L->w < MH/(float)L->h ? MW/(float)L->w : MH/(float)L->h;
        float offX=(MW-L->w*scale)/2, offY=(MH-L->h*scale)/2;
        int x,y,i;
        gfxQuad(MX-2,MY-2,MW+4,MH+4,COL(6,5,3,224));
        for (y=0;y<L->h;++y)
            for (x=0;x<L->w;++x){
                unsigned int col;
                if (!g_world.visited[y*L->w+x]) continue;
                if (L->grid[y*L->w+x]==T_WALL) col=C_HEX(24,1C,12);
                else col=C_HEX(3A,2F,22);
                gfxQuad(MX+offX+x*scale,MY+offY+y*scale,
                        (scale*0.9f<1?1:scale*0.9f),(scale*0.9f<1?1:scale*0.9f),col|0xA0000000u);
            }
        /* scala */
        gfxQuad(MX+offX+L->stairsX*scale,MY+offY+L->stairsY*scale,scale+1,scale+1,C_HEX(E8,A1,3D));
        for (i=0;i<L->chestCount;++i)
            if (!L->opened[i])
                gfxQuad(MX+offX+L->chests[i].x*scale,MY+offY+L->chests[i].y*scale,scale,scale,C_HEX(FF,D2,7A));
        gfxQuad(MX+offX+L->merchantX*scale,MY+offY+L->merchantY*scale,scale+1,scale+1,C_HEX(7F,AE,63));
        for (i=0;i<MONSTER_CAP;++i){
            Monster* m=&g_world.monsters[i];
            if (m->type<0) continue;
            if (!g_world.visible[(int)m->ry*L->w+(int)m->rx]) continue;
            gfxQuad(MX+offX+m->rx*scale-1,MY+offY+m->ry*scale-1,3,3,C_HEX(FF,50,40));
        }
        /* faro della tana del boss */
        if (isBossFloor(g_world.depth) && L->hasBossRoom && !g_world.bossDead){
            Room* br=&L->bossRoom;
            float pulse=0.6f+0.4f*sinf(g_torchClock*3.6f);
            gfxQuad(MX+offX+br->x*scale,MY+offY+br->y*scale,br->w*scale,br->h*scale,
                    withAlpha(C_HEX(E8,50,28),0.14f));
            gfxGlow(MX+offX+(br->x+br->w/2)*scale,MY+offY+(br->y+br->h/2)*scale,
                    7*pulse+2,C_HEX(FF,78,32),0.9f*pulse);
        }
        /* giocatore */
        gfxGlow(MX+offX+g_me.x*scale,MY+offY+g_me.y*scale,
                2.2f+0.5f*sinf(g_torchClock*5),0xFFFFFF,1);
    }

    /* toast */
    if (currentToast()){
        float w=gfxTextW(currentToast(),1);
        gfxQuad((SCR_W-w)/2-8,64,w+16,16,COL(10,8,6,216));
        gfxText((SCR_W-w)/2,68,currentToast(),C_HEX(E8,DC,C5),1);
    }
    /* banner loot */
    if (bannerRemaining()>0){
        int iconLine;
        const char* txt=currentBanner(&iconLine);
        float w=gfxTextW(txt,1);
        unsigned int bc=bannerColor();
        gfxQuad((SCR_W-w)/2-10,88,w+20,20,COL(10,8,6,220));
        gfxQuad((SCR_W-w)/2-10,88,w+20,1,bc);
        gfxQuad((SCR_W-w)/2-10,107,w+20,1,bc);
        gfxText((SCR_W-w)/2,92,txt,bc,1);
    }
    /* log (ultime 3 righe) */
    {
        int k; float ly=SCR_H-56;
        for (k=LOG_SHOWN-1;k>=0;--k){
            const char* line=logLineAt(k);
            if (line){ gfxText(8,ly,line,withAlpha(logColorAt(k),k==0?0.95f:0.55f),1); ly+=10; }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Overlay: a terra / morto                                            */
/* ------------------------------------------------------------------ */
void uiRenderDownedOverlay(void){
    char buf[80];
    gfxQuad(0,SCR_H/2-30,SCR_W,60,COL(40,6,4,170));
    { float w=gfxTextW("SEI A TERRA!",2);
      gfxText((SCR_W-w)/2,SCR_H-30-46+8,"SEI A TERRA!",C_HEX(C1,44,3A),2); }
    snprintf(buf,sizeof buf,"Fine del conto tra %ds - nessuno puo' salvarti",
             (int)ceilf(g_me.downedTimer));
    { float w=gfxTextW(buf,1);
      gfxText((SCR_W-w)/2,SCR_H/2+12,buf,C_HEX(E8,DC,C5),1); }
}
void uiRenderDeadOverlay(void){
    char buf[96];
    float w;
    gfxQuad(0,0,SCR_W,SCR_H,COL(2,1,1,200));
    w=gfxTextW("SEI CADUTO NELL'OSCURITA'",2);
    gfxText((SCR_W-w)/2,90,"SEI CADUTO NELL'OSCURITA'",C_HEX(C1,44,3A),2);
    snprintf(buf,sizeof buf,"Permadeath: hai perso oro, pozioni ed equipaggiamento.");
    w=gfxTextW(buf,1);
    gfxText((SCR_W-w)/2,126,buf,C_HEX(9C,8E,77),1);
    snprintf(buf,sizeof buf,"Record in questo mondo: %d oro, piano %d.",
             g_recordGold,g_recordDepth);
    w=gfxTextW(buf,1);
    gfxText((SCR_W-w)/2,140,buf,C_HEX(D4,AF,37),1);
}

/* ------------------------------------------------------------------ */
/* Mercante                                                            */
/* ------------------------------------------------------------------ */
int uiMerchantActive(void){ return s_merchantOpen; }
void uiOpenMerchant(void){ s_merchantOpen=1; s_merchantSel=0; }
void uiCloseMerchant(void){ s_merchantOpen=0; }
void uiMerchantMove(int delta){
    s_merchantSel=(s_merchantSel+delta)%4;
    if (s_merchantSel<0) s_merchantSel+=4;
}
void uiMerchantBuy(void){
    static const int kinds[4]={IK_POTION,IK_MANAPOT,IK_POWER,IK_EQUIP};
    buyFromMerchant(kinds[s_merchantSel]);
}
void uiRenderMerchantPanel(void){
    static const struct { int kind; const char* name; int spr; } rows[4]={
        { IK_POTION,  "Pozione di salute",        AR_ICON_POTION_HP },
        { IK_MANAPOT, "Pozione di mana",          AR_ICON_POTION_MANA },
        { IK_POWER,   "Potenziamento a sorte",   AR_PW_FURIA },
        { IK_EQUIP,   "Equipaggiamento a sorte", AR_EQUIP_HELM }
    };
    const int PW_=260, PH_=120;
    const int PX_=(SCR_W-PW_)/2, PY_=(SCR_H-PH_)/2;
    int d=g_world.depth,i;
    char buf[96];
    float w;
    gfxQuad(PX_,PY_,PW_,PH_,COL(10,8,6,236));
    gfxQuad(PX_,PY_,PW_,1,C_HEX(E8,A1,3D));
    gfxQuad(PX_,PY_+PH_-1,PW_,1,C_HEX(E8,A1,3D));
    w=gfxTextW("MERCANTE",2);
    gfxText((SCR_W-w)/2,PY_+8,"MERCANTE",C_HEX(E8,A1,3D),2);
    snprintf(buf,sizeof buf,"Il tuo oro: %d",g_me.gold);
    gfxText(PX_+10,PY_+28,buf,C_HEX(D4,AF,37),1);
    for (i=0;i<4;++i){
        int cost = (rows[i].kind==IK_EQUIP)? 45+d*8 :
                   (rows[i].kind==IK_POWER)? 20+d*4 : 12+d*2;
        int y=PY_+42+i*16;
        if (i==s_merchantSel) gfxQuad(PX_+6,y-1,PW_-12,14,COL(232,161,61,60));
        gfxDrawSprite(rows[i].spr,PX_+16,y+5,TILE_PX*0.45f,1,0,0);
        gfxText(PX_+28,y,rows[i].name,g_me.gold>=cost?C_HEX(E8,DC,C5):C_HEX(9C,8E,77),1);
        snprintf(buf,sizeof buf,"%d oro",cost);
        gfxText(PX_+PW_-10-gfxTextW(buf,1),y,buf,C_HEX(D4,AF,37),1);
    }
    snprintf(buf,sizeof buf,"X compra   O chiude");
    gfxText(PX_+10,PY_+PH_-14,buf,C_HEX(9C,8E,77),1);
}
