#!/bin/bash
# Phase 3: Testbild GDITEST auf CGA und VGA mono (QEMU), Bildschirmfotos nach obj/gdi_*.ppm,
# Vergleich mit test/ref/gdi_*.png (python3 test/bildvergleich.py). Hercules: test/gdi_hgc.sh (DOSBox).
cd "$(dirname "$0")/.."
IMG=obj/gdi.img; SER=obj/gdi_com1.txt; SOCK=/tmp/qmon_gdi.sock
cat > obj/ae_gdi.txt <<'AE'
@echo off
A:\RD2\RD2.EXE /NOMAUS /CGA A:\RD2\GDITEST.EXE
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\GDITEST.EXE
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_gdi.txt
rm -f $SER $SOCK obj/gdi_cga.ppm obj/gdi_vga.ppm
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_gdi.py
Q() { python3 obj/qmon_gdi.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null)" -ge "$2" ] && return 0; sleep 0.2; done; return 1; }
waitn "GDITEST: fertig" 1 && sleep 0.5 && Q "screendump obj/gdi_cga.ppm" "pmemsave 0xb8000 16384 obj/gdi_cga.vram"
waitn "GDITEST: fertig" 2 && sleep 0.5 && Q "screendump obj/gdi_vga.ppm"
wait $QP
tr -d '\r' < $SER | grep -E "GDITEST|RD: (Speicher|SPEICHER|alle|Fehler)"
python3 test/bildvergleich.py obj/gdi_cga.ppm test/ref/gdi_cga.png obj/gdi_vga.ppm test/ref/gdi_vga.png
