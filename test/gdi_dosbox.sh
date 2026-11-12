#!/bin/bash
# Phase 3: Testbild GDITEST in DOSBox auf CGA, Hercules und VGA (8086, 2000 Zyklen ~ XT).
# GDITEST schreibt den Bildspeicher nach SHOT.BIN; Vergleich mit test/ref/${B}_<karte>.png.
cd "$(dirname "$0")/.."
P=${PROG:-GDITEST}; B=${BILD:-gdi}      # PROG=TEXTTEST BILD=text: Schrift-Testbild
R=obj/dbx; F=0
for k in ${KARTEN:-cga hgc vga}; do
  case $k in vga) MACH=vgaonly; OPT=/VGA;; hgc) MACH=hercules; OPT=/HGC;; *) MACH=cga; OPT=/CGA;; esac
  python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  cp out/RD2.EXE out/*.MOD out/*.DRV out/*.FNT out/$P.EXE $R/RD2/
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=%s\nmemsize=1\n[cpu]\ncore=normal\ncputype=8086\ncycles=fixed 2000\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[serial]\nserial1=file file:%s/com1.txt\n' $MACH $PWD/$R > $R/dbx.conf
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 240 dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" \
    -c "C:\\RD2\\RD2.EXE /D /NOMAUS $OPT $P.EXE $STUFE > \\LOG.TXT" -c exit >/dev/null 2>&1
  echo "--- $k"
  tr -d '\r' < $R/LOG.TXT | sed -n '/^-$/,$p' | grep -E "$P|RDGDI|SPEICHER|Speicher voll|Fehler"
  rm -f obj/${B}_$k.ppm obj/${B}_${k}_gross.png   # kein altes Bild vergleichen
  if [ -f $R/RD2/SHOT.BIN ]; then
    cp $R/RD2/SHOT.BIN obj/${B}_$k.bin; python3 tools/shot.py obj/${B}_$k.bin $k obj/${B}_${k}_gross.png
    python3 -c "from PIL import Image; import sys; w,h={'cga':(640,200),'hgc':(720,348),'vga':(640,480)}['$k']; Image.open('obj/${B}_${k}_gross.png').convert('1').resize((w,h),Image.NEAREST).save('obj/${B}_${k}.ppm')"
    python3 test/bildvergleich.py obj/${B}_$k.ppm test/ref/${B}_$k.png || F=1
  else echo "FEHLER kein SHOT.BIN"; F=1; fi
done
exit $F
