#!/bin/bash
# Taskmanager: SHELL startet UHR, dann RD > Taskmanager: 2 Programme (SHELL, UHR), UHR beenden -> 1 Programm.
cd "$(dirname "$0")/.."
IMG=obj/tm.img; SER=obj/tm_com1.txt; S=/tmp/qmon_tm.sock; export QMON=$S; F=0
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\TASKMAN.EXE T\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_tm.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') TASKMAN.EXE UHR.EXE" bash tools/mkdisk.sh $IMG obj/ae_tm.txt >/dev/null 2>&1
rm -f $SER $S; touch $SER
timeout 130 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 250); do c=$(tr -d '\r' < $SER | grep -c "$1"); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
ok() { if waitn "$1" ${3:-1}; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
ok "TASKMAN: Programme 0x00000001" "zeigt das laufende Programm (UHR)"
sleep 2; Q "screendump obj/tm1.ppm"; Q "sendkey tab"; sleep 0.5; Q "sendkey ret"; sleep 1.5; Q "screendump obj/tm2.ppm"; Q "sendkey tab"; sleep 0.4; Q "sendkey ret"
ok "TASKMAN: beendet" "Beenden mit Rueckfrage"
tr -d '\r' < $SER | grep -A1 "TASKMAN: beendet" | grep -qi "UHR" && echo "ok     UHR beendet" || { echo "FEHLER falsches Programm"; tr -d '\r' < $SER | grep -A1 "TASKMAN: beendet"; F=1; }
ok "TASKMAN: Programme 0x00000000" "danach keins mehr"
Q "quit"; wait $QP 2>/dev/null
[ -f obj/tm1.ppm ] && python3 -c "from PIL import Image; Image.open('obj/tm1.ppm').save('obj/tm1.png')"
[ $F = 0 ] && echo "Taskmanager: alles ok"; exit $F
