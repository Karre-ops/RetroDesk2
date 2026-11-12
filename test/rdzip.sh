#!/bin/bash
# RDZIP: 1 /X entpackt ein Python-ZIP (Deflate + gespeichert, Unterordner, langer Name -> 8.3), 2 /P prueft,
# 3 /A packt einen Ordner in ein neues Archiv (Python prueft CRC und Inhalt), 4 Liste ansehen (Bild), Esc.
cd "$(dirname "$0")/.."
IMG=obj/rdzip.img; SER=obj/rdzip_com1.txt; SOCK=/tmp/qmon_rdzip.sock
pkill -f qmon_rdzip 2>/dev/null; pkill -f "file=$IMG" 2>/dev/null
cat > obj/ae_rdzip.txt <<'AE'
@echo off
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\RDZIP.EXE T /X A:\TEST.ZIP A:\AUS
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\RDZIP.EXE T /P A:\TEST.ZIP
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\RDZIP.EXE T /A A:\NEU.ZIP A:\QUELLE
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\RDZIP.EXE T /N A:\QUELLE\ TEXT.TXT NULL.DAT
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\RDZIP.EXE T A:\NEU.ZIP
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_rdzip.txt
D=obj/rdzip_d
python3 -c "import shutil; shutil.rmtree('obj/rdzip_d', True)"
mkdir -p $D/QUELLE/UNTER
python3 - $D <<'PY'
import sys, os, zipfile, random
d = sys.argv[1]; random.seed(7)
txt = open('src/shell/SHELL.C', 'rb').read()[:40000]
z = zipfile.ZipFile(d + '/TEST.ZIP', 'w')
z.writestr(zipfile.ZipInfo('LIESMICH.TXT'), txt, zipfile.ZIP_DEFLATED)
z.writestr(zipfile.ZipInfo('ordner/Ein langer Name.text'), txt[:5000], zipfile.ZIP_DEFLATED)
z.writestr(zipfile.ZipInfo('ordner/ROH.BIN'), bytes(random.getrandbits(8) for _ in range(3000)), zipfile.ZIP_STORED)
z.writestr(zipfile.ZipInfo('LEER.DAT'), b'', zipfile.ZIP_DEFLATED)
z.close()
open(d + '/QUELLE/TEXT.TXT', 'wb').write(txt[:20000])
open(d + '/QUELLE/ZUFALL.BIN', 'wb').write(bytes(random.getrandbits(8) for _ in range(6000)))
open(d + '/QUELLE/NULL.DAT', 'wb').write(b'\0' * 30000)
open(d + '/QUELLE/UNTER/KLEIN.TXT', 'wb').write(b'Hallo RetroDesk\r\n')
PY
mcopy -i $IMG $D/TEST.ZIP ::/; mcopy -s -i $IMG $D/QUELLE ::/
rm -f $SER $SOCK obj/rdzip?_*.ppm
timeout 150 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_rdzip.py
Q() { python3 obj/qmon_rdzip.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
if waitn "RDZIP: Eintraege" 4; then sleep 2; Q "screendump obj/rdzip1_vga.ppm" sleep0.3 "sendkey esc"; else echo "FEHLER: Liste kommt nicht"; fi
waitn "RDZIP: Ende" 5 || echo "FEHLER: Esc schliesst die Liste nicht"
wait $QP
tr -d '\r' < $SER | grep -E "RDZIP|RD: (Fehler|SPEICHER)" | grep -v "bereit" | head -40
python3 -c "import shutil; shutil.rmtree('obj/rdzip_d/aus', True); shutil.rmtree('obj/rdzip_d/neu', True)"
mkdir -p $D/aus $D/neu
mcopy -s -n -i $IMG ::/AUS $D/aus/ 2>/dev/null; mcopy -n -i $IMG ::/NEU.ZIP ::/QUELLE/TEXT.ZIP $D/neu/ 2>/dev/null
python3 - $D <<'PY'
import sys, os, zipfile
d = sys.argv[1]; z = zipfile.ZipFile(d + '/TEST.ZIP')
def pr(t, ok): print(('ok     ' if ok else 'FEHLER ') + t)
want = {'LIESMICH.TXT': 'LIESMICH.TXT', 'ordner/Ein langer Name.text': 'ORDNER/EIN_LANG.TEX', 'ordner/ROH.BIN': 'ORDNER/ROH.BIN', 'LEER.DAT': 'LEER.DAT'}
for n, f in want.items():
    p = d + '/aus/AUS/' + f
    pr('entpackt ' + f, os.path.exists(p) and open(p, 'rb').read() == z.read(n))
try:
    n = zipfile.ZipFile(d + '/neu/NEU.ZIP'); bad = n.testzip(); names = sorted(n.namelist())
    pr('NEU.ZIP CRC (Python)', bad is None)
    pr('NEU.ZIP Namen ' + ' '.join(names), names == ['QUELLE/NULL.DAT', 'QUELLE/TEXT.TXT', 'QUELLE/UNTER/KLEIN.TXT', 'QUELLE/ZUFALL.BIN'])
    for x in names:
        pr('NEU.ZIP Inhalt ' + x, n.read(x) == open(d + '/' + x, 'rb').read())
    for i in n.infolist(): print('       %-24s %6d -> %6d  %s' % (i.filename, i.file_size, i.compress_size, 'deflate' if i.compress_type == 8 else 'gespeichert'))
except Exception as e: pr('NEU.ZIP lesbar: %s' % e, False)
try:
    n = zipfile.ZipFile(d + '/neu/TEXT.ZIP'); pr('/N (Shell: Auswahl packen) TEXT.ZIP ' + ' '.join(n.namelist()), n.testzip() is None and sorted(n.namelist()) == ['NULL.DAT', 'TEXT.TXT'] and n.read('TEXT.TXT') == open(d + '/QUELLE/TEXT.TXT', 'rb').read())
except Exception as e: pr('TEXT.ZIP lesbar: %s' % e, False)
PY
[ -f obj/rdzip1_vga.ppm ] && python3 -c "from PIL import Image; Image.open('obj/rdzip1_vga.ppm').save('obj/rdzip1_vga.png')"
exit 0
