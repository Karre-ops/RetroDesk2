#!/bin/bash
# Shell Teil 3 (VGA, PS/2-Maus mit RDMAUS): Ordner ZIEL anlegen (F7), CONFIG.SYS mit der Maus auf ZIEL
# ziehen (gleiches Laufwerk = verschieben), Rechtsklick auf Laufwerk A: -> Eigenschaften.
# Maus: QEMU mouse_button 1 = links, 2 = rechts. RDMAUS: 1 QEMU-Einheit = 1 Pixel waagrecht, 0,5 senkrecht (Wege unten = 3x die CTMOUSE-Werte); jeder Weg beginnt oben links (0,0).
# Ordnerfenster: Client ab (50,72), darunter Laufwerksleiste (18 hoch), Zellen ab y 90, Zelle 100 x 54.
# Danach ZIEL oeffnen, CONFIG.SYS auf "A:" der Laufwerksleiste ziehen (zurueck nach A:\), Klick auf "A:" in der Laufwerksleiste: Ordner A:\ wird neu gelesen. Bilder obj/desk3_zug.ppm, obj/desk3_lw.ppm.
cd "$(dirname "$0")/.."
IMG=obj/desk3.img; SER=obj/desk3_com1.txt; SOCK=/tmp/qmon_desk3.sock
printf '@echo off\nA:\\RDMAUS.COM\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_desk3.txt
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_desk3.txt >/dev/null
mcopy -i $IMG out/RDMAUS.COM ::/
rm -f $SER $SOCK obj/desk3_zug.ppm obj/desk3_lw.ppm; touch $SER
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_desk3.py
Q() { python3 obj/qmon_desk3.py "$@"; }
waitn() { for i in $(seq 400); do [ "$(tr -d '\r' < $SER | grep -c "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
null() { Q "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.4; }   # RDMAUS: 0,5 Pixel je Einheit senkrecht, daher 4 Schritte
tippe() { local a=(); for c in $(echo "$1" | sed 's/./& /g'); do a+=("sendkey $c" sleep0.15); done; Q "${a[@]}"; }
ok=1
waitn "SHELL: bereit" 1 && sleep 3 && Q "sendkey ret"
waitn 'SHELL: Ordner A:\\ ' 1 && sleep 2 && Q "sendkey f7" sleep1.5 && tippe ziel && Q "sendkey ret"
if waitn "SHELL: Ordner angelegt" 1; then
  sleep 2; null
  Q "mouse_move 201 324" sleep0.6 "mouse_button 1" sleep0.4 "mouse_move 0 -18" sleep0.3 "mouse_move 0 -36" sleep0.3 "mouse_move 0 -54" sleep0.5 "screendump obj/desk3_zug.ppm" sleep0.3 "mouse_button 0"
fi
waitn "SHELL: Systemwarnung" 1 && sleep 1.5 && Q "screendump obj/desk3_warn.ppm" sleep0.3 "sendkey tab" sleep0.4 "sendkey ret"   # CONFIG.SYS: Ja
if waitn "SHELL: verschoben" 1; then
  sleep 2; null
  # ZIEL oeffnen (Klick + Eingabe), CONFIG.SYS auf "A:" in der Laufwerksleiste ziehen: zurueck nach A:\
  Q "mouse_move 201 216" sleep0.6 "mouse_button 1" sleep0.2 "mouse_button 0" sleep0.5 "sendkey ret"
  waitn 'SHELL: Ordner A:\\ZIEL\\ ' 1 && sleep 2 && null
  Q "mouse_move 201 216" sleep0.6 "mouse_button 1" sleep0.4 "mouse_move -40 -20" sleep0.3 "mouse_move -50 -20" sleep0.3 "mouse_move -51 -16" sleep0.5 "mouse_button 0"
  waitn "SHELL: verschoben" 2 && sleep 2 && null
  c0=$(tr -d '\r' < $SER | grep -c 'SHELL: Ordner A:\\ ')
  Q "mouse_move 60 160" sleep0.6 "mouse_button 1" sleep0.2 "mouse_button 0"
  waitn 'SHELL: Ordner A:\\ ' $((c0 + 1)) && lst=1
  sleep 2; null
  Q "mouse_move 36 93" sleep0.6 "mouse_button 2" sleep0.2 "mouse_button 0" sleep1.0 "screendump obj/desk3_pop.ppm" sleep0.3 "sendkey e"
fi
sleep 3; Q "screendump obj/desk3_lw.ppm" sleep0.3 "sendkey ret"
sleep 1.5; Q "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
waitn "SHELL: Ende" 1 || ok=0
wait $QP
tr -d '\r' < $SER | grep -E "SHELL|^A:\\\\[A-Z]*\\\\?$|RD: (alle|Speicher voll|Fehler)"
chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; ok=0; fi; }
chk "Ziel A:\\ZIEL\\ gemeldet"            "tr -d '\r' < $SER | grep -qx 'A:\\\\ZIEL\\\\'"
chk "CONFIG.SYS nach ZIEL verschoben"     "tr -d '\r' < $SER | grep -q 'SHELL: verschoben'"
chk "Leiste als Ziel: CONFIG.SYS auf A: zurueck" "mdir -i $IMG ::/CONFIG.SYS >/dev/null 2>&1 && ! mdir -i $IMG ::/ZIEL/CONFIG.SYS >/dev/null 2>&1 && [ \$(tr -d '\r' < $SER | grep -c 'SHELL: verschoben') -ge 2 ]"
chk "Laufwerksinfo gezeigt"               "tr -d '\r' < $SER | grep -q 'SHELL: Laufwerksinfo'"
chk "Laufwerksleiste: Klick auf A: liest A:\\ neu" "[ \"\$lst\" = 1 ]"
chk "Speicher zurueck"                    "tr -d '\r' < $SER | grep -q 'Speicher vollstaendig zurueck'"
python3 test/bildvergleich.py obj/desk3_zug.ppm test/ref/desk3_zug.png obj/desk3_lw.ppm test/ref/desk3_lw.png || ok=0
[ $ok = 1 ] && echo "Ziehen und Laufwerksmenue: alles ok"
exit 0
