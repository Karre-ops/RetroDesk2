#!/bin/bash
# Punkt 4: Zubehoer Notizblock (NOTIZ.EXE T Datei), nur Tastatur.
# 1 VGA: neue Datei, Tippen, Spaltengrenze 60, Strg+S, Esc.  2 VGA+CGA: zu lange Datei nur lesend, Tippen wirkungslos.
# 3 VGA: Aenderung, Esc -> Rueckfrage, Eingabe = Ja speichert.  4 VGA: 80 Zeilen voll, Eingabe abgewiesen.
cd "$(dirname "$0")/.."
IMG=obj/notiz.img; SER=obj/notiz_com1.txt; SOCK=/tmp/qmon_notiz.sock
cat > obj/ae_notiz.txt <<'AE'
@echo off
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\NOTIZ.EXE T A:\NEU.TXT
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\NOTIZ.EXE T A:\LANG.TXT
A:\RD2\RD2.EXE /NOMAUS /CGA A:\RD2\NOTIZ.EXE T A:\LANG.TXT
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\NOTIZ.EXE T A:\NEU.TXT
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\NOTIZ.EXE T A:\VOLL.TXT
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_notiz.txt   # 30.10.: 360 KB voll, LANG.TXT und VOLL.TXT passten nicht mehr
python3 -c "
open('/tmp/LANG.TXT','w',newline='').write(''.join('Zeile %d: RetroDesk 2 Notizblock\r\n' % i for i in range(1, 101)))
open('/tmp/VOLL.TXT','w',newline='').write('a\r\n' * 79)"
mcopy -o -i $IMG /tmp/LANG.TXT /tmp/VOLL.TXT ::/
rm -f $SER $SOCK obj/notiz?_*.ppm
timeout 400 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_notiz.py
Q() { python3 obj/qmon_notiz.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
tipp() { local s=$1 i c a=(); for ((i = 0; i < ${#s}; i++)); do c=${s:i:1}; case $c in ' ') c=spc;; [A-Z]) c=shift-${c,,};; esac; a+=("sendkey $c" sleep0.06); done; Q "${a[@]}"; }
lauf() {
  waitn "NOTIZ: bereit" 1 || { echo "FEHLER: Notizblock 1 startet nicht"; return; }; sleep 3
  tipp Hallo; Q "sendkey ret"; tipp Welt; Q "sendkey ret"; tipp "$(printf 'x%.0s' $(seq 62))"; sleep 0.5
  Q "sendkey ctrl-s"; waitn "NOTIZ: gespeichert" 1 || echo "FEHLER: Strg+S speichert nicht"; sleep 1.5
  Q "screendump obj/notiz1_vga.ppm" sleep0.3 "sendkey esc"
  for n in 2 3; do K=$([ $n = 2 ] && echo vga || echo cga)
    waitn "NOTIZ: bereit" $n || { echo "FEHLER: Notizblock $n startet nicht"; return; }; sleep 3
    tipp abc; sleep 1; Q "screendump obj/notiz2_$K.ppm" sleep0.3 "sendkey esc"; done
  waitn "NOTIZ: bereit" 4 || { echo "FEHLER: Notizblock 4 startet nicht"; return; }; sleep 3
  tipp z; sleep 0.5; Q "sendkey esc"; sleep 2.5; Q "screendump obj/notiz3_vga.ppm" sleep0.3 "sendkey ret"
  waitn "NOTIZ: gespeichert" 2 || echo "FEHLER: Rueckfrage speichert nicht"
  waitn "NOTIZ: bereit" 5 || { echo "FEHLER: Notizblock 5 startet nicht"; return; }; sleep 3
  Q "sendkey ret" sleep0.2 "sendkey ret" sleep0.2; tipp b; Q "sendkey ctrl-s"; waitn "NOTIZ: gespeichert" 3 || echo "FEHLER: volle Datei speichert nicht"; sleep 1
  Q "screendump obj/notiz4_vga.ppm" sleep0.3 "sendkey esc"
}
lauf
wait $QP
tr -d '\r' < $SER | grep -A1 -E "NOTIZ|RD: (Speicher|SPEICHER|alle|Fehler)" | grep -v "^--"
pr() { [ "$2" = "$3" ] && echo "ok     $1" || { echo "FEHLER $1"; echo "  ist:  $(printf %q "$2")"; echo "  soll: $(printf %q "$3")"; }; }
X=$(printf 'x%.0s' $(seq 60))
pr "NEU.TXT: Text, 60 Spalten, Rueckfrage speichert" "$(mtype -i $IMG ::/NEU.TXT | od -An -c | tr -s ' ' | tr -d '\n')" "$(printf "zHallo\r\nWelt\r\n$X" | od -An -c | tr -s ' ' | tr -d '\n')"
pr "LANG.TXT unveraendert (nur lesen)" "$(mtype -i $IMG ::/LANG.TXT | md5sum)" "$(md5sum < /tmp/LANG.TXT)"
pr "VOLL.TXT: 80 Zeilen, Eingabe abgewiesen" "$(mtype -i $IMG ::/VOLL.TXT | md5sum)" "$( (printf 'b'; printf 'a\r\n%.0s' $(seq 79)) | md5sum)"
[ "$(tr -d '\r' < $SER | grep -c 'nur lesen')" = 2 ] && echo "ok     LANG.TXT zweimal nur lesend geladen" || echo "FEHLER LANG.TXT nicht nur lesend"
[ -f test/ref/notiz1_vga.png ] && python3 test/bildvergleich.py obj/notiz1_vga.ppm test/ref/notiz1_vga.png obj/notiz2_vga.ppm test/ref/notiz2_vga.png obj/notiz2_cga.ppm test/ref/notiz2_cga.png obj/notiz3_vga.ppm test/ref/notiz3_vga.png obj/notiz4_vga.ppm test/ref/notiz4_vga.png
exit 0
