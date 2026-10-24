#!/bin/bash
# RDFDISK in QEMU, nur Tastatur. Start von der RD2-Diskette A:, Festplatte 16 Koepfe, 63 Sektoren.
# Faelle: leer   (20 MB ohne Partitionstabelle: Anlegen mit Vorgabe = ganze Platte, Schreiben -> C: primaer, aktiv)
#         zwei   (100 MB leer: C: 40 MB primaer, Rest als logisches Laufwerk D: in einer erweiterten)
#         weg    (20 MB mit FAT16-Partition: loeschen, Schreiben -> Tabelle leer)
#         zurueck (wie weg, dann Schliessen und Verwerfen -> Platte unveraendert)
# Danach prueft Python die Tabellen (MBR, EMBR). FAELLE="leer weg" waehlt aus. Bilder obj/fdisk{1..3}_FALL.ppm.
cd "$(dirname "$0")/.."
SOCK=/tmp/qmon_fdisk.sock
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_fdisk.py
Q() { python3 obj/qmon_fdisk.py "$@"; }
waitn() { for i in $(seq ${3:-600}); do [ "$(grep -c "$1" $2 2>/dev/null || true)" -ge "${4:-1}" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\RDFDISK.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_fdisk.txt
NUR="$(awk '$4=="S" && $1!="RDSYS.EXE"{print $1}' tools/budget.txt | tr '\n' ' ') RDFDISK.EXE RDDISK.MOD" bash tools/mkdisk.sh obj/fdisk_a.img obj/ae_fdisk.txt >/dev/null 2>&1 || { echo "FEHLER: Diskette"; exit 1; }
ok=0; fehler=0
for FALL in ${FAELLE:-leer zwei weg zurueck}; do
  ZYL=40; [ $FALL = zwei ] && ZYL=203
  H=obj/fdisk_$FALL.img; SER=obj/fdisk_${FALL}_com1.txt
  python3 - $H $ZYL $FALL <<'PY'
import sys, struct
f, z, fall = sys.argv[1], int(sys.argv[2]), sys.argv[3]
d = bytearray(z*16*63*512)
if fall in ('weg', 'zurueck'):
    n = z*16*63 - 63
    d[446:462] = bytes([0x80, 1, 1, 0, 4, 15, 63 | (((z-1) >> 8) << 6), (z-1) & 255]) + struct.pack('<II', 63, n)
    d[510] = 0x55; d[511] = 0xAA
open(f, 'wb').write(d)
PY
  md5sum $H > obj/fdisk_$FALL.md5
  rm -f $SER $SOCK obj/fdisk?_$FALL.ppm
  timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=obj/fdisk_a.img,format=raw,if=floppy,index=0 \
    -drive file=$H,format=raw,if=ide,index=0 -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  if waitn "RDFDISK: Liste bereit" $SER; then
    sleep 3; Q "screendump obj/fdisk1_$FALL.ppm" sleep0.3
    case $FALL in
      leer) Q "sendkey alt-a"; waitn "RDFDISK: Liste bereit" $SER 100 1 && sleep 2 && Q "sendkey ret"; waitn "RDFDISK: Liste bereit" $SER 300 2 ;;
      zwei) Q "sendkey alt-a"; sleep 2.5; Q "sendkey end" "sendkey backspace" "sendkey backspace" "sendkey backspace" "sendkey backspace" sleep0.2 "sendkey 4" "sendkey 0" sleep0.3 "sendkey ret"; waitn "RDFDISK: Liste bereit" $SER 300 2
            sleep 2; Q "sendkey alt-a"; sleep 2.5; Q "sendkey ret"; waitn "RDFDISK: Liste bereit" $SER 300 3 ;;
      weg|zurueck) Q "sendkey alt-l"; waitn "RDFDISK: Meldung 11" $SER 100 && sleep 2 && Q "sendkey ret"; waitn "RDFDISK: Liste bereit" $SER 300 2 ;;
    esac
    sleep 2.5; Q "screendump obj/fdisk2_$FALL.ppm" sleep0.3
    if [ $FALL = zurueck ]; then
      Q "sendkey esc"; waitn "RDFDISK: Meldung 15" $SER 100 && sleep 2 && Q "sendkey ret"
    else
      Q "sendkey alt-s"; waitn "RDFDISK: Meldung 13" $SER 100 && sleep 2 && Q "sendkey ret"
      waitn "RDFDISK: Meldung 14" $SER 300 && sleep 2 && Q "screendump obj/fdisk3_$FALL.ppm" sleep0.3 "sendkey ret"
    fi
  else echo "FEHLER: RDFDISK kommt nicht ($FALL)"; fi
  wait $QP
  echo "--- $FALL: Protokoll"
  tr -d '\r' < $SER | grep -vE "^A:.RD2.|^RD: (Speicher|Modul)" | sed -n '/RDFDISK: bereit/,$p' | head -30
  python3 - $H $FALL <<'PY'
import sys, struct
f, fall = sys.argv[1], sys.argv[2]
d = open(f, 'rb').read()
def tab(off):
    s = d[off:off+512]; e = []
    for i in range(4):
        b = s[446+16*i:462+16*i]; t = b[4]
        if t: e.append((b[0], t) + struct.unpack('<II', b[8:16]))
    return e, s[510:512] == b'\x55\xaa'
p, sig = tab(0)
print("MBR", sig, [(hex(a), hex(t), r, n, n*512//1048576) for a, t, r, n in p])
ok = True
if fall == 'leer':
    ok = sig and len(p) == 1 and p[0][0] == 0x80 and p[0][1] in (4, 6) and p[0][2] >= 63 and p[0][2] + p[0][3] <= len(d)//512 and d[0] != 0
elif fall == 'zwei':
    pri = [e for e in p if e[1] in (4, 6)]; ext = [e for e in p if e[1] == 5]
    ok = sig and len(pri) == 1 and pri[0][0] == 0x80 and 39 <= pri[0][3]*512//1048576 <= 41 and len(ext) == 1
    if ok:
        l, s2 = tab(ext[0][2]*512); print("EMBR", s2, [(hex(t), r, n, n*512//1048576) for a, t, r, n in l])
        ok = s2 and len(l) >= 1 and l[0][1] in (4, 6) and ext[0][2] + l[0][2] + l[0][3] <= ext[0][2] + ext[0][3] and l[0][3]*512//1048576 >= 55
elif fall == 'weg':
    ok = sig and len(p) == 0
sys.exit(0 if ok else 1)
PY
  r=$?
  [ $FALL = zurueck ] && { md5sum -c --quiet obj/fdisk_$FALL.md5 && ! grep -q "RDFDISK: geschrieben" $SER; r=$?; }
  grep -q "Speicher vollstaendig zurueck" $SER || r=1
  [ $r = 0 ] && { echo "OK $FALL"; ok=$((ok+1)); } || { echo "FEHLER $FALL"; fehler=$((fehler+1)); }
done
echo "fdisk.sh: $ok ok, $fehler Fehler"
