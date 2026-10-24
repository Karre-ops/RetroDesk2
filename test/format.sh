#!/bin/bash
# RDFORMAT (FreeDOS FORMAT 0.92a mit RD2-Oberflaeche) in QEMU: Diskette in B: formatieren, nur Tastatur.
# Faelle: 360 (1,2-MB-Laufwerk), 720 (1,44-MB-Laufwerk), abbruch (Esc bei 25 %), wp (schreibgeschuetzt,
# Diskette bleibt unveraendert, Fehlermeldung). FAELLE="360 wp" waehlt aus.
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
for FALL in ${FAELLE:-360 720 abbruch wp}; do
  KB=$FALL; RO=; case $FALL in abbruch|wp) KB=360;; esac; [ $FALL = wp ] && RO=,readonly=on
  SER=obj/fmt_${FALL}_com1.txt; B=obj/fmt_b$FALL.img
  rm -f $SER $SOCK obj/fmt?_$FALL.ppm $B
  mformat -C -f $KB -v ALTDISK -i $B :: && echo "alte Daten" > obj/alt.txt && mcopy -i $B obj/alt.txt ::/ALT.TXT
  timeout 400 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/fmt.img,format=raw,if=floppy,index=0 \
    -drive file=$B,format=raw,if=floppy,index=1$RO -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  waitn() { for i in $(seq ${3:-600}); do grep -q "$1" $SER 2>/dev/null && return 0; sleep 0.2; done; return 1; }
  if waitn "RDFORMAT: bereit"; then
    sleep 3; Q "screendump obj/fmt1_$FALL.ppm" sleep0.3 "sendkey ret"; sleep 2
    if [ $FALL = abbruch ]; then Q "screendump obj/fmt2_$FALL.ppm" sleep0.3 "sendkey ret" "sendkey esc"   # QEMU formatiert in unter 0,5 s: Esc gleich hinter Enter in die Warteschlange
    else Q "screendump obj/fmt2_$FALL.ppm" sleep0.3 "sendkey ret"; fi
    if [ $FALL = abbruch ]; then waitn "Abbruch gewaehlt" 300 && Q "screendump obj/fmt3_$FALL.ppm"
    else waitn "Prozent formatiert" 300 && Q "screendump obj/fmt3_$FALL.ppm"; fi
    if waitn "RDFORMAT: \(fertig\|Fehler\|Abbruch\)" 1500; then sleep 3; Q "screendump obj/fmt4_$FALL.ppm" sleep0.3 "sendkey ret"; fi
  else echo "FEHLER: RDFORMAT kommt nicht ($KB)"; fi
  wait $QP
  echo "--- $FALL: Protokoll"
  tr -d '\r' < $SER | grep -vE "^RD: (Speicher|Modul)" | grep -vE "^I[0-9A-F]{2} |^A:.RD2.|^RD: " | sed -n '/^\/[UQ]/,$p' | head -40
  echo "--- $FALL: Ergebnis"
  mdir -i $B :: 2>&1 | head -8
  case $FALL in
    abbruch) grep -q "RDFORMAT: Abbruch gewaehlt" $SER && ! grep -q "RDFORMAT: fertig" $SER && ! grep -q "^100 Prozent" $SER && grep -q "Speicher vollstaendig zurueck" $SER;;
    wp) grep -q "RDFORMAT: Meldung 8" $SER && ! grep -q "RDFORMAT: fertig" $SER && mdir -i $B ::/ALT.TXT >/dev/null 2>&1 && grep -q "Speicher vollstaendig zurueck" $SER;;
    *) grep -q "RDFORMAT: fertig" $SER && ! mdir -i $B ::/ALT.TXT >/dev/null 2>&1 && fsck.fat -n $B >/dev/null 2>&1 \
       && [ "$(minfo -i $B :: 2>/dev/null | grep -m1 'sectors per track' | grep -o '[0-9]*$')" = "9" ];;
  esac && { echo "OK $FALL"; ok=$((ok+1)); } || { echo "FEHLER $FALL"; fehler=$((fehler+1)); }
done
echo "format.sh: $ok ok, $fehler Fehler"
