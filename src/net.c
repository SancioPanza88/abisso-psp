#include "net.h"
#include "gfx.h"
#include "data.h"
#include "atlas_data.h"

#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

/* =====================================================================
   Implementazione del multiplayer (vedi net.h).
   ===================================================================== */

#define NET_PORT        34567
#define NET_ADHOC_PORT  34568
#define NET_ADHOC_GROUP 0x41424953u   /* "ABIS" */
#define NET_TICK        0.05f         /* 20 Hz come lo snapshot originale */
#define NET_TIMEOUT     8.0f

typedef struct {
    unsigned char type;
    unsigned char flags;
    unsigned short seq;
    int peerId;
    float x, y, fx, fy, hp, maxHp;
    int a, b;                 /* campi generici (cls/depth/gold/potions) */
    char text[NET_CHAT_LEN];
} NetPacket;

static int s_transport = NET_TRANSPORT_NONE;
static int s_active = 0;
static int s_isHost = 0;
static int s_localId = 0;
static int s_room = 0;
static char s_name[NET_NAME_LEN];
static int s_cls = 0;
static float s_sendAcc = 0;
static unsigned short s_seq = 0;

static NetPlayer s_players[NET_MAX_PLAYERS];
static NetHitFn s_hitFn = 0;

static unsigned char s_snapshot[NET_SNAPSHOT_MAX];
static int s_snapshotLen = 0;
static unsigned int s_worldFp = 0;

/* stato locale da condividere */
static float s_lx, s_ly, s_lfx, s_lfy, s_lhp, s_lmaxhp;
static int s_lflags, s_ldepth, s_lgold, s_lpotions;

/* ---- trasporto adhoc ---- */
static int s_adhocId = -1;
static struct sockaddr_adhoc s_adhocAddr;

/* ---- trasporto inet ---- */
static int s_sock = -1;
static struct sockaddr_in s_dest;
static int s_netInited = 0;

static void playersReset(void)
{
    memset(s_players, 0, sizeof(s_players));
    s_players[0].used = 1;
    s_players[0].isLocal = 1;
    s_players[0].peerId = s_localId;
    snprintf(s_players[0].name, NET_NAME_LEN, "%s", s_name);
    s_players[0].cls = s_cls;
}

static NetPlayer* playerSlot(int peerId)
{
    for (int i = 0; i < NET_MAX_PLAYERS; i++)
        if (s_players[i].used && s_players[i].peerId == peerId) return &s_players[i];
    return 0;
}

static NetPlayer* playerAlloc(int peerId)
{
    NetPlayer* p = playerSlot(peerId);
    if (p) return p;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        if (!s_players[i].used) {
            memset(&s_players[i], 0, sizeof(s_players[i]));
            s_players[i].used = 1;
            s_players[i].peerId = peerId;
            return &s_players[i];
        }
    }
    return 0;
}

/* ---- invio/ricezione basso livello ---- */

static void rawSend(const void* data, int len)
{
    if (s_transport == NET_TRANSPORT_ADHOC) {
        if (s_adhocId >= 0)
            sceNetAdhocPdpSend(s_adhocId, &s_adhocAddr, NET_ADHOC_PORT,
                               (void*)data, len, 0, 0);
    } else if (s_transport == NET_TRANSPORT_INET) {
        if (s_sock >= 0)
            sendto(s_sock, data, len, 0, (struct sockaddr*)&s_dest, sizeof(s_dest));
    }
}

static int rawRecv(void* data, int maxLen, int* fromPeer)
{
    if (s_transport == NET_TRANSPORT_ADHOC) {
        struct sockaddr_adhoc from;
        unsigned int fromLen = sizeof(from);
        int r = sceNetAdhocPdpRecv(s_adhocId, &from, &fromLen, data, &maxLen, 0, 0);
        if (r < 0) return 0;
        if (fromPeer) *fromPeer = 0;
        return maxLen;
    } else if (s_transport == NET_TRANSPORT_INET) {
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        int r = recvfrom(s_sock, data, maxLen, 0, (struct sockaddr*)&from, &fromLen);
        if (r <= 0) return 0;
        if (fromPeer) *fromPeer = ntohl(from.sin_addr.s_addr) & 0x7fffffff;
        return r;
    }
    return 0;
}

/* ---- protocollo ---- */

