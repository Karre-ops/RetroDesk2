#!/bin/bash
# Punkt 5a/5b: RDSND mit SPEAKER.DRV, ADLIB.DRV und RDMUS.MOD (SNDTEST.EXE, Musik aus test/klang). Vier Starts:
# 1 ohne RD2.INI: Lautsprecher, Piep und START enden ueber den Kern-Takt.  2 Sound=ADLIB mit QEMU-AdLib: OPL2, IMF/DRO mit Endmeldung.
# 3 Sound=SB mit QEMU-SB16 und AdLib: PCM per DMA mit Endmeldung, Schleife, IMF.
# 4 Sound=ADLIB ohne AdLib, Systemklang=0: Rueckfall auf den Lautsprecher, Piep still, Noten gehen.
cd "$(dirname "$0")/.."
SER=obj/klang_com1.txt; SOCK=/tmp/qmon_klang.sock; : > obj/klang_alle.txt
lauf() {  # $1 Name  $2 Befehl  $3 RD2.INI-Inhalt (leer = keine)  $4 QEMU-Zusatz
  local IMG=obj/klang_$1.img
  printf '@echo off\r\nA:\\RD2\\RD.EXE /NOMAUS /CGA A:\\RD2\\SNDTEST.EXE %s\r\nA:\\RD2\\RD.EXE /NOMAUS /CGA A:\\RD2\\SNDTEST.EXE %s\r\nA:\\FDAPM.COM POWEROFF\r\n' $2 $2 > obj/ae_klang.txt
  bash tools/mkdisk.sh $IMG obj/ae_klang.txt
  mcopy -i $IMG test/klang/* ::/RD2/
  if [ -n "$3" ]; then printf "$3" > obj/klang_ini.txt; mcopy -o -i $IMG obj/klang_ini.txt ::/RD2/RD2.INI; fi
  rm -f $SER
  timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
    -audiodev none,id=a0 -machine pcspk-audiodev=a0 $4 -serial file:$SER >/dev/null 2>&1
  echo "== $1" >> obj/klang_alle.txt; tr -d '\r' < $SER >> obj/klang_alle.txt
  local f=$(tr -d '\r' < $SER | grep -c "^SNDTEST: ^^^ FALSCH"); local n=$(tr -d '\r' < $SER | grep -c "^SNDTEST: fertig")
  local sp=$(tr -d '\r' < $SER | grep -c "RD: Speicher vollstaendig zurueck")
  [ "$f" = 0 ] && [ "$n" = 2 ] && [ "$sp" = 2 ] && echo "ok     $1 (2 Laeufe, Speicher zurueck)" || { echo "FEHLER $1: $f falsch, $n fertig, $sp Speicher"; tr -d '\r' < $SER | grep -B1 "FALSCH" | head -6; }
}
lauf lautsprecher S ""
lauf adlib A '[System]\r\nSound=ADLIB\r\n' "-device adlib,audiodev=a0"
lauf sb B '[System]\r\nSound=SB\r\nSB=A220 I5 D1\r\n' "-device sb16,audiodev=a0 -device adlib,audiodev=a0"
lauf leise L '[System]\r\nSound=ADLIB\r\nSystemklang=0\r\n'
grep -E "Piep endet nach|START dauert|Musik endet|Sample endet|SndCaps" obj/klang_alle.txt | sort | uniq -c
exit 0
