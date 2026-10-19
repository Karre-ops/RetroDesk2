#!/bin/bash
# Phase-1-Test: RD2 von einer 720-KB-Diskette booten (QEMU, nur Laufwerk A:, keine Festplatte).
#  1. LEER.EXE direkt unter DOS: muss "benoetigt RetroDesk 2" melden und Fehlercode 1 liefern
#  2. RD.EXE laedt LEER.EXE: Ausgaben auf COM1 pruefen (API-Version, Befehlszeile, Ergebnis 42)
cd "$(dirname "$0")/.."
IMG=obj/sys720.img; SER=obj/com1.txt
cat > obj/autoexec.txt <<'AE'
@echo off
A:\RD2\LEER.EXE > A:\DIREKT.TXT
if errorlevel 1 echo FEHLERCODE1 >> A:\DIREKT.TXT
A:\RD2\RD.EXE A:\RD2\LEER.EXE hallo welt > A:\RD.TXT
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/autoexec.txt
rm -f $SER
timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a -serial file:$SER >/dev/null 2>&1
echo "qemu rc=$?"
F=0; chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=$((F+1)); fi; }
D=$(mtype -i $IMG ::/DIREKT.TXT 2>/dev/null | tr -d '\r')
chk "LEER.EXE unter DOS meldet 'benoetigt RetroDesk 2'" '[[ "$D" == *"tigt RetroDesk 2."* ]]'
chk "LEER.EXE unter DOS endet mit Fehlercode 1"       '[[ "$D" == *FEHLERCODE1* ]]'
S=$(tr -d '\r' < $SER 2>/dev/null)
chk "Kern startet (COM1)"                    '[[ "$S" == *"RD: RetroDesk 2 Kern (Phase 1), API 1.0"* ]]'
chk "Programm sieht API 1.0"                 '[[ "$S" == *"LEER: Kern-API 0x00000100"* ]]'
chk "Befehlszeile kommt an"                  '[[ "$S" == *"hallo welt"* ]]'
chk "fehlende Funktion liefert 0 (MemAlloc)" '[[ "$S" == *"MemAlloc fehlt noch (erwartet)"* ]]'
chk "Rueckkehr zum Kern, Ergebnis 42"        '[[ "$S" == *"Programm beendet, Ergebnis 0x0000002A"* ]]'
python3 tools/budget.py out $IMG | tail -2
echo "--- COM1 ---"; echo "$S"
[ $F = 0 ] && echo "Diskettentest: alles ok" || echo "Diskettentest: $F Fehler"
exit $F
