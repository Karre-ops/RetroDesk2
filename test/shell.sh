#!/bin/bash
# RD2.EXE als Shell (CONFIG.SYS: SHELL=A:\RD2\RD2.EXE ..., kein COMMAND.COM), QEMU-Diskette ohne XMS.
# RD legt den Rest (Modus 2) an und laeuft darunter. DOSTEST ruft zweimal RunDos (Neustart),
# danach endet RD; der Rest zeigt "RetroDesk ist beendet", nach einer Taste startet RD wieder.
# /P ist noetig: der FreeDOS-Kern traegt bei der Shell nicht sich selbst als Eltern ein (MS-DOS schon).
cd "$(dirname "$0")/.."
F=0
for lauf in "/P"; do
  IMG=obj/shell720.img; SER=obj/com1_shell.txt; rm -f $SER /tmp/qmon_s.sock
  echo "=== SHELL=A:\\RD2\\RD2.EXE ${lauf:-(ohne /P)}"
  echo "@echo off" > obj/ae_shell.txt
  SHELLZEILE="SHELL=A:\\RD2\\RD2.EXE $lauf A:\\RD2\\DOSTEST.EXE A:\\RD2\\FREIMEM.EXE A:\\FREI.TXT N" bash tools/mkdisk.sh $IMG obj/ae_shell.txt
  mdel -i $IMG ::/COMMAND.COM
  timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
    -serial file:$SER -monitor unix:/tmp/qmon_s.sock,server,nowait >/dev/null 2>&1 &
  QP=$!
  Q="python3 test/qemu/qmon.py"; export QMON=/tmp/qmon_s.sock
  waitscr() { for i in $(seq ${2:-30}); do $Q text 2>/dev/null | grep -q "$1" && return 0; sleep 1; done; return 1; }
  if waitscr "RetroDesk ist beendet" 200; then echo "ok     Rest meldet das Ende und wartet"; else echo "FEHLER keine Meldung vom Rest"; F=1; fi
  N1=$(grep -c "DOSTEST: mit KEEP" $SER)
  $Q "sendkey spc" >/dev/null 2>&1
  for i in $(seq 60); do [ $(grep -c "DOSTEST: mit KEEP" $SER) -gt $N1 ] && break; sleep 1; done
  N2=$(grep -c "DOSTEST: mit KEEP" $SER)
  kill $QP 2>/dev/null; wait $QP 2>/dev/null
  L=$(tr -d '\r' < $SER)
  chk() { if echo "$L" | grep -q "$1"; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
  chk "DOSTEST: zweiter Lauf" "RD nach dem ersten DOS-Programm wieder da"
  chk "DOSTEST: nach Neustart wieder da" "RD nach dem zweiten DOS-Programm wieder da"
  [ "$N2" -gt "$N1" ] && echo "ok     nach der Taste startet RD wieder" || { echo "FEHLER kein Neustart nach Taste ($N1/$N2)"; F=1; }
  mcopy -o -i $IMG ::/FREI.TXT obj/shell_frei.txt 2>/dev/null && echo "ok     DOS-Programm bekam $(head -1 obj/shell_frei.txt | tr -d '\r' | cut -d' ' -f2-)" || { echo "FEHLER DOS-Programm lief nicht"; F=1; }
done
[ $F = 0 ] && echo "Shell: alles ok"
exit $F
