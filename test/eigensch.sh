#!/bin/bash
# Datei-Eigenschaften (SHELL, Alt+Eingabe): A:\ oeffnen, AUTOEXEC.BAT waehlen, Dialog (Groesse, Datum),
# Schreibgeschuetzt ankreuzen (Alt+S), OK -> Attribut auf der Diskette gesetzt. Dann Ordner: Groesse "Ordner".
cd "$(dirname "$0")/.."
IMG=obj/eig.img; SER=obj/eig_com1.txt; S=/tmp/qmon_eig.sock; export QMON=$S; F=0
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_eig.txt
bash tools/mkdisk.sh $IMG obj/ae_eig.txt >/dev/null 2>&1
G=$(mdir -i $IMG ::/AUTOEXEC.BAT | awk '/AUTOEXEC/{print $3}')
rm -f $SER $S
timeout 150 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 300); do c=$(tr -d '\r' < $SER 2>/dev/null | grep -c "$1"); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
ok() { if waitn "$1" ${3:-1}; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
waitn "SHELL: bereit"; sleep 3; Q "sendkey ret"
waitn 'SHELL: Ordner A:\\ '; sleep 3
Q "sendkey kp_6"; sleep 0.5; Q "sendkey alt-ret"
ok "SHELL: Eigenschaften" "Alt+Eingabe zeigt die Eigenschaften"
tr -d '\r' < $SER | grep -q " $G Bytes" && echo "ok     Groesse $G Bytes wie auf der Diskette" || { echo "FEHLER Groesse"; tr -d '\r' < $SER | grep -A2 "Eigenschaften" | head -3; F=1; }
tr -d '\r' < $SER | grep -qE "[0-9]{2}\.[0-9]{2}\.20[0-9]{2} [0-9]{2}:[0-9]{2}" && echo "ok     Datum und Uhrzeit" || { echo "FEHLER Datum"; F=1; }
sleep 2; Q "screendump obj/eig1.ppm"; Q "sendkey alt-s"; sleep 0.5; Q "sendkey ret"
ok "SHELL: Attribute 0x00000021" "Schreibgeschuetzt + Archiv gesetzt"
sleep 1; Q "sendkey kp_4"; sleep 0.5; Q "sendkey alt-ret"
ok "SHELL: Eigenschaften" "Eigenschaften des Ordners RD2" 2
tr -d '\r' < $SER | grep -q "Ordner$" && echo "ok     Ordner ohne Bytes" || { echo "FEHLER Ordneranzeige"; F=1; }
Q "sendkey esc"; sleep 1; Q "sendkey alt-f4"; sleep 1; Q "sendkey alt-f4"; sleep 1; Q "sendkey j"
wait $QP 2>/dev/null
mattrib -i $IMG ::/AUTOEXEC.BAT | grep -q "^ *A *R\|R" && echo "ok     AUTOEXEC.BAT ist auf der Diskette schreibgeschuetzt" || { echo "FEHLER Attribut auf Diskette"; mattrib -i $IMG ::/AUTOEXEC.BAT; F=1; }
[ -f obj/eig1.ppm ] && python3 -c "from PIL import Image; Image.open('obj/eig1.ppm').save('obj/eig1.png')"
[ $F = 0 ] && echo "Eigenschaften: alles ok"; exit $F
