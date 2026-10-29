#!/bin/bash
# Burgenkampf (BURGEN.EXE) in DOSBox, 8086 mit festen Takten (XT-nah): Computer gegen Computer (D) bis zum Sieg,
# auf CGA, Hercules und VGA mono. Bild nach dem ersten Zug (SHOT.BIN), Zeiten fuer Bild und Zielen des Computers.
# KARTEN="cga hgc vga" CYC=2000 OPT="N" (N = ohne 8087)
cd "$(dirname "$0")/.."
R=obj/dbxburg; F=0; CYC=${CYC:-2000}
for k in ${KARTEN:-cga hgc vga}; do
  case $k in vga) MACH=vgaonly; SW=/VGA;; hgc) MACH=hercules; SW=/HGC;; *) MACH=cga; SW=/CGA;; esac
  python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  cp out/RD.EXE out/*.MOD out/*.DRV out/*.FNT out/DE.LNG out/EN.LNG out/BURGEN.EXE $R/RD2/
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=%s\nmemsize=1\n[cpu]\ncore=normal\ncputype=8086\ncycles=fixed %s\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[dos]\nxms=false\numb=false\n' $MACH $CYC > $R/dbx.conf
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout ${ZEIT:-600} dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" \
    -c "C:\\RD2\\RD.EXE /D /NOMAUS $SW BURGEN.EXE TDKZ${OPT}S${START:-4711}L2 > \\LOG.TXT" -c exit >/dev/null 2>&1
  echo "--- $k"
  tr -d '\r' < $R/LOG.TXT | sed -n '/^-$/,$p' | grep -E "BURGEN|SPEICHER|Speicher voll|Fehler" > obj/burgen_$k.log
  grep -E "bereit|Sieger|Grund|Runden|Ende" obj/burgen_$k.log
  echo "Bild ms (ganzes Fenster):" $(grep "Bild ms" obj/burgen_$k.log | head -3 | awk '{print $NF}')
  echo "KI ms:" $(grep "KI ms" obj/burgen_$k.log | awk '{print $NF}' | head -8)
  echo "KI Probeschuesse:" $(grep "KI Probe" obj/burgen_$k.log | awk '{print $NF}' | head -8)
  grep -q "BURGEN: Sieger [01]" obj/burgen_$k.log || { echo "FEHLER kein Sieger"; F=1; }
  grep -q "BURGEN: Ende" obj/burgen_$k.log || { echo "FEHLER Fenster nicht zu"; F=1; }
  grep -qE "SPEICHERLECK|Speicher voll" obj/burgen_$k.log && { echo "FEHLER Speicher"; F=1; }
  if [ -f $R/RD2/SHOT.BIN ]; then cp $R/RD2/SHOT.BIN obj/burgen_$k.bin; python3 tools/shot.py obj/burgen_$k.bin $k obj/burgen_$k.png; echo "Bild obj/burgen_$k.png"
  else echo "FEHLER kein SHOT.BIN"; F=1; fi
done
exit $F
