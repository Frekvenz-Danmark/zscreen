#!/usr/bin/env python3
"""
zScreen - ingen raa pixeltal i fladens geometri.

HVORFOR DEN FINDES.

Fladen blev tegnet paa 480 x 480, og tallene stod som de var: 14 px her,
26 px der, "(ZS_BAR_HEIGHT - 20) / 2" for at centrere noget der var 20
px hoejt. Det virkede paa den skaerm de blev skrevet til.

Da den samme flade blev bygget til 800 x 1280, fulgte kasserne med og
tallene inde i dem stod stille. Maalt paa statuslinjen:

  * maerket "Forbundet" startede paa x = 14 + 26 + 10 = 50, mens logoet
    fyldte 14..57. Det OVERLAPPEDE logoet med 7 px
  * wifi-ikonet var centreret som om det var 20 px hoejt. Det er 33, saa
    det sad 6 px FOR LAVT
  * tandhjulet var 44 x 44 i en linje paa 73, placeret i y = 0, altsaa
    FOR HOEJT - og det raakte 19 px ud over hoejre kant, fordi x blev
    regnet med 44 og knappen tegnet med 73
  * klokkeslaettet stod i y = 10, midten af en linje der ikke findes
    laengere

Alle fire er samme fejl: et tal der kun gaelder én skaerm, skrevet ind i
et kald. Vagten her finder dem FOER de bliver til noget man kan se.

REGLEN: et tal i et geometri-kald skal komme fra zs_layout.h eller vaere
pakket i ZS_G_SK, ZS_G_SK_WIDTH eller ZS_G_SK_HEIGHT. 0, 1 og 2 er
undtaget - de er sjaeldent et maal, og en ramme paa 1 px er en ramme paa
1 px paa enhver skaerm.

Koeres af tools/check-alle.sh.
"""
import pathlib
import re
import sys

ROD = pathlib.Path(__file__).resolve().parent.parent
UI = ROD / "firmware/kerne/ui"
GROEN, ROED, SLUT = "\033[1;32m", "\033[1;31m", "\033[0m"

# De kald hvor et tal ER et maal paa skaermen.
KALD = re.compile(
    r"\b(lv_obj_set_pos|lv_obj_set_size|lv_obj_set_width|lv_obj_set_height"
    r"|lv_obj_align|lv_obj_align_to|lv_obj_set_style_pad_\w+"
    r"|lv_obj_set_style_radius|lv_obj_set_style_border_width"
    r"|lv_obj_set_style_translate_\w+|lv_obj_set_style_size"
    r"|lv_obj_set_style_line_width|lv_obj_set_style_outline_width"
    # OG DE TO HELT ALMINDELIGE. Listen havde _radius, _border_width,
    # _line_width, _outline_width, _max_width og _min_width - men ikke
    # det bare _width og _height den sad lige ved siden af. Resultatet
    # var en scrollbar paa 4 raa pixels i zs_theme.c, som blev 4 paa en
    # 800x1280 skaerm hvor alt andet var skaleret med 1,67 - og vagten
    # meldte "ingen af dem har et tal der kun passer paa én skaerm".
    r"|lv_obj_set_style_width|lv_obj_set_style_height"
    # Dertil de maal der ogsaa er pixels, og som blev glemt foerste
    # gang: buens tykkelse paa en spinder, skyggen, afstanden mellem
    # bogstaver og mellem linjer, og graenserne for et objekts stoerrelse.
    r"|lv_obj_set_style_arc_width|lv_obj_set_style_shadow_\w+"
    r"|lv_obj_set_style_text_letter_space|lv_obj_set_style_text_line_space"
    r"|lv_obj_set_style_max_width|lv_obj_set_style_min_width"
    r"|lv_obj_set_style_max_height|lv_obj_set_style_min_height)\s*\(",
    re.S)
TAL = re.compile(r"(?<![\w.])(\d+)(?![\w.])")

