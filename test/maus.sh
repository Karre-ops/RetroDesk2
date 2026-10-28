#!/bin/bash
# Mauseinstellungen: MAUS.EXE T schreibt RD2.INI [Maus] Tempo=5 Doppelklick=700 Tasten=1, danach startet RD2 neu.
# Mit getauschten Tasten oeffnet QEMU-Taste 1 (links) das Kontextmenue, zwei Klicks mit Taste 2 im Abstand von
# 600 ms (ueber den alten 500 ms) oeffnen das Laufwerk als Doppelklick. Tempo 5 = doppelt so schnell: halber Weg (6 15 statt 12 31).
cd "$(dirname "$0")/.."
IMG=obj/maus.img; SER=obj/maus_com1.txt; SOCK=/tmp/qmon_maus.sock; F=0
printf '@echo off\nA:\\RDMAUS.COM\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\MAUS.EXE T\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_maus.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') MAUS.EXE" bash tools/mkdisk.sh $IMG obj/ae_maus.txt >/dev/null 2>&1
mcopy -i $IMG out/RDMAUS.COM ::/
rm -f $SER $SOCK; touch $SER
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_maus.py
Q() { timeout 20 python3 obj/qmon_maus.py "$@" >/dev/null 2>&1; }
waitn() { for i in $(seq 300); do [ "$(tr -d '\r' < $SER | grep -ac "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; return 1; }
diffpx() { python3 -c "
from PIL import Image, ImageChops
a=Image.open('$1').convert('L'); b=Image.open('$2').convert('L'); print(sum(1 for v in ImageChops.difference(a,b).tobytes() if v))"; }
waitn "MAUS: gespeichert" 1 && echo "ok     MAUS.EXE hat gespeichert" || { echo "FEHLER MAUS.EXE"; F=1; }
if waitn "SHELL: bereit" 1; then
  sleep 3; Q "mouse_move -400 -400" sleep0.3 "mouse_move -400 -400" sleep0.3 "mouse_move -400 -400" sleep0.3 "mouse_move -400 -400" sleep0.5 "mouse_move 18 45" sleep0.8 "screendump obj/maus_vor.ppm" sleep0.3 \
    "mouse_button 1" sleep0.2 "mouse_button 0" sleep1.2 "screendump obj/maus_pop.ppm" sleep0.3 "sendkey esc" sleep1.0
  n=$(diffpx obj/maus_vor.ppm obj/maus_pop.ppm); [ "$n" -gt 3000 ] && echo "ok     linke Taste wirkt als rechte (Kontextmenue, $n Pixel)" || { echo "FEHLER kein Kontextmenue ($n Pixel)"; F=1; }
  Q "mouse_button 2" sleep0.1 "mouse_button 0" sleep0.5 "mouse_button 2" sleep0.1 "mouse_button 0" sleep3.5
  tr -d '\r' < $SER | grep -aq 'SHELL: Ordner A:' && echo "ok     Doppelklick mit 600 ms Abstand erkannt (700 ms eingestellt)" || { echo "FEHLER kein Doppelklick"; F=1; }
  Q "sendkey alt-f4" sleep1 "sendkey alt-f4" sleep1 "sendkey ret"
else echo "FEHLER Shell startet nicht"; F=1; fi
sleep 4; kill $QP 2>/dev/null; wait $QP 2>/dev/null
mtype -i $IMG ::/RD2/RD2.INI 2>/dev/null | tr -d '\r' | grep -A3 "^\[Maus\]" | tr '\n' ' '; echo
[ $F = 0 ] && echo "Mauseinstellungen: alles ok"; exit $F
