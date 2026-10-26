#!/bin/bash
# Punkt 6: Editor RDEDIT (RDEDIT.EXE T Datei), nur Tastatur.
# 1 VGA: neue Datei, Tippen, Auswahl Umschalt+Ende, Strg+C/V, Rueck verbindet Zeilen, Strg+S.
# 2 VGA: 3000 Zeilen (ca. 90 KB), Strg+G 2500, Strg+F, Strg+Ende, Strg+S.
# 3 CGA: T\AUTOEXEC.BAT, Strg+H alle ersetzen, Strg+S legt .BAK an; Aenderung, Esc -> Rueckfrage speichert (ohne neue .BAK).
# 4 VGA: Zeile laenger als 250 Zeichen: nur lesen, Tippen wirkungslos, Esc ohne Rueckfrage.
cd "$(dirname "$0")/.."
IMG=obj/edit.img; SER=obj/edit_com1.txt; SOCK=/tmp/qmon_edit.sock
cat > obj/ae_edit.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDEDIT.EXE T A:\NEU.TXT
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDEDIT.EXE T A:\LANG.TXT
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\RDEDIT.EXE T A:\T\AUTOEXEC.BAT
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDEDIT.EXE T A:\BREIT.TXT
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_edit.txt
python3 -c "
open('/tmp/LANG.TXT','w',newline='').write(''.join('Zeile %d: RetroDesk 2 Editor\r\n' % i for i in range(1, 3001)))
open('/tmp/AUTOEXEC.BAT','w',newline='').write('@echo off\r\nPATH A:\\\\\r\necho Hallo\r\n')
open('/tmp/BREIT.TXT','w',newline='').write('kurz\r\n' + 'a' * 300 + '\r\nende\r\n')"
mcopy -o -i $IMG /tmp/LANG.TXT /tmp/BREIT.TXT ::/; mmd -i $IMG ::/T 2>/dev/null; mcopy -o -i $IMG /tmp/AUTOEXEC.BAT ::/T/
rm -f $SER $SOCK obj/edit?_*.ppm
timeout 600 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_edit.py
Q() { python3 obj/qmon_edit.py "$@"; }
waitn() { for i in $(seq 900); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
tipp() { local s=$1 i c a=(); for ((i = 0; i < ${#s}; i++)); do c=${s:i:1}; case $c in ' ') c=spc;; [A-Z]) c=shift-${c,,};; ':') c=shift-semicolon;; '!') c=shift-1;; esac; a+=("sendkey $c" sleep0.06); done; Q "${a[@]}"; }
k() { local a=(); for x in "$@"; do a+=("sendkey $x" sleep0.15); done; Q "${a[@]}"; }
lauf() {
  waitn "RDEDIT: bereit" 1 || { echo "FEHLER: Editor 1 startet nicht"; return; }; sleep 3
  tipp "Hallo Welt"; k ret; tipp Zweite; k home shift-end ctrl-c end ret ctrl-v ret; tipp abc; k backspace home backspace
  sleep 0.5; k ctrl-s; waitn "RDEDIT: gespeichert" 1 || echo "FEHLER: Strg+S speichert nicht"; sleep 1.5
  Q "screendump obj/edit1_vga.ppm" sleep0.3 "sendkey esc"
  waitn "RDEDIT: bereit" 2 || { echo "FEHLER: Editor 2 startet nicht"; return; }; sleep 3
  k ctrl-g; sleep 1.5; tipp 2500; k ret; sleep 1; tipp X
  k ctrl-f; sleep 1.5; tipp "zeile 1234:"; k ret; waitn "RDEDIT: gefunden" 1 || echo "FEHLER: Strg+F findet nicht"; sleep 1
  Q "screendump obj/edit2_vga.ppm" sleep0.3
  k ctrl-end; tipp Ende; k ctrl-s; waitn "RDEDIT: gespeichert" 2 || echo "FEHLER: grosse Datei speichert nicht"; sleep 1; k esc
  waitn "RDEDIT: bereit" 3 || { echo "FEHLER: Editor 3 startet nicht"; return; }; sleep 3
  k ctrl-h; sleep 1.5; tipp echo; k tab; tipp REM; sleep 0.5; Q "screendump obj/edit3_cga.ppm" sleep0.3; k alt-a
  waitn "RDEDIT: ersetzt" 1 || echo "FEHLER: Ersetzen meldet nichts"; sleep 1
  k ctrl-s; waitn "RDEDIT: gespeichert, BAK" 1 || echo "FEHLER: keine .BAK"; sleep 1
  tipp x; k esc; sleep 2; k ret; waitn "RDEDIT: gespeichert" 4 || echo "FEHLER: Rueckfrage speichert nicht"
  waitn "RDEDIT: bereit" 4 || { echo "FEHLER: Editor 4 startet nicht"; return; }; sleep 3
  tipp zz; sleep 1; Q "screendump obj/edit4_vga.ppm" sleep0.3; k esc
}
lauf
wait $QP
tr -d '\r' < $SER | grep -A1 -E "RDEDIT|RD: (Speicher|SPEICHER|alle|Fehler)" | grep -v "^--"
pr() { [ "$2" = "$3" ] && echo "ok     $1" || { echo "FEHLER $1"; echo "  ist:  $(printf %q "$2" | cut -c1-300)"; echo "  soll: $(printf %q "$3" | cut -c1-300)"; }; }
pr "NEU.TXT: Tippen, Kopieren, Einfuegen, Verbinden" "$(mtype -i $IMG ::/NEU.TXT | od -An -c | tr -s ' ' | tr -d '\n')" "$(printf 'Hallo Welt\r\nZweite\r\nZweiteab' | od -An -c | tr -s ' ' | tr -d '\n')"
SOLL=$(python3 -c "
import sys
z=['Zeile %d: RetroDesk 2 Editor' % i for i in range(1, 3001)] + ['']
z[2499]='X'+z[2499]; z[-1]='Ende'
sys.stdout.write('\r\n'.join(z))" | md5sum)
pr "LANG.TXT: Gehe zu 2500, Ende, 3001 Zeilen" "$(mtype -i $IMG ::/LANG.TXT | md5sum)" "$SOLL"
pr "AUTOEXEC.BAT: alle ersetzt, dann x" "$(mtype -i $IMG ::/T/AUTOEXEC.BAT | od -An -c | tr -s ' ' | tr -d '\n')" "$(printf 'x@REM off\r\nPATH A:\\\r\nREM Hallo\r\n' | od -An -c | tr -s ' ' | tr -d '\n')"
pr "AUTOEXEC.BAK: Original" "$(mtype -i $IMG ::/T/AUTOEXEC.BAK | md5sum)" "$(md5sum < /tmp/AUTOEXEC.BAT)"
pr "BREIT.TXT unveraendert (nur lesen)" "$(mtype -i $IMG ::/BREIT.TXT | md5sum)" "$(md5sum < /tmp/BREIT.TXT)"
[ "$(tr -d '\r' < $SER | grep -c 'geladen, nur lesen')" = 1 ] && echo "ok     BREIT.TXT nur lesend geladen" || echo "FEHLER BREIT.TXT nicht nur lesend"
mdir -i $IMG ::/ | grep -q '\$\$\$' && echo "FEHLER .\$\$\$ liegt noch da" || echo "ok     keine .\$\$\$-Reste"
[ -f test/ref/edit1_vga.png ] && python3 test/bildvergleich.py obj/edit1_vga.ppm test/ref/edit1_vga.png obj/edit2_vga.ppm test/ref/edit2_vga.png obj/edit3_cga.ppm test/ref/edit3_cga.png obj/edit4_vga.ppm test/ref/edit4_vga.png
exit 0