static void sendPacket(unsigned char type, int peerId, const NetPlayer* p)
{
    NetPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = type;
    pkt.seq = ++s_seq;
    pkt.peerId = peerId;
    if (p) {
        pkt.x = p->x; pkt.y = p->y;
        pkt.fx = p->facingX; pkt.fy = p->facingY;
        pkt.hp = p->hp; pkt.maxHp = p->maxHp;
        pkt.flags = (unsigned char)p->flags;
        pkt.a = p->cls;
        pkt.b = p->depth;
    }
    rawSend(&pkt, sizeof(pkt));
}

static void handlePacket(const NetPacket* pkt, int fromPeer)
{
    switch (pkt->type) {
    case NMSG_HELLO: {
        NetPlayer* p = playerAlloc(pkt->peerId);
        if (p) {
            p->cls = pkt->a;
            p->depth = pkt->b;
            p->x = p->rx = pkt->x;
            p->y = p->ry = pkt->y;
            p->hp = pkt->hp; p->maxHp = pkt->maxHp;
            p->lastSeen = 0;
        }
        /* rispondi con il nostro stato */
        sendPacket(NMSG_STATE, s_localId, &s_players[0]);
        break;
    }
    case NMSG_BYE: {
        NetPlayer* p = playerSlot(pkt->peerId);
        if (p && !p->isLocal) p->used = 0;
        break;
    }
    case NMSG_INPUT:
    case NMSG_STATE: {
        NetPlayer* p = playerAlloc(pkt->peerId);
        if (p) {
            p->rx = pkt->x; p->ry = pkt->y;
            p->facingX = pkt->fx; p->facingY = pkt->fy;
            p->hp = pkt->hp; p->maxHp = pkt->maxHp;
            p->flags = pkt->flags;
            p->cls = pkt->a;
            p->depth = pkt->b;
            p->lastSeen = 0;
        }
        break;
    }
    case NMSG_SNAPSHOT: {
        int len = (int)sizeof(NetPacket);
        (void)len;
        /* il payload snapshot e' incapsulato nel pacchetto: copiamo i byte utili */
        int n = (int)sizeof(NetPacket) - (int)sizeof(NetPacket) + 0;
        (void)n;
        break;
    }
    case NMSG_MHIT: {
        if (pkt->peerId == s_localId && s_hitFn)
            s_hitFn(pkt->x, (int)pkt->y);
        break;
    }
    case NMSG_SYNCREP:
        s_worldFp = (unsigned int)pkt->a;
        break;
    case NMSG_CHAT: {
        NetPlayer* p = playerSlot(pkt->peerId);
        if (p) {
            snprintf(p->chat, NET_CHAT_LEN, "%s", pkt->text);
            p->chatT = 4.0f;
        }
        break;
    }
    default:
        break;
    }
    (void)fromPeer;
}

static void pump(void)
{
    unsigned char buf[sizeof(NetPacket) + NET_SNAPSHOT_MAX];
    for (int i = 0; i < 32; i++) {
        int fromPeer = 0;
        int n = rawRecv(buf, sizeof(buf), &fromPeer);
        if (n < (int)sizeof(NetPacket)) break;
        NetPacket pkt;
        memcpy(&pkt, buf, sizeof(pkt));
        handlePacket(&pkt, fromPeer);
        if (pkt.type == NMSG_SNAPSHOT) {
            int payload = n - (int)sizeof(NetPacket);
            if (payload > 0) {
                if (payload > NET_SNAPSHOT_MAX) payload = NET_SNAPSHOT_MAX;
                memcpy(s_snapshot, buf + sizeof(NetPacket), payload);
                s_snapshotLen = payload;
            }
        }
    }
}

/* ---- init trasporti ---- */

static int initAdhoc(void)
{
    if (sceNetAdhocInit() < 0) return 0;
    if (sceNetAdhocctlInit(0x2000, 0x20, 0) < 0) return 0;
    if (sceNetAdhocctlConnect("ABISSO") < 0) return 0;
    s_adhocId = sceNetAdhocPdpCreate((unsigned char*)&s_adhocAddr, NET_ADHOC_PORT,
                                     0x400, 0);
    if (s_adhocId < 0) return 0;
    s_adhocAddr.sa_family = AF_INET;
    s_adhocAddr.sa_data[0] = 0;
    s_adhocAddr.sa_data[1] = 0;
    memset(s_adhocAddr.sa_data + 2, 0xff, 4); /* broadcast adhoc */
    return 1;
}

