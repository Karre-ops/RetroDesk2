#!/bin/bash
# QEMU-Diskettentest: nur EIN Laufwerk A:, keine Platte. FreeDOS + RetroDesk auf der Bootdiskette,
# Programm auf einer zweiten Diskette. Startet per /TRUN 0 das angeheftete Programm A:\SPIELE\HALLO.EXE,
# wechselt die Disketten ueber den QEMU-Monitor und prueft, dass RetroDesk danach wieder laeuft.
cd "$(dirname "$0")"
R=../..
cp boot.img disk_rd.img
printf 'SHELL=\\FREEDOS\\BIN\\COMMAND.COM \\FREEDOS\\BIN /E:1024 /P=\\FDAUTO.BAT\r\n' > /tmp/cfg.sys
printf '@echo off\r\nSET PATH=A:\\FREEDOS\\BIN\r\nA:\\CTMOUSE.EXE >NUL\r\nset RDHOME=A:\\RDESK\r\ncall A:\\RDESK\\RETRO.BAT /CGA /NOSND /TRUN 0\r\nFDAPM POWEROFF\r\n' > /tmp/auto.bat
mcopy -o -i disk_rd.img /tmp/cfg.sys ::/FDCONFIG.SYS; mcopy -o -i disk_rd.img /tmp/auto.bat ::/FDAUTO.BAT
mcopy -o -i disk_rd.img CTMOUSE.EXE ::/
mmd -i disk_rd.img ::/RDESK
printf 'Hallo=A:\\SPIELE\\HALLO.EXE\r\n' > /tmp/rdesk.ini
for f in $R/RDESK.EXE $R/RETRO.BAT $R/RDGO.COM /tmp/rdesk.ini; do mcopy -o -i disk_rd.img "$f" ::/RDESK/; done
mren -i disk_rd.img ::/RDESK/rdesk.ini ::/RDESK/RDESK.INI 2>/dev/null
rm -f disk_prog.img; mformat -f 720 -C -v PROG -i disk_prog.img ::
mmd -i disk_prog.img ::/SPIELE; mcopy -i disk_prog.img $R/test/data/disk/HALLO.EXE ::/SPIELE/
rm -f /tmp/qmon.sock
timeout 240 qemu-system-i386 -m 1 -display none -no-reboot -vga std \
  -drive file=disk_rd.img,format=raw,if=floppy,index=0 -boot a \
  -monitor unix:/tmp/qmon.sock,server,nowait >/dev/null 2>&1 &
QP=$!
Q="python3 qmon.py"
for i in $(seq 120); do sleep 2; $Q text 2>/dev/null | grep -q "Programmdiskette einlegen" && break; done
$Q text 2>/dev/null | grep -q "Programmdiskette einlegen" && echo "RDGO fragt nach der Programmdiskette: ok" || echo "RDGO fragt nach der Programmdiskette: FEHLER"
$Q "eject -f floppy0" sleep1 "sendkey ret" sleep6          # Laufwerk leer: keine DOS-Abfrage "Abort, Retry, Fail"
$Q text 2>/dev/null | grep -qiE "abort|retry|fail" && echo "Leeres Laufwerk: FEHLER (DOS-Abfrage)" || echo "Leeres Laufwerk: RDGO fragt einfach weiter: ok"
$Q "change floppy0 disk_prog.img" sleep1 "sendkey ret"
for i in $(seq 30); do sleep 2; $Q text 2>/dev/null | grep -q "RetroDesk-Diskette wieder" && break; done
$Q text 2>/dev/null | grep -q "RetroDesk-Diskette wieder" && echo "Nach dem Programm: RDGO will die RetroDesk-Diskette: ok" || echo "Nach dem Programm: FEHLER"
$Q "change floppy0 disk_rd.img" sleep1 "sendkey ret"
wait $QP
echo "qemu rc=$?"
mtype -i disk_prog.img ::/SPIELE/LIEF.TXT 2>/dev/null | grep -q "HALLO lief" && echo "Programm lief von der Programmdiskette: ok" || echo "Programm lief: FEHLER"
mdir -i disk_rd.img ::/RDESK/SHOT.BIN >/dev/null 2>&1 && ! mdir -i disk_rd.img ::/RDESK/RDRUN.BAT >/dev/null 2>&1 && echo "RetroDesk wieder gestartet (Bild), RDRUN.BAT weg: ok" || echo "RetroDesk wieder gestartet: FEHLER"
