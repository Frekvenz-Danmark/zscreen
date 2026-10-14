# Versionsnummeret, ét sted for alle produkter.
#
# HVORFOR DEN LIGGER FOR SIG SELV OG IKKE I kilder.cmake.
#
# Et boards PROJEKTfil skal have versionen for at kunne kalde
# project(... VERSION ...). Dens main-KOMPONENT skal have kildelisten.
# Det er to forskellige ting paa to forskellige tidspunkter, og de to
# filer kan ikke se hinandens variabler: en komponent bliver behandlet i
# sit eget omfang, saa et "set()" i projektfilen naar ikke derind. Det
# blev maalt - byggeriet faldt paa at ZS_ASSET_SAET ikke var sat.
#
# Derfor: versionen her, kildelisten i kilder.cmake, og hver fil hentes
# der hvor den bruges.

# ESP-IDF laeser normalt version.txt fra PROJEKTETS mappe. Med to
# produkter ville det vaere to filer, og de ville komme ud af trit: de
# to udgives med SAMME versionsmaerke fra samme repo.
#
# Her stod der foer ingenting for ZDisplay, og saa faldt den tilbage paa
# "git describe". Det gav "v0.27.0-dirty" i en byggemappe med
# uncommitterede aendringer, og det er IKKE et versionsnummer
# zs_version_parse kan laese. Konsekvensen ville vaere en skaerm der
# aldrig opdaterer, fordi den ikke forstaar sin egen version.
#
# Derfor: ét version.txt i firmware/, laest her, sat for begge.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../version.txt" ZS_VERSION LIMIT_COUNT 1)
string(STRIP "${ZS_VERSION}" ZS_VERSION)
if(NOT ZS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "firmware/version.txt siger '${ZS_VERSION}'. Det skal vaere tre "
        "tal med punktummer, fx 0.27.0 - ellers kan skaermen ikke "
        "sammenligne sin egen version med et maerke paa GitHub, og saa "
        "opdaterer den aldrig.")
endif()

