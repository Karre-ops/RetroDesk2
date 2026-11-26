#!/bin/bash
# mkset.sh - Verteil-Diskettensatz RD2 (360 KB, SPEC 12): out/set/RD2_1.IMG .. RD2_4.IMG
# 1 RD2SYSTEM (bootfaehig, FreeDOS-Kern 2046 8086/FAT16), 2 RD2 PROG, 3 RD2 NETZ, 4 RD2 EXTRA (SDK), 5 RD2 32 (1,44 MB, ab 386).
# Zuordnung aus Spalte 4 von tools/budget.txt (S/P/N/D).
set -e
cd "$(dirname "$0")/.."
O=out/set; T=obj/set.tmp
python3 -c "import shutil,os; shutil.rmtree('$T', True); os.makedirs('$T'); os.makedirs('$O', exist_ok=True)"
K=src/disk/fdkern/KERNEL.SYS; [ -f $K ] || { echo "mkset: $K fehlt (FreeDOS 2046 8086/FAT16, siehe HERKUNFT.TXT)"; exit 1; }
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
printf 'INSTALL=A:\\RD2\\RDKEYB.COM DE\r\nINSTALL=A:\\RD2\\RDMAUS.COM\r\nSHELL=A:\\RD2\\RD2.EXE /P A:\\RD2\\SHELL.EXE\r\n' > $T/CONFIG.SYS
txt src/disk/fdsys/COPYING COPYING; txt docs/LIZENZ.TXT LIZENZ.TXT; txt docs/QUELLEN.TXT QUELLEN.TXT
mcopy -i $O/RD2_1.IMG $T/CONFIG.SYS $T/COPYING $T/LIZENZ.TXT $T/QUELLEN.TXT ::/
mmd -i $O/RD2_1.IMG ::/RD2; kopie RD2_1.IMG $(liste S)
# Disketten 2 bis 4
neu RD2_2.IMG "RD2 PROG"; mmd -i $O/RD2_2.IMG ::/RD2; kopie RD2_2.IMG $(liste P); mcopy -i $O/RD2_2.IMG $T/COPYING $T/QUELLEN.TXT ::/
neu RD2_3.IMG "RD2 NETZ"; mmd -i $O/RD2_3.IMG ::/RD2; kopie RD2_3.IMG $(liste N); mcopy -i $O/RD2_3.IMG $T/COPYING $T/QUELLEN.TXT ::/
neu RD2_4.IMG "RD2 EXTRA"; mmd -i $O/RD2_4.IMG ::/SDK
mcopy -i $O/RD2_4.IMG sdk/RD2.H sdk/RD2API.H sdk/RD2DRV.H sdk/RD2ORD.H sdk/RD2START.ASM sdk/RD2MOD.ASM out/RD2.LIB out/RDRC.EXE ::/SDK/
mcopy -i $O/RD2_4.IMG docs/SDK.TXT docs/SDK_EN.TXT ::/SDK/; mmd -i $O/RD2_4.IMG ::/SDK/BEISPIEL; mcopy -i $O/RD2_4.IMG apps/hallo/HALLO.C apps/hallo/HALLO.RC sdk/SWAPSAFE.C apps/test/GESAMT.C apps/test/VOLLTEST.C apps/test/VESATEST.C apps/test/VOLLMAUS.C apps/test/TAKTTEST.C apps/test/MIXTEST.C apps/test/PRNTEST.C ::/SDK/BEISPIEL/   # PLAN_API 6: Beispiele zu Vollbild, VESA, Maus, Takt, Mischer, Drucken
python3 -c "import zipfile,sys; z=zipfile.ZipFile('$T/PASCAL.ZIP','w',zipfile.ZIP_DEFLATED,compresslevel=9); [z.write(f, f.split('/')[-1].upper()) for f in sys.argv[1:]]" out/RD2.PAS sdk/RD2PAS.ASM apps/pascal/PHALLO.PAS apps/pascal/PTEST.PAS docs/PASCAL.TXT
mcopy -i $O/RD2_4.IMG $T/PASCAL.ZIP ::/SDK/   # Pascal-Teil gepackt (Diskette 4 voll, 24.11.2026): RDZIP entpackt ihn
mcopy -i $O/RD2_4.IMG $T/COPYING $T/LIZENZ.TXT $T/QUELLEN.TXT ::/
# Diskette 5 (1,44 MB, ab 386): RD2/32 mit SDK (PLAN_32BIT Schritt 4)
rm -f $O/RD2_5.IMG; mformat -C -f 1440 -v "RD2 32" -i $O/RD2_5.IMG ::; mmd -i $O/RD2_5.IMG ::/RD2; kopie RD2_5.IMG $(liste Z | grep -v '\.OBJ$')
mmd -i $O/RD2_5.IMG ::/SDK32; mcopy -i $O/RD2_5.IMG sdk/RD2_32.H sdk/RD32C0.ASM sdk/RD32LIB.C sdk/RD32STUB.C out/RD32C0.OBJ out/RD32C0V.OBJ out/RD32LIB.OBJ out/RD32API.OBJ out/RD32STUB.OBJ out/RD3D.LIB sdk/RD3D.H sdk/RD32INT.H apps/test32/VOLL32.C apps/test32/VOLL32.RC apps/test32/FEN32.C apps/test32/FEN32.RC apps/test32/WUERFEL.C apps/test32/WUERFEL.RC apps/test32/SCHUTZ32.C apps/test32/SCHUTZ32.RC out/RD32C.LIB sdk/RD32C.C apps/test32/TESTC32.C apps/test32/TESTC32.RC out/RD32PRG.OBJ sdk/RD32PRG.C out/RD32M0.OBJ sdk/RD32M0.ASM tools/rd32mod.py apps/test32/TESTM32.C apps/test32/TESTM32.LNK apps/test32/TESTM32.EXP sdk/MKM32.BAT tools/RD32MOD.C out/RD32MOD.EXE apps/test32/TESTMOD.C apps/test32/TESTMOD.RC apps/test32/VESA32.C apps/test/GESAMT.C ::/SDK32/ && mmd -i $O/RD2_5.IMG ::/SDK32/RD32C && mcopy -i $O/RD2_5.IMG sdk/rd32c/*.h ::/SDK32/RD32C/
mcopy -i $O/RD2_5.IMG $T/COPYING $T/QUELLEN.TXT ::/
for i in 1 2 3 4 5; do echo "RD2_$i.IMG: $(mdir -i $O/RD2_$i.IMG -/ :: | tail -1)"; done
python3 -c "import shutil; shutil.rmtree('$T', True)"
