#!/bin/bash
# Speicherschutz RD2/32 (PLAN_API 3b) in Bochs: QEMU prueft keine Segmentgrenzen, Bochs schon.
# SCHUTZ32 L schreibt unter sein Bild (DOS-Speicher 500h): erwartet Ausnahme 0Dh, nur das Programm endet, RD2 laeuft weiter.
# Die Meldung nach der Ausnahme bestaetigt test/qemu/rfbmaus.py (Eingabetaste ueber VNC).
# SCHUTZ32 B liest BIOS ueber BiosPtr und beschreibt 4 MB aus rd32_alloc (Grenze waechst mit). Braucht bochs, ca. 2 Minuten.
cd "$(dirname "$0")/.."
command -v bochs >/dev/null || { echo "schutz32: bochs fehlt (apt-get install bochs bochsbios vgabios)"; exit 1; }
F=0; SER=obj/bs_com1.txt
pruef() { local t="$1"; if eval "$2"; then echo "ok     $t"; else echo "FEHLER $t"; F=1; fi; }
stopp() { for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in bochs*) kill ${p#/proc/} 2>/dev/null;; esac; done; sleep 0.5; }
printf '@echo off\r\nA:\\RD2\\CWSDPMI.EXE -p\r\nA:\\RD2\\RD2.EXE /VGA /D A:\\RD2\\SCHUTZ32.EXE B >A:\\S1.TXT\r\nA:\\RD2\\RD2.EXE /VGA /D A:\\RD2\\SCHUTZ32.EXE L >A:\\S2.TXT\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_bs.txt
FMT=1440 bash tools/mkdisk.sh obj/bs.img obj/ae_bs.txt >/dev/null 2>&1 || exit 1
mcopy -o -i obj/bs.img out/SCHUTZ32.EXE out/RD32.MOD out/CWSDPMI.EXE ::/RD2/
sed "s#DISK#obj/bs.img#; s#LOG#obj/bochs_s.log#" test/bochs/schutz.rc > obj/bochs_s.rc
stopp; rm -f $SER obj/bochs_s.log
(timeout 170 python3 test/qemu/comlausch.py 5911 $SER >/dev/null 2>&1 &); sleep 0.5
(timeout 165 python3 test/qemu/rfbmaus.py 5900 w20 $(for i in $(seq 32); do printf 'r w4 '; done) >/dev/null 2>&1 &)   # Eingabetaste fuer die Meldung nach der Ausnahme
(echo c; sleep 165) | timeout 165 bochs -q -f obj/bochs_s.rc >/dev/null 2>&1   # endet mit FDAPM POWEROFF
stopp
L=$( (mtype -i obj/bs.img ::/S1.TXT; mtype -i obj/bs.img ::/S2.TXT) 2>/dev/null | tr -d '\r')   # /D: Protokoll nach dem Ende auf stdout (Bochs: COM1 zu langsam)
for z in "SCHUTZ32: BIOS ueber BiosPtr lesbar" "SCHUTZ32: 4 MB aus rd32_alloc beschreibbar" "SCHUTZ32: L schreibt unter das Bild" "RD32: Ausnahme 0x0000000D" "RD32: Programm nach Ausnahme beendet"; do
  pruef "$z" "echo \"\$L\" | grep -q '$z'"; done
pruef "Schreibzugriff unter das Bild kam nicht durch" "! echo \"\$L\" | grep -q 'L FEHLER (kein Absturz)'"
pruef "Bochs meldet die Grenzverletzung" "grep -q 'beyond limit' obj/bochs_s.log"
pruef "RD2 zweimal sauber beendet, Speicher zurueck" "[ \$(echo \"\$L\" | grep -c 'RD2: Speicher vollstaendig zurueck') -ge 2 ]"
[ $F = 0 ] && echo "schutz32: alle ok" || { echo "$L" | grep -a "SCHUTZ32\|RD32\|RD2: Sp" | sed 's/^/       /'; echo "schutz32: FEHLER"; }
exit $F
