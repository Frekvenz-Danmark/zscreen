/* GENERERET af tools/build-assets.sh. Ret den ikke. */
/*
 * Linjehoejderne som lv_font_conv selv har regnet dem, for
 * saettet til 480 px bredde. zs_layout.h reserverer
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
#define ZS_SKRIFT_SAET_WIDTH 480
#define ZS_LH_TAL      54
#define ZS_LH_STOR     34
#define ZS_LH_RAD      26
#define ZS_LH_BROED    20
#define ZS_LH_ETIKET   17
#define ZS_LH_KB       28
#define ZS_LH_IKON_LILLE 20
#define ZS_LH_IKON_STOR 28
#define ZS_LH_IKON_KAEMPE 24
#endif /* ZS_SKRIFT_MAAL_INC */
