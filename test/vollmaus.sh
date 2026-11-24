#!/bin/bash
# Maus und Tasten im Vollbild (PLAN_API 3): VOLLMAUS.EXE in Modus 13h mit ScreenMouse(320, 200). Maus weit nach rechts
# unten: Koordinaten bis 319/199 (nicht die 640x480 des Schreibtischs), Klick kommt mit Vollbild-Koordinaten, links und
# rechts zugleich ueber KeyState(KS_SCAN), Esc beendet, danach Schreibtisch mit Maus wie vorher, Speicher zurueck.
cd "$(dirname "$0")/.."
IMG=obj/vollmaus.img; SER=obj/vollmaus_com1.txt; SOCK=/tmp/qmon_vollmaus.sock; F=0; export QMON=$SOCK
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
Q() { timeout 20 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { for i in $(seq 300); do [ "$(tr -d '\r' < $SER | grep -ac "$1" || true)" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
printf '@echo off\r\nA:\\RDMAUS.COM\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\VOLLMAUS.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' | tr -d '\r' > obj/ae_vollmaus.txt
FMT=1440 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') VOLLMAUS.EXE" bash tools/mkdisk.sh $IMG obj/ae_vollmaus.txt >/dev/null 2>&1
mcopy -i $IMG out/RDMAUS.COM ::/
rm -f $SER $SOCK; touch $SER
timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
pruef "Vollbild mit Maus" "waitn 'VOLLMAUS: Vollbild'"
sleep 1; Q "mouse_move 400 400" sleep0.3 "mouse_move 400 400" sleep0.3 "mouse_move 400 400" sleep0.5 "mouse_button 1" sleep0.2 "mouse_button 0" sleep0.8 \
  "sendkey left-right 600" sleep1.2 "sendkey esc" sleep2
waitn "VOLLMAUS: Ende" 1; sleep 3; kill $QP 2>/dev/null; wait $QP 2>/dev/null
h() { tr -d '\r' < $SER | grep -a "VOLLMAUS: $1" | head -1 | sed 's/.*0x//' | python3 -c "import sys; print(int(sys.stdin.read().strip() or '0', 16))"; }
XM=$(h xmax); YM=$(h ymax); BEI=$(h bei); echo "       xmax $XM, ymax $YM, Klick bei $(( BEI & 0xFFFF ))/$(( BEI >> 16 ))"
pruef "Maus im Bereich des Vollbilds (319/199, nicht 639/479)" "[ $XM = 319 ] && [ $YM = 199 ]"
pruef "Klick mit Vollbild-Koordinaten" "tr -d '\r' < $SER | grep -q 'VOLLMAUS: Taste' && [ $(( BEI & 0xFFFF )) = 319 ] && [ $(( BEI >> 16 )) = 199 ]"
pruef "KeyState: links und rechts zugleich gedrueckt" "tr -d '\r' < $SER | grep -q 'VOLLMAUS: links und rechts zugleich'"
pruef "Ende, Speicher vollstaendig zurueck" "tr -d '\r' < $SER | grep -q 'VOLLMAUS: Ende' && tr -d '\r' < $SER | grep -q 'RD2: Speicher vollstaendig zurueck'"
[ $F = 0 ] && echo "VOLLMAUS: alles ok"; exit $F
