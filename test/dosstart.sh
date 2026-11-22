#!/bin/bash
# DOS-Startarten (PLAN_DOS Schritte 2-4), DOSBox 386 mit 4 MB und XMS, RD2 mit DOSART.EXE:
# B  .BAT ueber COMMAND.COM /C (FreeCOM im RD2-Ordner), Parameter kommt an
# H  6 offene RD2-Dateien kosten das DOS-Programm keine Dateiplaetze (nicht vererbt)
# K  Tasten im Puffer vor dem Start sind weg
# A/V  Startart Schnell und Voll: bei Voll hat das Programm deutlich mehr XMS, RD2 startet danach neu
# T  residentes Programm: RunDos meldet RD_ERR_RESIDENT (Shell bietet dann Startart Voll an)
cd "$(dirname "$0")/.."
R=obj/dbxs; F=0
chk() { if echo "$L" | grep -q "$1"; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
lauf() {   # $1 Buchstabe
  python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  cp out/RD2.EXE out/*.MOD out/*.DRV out/RDSWAP.BIN out/DOSART.EXE out/TSRTEST.EXE out/COMMAND.COM obj/DOSCHK.COM $R/RD2/
  printf '@echo off\r\necho %%1> C:\\BAT.TXT\r\n' > $R/RD2/T.BAT
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=vgaonly\nmemsize=4\n[cpu]\ncore=normal\ncputype=386\ncycles=fixed 4000\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[dos]\nxms=true\numb=false\n' > $R/dbx.conf
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -s KILL ${TO:-150} dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" \
    -c "C:\\RD2\\RD2.EXE /D DOSART.EXE $1 > \\LOG$1.TXT" -c exit >/dev/null 2>&1
  L=$(tr -d '\r' < $R/LOG$1.TXT 2>/dev/null)
}
lauf B; echo "=== B: .BAT"
chk "DOSART: BAT Rueckgabe 0x00000000" ".BAT ueber COMMAND.COM /C gestartet"
chk "^hallo" "Parameter %1 kommt in der .BAT an (C:\\BAT.TXT)"
chk "Speicher vollstaendig zurueck" "Speicher zurueck"
lauf H; echo "=== H: offene RD2-Dateien"
N=$(echo "$L" | grep "freie Dateiplaetze" | awk '{print $NF}'); N=$((N))
[ $N -ge 14 ] && echo "ok     DOS-Programm hat $N freie Dateiplaetze (6 offene RD2-Dateien nicht vererbt)" || { echo "FEHLER nur $N freie Dateiplaetze"; F=1; }
chk "Speicher vollstaendig zurueck" "Speicher zurueck"
lauf K; echo "=== K: Tastaturpuffer"
chk "DOSART: Taste im Puffer 0x00000000" "Tastaturpuffer beim Start leer"
lauf A; XA=$(tr -d '\r' < $R/XMSA.TXT 2>/dev/null | head -1)
echo "=== A: Startart Schnell, XMS frei fuer das Programm: ${XA:-?} KB"
chk "DOSART: Schnell Rueckgabe 0x00000000" "Startart Schnell kommt zurueck"
lauf V; XV=$(tr -d '\r' < $R/XMSV.TXT 2>/dev/null | head -1)
echo "=== V: Startart Voll, XMS frei fuer das Programm: ${XV:-?} KB"
[ -n "$XV" ] && [ -n "$XA" ] && [ "$XV" -gt "$XA" ] && echo "ok     Voll: Programm hat mehr XMS ($XV statt $XA KB)" || { echo "FEHLER Voll: XMS $XV / $XA"; F=1; }
chk "DOSART: nach Neustart wieder da" "Voll: RD2 startet danach neu"
! echo "$L" | grep -q "FEHLER, Voll kam zurueck" && echo "ok     Voll: RunDos kehrt nicht zurueck" || { echo "FEHLER Voll kam zurueck"; F=1; }
lauf T; echo "=== T: residentes Programm"
chk "DOSART: TSR gemeldet 0x00000001" "RunDos meldet RD_ERR_RESIDENT"
chk "residentes Programm entfernt" "Rest entfernt es wie bisher"
chk "Speicher vollstaendig zurueck" "Speicher zurueck"
[ $F = 0 ] && echo "Startarten: alles ok"
exit $F
