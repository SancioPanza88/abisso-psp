#ifndef ABISSO_AUDIO_H
#define ABISSO_AUDIO_H

/* Audio generato al volo come l'originale (WebAudio -> synth PSP):
   oscillatori square/triangle + noise con decadimento esponenziale,
   piu' un tappeto armonico ambient che cambia col piano. */

enum {
    SFX_STEP=0, SFX_SWING, SFX_HIT, SFX_CRIT, SFX_HURT,
    SFX_SHOOT_MAGE, SFX_SHOOT_RANGER, SFX_SHOOT_PLASMA,
    SFX_POTION, SFX_MANA, SFX_PICKUP, SFX_GEM, SFX_POWER,
    SFX_CHEST, SFX_STAIR, SFX_BOOM, SFX_KILL,
    SFX_BOSS_ROAR, SFX_BOSS_KILL, SFX_DEATH, SFX_ABILITY, SFX_CLICK,
    SFX_COUNT
};

void audioInit(void);
void sfxPlay(int id);
void musicSetDepth(int depth);
int  audioIsMuted(void);
void audioToggleMute(void);

#endif
