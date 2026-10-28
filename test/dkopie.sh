#!/bin/bash
# Diskette kopieren (DKOPIE.EXE T: B: nach B: mit Wechseln): Quelle 360 KB mit Dateien, Ziel leer formatiert;
# danach muss das Ziel-Abbild byte-gleich mit der Quelle sein.
cd "$(dirname "$0")/.."
IMG=obj/dk.img; SRC=obj/dk_src.img; DST=obj/dk_dst.img; SER=obj/dk_com1.txt; S=/tmp/qmon_dk.sock; export QMON=$S; F=0
printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\DKOPIE.EXE T\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_dk.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') DKOPIE.EXE RDDISK.MOD" bash tools/mkdisk.sh $IMG obj/ae_dk.txt >/dev/null 2>&1
rm -f $SRC $DST; mformat -C -f 360 -v QUELLE -i $SRC ::; mformat -C -f 360 -v ZIEL -i $DST ::
python3 -c "import random; random.seed(7); open('obj/dk_daten.bin','wb').write(bytes(random.randrange(256) for _ in range(200000)))"
mcopy -i $SRC obj/dk_daten.bin ::/DATEN.BIN; mcopy -i $SRC out/RD.EXE ::/
rm -f $SER $S; touch $SER
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -drive file=$SRC,format=raw,if=floppy,index=1 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
cnt() { tr -d '\r' < $SER | grep -c "$1"; }
q=0; z=0
for i in $(seq 500); do
  if [ "$(cnt 'DKOPIE: Quelle einlegen')" -gt $q ]; then q=$((q + 1)); sleep 1; Q "change floppy1 $SRC"; sleep 1; Q "sendkey ret"; fi
  if [ "$(cnt 'DKOPIE: Ziel einlegen')" -gt $z ]; then z=$((z + 1)); sleep 1; Q "change floppy1 $DST"; sleep 1; Q "sendkey ret"; fi
  grep -q "DKOPIE: fertig\|DKOPIE: Fehler\|DKOPIE: kein\|Bootsektor Fehler" $SER && break; sleep 0.25
done
wait $QP 2>/dev/null
tr -d '\r' < $SER | grep "DKOPIE:" | tr '\n' ' ' | cut -c1-200; echo
grep -q "DKOPIE: fertig" $SER && echo "ok     fertig nach $q x Quelle, $z x Ziel" || { echo "FEHLER nicht fertig"; F=1; }
cmp -s $SRC $DST && echo "ok     Ziel byte-gleich mit der Quelle" || { echo "FEHLER Ziel weicht ab"; cmp $SRC $DST | head -2; F=1; }
mdir -i $DST ::/DATEN.BIN >/dev/null 2>&1 && echo "ok     DATEN.BIN auf dem Ziel" || { echo "FEHLER Datei fehlt"; F=1; }
[ $F = 0 ] && echo "Diskette kopieren: alles ok"; exit $F
