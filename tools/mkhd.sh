#!/bin/bash
# mkhd.sh IMAGE  - leere 10-MB-Festplatte (20 Zyl., 16 Koepfe, 63 Sekt.), eine FAT16-Partition ab Sektor 63.
# Zugriff danach mit mtools: -i IMAGE@@32256
set -e
IMG=$1; N=$((20*16*63 - 63)); P=$IMG.part
rm -f $P; mformat -C -i $P -T $N -h 16 -s 63 -H 63 -v RD2HD ::
python3 - $IMG $P $N <<'PY'
import sys, struct
img, part, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
mbr = bytearray(512)
def chs(l): c, r = divmod(l, 16*63); h, s = divmod(r, 63); return bytes([h, ((c >> 2) & 0xC0) | (s + 1), c & 0xFF])
mbr[446:462] = bytes([0x80]) + chs(63) + bytes([0x06]) + chs(63 + n - 1) + struct.pack('<II', 63, n)
mbr[510:512] = b'\x55\xaa'
open(img, 'wb').write(bytes(mbr) + bytes(62*512) + open(part, 'rb').read())
PY
rm -f $P
