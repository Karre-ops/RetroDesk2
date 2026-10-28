#!/bin/bash
# Diskettensatz (SPEC 12): von RD2_1.IMG booten, auf leere 10-MB-Platte installieren, Diskette 2 und 3 wechseln,
# danach ohne Diskette von C: starten. Prueft Dateien aller drei Disketten, KERNEL.SYS, CONFIG.SYS und den Start.
cd "$(dirname "$0")/.."
F=0; S=/tmp/qmon_satz.sock; export QMON=$S
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
T() { timeout 10 python3 test/qemu/qmon.py text 2>/dev/null; }
waitscr() { for i in $(seq ${2:-30}); do T | grep -q "$1" && return 0; sleep 1; done; return 1; }
bash tools/mkset.sh >/dev/null 2>&1 || { echo "FEHLER mkset"; exit 1; }
for i in 1 2 3; do cp out/set/RD2_$i.IMG obj/satz$i.img; done
HD=obj/satz_hd.img; bash tools/mkhd.sh $HD >/dev/null
dd if=test/daten/mbr_rdfdisk.bin of=$HD conv=notrunc 2>/dev/null   # Startcode des MBR wie von RDFDISK geschrieben (mkhd legt nur die Tabelle an)
rm -f $S
timeout 400 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/satz1.img,format=raw,if=floppy,index=0 \
  -drive file=$HD,format=raw,if=ide,index=0 -boot a -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
waitscr "Erster Start" 90 && echo "ok     Diskette 1 startet, Einrichtung erscheint" || { echo "FEHLER keine Einrichtung"; F=1; }
sleep 1; Q "sendkey i"; sleep 2; Q "sendkey ret"
waitscr "startfaehig machen" 120 && Q "sendkey j"
waitscr "Diskette 2 (RD2 PROG) einlegen" 120 && { echo "ok     fragt nach Diskette 2"; Q "change floppy0 obj/satz2.img"; sleep 2; Q "sendkey ret"; } || { echo "FEHLER keine Frage nach Diskette 2"; F=1; }
waitscr "Diskette 3 (RD2 NETZ) einlegen" 180 && { echo "ok     fragt nach Diskette 3"; Q "change floppy0 obj/satz3.img"; sleep 2; Q "sendkey ret"; } || { echo "FEHLER keine Frage nach Diskette 3"; F=1; }
waitscr "Diskette 3 (RD2 NETZ): [0-9]* Dateien" 120 || { echo "FEHLER Diskette 3 nicht kopiert"; F=1; }
T > obj/schirm_satz.txt; grep "Dateien kopiert\|startfaehig" obj/schirm_satz.txt | sed 's/^ */       /'
sleep 2; Q "sendkey esc"; sleep 2; T | grep -q "(J/N)" && Q "sendkey j"; sleep 3; Q quit; wait $QP 2>/dev/null
P=$HD@@32256
for f in KERNEL.SYS CONFIG.SYS RD2/RD.EXE RD2/SHELL.EXE RD2/RDMAUS.COM RD2/RD2.INI RD2/FARBSYM.DAT RD2/VGA16.DRV RD2/RDFDISK.EXE RD2/RDNET.EXE RD2/RDFTP.EXE; do
  mdir -i $P ::/$f >/dev/null 2>&1 || { echo "FEHLER C:\\$f fehlt"; F=1; }
done
[ $F = 0 ] && echo "ok     Dateien aller drei Disketten auf C:"
mtype -i $P ::/CONFIG.SYS | grep -q "SHELL=C:\\\\RD2\\\\RD.EXE" && echo "ok     CONFIG.SYS: RD2 ist die Shell" || { echo "FEHLER CONFIG.SYS"; F=1; }
# Start von C: ohne Diskette
rm -f $S
timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$HD,format=raw,if=ide,index=0 -boot c \
  -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!; ok=0
for i in $(seq 60); do sleep 1; Q "screendump obj/satz_c.ppm"; python3 -c "from PIL import Image; import sys; sys.exit(0 if Image.open('obj/satz_c.ppm').size==(640,480) else 1)" 2>/dev/null && { ok=1; break; }; done
T | grep -q "Bad or missing" && ok=0
[ $ok = 1 ] && { echo "ok     startet ohne Diskette von C: bis zur Oberflaeche"; python3 -c "from PIL import Image; Image.open('obj/satz_c.ppm').save('obj/satz_c.png')"; } || { echo "FEHLER Start von C:"; T | grep -v "^ *$" | tail -4; F=1; }
Q quit; wait $QP 2>/dev/null
[ $F = 0 ] && echo "Diskettensatz: alles ok"
exit $F
