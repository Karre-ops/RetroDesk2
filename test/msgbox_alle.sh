#!/bin/bash
# MsgBox MB_YESALLNOCANCEL (Ja fuer alle, 01.11.2026): Tab auf "Alle", Eingabe -> ID_ALLE (8)
cd /app/conversations/6abeca1f8dde4e6407393f7a/retrodesk2
IMG=obj/alle.img; SER=obj/alle_com1.txt; SOCK=/tmp/qmon_alle.sock
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\DLGTEST.EXE A\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_alle.txt
bash tools/mkdisk.sh $IMG obj/ae_alle.txt >/dev/null; rm -f $SER $SOCK; touch $SER
timeout 100 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_alle.py
Q() { python3 obj/qmon_alle.py "$@"; }
for i in $(seq 300); do grep -q "Alle bereit" $SER && break; sleep 0.2; done; sleep 2
sleep 4; Q "sendkey tab" sleep1 "sendkey ret" sleep3 "sendkey esc"
for i in $(seq 100); do grep -q "Alle ->" $SER && break; sleep 0.2; done
R=$(tr -d "\r" < $SER | grep "Alle ->"); kill %1 2>/dev/null
if [ "$R" = "DLGTEST: Alle -> 0x00000008" ]; then echo "ok     MsgBox Ja/Alle/Nein/Abbruch: Alle liefert ID_ALLE"; else echo "FEHLER MsgBox Alle: $R"; fi
