#!/bin/bash
# Einrichtung: RD2SETUP /AUTO in QEMU (ohne DOS-Maustreiber). Lauf 1: serielle Maus an COM2,
# Lauf 2: nur PS/2-Maus. Prueft Erkennung und RD2.INI (andere Abschnitte bleiben erhalten).
cd "$(dirname "$0")/.."
IMG=obj/setup720.img; F=0
cat > obj/ae_setup.txt <<'AE'
@echo off
A:\RD2\RD2SETUP.EXE /AUTO > A:\S.TXT
A:\FDAPM.COM POWEROFF
AE
for lauf in seriell ps2; do
  FMT=720 SETUP=1 bash tools/mkdisk.sh $IMG obj/ae_setup.txt   # 360 KB: Test-Diskette mit COMMAND.COM (85 KB) zu voll, echte Systemdiskette ohne
  printf '[Programme]\r\nUhr=UHR.EXE\r\n' > obj/rd2ini.tmp; mcopy -o -i $IMG obj/rd2ini.tmp ::/RD2/RD2.INI
  X=""; [ $lauf = seriell ] && X="-serial null -serial msmouse"
  timeout 200 qemu-system-i386 -m 2 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a $X >/dev/null 2>&1
  mcopy -o -i $IMG ::/S.TXT obj/setup_$lauf.txt; mcopy -o -i $IMG ::/RD2/RD2.INI obj/setup_$lauf.ini
  tr -d '\r' < obj/setup_$lauf.txt
  case $lauf in seriell) W="MausHW=SERIELL:COM2";; ps2) W="MausHW=PS2";; esac
  if grep -q "$W" obj/setup_$lauf.ini && grep -q "Uhr=UHR.EXE" obj/setup_$lauf.ini && grep -q "^Grafik=VGA" obj/setup_$lauf.ini; then echo "ok     $lauf: $W"; else echo "FEHLER $lauf"; tr -d '\r' < obj/setup_$lauf.ini; F=1; fi
done
[ $F = 0 ] && echo "Einrichtung: alles ok"
exit $F
