#ifndef ABISSO_NET_H
#define ABISSO_NET_H

#include "common.h"

/* Multiplayer: porting del netcode dell'originale (Trystero/WebRTC P2P)
   su trasporti nativi PSP:
     - Adhoc PSP (sceNetAdhoc): PSP <-> PSP senza infrastruttura.
     - Inet UDP (sceNetInet): PSP <-> PC tramite relay/bridge.
   Modello host-autorevole come l'originale. */

#define NET_MAX_PLAYERS 8
#define NET_NAME_LEN    16
#define NET_CHAT_LEN    48
#define NET_ROOM_LEN    16

enum { NET_TRANSPORT_NONE=0, NET_TRANSPORT_ADHOC, NET_TRANSPORT_INET };

enum {
    NMSG_HELLO=1, NMSG_BYE, NMSG_INPUT, NMSG_STATE, NMSG_SNAPSHOT,
    NMSG_MHIT, NMSG_SYNCREQ, NMSG_SYNCREP, NMSG_CHAT, NMSG_PING, NMSG_PONG
};

enum {
    NF_ATTACK=1<<0, NF_ABILITY=1<<1, NF_INTERACT=1<<2, NF_POTION=1<<3,
    NF_MANAPOT=1<<4, NF_DEAD=1<<5, NF_DOWNED=1<<6
};

typedef struct {
    int   used;
    int   isLocal;
    int   peerId;
    char  name[NET_NAME_LEN];
    int   cls;
    float x, y;
    float rx, ry;
    float facingX, facingY;
    float hp, maxHp;
    int   gold, potions;
    int   flags;
    int   depth;
    float lastSeen;
    char  chat[NET_CHAT_LEN];
    float chatT;
} NetPlayer;

typedef void (*NetHitFn)(float amount, int poison);

int  netInit(int transport, const char* room, const char* playerName, int cls);
void netShutdown(void);
int  netActive(void);
int  netIsHost(void);
int  netTransport(void);
int  netPeerCount(void);

void netUpdate(float dt);

void netSetLocalState(float x, float y, float fx, float fy, float hp, float maxHp,
                      int flags, int depth, int gold, int potions);

void netSendInput(float x, float y, float fx, float fy, int flags);

void netDealMonsterHit(int peerId, float amount, int poison);
void netSetHitHandler(NetHitFn fn);

#define NET_SNAPSHOT_MAX 512
void netHostSendSnapshot(const unsigned char* data, int len);
int  netRecvSnapshot(unsigned char* out, int maxLen);
unsigned int netWorldFingerprint(void);

void netSendChat(const char* text);

int        netPlayerCount(void);
NetPlayer* netPlayerAt(int i);
NetPlayer* netPlayerById(int peerId);
int        netLocalPeerId(void);

/* Disegna i giocatori remoti nel mondo (sprite + nome + bolla chat). */
void netRenderPlayers(void);

#endif
