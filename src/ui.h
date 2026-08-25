#ifndef ABISSO_UI_H
#define ABISSO_UI_H

/* HUD e schermate (equivalente dei pannelli DOM dell'originale) */

void uiRenderTitle(float blinkT);
void uiRenderClassSelect(int sel);

/* HUD di gioco: barre, oro, pozioni, abilita', buff, minimappa,
   barra boss, toast, banner, log */
void uiRenderGameHud(void);

void uiRenderDownedOverlay(void);
void uiRenderDeadOverlay(void);

/* pannello del mercante */
int  uiMerchantActive(void);
void uiOpenMerchant(void);
void uiCloseMerchant(void);
void uiMerchantMove(int delta);
void uiMerchantBuy(void);
void uiRenderMerchantPanel(void);

#endif
