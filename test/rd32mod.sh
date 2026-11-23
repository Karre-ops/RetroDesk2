#!/bin/bash
# RD32MOD.EXE + MKM32.BAT (32-Bit-Module unter DOS) gegen tools/rd32mod.py: TESTM32 wird in DOSBox mit dem
# DOS-wlink von Open Watcom zweimal gelinkt und mit RD32MOD fertiggestellt; TESTM32.M32 muss byte-gleich mit dem
# Linux-Bau (out/TESTM32.M32) sein. Dazu: falsche Eingabe (kein Modulbild) wird gemeldet.
cd "$(dirname "$0")/.."
W=obj/rd32modt; F=0; O=out
python3 -c "import shutil; shutil.rmtree('$W', True)"; mkdir -p $W
cp $O/RD32MOD.EXE sdk/MKM32.BAT apps/test32/TESTM32.LNK apps/test32/TESTM32.EXP $O/RD32M0.OBJ $O/RD32LIB.OBJ $O/RD32API.OBJ $O/RD3D.LIB $O/RD32C.LIB $W/
cp obj/TESTM32.M32.o32 $W/TESTM32.OBJ; cp /opt/ow/lib386/math3r.lib $W/MATH3R.LIB; cp /opt/ow/lib386/dos/clib3r.lib $W/CLIB3R.LIB
cp /opt/ow/binw/wlink.exe /opt/ow/binw/dos4gw.exe $W/; cp /opt/ow/binw/wlink.lnk $W/ 2>/dev/null
printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=vgaonly\nmemsize=16\n[cpu]\ncore=dynamic\ncycles=max\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n' > $W/dbx.conf
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -s KILL 200 dosbox -conf $W/dbx.conf -c "mount c $W" -c "c:" \
  -c "call MKM32 TESTM32" -c "RD32MOD F.M32 TESTM32.MAP TESTM32.MAP TESTM32.MAP TESTM32.EXP > F.TXT" -c exit >/dev/null 2>&1
L=$(python3 -c "
import struct; d=open('$W/TESTM32.M32','rb').read(); n=struct.unpack_from('<I',d,8)[0]; e=struct.unpack_from('<I',d,n)[0]; p=n+4
for i in range(e): p+=5+d[p+4]
print('Bild', n, 'Bytes,', e, 'Exporte,', struct.unpack_from('<I',d,p)[0], 'Verschiebungen')" 2>/dev/null)
echo "       DOS-Bau: $L"
[ -f $W/TESTM32.M32 ] && cmp -s $W/TESTM32.M32 $O/TESTM32.M32 && echo "ok     TESTM32.M32 unter DOS byte-gleich mit rd32mod.py ($(stat -c %s $O/TESTM32.M32) Bytes)" || { echo "FEHLER TESTM32.M32 ungleich oder fehlt"; F=1; }
echo "$L" | grep -q "6 Exporte" && echo "ok     6 Exporte aus TESTM32.EXP, Verschiebungen gefunden" && ! echo "$L" | grep -q " 0 Verschiebungen" || { echo "FEHLER Exporte"; F=1; }
[ ! -f $W/TESTM32.0 ] && [ ! -f $W/TESTM32.1 ] && echo "ok     Zwischenbilder geloescht" || { echo "FEHLER Zwischenbilder liegen noch da"; F=1; }
tr -d '\r' < $W/F.TXT 2>/dev/null | grep -q "Kopf R32M fehlt" && echo "ok     falsche Eingabe wird gemeldet" || { echo "FEHLER keine Meldung bei falscher Eingabe"; F=1; }
[ $F = 0 ] && echo "RD32MOD: alles ok"; exit $F
