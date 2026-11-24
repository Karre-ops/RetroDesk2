#!/bin/bash
# PLAN_API Luecke 3: Gesamttest. GESAMT.EXE (16 Bit) und GESAMT32.EXE K (RD2/32) nacheinander in einem QEMU (486, 16 MB,
# SB16, PS/2-Maus): Vollbild 13h, ScreenMouse, KeyState mit zwei Tasten, TaktSet (145,6 Hz), Mischer in Schleife,
# Alt+Tab weg und zurueck (Modus, Maus, Klang, Takt danach), Esc, Speicher zurueck. 32 Bit vorher neunmal Kind mit TaktSet
# per TaskKill (mehr als die 8 RD32-Plaetze: TaskKill gibt den Platz frei).
cd "$(dirname "$0")/.."
IMG=obj/gesamt.img; SER=obj/gesamt_com1.txt; SOCK=/tmp/qmon_gesamt.sock; F=0; export QMON=$SOCK
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
Q() { timeout 20 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { for i in $(seq ${3:-300}); do [ "$(tr -d '\r' < $SER | grep -ac "$1" || true)" -ge "${2:-1}" ] && return 0; sleep 0.3; done; echo "       (warte vergeblich auf '$1' x${2:-1})"; return 1; }
printf '@echo off\r\nA:\\RDMAUS.COM\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\GESAMT.EXE\r\nA:\\RD2\\CWSDPMI.EXE -p\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\GESAMT32.EXE K\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_gesamt.txt
FMT=1440 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') GESAMT.EXE GESAMT32.EXE RD32.MOD CWSDPMI.EXE SB.DRV RDMIX.MOD" bash tools/mkdisk.sh $IMG obj/ae_gesamt.txt >/dev/null 2>&1
mcopy -o -i $IMG out/RDMAUS.COM ::/
printf '[System]\r\nSound=SB\r\nSB=A220 I5 D1 H5\r\n' > obj/gesamt_ini.txt; mcopy -o -i $IMG obj/gesamt_ini.txt ::/RD2/RD2.INI
rm -f $SER $SOCK; touch $SER
timeout 280 qemu-system-i386 -cpu 486 -m 16 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -audiodev none,id=a0 -device sb16,audiodev=a0 -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
for n in 1 2; do
  waitn "GESAMT: Vollbild" $n 600 || break
  sleep 1.5; Q "mouse_move 400 400" sleep0.3 "mouse_move 400 400" sleep0.3 "mouse_move 400 400" sleep0.5 "mouse_button 1" sleep0.2 "mouse_button 0" sleep0.8 \
    "sendkey left-right 600" sleep1.5 "sendkey alt-tab"
  waitn "GESAMT: SCREENLOST" $n; sleep 1.5; Q "sendkey alt-tab"
  waitn "GESAMT: SCREENBACK" $n; sleep 1.5; Q "mouse_move -30 -30" sleep0.5 "mouse_move -30 -30" sleep1 "sendkey esc"
  waitn "GESAMT: Ende" $n
done
waitn "RD2: Speicher vollstaendig zurueck" 2 100; sleep 2; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
for n in 1 2; do
  [ $n = 1 ] && A="16 Bit" || A="32 Bit"
  T=$(echo "$L" | awk -v n=$n '/RD2: RetroDesk 2 Kern/{k++} k==n')
  h() { echo "$T" | grep -a "GESAMT: $1" | head -1 | sed 's/.*0x//' | python3 -c "import sys; print(int(sys.stdin.read().strip() or '0', 16))"; }
  T1=$(h "Takte in 1 s"); T2=$(h "Takte 0,5 s nach"); XM=$(h xmax); YM=$(h ymax); MO=$(h "SCREENBACK, BIOS-Modus"); K1=$(h "Mischkanaele dabei"); K2=$(h "Mischkanaele nach")
  echo "       $A: Takte 1 s $T1, 0,5 s nach Alt+Tab $T2, Maus bis $XM/$YM, Modus $MO, Kanaele $K1/$K2"
  pruef "$A: Vollbild, TaktSet, Mischer" "echo \"\$T\" | grep -q 'GESAMT: Vollbild' && [ \$(h TaktSet) != 0 ] && echo \"\$T\" | grep -q 'GESAMT: Mischer klingt'"
  pruef "$A: Takt 145,6 Hz waehrend Vollbild und Klang (130..160)" "[ $T1 -ge 130 ] && [ $T1 -le 160 ]"
  pruef "$A: Maus im Vollbild bis 319/199, Klick" "[ $XM = 319 ] && [ $YM = 199 ] && echo \"\$T\" | grep -q 'GESAMT: Klick bei'"
  pruef "$A: zwei Tasten zugleich, Klang laeuft dabei" "echo \"\$T\" | grep -q 'GESAMT: links und rechts zugleich' && [ $K1 != 0 ]"
  pruef "$A: Alt+Tab weg und zurueck, Modus 13h wieder" "echo \"\$T\" | grep -q 'GESAMT: SCREENLOST' && [ $MO = 19 ]"
  pruef "$A: nach Alt+Tab Maus, Klang und Takt (60..85 in 0,5 s)" "echo \"\$T\" | grep -q 'GESAMT: Maus nach Alt+Tab' && [ $K2 != 0 ] && [ $T2 -ge 60 ] && [ $T2 -le 85 ]"
  pruef "$A: Ende, Speicher vollstaendig zurueck" "echo \"\$T\" | grep -q 'GESAMT: Ende' && echo \"\$T\" | grep -q 'RD2: Speicher vollstaendig zurueck'"
done
T=$(echo "$L" | awk '/RD2: RetroDesk 2 Kern/{k++} k==2')
pruef "32 Bit: neunmal Kind mit TaktSet gestartet und per TaskKill beendet (RD32-Plaetze frei)" "[ \$(echo \"\$T\" | grep -c 'GESAMT: Kind Takt ok') = 9 ] && echo \"\$T\" | grep -q 'GESAMT: Kinder gestartet und beendet 0x00000009'"
[ $F = 0 ] && echo "gesamt: alle ok" || echo "gesamt: FEHLER"; exit $F
