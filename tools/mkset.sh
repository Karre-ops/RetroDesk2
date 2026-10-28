#!/bin/bash
# mkset.sh - Verteil-Diskettensatz RD2 (360 KB, SPEC 12): out/set/RD2_1.IMG .. RD2_4.IMG
# 1 RD2SYSTEM (bootfaehig, FreeDOS-Kern 2046 8086/FAT16), 2 RD2 PROG, 3 RD2 NETZ, 4 RD2 EXTRA (SDK).
# Zuordnung aus Spalte 4 von tools/budget.txt (S/P/N/D).
set -e
cd "$(dirname "$0")/.."
O=out/set; T=obj/set.tmp
python3 -c "import shutil,os; shutil.rmtree('$T', True); os.makedirs('$T'); os.makedirs('$O', exist_ok=True)"
K=disk/fdkern/KERNEL.SYS; [ -f $K ] || { echo "mkset: $K fehlt (ke2046_86f16.zip)"; exit 1; }
liste() { awk -v d=$1 '!/^[#@]/ && NF>=4 && $4==d {print $1}' tools/budget.txt; }
neu() { rm -f $O/$1; mformat -C -f 360 -v "$2" -i $O/$1 ::; }
kopie() { img=$1; shift; for f in "$@"; do [ -f out/$f ] || { echo "mkset: out/$f fehlt, nicht auf $img" >&2; continue; }; mcopy -i $O/$img out/$f ::/RD2/; done; }
txt() { sed 's/$/\r/' "$1" > $T/$2; }
# Diskette 1
neu RD2_1.IMG RD2SYSTEM
python3 - test/qemu/boot.img $O/RD2_1.IMG <<'PY'
import sys
s = open(sys.argv[1], 'rb').read(512); f = open(sys.argv[2], 'r+b'); d = bytearray(f.read(512))
d[0:3] = s[0:3]; d[0x3E:0x200] = s[0x3E:0x200]   # FreeDOS-Bootsektor (laedt KERNEL.SYS), BPB der 360-KB-Diskette
f.seek(0); f.write(d)
PY
mcopy -i $O/RD2_1.IMG $K ::/KERNEL.SYS
printf 'INSTALL=A:\\RD2\\RDKEYB.COM DE\r\nINSTALL=A:\\RD2\\RDMAUS.COM\r\nSHELL=A:\\RD2\\RD.EXE /P A:\\RD2\\SHELL.EXE\r\n' > $T/CONFIG.SYS
txt disk/fdsys/COPYING COPYING; txt docs/LIZENZ.TXT LIZENZ.TXT; txt docs/QUELLEN.TXT QUELLEN.TXT
mcopy -i $O/RD2_1.IMG $T/CONFIG.SYS $T/COPYING $T/LIZENZ.TXT $T/QUELLEN.TXT ::/
mmd -i $O/RD2_1.IMG ::/RD2; kopie RD2_1.IMG $(liste S)
# Disketten 2 bis 4
neu RD2_2.IMG "RD2 PROG"; mmd -i $O/RD2_2.IMG ::/RD2; kopie RD2_2.IMG $(liste P); mcopy -i $O/RD2_2.IMG $T/COPYING $T/QUELLEN.TXT ::/
neu RD2_3.IMG "RD2 NETZ"; mmd -i $O/RD2_3.IMG ::/RD2; kopie RD2_3.IMG $(liste N)
neu RD2_4.IMG "RD2 EXTRA"; mmd -i $O/RD2_4.IMG ::/SDK
mcopy -i $O/RD2_4.IMG sdk/RD2.H sdk/RD2API.H sdk/RD2DRV.H sdk/RD2ORD.H sdk/RD2START.ASM sdk/RD2MOD.ASM out/RD2.LIB ::/SDK/
mcopy -i $O/RD2_4.IMG $T/COPYING $T/LIZENZ.TXT $T/QUELLEN.TXT ::/
for i in 1 2 3 4; do echo "RD2_$i.IMG: $(mdir -i $O/RD2_$i.IMG -/ :: | tail -1)"; done
python3 -c "import shutil; shutil.rmtree('$T', True)"
