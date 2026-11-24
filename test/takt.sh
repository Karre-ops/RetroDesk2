#!/bin/bash
# Systemtakt 145,6 Hz (PLAN_API Schritt 2): TAKTTEST.EXE ueber 2 s auf einem 8088 (QEMU ohne -cpu: 386-Befehle
# fehlen nicht, Takt ist derselbe) und auf einem Pentium: Teiler 1 ~291, Teiler 8 ~36, BIOS-Uhr 18,2 Hz (~36 in 2 s),
# TaktKill haelt an, 7 freie Plaetze neben einem belegten, Ende ohne Fehler, Speicher zurueck.
cd "$(dirname "$0")/.."
IMG=obj/takt.img; SER=obj/takt_com1.txt; F=0
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
printf '@echo off\r\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\TAKTTEST.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' | tr -d '\r' > obj/ae_takt.txt
FMT=1440 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') TAKTTEST.EXE" bash tools/mkdisk.sh $IMG obj/ae_takt.txt >/dev/null 2>&1
w() { tr -d '\r' < $SER | grep -a "TAKTTEST: $1" | head -1 | sed 's/.*0x//' | python3 -c "import sys; print(int(sys.stdin.read().strip() or '0', 16))"; }
for cpu in "" "-cpu pentium"; do
  n=${cpu:+Pentium}; n=${n:-Standard}
  rm -f $SER; timeout 120 qemu-system-i386 $cpu -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a -serial file:$SER >/dev/null 2>&1
  T1=$(w Teiler1); T8=$(w Teiler8); TK=$(w Takte); BI=$(w BIOS); MS=$(w ms); KI=$(w "nach Kill"); PL=$(w Plaetze)
  echo "       $n: $MS ms, Takte $TK, Teiler1 $T1, Teiler8 $T8, BIOS $BI, nach Kill $KI, Plaetze $PL"
  pruef "$n: Rueckruf Teiler 1 mit 145,6 Hz (2 s: 275..310)" "[ $T1 -ge 275 ] && [ $T1 -le 310 ]"
  pruef "$n: Teiler 8 = 18,2 Hz (T1/8 +-2)" "[ $T8 -ge $(( T1 / 8 - 2 )) ] && [ $T8 -le $(( T1 / 8 + 2 )) ]"
  pruef "$n: TaktCount passt zu den Rueckrufen (+-3)" "[ $TK -ge $(( T1 - 3 )) ] && [ $TK -le $(( T1 + 3 )) ]"
  pruef "$n: BIOS-Uhr bleibt 18,2 Hz (2 s: 34..39)" "[ $BI -ge 34 ] && [ $BI -le 39 ]"
  pruef "$n: TaktKill haelt den Rueckruf an, 7 Plaetze frei" "[ $KI -le 1 ] && [ $PL = 7 ]"
  pruef "$n: Ende, Speicher vollstaendig zurueck" "tr -d '\r' < $SER | grep -q 'TAKTTEST: Ende' && tr -d '\r' < $SER | grep -q 'RD2: Speicher vollstaendig zurueck'"
done
[ $F = 0 ] && echo "TAKT: alles ok"; exit $F