static int initInet(const char* host)
{
    if (!s_netInited) {
        if (sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000) < 0) return 0;
        if (sceNetInetInit() < 0) return 0;
        s_netInited = 1;
    }
    s_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_sock < 0) return 0;
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(NET_PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(s_sock, (struct sockaddr*)&local, sizeof(local));

    memset(&s_dest, 0, sizeof(s_dest));
    s_dest.sin_family = AF_INET;
    s_dest.sin_port = htons(NET_PORT);
    s_dest.sin_addr.s_addr = inet_addr(host ? host : "255.255.255.255");
    return 1;
}

int netInit(int transport, const char* room, const char* playerName, int cls)
{
    s_transport = transport;
    s_room = room ? (int)hashStr(room) : 0;
    s_cls = cls;
    snprintf(s_name, NET_NAME_LEN, "%s", playerName ? playerName : "Eroe");
    s_localId = (int)(hashStr(s_name) & 0x7fffffff);
    s_isHost = 1;   /* primo giocatore = host; i client si agganciano via HELLO */
    s_seq = 0;
    s_snapshotLen = 0;
    playersReset();

    int ok = 0;
    if (transport == NET_TRANSPORT_ADHOC) ok = initAdhoc();
    else if (transport == NET_TRANSPORT_INET) ok = initInet(room);
    if (!ok) { s_transport = NET_TRANSPORT_NONE; return 0; }

    s_active = 1;
    sendPacket(NMSG_HELLO, s_localId, &s_players[0]);
    return 1;
}

void netShutdown(void)
{
    if (s_active) sendPacket(NMSG_BYE, s_localId, &s_players[0]);
    if (s_transport == NET_TRANSPORT_ADHOC) {
        if (s_adhocId >= 0) sceNetAdhocPdpDelete(s_adhocId, 0);
        sceNetAdhocctlDisconnect();
        sceNetAdhocctlTerm();
        sceNetAdhocTerm();
    } else if (s_transport == NET_TRANSPORT_INET) {
        if (s_sock >= 0) close(s_sock);
        s_sock = -1;
    }
    s_active = 0;
    s_transport = NET_TRANSPORT_NONE;
}

int netActive(void)    { return s_active; }
int netIsHost(void)    { return s_isHost; }
int netTransport(void) { return s_transport; }
int netLocalPeerId(void) { return s_localId; }

int netPeerCount(void)
{
    int n = 0;
    for (int i = 0; i < NET_MAX_PLAYERS; i++)
        if (s_players[i].used && !s_players[i].isLocal) n++;
    return n;
}

void netSetLocalState(float x, float y, float fx, float fy, float hp, float maxHp,
                      int flags, int depth, int gold, int potions)
{
    s_lx = x; s_ly = y; s_lfx = fx; s_lfy = fy;
    s_lhp = hp; s_lmaxhp = maxHp; s_lflags = flags;
    s_ldepth = depth; s_lgold = gold; s_lpotions = potions;
    NetPlayer* me = &s_players[0];
    me->x = x; me->y = y; me->facingX = fx; me->facingY = fy;
    me->hp = hp; me->maxHp = maxHp; me->flags = flags;
    me->depth = depth; me->gold = gold; me->potions = potions;
}

void netSendInput(float x, float y, float fx, float fy, int flags)
{
    NetPlayer p;
    memset(&p, 0, sizeof(p));
    p.x = x; p.y = y; p.facingX = fx; p.facingY = fy;
    p.hp = s_lhp; p.maxHp = s_lmaxhp; p.flags = flags;
    p.cls = s_cls; p.depth = s_ldepth;
    sendPacket(NMSG_INPUT, s_localId, &p);
}

void netDealMonsterHit(int peerId, float amount, int poison)
{
    NetPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = NMSG_MHIT;
    pkt.peerId = peerId;
    pkt.x = amount;
    pkt.y = (float)poison;
    rawSend(&pkt, sizeof(pkt));
}

void netSetHitHandler(NetHitFn fn) { s_hitFn = fn; }

void netHostSendSnapshot(const unsigned char* data, int len)
{
    if (!s_isHost || len <= 0) return;
    if (len > NET_SNAPSHOT_MAX) len = NET_SNAPSHOT_MAX;
    unsigned char buf[sizeof(NetPacket) + NET_SNAPSHOT_MAX];
    NetPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = NMSG_SNAPSHOT;
    pkt.peerId = s_localId;
    memcpy(buf, &pkt, sizeof(pkt));
    memcpy(buf + sizeof(pkt), data, len);
    rawSend(buf, (int)sizeof(pkt) + len);
}

