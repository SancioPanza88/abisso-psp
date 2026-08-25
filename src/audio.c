#include "audio.h"
#include "common.h"
#include <pspuser.h>
#include <pspaudio.h>
#include <string.h>

/* =====================================================================
   Sintesi audio: un canale SRC stereo 44100Hz alimentato da un thread
   dedicato. Gli eventi (sfx) arrivano da una coda circolare lock-free;
   la musica ambient e' generata dal thread stesso in base al piano.
   ===================================================================== */

#define SAMPS_PER_BLOCK 2048
#define MASTER_VOL 14000
#define QUEUE_SZ 32

typedef struct {
    float freq;      /* Hz (0 = rumore) */
    float dur;       /* secondi */
    float vol;       /* 0..1 */
    int   wave;      /* 0 square, 1 triangle, 2 noise, 3 saw */
    float slide;     /* moltiplicatore di freq a fine suono */
} VoiceEv;

static VoiceEv s_queue[QUEUE_SZ];
static volatile int s_qHead=0, s_qTail=0;
static volatile int s_running=0;
static volatile int s_muted=0;
static volatile int s_musicOn=1;
static int s_chRes=-1;
static int s_depth=1;

typedef struct { float t; float freq,dur,vol,slide; int active,wave; } Voice;
#define VOICES 8
static Voice s_voices[VOICES];
static unsigned int s_musicClock=0;

void musicSetDepth(int depth){ s_depth = depth; }
int  audioIsMuted(void){ return s_muted; }
void audioToggleMute(void){ s_muted ^= 1; }

static void spawnVoice(const VoiceEv* ev){
    int slot=-1,i;
    for (i=0;i<VOICES;++i)
        if (!s_voices[i].active){ slot=i; break; }
    if (slot<0) return;
    {
        Voice* v=&s_voices[slot];
        v->t=0.f; v->freq=ev->freq; v->dur=ev->dur; v->vol=ev->vol;
        v->wave=ev->wave; v->slide=ev->slide; v->active=1;
    }
}

static const VoiceEv* presetFor(int id){
    static VoiceEv ev;
    switch (id){
        case SFX_STEP:        ev.freq=90;   ev.dur=0.05f; ev.vol=0.10f; ev.wave=2; ev.slide=0.8f; break;
        case SFX_SWING:       ev.freq=520;  ev.dur=0.07f; ev.vol=0.16f; ev.wave=3; ev.slide=0.55f; break;
        case SFX_HIT:         ev.freq=180;  ev.dur=0.09f; ev.vol=0.30f; ev.wave=2; ev.slide=0.7f; break;
        case SFX_CRIT:        ev.freq=260;  ev.dur=0.16f; ev.vol=0.38f; ev.wave=0; ev.slide=0.5f; break;
        case SFX_HURT:        ev.freq=140;  ev.dur=0.18f; ev.vol=0.34f; ev.wave=0; ev.slide=0.6f; break;
        case SFX_SHOOT_MAGE:  ev.freq=880;  ev.dur=0.14f; ev.vol=0.20f; ev.wave=1; ev.slide=1.6f; break;
        case SFX_SHOOT_RANGER:ev.freq=660;  ev.dur=0.09f; ev.vol=0.18f; ev.wave=3; ev.slide=1.5f; break;
        case SFX_SHOOT_PLASMA:ev.freq=980;  ev.dur=0.16f; ev.vol=0.22f; ev.wave=1; ev.slide=1.8f; break;
        case SFX_POTION:      ev.freq=520;  ev.dur=0.18f; ev.vol=0.22f; ev.wave=1; ev.slide=1.5f; break;
        case SFX_MANA:        ev.freq=420;  ev.dur=0.18f; ev.vol=0.22f; ev.wave=1; ev.slide=1.4f; break;
        case SFX_PICKUP:      ev.freq=1180; ev.dur=0.08f; ev.vol=0.20f; ev.wave=0; ev.slide=1.4f; break;
        case SFX_GEM:         ev.freq=1560; ev.dur=0.20f; ev.vol=0.22f; ev.wave=1; ev.slide=1.5f; break;
        case SFX_POWER:       ev.freq=300;  ev.dur=0.30f; ev.vol=0.26f; ev.wave=0; ev.slide=2.0f; break;
        case SFX_CHEST:       ev.freq=240;  ev.dur=0.25f; ev.vol=0.26f; ev.wave=0; ev.slide=1.7f; break;
        case SFX_STAIR:       ev.freq=200;  ev.dur=0.50f; ev.vol=0.24f; ev.wave=1; ev.slide=2.2f; break;
        case SFX_BOOM:        ev.freq=60;   ev.dur=0.45f; ev.vol=0.42f; ev.wave=2; ev.slide=0.55f; break;
        case SFX_KILL:        ev.freq=160;  ev.dur=0.20f; ev.vol=0.28f; ev.wave=2; ev.slide=0.5f; break;
        case SFX_BOSS_ROAR:   ev.freq=70;   ev.dur=0.85f; ev.vol=0.46f; ev.wave=0; ev.slide=0.62f; break;
        case SFX_BOSS_KILL:   ev.freq=110;  ev.dur=1.00f; ev.vol=0.48f; ev.wave=0; ev.slide=0.35f; break;
        case SFX_DEATH:       ev.freq=220;  ev.dur=0.70f; ev.vol=0.40f; ev.wave=1; ev.slide=0.30f; break;
        case SFX_ABILITY:     ev.freq=700;  ev.dur=0.22f; ev.vol=0.24f; ev.wave=1; ev.slide=1.9f; break;
        default:              ev.freq=880;  ev.dur=0.06f; ev.vol=0.16f; ev.wave=0; ev.slide=1.f; break;
    }
    return &ev;
}

