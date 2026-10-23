#!/bin/bash
# Phase 4: Menuetest MENUTEST auf CGA und VGA mono (QEMU), Bildschirmfotos nach obj/menu_*.ppm,
# Vergleich mit test/ref/menu_*.png (python3 test/bildvergleich.py). Hercules: test/menu_hgc.sh (DOSBox).
cd "$(dirname "$0")/.."
IMG=obj/menu.img; SER=obj/menu_com1.txt; SOCK=/tmp/qmon_menu.sock
cat > obj/ae_menu.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\MENUTEST.EXE
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\MENUTEST.EXE
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_menu.txt
rm -f $SER $SOCK obj/menu_cga.ppm obj/menu_vga.ppm
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_menu.py
Q() { python3 obj/qmon_menu.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null)" -ge "$2" ] && return 0; sleep 0.2; done; return 1; }
for n in 1 2; do
  waitn "MENUTEST: bereit" $n || break; sleep 0.5
  K=$([ $n = 1 ] && echo cga || echo vga)
  Q "sendkey f10" sleep0.4 "sendkey kp_6" sleep0.4 "sendkey kp_2" sleep0.6 "screendump obj/menu_$K.ppm" sleep0.3 "sendkey ret" sleep0.6
  Q "sendkey f10" sleep0.4 "sendkey n" sleep0.6 "sendkey f10" sleep0.4 "sendkey esc" sleep0.3
done
wait $QP
tr -d '\r' < $SER | grep -E "MENUTEST|-> |RD: (Speicher|SPEICHER|alle|Fehler)"
[ -f test/ref/menu_cga.png ] && python3 test/bildvergleich.py obj/menu_cga.ppm test/ref/menu_cga.png obj/menu_vga.ppm test/ref/menu_vga.png
