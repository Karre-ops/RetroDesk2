#!/bin/bash
# Systemklaenge wie RD1 (31.10.2026, PC10-III "kein Ton in RD2"): Sound=ADLIB mit QEMU-AdLib, Ton als WAV aufgenommen.
# Erwartet: START beim Start der Shell, FRAGE bei der Rueckfrage Beenden, ENDE beim Beenden (drei Tonfolgen).
cd "$(dirname "$0")/.."
IMG=obj/klsh.img; SER=obj/klsh_com1.txt; S=/tmp/qmon_klsh.sock; export QMON=$S; WAV=obj/klsh.wav; F=0
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*) kill ${p#/proc/} 2>/dev/null;; esac; done
printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_klsh.txt
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_klsh.txt >/dev/null 2>&1
printf '[System]\r\nSound=ADLIB\r\n' > obj/klsh_ini.txt; mcopy -o -i $IMG obj/klsh_ini.txt ::/RD2/RD2.INI; mcopy -o -i $IMG out/ADLIB.DRV ::/RD2/
rm -f $SER $S $WAV
timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -audiodev wav,id=a0,path=$WAV -machine pcspk-audiodev=a0 -device adlib,audiodev=a0 -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
for i in $(seq 300); do grep -q "bereit" $SER 2>/dev/null && break; sleep 0.2; done
sleep 4; Q "sendkey alt-spc" sleep1 "sendkey b" sleep2.5 "sendkey ret"
wait $QP 2>/dev/null
python3 - "$WAV" <<'PY'
import wave, array, sys
w = wave.open(sys.argv[1]); r = w.getframerate(); c = w.getnchannels(); a = array.array('h', w.readframes(w.getnframes()))
b = r // 20 * c; laut = [max((abs(x) for x in a[i:i + b]), default=0) > 1500 for i in range(0, len(a), b)]
folgen = []; i = 0
while i < len(laut):
    if laut[i]:
        j = i
        while j < len(laut) and any(laut[j:j + 6]): j += 1
        folgen.append((i / 20, (j - i) / 20)); i = j
    else: i += 1
print(' '.join(f'{t:.1f}s/{d:.2f}s' for t, d in folgen))
sys.exit(0 if len(folgen) == 3 else 1)
PY
[ $? = 0 ] && echo "ok     drei Tonfolgen: START, FRAGE, ENDE" || { echo "FEHLER Tonfolgen"; F=1; }
[ $F = 0 ] && echo "Systemklaenge Shell: alles ok"
exit $F
