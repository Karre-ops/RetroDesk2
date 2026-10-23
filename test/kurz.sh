#!/bin/bash
# Schnelltest: RD.EXE mit einem Programm in QEMU, serielle Ausgabe zeigen. $1 = Befehlszeile hinter RD.EXE
cd "$(dirname "$0")/.."
printf '@echo off\r\nA:\\RD2\\RD.EXE %s\r\nA:\\FDAPM.COM POWEROFF\r\n' "$1" > obj/ae_kurz.txt
bash tools/mkdisk.sh obj/kurz.img obj/ae_kurz.txt >/dev/null
rm -f obj/kurz_com1.txt
timeout ${2:-40} qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/kurz.img,format=raw,if=floppy,index=0 -boot a -serial file:obj/kurz_com1.txt >/dev/null 2>&1
tr -d '\r' < obj/kurz_com1.txt | head -${3:-60}
