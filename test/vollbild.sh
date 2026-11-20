#!/bin/bash
# Vollbild (API 2.3, PLAN_32BIT Schritt 2), QEMU VGA: Shell startet VOLLTEST (als ZVOLL.EXE) aus dem Ordner A:\RD2\:
# 1. Modus 13h mit Farbverlauf (256 Farben), 2. Alt+Tab: Schreibtisch 640x480, Programm bekommt M_SCREENLOST,
# 3. Alt+Tab: zurueck, M_SCREENBACK, Programm zeichnet neu (Modus und Palette wie vorher), 4. Esc: ScreenRelease, Schreibtisch.
# Bilder obj/voll{1,2,3,4}.ppm.
cd "$(dirname "$0")/.."
IMG=obj/vollbild.img; SER=obj/vollbild_com1.txt; SOCK=/tmp/qmon_voll.sock; F=0
Q="python3 test/qemu/qmon.py"; export QMON=$SOCK
pruef() { local t="$1"; if eval "$2"; then echo "ok     $t"; else echo "FEHLER $t"; F=1; fi; }
waitn() { for i in $(seq ${3:-150}); do [ "$(tr -d '\r' < $SER | grep -c "$1")" -ge "${2:-1}" ] && return 0; sleep 0.4; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
bild() { python3 -c "
from PIL import Image
a = Image.open('$1').convert('RGB'); print(a.size[0], a.size[1], len(a.getcolors(1 << 20)))"; }
printf '@echo off\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\SHELL.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' | tr -d '\r' > obj/ae_voll.txt
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_voll.txt >/dev/null 2>&1
mcopy -o -i $IMG out/VOLLTEST.EXE ::/RD2/ZVOLL.EXE
rm -f $SOCK $SER; touch $SER
timeout 240 qemu-system-i386 -m 4 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
waitn "SHELL: bereit" && sleep 3 && $Q "sendkey ret" >/dev/null 2>&1
waitn 'SHELL: Ordner A:\\ ' && sleep 2 && $Q "sendkey ret" >/dev/null 2>&1
waitn 'SHELL: Ordner A:\\RD2\\ ' && sleep 2 && $Q "sendkey z" sleep0.8 "sendkey ret" >/dev/null 2>&1
waitn "VOLLTEST: gezeichnet" 1 && sleep 1.5 && $Q "screendump obj/voll1.ppm" sleep0.5 "sendkey alt-tab" >/dev/null 2>&1
waitn "VOLLTEST: SCREENLOST" 1 && sleep 3 && $Q "screendump obj/voll2.ppm" sleep0.5 "sendkey alt-tab" >/dev/null 2>&1
waitn "VOLLTEST: gezeichnet" 2 && sleep 1.5 && $Q "screendump obj/voll3.ppm" sleep0.5 "sendkey esc" >/dev/null 2>&1
waitn "VOLLTEST: Ende" 1 && sleep 3 && $Q "screendump obj/voll4.ppm" >/dev/null 2>&1
sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
for n in 1 2 3 4; do [ -f obj/voll$n.ppm ] && eval "B$n=\"$(bild obj/voll$n.ppm)\""; done
echo "       Bilder: 1: $B1 | 2: $B2 | 3: $B3 | 4: $B4 (Breite Hoehe Farben)"
gross() { set -- $1; [ "$1" = 640 ] && [ "$2" = 480 ]; }
voll() { set -- $1; [ "$2" = 400 ] && [ "${3:-0}" -ge 40 ]; }   # 13h zeigt QEMU als 640x400; 200 Zeilen, Farbe y >> 2: 50 Farben
pruef "Vollbild 13h mit Farbverlauf (mindestens 40 Farben)" "voll \"$B1\""
pruef "Alt+Tab: M_SCREENLOST, Schreibtisch 640x480" "echo \"\$L\" | grep -q 'VOLLTEST: SCREENLOST' && gross \"$B2\""
pruef "Alt+Tab zurueck: M_SCREENBACK, neu gezeichnet" "echo \"\$L\" | grep -q 'VOLLTEST: SCREENBACK' && voll \"$B3\""
pruef "Esc im Vollbild erreicht das Programm, danach Schreibtisch" "echo \"\$L\" | grep -q 'VOLLTEST: Ende' && gross \"$B4\""
pruef "geparkt angehalten: zwischen SCREENLOST und SCREENBACK kein neues Bild" "[ \$(echo \"\$L\" | sed -n '/VOLLTEST: SCREENLOST/,/VOLLTEST: SCREENBACK/p' | grep -c 'VOLLTEST: gezeichnet') = 0 ]"
pruef "Shell laeuft weiter" "! echo \"\$L\" | grep -qi 'RD2: Fehler\|Ausnahme'"
# ---- weitere Modi ohne Shell: RD2.EXE VOLLTEST.EXE <Modus>, Bild pruefen, Esc ----
for m in "X 640 480" "V 640 480" "E 640 400" "G 640 480" "C 640 400"; do set -- $m
  printf '@echo off\r\nA:\\RD2\\RD2.EXE /VGA /NOMAUS A:\\RD2\\ZVOLL.EXE %s\r\nA:\\FDAPM.COM POWEROFF\r\n' $1 > obj/ae_voll2.txt; mcopy -o -i $IMG obj/ae_voll2.txt ::/AUTOEXEC.BAT
  rm -f $SOCK $SER obj/voll_$1.ppm; touch $SER
  timeout 120 qemu-system-i386 -m 4 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
    -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  waitn "VOLLTEST: gezeichnet" 1 200 && sleep 1.5 && $Q "screendump obj/voll_$1.ppm" sleep0.5 "sendkey esc" >/dev/null 2>&1
  waitn "VOLLTEST: Ende" 1 40; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
  B=$([ -f obj/voll_$1.ppm ] && bild obj/voll_$1.ppm)
  pruef "Modus $1: Bild $2x$3 ($B), Esc beendet" "set -- $B; [ \"\$1\" = $2 ] && [ \"\$2\" = $3 ] && [ \"\${3:-0}\" -ge 2 ] && tr -d '\r' < $SER | grep -q 'VOLLTEST: Ende'"
done
[ $F = 0 ] && echo "Vollbild: alles ok" || echo "Vollbild: FEHLER"
exit $F