void sfxPlay(int id){
    if (id<0 || id>=SFX_COUNT) return;
    {
        int next=(s_qHead+1)%QUEUE_SZ;
        if (next==s_qTail) return; /* coda piena: salta */
        s_queue[s_qHead]=*presetFor(id);
        s_qHead=next;
    }
}

/* scala pentatonica minore di La: l'umore cambia col piano */
static const float SCALE_NOTES[5] = {110.f,130.81f,146.83f,164.81f,196.f};

static float synthSample(Voice* v){
    /* valore -1..1 con envelope esponenziale */
    float env = expf(-v->t*(4.5f/ (v->dur>0.001f?v->dur:0.001f)));
    float ph = v->t * v->freq * 2.f*M_PI;
    float out = 0.f;
    if (v->wave==0) out = sinf(ph)>=0.f ? 1.f : -1.f;
    else if (v->wave==1) out = (2.f/M_PI)*asinf(sinf(ph));
    else if (v->wave==3) out = (2.f/M_PI)*(v->freq*M_PI*fmodf(v->t,1.f/v->freq)) - 1.f;
    else {
        unsigned int n = (unsigned int)(ph*13.37f);
        n = (n ^ (n>>7)) * 2654435761u;
        out = ((int)((n>>16)&0xFF) - 128)/128.f;
    }
    return out*env*v->vol;
}

static int audioThread(SceSize args, void* argp){
    static short buf[SAMPS_PER_BLOCK*2]; /* static: non affolla lo stack del thread */
    (void)args; (void)argp;
    while (s_running){
        int i;
        /* consuma eventi */
        while (s_qTail!=s_qHead && !s_muted){
            spawnVoice(&s_queue[s_qTail]);
            s_qTail=(s_qTail+1)%QUEUE_SZ;
        }
        for (i=0;i<SAMPS_PER_BLOCK;++i){
            float dt=1.f/44100.f;
            float l=0.f,r=0.f;
            int vi;
            for (vi=0; vi<VOICES; ++vi){
                Voice* v=&s_voices[vi];
                if (!v->active) continue;
                {
                    float s=synthSample(v);
                    l+=s; r+=s;
                    v->t+=dt;
                    if (v->t>=v->dur) v->active=0;
                }
            }
            /* musica ambient: nota ogni ~0.42s */
            if (s_musicOn && !s_muted){
                s_musicClock++;
                if (s_musicClock % 18500 == 400){
                    unsigned int seed=(unsigned int)(s_depth*2654435761u + (s_musicClock/18500));
                    int ni=(seed>>3)%5;
                    VoiceEv mv;
                    mv.freq=SCALE_NOTES[ni]*((s_depth%5==0)?0.5f:1.f);
                    mv.dur=1.4f; mv.vol=0.05f+(float)((seed>>9)&3)*0.008f;
                    mv.wave=1; mv.slide=1.f;
                    spawnVoice(&mv);
                }
            }
            l*=MASTER_VOL; r*=MASTER_VOL;
            if (l>32767.f) l=32767.f; if (l<-32768.f) l=-32768.f;
            if (r>32767.f) r=32767.f; if (r<-32768.f) r=-32768.f;
            buf[i*2]=(short)l; buf[i*2+1]=(short)r;
        }
        sceAudioSRCOutputBlocking(s_muted?0:PSP_AUDIO_VOLUME_MAX-4096, buf);
    }
    sceAudioSRCChRelease();
    sceKernelExitDeleteThread(0);
    return 0;
}

void audioInit(void){
    SceUID thid;
    memset(s_voices,0,sizeof(s_voices));
    s_chRes = sceAudioSRCChReserve(SAMPS_PER_BLOCK,44100,2);
    if (s_chRes<0) return; /* nessun audio: il gioco prosegue in silenzio */
    s_running=1;
    thid = sceKernelCreateThread("abisso_audio",audioThread,0x32,0x1000,0,NULL);
    if (thid>=0) sceKernelStartThread(thid,0,NULL);
    else { s_running=0; sceAudioSRCChRelease(); }
}
