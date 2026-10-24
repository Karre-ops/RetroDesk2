#!/bin/bash
# Shell Teil 2: Dateioperationen auf der Testdiskette (VGA, nur Tastatur):
# F7 Ordner TEST, F5 FDAPM.COM nach A:\TEST, in TEST: F2 -> FDAPM.BAK, F5 Ordner TEST nach A:\RD2 (rekursiv),
# Entf Ordner TEST samt Inhalt. Danach prueft mtools das Abbild. Bild obj/desk2_ops.ppm (Ordner TEST).
# Graue Tasten (Pos1, Pfeile, Entf) kommen unter QEMU/SeaBIOS doppelt an, daher Ziffernblock (NumLock aus).
cd "$(dirname "$0")/.."
IMG=obj/desk2.img; SER=obj/desk2_com1.txt; SOCK=/tmp/qmon_desk2.sock
printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_desk2.txt
bash tools/mkdisk.sh $IMG obj/ae_desk2.txt
rm -f $SER $SOCK obj/desk2_ops.ppm; touch $SER
timeout 240 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_desk2.py
Q() { python3 obj/qmon_desk2.py "$@"; }
waitn() { for i in $(seq 400); do [ "$(tr -d '\r' < $SER | grep -c "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
tippe() { local a=(); for c in $(echo "$1" | sed 's/./& /g'); do a+=("sendkey $c" sleep0.15); done; Q "${a[@]}"; }
ok=1
waitn "SHELL: bereit" 1 && sleep 3 && Q "sendkey ret"
waitn 'SHELL: Ordner A:\\ ' 1 && sleep 2 && Q "sendkey f7" sleep1.5 && tippe test && Q "sendkey ret"
waitn "SHELL: Ordner angelegt" 1 && sleep 2 && Q "sendkey kp_1" sleep0.3 "sendkey f5" sleep1.5 && tippe test && Q "sendkey ret"
waitn "SHELL: kopiert" 1 && sleep 2 && Q "sendkey kp_7" sleep0.3 "sendkey kp_6" sleep0.3 "sendkey ret"
waitn 'SHELL: Ordner A:\\TEST\\ 2' 1 && sleep 2 && Q "sendkey kp_6" sleep0.3 "sendkey f2" sleep1.5 "sendkey backspace" sleep0.15 "sendkey backspace" sleep0.15 "sendkey backspace" sleep0.15 && tippe bak && Q "sendkey ret"
waitn "SHELL: umbenannt" 1 && sleep 2.5 && Q "screendump obj/desk2_ops.ppm" sleep0.3 "sendkey backspace"
waitn 'SHELL: Ordner A:\\ ' 4 && sleep 2 && Q "sendkey kp_7" sleep0.3 "sendkey kp_6" sleep0.3 "sendkey f5" sleep1.5 && tippe rd2 && Q "sendkey ret"
waitn "SHELL: kopiert" 2 && sleep 2 && Q "sendkey kp_7" sleep0.3 "sendkey kp_6" sleep0.3 "sendkey f8" sleep1.5 "sendkey ret"
waitn "SHELL: geloescht" 1 && sleep 2 && Q "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
waitn "SHELL: Ende" 1 || ok=0
wait $QP
tr -d '\r' < $SER | grep -E "SHELL|A:\\\\|RD: (alle|Speicher voll|Fehler)"
chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; ok=0; fi; }
chk "Ordner TEST geloescht"              "! mdir -i $IMG ::/TEST >/dev/null 2>&1"
chk "RD2\\TEST\\FDAPM.BAK kopiert"       "mdir -i $IMG ::/RD2/TEST/FDAPM.BAK >/dev/null 2>&1"
chk "Kopie gleich dem Original"          "mtype -i $IMG ::/FDAPM.COM | cmp -s - <(mtype -i $IMG ::/RD2/TEST/FDAPM.BAK)"
chk "Original FDAPM.COM noch da"         "mdir -i $IMG ::/FDAPM.COM >/dev/null 2>&1"
chk "Speicher zurueck"                   "tr -d '\r' < $SER | grep -q 'Speicher vollstaendig zurueck'"
[ $ok = 1 ] && echo "Dateioperationen: alles ok"
exit 0
