#!/bin/bash
# Punkt 9 Farbe: VGA16.DRV (Modus 12h), EGA.DRV (Modus 10h) und VESA.DRV (101h, 256 Farben) in QEMU, Grafik= aus RD2.INI.
# Bilder obj/farbe{1..5}_{vga16,ega}.ppm: 1 Schreibtisch, 2 Ordner A:\RD2 (Symbole), 3 Menue RD offen,
# 4 Menue wieder zu (muss gleich 2 sein: Hintergrund im Geraeteformat), 5 Info. Vergleich mit test/ref.
cd "$(dirname "$0")/.."
IMG=obj/farbe.img; SER=obj/farbe_com1.txt; SOCK=/tmp/qmon_farbe.sock; F=0
cat > obj/ae_farbe.txt <<'AE'
@echo off
copy A:\RD2\INI16.TXT A:\RD2\RD2.INI > NUL
A:\RD2\RD.EXE /NOMAUS A:\RD2\SHELL.EXE
if exist A:\RD2\RD2.STA del A:\RD2\RD2.STA
copy A:\RD2\INIEGA.TXT A:\RD2\RD2.INI > NUL
A:\RD2\RD.EXE /NOMAUS A:\RD2\SHELL.EXE
if exist A:\RD2\RD2.STA del A:\RD2\RD2.STA
copy A:\RD2\INIVESA.TXT A:\RD2\RD2.INI > NUL
A:\RD2\RD.EXE /NOMAUS A:\RD2\SHELL.EXE
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_farbe.txt
mcopy -i $IMG out/VGA16.DRV out/EGA.DRV out/VESA.DRV ::/RD2/
printf '[System]\r\nGrafik=VGA16\r\n' > obj/ini16.txt; printf '[System]\r\nGrafik=EGA\r\n' > obj/iniega.txt; printf '[System]\r\nGrafik=VESA\r\n' > obj/inivesa.txt
mcopy -i $IMG obj/ini16.txt ::/RD2/INI16.TXT; mcopy -i $IMG obj/iniega.txt ::/RD2/INIEGA.TXT; mcopy -i $IMG obj/inivesa.txt ::/RD2/INIVESA.TXT
rm -f $SER $SOCK obj/farbe?_*.ppm
timeout 420 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_farbe.py
Q() { timeout 30 python3 obj/qmon_farbe.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(tr -d '\r' < $SER 2>/dev/null | grep -c "$1" || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
for n in 1 2 3; do
  K=$(echo x vga16 ega vesa | cut -d" " -f$((n + 1)))
  waitn "SHELL: bereit" $n || { echo "FEHLER: Shell kommt nicht ($K)"; F=1; break; }; sleep 3
  Q "screendump obj/farbe1_$K.ppm" sleep0.3 "sendkey ret"
  waitn 'SHELL: Ordner A:\\ ' $n || { echo "FEHLER: Ordner A: kommt nicht ($K)"; F=1; break; }; sleep 3
  Q "sendkey ret"
  waitn 'SHELL: Ordner A:\\RD2\\ ' $n || { echo "FEHLER: Ordner RD2 kommt nicht ($K)"; F=1; break; }; sleep 4
  Q "screendump obj/farbe2_$K.ppm" sleep0.3 "sendkey alt-spc" sleep1.5 "screendump obj/farbe3_$K.ppm" sleep0.3 "sendkey esc" sleep1.5 \
    "screendump obj/farbe4_$K.ppm" sleep0.3 "sendkey alt-spc" sleep1 "sendkey i"; sleep 3
  Q "screendump obj/farbe5_$K.ppm" sleep0.3 "sendkey ret" sleep1 "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
done
wait $QP 2>/dev/null
grep -a "RD: Grafik\|VGA16\|EGA.DRV\|Speicher vollst\|SPEICHERLECK" $SER | tr -d '\r' | head
for K in vga16 ega vesa; do
  [ -f obj/farbe2_$K.ppm ] && [ -f obj/farbe4_$K.ppm ] && { python3 -c "
from PIL import Image, ImageChops, ImageDraw; import sys
a, b = [Image.open('obj/farbe%d_$K.ppm' % i).convert('RGB') for i in (2, 4)]
for x in (a, b): ImageDraw.Draw(x).rectangle((x.size[0] - 64, 0, x.size[0], x.size[1] // 20), fill=0)   # Uhr
sys.exit(1 if ImageChops.difference(a, b).getbbox() else 0)" && echo "ok     $K: Menue zu, Hintergrund wieder wie vorher" || { echo "FEHLER $K: Hintergrund nach dem Menue anders"; F=1; }; }
  for i in 1 2 3 5; do f=obj/farbe${i}_$K.ppm; [ -f $f ] || { echo "FEHLER kein Bild $f"; F=1; continue; }
    python3 -c "
from PIL import Image; im=Image.open('$f').convert('RGB'); print('       $f', im.size, len(im.getcolors(1<<20)), 'Farben')"
    if [ -f test/ref/farbe${i}_$K.png ]; then python3 -c "
from PIL import Image, ImageChops, ImageDraw; import sys
a, b = Image.open('$f').convert('RGB'), Image.open('test/ref/farbe${i}_$K.png').convert('RGB')
for x in (a, b): ImageDraw.Draw(x).rectangle((x.size[0] - 64, 0, x.size[0], x.size[1] // 20), fill=0)   # Uhr
d = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v else 0).histogram()[255] if a.size == b.size else -1
print(('ok     $f wie Referenz (Farben)') if d == 0 else 'FEHLER $f: %d Pixel abweichend (Farben)' % d); sys.exit(d != 0)" || F=1
    else python3 -c "from PIL import Image; Image.open('$f').save('test/ref/farbe${i}_$K.png')"; echo "NEU    test/ref/farbe${i}_$K.png"; fi
  done
done
exit $F
