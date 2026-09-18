# ABISSO — Porting PSP

Porting **1:1** di [Abisso 2.0](https://github.com/SancioPanza88/Abisso-2.0)
(roguelike multiplayer in tempo reale in un unico file HTML) per
**PlayStation Portable**, scritto in C con PSPSDK + GE (libgu).

Il gioco usa gli **asset grafici originali** (PNG di eroi, mostri, boss,
tile, icone): sono convertiti in un atlas texture (`assets/atlas.rle`)
incorporato nell'EBOOT dallo script `tools/build_atlas.ps1`, che replica i
ritagli (`SPRITE_CROP`), le celle degli sheet (`SPRITE_SHEET_INFO`) e le
riduzioni a 64px dei tile dell'originale.

## Build

La build avviene su GitHub Actions con il container ufficiale
[`pspdev/pspsdk`](https://hub.docker.com/r/pspdev/pspsdk):

1. push su `main`/`master` (o avvio manuale da tab *Actions*);
2. al termine scaricare l'artefatto **abisso-psp-eboot** contenente `EBOOT.PBP`;
3. copiarlo in `ms0:/PSP/GAME/ABISSO/EBOOT.PBP` (richiede custom firmware).

Build manuale (con PSPSDK installato):

```
make
```

## Comandi (mappatura dei tasti originali)

| Originale (PC)        | PSP                    | Azione                          |
|-----------------------|------------------------|---------------------------------|
| WASD / frecce / mouse | Analogico / D-pad      | Muoviti                         |
| Spazio / click        | X                      | Attacca (tieni premuto)         |
| E                     | □ Quadro               | Interagisci (scale/forziere/negozio) |
| F                     | ○ Cerchio              | Abilità di classe               |
| Q                     | △ Triangolo            | Pozione di salute               |
| R                     | L                      | Pozione di mana                 |
| M                     | SELECT                 | Minimappa on/off                |
| Esc                   | START                  | Pausa                           |

## Multiplayer

Come l'originale, il gioco è **host-autorevole**: l'host simula mostri,
proiettili, oggetti e boss, i client inviano il proprio input e ricevono gli
aggiornamenti. Su PSP il netcode WebRTC/Trystero originale è sostituito da due
trasporti nativi (vedi `src/net.c`):

- **Adhoc** (`sceNetAdhoc`): partite PSP-PSP, senza infrastruttura.
- **Rete PSP-PC** (`sceNetInet`): co-op con il gioco su browser via bridge,
  o PSP-PSP su LAN.

Dalla schermata di scelta classe si entra nel menu **Modalità di gioco**:
1 Giocatore / Multiplayer Adhoc (PSP-PSP) / Multiplayer Rete (PSP-PC).

Protocollo binario a 20 Hz (vedi `src/net.h`): `HELLO/BYE`, `INPUT/STATE`
(posizione, facing, hp, flag azione), `SNAPSHOT` (delta mondo host-client),
`MHIT` (danno mostro-giocatore), `SYNCREQ/SYNCREP` (fingerprint mondo), `CHAT`.
I giocatori remoti sono interpolati e disegnati con sprite, barra vita, nome e
bolla di chat, come la mappa `remotePlayers` originale.

### Co-op PSP - PC

1. Sulla PSP scegli *Multiplayer Rete (PSP-PC)* e avvia la partita.
2. Sul PC esegui `node tools/pc_bridge.js` (solo Node.js, nessuna dipendenza).
3. Il bridge ascolta UDP `34567` (PSP) e WebSocket `34569` (browser), scopre la
   PSP e fa da relay. Un client d'esempio è in `tools/pc_client.html`.

## Cosa è stato portato (fedele all'originale)

- Le **9 classi eroe** con statistiche, gittate, archi d'attacco, critici,
  costi di mana e abilità identiche (Carica, Passo Furtivo, Onda d'Urto,
  Raffica, Colpo Caricato del Prof, Muro Sacro, Drenaggio d'Anima,
  Canto d'Ispirazione, Onda di Chi).
- I **23 tipi di mostro** con HP/danno/velocità/aggro/pesi di spawn per
  profondità, affissi (Veloce, Esplosivo, Rigenerante) con la stessa
  probabilità `min(0.4, 0.1+depth*0.018)`, attacchi dedicati (balzo del
  serpente, picchiata dell'arpia, magia di cultista/sciamano, doppio
  fendente della mantide, pestone del golem, fendente a cono del cavaliere),
  veleno, furto vita, scissione delle gelatine.
- **Generazione dungeon identica**: `w=clamp(78+d*3,78,130)`,
  `h=clamp(44+d*2,44,74)`, stanze con overlap check, corridoi L, porte,
  BFS per le scale lontane, forzieri `3+d/2`, tesori `4+d/2`, pozioni,
  spot potenziamenti, torce, zona sicura col mercante.
- **Boss ogni 5 piani** nell'ordine originale (Drago, Golem di Pietra,
  Lich, Regina delle Melme, Re Ragno, Re dei Ratti) con tana-arena scolpita,
  cancelli che si sigillano all'ingresso, macchina a stati completa
  (soffio, palla di fuoco, volo+picchiata, evocazioni scalate 45% HP/70%
  danno, carica, pestone) e forziere leggendario sigillato.
- **Equipaggiamento** a 5 slot con rarità comune→leggendario (pesi che
  crescono col piano), statistiche `hp/dmg%/speed%/armor%`, auto-equip
  se migliore o scomposisto in oro; premi epici/leggendari dai forzieri boss.
- **Mercante** con prezzi `12+d*2 / 45+d*8 / 20+d*4`, potenziamenti a sorte.
- **Permadeath**: alla morte si perdono oro/pozioni/equip; record oro+piano
  salvato su memory stick (`ms0:/ABISSO/RECORD.BIN`) come il localStorage.
- **Fog of war** a raggi Bresenham (raggio 7.4), torce tremolanti, luce
  calda del giocatore, dissolvenza ai cambi di piano, screen shake,
  hitstop sui colpi pesanti, particelle, testi di danno flottanti,
  minimappa con faro della tana del boss, barra boss, toast e log.
- Respawn mostri ogni `13+rnd*11s` con cap `min(30, 9+2d)` e potenziamenti
  periodici, discese con guardia di 4 secondi, sfondo che vira col piano.

## Differenze dichiarate (hardware/network)

- **Multiplayer**: il WebRTC/Trystero dell'originale non esiste su PSP; è
  sostituito dal netcode nativo descritto sopra (Adhoc PSP-PSP e UDP PSP-PC).
  Chat vocale non inclusa; chat testuale via protocollo `CHAT`.
- **Modalità prima persona/isometrica (tasto V)** e modalità C64 non incluse;
  vista topdown come il gioco principale.
- Audio sintetizzato al volo (come le musiche WebAudio generate) via synth
  interno square/triangle/noise.
- Zoom fisso (22px/cella), niente zoom rotellina né console segreta Ctrl+Shift+D.

## Crediti

Gioco originale **Abisso 2.0** di [SancioPanza88](https://github.com/SancioPanza88)
(giocabile su [youdev.it](https://www.youdev.it/games/abisso.html)).
Tutti gli asset grafici appartengono al progetto originale.
