#!/bin/bash
# RDMAUS.COM mit Bus-, InPort- und PS/2-Maus in Bochs (QEMU kennt keine Busmaus). Bochs-Display ueber RFB (VNC),
# test/qemu/rfbmaus.py schickt Bewegung und Klicks (links, rechts, Mitte). MPROBE.COM (test/dos) meldet INT 33h auf COM1.
# Erwartet: richtige Erkennung, Bewegung, Tasten B1 B2 B4 in dieser Reihenfolge, Funktion 5 zaehlt je Taste 1.
# Aufruf: test/busmaus.sh [bus|inport|ps2]  (ohne Angabe alle drei, je ca. 50 s; wegen der 2,5-Minuten-Grenze einzeln aufrufen). Braucht: apt-get install bochs bochsbios vgabios
cd "$(dirname "$0")/.."
command -v bochs >/dev/null || { echo "busmaus: bochs fehlt (apt-get install bochs bochsbios vgabios)"; exit 1; }
F=0; mkdir -p obj
nasm -f bin -o obj/MPROBE.COM test/dos/MPROBE.ASM || exit 1
NUR="RDMAUS.COM" FMT=1440 bash tools/mkdisk.sh obj/bm.img test/bochs/ae_maus.txt >/dev/null || exit 1
mcopy -o -i obj/bm.img obj/MPROBE.COM ::/
stopp() { for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in bochs*) kill ${p#/proc/} 2>/dev/null;; esac; done; sleep 0.5; }
for T in ${1:-bus inport ps2}; do
  case $T in bus) E='Busmaus (Logitech';; inport) E='InPort-Busmaus';; ps2) E='PS/2-Maus';; esac
  stopp; rm -f obj/bm_com1.txt
  sed "s#MAUSTYP#$T#; s#DISK#obj/bm.img#; s#LOG#obj/bochs_$T.log#" test/bochs/maus.rc > obj/bochs_$T.rc
  (timeout 110 python3 test/qemu/comlausch.py 5901 obj/bm_com1.txt >/dev/null 2>&1 &); sleep 0.3
  ( (echo c; sleep 110) | timeout 110 bochs -q -f obj/bochs_$T.rc & ) >/dev/null 2>&1
  sleep 1
  timeout 105 python3 test/qemu/rfbmaus.py 5900 fobj/bm_com1.txt:X320 w1 c w1 m400,300 w1 m420,300 w1 d1 w0.5 d0 w0.5 d4 w0.5 d0 w0.5 d2 w0.5 d0 "fobj/bm_com1.txt:P " w1 >/dev/null 2>&1
  sleep 1; stopp
  O=$(tr -d '\r' < obj/bm_com1.txt 2>/dev/null)
  ok() { if [ "$1" = 1 ]; then echo "ok     $T: $2"; else echo "FEHLER $T: $2"; F=1; fi; }
  ok "$(echo "$O" | grep -qF "RDMAUS geladen: $E" && echo 1)" "erkannt als $E"
  ok "$(echo "$O" | grep -q "^R65535 N3" && echo 1)" "INT 33h/0: Treiber da, 3 Tasten"
  ok "$(echo "$O" | grep -E "^X" | grep -vq "^X320 Y100 " && echo 1)" "Zeiger bewegt sich"
  B=$(echo "$O" | grep -oE "B[0-9]+" | uniq | tr '\n' ' ')
  ok "$(echo "$B" | grep -q "B1 B0 B2 B0 B4 B0" && echo 1)" "Tasten links, rechts, Mitte ($B)"
  ok "$(echo "$O" | grep -q "^P 1  1  1" && echo 1)" "Funktion 5 zaehlt je Taste einen Druck"
done
[ $F = 0 ] && echo "BUSMAUS: alles ok" || echo "BUSMAUS: FEHLER"
exit $F
