#!/bin/bash
# Menueleiste mit der Maus (30.10.2026): von einem Titel der Shell nach links auf "RD" (klappt auf), von RD nach rechts
# zurueck auf Datei, Pfeil links von Datei auf RD, Klick auf RD nach einem Menue.
# Maus: QEMU mouse_button 1 = links, 2 = rechts. RDMAUS: 1 QEMU-Einheit = 1 Pixel waagrecht, 0,5 senkrecht (Wege unten = 3x die CTMOUSE-Werte); jeder Weg beginnt oben links (0,0).
cd "$(dirname "$0")/.."
IMG=obj/mleiste.img; SER=obj/mleiste_com1.txt; SOCK=/tmp/qmon_mleiste.sock
printf '@echo off\nA:\\RDMAUS.COM\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_mleiste.txt
bash tools/mkdisk.sh $IMG obj/ae_mleiste.txt >/dev/null
mcopy -i $IMG out/RDMAUS.COM ::/
rm -f $SER $SOCK obj/mleiste?.ppm; touch $SER
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_mleiste.py
Q() { python3 obj/qmon_mleiste.py "$@"; }
waitn() { for i in $(seq 400); do [ "$(tr -d '\r' < $SER | grep -c "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
null() { Q "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.4; }   # RDMAUS: 0,5 Pixel je Einheit senkrecht, daher 4 Schritte
tippe() { local a=(); for c in $(echo "$1" | sed 's/./& /g'); do a+=("sendkey $c" sleep0.15); done; Q "${a[@]}"; }
ok=1
waitn "SHELL: bereit" 1 && sleep 3
null
Q "mouse_move 120 14" sleep0.6 "mouse_button 1" sleep0.2 "mouse_button 0" sleep1.0 "mouse_move -60 0" sleep0.8 "mouse_move -45 0" sleep1.2 "screendump obj/mleiste1.ppm"
Q "mouse_move 45 0" sleep1.2 "screendump obj/mleiste2.ppm" sleep0.3 "sendkey esc" sleep0.8
Q "sendkey f10" sleep1.0 "sendkey left" sleep1.2 "screendump obj/mleiste3.ppm" sleep0.3 "sendkey esc" sleep0.8
Q "mouse_move 60 0" sleep0.6 "mouse_button 1" sleep0.2 "mouse_button 0" sleep1.0 "sendkey esc" sleep0.8 "mouse_move -105 0" sleep0.6 "mouse_button 1" sleep0.2 "mouse_button 0" sleep1.2 "screendump obj/mleiste4.ppm" sleep0.3 "sendkey esc" sleep0.8
Q "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
waitn "SHELL: Ende" 1 || ok=0
wait $QP
tr -d '\r' < $SER | grep -E "RD: (alle|Speicher voll|Fehler)"
chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; ok=0; fi; }
chk "Speicher zurueck"                    "tr -d '\r' < $SER | grep -q 'Speicher vollstaendig zurueck'"
[ -f test/ref/mleiste1.png ] && { python3 test/bildvergleich.py obj/mleiste1.ppm test/ref/mleiste1.png obj/mleiste2.ppm test/ref/mleiste2.png obj/mleiste3.ppm test/ref/mleiste3.png obj/mleiste4.ppm test/ref/mleiste4.png || ok=0; }
[ $ok = 1 ] && echo "Menueleiste mit RD: alles ok"
exit 0
