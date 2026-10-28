#!/bin/bash
# Bildschirmschoner: RD2.INI [Schoner] Zeit=5 Art=0. Die Shell startet nach 5 s ohne Eingabe SCHONER.EXE S
# (Sternenfeld: schwarzer Schirm mit hellen Punkten), eine Taste beendet ihn und der Schreibtisch ist wieder da.
cd "$(dirname "$0")/.."
IMG=obj/schoner.img; SER=obj/schoner_com1.txt; SOCK=/tmp/qmon_schoner.sock; F=0
printf '@echo off\nA:\\RD2\\RD.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_schoner.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') SCHONER.EXE" bash tools/mkdisk.sh $IMG obj/ae_schoner.txt >/dev/null 2>&1
printf '[Schoner]\r\nZeit=5\r\nArt=0\r\n' > obj/schoner_ini.txt; mcopy -o -i $IMG obj/schoner_ini.txt ::/RD2/RD2.INI
rm -f $SER $SOCK; touch $SER
timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_schoner.py
Q() { timeout 20 python3 obj/qmon_schoner.py "$@" >/dev/null 2>&1; }
waitn() { for i in $(seq 300); do [ "$(tr -d '\r' < $SER | grep -ac "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; return 1; }
hell() { python3 -c "
from PIL import Image; a=Image.open('$1').convert('L'); print(sum(1 for v in a.tobytes() if v > 128))"; }
if waitn "SHELL: bereit" 1; then
  sleep 1; Q "screendump obj/schoner_vor.ppm"
  if waitn "SCHONER: an" 1; then
    sleep 3; Q "screendump obj/schoner_an.ppm" sleep1 "screendump obj/schoner_an2.ppm"
    n=$(hell obj/schoner_an.ppm); m=$(hell obj/schoner_vor.ppm)
    [ "$n" -ge 20 ] && [ "$n" -lt 1000 ] && echo "ok     Sternenfeld laeuft ($n helle Pixel, Schreibtisch hatte $m)" || { echo "FEHLER Sternenfeld ($n helle Pixel)"; F=1; }
    cmp -s obj/schoner_an.ppm obj/schoner_an2.ppm && { echo "FEHLER Sterne bewegen sich nicht"; F=1; } || echo "ok     Sterne bewegen sich"
    Q "sendkey spc" sleep2 "screendump obj/schoner_nach.ppm"
    waitn "SCHONER: aus" 1 && echo "ok     Taste beendet den Schoner" || { echo "FEHLER Schoner endet nicht"; F=1; }
    k=$(hell obj/schoner_nach.ppm); [ "$k" -gt 50000 ] && echo "ok     Schreibtisch wieder da ($k helle Pixel)" || { echo "FEHLER Schreibtisch nicht wieder da ($k)"; F=1; }
  else echo "FEHLER Schoner startet nicht"; F=1; fi
  Q "sendkey alt-f4" sleep1 "sendkey ret"
else echo "FEHLER Shell startet nicht"; F=1; fi
sleep 3; kill $QP 2>/dev/null; wait $QP 2>/dev/null
[ "$(tr -d '\r' < $SER | grep -ac 'SCHONER: an')" = 1 ] && echo "ok     genau ein Start je Ruhephase" || { echo "FEHLER Startzahl $(grep -ac 'SCHONER: an' $SER)"; F=1; }
[ $F = 0 ] && echo "Bildschirmschoner: alles ok"; exit $F
