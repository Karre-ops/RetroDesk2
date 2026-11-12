#!/bin/bash
# Datentraeger pruefen (DPRUEF.EXE T B:): 360-KB-Diskette mit Ordnern, dazu ein verlorener Cluster (300, in beiden
# FATs belegt) und eine abweichende FAT 2 (Cluster 301). Erwartet: beides gefunden, repariert, zweite Pruefung ohne
# Funde; danach FAT 1 = FAT 2, Cluster 300 frei, Dateien unversehrt.
cd "$(dirname "$0")/.."
IMG=obj/dp.img; DSK=obj/dp_b.img; SER=obj/dp_com1.txt; S=/tmp/qmon_dp.sock; export QMON=$S; F=0
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\DPRUEF.EXE T B:\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_dp.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') DPRUEF.EXE RDDISK.MOD" bash tools/mkdisk.sh $IMG obj/ae_dp.txt >/dev/null 2>&1
rm -f $DSK; mformat -C -f 360 -v PRUEF -i $DSK ::; mmd -i $DSK ::/SPIELE; mmd -i $DSK ::/SPIELE/ALT
mcopy -i $DSK out/RD2.EXE ::/SPIELE/; mcopy -i $DSK out/SHELL.EXE ::/SPIELE/ALT/; mcopy -i $DSK tools/budget.txt ::/
md5a=$(mtype -i $DSK ::/SPIELE/RD2.EXE | md5sum)
python3 - $DSK <<'PY'
import sys
f=open(sys.argv[1],'r+b'); d=bytearray(f.read())
def setf(fat,c,v):
    o=512+fat*2*512+c*3//2
    if c&1: d[o]=(d[o]&0x0F)|((v<<4)&0xF0); d[o+1]=(v>>4)&0xFF
    else: d[o]=v&0xFF; d[o+1]=(d[o+1]&0xF0)|((v>>8)&0x0F)
setf(0,300,0xFFF); setf(1,300,0xFFF); setf(1,301,0xFFF)
f.seek(0); f.write(d)
PY
rm -f $SER $S; touch $SER
timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -drive file=$DSK,format=raw,if=floppy,index=1 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
for i in $(seq 400); do grep -q "DPRUEF: Test fertig\|DPRUEF: Funde 0x0000000[0-9]" $SER && grep -q "Test fertig" $SER && break; sleep 0.25; done
wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER | grep -a -A1 "^DPRUEF" | grep -av "^--" | iconv -f CP437 -t UTF-8)
echo "$L" | grep -q "FAT-Kopien sind verschieden" && echo "ok     abweichende FAT-Kopie gefunden" || { echo "FEHLER FAT-Kopien"; F=1; }
echo "$L" | grep -q "Verlorene Cluster: 1" && echo "ok     1 verlorener Cluster gefunden" || { echo "FEHLER verlorene Cluster"; F=1; }
echo "$L" | grep -q "DPRUEF: Funde 0x00000002" && echo "ok     genau 2 Funde (keine Fehlalarme)" || { echo "FEHLER Zahl der Funde"; echo "$L" | head -12; F=1; }
echo "$L" | grep -q "DPRUEF: repariert" && echo "$L" | grep -q "DPRUEF: Funde 0x00000000" && echo "ok     repariert, zweite Pruefung ohne Funde" || { echo "FEHLER Reparatur"; F=1; }
python3 - $DSK <<'PY' && echo "ok     auf der Diskette: FAT 1 = FAT 2, Cluster 300 frei" || { echo "FEHLER Diskette nach Reparatur"; F=1; }
import sys
d=open(sys.argv[1],'rb').read()
f1=d[512:512+1024]; f2=d[512+1024:512+2048]
c=300; o=c*3//2; v=(f1[o]|(f1[o+1]<<8)); v = v>>4 if c&1 else v&0xFFF
sys.exit(0 if f1==f2 and v==0 else 1)
PY
[ "$(mtype -i $DSK ::/SPIELE/RD2.EXE | md5sum)" = "$md5a" ] && echo "ok     Dateien unversehrt" || { echo "FEHLER Datei veraendert"; F=1; }
[ $F = 0 ] && echo "Datentraeger pruefen: alles ok"; exit $F
