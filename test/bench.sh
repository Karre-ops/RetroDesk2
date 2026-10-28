#!/bin/bash
# Leistungsvergleich RD1 gegen RD2 (Prioritaet 1: RD2 darf nicht langsamer sein als RD1).
# DOSBox, 8086 mit festen Takten (XT-nah), CGA, Hercules, VGA mono. BENCH1 = RD1-Renderer,
# BENCH2 = dieselben Aufgaben ueber die RD2-API unter RD.EXE. Ausgabe obj/bench.txt.
cd "$(dirname "$0")/.."
R=obj/dbxb; CYC=${CYC:-1000}; : > obj/bench_roh.txt
MODES=("cga /CGA /C" "hercules /HGC /H" "vgaonly /VGA /V"); [ -n "$NUR_CGA" ] && MODES=("cga /CGA /C")
for m in "${MODES[@]}"; do
  set -- $m
  python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  cp out/RD.EXE out/*.MOD out/*.DRV out/*.FNT out/RDSWAP.BIN out/BENCH2.EXE obj/bench1/BENCH1.EXE rd1ref/RDFONT.F14 $R/RD2/
  printf '[System]\nMaus=DOS\n' > $R/RD2/RD2.INI
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=%s\nmemsize=1\n[cpu]\ncore=normal\ncputype=8086\ncycles=fixed %s\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[dos]\nxms=false\numb=false\n' $1 $CYC > $R/dbx.conf
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 300 dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" \
    -c "BENCH1.EXE $2 C:\\B1.TXT" -c "C:\\RD2\\RD.EXE /D $3 BENCH2.EXE > C:\\B2.TXT" -c exit >/dev/null 2>&1
  echo "== $1" >> obj/bench_roh.txt; tr -d '\r' < $R/B1.TXT 2>/dev/null | sed 's/^/1 /' >> obj/bench_roh.txt
  tr -d '\r' < $R/B2.TXT 2>/dev/null | sed -n '/^-$/,$p' | grep "BENCH2" | sed 's/^/2 /' >> obj/bench_roh.txt
done
python3 - $CYC <<'PY'
import sys, re
cyc = sys.argv[1]; L = open('obj/bench_roh.txt').read().split('\n'); out = []; bad = 0; mode = None; r1 = {}; r2 = []
def flush():
    global bad
    if not mode: return
    out.append(f"\n{mode} (8086, {cyc} Takte/ms)\n{'Aufgabe':34s} {'RD1 us':>9s} {'RD2 us':>9s}  RD2/RD1")
    for name, v in r2:
        a = r1.get(name)
        if a: q = v / a; f = '' if q <= 1.05 else '  LANGSAMER'; bad += f != ''
        out.append(f"{name:34s} {a if a else '-':>9} {v:>9}  " + (f"{q:5.2f}{f}" if a else "  (nur RD2)"))
last = None
for l in L:
    if l.startswith('== '): flush(); mode = l[3:]; r1 = {}; r2 = []; continue
    m = re.match(r'1 (.*?)\s+(\d+) us', l)
    if m: r1[m.group(1).strip()] = int(m.group(2)); continue
    m = re.match(r'2 BENCH2: us (0x[0-9A-F]+)', l)
    if m and last: r2.append((last, int(m.group(1), 16))); last = None; continue
    m = re.match(r'2 BENCH2: (.*)$', l)
    if m and not m.group(1).startswith('fertig') and not m.group(1).startswith('us'): last = m.group(1).strip()
flush()
open('obj/bench.txt', 'w').write('\n'.join(out) + '\n'); print('\n'.join(out))
print(f"\nLeistung: {bad} Aufgaben bei RD2 langsamer als RD1 (Grenze +5 %)" if bad else "\nLeistung: RD2 nirgends langsamer als RD1")
PY
