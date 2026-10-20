#!/bin/bash
# Grafikwahl: GfxList/GfxSelect und RD2.INI auf CGA, Hercules und VGA, CPU 8086 bzw. 386.
# Je Karte: Liste, unmoegliche Wahl (muss abgelehnt werden), moegliche Wahl, GDITEST mit
# der Wahl (Hoehe pruefen), zurueck auf automatisch.
cd "$(dirname "$0")/.."
R=obj/dbxw; F=0
for k in cga hgc vga; do
  case $k in vga) MACH=vgaonly; NEIN=f; JA=c; HJA=200;; hgc) MACH=hercules; NEIN=c; JA=h; HJA=348;; *) MACH=cga; NEIN=v; JA=c; HJA=200;; esac
  for cpu in ${CPUS:-8086 386}; do
    python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
    cp out/RD.EXE out/*.MOD out/*.DRV out/GDITEST.EXE out/GFXWAHL.EXE $R/RD2/
    printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=%s\nmemsize=1\n[cpu]\ncore=normal\ncputype=%s\ncycles=fixed 3000\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n' $MACH $cpu > $R/dbx.conf
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 200 dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" \
      -c "C:\\RD2\\RD.EXE /D GFXWAHL.EXE > \\L1.TXT" -c "C:\\RD2\\RD.EXE /D GFXWAHL.EXE $NEIN > \\L2.TXT" \
      -c "C:\\RD2\\RD.EXE /D GFXWAHL.EXE $JA > \\L3.TXT" -c "C:\\RD2\\RD.EXE /D GDITEST.EXE > \\L4.TXT" \
      -c "C:\\RD2\\RD.EXE /D GFXWAHL.EXE a > \\L5.TXT" -c exit >/dev/null 2>&1
    echo "=== $k, cputype=$cpu"
    for n in 1 2 3 4 5; do tr -d '\r' < $R/L$n.TXT 2>/dev/null | sed -n '/^-$/,$p' | grep -E "GFXWAHL|Hoehe|RDGDI"; done
    tr -d '\r' < $R/L2.TXT | grep -q "Fehler 0x00000008" || { echo "FEHLER: unmoegliche Wahl nicht abgelehnt"; F=1; }
    tr -d '\r' < $R/L3.TXT | grep -q "Fehler 0x00000000" || { echo "FEHLER: moegliche Wahl abgelehnt"; F=1; }
    printf -v HX "%08X" $HJA; tr -d '\r' < $R/L4.TXT | grep -q "Hoehe 0x$HX" || { echo "FEHLER: Wahl nicht wirksam (Hoehe $HJA erwartet)"; F=1; }
    tr -d '\r' < $R/L5.TXT | grep -q "Grafik=AUTO" || { echo "FEHLER: zurueck auf AUTO"; F=1; }
  done
done
[ $F = 0 ] && echo "Grafikwahl: alles ok"
exit $F
