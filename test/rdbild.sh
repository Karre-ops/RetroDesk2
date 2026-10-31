#!/bin/bash
# RDBILD: JPEG 640x480 (VESA 256 Farben, VGA16), grosses JPEG 2016x1512 mit Exif-Lage 6 (1/4 bzw. 1/8, gedreht),
# GIF auf S/W-VGA, PCX auf CGA, BMP mit Bild ab (naechstes Bild im Ordner). Bilder obj/bild?_*.png.
cd "$(dirname "$0")/.."
IMG=obj/rdbild.img; SER=obj/rdbild_com1.txt; SOCK=/tmp/qmon_rdbild.sock
pkill -f qmon_rdbild 2>/dev/null; pkill -f "file=$IMG" 2>/dev/null
cat > obj/ae_rdbild.txt <<'AE'
@echo off
copy A:\RD2\INIVESA.TXT A:\RD2\RD2.INI > NUL
A:\RD2\RD.EXE /NOMAUS A:\RD2\RDBILD.EXE T A:\BILDER\FOTO.JPG
A:\RD2\RD.EXE /NOMAUS A:\RD2\RDBILD.EXE T A:\BILDER\GROSS.JPG
copy A:\RD2\INI16.TXT A:\RD2\RD2.INI > NUL
A:\RD2\RD.EXE /NOMAUS A:\RD2\RDBILD.EXE T A:\BILDER\FOTO.JPG
del A:\RD2\RD2.INI
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDBILD.EXE T A:\BILDER\BUNT.GIF
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\RDBILD.EXE T A:\BILDER\BUNT.PCX
copy A:\RD2\INIVESA.TXT A:\RD2\RD2.INI > NUL
A:\RD2\RD.EXE /NOMAUS A:\RD2\RDBILD.EXE T A:\BILDER\BUNT.BMP
A:\FDAPM.COM POWEROFF
AE
FMT=1440 bash tools/mkdisk.sh $IMG obj/ae_rdbild.txt
mcopy -i $IMG out/VGA16.DRV out/VESA.DRV ::/RD2/
printf '[System]\r\nGrafik=VGA16\r\n' > obj/ini16.txt; printf '[System]\r\nGrafik=VESA\r\n' > obj/inivesa.txt
mcopy -i $IMG obj/ini16.txt ::/RD2/INI16.TXT; mcopy -i $IMG obj/inivesa.txt ::/RD2/INIVESA.TXT
D=obj/rdbild_d; mkdir -p $D
python3 - $D <<'PY'
import sys, math, random
from PIL import Image, ImageDraw
d = sys.argv[1]; random.seed(4)
def foto(W, H):
    im = Image.new('RGB', (W, H)); px = im.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = (int(90 + 120 * x / W), int(60 + 150 * y / H), int(128 + 100 * math.sin(x / (W / 9) + y / (H / 7))))
    g = ImageDraw.Draw(im)
    g.ellipse((W * .1, H * .15, W * .45, H * .7), fill=(240, 200, 30)); g.rectangle((W * .55, H * .2, W * .9, H * .55), fill=(30, 60, 200))
    g.polygon([(W * .2, H * .95), (W * .5, H * .6), (W * .8, H * .95)], fill=(200, 40, 40))
    g.text((W * .05, H * .03), "RetroDesk 2 - RDBILD", fill=(255, 255, 255))
    return im
f = foto(640, 480); f.save(d + '/FOTO.JPG', quality=85); f.save(d + '/foto_ref.png')
g = foto(2016, 1512); ex = Image.Exif(); ex[0x0112] = 6; g.save(d + '/GROSS.JPG', quality=80, exif=ex.tobytes())
b = foto(320, 200)
b.quantize(256).save(d + '/BUNT.GIF', interlace=True); b.quantize(256).save(d + '/BUNT.PCX'); b.save(d + '/BUNT.BMP')
PY
mmd -i $IMG ::/BILDER; mcopy -i $IMG $D/FOTO.JPG $D/GROSS.JPG $D/BUNT.GIF $D/BUNT.PCX $D/BUNT.BMP ::/BILDER/
rm -f $SER $SOCK obj/bild?_*.ppm
timeout 145 qemu-system-i386 -m 2 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_rdbild.py
Q() { python3 obj/qmon_rdbild.py "$@"; }
waitn() { for i in $(seq 500); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
for n in 1 2 3 4 5; do
  K=$(case $n in 1|2) echo vesa;; 3) echo vga16;; 4) echo vga;; 5) echo cga;; esac)
  if waitn "RDBILD: fertig" $n; then sleep 1.5; Q "screendump obj/bild${n}_$K.ppm" sleep0.3 "sendkey esc"; else echo "FEHLER: Bild $n wird nicht fertig"; Q "sendkey esc"; fi
done
if waitn "RDBILD: fertig" 6; then sleep 1; Q "sendkey pgdn"; fi
if waitn "RDBILD: fertig" 7; then sleep 1.5; Q "screendump obj/bild6_vesa.ppm" sleep0.3 "sendkey esc"; else echo "FEHLER: Bild ab laedt kein naechstes Bild"; Q "sendkey esc"; fi
wait $QP
tr -d '\r' < $SER | grep -E "RDBILD|RD: (Fehler|SPEICHER)" | grep -v bereit
c=$(tr -d '\r' < $SER | grep -c "RDBILD: fertig"); [ "$c" = 7 ] && echo "ok     7 Bilder fertig" || echo "FEHLER nur $c Bilder fertig"
tr -d '\r' < $SER | grep -q "RDBILD: Lesen 2016x1512 skal [48]" && echo "ok     grosses JPEG verkleinert entpackt" || echo "FEHLER grosses JPEG nicht verkleinert"
tr -d '\r' < $SER | grep -q "fertig [0-9]*x[0-9]* Lage 6" && echo "ok     Exif-Lage 6 (gedreht)" || echo "FEHLER Exif-Lage"
for f in obj/bild?_*.ppm; do python3 -c "from PIL import Image; Image.open('$f').save('${f%.ppm}.png')"; done 2>/dev/null
exit 0
