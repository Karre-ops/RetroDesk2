#!/bin/bash
# Esc bricht Kopieren ab (30.10.2026): 2,88-MB-Testdiskette mit GROSS.DAT (1 MB), in A:\ "G" springt zur Datei,
# F5 nach A:\RD2, kurz danach Esc. Erwartet: "SHELL: Abbruch (Esc)", keine halbe Kopie A:\RD2\GROSS.DAT,
# Original unveraendert, danach Umschalt+F10 an FDAPM.COM (Bild obj/kopabbr_menu.ppm: Menue Datei an der Auswahl), danach normales Kopieren von FDAPM.COM geht noch (Shell nicht verklemmt), Speicher zurueck.
cd "$(dirname "$0")/.."
IMG=obj/kopabbr.img; SER=obj/kopabbr_com1.txt; SOCK=/tmp/qmon_kopabbr.sock
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_kopabbr.txt
FMT=2880 bash tools/mkdisk.sh $IMG obj/ae_kopabbr.txt
head -c 1048576 /dev/urandom > obj/gross.dat; mcopy -o -i $IMG obj/gross.dat ::/GROSS.DAT || { echo "FEHLER GROSS.DAT passt nicht"; exit 0; }
rm -f $SER $SOCK obj/kopabbr_menu.ppm; touch $SER
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_kopabbr.py
Q() { python3 obj/qmon_kopabbr.py "$@"; }
waitn() { for i in $(seq 400); do [ "$(tr -d '\r' < $SER | grep -c "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
tippe() { local a=(); for c in $(echo "$1" | sed 's/./& /g'); do a+=("sendkey $c" sleep0.15); done; Q "${a[@]}"; }
ok=1
waitn "SHELL: bereit" 1 && sleep 3 && Q "sendkey ret"
waitn 'SHELL: Ordner A:\\ ' 1 && sleep 2 && Q "sendkey g" sleep0.3 "sendkey f5" sleep1.5 && tippe rd2 && Q "sendkey ret" sleep0.4 "sendkey esc"
waitn "SHELL: kopiert" 1 && sleep 2 && Q "sendkey f" sleep0.3 "sendkey f5" sleep1.5 && tippe rd2 && Q "sendkey ret"
waitn "SHELL: kopiert" 2 && sleep 2 && Q "sendkey f" sleep0.5 "sendkey shift-f10" sleep1.5 "screendump obj/kopabbr_menu.ppm" sleep0.3 "sendkey esc" sleep1 "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
waitn "SHELL: Ende" 1 || ok=0
wait $QP
tr -d '\r' < $SER | grep -E "SHELL|RD: (alle|Speicher voll|Fehler)"
chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; ok=0; fi; }
chk "Esc erkannt"                        "tr -d '\r' < $SER | grep -q 'SHELL: Abbruch (Esc)'"
chk "keine halbe Kopie RD2\\GROSS.DAT"    "! mdir -i $IMG ::/RD2/GROSS.DAT >/dev/null 2>&1"
chk "Original GROSS.DAT unveraendert"    "mtype -i $IMG ::/GROSS.DAT | cmp -s - obj/gross.dat"
chk "danach FDAPM.COM kopiert"           "mtype -i $IMG ::/FDAPM.COM | cmp -s - <(mtype -i $IMG ::/RD2/FDAPM.COM)"
chk "Speicher zurueck"                   "tr -d '\r' < $SER | grep -q 'Speicher vollstaendig zurueck'"
python3 test/bildvergleich.py obj/kopabbr_menu.ppm test/ref/kopabbr_menu.png || ok=0
exit 0
