#!/bin/bash
# RDFORMAT (FreeDOS FORMAT 0.92a mit RD2-Oberflaeche) in QEMU: Diskette in B: formatieren, nur Tastatur.
# Lauf 1: 360-KB-Diskette (1,2-MB-Laufwerk), Lauf 2: 720-KB-Diskette (1,44-MB-Laufwerk).
# Die Ziel-Diskette ist vorher formatiert und enthaelt ALT.TXT; danach muss sie leer, fehlerfrei und
# im richtigen Format sein. Bildschirmfotos obj/fmt{1..4}_{360,720}.ppm:
#   1 Einstellungen   2 Rueckfrage "Diskette einlegen"   3 Fortschritt   4 Ergebnis
cd "$(dirname "$0")/.."
SOCK=/tmp/qmon_fmt.sock
cat > obj/ae_fmt.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDFORMAT.EXE B:
A:\FDAPM.COM POWEROFF
AE
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') RDFORMAT.EXE RDDISK.MOD" bash tools/mkdisk.sh obj/fmt.img obj/ae_fmt.txt >/dev/null
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_fmt.py
Q() { python3 obj/qmon_fmt.py "$@"; }
ok=0; fehler=0
for KB in ${KBS:-360 720}; do
  SER=obj/fmt_${KB}_com1.txt; B=obj/fmt_b$KB.img
  rm -f $SER $SOCK obj/fmt?_$KB.ppm $B
  mformat -C -f $KB -v ALTDISK -i $B :: && echo "alte Daten" > obj/alt.txt && mcopy -i $B obj/alt.txt ::/ALT.TXT
  timeout 400 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/fmt.img,format=raw,if=floppy,index=0 \
    -drive file=$B,format=raw,if=floppy,index=1 -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  waitn() { for i in $(seq ${3:-600}); do grep -q "$1" $SER 2>/dev/null && return 0; sleep 0.2; done; return 1; }
  if waitn "RDFORMAT: bereit"; then
    sleep 3; Q "screendump obj/fmt1_$KB.ppm" sleep0.3 "sendkey ret"; sleep 2
    Q "screendump obj/fmt2_$KB.ppm" sleep0.3 "sendkey ret"
    waitn "Prozent formatiert" 300 && Q "screendump obj/fmt3_$KB.ppm"
    if waitn "RDFORMAT: \(fertig\|Fehler\)" 1500; then sleep 2; Q "screendump obj/fmt4_$KB.ppm" sleep0.3 "sendkey ret"; fi
  else echo "FEHLER: RDFORMAT kommt nicht ($KB)"; fi
  wait $QP
  echo "--- $KB KB: Protokoll"
  tr -d '\r' < $SER | grep -vE "^RD: (Speicher|Modul)" | grep -vE "^I[0-9A-F]{2} |^A:.RD2.|^RD: " | sed -n '/^\/[UQ]/,$p' | head -40
  echo "--- $KB KB: Ergebnis"
  mdir -i $B :: 2>&1 | head -8
  if grep -q "RDFORMAT: fertig" $SER && ! mdir -i $B ::/ALT.TXT >/dev/null 2>&1 && fsck.fat -n $B >/dev/null 2>&1 \
     && [ "$(minfo -i $B :: 2>/dev/null | grep -m1 'sectors per track' | grep -o '[0-9]*$')" = "9" ]; then
    echo "OK $KB KB"; ok=$((ok+1)); else echo "FEHLER $KB KB"; fehler=$((fehler+1)); fsck.fat -n $B | tail -3; fi
done
echo "format.sh: $ok ok, $fehler Fehler"
