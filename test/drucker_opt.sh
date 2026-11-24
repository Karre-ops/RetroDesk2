#!/bin/bash
# PLAN_API 7: Druckerauswahl in OPTIONEN (Karte 7, Dialog 18). OPTIONEN T7: Drucker HP PCL (zweimal runter), Anschluss
# Datei, Entwurf an, OK -> RD2.INI [Drucker] Treiber=PCL Port=0 Entwurf=1. Danach PRNTEST mit dieser RD2.INI: PCL 150 dpi.
cd "$(dirname "$0")/.."
IMG=obj/drkopt.img; SER=obj/drkopt_com1.txt; SOCK=/tmp/qmon_drkopt.sock; F=0
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\OPTIONEN.EXE T7 A:\\T1\\\r\nA:\\RD2\\RD2.EXE /NOMAUS /CGA A:\\RD2\\PRNTEST.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_drkopt.txt
FMT=1440 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') OPTIONEN.EXE PRNTEST.EXE RDPRN.MOD PCL.DRV" bash tools/mkdisk.sh $IMG obj/ae_drkopt.txt >/dev/null 2>&1
mkdir -p obj/drkopt/T1; printf 'FILES=20\r\nSHELL=A:\\RD2\\RD2.EXE /P A:\\RD2\\SHELL.EXE\r\n' > obj/drkopt/T1/CONFIG.SYS; printf 'x' > obj/drkopt/T1/COMMAND.COM
mmd -i $IMG ::/T1; mcopy -i $IMG obj/drkopt/T1/* ::/T1/
rm -f $SER $SOCK; touch $SER
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_drkopt.py
Q() { timeout 20 python3 obj/qmon_drkopt.py "$@" >/dev/null 2>&1; }
for i in $(seq 600); do grep -aq "OPTIONEN: Karte 7" $SER && break; sleep 0.2; done; sleep 3
Q "screendump obj/drkopt.ppm" sleep0.3 "sendkey alt-d" sleep0.4 "sendkey down" sleep0.3 "sendkey down" sleep0.3 "sendkey alt-a" sleep0.4 "sendkey down" sleep0.3 "sendkey down" sleep0.3 "sendkey alt-e" sleep0.4 "sendkey ret"
wait $QP
L=$(tr -d '\r' < $SER); I=$(mtype -i $IMG ::/RD2/RD2.INI 2>/dev/null | tr -d '\r')
echo "$I" | grep -iA3 "^\[Drucker\]" | sed 's/^/       /'
pruef "Karte 7 (Drucker) geoeffnet" "echo \"\$L\" | grep -q 'OPTIONEN: Karte 7'"
pruef "RD2.INI: Treiber=PCL, Port=0, Entwurf=1" "echo \"\$I\" | grep -qx 'Treiber=PCL' && echo \"\$I\" | grep -qx 'Port=0' && echo \"\$I\" | grep -qx 'Entwurf=1'"
pruef "PRNTEST druckt danach mit PCL 150 dpi in die Datei" "echo \"\$L\" | grep -q 'PRNTEST: dpi x 0x00000096' && echo \"\$L\" | grep -q 'PRNTEST: PrnClose 0x00000000'"
python3 -c "from PIL import Image; Image.open('obj/drkopt.ppm').save('obj/drkopt.png')" 2>/dev/null
[ $F = 0 ] && echo "drucker_opt: alle ok"; exit $F
