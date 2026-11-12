#!/bin/bash
# Phase 4: Mauszeiger-Formen in QEMU (PS/2-Maus mit RDMAUS), CGA und VGA. Pruefung: test/zeiger_pruef.py.
cd "$(dirname "$0")/.."
IMG=obj/zeig.img; SER=obj/zeig_com1.txt; SOCK=/tmp/qmon_zeig.sock
cat > obj/ae_zeig.txt <<AE
@echo off
A:\\RDMAUS.COM
A:\\RD2\\RD2.EXE /CGA A:\\RD2\\ZEIGTEST.EXE
A:\\RD2\\RD2.EXE /VGA A:\\RD2\\ZEIGTEST.EXE
A:\\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_zeig.txt
mcopy -i $IMG out/RDMAUS.COM ::/
rm -f $SER $SOCK obj/zeig_*.ppm
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_zeig.py
Q() { python3 obj/qmon_zeig.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
for n in 1 2; do
  K=$([ $n = 1 ] && echo cga || echo vga)
  waitn "ZEIGTEST: bereit" $n || { echo "FEHLER: ZEIGTEST startet nicht"; break; }
  sleep 0.5
  Q "mouse_move 36 24" sleep0.4 "mouse_move 9 6" sleep0.4 "sendkey 1" sleep0.6 "screendump obj/zeig1_$K.ppm" sleep0.3
  Q "sendkey 2" sleep0.6 "screendump obj/zeig2_$K.ppm" sleep0.3
  Q "sendkey 3" sleep0.4 "mouse_move 12 9" sleep0.4 "mouse_move 6 3" sleep0.4 "sendkey 4" sleep0.6 "screendump obj/zeig3_$K.ppm" sleep0.3 "sendkey q"
  waitn "ZEIGTEST: fertig" $n || echo "FEHLER: kein Ende"
done
wait $QP
tr -d '\r' < $SER | grep -E "ZEIGTEST|RD: (Speicher|SPEICHER|alle|Fehler)"
python3 test/zeiger_pruef.py $SER
