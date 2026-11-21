#!/bin/bash
# RD3D (PLAN_32BIT Schritt 5a), QEMU 486 mit CWSDPMI -p: WUERFEL im Vollbild 13h, VESA linear 640x480 und im Fenster
# (VESA- und VGA16-Schreibtisch). Je Lauf: Selbsttest (Z-Puffer, Alpha-Raster, Nebel), Bild pruefen, Esc, Speicher.
cd "$(dirname "$0")/.."
IMG=obj/rd3d.img; SER=obj/rd3d_com1.txt; SOCK=/tmp/qmon_rd3d.sock; F=0
Q="python3 test/qemu/qmon.py"; export QMON=$SOCK
pruef() { local t="$1"; if eval "$2"; then echo "ok     $t"; else echo "FEHLER $t"; F=1; fi; }
waitn() { for i in $(seq ${3:-150}); do [ "$(tr -d '\r' < $SER | grep -c "$1")" -ge "${2:-1}" ] && return 0; sleep 0.4; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
bild() { python3 -c "
from PIL import Image
a = Image.open('$1').convert('RGB'); print(a.size[0], a.size[1], len(a.getcolors(1 << 20)))"; }
lauf() {   # $1 Name  $2 Befehlszeile  $3 Grafik (leer = /VGA)  $4 Mindestfarben  $5 Breite  $6 Hoehe
  local opt="/VGA"; [ -n "$3" ] && opt=""
  printf '@echo off\r\nA:\\RD2\\CWSDPMI.EXE -p\r\nA:\\RD2\\RD2.EXE %s A:\\RD2\\WUERFEL.EXE %s\r\nA:\\FDAPM.COM POWEROFF\r\n' "$opt" "$2" > obj/ae_rd3d.txt
  FMT=1440 bash tools/mkdisk.sh $IMG obj/ae_rd3d.txt >/dev/null 2>&1
  mcopy -o -i $IMG out/WUERFEL.EXE out/RD32.MOD out/CWSDPMI.EXE ::/RD2/
  [ -n "$3" ] && { printf '[System]\r\nGrafik=%s\r\n' $3 > obj/RD2.INI; mcopy -o -i $IMG out/$3.DRV obj/RD2.INI ::/RD2/; }
  rm -f $SOCK $SER obj/r3d_$1.ppm; touch $SER
  timeout 200 qemu-system-i386 -cpu 486 -m 16 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
    -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  waitn "WUERFEL: gezeichnet" 1 400 && sleep 1 && $Q "screendump obj/r3d_$1.ppm" sleep0.5 "sendkey esc" >/dev/null 2>&1
  waitn "RD2: Speicher vollstaendig zurueck" 1 100; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
  L=$(tr -d '\r' < $SER); B=$([ -f obj/r3d_$1.ppm ] && bild obj/r3d_$1.ppm)
  echo "       $1: Bild $B, $(echo "$L" | grep -a '20 Bilder in ms' | sed 's/.*ms //')ms fuer 20 Bilder"
  pruef "$1: Treiber Software, Selbsttest Z-Puffer, Alpha, Nebel" "echo \"\$L\" | grep -q 'WUERFEL: Treiber Software' && echo \"\$L\" | grep -q 'WUERFEL: Z-Test ok' && echo \"\$L\" | grep -q 'WUERFEL: Alpha-Raster ok' && echo \"\$L\" | grep -q 'WUERFEL: Nebel ok'"
  pruef "$1: Wuerfel ${5}x$6 mit mindestens $4 Farben ($B)" "set -- $B; [ \"\$1\" = $5 ] && [ \"\$2\" = $6 ] && [ \"\${3:-0}\" -ge $4 ]"
  pruef "$1: Esc beendet, Speicher vollstaendig zurueck" "echo \"\$L\" | grep -q 'WUERFEL: Ende' && echo \"\$L\" | grep -q 'RD2: Speicher vollstaendig zurueck'"
}
lauf voll13 "" "" 12 640 400
lauf lfb "L" "" 12 640 480
lauf fenster_vesa "F" VESA 8 640 480   # Farbzahl haengt vom Drehwinkel beim Foto ab (9 bis 13)
lauf fenster_vga16 "F" VGA16 5 640 480
[ $F = 0 ] && echo "RD3D: alles ok" || { echo "RD3D: FEHLER"; echo "$L" | grep -a "WUERFEL\|RD32\|Fehler" | tail -15; }
exit $F
