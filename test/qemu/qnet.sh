#!/bin/bash
# QEMU-Netztest: FreeDOS + NE2000 (Crynwr) + Programm(e) auf B:, Ausgabe B:\OUT.TXT
# qnet.sh "befehle;durch;semikolon" datei1 datei2 ...
cd "$(dirname "$0")"
CMDS="$1"; shift
cp boot.img run_a.img
printf 'SHELL=\\FREEDOS\\BIN\\COMMAND.COM \\FREEDOS\\BIN /E:1024 /P=\\FDAUTO.BAT\r\n' > /tmp/cfg.sys
{ printf '@echo off\r\nSET PATH=A:\\FREEDOS\\BIN\r\nB:\r\n'
  [ -z "$NODRV" ] && printf 'NE2000 0x60 3 0x300 > OUT.TXT\r\n'
  echo "$CMDS" | tr ';' '\n' | sed 's/$/\r/'
  printf 'FDAPM POWEROFF\r\n'; } > /tmp/auto.bat
mcopy -o -i run_a.img /tmp/cfg.sys ::/FDCONFIG.SYS; mcopy -o -i run_a.img /tmp/auto.bat ::/FDAUTO.BAT
rm -f run_b.img; mformat -f 1440 -C -i run_b.img ::
mcopy -i run_b.img pkt/NE2000.COM ::/
for f in "$@"; do mcopy -o -i run_b.img "$f" ::/; done
timeout ${QT:-120} qemu-system-i386 -m 1 -display none -no-reboot \
  -drive file=run_a.img,format=raw,if=floppy,index=0 -drive file=run_b.img,format=raw,if=floppy,index=1 -boot a \
  -netdev user,id=n0${QFWD:+,$QFWD} -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 ${QEXTRA} >/dev/null 2>&1
echo "qemu rc=$?"
mtype -i run_b.img ::/OUT.TXT 2>/dev/null | tr -d '\r'