# 0, 1 og 2 er ikke maal man skalerer.
SMAA = {0, 1, 2}

# Det der er pakket ind, eller kommer fra layoutet, taeller ikke med.
FRI = (
    # PROCENT ER IKKE PIXELS, og den her skal staa FOERST.
    #
    # LV_PCT(100) betyder "hele foraeldrens bredde". Moensteret for
    # LV_-navne nedenfor fjerner macroens NAVN og lader argumentet staa,
    # saa 100 saa ud som et raa pixeltal. Det blev pakket ind, og 100
    # procent blev 166 procent - et kort halvanden gang bredere end
    # siden. Raekkefoelgen her ER rettelsen.
    re.compile(r"\b(?:LV_PCT|lv_pct)\s*\([^()]*\)"),
    re.compile(r"ZS_G_SK(?:_WIDTH|_HEIGHT)?\([^()]*(?:\([^()]*\)[^()]*)*\)"),
    # ET TAL FORAN ET GANGETEGN ER EN FAKTOR, IKKE EN LAENGDE.
    #
    # "ZS_SCR_WIDTH - 4 * ZS_EDGE" betyder "fire kanter". Da de raa tal
    # blev pakket ind, blev de fire til ZS_G_SK_WIDTH(4), altsaa SEKS
    # kanter paa den store skaerm - en bredde der var 2 kanter for lille.
    # Fire af dem stod i koden. Tallet er et ANTAL, og antal skaleres
    # ikke; det er ZS_EDGE selv der goer det.
    re.compile(r"\b\d+\s*\*\s*ZS_[A-Z_0-9]+"),
    re.compile(r"ZS_[A-Z_0-9]+\s*\*\s*\d+\b"),
    re.compile(r"\bZS_[A-Z_0-9]+\b"),
    re.compile(r"\bLV_[A-Z_0-9]+\b"),
)

fejl = []


def argumenter(tekst, i):
    """Teksten mellem parenteserne i et kald der starter paa i."""
    dybde = 0
    start = tekst.index("(", i)
    for j in range(start, len(tekst)):
        if tekst[j] == "(":
            dybde += 1
        elif tekst[j] == ")":
            dybde -= 1
            if dybde == 0:
                return tekst[start + 1:j]
    return ""


