#!/bin/bash
# RDFORMAT auf Festplatte (FreeDOS FORMAT, FAT16) in QEMU, nur Tastatur. Start von der RD2-Diskette A:,
# Festplatte 20 MB mit einer FAT16-Partition (Typ 04h), vorher formatiert mit ALT.TXT, Bezeichnung ALTHDD.
# Faelle: schnell (Haken Schnell, Vorgabe), voll (Bezeichnung NEU, Haken aus, /U), nein (Rueckfrage mit Nein: Platte unveraendert),
#         heim (RDFORMAT A: = RD2-Laufwerk: Meldung, nichts passiert),
#         gross (100 MB, Typ 06h ueber 32 MB: INT 25h/26h mit Paket, Audit M5). FAELLE="schnell nein" waehlt aus.
# Bilder obj/hdd{1..4}_FALL.ppm: 1 Einstellungen  2 Rueckfrage  3 Fortschritt  4 Ergebnis
cd "$(dirname "$0")/.."
SOCK=/tmp/qmon_hdd.sock
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_hdd.py
Q() { python3 obj/qmon_hdd.py "$@"; }
OFS=32256
neue_platte() {   # $1 = Bild: MBR (16 Koepfe, 63 Sektoren, ZYL Zylinder), Partition ab LBA 63, Typ TYP, aktiv
  python3 - $1 $NSEK $ZYL $TYP <<'PY'
import sys, struct
f, n, z, t = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4], 16)
d = bytearray(z*16*63*512)
e = bytes([0x80, 1, 1, 0, t, 15, 63 | (((z-1) >> 8) << 6), (z-1) & 255]) + struct.pack('<II', 63, n)
d[446:462] = e; d[510] = 0x55; d[511] = 0xAA
open(f, 'wb').write(d)
PY
  mformat -i $1@@$OFS -T $NSEK -h 16 -s 63 -H 63 -v ALTHDD :: && echo "alte Daten" > obj/alt.txt && mcopy -i $1@@$OFS obj/alt.txt ::/ALT.TXT
}
waitn() { for i in $(seq ${3:-600}); do grep -q "$1" $2 2>/dev/null && return 0; sleep 0.2; done; return 1; }
ok=0; fehler=0
for FALL in ${FAELLE:-schnell voll nein heim gross}; do
  LW=C:; [ $FALL = heim ] && LW=A:
  ZYL=40; TYP=04; [ $FALL = gross ] && { ZYL=203; TYP=06; }
  NSEK=$((ZYL*16*63-63))
  printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\RDFORMAT.EXE %s\nA:\\FDAPM.COM POWEROFF\n' $LW > obj/ae_hdd.txt
  NUR="$(awk '$4=="S" && $1!="RDSYS.EXE"{print $1}' tools/budget.txt | tr '\n' ' ') RDFORMAT.EXE RDDISK.MOD" bash tools/mkdisk.sh obj/hdd_a.img obj/ae_hdd.txt >/dev/null 2>&1
  SER=obj/hdd_${FALL}_com1.txt; H=obj/hdd_$FALL.img
  rm -f $SER $SOCK obj/hdd?_$FALL.ppm; neue_platte $H
  timeout 500 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/hdd_a.img,format=raw,if=floppy,index=0 \
    -drive file=$H,format=raw,if=ide,index=0 -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  if [ $FALL = heim ]; then
    waitn "RDFORMAT: Meldung 31" $SER && { sleep 3; Q "screendump obj/hdd4_$FALL.ppm" sleep0.3 "sendkey ret"; }
  elif waitn "RDFORMAT: bereit" $SER; then
    sleep 3; Q "screendump obj/hdd1_$FALL.ppm" sleep0.3
    [ $FALL = voll ] && Q "sendkey n" "sendkey e" "sendkey u" sleep0.2 "sendkey alt-s" sleep0.3   # Bezeichnung NEU, Schnell aus
    Q "sendkey ret"
    if waitn "RDFORMAT: Meldung 22" $SER 300; then
      sleep 2; Q "screendump obj/hdd2_$FALL.ppm" sleep0.3
      if [ $FALL = nein ]; then Q "sendkey tab" sleep0.2 "sendkey ret"; else Q "sendkey ret"; fi
    fi
    [ $FALL = voll ] && waitn "Prozent formatiert" $SER 300 && Q "screendump obj/hdd3_$FALL.ppm"
    if waitn "RDFORMAT: \(fertig\|Fehler\|Meldung 33\|Meldung 18\)" $SER 2000; then sleep 3; Q "screendump obj/hdd4_$FALL.ppm" sleep0.3 "sendkey ret"; fi
  else echo "FEHLER: RDFORMAT kommt nicht ($FALL)"; fi
  wait $QP
  echo "--- $FALL: Protokoll"
  tr -d '\r' < $SER | grep -vE "^I[0-9A-F]{2} |^A:.RD2.|^RD: (Speicher|Modul)" | sed -n '/RDFORMAT: \(Festplatte\|Meldung 31\)/,$p' | head -45
  echo "--- $FALL: Platte"; mdir -i $H@@$OFS :: 2>&1 | head -5
  case $FALL in
    heim)  grep -q "RDFORMAT: Meldung 31" $SER && mdir -i $H@@$OFS ::/ALT.TXT >/dev/null 2>&1;;
    nein)  ! grep -q "RDFORMAT: fertig" $SER && mdir -i $H@@$OFS ::/ALT.TXT >/dev/null 2>&1 && grep -q "Speicher vollstaendig zurueck" $SER;;
    *)     { [ $FALL != voll ] || mlabel -s -i $H@@$OFS :: 2>/dev/null | grep -q "NEU"; } && grep -q "RDFORMAT: fertig" $SER && ! mdir -i $H@@$OFS ::/ALT.TXT >/dev/null 2>&1 && grep -q "Speicher vollstaendig zurueck" $SER \
           && dd if=$H of=obj/hdd_part.img bs=512 skip=63 count=$NSEK 2>/dev/null && fsck.fat -n obj/hdd_part.img >/dev/null 2>&1;;
  esac && { echo "OK $FALL"; ok=$((ok+1)); } || { echo "FEHLER $FALL"; fehler=$((fehler+1)); }
done
echo "hdd.sh: $ok ok, $fehler Fehler"
