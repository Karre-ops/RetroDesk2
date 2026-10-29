#!/bin/bash
# Shell Punkt 3 (VGA, QEMU 1 MB ohne XMS = Diskettenbetrieb): Ordner A:\RD2\ oeffnen, mit "F" zu FREIMEM.EXE springen,
# Datei > Anheften, Datei > Auf den Schreibtisch, Programme > 1 startet es (RunDos: RD2.STA, Neustart, Zustand zurueck),
# Diskette wechseln (QEMU change) und zurueck, Strg+A, Ende. Prueft Protokoll, PROGRAMM.LST und RD2.STA auf der Diskette.
# Bilder obj/sh3_{desk,zurueck,wechsel,alle}.ppm.
cd "$(dirname "$0")/.."
IMG=obj/sh3.img; IMG2=obj/sh3b.img; SER=obj/sh3_com1.txt; SOCK=/tmp/qmon_sh3.sock
printf '@echo off\nREM FREIMEM.EXE\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_sh3.txt
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_sh3.txt >/dev/null   # 720 KB: Platz fuer die Sitzungsdatei (360 KB voll)
rm -f $IMG2; mformat -C -f 360 -v ANDERE -i $IMG2 ::; printf 'x\r\n' > obj/sh3_x.txt
for n in EINS ZWEI DREI; do mcopy -i $IMG2 obj/sh3_x.txt ::/$n.TXT; done
rm -f $SER $SOCK obj/sh3_*.ppm; touch $SER
timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_sh3.py
Q() { python3 obj/qmon_sh3.py "$@"; }
waitn() { for i in $(seq 400); do [ "$(tr -d '\r' < $SER | grep -c "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
ok=1
waitn "SHELL: bereit" 1 && sleep 3 && Q "sendkey ret"
waitn 'SHELL: Ordner A:\\ ' 1 && sleep 2 && Q "sendkey ret"
waitn 'SHELL: Ordner A:\\RD2\\ ' 1 && sleep 2 && Q "sendkey f" sleep0.8 "sendkey f10" sleep0.8 "sendkey h"
waitn "SHELL: angeheftet" 1 && sleep 1.5 && Q "sendkey f10" sleep0.8 "sendkey t"
sleep 3; Q "screendump obj/sh3_desk.ppm" "sendkey f10" sleep0.8 "sendkey kp_6" sleep1 "sendkey kp_6" sleep1 "screendump obj/sh3_prgmenu.ppm" "sendkey 1"
waitn "SHELL: Start" 1 && waitn "SHELL: Zustand gesichert" 1
sleep 8; Q "sendkey ret"
waitn "SHELL: Zustand geladen" 1 && waitn 'SHELL: Ordner A:\\RD2\\ ' 2 && sleep 3 && Q "screendump obj/sh3_zurueck.ppm"
# QEMU (SeaBIOS) meldet bei INT 13h 16h immer "gewechselt": die Shell muss das als "kein Signal" erkennen,
# darf nicht staendig neu lesen, und "Neu einlesen" (Ansicht) liest die andere Diskette.
Q "sendkey backspace"; waitn 'SHELL: Ordner A:\\ 6$' 1; sleep 2
Q "change floppy0 $IMG2"; sleep 6
Q "sendkey f10" sleep0.8 "sendkey kp_6" sleep1 "sendkey n"
waitn 'SHELL: Ordner A:\\ 3$' 1 && sleep 2 && Q "screendump obj/sh3_wechsel.ppm"
Q "change floppy0 $IMG"; sleep 2; Q "sendkey f10" sleep0.8 "sendkey kp_6" sleep1 "sendkey n"
waitn 'SHELL: Ordner A:\\ 6$' 2 && sleep 5 && Q "sendkey ctrl-a" sleep1.5 "screendump obj/sh3_alle.ppm"
sleep 1; Q "sendkey alt-f4" sleep1.5 "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
waitn "SHELL: Ende" 1 || ok=0
wait $QP
L=$(tr -d '\r' < $SER)
echo "$L" | grep -E "SHELL: (Ordner|angeheftet|geloest|Start|Zustand|Diskettenwechsel|kein Wechsel|Ende)|RD: (Speicher voll|Fehler)"
LST=$(mtype -i $IMG ::/RD2/PROGRAMM.LST 2>/dev/null | tr -d '\r'); echo "$LST"
chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; ok=0; fi; }
chk "angeheftet, Eintrag in PROGRAMM.LST"   "echo \"\$LST\" | grep -qi 'Datei=A:.RD2.FREIMEM.EXE'"
chk "auf dem Schreibtisch (Schreibtisch=1)" "echo \"\$LST\" | grep -q 'Schreibtisch=1'"
chk "Start aus dem Menue Programme"         "echo \"\$L\" | grep -A1 'SHELL: Start' | grep -qi FREIMEM"
chk "RD2.STA vor RunDos, danach geladen"    "echo \"\$L\" | grep -q 'Zustand geladen'"
chk "Ordner A:\\RD2\\ wiederhergestellt"     "[ \$(echo \"\$L\" | grep -c 'SHELL: Ordner A:.RD2. ') -ge 2 ]"
chk "ohne Wechselsignal erkannt, kein Dauerlesen" "echo \"\$L\" | grep -q 'kein Wechselsignal' && [ \$(echo \"\$L\" | grep -c 'SHELL: Ordner A:') -le 6 ]"
chk "andere Diskette gelesen (3 Dateien)"   "echo \"\$L\" | grep -q 'Ordner A:. 3$'"
chk "RD2.STA beim Beenden geschrieben"      "mdir -i $IMG ::/RD2/RD2.STA >/dev/null 2>&1"
chk "Speicher vollstaendig zurueck"         "echo \"\$L\" | grep -q 'Speicher vollstaendig zurueck'"
[ $ok = 1 ] && echo "Shell 3: alles ok" || { echo "Shell 3: FEHLER"; exit 1; }
