#!/bin/bash
# PLAN_FS 3/4: RDFORMAT wechselt FAT16 <-> FAT32 mit Neustart (QEMU, 386+, FreeDOS 8086/FAT32-Kern auf der Startdiskette).
# Platte 300 MB, eine FAT16-Partition (06h) mit ALT.TXT. Lauf 1: RDFORMAT C: als Programm, FAT32 waehlen (Alt+3), Start,
# Rueckfrage 36 mit Ja -> Typ 0Bh, Merker RD2FMT1 in Sektor 2, Sicherung PTAB1.BAK, Neustart (QEMU endet, -no-reboot).
# Lauf 2: RD2 mit SHELL: findet den Merker, RDFORMAT /W formatiert ohne Rueckfrage FAT32. Pruefung mit fsck.fat.
# Dann dasselbe zurueck auf FAT16 (Alt+1, Typ 06h).
cd "$(dirname "$0")/.."
SOCK=/tmp/qmon_f32.sock; export QMON=$SOCK
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { for i in $(seq ${3:-600}); do grep -aq "$1" $2 2>/dev/null && return 0; sleep 0.2; done; return 1; }
F=0; ok() { echo "ok     $1"; }; feh() { echo "FEHLER $1"; F=1; }
IMG=obj/f32_a.img; H=obj/f32_hd.img; SER=obj/f32_com1.txt; ZYL=609; NSEK=$((ZYL*16*63-63)); P=$H@@32256
python3 - $H $NSEK $ZYL <<'PY'
import sys, struct
f, n, z = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
with open(f, 'wb') as d: d.truncate(z*16*63*512)
m = bytearray(512); m[446:462] = bytes([0x80, 1, 1, 0, 6, 15, 63 | (((z-1) >> 8) << 6), (z-1) & 255]) + struct.pack('<II', 63, n); m[510:512] = b'\x55\xaa'
with open(f, 'r+b') as d: d.write(m)
PY
mformat -i $P -T $NSEK -h 16 -s 63 -H 63 -v ALTHDD :: && echo "alte Daten" > obj/alt.txt && mcopy -i $P obj/alt.txt ::/ALT.TXT
typ() { python3 -c "import sys; print('%02X' % open('$H','rb').read(512)[446+4])"; }
lauf() {   # $1 Programm (RDFORMAT C: oder SHELL)  $2 Taste fuer FAT-Wahl (leer: Lauf 2)
  printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\%s\r\nA:\\FDAPM.COM POWEROFF\r\n' "$1" > obj/ae_f32.txt
  mcopy -o -i $IMG obj/ae_f32.txt ::/AUTOEXEC.BAT
  rm -f $SER $SOCK; touch $SER
  timeout 600 qemu-system-i386 -m 4 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 \
    -drive file=$H,format=raw,if=ide,index=0 -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  if [ -n "$2" ]; then
    waitn "RDFORMAT: bereit" $SER 900 || feh "RDFORMAT kommt nicht"
    sleep 3; Q "screendump obj/f32_dlg_$2.ppm"; Q "sendkey alt-$2"; sleep 0.5; Q "sendkey ret"
    waitn "RDFORMAT: Meldung 36" $SER 300 && { sleep 2; Q "sendkey tab"; sleep 0.3; Q "sendkey ret"; } || feh "keine Rueckfrage 36"
  else
    waitn "RDFORMAT: fertig\|RDFORMAT: Fehler\|Meldung 37" $SER 3000; sleep 3; Q "sendkey ret"; sleep 2; Q quit
  fi
  wait $QP; cp $SER obj/f32_com1_${1:0:5}_$fs.txt
}
FDK=src/disk/fdkern/F32/KERNEL.SYS
printf '@echo off\r\n' > obj/ae_f32.txt
FMT=720 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') RDFORMAT.EXE RDDISK.MOD" bash tools/mkdisk.sh $IMG obj/ae_f32.txt >/dev/null 2>&1
mcopy -o -i $IMG $FDK ::/KERNEL.SYS
for z in 32:3:0B:FAT32 16:1:06:FAT16; do
  IFS=: read fs k soll name <<< "$z"
  lauf "RDFORMAT.EXE C:" $k
  grep -aq "RDFORMAT: Typ $name" $SER && ok "$name gewaehlt, Typ geschrieben, Neustart" || { feh "$name: kein Typwechsel"; tr -d '\r' < $SER | grep -a "RDFORMAT" | tail -5; }
  [ "$(typ)" = $soll ] && ok "Partitionstyp $soll" || feh "Partitionstyp $(typ) statt $soll"
  dd if=$H bs=512 skip=1 count=1 2>/dev/null | head -c 7 | grep -q RD2FMT1 && ok "Merker in Sektor 2" || feh "kein Merker"
  mdir -i $IMG ::/RD2/PTAB1.BAK >/dev/null 2>&1 && ok "PTAB1.BAK gesichert" || feh "keine Sicherung"
  lauf "SHELL.EXE"
  grep -aq "RDFORMAT: Merker gefunden" $SER && ok "SHELL startet RDFORMAT /W" || feh "Merker nicht gefunden"
  grep -aq "RDFORMAT: fertig" $SER && ok "$name formatiert ohne Rueckfrage" || { feh "$name nicht formatiert"; tr -d '\r' < $SER | grep -a "RDFORMAT" | tail -8; }
  [ "$(dd if=$H bs=512 skip=1 count=1 2>/dev/null | tr -d '\0' | wc -c)" = 0 ] && ok "Merker geloescht" || feh "Merker noch da"
  dd if=$H of=obj/f32_part.img bs=512 skip=63 count=$NSEK 2>/dev/null
  [ $fs = 32 ] && o=82 || o=54
  [ "$(dd if=obj/f32_part.img bs=1 skip=$o count=5 2>/dev/null)" = "FAT$fs" ] && ok "Startsektor $name" || feh "Startsektor nicht $name"
  fsck.fat -n obj/f32_part.img >/dev/null 2>&1 && ok "fsck.fat sauber" || feh "fsck.fat meldet Fehler"
  ! mdir -i $P ::/ALT.TXT >/dev/null 2>&1 && ok "alte Daten weg" || feh "ALT.TXT noch da"
done
[ $F = 0 ] && echo "fat32.sh: alles ok" || echo "fat32.sh: FEHLER"
exit $F
