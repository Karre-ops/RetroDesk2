#!/bin/bash
# QEMU-Test Formatieren (RDFMT.OVL) unter FreeDOS: Testbau (MTEST) spielt \MOUSE.SCR ab.
# QEMU-Laufwerk B: ist 1,44 MB (Vorschlag), Leertaste hakt 720 KB an (erste Zeile).
# Rechtsklick auf B:, Formatieren..., 720 KB, Bezeichnung RDTEST, Ja. Danach B: mit mtools pruefen.
# Aufruf: qfmt.sh [quick]   (quick: B: ist schon formatiert, nur Verwaltung neu)
cd "$(dirname "$0")"
R=../..
cp boot.img disk_fm.img
( cd $R; export WATCOM=${WATCOM:-/opt/ow} PATH=/opt/ow/binl64:/opt/ow/binl:$PATH INCLUDE=${INCLUDE:-/opt/ow/h}
  wcl -q -0 -ms -os -s -fpi -bt=dos -dMTEST -fe=/tmp/RDESKT.EXE main.c gfx.c mouse.c snd.c rdui.c fpu.c fm.c launch.c desk.c calc.c mod.c sh.c >/dev/null; rm -f *.o )
if [ "$1" = quick ]; then
  dd if=/dev/zero of=/tmp/fb.img bs=1024 count=720 2>/dev/null; mformat -i /tmp/fb.img -f 720 -v ALT ::
  echo alt > /tmp/alt.txt; mcopy -i /tmp/fb.img /tmp/alt.txt ::/
  Q1="k {DOWN}{DOWN}{SPC};w 2;"                     # Haekchen Schnell (3. Zeile im Dialog)
elif [ "$1" = falsch ]; then                          # 1,44-MB-Format, Schnell mit 720 KB: muss abgelehnt werden
  dd if=/dev/zero of=/tmp/fb.img bs=1024 count=720 2>/dev/null; mformat -i /tmp/fb.img -t 80 -h 2 -s 18 -v ALT ::
  Q1="k {DOWN}{DOWN}{SPC};w 2;"
elif [ "$1" = ed ]; then                              # 2,88-MB-Laufwerk: Vorschlag 2,88 MB uebernehmen
  dd if=/dev/zero of=/tmp/fb.img bs=1024 count=2880 2>/dev/null; Q1=""; SP=""; FDT="-global isa-fdc.fdtypeB=288"
elif [ "$1" = ss ]; then                              # 1,2-MB-Laufwerk: erste Zeile = 180 KB einseitig
  dd if=/dev/zero of=/tmp/fb.img bs=1024 count=180 2>/dev/null; Q1=""; FDT="-global isa-fdc.fdtypeB=120"
else
  dd if=/dev/zero of=/tmp/fb.img bs=1024 count=720 2>/dev/null; Q1=""
