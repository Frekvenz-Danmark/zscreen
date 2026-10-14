/* GENERERET af tools/build-assets.sh. Ret den ikke. */
/*
 * Linjehoejderne som lv_font_conv selv har regnet dem, for
 * saettet til 800 px bredde. zs_layout.h reserverer
 * plads efter DE HER tal og ikke efter et skaleret
 * overslag - se ZS_G_CARD_SUB_HEIGHT.
 */
/* Guarden slutter paa _INC og ikke _H: et navn der
 * slutter paa _H er forbeholdt headere der ligger
 * hvor tools/check-headers.sh kigger, og den her
 * ligger dybere. Se husreglen i check-headers.sh. */
#ifndef ZS_SKRIFT_MAAL_INC
#define ZS_SKRIFT_MAAL_INC
/* Hvilken skaermbredde saettet er bygget til. zs_layout.h
 * holder den op mod ZS_G_SCR_WIDTH, saa en skaerm der
 * peger paa det forkerte saet ikke bygger. Vaelger
 * ZDisplay saettet til 480, bliver skriften en tredjedel
 * for lille, og INTET andet siger fra: layoutet regner
 * videre paa 800, kortene bliver store og tomme, og det
 * ser ud som et designvalg. */
#define ZS_SKRIFT_SAET_WIDTH 800
#define ZS_LH_TAL      88
#define ZS_LH_STOR     60
#define ZS_LH_RAD      42
#define ZS_LH_BROED    33
#define ZS_LH_ETIKET   29
#define ZS_LH_KB       47
#endif /* ZS_SKRIFT_MAAL_INC */
