#!/bin/bash
# mkverteil.sh - alle Verteilformen von RetroDesk 2 in out/verteilung (23.11.2026):
#   1_XT_360KB    RD2_1..4.IMG   360-KB-Satz (8088 bis 286, 16 Bit), wie tools/mkset.sh
#   2_386_144MB   RD2_386A.IMG   1,44 MB bootfaehig: System, Programme, Treiber, RD2/32 (ab 386)
#                 RD2_386B.IMG   1,44 MB: Netz, SDK, SDK32
#   3_FESTPLATTE  RD2_HDD.ZIP    Ordner RD2 zum Kopieren auf den Ziel-PC, Installation mit RD2INST (tools/mkftp.sh)
#   4_CD_USB      RD2_CD.ISO     CD, bootet einen 2,88-MB-Diskettenabzug (El Torito, alles drauf) und enthaelt den
#                                Ordner RD2 wie das ZIP; RD2_USB.IMG  USB-Stick als Festplatte (FAT16, MBR, 64 MB)
# Erwartet out/set/RD2_1..5.IMG (tools/mkset.sh) und obj/sys/fat16com.bin (build.sh). Python-Paket pycdlib fuer das ISO.
set -e
cd "$(dirname "$0")/.."
V=out/verteilung; T=obj/vt
python3 -c "import shutil,os; [shutil.rmtree(p, True) for p in ('$V', '$T')]; [os.makedirs(p) for p in ('$T', '$V/1_XT_360KB', '$V/2_386_144MB', '$V/3_FESTPLATTE', '$V/4_CD_USB')]"
for i in 1 2 3 4 5; do mkdir $T/d$i; mcopy -s -n -i out/set/RD2_$i.IMG "::/*" $T/d$i/; done
nasm -f bin -o $T/mbr.bin tools/mbr.asm
cfg() { printf 'INSTALL=%s:\\RD2\\RDKEYB.COM DE\r\nINSTALL=%s:\\RD2\\RDMAUS.COM\r\nINSTALL=%s:\\RD2\\CWSDPMI.EXE -p -s-\r\nSHELL=%s:\\RD2\\RD2.EXE /P %s:\\RD2\\SHELL.EXE\r\n' $1 $1 $1 $1 $1 > $T/CONFIG.$1; }
cfg A; cfg C
K32=src/disk/fdkern/F32/KERNEL.SYS   # FreeDOS 2046 8086/FAT32: Startkern der 386-Medien (laeuft auch auf 8088/286), XT-Satz bleibt FAT16
diskette() {   # $1 Abbild  $2 KB  $3 Name  $4 Boot (1/0)  danach Quellordner: RD2-Teile (dX/RD2) und Unterordner (dX/SDK ...)
  local img=$1 kb=$2 lbl="$3" boot=$4; shift 4
  rm -f $img; mformat -C -f $kb -v "$lbl" -i $img ::
  if [ $boot = 1 ]; then
    python3 - test/qemu/boot.img $img <<'PY'
import sys
s = open(sys.argv[1], 'rb').read(512); f = open(sys.argv[2], 'r+b'); d = bytearray(f.read(512))
d[0:3] = s[0:3]; d[0x3E:0x200] = s[0x3E:0x200]   # FreeDOS-Bootsektor (FAT12, laedt KERNEL.SYS), BPB bleibt
f.seek(0); f.write(d)
PY
    mcopy -i $img $K32 ::/KERNEL.SYS; mcopy -i $img $T/CONFIG.A ::/CONFIG.SYS; mcopy -i $img $T/d1/LIZENZ.TXT ::/   # Startkern FAT32 (PLAN_FS 1)
  fi
  mcopy -i $img $T/d1/COPYING $T/d1/QUELLEN.TXT ::/
  mmd -i $img ::/RD2
  [ $boot = 1 ] && { mmd -i $img ::/RD2/K16; mcopy -i $img $T/d1/KERNEL.SYS ::/RD2/K16/KERNEL.SYS; }   # FAT16-Kern fuer die Festplatte (Vorgabe)
  for q in "$@"; do
    case $q in */RD2) mcopy -o -i $img $q/* ::/RD2/ ;; *) mcopy -s -o -i $img $q ::/ ;; esac
  done
}
# 1: 360 KB
cp out/set/RD2_1.IMG out/set/RD2_2.IMG out/set/RD2_3.IMG out/set/RD2_4.IMG $V/1_XT_360KB/
# 2: 1,44 MB (ab 386)
diskette $V/2_386_144MB/RD2_386A.IMG 1440 RD2SYSTEM 1 $T/d1/RD2 $T/d2/RD2 $T/d5/RD2
diskette $V/2_386_144MB/RD2_386B.IMG 1440 "RD2 386 2" 0 $T/d3/RD2 $T/d4/SDK $T/d5/SDK32
# 3: Festplatte (ZIP)
R=$T/ftp bash tools/mkftp.sh >/dev/null; cp $T/ftp/RD2_HDD.ZIP $V/3_FESTPLATTE/
# 4: CD (2,88-MB-Abzug + Ordner RD2) und USB (Festplattenabbild)
diskette $T/RD2_288.IMG 2880 RD2SYSTEM 1 $T/d1/RD2 $T/d2/RD2 $T/d3/RD2 $T/d5/RD2 $T/d4/SDK $T/d5/SDK32
python3 - $T/RD2_288.IMG obj/hd $V/4_CD_USB/RD2_CD.ISO <<'PY'
import sys, os, pycdlib
boot, baum, aus = sys.argv[1:4]
iso = pycdlib.PyCdlib(); iso.new(interchange_level=1, vol_ident='RD2')
iso.add_file(boot, '/RD2_288.IMG;1')
iso.add_eltorito('/RD2_288.IMG;1', '/BOOT.CAT;1', media_name='floppy')
for w, ds, fs in sorted(os.walk(baum)):
    ds.sort(); rel = os.path.relpath(w, baum)
    if rel != '.': iso.add_directory('/' + rel.upper().replace(os.sep, '/'))
    for f in sorted(fs): iso.add_file(os.path.join(w, f), '/' + (f if rel == '.' else rel.upper().replace(os.sep, '/') + '/' + f).upper() + ';1')
iso.write(aus); iso.close()
PY
H=$V/4_CD_USB/RD2_USB.IMG; P=$H@@32256; ZYL=130; N=$((ZYL*16*63 - 63))
rm -f $T/usb.part; mformat -C -i $T/usb.part -T $N -h 16 -s 63 -H 63 -v RD2SYSTEM ::
python3 - $H $T/usb.part $N $T/mbr.bin obj/sys/fat16com.bin <<'PY'
import sys, struct
img, part, n, mbrf, vbrf = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5]
def chs(l): c, r = divmod(l, 16*63); h, s = divmod(r, 63); return bytes([h, ((c >> 2) & 0xC0) | (s + 1), c & 0xFF])
mbr = bytearray(512); mbr[0:446] = open(mbrf, 'rb').read()[:446]
mbr[446:462] = bytes([0x80]) + chs(63) + bytes([0x06]) + chs(63 + n - 1) + struct.pack('<II', 63, n)
mbr[510:512] = b'\x55\xaa'
p = bytearray(open(part, 'rb').read()); v = open(vbrf, 'rb').read()
if p[0x36:0x3B] != b'FAT16': sys.exit('mkverteil: USB-Partition ist nicht FAT16')
p[0:3] = v[0:3]; p[0x3E:0x200] = v[0x3E:0x200]; p[0x24] = 0x80   # FreeDOS-Startsektor FAT16, Platte 80h
open(img, 'wb').write(bytes(mbr) + bytes(62*512) + bytes(p))
PY
rm -f $T/usb.part
mcopy -i $P $K32 ::/KERNEL.SYS; mcopy -i $P $T/CONFIG.C ::/CONFIG.SYS; mcopy -i $P $T/d1/COPYING $T/d1/LIZENZ.TXT $T/d1/QUELLEN.TXT ::/
mmd -i $P ::/RD2; mmd -i $P ::/RD2/K16; mcopy -i $P $T/d1/KERNEL.SYS ::/RD2/K16/KERNEL.SYS; for d in 1 2 3 5; do mcopy -o -i $P $T/d$d/RD2/* ::/RD2/; done; mcopy -s -i $P $T/d4/SDK $T/d5/SDK32 ::/
sed 's/$/\r/' tools/dist/VERTEIL.TXT > $V/LIESMICH.TXT
(cd $V && find . -type f ! -name SHA256.TXT ! -name LIESMICH.TXT | sort | sed 's|^\./||' | xargs sha256sum > SHA256.TXT)
for f in $(cd $V && find . -type f | sort); do printf '%-34s %9d\n' "${f#./}" $(stat -c %s $V/$f); done
for f in 2_386_144MB/RD2_386A.IMG 2_386_144MB/RD2_386B.IMG; do echo "$f: frei $(minfo -i $V/$f :: 2>/dev/null | true; mdir -i $V/$f :: | tail -1)"; done
