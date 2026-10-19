#!/bin/bash
# mkdisk.sh IMAGE AUTOEXEC.BAT  - bootfaehige 720-KB-Systemdiskette fuer die automatischen Tests.
# FreeDOS-Kern und COMMAND.COM aus test/qemu/boot.img (DOS 5 darf nicht weitergegeben werden;
# am PC10-III wird dieselbe Verzeichnisstruktur auf eine DOS-5-Bootdiskette kopiert).
set -e
cd "$(dirname "$0")/.."
IMG=$1; AE=$2; SRC=../test/qemu/boot.img
rm -f $IMG
mformat -C -f 720 -v RD2SYSTEM -i $IMG ::
python3 - $SRC $IMG <<'PY'
import sys
s = open(sys.argv[1], 'rb').read(512); f = open(sys.argv[2], 'r+b'); d = bytearray(f.read(512))
d[0:3] = s[0:3]; d[0x3E:0x200] = s[0x3E:0x200]          # FreeDOS-Bootcode, BPB der 720-KB-Diskette behalten
f.seek(0); f.write(d)
PY
T=obj/disk.tmp
python3 -c "import shutil; shutil.rmtree('$T', True)"; mkdir -p $T
mcopy -i $SRC ::/KERNEL.SYS $T/; mcopy -i $SRC ::/FREEDOS/BIN/COMMAND.COM $T/; mcopy -i $SRC ::/FREEDOS/BIN/FDAPM.COM $T/
mcopy -i $IMG $T/KERNEL.SYS $T/COMMAND.COM ::/
printf 'SHELL=A:\\COMMAND.COM A:\\ /E:512 /P\r\n' > $T/CONFIG.SYS
sed 's/$/\r/' $AE > $T/AUTOEXEC.BAT
mcopy -i $IMG $T/CONFIG.SYS $T/AUTOEXEC.BAT $T/FDAPM.COM ::/
mmd -i $IMG ::/RD2
for f in out/*.EXE out/*.MOD out/*.DRV out/*.FNT out/*.LNG; do [ -f "$f" ] && mcopy -i $IMG "$f" ::/RD2/; done
python3 -c "import shutil; shutil.rmtree('$T', True)"
exit 0
