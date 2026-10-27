#!/bin/bash
# Punkt 10 Hilfe: HILFE.EXE im Testmodus (VGA): Inhalt, Tab/Eingabe folgt Verweis, Rueckschritt zurueck, Bilder.
# Danach NOTIZ.EXE: F1 startet ueber HelpShow (RDKX -> RDPRN.MOD Nr. 6) die Hilfe.
cd "$(dirname "$0")/.."
IMG=obj/hilfe.img; SER=obj/hilfe_com1.txt; S=/tmp/qmon_hilfe.sock; export QMON=$S; F=0
cat > obj/ae_hilfe.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\HILFE.EXE T RD2.HLP 0
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\NOTIZ.EXE
A:\FDAPM.COM POWEROFF
AE
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') HILFE.EXE RD2.HLP RDPRN.MOD NOTIZ.EXE VGA16.DRV" FMT=720 bash tools/mkdisk.sh $IMG obj/ae_hilfe.txt >/dev/null 2>&1
rm -f $SER $S obj/hilfe?.ppm
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 ../test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { for i in $(seq 300); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
ok() { if waitn "$1" ${3:-1}; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
ok "HILFE: bereit" "Hilfe startet"
ok "HILFE: Thema 0 Inhalt" "Thema 0 Inhalt geladen"
grep -q "HILFE: Verweise.*000D" $SER && echo "ok     13 Verweise im Inhalt" || { echo "FEHLER Zahl der Verweise"; grep "Verweise" $SER | head -2; F=1; }
sleep 2; Q "screendump obj/hilfe1.ppm"
Q "sendkey tab"; sleep 0.5; Q "sendkey ret"
ok "HILFE: Thema 2 Der Schreibtisch" "Tab + Eingabe folgt dem 2. Verweis"
sleep 1; Q "screendump obj/hilfe2.ppm"
Q "sendkey backspace"; ok "HILFE: Thema 0 Inhalt" "Rueckschritt zurueck zum Inhalt" 2
Q "sendkey esc"
sleep 6; Q "sendkey f1"
ok "HILFE: Start" "Notizblock: F1 startet die Hilfe" 2
sleep 3; Q "screendump obj/hilfe3.ppm"; Q "sendkey esc"; sleep 1; Q "sendkey esc"; sleep 1; Q "sendkey n"
wait $QP 2>/dev/null
for i in 1 2 3; do [ -f obj/hilfe$i.ppm ] && python3 -c "from PIL import Image; Image.open('obj/hilfe$i.ppm').save('obj/hilfe$i.png')"; done
[ $F = 0 ] && echo "Hilfe: alles ok"
exit $F
