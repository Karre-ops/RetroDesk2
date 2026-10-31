#!/bin/bash
# Installation mit "C: vorher formatieren" (31.10.2026, Karsten): Platte mit altem DOS und Daten, von RD2_1.IMG booten,
# Haekchen Formatieren, Warnung Ja, Diskette 2, RDFORMAT (OK, Ja), Diskette 1 zurueck, dann normale Installation mit
# FreeDOS-Kern. Danach: alte Dateien weg, KERNEL.SYS und RD2 da, Start von C: bis zur Oberflaeche. Laeuft gut 4 Min.:
# nohup bash test/install_fmt.sh > obj/install_fmt.txt &
cd "$(dirname "$0")/.."
F=0; S=/tmp/qmon_ifmt.sock; SER=obj/com1_ifmt.txt; export QMON=$S
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
T() { timeout 10 python3 test/qemu/qmon.py text 2>/dev/null; }
waitscr() { for i in $(seq ${2:-30}); do T | grep -q "$1" && return 0; sleep 1; done; return 1; }
waitser() { for i in $(seq ${3:-60}); do [ "$(tr -d '\r' < $SER | grep -c "$1")" -ge "${2:-1}" ] && return 0; sleep 1; done; return 1; }
schritt() { if waitser "$1" ${3:-1} ${4:-90}; then echo "ok     $2"; return 0; else echo "FEHLER $2"; tr -d '\r' < $SER | tail -4; F=1; return 1; fi; }
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*) kill ${p#/proc/} 2>/dev/null;; esac; done
bash tools/mkset.sh >/dev/null 2>&1 || { echo "FEHLER mkset"; exit 1; }
for i in 1 2 3; do cp out/set/RD2_$i.IMG obj/ifmt$i.img; done
HD=obj/ifmt_hd.img; bash tools/mkhd.sh $HD >/dev/null
dd if=test/daten/mbr_rdfdisk.bin of=$HD conv=notrunc 2>/dev/null
C="-i $HD@@32256"
printf 'x' > obj/ifmt_io.tmp; mcopy $C obj/ifmt_io.tmp ::/IO.SYS
printf 'SHELL=C:\\COMMAND.COM /P\r\n' > obj/ifmt_cs.tmp; mcopy $C obj/ifmt_cs.tmp ::/CONFIG.SYS
mmd $C ::/SPIELE; mcopy $C obj/ifmt_io.tmp ::/SPIELE/SIMCITY.EXE
rm -f $S $SER; touch $SER
timeout 330 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/ifmt1.img,format=raw,if=floppy,index=0 \
  -drive file=$HD,format=raw,if=ide,index=0 -boot a -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
waitscr "Erster Start" 90 && echo "ok     Einrichtung erscheint" || { echo "FEHLER keine Einrichtung"; F=1; }
sleep 1; Q "sendkey i"
schritt "INSTALL: Start" "Installation offen" && { sleep 6; Q "sendkey alt-f"; sleep 1; Q "screendump obj/ifmt_dialog.ppm"; Q "sendkey ret"; }
schritt "INSTALL: Frage" "Warnung vor dem Formatieren" && { sleep 1; Q "sendkey ret"; }
schritt "Diskette 2 (RD2" "Diskette 2 fuer RDFORMAT" && { Q "change floppy0 obj/ifmt2.img"; sleep 2; Q "sendkey ret"; }
schritt "RDFORMAT: bereit" "RDFORMAT startet fuer C:" && { sleep 2; Q "sendkey ret"; }
schritt "RDFORMAT: Meldung 22" "RDFORMAT fragt nach" && { sleep 2; Q "sendkey ret"; }
schritt "RDFORMAT: fertig" "C: formatiert" 1 120 && { sleep 2; Q "sendkey ret"; }
schritt "Diskette 1 (RD2SYSTEM)" "Diskette 1 zurueck" && { Q "change floppy0 obj/ifmt1.img"; sleep 2; Q "sendkey ret"; }
schritt "INSTALL: formatiert" "Installer sieht leere Platte"
for d in 2 3; do schritt "Diskette $d (RD2" "Diskette $d" $([ $d = 2 ] && echo 2 || echo 1) && { Q "change floppy0 obj/ifmt$d.img"; sleep 2; Q "sendkey ret"; }; done
schritt "Diskette 1 (RD2SYSTEM)" "Diskette 1 wieder" 2 && { Q "change floppy0 obj/ifmt1.img"; sleep 2; Q "sendkey ret"; }
waitser "startf" 1 60 && { sleep 1; Q "sendkey ret"; }
waitser "Diskette 1 drin lassen" 1 60 && { sleep 1; Q "sendkey ret"; }
schritt "SHELL: bereit" "RDSYS gelaufen, RD2 wieder da" 2 90
sleep 2; Q quit; wait $QP 2>/dev/null
P=$HD@@32256
for f in KERNEL.SYS CONFIG.SYS RD2/RD.EXE RD2/SHELL.EXE RD2/RDFORMAT.EXE RD2/RDNET.EXE; do mdir -i $P ::/$f >/dev/null 2>&1 || { echo "FEHLER C:\\$f fehlt"; F=1; }; done
for f in IO.SYS SPIELE; do mdir -i $P ::/$f >/dev/null 2>&1 && { echo "FEHLER C:\\$f noch da"; F=1; }; done
mtype -i $P ::/CONFIG.SYS | tr -d '\r' | grep -q "^SHELL=C:.RD2.RD.EXE" && ! mtype -i $P ::/CONFIG.SYS | grep -q COMMAND && echo "ok     CONFIG.SYS neu, RD2 ist die Shell" || { echo "FEHLER CONFIG.SYS"; F=1; }
rm -f $S
timeout 100 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$HD,format=raw,if=ide,index=0 -boot c -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!; ok=0
for i in $(seq 60); do sleep 1; Q "screendump obj/ifmt_c.ppm"; python3 -c "from PIL import Image; import sys; sys.exit(0 if Image.open('obj/ifmt_c.ppm').size==(640,480) else 1)" 2>/dev/null && { ok=1; break; }; done
[ $ok = 1 ] && echo "ok     startet von C: (FreeDOS-Kern) bis zur Oberflaeche" || { echo "FEHLER Start von C:"; F=1; }
Q quit; wait $QP 2>/dev/null
[ $F = 0 ] && echo "Installation mit Formatieren: alles ok" || echo "Installation mit Formatieren: FEHLER"
exit $F
