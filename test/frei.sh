#!/bin/bash
# PLAN_FS: freier Platz ueber 2 GB. FAT32-Kern, Platte 3 GB (eine FAT32-Partition 0Ch) und 6 GB.
# DriveInfo muss ueber 7303h richtig zaehlen (3 GB: frei und gesamt ueber 2 GB) bzw. bei ueber 4 GB den Hoechstwert liefern.
cd "$(dirname "$0")/.."
export WATCOM=/opt/ow PATH=/opt/ow/binl64:/opt/ow/binl:$PATH INCLUDE=/opt/ow/h
SER=obj/frei_com1.txt; IMG=obj/frei_a.img; H=obj/frei_hd.img; F=0
wcc -0 -mm -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/FREI.obj test/FREI.C || exit 1
wlink option quiet format dos name obj/FREI.EXE file obj/rd2start.obj,obj/FREI.obj library out/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs >/dev/null || exit 1
python3 tools/rd2link.py obj/FREI.EXE "Frei" >/dev/null
printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\FREI.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_frei.txt
FMT=720 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ')" bash tools/mkdisk.sh $IMG obj/ae_frei.txt >/dev/null 2>&1
mcopy -o -i $IMG src/disk/fdkern/F32/KERNEL.SYS ::/KERNEL.SYS; mcopy -o -i $IMG obj/FREI.EXE ::/RD2/FREI.EXE
for gb in 3 6; do
  n=$((gb*1024*2048 - 2048)); rm -f $H; truncate -s ${gb}G $H
  python3 -c "
import struct; m=bytearray(512); m[446:462]=bytes([0x80,0xFE,0xFF,0xFF,0x0C,0xFE,0xFF,0xFF])+struct.pack('<II',2048,$n); m[510:512]=b'\x55\xaa'
open('$H','r+b').write(m)"
  mformat -F -i $H@@1048576 -T $n -h 255 -s 63 -H 2048 -v FREI ::
  rm -f $SER; touch $SER
  timeout 200 qemu-system-i386 -cpu pentium -m 4 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 \
    -drive file=$H,format=raw,if=ide,index=0 -boot a -serial file:$SER >/dev/null 2>&1
  fr=$(tr -d '\r' < $SER | grep -a "FREI: frei" | awk '{print $3}'); ge=$(tr -d '\r' < $SER | grep -a "FREI: gesamt" | awk '{print $3}')
  echo "     $gb GB: frei $fr, gesamt $ge"
  [ -z "$fr" ] && { echo "FEHLER $gb GB: keine Ausgabe"; tr -d '\r' < $SER | tail -3; F=1; continue; }
  f=$((fr)); g=$((ge))
  if [ $gb = 3 ]; then
    [ $f -gt 3000000000 ] && [ $g -gt 3000000000 ] && [ $f -le $g ] && echo "ok     3 GB: frei und gesamt ueber 2 GB" || { echo "FEHLER 3 GB"; F=1; }
  else
    [ $f = 4294967295 ] && [ $g = 4294967295 ] && echo "ok     6 GB: Hoechstwert" || { echo "FEHLER 6 GB"; F=1; }
  fi
done
[ $F = 0 ] && echo "frei.sh: alles ok" || echo "frei.sh: FEHLER"; exit $F
