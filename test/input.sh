#!/bin/bash
# Phase 3: Tastatur und Maus in QEMU (PS/2-Maus mit RDMAUS). Tasten und Mausbewegungen ueber
# den QEMU-Monitor; INPTEST meldet alles auf COM1. Pruefung: test/input_pruef.py.
cd "$(dirname "$0")/.."
K=${1:-VGA}
IMG=obj/inp.img; SER=obj/inp_com1.txt; SOCK=/tmp/qmon_inp.sock
cat > obj/ae_inp.txt <<AE
@echo off
A:\\RDMAUS.COM
A:\\RD2\\RD2.EXE /$K A:\\RD2\\INPTEST.EXE
A:\\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_inp.txt
mcopy -i $IMG out/RDMAUS.COM ::/
rm -f $SER $SOCK obj/inp_*.ppm
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_inp.py
Q() { python3 obj/qmon_inp.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null)" -ge "$2" ] && return 0; sleep 0.2; done; return 1; }
waitn "INPTEST: bereit" 1 || { echo "FEHLER: INPTEST startet nicht"; kill $QP; exit 1; }
sleep 0.5
Q "sendkey h" sleep0.1 "sendkey a" sleep0.1 "sendkey l" sleep0.1 "sendkey l" sleep0.1 "sendkey o" sleep0.1 "sendkey shift-a" sleep0.1 "sendkey f1" sleep0.1 "sendkey ctrl-c" sleep0.3
Q "mouse_move -120 -90" sleep0.5 "mouse_button 1" sleep0.2 "mouse_button 0" sleep0.8        # Klick: Kaestchen
Q "mouse_move 90 60" sleep0.6 "mouse_button 1" sleep0.05 "mouse_button 0" sleep0.05 "mouse_button 1" sleep0.05 "mouse_button 0" sleep0.8   # Doppelklick
Q "mouse_move 75 -30" sleep0.6 "mouse_button 1" sleep0.2 "mouse_button 0" sleep0.4         # zweites Kaestchen, Zeiger bleibt darauf
Q "mouse_move 180 180" sleep0.8                                                              # Zeiger weg
Q "screendump obj/inp_${K}.ppm" sleep0.3 "sendkey esc"
waitn "INPTEST: fertig" 1 || echo "FEHLER: kein Ende"
wait $QP
tr -d '\r' < $SER | grep -E "INPTEST|RD: (Speicher|SPEICHER|Fehler)" > obj/inp_log.txt
cat obj/inp_log.txt
python3 test/input_pruef.py obj/inp_log.txt obj/inp_${K}.ppm
