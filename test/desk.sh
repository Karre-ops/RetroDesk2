#!/bin/bash
# Desktop (SHELL.EXE Teil 1) auf CGA und VGA mono (QEMU), nur Tastatur. Bildschirmfotos
# obj/desk{1..6}_*.ppm, Vergleich mit test/ref/desk*_*.png:
#   1 Schreibtisch mit Laufwerk A:   2 Ordner A:\   3 Ordner A:\RD2\ (Symbole, Rollbalken)
#   4 Menue "RD" (Alt+Leertaste)     5 Info          6 Rueckfrage Beenden
cd "$(dirname "$0")/.."
IMG=obj/desk.img; SER=obj/desk_com1.txt; SOCK=/tmp/qmon_desk.sock
cat > obj/ae_desk.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\SHELL.EXE
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\SHELL.EXE
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_desk.txt
rm -f $SER $SOCK obj/desk?_*.ppm
timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_desk.py
Q() { python3 obj/qmon_desk.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(tr -d '\r' < $SER 2>/dev/null | grep -c "$1" || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
for n in 1 2; do
  K=$([ $n = 1 ] && echo cga || echo vga)
  waitn "SHELL: bereit" $n || { echo "FEHLER: Shell kommt nicht"; break; }; sleep 3
  Q "screendump obj/desk1_$K.ppm" sleep0.3 "sendkey ret"
  waitn 'SHELL: Ordner A:\\ ' $n || { echo "FEHLER: Ordner A: kommt nicht"; break; }; sleep 3
  Q "screendump obj/desk2_$K.ppm" sleep0.3 "sendkey ret"
  waitn 'SHELL: Ordner A:\\RD2\\ ' $n || { echo "FEHLER: Ordner RD2 kommt nicht"; break; }; sleep 4
  Q "sendkey right" sleep0.3 "sendkey down" sleep0.5 "screendump obj/desk3_$K.ppm" sleep0.3 "sendkey alt-spc" sleep1 "screendump obj/desk4_$K.ppm" sleep0.3 "sendkey i"; sleep 2
  Q "screendump obj/desk5_$K.ppm" sleep0.3 "sendkey ret" sleep1 "sendkey alt-spc" sleep0.8 "sendkey b"; sleep 2
  Q "screendump obj/desk6_$K.ppm" sleep0.3 "sendkey ret"
  waitn "SHELL: Ende" $n || { echo "FEHLER: Shell endet nicht"; break; }
done
wait $QP
tr -d '\r' < $SER | grep -E "SHELL|RD: (Speicher|SPEICHER|alle|Fehler|Modul)"
[ -f test/ref/desk1_cga.png ] && for k in cga vga; do for b in 1 2 3 4 5 6; do python3 test/bildvergleich.py obj/desk${b}_$k.ppm test/ref/desk${b}_$k.png; done; done
exit 0
