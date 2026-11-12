#!/bin/bash
# Geschwindigkeit aller Grafikarten (30.10.2026): BENCH2 unter RD2.EXE in DOSBox, Grafik= aus RD2.INI,
# gleiche Takte wie test/bench.sh (8086, CYC Takte/ms; VESA-Treiber sind 386-Code: cputype 386, gleiche Takte).
# Aufruf: bash test/bench_modi.sh [MODUS ...]   Ergebnis: obj/bench_modi/<MODUS>.txt, Tabelle obj/bench_modi.txt
cd "$(dirname "$0")/.."
CYC=${CYC:-1000}; mkdir -p obj/bench_modi
ALLE="CGA CGA4 HGC VGA VGA4 VGA16 MCGA EGAM EGA4 EGA EGA200 EGA320 VESA VESA800 VESA1024 SVGA16"
for g in ${@:-$ALLE}; do
  case $g in CGA|CGA4) mach=cga;; HGC) mach=hercules;; EGA*) mach=ega;; VESA*|SVGA16) mach=svga_s3;; *) mach=vgaonly;; esac
  cpu=8086; case $g in VESA*) cpu=386;; esac
  R=obj/dbxm_$g; python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  cp out/RD2.EXE out/*.MOD out/*.DRV out/*.FNT out/RDSWAP.BIN out/BENCH2.EXE $R/RD2/ 2>/dev/null
  printf '[System]\nMaus=DOS\nGrafik=%s\n' $g > $R/RD2/RD2.INI
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=%s\nmemsize=1\n[cpu]\ncore=normal\ncputype=%s\ncycles=fixed %s\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[dos]\nxms=false\numb=false\n' $mach $cpu $CYC > $R/dbx.conf
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 200 dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" \
    -c "C:\\RD2\\RD2.EXE /D BENCH2.EXE > C:\\B2.TXT" -c exit >/dev/null 2>&1
  tr -d '\r' < $R/B2.TXT 2>/dev/null > obj/bench_modi/$g.txt
  echo "$g fertig ($mach, $cpu): $(grep -c 'BENCH2: us' obj/bench_modi/$g.txt) Werte, Treiber $(grep -o '[A-Z0-9]*\.DRV' obj/bench_modi/$g.txt | grep -v SPEAKER | head -1)"
done
