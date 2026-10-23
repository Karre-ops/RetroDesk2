#!/bin/bash
# Phase 4: Fenstertest WINTEST auf CGA und VGA mono (QEMU), Bildschirmfotos nach obj/win_*.ppm,
# Vergleich mit test/ref/win_*.png (python3 test/bildvergleich.py). Hercules: test/win_hgc.sh (DOSBox).
cd "$(dirname "$0")/.."
IMG=obj/win.img; SER=obj/win_com1.txt; SOCK=/tmp/qmon_win.sock
cat > obj/ae_win.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\WINTEST.EXE
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\WINTEST.EXE
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_win.txt
rm -f $SER $SOCK obj/win_cga.ppm obj/win_vga.ppm
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_win.py
Q() { python3 obj/qmon_win.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
waitn "WINTEST: bild" 1 && sleep 0.5 && Q "screendump obj/win_cga.ppm" "pmemsave 0xb8000 16384 obj/win_cga.vram"
waitn "WINTEST: bild" 2 && sleep 0.5 && Q "screendump obj/win_vga.ppm"
wait $QP
tr -d '\r' < $SER | grep -E "WINTEST|-> |RD: (Speicher|SPEICHER|alle|Fehler)"
[ -f test/ref/win_cga.png ] && python3 test/bildvergleich.py obj/win_cga.ppm test/ref/win_cga.png obj/win_vga.ppm test/ref/win_vga.png
