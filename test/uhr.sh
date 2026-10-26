#!/bin/bash
# Punkt 4: Zubehoer Uhr (UHR.EXE T: feste Zeit 12:34:56) auf CGA und VGA, nur Tastatur.
# Bild 1: Anzeige, dann Stellen (Eingabe) auf 08:15:00 / 01.02.1999 (DOS meldet Montag), Bild 2, Esc schliesst.
cd "$(dirname "$0")/.."
IMG=obj/uhr.img; SER=obj/uhr_com1.txt; SOCK=/tmp/qmon_uhr.sock
cat > obj/ae_uhr.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\UHR.EXE T
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\UHR.EXE T
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_uhr.txt
rm -f $SER $SOCK obj/uhr?_*.ppm
timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_uhr.py
Q() { python3 obj/qmon_uhr.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
bs() { for i in 1 2 3 4 5 6 7 8 9 10; do Q "sendkey backspace" sleep0.1; done; }
tipp() { local s=$1 i c; for ((i = 0; i < ${#s}; i++)); do c=${s:i:1}; case $c in :) c=shift-dot;; .) c=dot;; esac; Q "sendkey $c" sleep0.1; done; }
for n in 1 2; do
  K=$([ $n = 1 ] && echo cga || echo vga)
  waitn "UHR: bereit" $n || { echo "FEHLER: Uhr startet nicht"; break; }; sleep 4
  Q "screendump obj/uhr1_$K.ppm" sleep0.3 "sendkey ret"; sleep 2
  bs; tipp 08:15:00; Q "sendkey ret"; sleep 2
  bs; tipp 01.02.1999; Q "sendkey ret"
  waitn "UHR: gestellt" $n || { echo "FEHLER: nicht gestellt"; break; }; sleep 2
  Q "screendump obj/uhr2_$K.ppm" sleep0.3 "sendkey esc"
done
wait $QP
tr -d '\r' < $SER | grep -A1 -E "UHR|RD: (Speicher|SPEICHER|alle|Fehler)" | grep -v "^--"
[ "$(tr -d '\r' < $SER | grep -A1 "UHR: gestellt" | grep -cE "^08:1[45] 01.02")" = 2 ] && echo "ok     Zeit in DOS gestellt (CGA, VGA)" || echo "FEHLER Zeit nicht in DOS"
[ -f test/ref/uhr1_cga.png ] && python3 test/bildvergleich.py obj/uhr1_cga.ppm test/ref/uhr1_cga.png obj/uhr1_vga.ppm test/ref/uhr1_vga.png obj/uhr2_cga.ppm test/ref/uhr2_cga.png obj/uhr2_vga.ppm test/ref/uhr2_vga.png
exit 0
