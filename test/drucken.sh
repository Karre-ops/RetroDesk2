#!/bin/bash
# Punkt 7: Drucken (RDPRN.MOD, EPSON9.DRV, PRNTEST.EXE). QEMU, Ausgabe ueber test/escp.py als Bild gegen test/ref.
# 1 Datei (Port=0): Testseite 120 x 72 dpi.  2 LPT1 (QEMU-Druckerport in eine Datei): gleiche Bytes wie 1.
# 3 Entwurf, 2 Kopien, 2 Seiten in die Datei: 60 dpi, 4 Seiten.  4 Abbruch nach dem ersten Band.
cd "$(dirname "$0")/.."
SER=obj/prn_com1.txt; F=0; : > obj/prn_alle.txt; SOCK=/tmp/qmon_prn.sock
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_prn.py
lauf() {  # $1 Name  $2 PRNTEST-Befehl  $3 RD2.INI  $4 QEMU-Zusatz
  local IMG=obj/prn_$1.img
  printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS /CGA A:\\RD2\\PRNTEST.EXE %s\r\nA:\\FDAPM.COM POWEROFF\r\n' "$2" > obj/ae_prn.txt
  FMT=720 bash tools/mkdisk.sh $IMG obj/ae_prn.txt
  printf "$3" > obj/prn_ini.txt; mcopy -o -i $IMG obj/prn_ini.txt ::/RD2/RD2.INI
  rm -f $SER obj/prn_$1.out
  if [ -z "$5" ]; then
    timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
      ${4:--parallel none} -serial file:$SER >/dev/null 2>&1
  else  # Dialog: Bild nehmen, dann Tasten ($5), auf das Ende warten
    rm -f $SOCK; timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
      ${4:--parallel none} -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 & local P=$!
    for t in $(seq 60); do sleep 1; grep -q "RDCTL.MOD" $SER 2>/dev/null && break; done; sleep 4
    timeout 10 python3 obj/qmon_prn.py "screendump obj/prn_$1.ppm"; local K; IFS="|" read -ra K <<< "$5"; timeout 20 python3 obj/qmon_prn.py "${K[@]}"
    wait $P
  fi
  echo "== $1" >> obj/prn_alle.txt; tr -d '\r' < $SER >> obj/prn_alle.txt
  L=$(tr -d '\r' < $SER)
  [ -f obj/prn_$1.out ] || mcopy -i $IMG ::/RD2/DRUCK.PRN obj/prn_$1.out 2>/dev/null
}
chk() { if echo "$L" | grep -q "$1"; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
bild() {  # $1 Name: Bild gegen test/ref/prn_$1.png
  if [ ! -s obj/prn_$1.out ]; then echo "FEHLER $1: keine Druckdaten"; F=1; return; fi
  echo "       $1: $(python3 test/escp.py obj/prn_$1.out obj/prn_$1.png)"
  if [ -f test/ref/prn_$1.png ]; then python3 test/bildvergleich.py obj/prn_$1.png test/ref/prn_$1.png >/dev/null && echo "ok     $1 Bild wie Referenz" || { echo "FEHLER $1 Bild weicht ab"; F=1; }
  else cp obj/prn_$1.png test/ref/prn_$1.png; echo "NEU    test/ref/prn_$1.png"; fi
}
lauf datei "" '[Drucker]\r\nPort=0\r\n'
chk "PRNTEST: dpi x 0x00000078" "datei: 120 dpi"; chk "PRNTEST: Breite 0x000003C0" "datei: 960 Punkte breit"
chk "PRNTEST: Hoehe 0x00000318" "datei: 792 Zeilen hoch"; chk "PRNTEST: Durchgaenge 0x00000011" "datei: 17 Baender (792 / 48)"
chk "PRNTEST: PrnClose 0x00000000" "datei: PrnClose ok"; chk "Speicher vollstaendig zurueck" "datei: Speicher zurueck"
bild datei
lauf lpt "" '[Drucker]\r\nPort=1\r\n' "-parallel file:obj/prn_lpt.out"
chk "PRNTEST: PrnClose 0x00000000" "lpt: PrnClose ok"; chk "Speicher vollstaendig zurueck" "lpt: Speicher zurueck"
cmp -s obj/prn_datei.out obj/prn_lpt.out && echo "ok     lpt: gleiche Bytes wie Datei" || { echo "FEHLER lpt: Bytes anders ($(stat -c %s obj/prn_lpt.out 2>/dev/null) / $(stat -c %s obj/prn_datei.out))"; F=1; }
lauf entwurf "2S" '[Drucker]\r\nPort=0\r\nEntwurf=1\r\n'
chk "PRNTEST: dpi x 0x0000003C" "entwurf: 60 dpi"; chk "PRNTEST: Kopien 0x00000002" "entwurf: 2 Kopien"
chk "PRNTEST: Durchgaenge 0x00000022" "entwurf: 34 Durchgaenge je Seite (2 Kopien)"; chk "Speicher vollstaendig zurueck" "entwurf: Speicher zurueck"
bild entwurf
lauf abbruch "A" '[Drucker]\r\nPort=0\r\n'
chk "PRNTEST: abgebrochen nach Durchgang 0x00000001" "abbruch: nach dem ersten Band"; chk "Speicher vollstaendig zurueck" "abbruch: Speicher zurueck"
# 5 Druckdialog: Datei vorgewaehlt (Port=0), Alt+K: Kopien 3, Eingabe druckt; Port und Kopien landen in RD2.INI
lauf dialog "D" '[Drucker]\r\nPort=0\r\n' "" "sendkey alt-k|sleep0.5|sendkey backspace|sleep0.3|sendkey 3|sleep0.3|sendkey ret"
chk "PRNTEST: Kopien 0x00000003" "dialog: 3 Kopien aus dem Dialog"; chk "PRNTEST: PrnClose 0x00000000" "dialog: PrnClose ok"
chk "Speicher vollstaendig zurueck" "dialog: Speicher zurueck"
python3 -c "from PIL import Image; Image.open('obj/prn_dialog.ppm').save('obj/prn_dialog.png')" 2>/dev/null
if [ -f test/ref/prn_dialog.png ]; then python3 test/bildvergleich.py obj/prn_dialog.png test/ref/prn_dialog.png >/dev/null && echo "ok     dialog: Bild wie Referenz" || { echo "FEHLER dialog: Bild weicht ab"; F=1; }
else cp obj/prn_dialog.png test/ref/prn_dialog.png 2>/dev/null && echo "NEU    test/ref/prn_dialog.png"; fi
exit $F
