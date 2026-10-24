#!/bin/bash
# Installation: RD2SETUP von der Diskette, Taste I, Ziel C:\RD2, AUTOEXEC.BAT eintragen (J).
# Prueft auf der Festplatte: Dateien, RD2.INI, AUTOEXEC.BAT-Eintrag und Sicherung AUTOEXEC.RDA.
cd "$(dirname "$0")/.."
IMG=obj/inst720.img; HD=obj/inst_hd.img; F=0
printf '@echo off\r\nA:\\RD2\\RD2SETUP.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' | tr -d '\r' > obj/ae_inst.txt
SETUP=1 bash tools/mkdisk.sh $IMG obj/ae_inst.txt
bash tools/mkhd.sh $HD; C="-i $HD@@32256"
printf 'PATH A:\\\r\n' > obj/ae_alt.tmp; mcopy $C obj/ae_alt.tmp ::/AUTOEXEC.BAT
rm -f /tmp/qmon_i.sock
timeout 250 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -drive file=$HD,format=raw,index=0,media=disk -boot a \
  -monitor unix:/tmp/qmon_i.sock,server,nowait >/dev/null 2>&1 &
QP=$!; export QMON=/tmp/qmon_i.sock; Q="python3 ../test/qemu/qmon.py"
waitscr() { for i in $(seq ${2:-30}); do $Q text 2>/dev/null | grep -q "$1" && return 0; sleep 1; done; return 1; }
waitscr "Einrichtung" 120 || { echo "FEHLER keine Einrichtung"; F=1; }
$Q "sendkey i" sleep2 "sendkey ret" >/dev/null 2>&1
waitscr "In AUTOEXEC.BAT eintragen" 90 && $Q "sendkey j" sleep2 >/dev/null 2>&1 || { echo "FEHLER keine Rueckfrage"; F=1; }
waitscr "AUTOEXEC.BAT erg" 20; $Q text > obj/schirm_install.txt 2>/dev/null
$Q "sendkey esc" sleep2 "sendkey n" >/dev/null 2>&1
wait $QP
mdir $C -b ::/RD2 > obj/inst_dir.txt 2>&1
for d in RD.EXE RDGDI.MOD RDSWAP.BIN RD2SETUP.EXE RD2.INI; do grep -qi "/$d" obj/inst_dir.txt && echo "ok     C:\\RD2\\$d" || { echo "FEHLER $d fehlt"; F=1; }; done
mtype $C ::/AUTOEXEC.BAT | tr -d '\r' > obj/inst_ae.txt; grep -q "^C:\\\\RD2\\\\RD.EXE" obj/inst_ae.txt && grep -q "PATH A" obj/inst_ae.txt && echo "ok     AUTOEXEC.BAT ergaenzt" || { echo "FEHLER AUTOEXEC.BAT"; cat obj/inst_ae.txt; F=1; }
mtype $C ::/AUTOEXEC.RDA >/dev/null 2>&1 && echo "ok     Sicherung AUTOEXEC.RDA" || { echo "FEHLER keine AUTOEXEC.RDA"; F=1; }
[ $F = 0 ] && echo "Installation: alles ok"
exit $F
