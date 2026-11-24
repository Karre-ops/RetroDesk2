#!/bin/bash
# Audit 24.11. (FPU-Erkennung): 386SX ohne 387 mit gesetztem CR0.EM. EMSET.COM setzt EM, dann startet RD2: es muss
# "8087 0" melden und normal laufen (ohne Pruefung haengt fninit in INT 7). Gegenprobe ohne EMSET: 8087 1.
cd "$(dirname "$0")/.."
F=0; SER=obj/fpuem_com1.txt; IMG=obj/fpuem.img
nasm -f bin -o obj/EMSET.COM test/m512/EMSET.ASM || exit 1
for em in 1 0; do
  [ $em = 1 ] && Z='A:\\EMSET.COM\r\n' || Z=''
  printf "@echo off\r\n${Z}A:\\\\RD2\\\\RD2.EXE /NOMAUS /CGA A:\\\\RD2\\\\LEER.EXE\r\nA:\\\\FDAPM.COM POWEROFF\r\n" > obj/ae_fpuem.txt
  FMT=720 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') LEER.EXE" bash tools/mkdisk.sh $IMG obj/ae_fpuem.txt >/dev/null 2>&1
  mcopy -o -i $IMG obj/EMSET.COM ::/
  rm -f $SER; timeout 60 qemu-system-i386 -cpu 486 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a -serial file:$SER >/dev/null 2>&1
  L=$(tr -d '\r' < $SER); W=$([ $em = 1 ] && echo 0 || echo 1)
  if echo "$L" | grep -q "RD2: 8087 0x0000000$W" && echo "$L" | grep -q "RD2: Speicher vollstaendig zurueck"; then echo "ok     EM=$em: 8087 $W, RD2 laeuft und endet"; else echo "FEHLER EM=$em"; echo "$L" | grep -a "8087\|CPU" | sed 's/^/       /'; F=1; fi
done
[ $F = 0 ] && echo "fpu_em: alle ok"; exit $F
