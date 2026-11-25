#!/bin/bash
# PLAN_FS 2: RDFDISK ab 386 mit FAT32 und LBA. Leere 10-GB-Platte (QEMU-BIOS kann INT-13h-Erweiterungen):
# Anlegen (Alt+A), "mit FAT32?" Ja, ganze Groesse, Schreiben. Erwartet: eine primaere Partition Typ 0Ch (LBA, ueber 8 GB),
# Groesse fast die ganze Platte. Danach dasselbe mit Nein: FAT16 hoechstens 2 GB (Typ 06h oder 0Eh).
cd "$(dirname "$0")/.."
S=/tmp/qmon_fd32.sock; export QMON=$S; SER=obj/com1_fd32.txt; IMG=obj/fd32_a.img; HD=obj/fd32_hd.img; F=0
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { for i in $(seq ${3:-600}); do [ "$(grep -ac "$1" $2 2>/dev/null)" -ge "${4:-1}" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\RDFDISK.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_fd32.txt
FMT=720 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') RDFDISK.EXE RDDISK.MOD" bash tools/mkdisk.sh $IMG obj/ae_fd32.txt >/dev/null 2>&1
for fall in ja nein; do
  rm -f $HD; truncate -s 10G $HD; rm -f $S $SER; touch $SER
  timeout 300 qemu-system-i386 -cpu pentium -m 4 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 \
    -drive file=$HD,format=raw,if=ide,index=0 -boot a -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 & QP=$!
  if waitn "RDFDISK: Liste bereit" $SER 600; then
    sleep 3; Q "sendkey alt-a"
    waitn "RDFDISK: Meldung 25" $SER 100 && { sleep 1.5; [ $fall = ja ] && Q "sendkey tab" sleep0.3 "sendkey ret" || Q "sendkey ret"; } || { echo "FEHLER keine Frage FAT32"; F=1; }
    sleep 2.5; Q "screendump obj/fd32_groesse_$fall.ppm"; Q "sendkey ret"; waitn "RDFDISK: Liste bereit" $SER 300 2
    sleep 2; Q "sendkey alt-s"; waitn "RDFDISK: Meldung 13" $SER 100 && sleep 2 && Q "sendkey ret"
    waitn "RDFDISK: Meldung 14" $SER 300 && sleep 2 && Q "sendkey tab" sleep0.4 "sendkey ret"
    sleep 3; Q quit
  else echo "FEHLER RDFDISK kommt nicht"; F=1; Q quit; fi
  wait $QP 2>/dev/null
  r=$(python3 -c "
import struct; d=open('$HD','rb').read(512); e=d[446:462]
print('%02X %d' % (e[4], struct.unpack('<I', e[12:16])[0] // 2048))")
  echo "     $fall: Typ/MB $r"
  t=${r% *}; mb=${r#* }
  if [ $fall = ja ]; then ok "FAT32 mit LBA: Typ 0Ch" "[ $t = 0C ]"; ok "fast die ganze Platte ($mb MB)" "[ $mb -gt 10000 ]"
  else ok "FAT16: Typ 06h oder 0Eh" "[ $t = 06 ] || [ $t = 0E ]"; ok "hoechstens 2 GB ($mb MB)" "[ $mb -le 2048 ] && [ $mb -gt 1000 ]"; fi
done
[ $F = 0 ] && echo "fdisk32.sh: alles ok" || echo "fdisk32.sh: FEHLER"
exit $F