fi
SP=${SP-"k {SPC};w 2;"}
X=84; K2="k {ENTER};w 3;"
[ "$1" = home ] && { X=48; K2="k {ENTER};w 3;k {ENTER};w 3;k {ESC};w 3;"; }   # A: = RetroDesk-Diskette: Wechsel verlangt, Abbruch
printf 'SHELL=\\FREEDOS\\BIN\\COMMAND.COM \\FREEDOS\\BIN /E:1024 /P=\\FDAUTO.BAT\r\n' > /tmp/cfg.sys
printf '@echo off\r\nSET PATH=A:\\FREEDOS\\BIN\r\nA:\\CTMOUSE.EXE >NUL\r\nset RDHOME=A:\\RDESK\r\ncall A:\\RDESK\\RETRO.BAT /CGA /NOSND /KT\r\nFDAPM POWEROFF\r\n' > /tmp/auto.bat
mcopy -o -i disk_fm.img /tmp/cfg.sys ::/FDCONFIG.SYS; mcopy -o -i disk_fm.img /tmp/auto.bat ::/FDAUTO.BAT
mcopy -o -i disk_fm.img CTMOUSE.EXE ::/
mmd -i disk_fm.img ::/RDESK 2>/dev/null
for f in $R/RETRO.BAT $R/RDGO.COM $R/RDFMT.OVL; do mcopy -o -i disk_fm.img "$f" ::/RDESK/; done
mcopy -o -i disk_fm.img /tmp/RDESKT.EXE ::/RDESK/RDESK.EXE
printf "$(python3 $R/test/mouse.py "w 20;r $X 40;w 3;c $((X+40)) 57;w 3;${SP}${Q1}k {ENTER};w 3;k RDTEST{ENTER};w 3;${K2}k {ENTER};w 5;r 84 40;w 3;c 124 45;w 3;k {ENTER};w 3;k {ALTD}{UP}{ENTER};w 3;k {ENTER}")" > /tmp/MOUSE.SCR
mcopy -o -i disk_fm.img /tmp/MOUSE.SCR ::/
timeout 280 qemu-system-i386 -m 1 -display none -no-reboot -vga std \
  -drive file=disk_fm.img,format=raw,if=floppy,index=0 -drive file=/tmp/fb.img,format=raw,if=floppy,index=1 $FDT -boot a >/dev/null 2>&1
mtype -i disk_fm.img ::/DINFO.TXT 2>/dev/null | tr -d '\r' > /tmp/dinfo.txt; cat /tmp/dinfo.txt
if [ "$1" = falsch ]; then
  ok(){ if eval "$2"; then echo "$1: ok"; else echo "$1: FEHLER"; fi; }
  ok "Formatieren Schnell (QEMU): anderes Format abgelehnt, Diskette unveraendert" 'grep -q "Anderes Format" /tmp/dinfo.txt && mlabel -i /tmp/fb.img -s :: | grep -q ALT'; exit
fi
if [ "$1" = home ]; then
  ok(){ if eval "$2"; then echo "$1: ok"; else echo "$1: FEHLER"; fi; }
  ok "Formatieren A: (QEMU): RetroDesk-Diskette wird nicht formatiert" '! grep -q "FMT r=" /tmp/dinfo.txt && mdir -i disk_fm.img ::/RDESK | grep -q RDFMT'; exit
fi
ok(){ if eval "$2"; then echo "$1: ok"; else echo "$1: FEHLER"; fi; }
if [ "$1" = ed ] || [ "$1" = ss ]; then
  [ "$1" = ed ] && { N="2,88 MB"; F="2 931 712 bytes free"; } || { N="180 KB einseitig"; F="179 712 bytes free"; }
  ok "Formatieren $N (QEMU): Bezeichnung RDTEST, leer" "mlabel -i /tmp/fb.img -s :: 2>/dev/null | grep -q RDTEST && mdir -i /tmp/fb.img :: 2>/dev/null | grep -q \"$F\""; exit
fi
ok "Formatieren (QEMU): Diskette B: lesbar, Bezeichnung RDTEST" 'mlabel -i /tmp/fb.img -s :: 2>/dev/null | grep -q RDTEST'
ok "Formatieren (QEMU): 720 KB, leer" 'mdir -i /tmp/fb.img :: 2>/dev/null | grep -q "730 112 bytes free"'
ok "Formatieren (QEMU): FAT12, Seriennummer, Bootsektor 55AA" 'minfo -i /tmp/fb.img :: 2>/dev/null | grep -q "serial number: 1A" && [ "$(od -An -tx1 -j510 -N2 /tmp/fb.img | tr -d " ")" = 55aa ]'
ok "Laufwerksinfo (QEMU): Diskette, Bezeichnung, Seriennummer, FAT12" 'grep -q "Typ: Diskette" /tmp/dinfo.txt && grep -q "Bezeichnung: RDTEST" /tmp/dinfo.txt && grep -q "FAT12" /tmp/dinfo.txt'
