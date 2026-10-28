#!/bin/bash
# QEMU-Test DOS-Shell unter echtem DOS (FreeDOS, FreeCOM): Datei > DOS-Shell, PAUSE eingeben.
# Waehrend PAUSE auf die Taste wartet, muss die Rueckfrage schon live im Fenster stehen.
cd "$(dirname "$0")"
R=../..
cp boot.img disk_sh.img
printf 'SHELL=\\FREEDOS\\BIN\\COMMAND.COM \\FREEDOS\\BIN /E:1024 /P=\\FDAUTO.BAT\r\n' > /tmp/cfg.sys
printf '@echo off\r\nSET PATH=A:\\FREEDOS\\BIN\r\nSET COMSPEC=A:\\FREEDOS\\BIN\\COMMAND.COM\r\nA:\\CTMOUSE.EXE >NUL\r\nset RDHOME=A:\\RDESK\r\ncall A:\\RDESK\\RETRO.BAT /CGA /NOSND\r\nFDAPM POWEROFF\r\n' > /tmp/auto.bat
mcopy -o -i disk_sh.img /tmp/cfg.sys ::/FDCONFIG.SYS; mcopy -o -i disk_sh.img /tmp/auto.bat ::/FDAUTO.BAT
mcopy -o -i disk_sh.img CTMOUSE.EXE ::/
mmd -i disk_sh.img ::/RDESK 2>/dev/null
for f in $R/RDESK.EXE $R/RETRO.BAT $R/RDGO.COM $R/RDSHELL.OVL; do mcopy -o -i disk_sh.img "$f" ::/RDESK/; done
rm -f /tmp/qmon.sock
timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -vga std \
  -drive file=disk_sh.img,format=raw,if=floppy,index=0 -boot a \
  -monitor unix:/tmp/qmon.sock,server,nowait >/dev/null 2>&1 &
QP=$!
Q="python3 qmon.py"
for i in $(seq 90); do sleep 2; [ "$($Q mode 2>/dev/null)" = "(640, 200)" ] && break; done
sleep 8
$Q "sendkey f10" sleep2 "sendkey down" "sendkey down" "sendkey down" "sendkey down" "sendkey down" "sendkey down" sleep1 "sendkey ret" sleep4
$Q "sendkey p" "sendkey a" "sendkey u" "sendkey s" "sendkey e" "sendkey ret" sleep12 "screendump /tmp/qsh1.ppm"
$Q "sendkey ret" sleep10 "screendump /tmp/qsh2.ppm"
kill $QP 2>/dev/null; wait $QP 2>/dev/null
python3 - <<'PY'
from PIL import Image
def dark(n, y0, y1, x1=624):
    im = Image.open(f'/tmp/qsh{n}.ppm').convert('L').resize((640, 200), Image.NEAREST)
    return sum(1 for y in range(y0, y1) for x in range(256, x1) if im.getpixel((x, y)) < 128)
ok = lambda b: "ok" if b else "FEHLER"
print("DOS-Shell (QEMU, FreeDOS): PAUSE-Rueckfrage live im Fenster:", ok(dark(1, 152, 176) > 300 and dark(1, 182, 190, 400) > 60))
print("DOS-Shell (QEMU, FreeDOS): nach der Taste fertig, Eingabezeile wieder frei:", ok(dark(2, 152, 176) > 300 and dark(2, 182, 190, 400) < 40))
PY
