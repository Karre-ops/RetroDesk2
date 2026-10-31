#!/bin/bash
# Grafische Installation (Variante B): von RD2_1.IMG booten, in RD2SETUP "I" -> RD2 startet INSTALL.EXE.
# Enter = Installieren nach C:\RD2, Disketten 2 und 3 und wieder 1 per Meldung, Festplatte startfaehig
# (RDSYS per RunDos, ohne XMS mit Neustart von RD2), danach ohne Diskette von C: bis zur Oberflaeche.
# FALL=2: bestehendes DOS (CONFIG.SYS mit SHELL=COMMAND.COM, AUTOEXEC.BAT mit LH MOUSE.COM): Fragen als Dialog
#         (Shell eintragen: Ja, Maustreiber uebernehmen: Ja), Hinweis SET/PATH, kein RDMAUS, CONFIG.RDA, kein SYS.
# Bilder: obj/inst_dialog.png (Dialog), obj/inst_balken.png (waehrend Diskette 2), obj/inst_c.png (Start von C:).
cd "$(dirname "$0")/.."
F=0; S=/tmp/qmon_inst.sock; SER=obj/com1_inst.txt; export QMON=$S
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
T() { timeout 10 python3 test/qemu/qmon.py text 2>/dev/null; }
waitscr() { for i in $(seq ${2:-30}); do T | grep -q "$1" && return 0; sleep 1; done; return 1; }
waitser() { for i in $(seq ${3:-60}); do [ "$(tr -d '\r' < $SER | grep -c "$1")" -ge "${2:-1}" ] && return 0; sleep 1; done; return 1; }
png() { python3 -c "from PIL import Image; Image.open('$1.ppm').save('$1.png')" 2>/dev/null; }
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*) kill ${p#/proc/} 2>/dev/null;; esac; done
bash tools/mkset.sh >/dev/null 2>&1 || { echo "FEHLER mkset"; exit 1; }
for i in 1 2 3; do cp out/set/RD2_$i.IMG obj/inst$i.img; done
HD=obj/inst_hd.img; bash tools/mkhd.sh $HD >/dev/null
dd if=test/daten/mbr_rdfdisk.bin of=$HD conv=notrunc 2>/dev/null
FALL=${FALL:-1}; C="-i $HD@@32256"
if [ $FALL = 2 ]; then
  printf 'FILES=20\r\nSHELL=C:\\COMMAND.COM C:\\ /P\r\n' > obj/cs_alt.tmp; mcopy $C obj/cs_alt.tmp ::/CONFIG.SYS
  printf '@ECHO OFF\r\nPATH A:\\\r\nLH C:\\TREIBER\\MOUSE.COM /S50\r\nREM Ende\r\n' > obj/ae_alt.tmp; mcopy $C obj/ae_alt.tmp ::/AUTOEXEC.BAT
  mmd $C ::/TREIBER; mcopy $C test/qemu/CTMOUSE.EXE ::/TREIBER/MOUSE.COM
  printf 'x' > obj/io.tmp; mcopy $C obj/io.tmp ::/IO.SYS   # Platte hat schon ein DOS: kein SYS
fi
rm -f $S $SER; touch $SER
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/inst1.img,format=raw,if=floppy,index=0 \
  -drive file=$HD,format=raw,if=ide,index=0 -boot a -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
waitscr "Erster Start" 90 && echo "ok     Diskette 1 startet, Einrichtung erscheint" || { echo "FEHLER keine Einrichtung"; F=1; }
sleep 1; Q "sendkey i"
if waitser "INSTALL: Start" 1 90; then echo "ok     I: RD2 startet und oeffnet die Installation"
  sleep 3; Q "screendump obj/inst_dialog.ppm"; png obj/inst_dialog; Q "sendkey ret"
else echo "FEHLER INSTALL.EXE startet nicht"; F=1; fi
for d in 2 3; do
  if waitser "Diskette $d (RD2" 1 120; then
    echo "ok     Meldung: Diskette $d einlegen"; Q "change floppy0 obj/inst$d.img"; sleep 2; Q "sendkey ret"
    [ $d = 2 ] && { sleep 4; Q "screendump obj/inst_balken.ppm"; png obj/inst_balken; }
  else echo "FEHLER keine Meldung fuer Diskette $d"; F=1; fi
done
if waitser "Diskette 1 (RD2SYSTEM)" 1 120; then echo "ok     Meldung: Diskette 1 wieder einlegen"; Q "change floppy0 obj/inst1.img"; sleep 2; Q "sendkey ret"
else echo "FEHLER keine Meldung fuer Diskette 1"; F=1; fi
if [ $FALL = 2 ]; then
  for q in "als Shell in CONFIG.SYS" "MOUSE.COM /S50"; do
    waitser "$q" 1 60 && { echo "ok     Frage als Dialog: $q"; sleep 1; Q "sendkey ret"; } || { echo "FEHLER keine Frage: $q"; F=1; }
  done
  waitser "PATH-Zeilen" 1 30 && { echo "ok     Hinweis SET/PATH"; sleep 1; Q "sendkey ret"; } || { echo "FEHLER kein Hinweis SET/PATH"; F=1; }
fi
waitser "INSTALL: Dateien" 1 60 && echo "       $(tr -d '\r' < $SER | grep "INSTALL: Dateien\|INSTALL: Fehler\|INSTALL: CONFIG" | tr '\n' ' ')"
grep -q "INSTALL: Fehler 0x00000000" <(tr -d '\r' < $SER) && echo "ok     Kopieren ohne Fehler" || { echo "FEHLER beim Kopieren"; F=1; }
if [ $FALL = 1 ]; then
  waitser "startf" 1 30 && { sleep 1; Q "sendkey ret"; }
  waitser "Diskette 1 drin lassen" 1 60 && { sleep 1; Q "sendkey ret"; }
  waitser "SHELL: bereit" 2 90 && echo "ok     RDSYS gelaufen, RD2 wieder da" || { echo "FEHLER kein Neustart von RD2 nach RDSYS"; F=1; }
else
  waitser "ist installiert" 1 60 && { echo "ok     fertig ohne SYS (Platte hat schon ein System)"; sleep 1; Q "sendkey ret"; } || { echo "FEHLER keine Fertigmeldung"; F=1; }
  grep -q "INSTALL: RDSYS" <(tr -d '\r' < $SER) && { echo "FEHLER RDSYS trotz System"; F=1; }
fi
sleep 2; Q quit; wait $QP 2>/dev/null
P=$HD@@32256
K=KERNEL.SYS; [ $FALL = 2 ] && K=
for f in $K CONFIG.SYS RD2/RD.EXE RD2/SHELL.EXE RD2/RDMAUS.COM RD2/RD2.INI RD2/FARBSYM.DAT RD2/VGA16.DRV RD2/RDFDISK.EXE RD2/RDNET.EXE RD2/RDFTP.EXE RD2/INSTALL.EXE; do
  mdir -i $P ::/$f >/dev/null 2>&1 || { echo "FEHLER C:\\$f fehlt"; F=1; }
done
[ $F = 0 ] && echo "ok     Dateien aller drei Disketten${K:+ und KERNEL.SYS} auf C:"
mtype -i $P ::/CONFIG.SYS | tr -d '\r' > obj/inst_config.sys
if [ $FALL = 1 ]; then
  grep -q "SHELL=C:\\\\RD2\\\\RD.EXE" obj/inst_config.sys && grep -q "RDMAUS.COM" obj/inst_config.sys && echo "ok     CONFIG.SYS: RD2 ist die Shell, RDMAUS" || { echo "FEHLER CONFIG.SYS"; cat obj/inst_config.sys; F=1; }
else
  grep -q "^FILES=20" obj/inst_config.sys && grep -q "^REM SHELL=C:.COMMAND.COM" obj/inst_config.sys && grep -q "^SHELL=C:.RD2.RD.EXE /P C:.RD2.SHELL.EXE" obj/inst_config.sys \
    && grep -q "^INSTALL=C:.TREIBER.MOUSE.COM /S50" obj/inst_config.sys && ! grep -q "RDMAUS" obj/inst_config.sys \
    && echo "ok     CONFIG.SYS: alte Zeilen, alte Shell als REM, RD2 Shell, Maustreiber uebernommen, kein RDMAUS" || { echo "FEHLER CONFIG.SYS"; cat obj/inst_config.sys; F=1; }
  mtype $C ::/CONFIG.RDA | tr -d '\r' | grep -q "^SHELL=C:.COMMAND.COM" && echo "ok     Sicherung CONFIG.RDA" || { echo "FEHLER keine CONFIG.RDA"; F=1; }
  mtype $C ::/AUTOEXEC.BAT | cmp -s - obj/ae_alt.tmp && echo "ok     AUTOEXEC.BAT unveraendert" || { echo "FEHLER AUTOEXEC.BAT veraendert"; F=1; }
fi
mtype -i $P ::/RD2/RD2.INI | tr -d '\r' | grep -q "^Installieren=0" && echo "ok     RD2.INI uebernommen, Installation nicht noch einmal" || { echo "FEHLER RD2.INI"; F=1; }
[ $FALL = 2 ] && { [ $F = 0 ] && echo "Grafische Installation (bestehendes DOS): alles ok" || echo "Grafische Installation (bestehendes DOS): FEHLER"; exit $F; }   # Platte ohne Startcode: kein Start von C:
rm -f $S
timeout 100 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$HD,format=raw,if=ide,index=0 -boot c \
  -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!; ok=0
for i in $(seq 60); do sleep 1; Q "screendump obj/inst_c.ppm"; python3 -c "from PIL import Image; import sys; sys.exit(0 if Image.open('obj/inst_c.ppm').size==(640,480) else 1)" 2>/dev/null && { ok=1; break; }; done
[ $ok = 1 ] && { echo "ok     startet ohne Diskette von C: bis zur Oberflaeche"; sleep 3; Q "screendump obj/inst_c.ppm"; png obj/inst_c; } || { echo "FEHLER Start von C:"; F=1; }
Q quit; wait $QP 2>/dev/null
[ $F = 0 ] && echo "Grafische Installation: alles ok" || echo "Grafische Installation: FEHLER"
exit $F
