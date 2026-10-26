#!/bin/bash
# Punkt 4: Zubehoer Rechner (RECHNER.EXE T), Tastatur und Maus. Der Rechenkern selbst: test/rechner_kern.sh (am PC).
# VGA: 12+30= -> 42, Bild; Entf, 2 W -> 1,414213562; 7/0= -> Fehler; Klick auf 9, W -> 3.  CGA: 1,5*4= -> 6, Bild.
cd "$(dirname "$0")/.."
IMG=obj/rechner.img; SER=obj/rechner_com1.txt; SOCK=/tmp/qmon_rechner.sock
cat > obj/ae_rechner.txt <<'AE'
@echo off
A:\CTMOUSE.EXE
A:\RD2\RD.EXE /VGA A:\RD2\RECHNER.EXE T
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\RECHNER.EXE T
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_rechner.txt
mcopy -i $IMG ../test/qemu/CTMOUSE.EXE ::/
rm -f $SER $SOCK obj/rechner?_*.ppm
timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_rechner.py
Q() { python3 obj/qmon_rechner.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
k() { local a=() c; for c in "$@"; do a+=("sendkey $c" sleep0.15); done; Q "${a[@]}"; }
letzte() { tr -d '\r' < $SER | grep "^RECHNER: " | grep -v bereit | tail -1 | cut -c10-; }
pr() { local i; for i in $(seq 20); do [ "$(letzte)" = "$2" ] && break; sleep 0.2; done; [ "$(letzte)" = "$2" ] && echo "ok     $1 -> $2" || echo "FEHLER $1 -> $(letzte) (erwartet $2)"; }
lauf() {
  waitn "RECHNER: bereit" 1 || { echo "FEHLER: Rechner 1 startet nicht"; return; }; sleep 3
  k 1 2 shift-equal 3 0 ret; pr "12+30 Eingabe" 42
  sleep 0.5; Q "screendump obj/rechner1_vga.ppm"
  k delete 2 w; pr "Entf, 2 W" 1.414213562
  k 7 slash 0 equal; pr "7/0=" Fehler
  k c 9 shift-5 ret; pr "9 % (Prozent)" 0.09
  k delete
  # Maus (QEMU-Einheit 3 Pixel waagrecht, 1,5 senkrecht, ab oben links): "9" bei (478,152), "Wz" bei (570,182)
  null() { Q "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.4; }
  null; Q "mouse_move 159 101" sleep0.6 "mouse_button 1" sleep0.2 "mouse_button 0" sleep0.5; pr "Klick 9" 9
  null; Q "mouse_move 190 121" sleep0.6 "mouse_button 1" sleep0.2 "mouse_button 0" sleep0.5; pr "Klick Wz" 3
  k 4 ret; pr "danach Tastatur (Fokus zurueck)" 4
  Q "sendkey esc"
  waitn "RECHNER: bereit" 2 || { echo "FEHLER: Rechner 2 startet nicht"; return; }; sleep 3
  k 1 comma 5 shift-8 4 ret; pr "1,5*4 (CGA)" 6
  sleep 0.5; Q "screendump obj/rechner2_cga.ppm" sleep0.3 "sendkey esc"
}
lauf
wait $QP
tr -d '\r' < $SER | grep -E "RD: (Speicher vollstaendig|SPEICHER|Fehler)"
[ -f test/ref/rechner1_vga.png ] && python3 test/bildvergleich.py obj/rechner1_vga.ppm test/ref/rechner1_vga.png obj/rechner2_cga.ppm test/ref/rechner2_cga.png
exit 0
