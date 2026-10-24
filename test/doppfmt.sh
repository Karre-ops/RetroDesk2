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

# ---- G23: RDFDISK und RDFORMAT einer Festplatte nicht zugleich ----
python3 -c "open('obj/dfmt_hd.img','wb').write(bytes(40*16*63*512))"
mformat -i obj/dfmt_hd.img@@32256 -T 40257 -h 16 -s 63 -H 63 :: 2>/dev/null
python3 - <<'PY'
import struct
f=open('obj/dfmt_hd.img','r+b'); mbr=bytearray(512)
mbr[446:462]=bytes([0x80,1,1,0,0x06,15,0xFF,39])+struct.pack('<II',63,40257); mbr[510:512]=b'\x55\xaa'
f.write(mbr); f.close()
PY
for FALL in F G; do
  [ $FALL = F ] && M="RDFORMAT: Meldung 35" || M="RDFDISK: Meldung 24"
  printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\DOPPFMT.EXE %s\nA:\\FDAPM.COM POWEROFF\n' $FALL > obj/ae_dfmt.txt
  NUR="$(awk '$4=="S" && $1!="RDSYS.EXE"{print $1}' tools/budget.txt | tr '\n' ' ') RDFORMAT.EXE RDFDISK.EXE RDDISK.MOD DOPPFMT.EXE" bash tools/mkdisk.sh $IMG obj/ae_dfmt.txt >/dev/null 2>&1
  rm -f $SER $SOCK
  timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 \
    -drive file=obj/dfmt_b.img,format=raw,if=floppy,index=1 -drive file=obj/dfmt_hd.img,format=raw,if=ide,index=0 -boot a \
    -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  if waitn "$M"; then sleep 2; Q "screendump obj/dfmt_$FALL.ppm" sleep0.3 "sendkey ret"; else echo "FEHLER: keine $M"; F=1; fi
  sleep 3; kill $QP 2>/dev/null; wait $QP 2>/dev/null       # erstes Programm laeuft weiter, Ende pruefen M4/rundos
  S=$(tr -d '\r' < $SER)
  [ "$(echo "$S" | grep -c "$M")" = 1 ] && echo "ok     $FALL: $M" || { echo "FEHLER $FALL: $M"; F=1; }
  [[ "$S" == *"RD: Task beendet"* ]] && echo "ok     $FALL: zweites Programm beendet" || { echo "FEHLER $FALL: zweites laeuft"; F=1; }
done
mdir -i obj/dfmt_hd.img@@32256 :: >/dev/null 2>&1 && echo "ok     Festplatte unveraendert lesbar" || { echo "FEHLER Festplatte"; F=1; }
[ $F = 0 ] && echo "G23: alles ok" || echo "G23: Fehler"
exit $F
