#!/bin/bash
# Phase 4: Dialogtest DLGTEST auf CGA und VGA mono (QEMU), nur Tastatur. Bildschirmfotos
# obj/dlg{1,2}_*.ppm, Vergleich mit test/ref/dlg{1,2}_*.png.
cd "$(dirname "$0")/.."
IMG=obj/dlg.img; SER=obj/dlg_com1.txt; SOCK=/tmp/qmon_dlg.sock
cat > obj/ae_dlg.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\DLGTEST.EXE
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\DLGTEST.EXE
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_dlg.txt
rm -f $SER $SOCK obj/dlg?_*.ppm
timeout 400 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_dlg.py
Q() { python3 obj/qmon_dlg.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
for n in 1 2; do
  K=$([ $n = 1 ] && echo cga || echo vga)
  waitn "DLGTEST: bereit" $((2 * n - 1)) || { echo "FEHLER: Dialog 1 kommt nicht"; break; }; sleep 4
  Q "screendump obj/dlg1_$K.ppm" sleep0.3 "sendkey kp_1" sleep0.3 "sendkey shift-1" sleep0.3 "sendkey tab" sleep0.3 "sendkey spc" sleep0.3 \
    "sendkey tab" sleep0.3 "sendkey kp_2" sleep0.3 "sendkey spc" sleep0.4 "sendkey ret"
  waitn "DLGTEST: Meldung" $n || { echo "FEHLER: Dialog 1 endet nicht"; break; }; sleep 1.2
  Q "screendump obj/dlg2_$K.ppm" sleep0.3 "sendkey esc"
  waitn "DLGTEST: bereit" $((2 * n)) || { echo "FEHLER: Dialog 2 kommt nicht"; break; }; sleep 1
  Q "sendkey alt-t" sleep0.5 "sendkey esc"
  waitn "DLGTEST: Eingabe" $n || { echo "FEHLER: keine InputBox"; break; }; sleep 2
  Q "sendkey shift-x" sleep0.3 "screendump obj/dlg4_$K.ppm" sleep0.3 "sendkey ret"
  waitn "DLGTEST: Liste bereit" $n || { echo "FEHLER: Dialog 3 kommt nicht"; break; }; sleep 3
  Q "sendkey c" sleep0.3 "sendkey c" sleep0.3 "sendkey kp_1" sleep0.3 "sendkey kp_8" sleep0.6 "screendump obj/dlg3_$K.ppm" sleep0.3 "sendkey ret"
  waitn "DLGTEST: Datei" $n || { echo "FEHLER: kein Dateidialog"; break; }; sleep 4
  Q "sendkey r" sleep0.2 "sendkey d" sleep0.2 "sendkey 2" sleep0.3 "sendkey ret"; sleep 4
  Q "sendkey tab" sleep0.3 "sendkey d" sleep0.6 "screendump obj/dlg5_$K.ppm" sleep0.3 "sendkey ret"
  waitn "DLGTEST: Speichern" $n || { echo "FEHLER: kein Speichern"; break; }; sleep 4
  Q "sendkey ret"; sleep 2.5; Q "screendump obj/dlg6_$K.ppm" sleep0.3 "sendkey esc" sleep1 "sendkey esc"
  waitn "DLGTEST: Fehler" $n || echo "FEHLER: kein Ende"
done
wait $QP
tr -d '\r' < $SER | grep -E "DLGTEST|RD: (Speicher|SPEICHER|alle|Fehler|Modul)"
[ -f test/ref/dlg1_cga.png ] && python3 test/bildvergleich.py obj/dlg1_cga.ppm test/ref/dlg1_cga.png obj/dlg1_vga.ppm test/ref/dlg1_vga.png obj/dlg2_cga.ppm test/ref/dlg2_cga.png obj/dlg2_vga.ppm test/ref/dlg2_vga.png obj/dlg3_cga.ppm test/ref/dlg3_cga.png obj/dlg3_vga.ppm test/ref/dlg3_vga.png obj/dlg4_cga.ppm test/ref/dlg4_cga.png obj/dlg4_vga.ppm test/ref/dlg4_vga.png obj/dlg5_cga.ppm test/ref/dlg5_cga.png obj/dlg5_vga.ppm test/ref/dlg5_vga.png obj/dlg6_cga.ppm test/ref/dlg6_cga.png obj/dlg6_vga.ppm test/ref/dlg6_vga.png
exit 0
