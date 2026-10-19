#!/bin/bash
# Diskettentest: RD2 von einer 720-KB-Diskette booten (QEMU, nur Laufwerk A:, keine Festplatte).
# Phase 1: LEER.EXE direkt unter DOS und ueber den Kern.
# Phase 2: PING startet PONG, RECHNE, MEMTEST: Nachrichten, Zeitgeber, Zeitscheiben, Speicher.
#          Einmal mit Zeitscheiben, einmal mit /KOOP (dann muss RECHNE alles aufhalten).
cd "$(dirname "$0")/.."
IMG=obj/sys720.img; SER=obj/com1.txt
cat > obj/autoexec.txt <<'AE'
@echo off
A:\RD2\LEER.EXE > A:\DIREKT.TXT
if errorlevel 1 echo FEHLERCODE1 >> A:\DIREKT.TXT
A:\RD2\RD.EXE A:\RD2\LEER.EXE hallo welt > A:\RD.TXT
echo === PRAEEMPTIV
A:\RD2\RD.EXE A:\RD2\PING.EXE > A:\RD2.TXT
echo === KOOP
A:\RD2\RD.EXE /KOOP A:\RD2\PING.EXE > A:\RD3.TXT
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/autoexec.txt
rm -f $SER
timeout 180 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a -serial file:$SER >/dev/null 2>&1
echo "qemu rc=$?"
F=0; chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=$((F+1)); fi; }
D=$(mtype -i $IMG ::/DIREKT.TXT 2>/dev/null | tr -d '\r')
S=$(tr -d '\r' < $SER 2>/dev/null)
P1=${S%%RD: RetroDesk 2 Kern (Phase 2)*}               # bis zum zweiten Kernstart: LEER
REST=${S#*Zeitscheiben an}; P2=${REST%%Umschalten nur kooperativ*}; P3=${S#*Umschalten nur kooperativ}
echo "--- Phase 1"
chk "LEER.EXE unter DOS meldet 'benoetigt RetroDesk 2'" '[[ "$D" == *"tigt RetroDesk 2."* ]]'
chk "LEER.EXE unter DOS endet mit Fehlercode 1"       '[[ "$D" == *FEHLERCODE1* ]]'
chk "LEER sieht API 1.0"                 '[[ "$S" == *"LEER: Kern-API 0x00000100"* ]]'
chk "Befehlszeile kommt an"              '[[ "$S" == *"hallo welt"* ]]'
chk "fehlende Funktion: 0 und RD_ERR_NOTSUP" '[[ "$S" == *"SndCaps fehlt noch (erwartet)"* ]]'
chk "LEER endet mit 42"                  '[[ "$S" == *"Task beendet: 0x0001002A"* ]]'
echo "--- Phase 2 mit Zeitscheiben"
chk "PING bekommt 5 Antworten"           '[[ "$P2" == *"PING: Antwort 0x00000005"* ]]'
chk "PONG hat 5 beantwortet"             '[[ "$P2" == *"PONG: Ende, beantwortet 0x00000005"* ]]'
chk "Zeitgeber, waehrend RECHNE rechnet" '[[ "$P2" == *"PING: Timer ok, RECHNE rechnet noch"* ]]'
chk "RECHNE fertig"                      '[[ "$P2" == *"RECHNE: fertig"* ]]'
chk "Speicher gefuellt (4-KB-Bloecke)"   '[[ "$P2" =~ "MEMTEST: 4-KB-Bloecke 0x000000"[0-9A-F][0-9A-F] ]]'
chk "65000 Bytes nach Verdichten"        '! [[ "$P2" == *"MEMTEST: 65000-Byte-Block 0x00000000"* ]] && [[ "$P2" == *"MEMTEST: 65000-Byte-Block"* ]]'
chk "Inhalte nach Verdichten richtig"    '[[ "$P2" == *"MEMTEST: falsche Bloecke nach Verdichten 0x00000000"* ]]'
chk "verwerfbarer Block verworfen"       '[[ "$P2" == *"MEMTEST: verwerfbarer Block verworfen"* ]]'
chk "MEMTEST gibt alles frei"            '[[ "$P2" == *"MEMTEST: alles frei"* ]]'
chk "PING endet mit 7"                   '[[ "$P2" =~ "Task beendet: 0x000"[0-9A-F]"0007" ]]'
chk "Kern: Speicher vollstaendig zurueck" '[[ "$P2" == *"Speicher vollstaendig zurueck"* ]]'
echo "--- Phase 2 kooperativ (/KOOP)"
chk "ohne Zeitscheiben wartet alles auf RECHNE" '[[ "$P3" == *"PING: Timer ok, RECHNE war schon fertig"* ]]'
chk "Kern: Speicher vollstaendig zurueck" '[[ "$P3" == *"Speicher vollstaendig zurueck"* ]]'
python3 tools/budget.py out $IMG | tail -2
echo "--- COM1 (mit Zeitscheiben) ---"; echo "$P2" | head -40
[ $F = 0 ] && echo "Diskettentest: alles ok" || echo "Diskettentest: $F Fehler"
exit $F
