#!/bin/bash
# Ansicht > Versteckte Dateien (SHELL): GEHEIM.TXT (versteckt) erscheint erst nach dem Schalter, Zahl gegen mtools.
cd "$(dirname "$0")/.."
IMG=obj/verst.img; SER=obj/verst_com1.txt; S=/tmp/qmon_verst.sock; export QMON=$S; F=0
printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_verst.txt
bash tools/mkdisk.sh $IMG obj/ae_verst.txt >/dev/null 2>&1
echo geheim > obj/GEHEIM.TXT; mcopy -o -i $IMG obj/GEHEIM.TXT ::/; mattrib -i $IMG +h ::/GEHEIM.TXT
alle=$(mdir -i $IMG -a -b :: | wc -l); sicht=$(mdir -i $IMG -b :: | wc -l)
rm -f $SER $S; touch $SER
timeout 130 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 ../test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 250); do c=$(tr -d '\r' < $SER | grep -c "$1"); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
waitn "SHELL: bereit"; sleep 3; Q "sendkey ret"
waitn "SHELL: Ordner A:\\\\ $sicht" && echo "ok     ohne Schalter: $sicht Eintraege (GEHEIM.TXT unsichtbar)" || { echo "FEHLER ohne Schalter (soll $sicht)"; tr -d '\r' < $SER | grep "Ordner A"; F=1; }
sleep 2; Q "sendkey f10"; sleep 0.6; Q "sendkey kp_6"; sleep 0.8; Q "sendkey v"
waitn "SHELL: versteckte 0x00000001" && waitn "SHELL: Ordner A:\\\\ $alle" && echo "ok     mit Schalter: $alle Eintraege (versteckte grau)" || { echo "FEHLER mit Schalter (soll $alle)"; tr -d '\r' < $SER | grep "Ordner A\|versteckte"; F=1; }
sleep 2; Q "screendump obj/verst1.ppm"; Q "quit"; wait $QP 2>/dev/null
[ -f obj/verst1.ppm ] && python3 -c "from PIL import Image; Image.open('obj/verst1.ppm').save('obj/verst1.png')"
[ $F = 0 ] && echo "Versteckte: alles ok"; exit $F
