#!/bin/bash
# Audit M4: RDFORMAT zweimal gestartet. Das zweite meldet Text 34 und endet, das erste laeuft weiter.
cd "$(dirname "$0")/.."
IMG=obj/dfmt.img; SER=obj/dfmt_com1.txt; SOCK=/tmp/qmon_dfmt.sock
printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\DOPPFMT.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_dfmt.txt
NUR="$(awk '$4=="S" && $1!="RDSYS.EXE"{print $1}' tools/budget.txt | tr '\n' ' ') RDFORMAT.EXE RDDISK.MOD DOPPFMT.EXE" bash tools/mkdisk.sh $IMG obj/ae_dfmt.txt >/dev/null 2>&1
rm -f $SER $SOCK obj/dfmt_*.ppm obj/dfmt_b.img
mformat -C -f 360 -v ALTDISK -i obj/dfmt_b.img :: && echo "alte Daten" > obj/alt.txt && mcopy -i obj/dfmt_b.img obj/alt.txt ::/ALT.TXT
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 \
  -drive file=obj/dfmt_b.img,format=raw,if=floppy,index=1 -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_dfmt.py
Q() { python3 obj/qmon_dfmt.py "$@"; }
waitn() { for i in $(seq 600); do grep -q "$1" $SER 2>/dev/null && return 0; sleep 0.2; done; return 1; }
F=0
if waitn "RDFORMAT: Meldung 34"; then sleep 2; Q "screendump obj/dfmt_meldung.ppm" sleep0.3 "sendkey ret"; else echo "FEHLER: keine Meldung 34"; F=1; fi
sleep 2; Q "sendkey esc"; sleep 1; Q "sendkey esc"
wait $QP
S=$(tr -d '\r' < $SER)
echo "$S" | grep -E "DOPPFMT|RDFORMAT: (bereit|Meldung)|Task beendet|vollstaendig" | head -12
[ "$(echo "$S" | grep -c "RDFORMAT: Meldung 34")" = 1 ] && echo "ok     genau eine Meldung 34" || { echo "FEHLER Anzahl Meldung 34"; F=1; }
[ "$(echo "$S" | grep -c "RDFORMAT: bereit")" = 1 ] && echo "ok     nur das erste RDFORMAT zeigt den Dialog" || { echo "FEHLER Anzahl bereit"; F=1; }
mdir -i obj/dfmt_b.img ::/ALT.TXT >/dev/null 2>&1 && echo "ok     Diskette B: unveraendert" || { echo "FEHLER Diskette veraendert"; F=1; }
[[ "$S" == *"Speicher vollstaendig zurueck"* ]] && echo "ok     Speicher vollstaendig zurueck" || { echo "FEHLER Speicher"; F=1; }
[ $F = 0 ] && echo "Doppelstart: alles ok" || echo "Doppelstart: Fehler"
exit $F
