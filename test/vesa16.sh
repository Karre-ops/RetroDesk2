#!/bin/bash
# PLAN_API Luecke 3: VESA fuer 16-Bit-Programme (RD2.LIB VESA16.C). QEMU std-VGA (Bochs-VBE, Fenster A 64 KB).
cd "$(dirname "$0")/.."
IMG=obj/vesa16.img; SER=obj/vesa16_com1.txt; F=0
printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\VESATEST.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_vesa16.txt
FMT=1440 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') VESATEST.EXE" bash tools/mkdisk.sh $IMG obj/ae_vesa16.txt >/dev/null 2>&1
rm -f $SER; timeout 120 qemu-system-i386 -m 4 -display none -no-reboot -vga std -drive file=$IMG,format=raw,if=floppy,index=0 -boot a -serial file:$SER >/dev/null 2>&1
L=$(tr -d '\r' < $SER)
echo "$L" | grep "VESATEST: Modus ok" | sed 's/^/       /'
if echo "$L" | grep -q "VESATEST: fertig" && ! echo "$L" | grep -q FALSCH; then echo "ok     VESATEST: alle Pruefungen"; else echo "FEHLER VESATEST"; echo "$L" | grep -B1 FALSCH | head -10 | sed 's/^/       /'; F=1; fi
echo "$L" | grep -q "RD2: Speicher vollstaendig zurueck" && echo "ok     Speicher vollstaendig zurueck" || { echo "FEHLER Speicher"; F=1; }
[ $F = 0 ] && echo "vesa16: alle ok"; exit $F
