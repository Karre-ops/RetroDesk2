#!/bin/bash
# Startlaufwerk (SYSINFO.bootlw, 29.10.2026): RD2 von C: gestartet, ohne Diskette. OPTIONEN.EXE (Testmodus ohne Ordner)
# muss C:\CONFIG.SYS lesen ("Start an"), nicht A:. Vorher war bootlw fest 1 (A:): Optionen, RDNET, RDFTP, RDSHELL
# wollten auf einer Festplatteninstallation die Diskette. Braucht obj/inst_hd.img aus test/install.sh (FALL=1).
cd "$(dirname "$0")/.."
S=/tmp/qmon_blw.sock; SER=obj/com1_blw.txt; export QMON=$S; F=0
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*) kill ${p#/proc/} 2>/dev/null;; esac; done
[ -f obj/inst_hd.img ] || { echo "FEHLER obj/inst_hd.img fehlt (erst test/install.sh)"; exit 1; }
HD=obj/blw_hd.img; cp obj/inst_hd.img $HD; P="-i $HD@@32256"
mcopy -o $P out/RD2.EXE ::/RD2/RD2.EXE
printf 'SHELL=C:\\RD2\\RD2.EXE /P /NOMAUS /VGA C:\\RD2\\OPTIONEN.EXE T\r\n' > obj/blw_cs.tmp; mcopy -o $P obj/blw_cs.tmp ::/CONFIG.SYS
rm -f $S $SER; touch $SER
timeout 60 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$HD,format=raw,if=ide,index=0 -boot c \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
for i in $(seq 40); do tr -d '\r' < $SER | grep -aq "OPTIONEN: bereit" && break; sleep 1; done
r=$(tr -d '\r' < $SER | grep -a "OPTIONEN: bereit")
case "$r" in *"Start an"*) echo "ok     Optionen liest C:\\CONFIG.SYS (Start an)";; *) echo "FEHLER Optionen: '${r:-keine Meldung}'"; F=1;; esac
Q quit; wait $QP 2>/dev/null
exit $F