def uden_kommentarer(t):
    """Fjerner kommentarer UDEN at flytte linjenumrene.

    Her stod der bare "", og en blokkommentar over tolv linjer blev
    derfor til ingenting - saa alt under den rykkede tolv linjer op.
    Vagten pegede paa en linje der ikke var den den mente, og i den
    her kodebase er en blokkommentar paa tyve linjer det normale.
    En vagt der sender dig det forkerte sted hen, koster mere end den
    sparer.

    Nu beholder hver kommentar sine linjeskift, saa nummeret passer med
    filen.
    """
    def nyline(m):
        return "\n" * m.group(0).count("\n")
    t = re.sub(r"/\*.*?\*/", nyline, t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


# ---------------------------------------------------------------------
# Raa pixeltal der har faaet et NAVN.
#
# Vagten ovenfor kigger paa KALDENE. Det lukkede det ene hul og lod det
# andet staa aabent: et tal der staar i et #define, bliver aldrig set,
# fordi kaldet jo bruger et navn.
#
# Det kostede fire fejl paa ZDisplay, alle fundet paa glasset og ikke
# her:
#     ZS_KB_HEIGHT   224  - tastaturet var 224 px paa en 1280 px skaerm
#     DOT_SIZE        10  - prikkens fingerflade blev 38 px bred
#     DOT_GAP         30
#     ZS_INV_LIST_Y   56
#
# Reglen: et #define i fladen med et tal i, skal pakke tallet i en af
# skaleringsmakroerne eller henvise til noget i zs_layout.h. Er tallet
# ikke pixels, staar navnet i UNDTAGET nedenfor - og saa staar der
# ogsaa HVORFOR.
# ---------------------------------------------------------------------

DEFINE = re.compile(r"^#define\s+([A-Z_][A-Z0-9_]*)\s+(-?\d+)\s*(?:/\*|//|$)",
                    re.M)

# ET RAAT TAL INDE I ET REGNESTYKKE.
#
# DEFINE ovenfor rammer kun "#define NAVN <bart tal>". Derfor slap
#
#     #define CARD_W  ((ZS_CONTENT_WIDTH - 12) / 2)
#
# igennem: de to priskort stod 12 px fra hinanden paa BEGGE skaerme,
# mens hvert andet par kort paa enheden bruger ZS_G_GRID_GAP, som er
# 20 paa ZDisplay. Det var den eneste af sin slags tilbage i fladen,
# og den laa i en fil der lige var gennemgaaet.
# ENDELSER DER IKKE ER PIXELS - OG DE SKAL STAA TIL SIDST I NAVNET.
#
# HER STOD "if any(o in navn for o in (...))", altsaa et DELSTRENGS-
# tjek, og listen stod TO steder. Efterproevet 17. oktober 2026 mod
# vagtens eget moenster:
#
#     KORT_MINDSTE_HEIGHT   fritaget af _MIN   (dansk _MINDSTE)
#     FOO_VERDI             fritaget af _VER   (dansk _VERDI)
#     BAR_MAX_W             fritaget af _MAX   (men _W ER pixels)
#     PANEL_ID_X            fritaget af _ID    (men _X ER pixels)
#
# Alle fire blev sprunget over i TAVSHED: ingen UNDTAGET-post, ingen
# skreven grund, intet uddata. I en dansk navngivet kodebase er
# _MINDSTE, _VERDI og _MAALT naturlige navne, saa listen var praecis
# det "sted man skriver sig fri" som filens eget hoved advarer mod.
#
# Som endelse betyder _MAX "et maksimum af noget der ikke er pixels",
# fx ZS_DISCOVERY_MAX. BAR_MAX_W slutter paa _W og meldes nu.
IKKE_PIXELS = ("_MAX", "_MIN", "_MS", "_COUNT", "_LEN", "_SEK",
               "_ANTAL", "_BIT", "_OPA", "_ID", "_VER")


def ikke_pixels(navn):
    """Slutter navnet paa en endelse der ikke er pixels?"""
    return navn.endswith(IKKE_PIXELS)


# HER KRAEVEDE MOENSTERET AT VAERDIEN STARTEDE MED "(".
#
# Noten ovenfor sagde at DEFINE_UDTRYK lukkede hullet
# "CARD_W ((ZS_CONTENT_WIDTH - 12) / 2)". Den lukkede kun den
# PARENTETISEREDE form. Efterproevet 17. oktober 2026:
#
#     #define FOO 224              DEFINE ser den
#     #define FOO ((A - 12) / 2)   UDTRYK ser den
#     #define FOO 12 * 2           INGEN ser den
#     #define FOO ZS_EDGE + 24     INGEN ser den
#     #define FOO 224 + 0          INGEN ser den
#
# Og "#define X ZS_EDGE + 24" er netop den form der laa i
# zs_price_page.c. Nu tages hele vaerdien. Det giver ikke dubletter med
# DEFINE ovenfor: udtrukket nedenfor tager KUN tal der laegges til eller
# traekkes fra og ikke selv ganger, saa et bart tal matcher ikke.
DEFINE_UDTRYK = re.compile(
    r"^#define\s+([A-Z_][A-Z0-9_]*)\s+(\S.*)$", re.M)

# Navne hvor tallet IKKE er pixels. Hver med sin grund, for en liste
# uden grunde bliver et sted man skriver sig fri.
UNDTAGET = {
    "ZS_G_KB_RAEKKER":  "antal raekker paa tastaturet",
    "ZS_G_CARD_KANT":   "kantens tykkelse i px, og den skalerer ikke",
    "ZS_G_FIELD_KANT":  "samme",
    "ZS_G_CHOICE_KANT": "samme",
    "ZS_G_TOUCH_MIN":   "en finger bliver ikke stoerre af en stoerre skaerm",
    "ZS_G_REF_WIDTH":   "referenceskaermens maal, selve grundlaget",
    "ZS_G_REF_HEIGHT":  "samme",
    "ZS_G_SCR_WIDTH":   "skaermens eget maal",
    "ZS_G_SCR_HEIGHT":  "samme",
    "PAGE_COUNT":       "antal sider paa forsiden",
    "MAKS_DYBDE":       "hvor dybt maalebaandet gaar ned",
    "ZS_G_GUIDE_CARDS":   "antal kort paa kom-i-gang-siden",
    "ZS_QR_KANT_MODULER": "QR-moduler, ikke pixels - de bliver pixels "
                          "naar koden skaleres",
    "ZS_QR_MAKS_UDGAVE":  "QR-kodens udgavenummer",
}


# ---------------------------------------------------------------------
# Raa pixeltal i en lv_coord_t.
#
# Det tredje hul. Vagten saa kaldene og #define'erne, men ikke det her:
#
#     lv_coord_t x = (icon != NULL && icon[0] != '\0') ? (20 + 6) : 0;
#
# Tallet staar i en almindelig tildeling, og kaldet bagefter bruger
# bare "x". 20 er ikonets bredde og 6 er luften efter det - paa
# zScreen. Paa ZDisplay er ikonet 33 px bredt, saa teksten laa oven i
# pilen i baade NETTET- og BATTERI-kortet. Brugeren saa det paa
# glasset; vagten saa ingenting.
#
# lv_coord_t ER LVGLs pixeltype. Et raat tal der tildeles den, er et
# maal - der er ikke andet det kan vaere.
# ---------------------------------------------------------------------

# To moenstre, og det andet kom til fordi det foerste kun saa
# ERKLAERINGER.
#
#     lv_coord_t title_x = ZS_EDGE + ZS_G_SK(4);   <- blev set
#     title_x = 4 + ZS_TOUCH_MIN + 8;              <- blev IKKE set
#
# De to linjer stod to linjer fra hinanden i zs_theme.c. Den foerste
# blev pakket i ZS_G_SK, den anden beholdt sine raa tal - og vagten der
# var lavet til netop den fejl, kunne ikke se den.
#
# Gentildelingen kendes paa navnet: vi samler navnene paa alle
# lv_coord_t i filen og leder efter "navn = ..." uden type foran.
COORD = re.compile(r"\blv_coord_t\s+(\w+)\s*=\s*([^;]+);")
COORD_NAVNE = re.compile(r"\blv_coord_t\s+(\w+)")


def tjek_coord(tekst, fil):
    """Raa tal der tildeles en lv_coord_t - ogsaa ved gentildeling."""
    ud = []
    navne = set(COORD_NAVNE.findall(tekst))
    traef = list(COORD.finditer(tekst))
    # Gentildelinger: "navn = ..." uden en type foran. Kun navne vi
    # VED er lv_coord_t i den her fil, saa et tilfaeldigt "i = 4" ikke
    # taeller med.
    if navne:
        mellem = "|".join(re.escape(n) for n in sorted(navne))
        gen = re.compile(rf"^\s*({mellem})\s*=\s*([^;]+);", re.M)
        traef += [m for m in gen.finditer(tekst)]
    for m in traef:
        udtryk = m.group(2)
        rest = udtryk
        for r in FRI:
            rest = r.sub("", rest)
        raa = sorted({int(d.group(1)) for d in TAL.finditer(rest)} - SMAA)
        if raa:
            nr = tekst[:m.start()].count("\n") + 1
            ud.append(
                f"{fil}:{nr}: en lv_coord_t faar de raa pixeltal {raa}. "
                f"Pak dem i ZS_G_SK (former), ZS_G_SK_WIDTH (bredder) "
                f"eller ZS_G_SK_HEIGHT (hoejder) - eller giv dem et navn "
                f"i zs_layout.h")
    return ud


def referencegrene(tekst):
    """Hvor i teksten gaelder 'det her er referenceskaermen'.

    zs_layout.h har nogle faa steder formen

        #if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH && ...
            <zScreens gamle tal, raa og med vilje>
        #else
            <regnestykket>
        #endif

    Tallene i den foerste gren er de tal zScreen HAR, og de maa ikke
    laegges om - det er hele grunden til at grenen findes. Vagten ser
    derfor forbi dem, og kun dem.
    """
    ud = []
    i = 0
    while True:
        a = tekst.find("#if ZS_G_SCR_WIDTH == ZS_G_REF_WIDTH", i)
        if a < 0:
            return ud
        e = tekst.find("#else", a)
        slut = tekst.find("#endif", a)
        if e < 0 or (0 <= slut < e):
            e = slut if slut >= 0 else len(tekst)
        ud.append((a, e))
        i = e


def tjek_defines():
    """Raa tal der staar i et #define i stedet for i et kald."""
    antal = 0
    for f in sorted(UI.rglob("*.c")) + sorted(UI.rglob("*.h")):
        if "/assets/" in f.as_posix():
            continue
        tekst = f.read_text(encoding="utf-8")
        fri = referencegrene(tekst)
        for m in DEFINE.finditer(tekst):
            navn, vaerdi = m.group(1), int(m.group(2))
            antal += 1
            if navn in UNDTAGET or abs(vaerdi) in SMAA:
                continue
            # Inde i grenen for referenceskaermen ER et raat tal det
            # rigtige: det er zScreens gamle tal, skrevet af med vilje
            # saa fladen ikke flytter sig. Se ZS_G_CARD_KANT.
            if any(a <= m.start() < b for a, b in fri):
                continue
            # Tal der tydeligt ikke er pixels.
            if ikke_pixels(navn):
                continue
            nr = tekst[:m.start()].count("\n") + 1
            fejl.append(
                f"{f.relative_to(ROD)}:{nr}: {navn} er {vaerdi}, et raat "
                f"pixeltal med et navn paa. Pak det i ZS_G_SK (former), "
                f"ZS_G_SK_WIDTH (bredder) eller ZS_G_SK_HEIGHT (hoejder) "
                f"- er det ikke pixels, saa skriv navnet i UNDTAGET i "
                f"check-flade.py med en grund")

        # Og de samme tal, men inde i et regnestykke.
        for m in DEFINE_UDTRYK.finditer(tekst):
            navn, udtryk = m.group(1), m.group(2)
            # KOMMENTAREN ER IKKE EN DEL AF VAERDIEN.
            #
            # Moensteret ovenfor tager hele resten af linjen, og det
            # SKAL det for at se "#define X ZS_EDGE + 24". Men saa
            # kommer en forklarende kommentar med, og tallene DER er
            # ikke kode. Maalt da moensteret blev udvidet 17. oktober
            # 2026, to falske fund paa én gang:
            #
            #   ZS_G_BAR_BADGE_HOEJDE  ZS_G_SK(22)  /* 16 px + 3 px luft */
            #   ZS_CARD_HEIGHT         ZS_G_CARD_HEIGHT /* (408-12-12-12)/2 */
            #
            # Begge vaerdier er korrekt skalerede; tallene stod i
            # noterne. En vagt der melder forkert, bliver slaaet fra.
            udtryk = re.sub(r"/\*.*?\*/", " ", udtryk)
            udtryk = re.sub(r"//.*$", " ", udtryk)
            if navn in UNDTAGET:
                continue
            if any(a <= m.start() < b for a, b in fri):
                continue
            if ikke_pixels(navn):
                continue
            # HVAD DER ER PIXELS OG HVAD DER ER ET ANTAL.
            #
            # "/ 2" deler i to spalter og "3 * ZS_G_GRID_GAP" er tre
            # mellemrum - ingen af dem er en afstand. Kun et tal der
            # laegges til eller traekkes fra OG ikke selv ganger noget,
            # er pixels:
            #
            #   (ZS_CONTENT_WIDTH - 12) / 2        12 er pixels
            #   (ZS_G_PAGE_RAA - 3 * ZS_G_GRID_GAP)  3 er et antal
            #
            # Foerste udgave af den her vagt tog begge, og meldte tre
            # fund der alle var antal. En vagt der melder forkert, bliver
            # slaaet fra.
            raa = sorted({int(d) for d in
                          re.findall(r"[-+]\s*(\d+)(?!\s*[*\d])", udtryk)}
                         - SMAA)
            if not raa:
                continue
            nr = tekst[:m.start()].count("\n") + 1
            fejl.append(
                f"{f.relative_to(ROD)}:{nr}: {navn} laegger til eller "
                f"traekker fra med {raa}, altsaa raa pixels inde i et "
                f"regnestykke. Pak dem i ZS_G_SK (former), "
                f"ZS_G_SK_WIDTH (bredder) eller ZS_G_SK_HEIGHT "
                f"(hoejder) - eller giv dem et navn i zs_layout.h")
    return antal


def tjek_zscreen_staar_stille():
    """Fejlsidens opstilling skal stadig have referencegrenen.

    HVORFOR DEN HER VAGT IKKE KAN VAERE EN _Static_assert ELLER EN
    FIKSTUR.

    Opstillingen af fejlsiden er C-kode i stak_op(), ikke en makro.
    tests/fixtures/layout-480.h fanger MAKROER, saa da overskriften blev
    lavet om fra en fast y til en maalt stak, flyttede zScreens
    overskrift 41 px uden at én eneste proeve sagde fra - de gamle tal
    stod der jo stadig, de blev bare ikke laest laengere.

    Her tjekkes derfor at grenen ER der. Forsvinder den, maaler
    referenceskaermen med, og saa flytter hver skaerm i marken sig.
    """
    f = UI / "zs_screen_fejl.c"
    if not f.exists():
        fejl.append("tools/check-flade.py: zs_screen_fejl.c findes ikke "
                    "laengere - flyt vagten med, eller tag den ud")
        return
    tekst = f.read_text(encoding="utf-8")
    kode = uden_kommentarer(tekst)
    # DEN NEGEREDE FORM, og ikke bare navnet et sted i filen.
    #
    # Foerste udgave ledte efter "ZS_G_FEJL_MAAL_STAK". En mutation der
    # skiftede "#if !ZS_G_FEJL_MAAL_STAK" ud med "#if 0" slap igennem,
    # fordi navnet stadig stod i filen et andet sted. Det er GRENEN der
    # skal vaere der, og den kendes paa udraabstegnet.
    if "#if !ZS_G_FEJL_MAAL_STAK" not in kode:
        fejl.append(
            f"{f.relative_to(ROD)}: opstillingen af fejlsiden spoerger "
            f"ikke laengere paa ZS_G_FEJL_MAAL_STAK. Uden den gren "
            f"maaler ogsaa referenceskaermen, og zScreens overskrift "
            f"flytter sig 41 px paa hver skaerm der staar hos en kunde")
        return
    # Og de gamle tal skal stadig LAESES et sted, ellers er grenen tom.
    for navn in ("ZS_G_FEJL_TOP", "ZS_G_FEJL_BODY_Y", "ZS_G_FEJL_ITOP",
                 "ZS_G_FEJL_IKON_Y", "ZS_G_FEJL_IBODY_Y"):
        if navn not in kode:
            fejl.append(
                f"{f.relative_to(ROD)}: {navn} laeses ikke laengere. "
                f"Det er zScreens gamle plads, og bliver den ikke brugt, "
                f"staar tallet der kun for at se rigtigt ud")


# SIDER SELVTESTEN MED VILJE IKKE TEGNER.
#
# Listen ALLE[] i zs_selftest.c er haandholdt. Da Avancerede
# indstillinger kom til, blev den glemt - saa den nye side blev hverken
# bygget, tegnet eller maalt for laekager i de seks temaskift, og
# INGENTING sagde fra.
#
# De fire her stod heller ikke paa listen, og der staar ingen grund
# nogen steder. De er skrevet op saa manglen er SYNLIG og kan tages
# stilling til, i stedet for at vaere et hul man ikke kan se. En ny
# side kommer ikke ind her af sig selv - den faar proeven til at falde,
# og det er meningen.
# ALLE SIDER TEGNES NU. Her stod fire navne med "ingen har sagt
# hvorfor" - GUIDE, INVERTER_MANUAL, THEME og TILMELD. Svaret blev
# fundet 15. oktober 2026: der VAR ingen grund, listen var bare
# haandholdt. De er med nu, og ordbogen staar tom med vilje: skal en
# side undtages, skal der staa hvorfor.
UDEN_SELVTEST = {}


def tjek_480_skriftmaal():
    """Raa skriftmaal i referencegrenen skal passe med 480-saettet.

    HVORFOR DEN FINDES.

    zs_skrift_maal.h inkluderes IKKE paa referenceskaermen - de frosne
    tal skal staa alene, se noten ved ZS_G_CARD_KANT. Saa en
    linjehoejde dér er et tal skrevet i haanden, og et skrevet tal kan
    vaere skrevet af den forkerte skrift.

    DET VAR DET. ZS_G_FEJL_IKON_HEIGHT stod 28, altsaa zs_ikon_stor's
    hoejde - men ikonet tegnes med zs_ikon_kaempe, som er 24. Ikonet
    fik 28 px reserveret og fylder 24, saa stakken stod 4 px skaevt.
    Og vagterne i zs_layout.h lod det passere, fordi de maalte paa
    samme forkerte tal.

    Her sammenlignes de skrevne tal med den GENEREREDE header for 480.
    """
    lay = UI / "zs_layout.h"
    maal = UI / "assets/480/zs_skrift_maal.h"
    if not lay.exists() or not maal.exists():
        return
    genereret = dict(re.findall(r"^#define\s+(ZS_LH_\w+)\s+(\d+)",
                                maal.read_text(encoding="utf-8"), re.M))
    tekst = lay.read_text(encoding="utf-8")
    # (makro i zs_layout.h, hvilken skrift den SKAL vaere)
    PAR = (("ZS_G_FEJL_IKON_HEIGHT", "ZS_LH_IKON_KAEMPE"),)
    for makro, skrift in PAR:
        m = re.search(rf"^#define\s+{makro}\s+(\d+)", tekst, re.M)
        if m is None:
            continue                  # staar som et udtryk, ikke et tal
        skrevet, rigtigt = int(m.group(1)), genereret.get(skrift)
        if rigtigt is None:
            fejl.append(f"tools/check-flade.py: {skrift} findes ikke i "
                        f"480-saettets zs_skrift_maal.h")
            continue
        if skrevet != int(rigtigt):
            nr = tekst[:m.start()].count("\n") + 1
            fejl.append(
                f"firmware/kerne/ui/zs_layout.h:{nr}: {makro} er "
                f"{skrevet}, men {skrift} er {rigtigt} i 480-saettet. "
                f"Referencegrenen inkluderer ikke den genererede "
                f"header, saa tallet staar skrevet - og et skrevet tal "
                f"kan vaere skrevet af den forkerte skrift")


def tjek_alle_sider():
    """Hver side i zs_screen_id_t skal tegnes af selvtesten."""
    enum_fil = UI / "zs_ui.h"
    test_fil = ROD / "firmware/kerne/app/zs_selftest.c"
    if not enum_fil.exists() or not test_fil.exists():
        return
    enum_tekst = enum_fil.read_text(encoding="utf-8")
    m = re.search(r"typedef\s+enum\s*\{(.*?)\}\s*zs_screen_id_t",
                  enum_tekst, re.S)
    if m is None:
        fejl.append("tools/check-flade.py: kunne ikke finde "
                    "zs_screen_id_t i zs_ui.h - er den skrevet om?")
        return
    sider = re.findall(r"^\s*(ZS_SCREEN_[A-Z_0-9]+)", m.group(1), re.M)
    if not sider:
        fejl.append("tools/check-flade.py: zs_screen_id_t ser tom ud")
        return

    test_kode = uden_kommentarer(test_fil.read_text(encoding="utf-8"))
    mm = re.search(r"ALLE\[\]\s*=\s*\{(.*?)\};", test_kode, re.S)
    if mm is None:
        fejl.append("firmware/kerne/app/zs_selftest.c: fandt ikke ALLE[]")
        return
    paa_listen = set(re.findall(r"ZS_SCREEN_[A-Z_0-9]+", mm.group(1)))

    for side in sider:
        if side in paa_listen or side in UDEN_SELVTEST:
            continue
        fejl.append(
            f"firmware/kerne/app/zs_selftest.c: {side} staar i "
            f"zs_screen_id_t men ikke i ALLE[], saa selvtesten bygger "
            f"den aldrig - hverken for tegnefejl eller for laekager "
            f"efter et temaskift. Tilfoej den, eller skriv den i "
            f"UDEN_SELVTEST i check-flade.py med en grund")
    # Og en side der er fjernet fra enummet skal ud af undtagelserne
    # igen, ellers vokser listen med navne der ikke findes.
    for navn in sorted(UDEN_SELVTEST):
        if navn not in sider:
            fejl.append(
                f"tools/check-flade.py: UDEN_SELVTEST naevner {navn}, "
                f"som ikke findes i zs_screen_id_t laengere. Tag den ud")


def main():
    print("Tjekker at fladen ikke har raa pixeltal ...")
    if not UI.is_dir():
        print(f"  {ROED}FEJL{SLUT} {UI.relative_to(ROD)} findes ikke")
        return 1

    kald = 0
    for f in sorted(UI.rglob("*.c")):
        if "/assets/" in f.as_posix():
            continue
        tekst = uden_kommentarer(f.read_text(encoding="utf-8"))
        fejl.extend(tjek_coord(tekst, f.relative_to(ROD)))
        linjer = tekst.split("\n")
        for m in KALD.finditer(tekst):
            kald += 1
            arg = argumenter(tekst, m.start())
            rest = arg
            for r in FRI:
                rest = r.sub("", rest)
            raa = sorted({int(d.group(1)) for d in TAL.finditer(rest)}
                         - SMAA)
            if raa:
                nr = tekst[:m.start()].count("\n") + 1
                navn = m.group(1).replace("lv_obj_", "")
                fejl.append(
                    f"{f.relative_to(ROD)}:{nr}: {navn}() har raa "
                    f"pixeltal {raa}. Pak dem i ZS_G_SK (former), "
                    f"ZS_G_SK_WIDTH (bredder) eller ZS_G_SK_HEIGHT "
                    f"(hoejder) - eller giv dem et navn i zs_layout.h")

    defs = tjek_defines()
    tjek_zscreen_staar_stille()
    tjek_alle_sider()
    tjek_480_skriftmaal()

    if fejl:
        for e in fejl:
            print(f"  {ROED}FEJL{SLUT} {e}")
        print(f"\n  {len(fejl)} steder at rette.")
        return 1

    print(f"  {GROEN} OK{SLUT} {kald} geometri-kald og {defs} #define i "
          f"fladen, ingen af dem har et tal der kun passer paa én "
          f"skaerm.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
