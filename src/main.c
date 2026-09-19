#include <pspuser.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "common.h"
#include "gfx.h"
#include "game.h"
#include "ui.h"
#include "audio.h"
#include "data.h"
#include "net.h"

/* forward: definito in game.c, applica danno al giocatore locale */
void netHitFromRemote(float amount,int poison);
static void onNetHit(float amount,int poison){ netHitFromRemote(amount,poison); }

PSP_MODULE_INFO("ABISSO", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

/* ---- callback di uscita (HOME) come nei sample pspsdk ---- */
static int s_exitRequest=0;
static int exitCallback(int arg1,int arg2,void*common){
    (void)arg1;(void)arg2;(void)common;
    s_exitRequest=1;
    return 0;
}
static int callbackThread(SceSize args,void*argp){
    int cbid;
    (void)args;(void)argp;
    cbid=sceKernelCreateCallback("Exit Callback",exitCallback,NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}
static int setupCallbacks(void){
    int thid=sceKernelCreateThread("update_thread",callbackThread,0x11,0xFA0,0,0);
    if (thid>=0) sceKernelStartThread(thid,0,NULL);
    return thid;
}

enum { ST_TITLE=0, ST_CLASSSEL, ST_MPMENU, ST_PLAY, ST_PAUSE };
static int s_state=ST_TITLE;
static int s_classSel=0;
static int s_pauseSel=0;
static int s_mpSel=0;          /* 0 = single, 1 = adhoc, 2 = inet */
static int s_lastDepth=1;      /* per aggiornare il tappeto audio al cambio piano */
static float s_blinkT=0;
static char s_playerName[NET_NAME_LEN]="Eroe";
static char s_roomName[NET_ROOM_LEN]="abisso";

static float nowSeconds(void){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (float)tv.tv_sec + tv.tv_usec*0.000001f;
}

int main(void){
    SceCtrlData pad, oldPad;
    float lastT;
    GameInput in;

    setupCallbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    scePowerSetClockFrequency(333,333,166);

    if (!gfxInit()){
        sceKernelExitGame();
        return 0;
    }
    audioInit();
    netSetHitHandler(onNetHit);

    memset(&oldPad,0,sizeof oldPad);
    lastT=nowSeconds();

    while (!s_exitRequest){
        unsigned int pressed, released;
        float now,dt;

        sceCtrlReadBufferPositive(&pad,1);
        pressed  = pad.Buttons & ~oldPad.Buttons;
        released = oldPad.Buttons & ~pad.Buttons;
        oldPad=pad;

        now=nowSeconds();
        dt=now-lastT; lastT=now;
        if (dt>0.05f) dt=0.05f;      /* clamp identico all'originale */
        if (dt<0.f)   dt=0.f;
        s_blinkT+=dt;

        memset(&in,0,sizeof in);

        if (s_state==ST_TITLE){
            if (pressed & PSP_CTRL_START){
                s_state=ST_CLASSSEL;
                sfxPlay(SFX_CLICK);
            }
            /* senza frame la GE non riceve comandi: schermo nero su hardware reale */
            gfxFrameStart(0,0,COL(10,9,6,255));
            uiRenderTitle(s_blinkT);
        } else if (s_state==ST_CLASSSEL){
            if (pressed & PSP_CTRL_LEFT){  s_classSel=(s_classSel+CLASS_COUNT-1)%CLASS_COUNT; sfxPlay(SFX_CLICK); }
            if (pressed & PSP_CTRL_RIGHT){ s_classSel=(s_classSel+1)%CLASS_COUNT; sfxPlay(SFX_CLICK); }
            if (pressed & PSP_CTRL_UP)   { s_classSel=(s_classSel+CLASS_COUNT-1)%CLASS_COUNT; sfxPlay(SFX_CLICK); }
            if (pressed & PSP_CTRL_DOWN) { s_classSel=(s_classSel+1)%CLASS_COUNT; sfxPlay(SFX_CLICK); }
            if ((pressed & PSP_CTRL_CROSS) || (pressed & PSP_CTRL_START)){
                s_state=ST_MPMENU;
                s_mpSel=0;
                sfxPlay(SFX_CLICK);
            }
            gfxFrameStart(0,0,COL(10,9,6,255));
            uiRenderClassSelect(s_classSel);
        } else if (s_state==ST_MPMENU){
            static const char* mpopts[3]={
                "1 Giocatore",
                "Multiplayer Adhoc (PSP-PSP)",
                "Multiplayer Rete (PSP-PC)"
            };
            int k;
            if (pressed & PSP_CTRL_UP)   { s_mpSel=(s_mpSel+2)%3; sfxPlay(SFX_CLICK); }
            if (pressed & PSP_CTRL_DOWN) { s_mpSel=(s_mpSel+1)%3; sfxPlay(SFX_CLICK); }
            if (pressed & PSP_CTRL_CIRCLE){ s_state=ST_CLASSSEL; sfxPlay(SFX_CLICK); }
            if (pressed & PSP_CTRL_CROSS){
                gameRandSeed((unsigned int)(now*1000.f));
                gameNewRun(s_classSel);
                musicSetDepth(1);
                s_lastDepth=1;
                if (s_mpSel==1) netInit(NET_TRANSPORT_ADHOC, s_roomName, s_playerName, s_classSel);
                else if (s_mpSel==2) netInit(NET_TRANSPORT_INET, s_roomName, s_playerName, s_classSel);
                s_state=ST_PLAY;
                sfxPlay(SFX_STAIR);
            }
            /* Senza frame la GE non riceve comandi: schermo nero su hardware
               reale (stesso bug gia' corretto in TITLE/CLASSSEL). Il disegno
               deve stare in questo ramo: un secondo "else if" sullo stesso
               stato sarebbe irraggiungibile. */
            gfxFrameStart(0,0,COL(10,9,6,255));
            {
                const char* title="MODALITA' DI GIOCO";
                float w=gfxTextW(title,2);
                gfxText((SCR_W-w)/2,40,title,COL(232,161,61,255),2);
                for (k=0;k<3;++k){
                    float lw=gfxTextW(mpopts[k],2);
                    gfxText((SCR_W-lw)/2,100+k*34,mpopts[k],
                            k==s_mpSel?COL(232,161,61,255):COL(232,220,197,255),2);
                }
                {
                    const char* hint="X conferma   O indietro";
                    float hw=gfxTextW(hint,1);
                    gfxText((SCR_W-hw)/2,SCR_H-30,hint,COL(156,142,119,255),1);
                }
            }
        } else if (s_state==ST_PLAY){
            /* movimento: analogico + d-pad (keys WASD/frecce dell'originale) */
            {
                int lx=(int)pad.Lx-128, ly=(int)pad.Ly-128;
                float fx=(float)lx/127.f, fy=(float)ly/127.f;
                if (fabsf(fx)<0.28f) fx=0;
                if (fabsf(fy)<0.28f) fy=0;
                if (pad.Buttons & PSP_CTRL_LEFT)  fx=-1;
                if (pad.Buttons & PSP_CTRL_RIGHT) fx= 1;
                if (pad.Buttons & PSP_CTRL_UP)    fy=-1;
                if (pad.Buttons & PSP_CTRL_DOWN)  fy= 1;
                in.mvx=fx; in.mvy=fy;
            }
            in.attackHeld     = (pad.Buttons & PSP_CTRL_CROSS)?1:0;
            in.interactPressed= (pressed  & PSP_CTRL_SQUARE)?1:0;

            if (uiMerchantActive()){
                /* il pannello blocca i comandi di gioco come nell'originale */
                in.mvx=in.mvy=0; in.attackHeld=0; in.interactPressed=0;
                if (pressed & PSP_CTRL_UP)    uiMerchantMove(-1);
                if (pressed & PSP_CTRL_DOWN)  uiMerchantMove(1);
                if (pressed & PSP_CTRL_CROSS) uiMerchantBuy();
                if ((pressed & PSP_CTRL_SQUARE) || (pressed & PSP_CTRL_CIRCLE)) uiCloseMerchant();
            } else {
                /* eventi chiave: E/Q/R/F/M dell'originale */
                if (in.interactPressed) tryInteract();                /* KeyE */
                if (pressed & PSP_CTRL_TRIANGLE)  drinkPotion();      /* KeyQ */
                if (pressed & PSP_CTRL_LTRIGGER)  drinkManaPotion();  /* KeyR */
                if (pressed & PSP_CTRL_CIRCLE)    useClassAbility();  /* KeyF */
                if (pressed & PSP_CTRL_SELECT)    toggleMinimap();    /* KeyM */
            }
            if (pressed & PSP_CTRL_START){ s_state=ST_PAUSE; sfxPlay(SFX_CLICK); }

            {
                int nflags=0;
                if (g_me.dead)   nflags|=NF_DEAD;
                if (g_me.downed) nflags|=NF_DOWNED;
                if (in.attackHeld) nflags|=NF_ATTACK;
                if (netActive()){
                    netSetLocalState(g_me.x,g_me.y,g_me.facingX,g_me.facingY,
                                     g_me.hp,g_me.maxHp,nflags,g_world.depth,
                                     g_me.gold,g_me.potions);
                    netUpdate(dt);
                }
            }
            gameUpdate(dt,&in);
            /* il tappeto ambient cambia col piano come nell'originale */
            if (g_world.depth != s_lastDepth){
                s_lastDepth = g_world.depth;
                musicSetDepth(s_lastDepth);
            }
            /* ---- render ---- */
            gameRenderWorld();
            if (netActive()) netRenderPlayers();
            uiRenderGameHud();
            if (netActive() && netPeerCount()>0){
                char nbuf[48];
                snprintf(nbuf,sizeof nbuf,"Giocatori: %d  %s",
                         netPeerCount()+1,
                         netTransport()==NET_TRANSPORT_ADHOC?"(adhoc)":"(rete)");
                gfxText(8,SCR_H-14,nbuf,COL(127,174,99,255),1);
            }
            if (g_me.downed) uiRenderDownedOverlay();
            if (g_me.dead)   uiRenderDeadOverlay();
            if (uiMerchantActive()) uiRenderMerchantPanel();
        } else if (s_state==ST_PAUSE){
            static const char* opts[3]={"Riprendi","Suono","Torna al titolo"};
            static char soundLine[32];
            if (pressed & PSP_CTRL_UP)   { s_pauseSel=(s_pauseSel+3-1)%3; sfxPlay(SFX_CLICK); }
            if (pressed & PSP_CTRL_DOWN) { s_pauseSel=(s_pauseSel+1)%3; sfxPlay(SFX_CLICK); }
            if ((pressed & PSP_CTRL_START) || (pressed & PSP_CTRL_CIRCLE)){ s_state=ST_PLAY; }
            if (pressed & PSP_CTRL_CROSS){
                if (s_pauseSel==0) s_state=ST_PLAY;
                else if (s_pauseSel==1){ audioToggleMute(); sfxPlay(SFX_CLICK); }
                else { if (netActive()) netShutdown(); s_state=ST_TITLE; }
            }
            snprintf(soundLine,sizeof soundLine,"Suono: %s",audioIsMuted()?"OFF":"ON");
            gfxFrameStart(0,0,COL(10,9,6,255));
            {
                int k;
                for (k=0;k<3;++k){
                    const char* line = (k==1)? soundLine : opts[k];
                    float w=gfxTextW(line,2);
                    gfxText((SCR_W-w)/2,90+k*30,line,
                            k==s_pauseSel?COL(232,161,61,255):COL(232,220,197,255),2);
                }
            }
        }

        gfxFrameEnd();
    }

    gfxShutdown();
    sceKernelExitGame();
    return 0;
}
