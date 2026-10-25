#!/bin/bash
# Fenster (Checkliste Punkt 2, VGA, PS/2-Maus mit CTMOUSE): Ordner A:\ oeffnen, an der Ecke rechts unten
# groesser ziehen, mit dem Feld rechts im Titel minimieren, mit Alt+Tab zurueckholen, noch einmal minimieren
# und mit Alt+F6 (jedes BIOS) zurueckholen. QEMU-Einheit = 3 Pixel waagrecht, 1,5 senkrecht; Wege ab (0,0).
# Bilder obj/fen_{gross,min,tab}.ppm.
cd "$(dirname "$0")/.."
IMG=obj/fen.img; SER=obj/fen_com1.txt; SOCK=/tmp/qmon_fen.sock
printf '@echo off\nA:\\CTMOUSE.EXE\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_fen.txt
bash tools/mkdisk.sh $IMG obj/ae_fen.txt >/dev/null
mcopy -i $IMG ../test/qemu/CTMOUSE.EXE ::/
rm -f $SER $SOCK obj/fen_*.ppm; touch $SER
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_fen.py
Q() { python3 obj/qmon_fen.py "$@"; }
waitn() { for i in $(seq 400); do [ "$(tr -d '\r' < $SER | grep -c "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
null() { Q "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.4; }
geh() { null; Q "mouse_move $1 $2" sleep0.6; }
ok=1
waitn "SHELL: bereit" 1 && sleep 3 && Q "sendkey ret"
waitn 'SHELL: Ordner A:\\ ' 1 && sleep 2 && Q "screendump obj/fen_auf.ppm"
if [ -n "$NUR_AUF" ]; then geh ${ECKE:-121 185}; Q "mouse_button 1" sleep0.4 "mouse_move 10 10" sleep0.4 "screendump obj/fen_zeig.ppm" "mouse_button 0"; sleep 2; Q "sendkey alt-f4" sleep1 "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"; wait $QP; exit 0; fi
geh ${ECKE:-121 185}; Q "mouse_button 1" sleep0.4 "mouse_move 10 10" sleep0.3 "mouse_move 10 10" sleep0.3 "mouse_move 10 10" sleep0.5 "mouse_button 0"
waitn "SHELL: Groesse" 1 && sleep 2 && Q "screendump obj/fen_gross.ppm"
geh ${MINI:-144 41}; Q "mouse_button 1" sleep0.2 "mouse_button 0"
waitn "SHELL: Groesse 0x00000000" 1 && sleep 2 && Q "screendump obj/fen_min.ppm"
Q "sendkey alt-tab"
waitn "SHELL: aktiv" 1 && sleep 2 && Q "screendump obj/fen_tab.ppm"
geh ${MINI:-144 41}; Q "mouse_button 1" sleep0.2 "mouse_button 0"
waitn "SHELL: Groesse 0x00000000" 2 && sleep 1.5 && Q "sendkey alt-f6"
waitn "SHELL: aktiv" 2 && sleep 1.5
Q "sendkey alt-f4" sleep1.5 "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
waitn "SHELL: Ende" 1 || ok=0
wait $QP
L=$(tr -d '\r' < $SER)
echo "$L" | grep -E "SHELL: (Groesse|aktiv|Ende)|RD: (alle|Speicher voll|Fehler)"
chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; ok=0; fi; }
G=$(echo "$L" | grep "SHELL: Groesse" | sed -n 1p | awk '{print $NF}')   # vorher Client etwa 302 x 215
chk "Groesse geaendert ($G), breiter und hoeher" "[ -n \"$G\" ] && [ \$(( ($G & 0xFFFF) > 340 && ($G >> 16) > 225 )) = 1 ]"
chk "minimiert (M_SIZE SIZE_MIN), zweimal"  "[ \$(echo \"\$L\" | grep -c 'SHELL: Groesse 0x00000000') -ge 2 ]"
chk "Alt+Tab und Alt+F6 holen es zurueck"   "[ \$(echo \"\$L\" | grep -c 'SHELL: aktiv') -ge 2 ]"
chk "Speicher vollstaendig zurueck"         "echo \"\$L\" | grep -q 'Speicher vollstaendig zurueck'"
[ -f test/ref/fen_gross.png ] && for b in gross min tab; do python3 test/bildvergleich.py obj/fen_$b.ppm test/ref/fen_$b.png || ok=0; done
[ $ok = 1 ] && echo "Fenster: alles ok" || { echo "Fenster: FEHLER"; exit 1; }
