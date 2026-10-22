#!/bin/bash
# RunDos auf einem reinen Diskettenrechner (QEMU, 640 KB, kein XMS, nur A:): RD_DOS_KEEP muss
# abgelehnt werden. Ohne KEEP beendet sich RD, der Rest startet FREIMEM.EXE und danach RD neu;
# DOSTEST erkennt den zweiten Lauf an seiner Marke und ruft RunDos nochmal (jetzt unter dem
# Rest: Eintrag dort, RD endet mit 0D5h). Im dritten Lauf endet RD normal.
cd "$(dirname "$0")/.."
IMG=obj/rdisk720.img; F=0
cat > obj/ae_rdisk.txt <<'AE'
@echo off
A:\RD2\RD.EXE /D A:\RD2\DOSTEST.EXE A:\RD2\FREIMEM.EXE A:\FREI.TXT N > A:\R1.TXT
echo ENDE %ERRORLEVEL% > A:\R2.TXT
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_rdisk.txt
timeout 250 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a >/dev/null 2>&1
mcopy -o -i $IMG ::/R1.TXT obj/rdisk_r1.txt 2>/dev/null; mcopy -o -i $IMG ::/FREI.TXT obj/rdisk_frei.txt 2>/dev/null; mcopy -o -i $IMG ::/R2.TXT obj/rdisk_r2.txt 2>/dev/null; mcopy -o -i $IMG ::/FREI.TX2 obj/rdisk_frei2.txt 2>/dev/null
L=$(tr -d '\r' < obj/rdisk_r1.txt 2>/dev/null)
chk() { if echo "$L" | grep -q "$1"; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
chk "DOSTEST: mit KEEP 0x0000FFFF" "RD_DOS_KEEP ohne XMS/Festplatte abgelehnt"
chk "DOSTEST: zweiter Lauf" "RD nach dem DOS-Programm neu gestartet"
chk "DOSTEST: nach Neustart wieder da" "zweites RunDos unter dem Rest, RD wieder da"
echo "$L" | grep -q "FEHLER, RunDos kam zurueck" && { echo "FEHLER RunDos kam zurueck"; F=1; }
grep -q "ARG hallo" obj/rdisk_frei.txt 2>/dev/null && echo "ok     DOS-Programm lief ($(head -1 obj/rdisk_frei.txt | tr -d '\r'))" || { echo "FEHLER DOS-Programm lief nicht"; F=1; }
grep -q "ARG zwei" obj/rdisk_frei2.txt 2>/dev/null && echo "ok     zweites DOS-Programm lief ($(head -1 obj/rdisk_frei2.txt | tr -d '\r'))" || { echo "FEHLER zweites DOS-Programm"; F=1; }
mdir -i $IMG ::/DOSTEST.MRK >/dev/null 2>&1 && { echo "FEHLER Marke liegt noch da"; F=1; }
grep -q "ENDE" obj/rdisk_r2.txt 2>/dev/null && echo "ok     danach weiter in AUTOEXEC ($(tr -d '\r' < obj/rdisk_r2.txt))" || { echo "FEHLER RD kam nicht zurueck"; F=1; }
[ $F = 0 ] && echo "RunDos Diskette: alles ok"
exit $F