int netRecvSnapshot(unsigned char* out, int maxLen)
{
    if (s_snapshotLen <= 0) return 0;
    int n = s_snapshotLen < maxLen ? s_snapshotLen : maxLen;
    memcpy(out, s_snapshot, n);
    s_snapshotLen = 0;
    return n;
}

unsigned int netWorldFingerprint(void) { return s_worldFp; }

void netSendChat(const char* text)
{
    NetPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = NMSG_CHAT;
    pkt.peerId = s_localId;
    snprintf(pkt.text, NET_CHAT_LEN, "%s", text ? text : "");
    rawSend(&pkt, sizeof(pkt));
}

int netPlayerCount(void)
{
    int n = 0;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) if (s_players[i].used) n++;
    return n;
}

NetPlayer* netPlayerAt(int i)
{
    int n = 0;
    for (int k = 0; k < NET_MAX_PLAYERS; k++) {
        if (!s_players[k].used) continue;
        if (n == i) return &s_players[k];
        n++;
    }
    return 0;
}

NetPlayer* netPlayerById(int peerId) { return playerSlot(peerId); }

void netRenderPlayers(void)
{
    if (!s_active) return;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        NetPlayer* p = &s_players[i];
        if (!p->used || p->isLocal) continue;
        float sx, sy;
        gfxWorldToScreen(p->x, p->y, &sx, &sy);
        int atlas = AR_HERO_GUERRIERO;
        switch (p->cls) {
        case 1: atlas = AR_HERO_LADRO; break;
        case 2: atlas = AR_HERO_MAGO; break;
        case 3: atlas = AR_HERO_RANGER; break;
        case 5: atlas = AR_HERO_PALADINO; break;
        case 6: atlas = AR_HERO_NEGROMANTE; break;
        case 7: atlas = AR_HERO_BARDO; break;
        case 8: atlas = AR_HERO_MONACO; break;
        default: break;
        }
        float flash = (p->flags & NF_DEAD) ? 0.6f : 0.f;
        if (p->flags & NF_DOWNED) flash = 0.4f;
        gfxDrawSprite(atlas, sx, sy, TILE_PX * 1.4f, 1.f, 0, flash);
        /* barra hp sopra il compagno */
        if (p->maxHp > 0.f) {
            float ratio = clampf(p->hp / p->maxHp, 0.f, 1.f);
            gfxQuad(sx - TILE_PX * 0.5f, sy - TILE_PX * 1.1f, TILE_PX, 3, COL(30,20,20,255));
            gfxQuad(sx - TILE_PX * 0.5f, sy - TILE_PX * 1.1f, TILE_PX * ratio, 3, COL(193,68,58,255));
        }
        /* nome */
        float w = gfxTextW(p->name, 1);
        gfxText(sx - w * 0.5f, sy + TILE_PX * 0.7f, p->name, COL(232,220,197,220), 1);
        /* bolla di chat */
        if (p->chatT > 0.f && p->chat[0]) {
            float cw = gfxTextW(p->chat, 1);
            gfxQuad(sx - cw * 0.5f - 3, sy - TILE_PX * 1.9f, cw + 6, 11, COL(12,10,7,200));
            gfxText(sx - cw * 0.5f, sy - TILE_PX * 1.9f + 2, p->chat, COL(232,220,197,255), 1);
        }
    }
}

void netUpdate(float dt)
{
    if (!s_active) return;
    pump();

    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        NetPlayer* p = &s_players[i];
        if (!p->used || p->isLocal) continue;
        p->lastSeen += dt;
        if (p->lastSeen > NET_TIMEOUT) { p->used = 0; continue; }
        /* interpolazione verso la posizione ricevuta (come remotePlayers) */
        p->x = lerpf(p->x, p->rx, clampf(dt * 12.0f, 0, 1));
        p->y = lerpf(p->y, p->ry, clampf(dt * 12.0f, 0, 1));
        if (p->chatT > 0) p->chatT -= dt;
    }

    s_sendAcc += dt;
    if (s_sendAcc >= NET_TICK) {
        s_sendAcc = 0;
        NetPlayer p;
        memset(&p, 0, sizeof(p));
        p.x = s_lx; p.y = s_ly; p.facingX = s_lfx; p.facingY = s_lfy;
        p.hp = s_lhp; p.maxHp = s_lmaxhp; p.flags = s_lflags;
        p.cls = s_cls; p.depth = s_ldepth;
        sendPacket(NMSG_STATE, s_localId, &p);
    }
}