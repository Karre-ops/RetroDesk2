#!/bin/bash
# Auslagern bei Speichermangel (Checkliste Punkt 1, RDAUSL.MOD): AUSLTEST legt 60 x 16 KB verschiebbar an
# (mehr als frei ist), prueft alle Inhalte, gibt frei. Laeufe: 386 mit XMS auf C:, 8086 ohne XMS auf C:
# (RD2.SWP), 8086 von Diskette A: (kein Auslagern, sauberes RD_ERR_NOMEM), C: mit Auslagern=0 in RD2.INI.
cd "$(dirname "$0")/.."
R=obj/dbxa; F=0
for lauf in "386 true c ja" "8086 false c ja" "8086 false a nein" "8086 false c aus"; do
  set -- $lauf
  python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  cp out/RD2.EXE out/*.MOD out/*.DRV out/*.FNT out/*.LNG out/RDSWAP.BIN out/AUSLTEST.EXE $R/RD2/
  [ $4 = aus ] && printf '[System]\r\nAuslagern=0\r\n' > $R/RD2/RD2.INI || printf '[System]\r\nGrafik=VGAM\r\n' > $R/RD2/RD2.INI
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=vgaonly\nmemsize=4\n[cpu]\ncore=normal\ncputype=%s\ncycles=fixed 20000\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[dos]\nxms=%s\numb=false\n' $1 $2 > $R/dbx.conf
  if [ $3 = a ]; then M=(-c "mount a $R -t floppy" -c "a:"); else M=(-c "mount c $R" -c "c:"); fi
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -s KILL ${TO:-200} dosbox -conf $R/dbx.conf "${M[@]}" -c "cd \\RD2" \
    -c "$3:\\RD2\\RD2.EXE /D AUSLTEST.EXE > \\LOG.TXT" -c exit >/dev/null 2>&1
  L=$(tr -d '\r' < $R/LOG.TXT 2>/dev/null)
  chk() { if echo "$L" | grep -q "$1"; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
  echo "=== $1 xms=$2 Laufwerk $3 Auslagern $4: $(echo "$L" | grep -a "angelegt" | awk '{print $NF}') Bloecke, $(echo "$L" | grep -a "Auslagern nach" | sed 's/, Platte.*//; s/.*Auslagern //')"
  if [ $4 = ja ]; then
    chk "AUSLTEST: angelegt (16-KB-Bloecke) 0x0000003C" "alle 60 Bloecke angelegt"
    chk "AUSLTEST: mehr als frei war 0x00000001" "mehr als in den Speicher passt"
  else
    chk "AUSLTEST: Abbruch mit Fehler" "Anlegen endet mit Fehler (kein Auslagern)"
    echo "$L" | grep -q "Auslagern nach" && { echo "FEHLER Auslagern trotz Diskette/Auslagern=0"; F=1; }
  fi
  chk "AUSLTEST: als verworfen gemeldet 0x00000000" "kein Block als verworfen gemeldet"
  chk "AUSLTEST: Abweichungen 0x00000000" "alle Inhalte richtig"
  chk "AUSLTEST: Abweichungen rueckwaerts 0x00000000" "auch rueckwaerts richtig"
  chk "AUSLTEST: frei am Ende gleich 0x00000001" "freier Speicher wie am Anfang"
  chk "AUSLTEST: fertig" "Test zu Ende"
  chk "Speicher vollstaendig zurueck" "Speicher vollstaendig zurueck"
  [ -f $R/RD2/RD2.SWP ] && { echo "FEHLER RD2.SWP liegt noch da"; F=1; }
done
exit $F
