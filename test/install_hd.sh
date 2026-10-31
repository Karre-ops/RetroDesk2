#!/bin/bash
# Installation aus einem Festplattenordner (Weg ueber RD1-FTP, 29.10.2026): ${Z:-release/2.0}/RD2_HDD.ZIP liegt entpackt
# in C:\RD2NEU (bestehendes DOS mit CONFIG.SYS/AUTOEXEC.BAT), RD2 wird von dort gestartet: Erster Start -> RD2SETUP,
# "I" -> INSTALL.EXE, Ziel C:\RD2, ohne Diskettenwechsel, Fragen zu CONFIG.SYS, danach Start von C:\RD2.
# Gestartet ueber C:\RD2NEU\RD2INST.BAT. INI=1: im Ordner liegt schon eine RD2.INI (RD2INST ruft erst RD2SETUP auf).
cd "$(dirname "$0")/.."
F=0; S=/tmp/qmon_ihd.sock; SER=obj/com1_ihd.txt; export QMON=$S
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
T() { timeout 10 python3 test/qemu/qmon.py text 2>/dev/null; }
waitscr() { for i in $(seq ${2:-30}); do T | grep -q "$1" && return 0; sleep 1; done; return 1; }
waitser() { for i in $(seq ${3:-60}); do [ "$(tr -d '\r' < $SER | grep -c "$1")" -ge "${2:-1}" ] && return 0; sleep 1; done; return 1; }
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*) kill ${p#/proc/} 2>/dev/null;; esac; done
[ -f ${Z:-release/2.0}/RD2_HDD.ZIP ] || { echo "FEHLER ${Z:-release/2.0}/RD2_HDD.ZIP fehlt"; exit 1; }
python3 - <<'PY'
import zipfile, shutil, os
shutil.rmtree('obj/ihd', True); os.makedirs('obj/ihd'); zipfile.ZipFile(os.environ.get('Z', 'release/2.0') + '/RD2_HDD.ZIP').extractall('obj/ihd')
PY
HD=obj/ihd_hd.img; bash tools/mkhd.sh $HD >/dev/null; dd if=test/daten/mbr_rdfdisk.bin of=$HD conv=notrunc 2>/dev/null
C="-i $HD@@32256"
printf 'FILES=20\r\nSHELL=C:\\COMMAND.COM C:\\ /P\r\n' > obj/ihd_cs.tmp; mcopy $C obj/ihd_cs.tmp ::/CONFIG.SYS
printf '@ECHO OFF\r\nPATH A:\\\r\nLH C:\\TREIBER\\MOUSE.COM /S50\r\nREM Ende\r\n' > obj/ihd_ae.tmp; mcopy $C obj/ihd_ae.tmp ::/AUTOEXEC.BAT
mmd $C ::/TREIBER; mcopy $C test/qemu/CTMOUSE.EXE ::/TREIBER/MOUSE.COM
printf 'x' > obj/io.tmp; mcopy $C obj/io.tmp ::/IO.SYS
mmd $C ::/RD2NEU; mcopy -s $C obj/ihd/RD2/* ::/RD2NEU/
[ "${INI:-0}" = 1 ] && { printf '[System]\r\nGrafik=VGA\r\n' > obj/ihd_ini.tmp; mcopy $C obj/ihd_ini.tmp ::/RD2NEU/RD2.INI; }
printf '@echo off\r\nCALL C:\\RD2NEU\\RD2INST.BAT\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ihd_boot.txt
NUR="FREIMEM.EXE" bash tools/mkdisk.sh obj/ihd_a.img obj/ihd_boot.txt >/dev/null 2>&1
rm -f $S $SER; touch $SER
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/ihd_a.img,format=raw,if=floppy,index=0 \
  -drive file=$HD,format=raw,if=ide,index=0 -boot a -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
waitscr "Einrichtung" 60 && echo "ok     RD2INST.BAT startet die Einrichtung (INI=${INI:-0})" || { echo "FEHLER keine Einrichtung"; F=1; }
sleep 1; Q "sendkey i"
if waitser "INSTALL: Start" 1 60; then echo "ok     INSTALL.EXE startet, Quelle $(tr -d '\r' < $SER | grep -a -A1 'INSTALL: Start' | tail -1)"; sleep 3; Q "sendkey ret"
else echo "FEHLER INSTALL.EXE startet nicht"; F=1; fi
for q in "als Shell in CONFIG.SYS" "MOUSE.COM /S50"; do
  waitser "$q" 1 60 && { echo "ok     Frage: $q"; sleep 1; Q "sendkey ret"; } || { echo "FEHLER keine Frage: $q"; F=1; }
done
waitser "PATH-Zeilen" 1 30 && { sleep 1; Q "sendkey ret"; }
grep -aq "Diskette 2 (RD2" <(tr -d '\r' < $SER) && { echo "FEHLER fragt nach Diskette"; F=1; } || echo "ok     kein Diskettenwechsel"
waitser "INSTALL: Fehler" 1 60; grep -aq "INSTALL: Fehler 0x00000000" <(tr -d '\r' < $SER) && echo "ok     Kopieren ohne Fehler" || { echo "FEHLER beim Kopieren"; F=1; }
waitser "ist installiert" 1 60 && { echo "ok     fertig"; sleep 1; Q "sendkey ret"; } || { echo "FEHLER keine Fertigmeldung"; F=1; }
sleep 2; Q quit; wait $QP 2>/dev/null
n=$(mdir -b $C ::/RD2 2>/dev/null | wc -l); m=$(ls -p obj/ihd/RD2 | grep -vc /)
[ "$n" -ge $((m - 3)) ] && echo "ok     C:\\RD2: $n Dateien (Paket $m)" || { echo "FEHLER C:\\RD2 hat $n Dateien (Paket $m)"; F=1; }
for f in SDK/RD2.H SDK/RD2.LIB SDK/BEISPIEL/HALLO.C LIZENZ.TXT; do
  mtype $C ::/RD2/$f 2>/dev/null | cmp -s - obj/ihd/RD2/$f && echo "ok     C:\\RD2\\$f" || { echo "FEHLER C:\\RD2\\$f fehlt oder anders"; F=1; }
done
mtype $C ::/CONFIG.SYS | tr -d '\r' > obj/ihd_cfg.txt
grep -qi "SHELL=C:\\\\RD2\\\\RD.EXE" obj/ihd_cfg.txt && echo "ok     CONFIG.SYS: SHELL=C:\\RD2\\RD.EXE" || { echo "FEHLER CONFIG.SYS"; cat obj/ihd_cfg.txt; F=1; }
grep -qi "RD2NEU" obj/ihd_cfg.txt && { echo "FEHLER CONFIG.SYS zeigt auf C:\\RD2NEU"; F=1; }
exit $F
