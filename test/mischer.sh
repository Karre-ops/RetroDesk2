#!/bin/bash
# PLAN_API 4: Mischer SndMix* (RDMIX.MOD) mit QEMU-SB16 (DSP 4.05: 16 Bit stereo ueber H5, 8 Bit mono nach SndMixOut).
# MIXTEST.EXE prueft Endmeldungen, Schleife, Stopp, acht Kanaele, Wechsel mit SndSamplePlay. Die Tonaufnahme (QEMU wav)
# muss in der ersten halben Sekunde 440 Hz und 1000 Hz zugleich enthalten (Fourier, numpy).
cd "$(dirname "$0")/.."
SER=obj/mix_com1.txt; IMG=obj/mix.img; WAV=obj/mix.wav; F=0
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS /CGA A:\\RD2\\MIXTEST.EXE 6\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_mix.txt
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_mix.txt >/dev/null 2>&1 || exit 1
printf '[System]\r\nSound=SB\r\nSB=A220 I5 D1 H5\r\n' > obj/mix_ini.txt; mcopy -o -i $IMG obj/mix_ini.txt ::/RD2/RD2.INI
rm -f $SER $WAV
timeout 150 qemu-system-i386 -m 2 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -audiodev wav,id=a0,path=$WAV -machine pcspk-audiodev=a0 -device sb16,audiodev=a0 -device adlib,audiodev=a0 -serial file:$SER >/dev/null 2>&1
L=$(tr -d '\r' < $SER 2>/dev/null)
echo "$L" | grep "MIXTEST: .*nach ms" | sed 's/^/       /'
pruef "MIXTEST: alle Pruefungen" "echo \"\$L\" | grep -q 'MIXTEST: fertig' && ! echo \"\$L\" | grep -q 'FALSCH'"
pruef "Speicher vollstaendig zurueck" "echo \"\$L\" | grep -q 'RD2: Speicher vollstaendig zurueck'"
[ $F = 0 ] || echo "$L" | grep -B1 FALSCH | head -12 | sed 's/^/       /'
R=$(python3 - "$WAV" <<'PY'
import sys, wave, numpy as np
w = wave.open(sys.argv[1]); r = w.getframerate(); ch = w.getnchannels(); d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float)
d = d.reshape(-1, ch).mean(axis=1)
nz = np.nonzero(np.abs(d) > 500)[0]
if len(nz) == 0: print("still"); sys.exit()
s = d[nz[0] + r // 20: nz[0] + r // 20 + r // 4]          # 50..300 ms nach Beginn: beide Toene
f = np.abs(np.fft.rfft(s * np.hanning(len(s)))); hz = np.fft.rfftfreq(len(s), 1 / r)
def pk(x): m = (hz > x - 15) & (hz < x + 15); return f[m].max()
ref = np.median(f[(hz > 100) & (hz < 3000)])
print("440:%.0f 1000:%.0f 700:%.0f" % (pk(440) / ref, pk(1000) / ref, pk(700) / ref))
PY
)
echo "       Fourier (Vielfaches des Medians): $R"
pruef "Aufnahme: 440 Hz und 1000 Hz zugleich" "python3 -c \"import sys; d=dict(x.split(':') for x in '$R'.split()); sys.exit(0 if float(d['440'])>20 and float(d['1000'])>20 and float(d['700'])<10 else 1)\" 2>/dev/null"
# Aeltere Karten in DOSBox (ohne Tonaufnahme): SB 1.x (Einzelbloecke), SB 2.0 (auto-init), SB Pro (Stereo, High-Speed)
R=$PWD/obj/mixdbx
for k in "sb1 1" "sb2 2" "sbpro2 P"; do set -- $k
  python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  for f in $(awk '$4=="S" && $1!="RD2SETUP.EXE"{print $1}' tools/budget.txt) MIXTEST.EXE SB.DRV RDMIX.MOD; do [ -f out/$f ] && cp out/$f $R/RD2/; done
  printf '[System]\r\nSound=SB\r\nSB=A220 I7 D1\r\n' > $R/RD2/RD2.INI
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=vgaonly\nmemsize=4\n[cpu]\ncore=normal\ncputype=386\ncycles=fixed 20000\n[mixer]\nnosound=false\nrate=22050\n[speaker]\npcspeaker=false\n[sblaster]\nsbtype=%s\nsbbase=220\nirq=7\ndma=1\nhdma=5\noplmode=auto\n' $1 > $R/dbx.conf
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 150 dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" -c "C:\\RD2\\RD2.EXE /NOMAUS /CGA C:\\RD2\\MIXTEST.EXE ${2}F > \\LOG.TXT" -c "exit" >/dev/null 2>&1
  L=$(tr -d '\r' < $R/RD2/MIX.TXT 2>/dev/null)
  if echo "$L" | grep -q 'MIXTEST: fertig' && ! echo "$L" | grep -q FALSCH; then echo "ok     DOSBox $1: alle Pruefungen"; else echo "FEHLER DOSBox $1"; echo "$L" | grep -E "SndCaps|B FALSCH" | head -3 | sed 's/^/       /'; echo "$L" | grep -B1 FALSCH | head -8 | sed 's/^/       /'; F=1; fi
done
[ $F = 0 ] && echo "mischer: alle ok" || echo "mischer: FEHLER"
exit $F
