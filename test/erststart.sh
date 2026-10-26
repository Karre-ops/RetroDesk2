#!/bin/bash
# Erster Start: Diskette ohne RD2.INI. RD.EXE muss RD2SETUP /ERST zeigen; Taste S speichert,
# Esc beendet, danach startet der Kern LEER.EXE. Zweiter Start: kein Setup mehr (RD2.INI da).
cd "$(dirname "$0")/.."
IMG=obj/erst720.img; SER=obj/com1_erst.txt; F=0
cat > obj/ae_erst.txt <<'AE'
@echo off
A:\RD2\RD.EXE A:\RD2\LEER.EXE eins > A:\E1.TXT
A:\RD2\RD.EXE A:\RD2\LEER.EXE zwei > A:\E2.TXT
A:\FDAPM.COM POWEROFF
AE
FMT=720 SETUP=1 bash tools/mkdisk.sh $IMG obj/ae_erst.txt   # 720 KB: FreeDOS-Testdiskette + volle Systemdateien passen nicht mehr auf 360 KB (26.10.)
rm -f $SER /tmp/qmon_e.sock
timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:/tmp/qmon_e.sock,server,nowait >/dev/null 2>&1 &
QP=$!
Q="python3 ../test/qemu/qmon.py"; export QMON=/tmp/qmon_e.sock
waitscr() { for i in $(seq ${2:-30}); do $Q text 2>/dev/null | grep -q "$1" && return 0; sleep 1; done; return 1; }
if waitscr "Erster Start" 90; then echo "ok     Einrichtung erscheint beim ersten Start"; $Q text > obj/schirm_setup.txt 2>/dev/null
else echo "FEHLER keine Einrichtung"; F=1; fi
$Q "sendkey s" sleep1 "sendkey esc" sleep1 >/dev/null 2>&1
waitscr "Einrichtung" 3 && $Q "sendkey n" >/dev/null 2>&1        # Rueckfrage nach Esc (schon gespeichert)
wait $QP
mcopy -o -i $IMG ::/RD2/RD2.INI obj/erst.ini 2>/dev/null && grep -q "^Grafik=" obj/erst.ini && echo "ok     RD2.INI angelegt" || { echo "FEHLER keine RD2.INI"; F=1; }
N=$(grep -c "LEER" $SER)
grep -q "eins" $SER && grep -q "zwei" $SER && echo "ok     LEER lief beide Male" || { echo "FEHLER LEER lief nicht ($N)"; F=1; }
[ $F = 0 ] && echo "Erster Start: alles ok"
exit $F
