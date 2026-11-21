#!/bin/bash
# RD2/32-Programme (PLAN_32BIT Schritt 4a), QEMU 486 mit VGA, CWSDPMI -p vor RD2:
# 1. VOLL32 (32 Bit): DPMI-Anmeldung, Bild im XMS, 1 MB Feld fehlerfrei, Fenster, Vollbild 13h mit Farbverlauf direkt in A0000h,
# 2. Alt+Tab: M_SCREENLOST (Rueckruf in den 32-Bit-Code), 3. Alt+Tab: M_SCREENBACK, neu gezeichnet, 4. Esc: Ende, Speicher zurueck.
# 5. ohne DPMI-Host: Meldung statt Absturz.
cd "$(dirname "$0")/.."
IMG=obj/rd32.img; SER=obj/rd32_com1.txt; SOCK=/tmp/qmon_rd32.sock; F=0
Q="python3 test/qemu/qmon.py"; export QMON=$SOCK
pruef() { local t="$1"; if eval "$2"; then echo "ok     $t"; else echo "FEHLER $t"; F=1; fi; }
waitn() { for i in $(seq ${3:-150}); do [ "$(tr -d '\r' < $SER | grep -c "$1")" -ge "${2:-1}" ] && return 0; sleep 0.4; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
bild() { python3 -c "
from PIL import Image
a = Image.open('$1').convert('RGB'); print(a.size[0], a.size[1], len(a.getcolors(1 << 20)))"; }
start() {   # $1 AUTOEXEC-Zeilen
  printf "@echo off\r\n$1A:\\\\FDAPM.COM POWEROFF\r\n" > obj/ae_rd32.txt
  FMT=1440 bash tools/mkdisk.sh $IMG obj/ae_rd32.txt >/dev/null 2>&1
  mcopy -o -i $IMG out/VOLL32.EXE out/FEN32.EXE out/SCHUTZ32.EXE out/TESTC32.EXE out/TESTMOD.EXE out/TESTM32.M32 out/RD32.MOD out/CWSDPMI.EXE ::/RD2/; mcopy -o -i $IMG out/VOLL32.EXE ::/RD2/ZVOLL32.EXE
  [ -n "$GRAFIK" ] && { printf '[System]\r\nGrafik=%s\r\n' $GRAFIK > obj/RD2.INI; mcopy -o -i $IMG out/$GRAFIK.DRV obj/RD2.INI ::/RD2/; }
  rm -f $SOCK $SER; touch $SER
  timeout 150 qemu-system-i386 -cpu 486 -m 16 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
    -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
}
rm -f obj/r32_*.ppm
start 'A:\\RD2\\CWSDPMI.EXE -p\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\SHELL.EXE\r\n'   # ueber die Shell (Alt+Tab braucht ein zweites Fenster)
waitn "SHELL: bereit" 1 250 && sleep 3 && $Q "sendkey ret" >/dev/null 2>&1
waitn 'SHELL: Ordner A:\\ ' && sleep 2 && $Q "sendkey ret" >/dev/null 2>&1
waitn 'SHELL: Ordner A:\\RD2\\ ' && sleep 2 && $Q "sendkey z" sleep0.8 "sendkey ret" >/dev/null 2>&1
waitn "VOLL32: gezeichnet" 1 250 && sleep 1.5 && $Q "screendump obj/r32_1.ppm" sleep0.5 "sendkey alt-tab" >/dev/null 2>&1
waitn "VOLL32: SCREENLOST" 1 && sleep 3 && $Q "screendump obj/r32_2.ppm" sleep0.5 "sendkey alt-tab" >/dev/null 2>&1
waitn "VOLL32: gezeichnet" 2 && sleep 1.5 && $Q "screendump obj/r32_3.ppm" sleep0.5 "sendkey esc" >/dev/null 2>&1
waitn "RD32: Ende" 1 100; sleep 3; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
for n in 1 2 3; do [ -f obj/r32_$n.ppm ] && eval "B$n=\"$(bild obj/r32_$n.ppm)\""; done
echo "       Bilder: 1: $B1 | 2: $B2 | 3: $B3 (Breite Hoehe Farben)"
voll() { set -- $1; [ "$2" = 400 ] && [ "${3:-0}" -ge 40 ]; }
pruef "DPMI-Anmeldung und 32-Bit-Bild im XMS" "echo \"\$L\" | grep -q 'RD32: DPMI angemeldet' && echo \"\$L\" | grep -q 'RD32: Bild bei 0x00[1-9A-F]'"
pruef "32-Bit-Code laeuft, 1 MB Feld ohne Abweichung" "echo \"\$L\" | grep -q 'VOLL32: 1 MB Feld, Abweichungen 0x00000000'"
pruef "Fenster und Vollbild 13h mit Farbverlauf (A0000h aus 32 Bit)" "echo \"\$L\" | grep -q 'VOLL32: Vollbild' && voll \"$B1\""
pruef "Alt+Tab: M_SCREENLOST im 32-Bit-Code" "echo \"\$L\" | grep -q 'VOLL32: SCREENLOST'"
pruef "Alt+Tab zurueck: M_SCREENBACK, neu gezeichnet" "echo \"\$L\" | grep -q 'VOLL32: SCREENBACK' && voll \"$B3\""
pruef "Esc erreicht die 32-Bit-Fensterprozedur, Ende 0" "echo \"\$L\" | grep -q 'VOLL32: Esc' && echo \"\$L\" | grep -q 'RD32: Ende 0x00000000'"
pruef "Shell laeuft weiter, Task sauber beendet" "echo \"\$L\" | grep -q 'RD2: Task beendet: 0x0' && ! echo \"\$L\" | grep -qi 'RD2: Fehler\\|Ausnahme'"
L1="$L"
# ---- Stufe 4b: FEN32 (Fenster, Zeichnen, Puffer-Bitmap, Dialog, Datei, Profil), startet sich ein zweites Mal ----
GRAFIK=VESA start 'A:\\RD2\\CWSDPMI.EXE -p\r\nA:\\RD2\\RD2.EXE A:\\RD2\\FEN32.EXE\r\n'   # VESA 256 Farben: Puffer-Bitmap im 8-Bit-Weg
waitn "FEN32 1: Dialog offen" 1 250 && sleep 2 && $Q "screendump obj/r32_4.ppm" sleep0.5 "sendkey ret" >/dev/null 2>&1
waitn "FEN32 2: gezeichnet" 1 && sleep 0.5 && $Q "screendump obj/r32_5.ppm" >/dev/null 2>&1   # beide Fenster offen
waitn "FEN32 2: Ende" 1 120; waitn "FEN32 1: Ende" 1 60; waitn "RD2: Speicher vollstaendig zurueck" 1 60; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
[ -f obj/r32_5.ppm ] && B5="$(bild obj/r32_5.ppm)"
echo "       Bild Fenster: $B5"
for z in "FEN32 1: Datei 100 KB, Abweichungen 0x00000000" "FEN32 1: Profil ok" "FEN32 1: zweiter Task gestartet" "FEN32 1: Dialog offen" \
         "FEN32 1: Dialog Text zurueck ok" "FEN32 1: Dialog Ergebnis 0x00000001" "FEN32 1: gezeichnet" "FEN32 2: gezeichnet" \
         "FEN32 1: Takt 0x0000000C" "FEN32 2: Takt 0x0000000C" "FEN32 1: Ende" "FEN32 2: Ende"; do
  pruef "$z" "echo \"\$L\" | grep -q '$z'"; done
pruef "zwei 32-Bit-Tasks laufen verschraenkt (Takte beider vor dem ersten Ende)" "[ \$(echo \"\$L\" | grep -n 'FEN32 2: Takt 0x00000003' | cut -d: -f1) -lt \$(echo \"\$L\" | grep -n 'FEN32 1: Ende' | cut -d: -f1) ]"
pruef "Bild: Fenster mit Puffer-Bitmap (mindestens 12 Farben)" "set -- $B5; [ \"\${3:-0}\" -ge 12 ]"
pruef "Speicher nach zwei 32-Bit-Tasks vollstaendig zurueck" "echo \"\$L\" | grep -q 'RD2: Speicher vollstaendig zurueck'"
# ---- Zeitscheiben und Absturzschutz (SCHUTZ32): Endlosschleife, Division durch 0, Seitenfehler ----
start 'A:\\RD2\\CWSDPMI.EXE -p\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\SCHUTZ32.EXE\r\n'
waitn "SCHUTZ32: nach P noch am Leben" 1 300; sleep 3; $Q "sendkey ret" >/dev/null 2>&1; sleep 1; $Q "sendkey ret" >/dev/null 2>&1
waitn "SCHUTZ32: Ende" 1 60; waitn "RD2: Speicher vollstaendig zurueck" 1 60; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
for z in "SCHUTZ32: S gestartet" "SCHUTZ32: 8 Takte waehrend S rechnet" "SCHUTZ32: D teilt durch 0" "RD32: Ausnahme 0x00000000" \
         "SCHUTZ32: nach D noch am Leben" "SCHUTZ32: P schreibt ins Leere" "RD32: Ausnahme 0x0000000E" "SCHUTZ32: nach P noch am Leben" "SCHUTZ32: Ende"; do
  pruef "$z" "echo \"\$L\" | grep -q '$z'"; done
pruef "kein Absturz wurde uebersehen" "! echo \"\$L\" | grep -q 'FEHLER (kein Absturz)'"
pruef "Speicher nach Endlosschleife und zwei Abstuerzen vollstaendig zurueck" "echo \"\$L\" | grep -q 'RD2: Speicher vollstaendig zurueck'"
# ---- 32-Bit-Module (TESTMOD laedt TESTM32.M32) ----
start "A:\\\\RD2\\\\CWSDPMI.EXE -p\\r\\nA:\\\\RD2\\\\RD2.EXE /VGA A:\\\\RD2\\\\TESTMOD.EXE\\r\\n"
waitn "RD2: Speicher vollstaendig zurueck" 1 300; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
echo "$L" | grep -a "TESTMOD FEHLER\|TESTMOD: LastError" | head -5 | sed 's/^/       /'
pruef "Module: laden, Exporte, Laufzeit, API, Fenster im Modul, zwei Ladungen" "echo \"\$L\" | grep -q 'TESTMOD: [0-9]* Pruefungen, Fehler 0' && ! echo \"\$L\" | grep -q 'TESTMOD FEHLER'"
pruef "Module: Ende jeder Ladung (auch am Programmende), Speicher zurueck" "[ \$(echo \"\$L\" | grep -c 'TESTM32: Ende') = 3 ] && echo \"\$L\" | grep -q 'RD32: Ende 0x00000000' && echo \"\$L\" | grep -q 'RD2: Speicher vollstaendig zurueck'"
# ---- C-Laufzeit (TESTC32): mit 387 und mit Gleitkomma in Software ----
for fp in "" S; do
  start "A:\\\\RD2\\\\CWSDPMI.EXE -p\\r\\nA:\\\\RD2\\\\RD2.EXE /VGA A:\\\\RD2\\\\TESTC32.EXE $fp\\r\\n"
  waitn "RD2: Speicher vollstaendig zurueck" 1 300; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
  L=$(tr -d '\r' < $SER); art=$([ -n "$fp" ] && echo "Software" || echo "387")
  echo "$L" | grep -a "TESTC32 FEHLER" | head -5 | sed 's/^/       /'
  pruef "C-Laufzeit ($art): alle Pruefungen ohne Fehler" "echo \"\$L\" | grep -q 'TESTC32: Start, Gleitkomma $art' && echo \"\$L\" | grep -q 'TESTC32: [0-9]* Pruefungen, Fehler 0' && ! echo \"\$L\" | grep -q 'TESTC32 FEHLER'"
  pruef "C-Laufzeit ($art): exit(7), stdout am Ende geleert, Speicher zurueck" "echo \"\$L\" | grep -q 'RD32: Ende 0x00000007' && echo \"\$L\" | grep -q 'TESTC32: Teil 1 ohne Zeilenende' && echo \"\$L\" | grep -q 'RD2: Speicher vollstaendig zurueck'"
done
# ---- zweimal hintereinander: RD2 meldet sich am Ende beim DPMI-Host ab, der zweite Lauf meldet sich neu an ----
start 'A:\\RD2\\CWSDPMI.EXE -p\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\VOLL32.EXE A\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\VOLL32.EXE A\r\n'
waitn "RD2: Speicher vollstaendig zurueck" 2 300; sleep 2; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
pruef "zweimal RD2 mit 32-Bit-Programm: je angemeldet, abgemeldet, Speicher zurueck" "[ \$(echo \"\$L\" | grep -c 'RD32: DPMI angemeldet') = 2 ] && [ \$(echo \"\$L\" | grep -c 'RD32: beim DPMI-Host abgemeldet') = 2 ] && [ \$(echo \"\$L\" | grep -c 'VOLL32: Ende') = 2 ] && [ \$(echo \"\$L\" | grep -c 'RD2: Speicher vollstaendig zurueck') = 2 ]"
# ---- VESA 2.0 linear (SM_LFB) aus 32 Bit ----
start 'A:\\RD2\\CWSDPMI.EXE -p\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\VOLL32.EXE L\r\n'
waitn "VOLL32: gezeichnet" 1 250 && sleep 1.5 && $Q "screendump obj/r32_6.ppm" sleep0.5 "sendkey esc" >/dev/null 2>&1
waitn "RD2: Speicher vollstaendig zurueck" 1 60; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
[ -f obj/r32_6.ppm ] && B6="$(bild obj/r32_6.ppm)"
pruef "VESA 2.0 linear 640x480 mit Farbverlauf ($B6), Esc beendet" "set -- $B6; [ \"\$1\" = 640 ] && [ \"\$2\" = 480 ] && [ \"\${3:-0}\" -ge 40 ] && tr -d '\r' < $SER | grep -q 'VOLL32: Ende'"
start 'A:\\RD2\\RD2.EXE /VGA A:\\RD2\\VOLL32.EXE\r\n'
waitn "RD32: kein DPMI-Host" 1 250; sleep 3; $Q "sendkey ret" >/dev/null 2>&1
waitn "RD2: Speicher vollstaendig zurueck" 1 60; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
pruef "ohne DPMI-Host: Meldung, RD2 endet sauber" "echo \"\$L\" | grep -q 'RD32: kein DPMI-Host' && echo \"\$L\" | grep -q 'RD2: Speicher vollstaendig zurueck'"
[ $F = 0 ] && echo "RD2/32: alles ok" || { echo "RD2/32: FEHLER"; echo "$L1" | tail -25; }
exit $F
