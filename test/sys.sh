#!/bin/bash
# Bootfaehig machen (FreeDOS SYS in RDFORMAT) in QEMU, nur Tastatur. Quelle ist die RD2-Diskette in A:
# (KERNEL.SYS, COMMAND.COM), Ziel die Diskette in B:.
# Faelle: sys  = RDFORMAT B: /SYS auf eine leere, formatierte 360-KB-Diskette (Rueckfrage Ja, Ergebnis OK)
#         fsys = RDFORMAT B: mit Haken "Systemdateien" (Alt+Y, in QEMU alt-z): formatieren und bootfaehig machen
# Danach startet QEMU von der Ziel-Diskette allein: FreeDOS-Kern, COMMAND.COM, AUTOEXEC schreibt BOOTOK
# auf COM1. Bilder obj/sys{1..3}_FALL.ppm (1 Rueckfrage/Einstellungen, 2 Uebertragen, 3 Ergebnis), obj/sys4_FALL.ppm = Start.
cd "$(dirname "$0")/.."
SOCK=/tmp/qmon_sys.sock
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_sys.py
Q() { python3 obj/qmon_sys.py "$@"; }
ok=0; fehler=0
for FALL in ${FAELLE:-sys fsys}; do
  ARG="B: /SYS"; [ $FALL = fsys ] && ARG="B:"
  printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\RDFORMAT.EXE %s\nA:\\FDAPM.COM POWEROFF\n' "$ARG" > obj/ae_sys.txt
  NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') RDFORMAT.EXE RDDISK.MOD" bash tools/mkdisk.sh obj/sys.img obj/ae_sys.txt >/dev/null
  SER=obj/sys_${FALL}_com1.txt; B=obj/sys_b$FALL.img
  rm -f $SER $SOCK obj/sys?_$FALL.ppm $B
  mformat -C -f 360 -v ZIEL -i $B ::
  waitn() { for i in $(seq ${3:-600}); do grep -q "$1" $2 2>/dev/null && return 0; sleep 0.2; done; return 1; }
  timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/sys.img,format=raw,if=floppy,index=0 \
    -drive file=$B,format=raw,if=floppy,index=1 -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  if waitn "RDFORMAT: SYS \(moeglich\|nicht\)" $SER; then
    sleep 3; Q "screendump obj/sys1_$FALL.ppm" sleep0.3
    # alt-z: QEMU-Taste z = Scancode 2Ch = y auf deutscher Tastatur
    if [ $FALL = fsys ]; then Q "sendkey alt-z" sleep0.3 "screendump obj/sys1b_$FALL.ppm" sleep0.3 "sendkey ret"; sleep 2; Q "sendkey ret"; else Q "sendkey ret"; fi
    waitn "RDFORMAT: SYS fertig\|RDFORMAT: SYS Fehler\|^SYS$" $SER 1500 && sleep 0.5 && Q "screendump obj/sys2_$FALL.ppm"
    if waitn "RDFORMAT: \(SYS fertig\|SYS Fehler\|Fehler\|Meldung 2[6-9]\)" $SER 1500; then sleep 3; Q "screendump obj/sys3_$FALL.ppm" sleep0.3 "sendkey ret"; fi
    [ $FALL = fsys ] && { waitn "RDFORMAT: fertig" $SER 300; sleep 3; Q "screendump obj/sys3_$FALL.ppm" sleep0.3 "sendkey ret"; }
  else echo "FEHLER: RDFORMAT kommt nicht ($FALL)"; fi
  wait $QP
  echo "--- $FALL: Protokoll"
  tr -d '\r' < $SER | grep -vE "^I[0-9A-F]{2} |^A:.RD2.|^RD: (Speicher|Modul)" | sed -n '/SYS moeglich\|SYS nicht/,$p' | head -40
  echo "--- $FALL: Ziel"; mdir -a -i $B :: 2>&1 | head -8
  # von der Ziel-Diskette starten
  printf '@echo off\r\nECHO BOOTOK > COM1\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_boot.txt
  mcopy -o -i $B obj/ae_boot.txt ::/AUTOEXEC.BAT; mcopy -o -i ../test/qemu/boot.img ::/FREEDOS/BIN/FDAPM.COM obj/fdapm.tmp 2>/dev/null; mcopy -o -i $B obj/fdapm.tmp ::/FDAPM.COM
  BSER=obj/sys_${FALL}_boot.txt; rm -f $BSER
  timeout 60 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$B,format=raw,if=floppy,index=0 -boot a \
    -serial file:$BSER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!; waitn "BOOTOK" $BSER 200 || Q "screendump obj/sys4_$FALL.ppm"; wait $QP
  echo "--- $FALL: Start von B: $(tr -d '\r' < $BSER | head -3)"
  grep -q "RDFORMAT: SYS fertig" $SER && mdir -a -i $B ::/KERNEL.SYS >/dev/null 2>&1 && fsck.fat -n $B >/dev/null 2>&1 \
    && grep -q "BOOTOK" $BSER && grep -q "Speicher vollstaendig zurueck" $SER \
    && { [ $FALL = sys ] || grep -q "RDFORMAT: fertig" $SER; } \
    && { echo "OK $FALL"; ok=$((ok+1)); } || { echo "FEHLER $FALL"; fehler=$((fehler+1)); }
done
echo "sys.sh: $ok ok, $fehler Fehler"
