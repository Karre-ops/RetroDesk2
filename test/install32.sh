#!/bin/bash
# PLAN_FS 5: Installation vom 1,44-MB-Satz (Pentium) mit "C: vorher formatieren" und FAT32 (Alt+3).
# Platte 300 MB mit FAT16-Partition (06h), altem DOS und Daten. Lauf 1: Einrichtung "I", INSTALL, Formatieren + FAT32,
# Warnung Ja, RDFORMAT (FAT32 vorgewaehlt), Rueckfrage 36 Ja -> Typ 0Bh, Neustart (QEMU endet).
# Lauf 2: RD2 von Diskette, SHELL findet den Merker, RDFORMAT /W formatiert FAT32, INSTALL macht weiter (Disketten 2, 1),
# RDSYS mit dem FAT32-Kern. Lauf 3: Start von C: bis zur Oberflaeche. Laeuft etwa 6 Minuten.
cd "$(dirname "$0")/.."
F=0; S=/tmp/qmon_i32.sock; SER=obj/com1_i32.txt; export QMON=$S; V=out/verteilung
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
T() { timeout 10 python3 test/qemu/qmon.py text 2>/dev/null; }
waitscr() { for i in $(seq ${2:-30}); do T | grep -q "$1" && return 0; sleep 1; done; return 1; }
waitser() { for i in $(seq ${3:-60}); do [ "$(tr -d '\r' < $SER | grep -ac "$1")" -ge "${2:-1}" ] && return 0; sleep 1; done; return 1; }
schritt() { if waitser "$1" ${3:-1} ${4:-90}; then echo "ok     $2"; return 0; else echo "FEHLER $2"; tr -d '\r' < $SER | grep -av "^I[0-9A-F][0-9A-F] " | tail -4; F=1; return 1; fi; }
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
[ -f $V/2_386_144MB/RD2_386A.IMG ] || bash tools/mkverteil.sh >/dev/null || exit 1
cp $V/2_386_144MB/RD2_386A.IMG obj/i32_a.img; cp $V/2_386_144MB/RD2_386B.IMG obj/i32_b.img
HD=obj/i32_hd.img; ZYL=609; NSEK=$((ZYL*16*63-63)); P=$HD@@32256
python3 - $HD $NSEK $ZYL <<'PY'
import sys, struct
f, n, z = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
with open(f, 'wb') as d: d.truncate(z*16*63*512)
m = bytearray(open('test/daten/mbr_rdfdisk.bin', 'rb').read(512)); m[446:510] = bytes(64)
m[446:462] = bytes([0x80, 1, 1, 0, 6, 15, 63 | (((z-1) >> 8) << 6), (z-1) & 255]) + struct.pack('<II', 63, n); m[510:512] = b'\x55\xaa'
with open(f, 'r+b') as d: d.write(m)
PY
mformat -i $P -T $NSEK -h 16 -s 63 -H 63 -v ALTHDD ::
printf 'x' > obj/i32_io.tmp; mcopy -i $P obj/i32_io.tmp ::/IO.SYS; mmd -i $P ::/SPIELE; mcopy -i $P obj/i32_io.tmp ::/SPIELE/SIMCITY.EXE
start() { rm -f $S $SER; touch $SER
  timeout ${2:-400} qemu-system-i386 -cpu pentium -m 16 -display none -no-reboot $1 -drive file=$HD,format=raw,if=ide,index=0 \
    -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 & QP=$!; }
echo "=== Lauf 1: Installation, FAT32 gewaehlt"
start "-drive file=obj/i32_a.img,format=raw,if=floppy,index=0 -boot a"
waitscr "Erster Start" 90 && echo "ok     Einrichtung erscheint" || { echo "FEHLER keine Einrichtung"; F=1; }
sleep 1; Q "sendkey i"
schritt "INSTALL: Start" "Installation offen" && { sleep 6; Q "sendkey alt-f"; sleep 1; Q "sendkey alt-3"; sleep 1; Q "screendump obj/i32_dialog.ppm"; Q "sendkey ret"; }
schritt "INSTALL: Frage" "Warnung vor dem Formatieren" && { sleep 1; Q "sendkey tab" sleep0.3 "sendkey ret"; }
schritt "RDFORMAT: bereit" "RDFORMAT startet fuer C:" && { sleep 3; Q "screendump obj/i32_rdformat.ppm"; Q "sendkey ret"; }
schritt "RDFORMAT: Meldung 36" "Rueckfrage FAT32 mit Neustart" && { sleep 2; Q "sendkey tab" sleep0.3 "sendkey ret"; }
wait $QP 2>/dev/null; cp $SER obj/com1_i32_1.txt
ok "Partitionstyp 0Bh, Merker in Sektor 2" "python3 -c \"import sys; d=open('$HD','rb').read(1024); sys.exit(0 if d[450]==0x0B and d[512:519]==b'RD2FMT1' else 1)\""
echo "=== Lauf 2: weiter formatieren und installieren"
start "-drive file=obj/i32_a.img,format=raw,if=floppy,index=0 -boot a" 600
waitscr "Erster Start" 25 && { echo "ok     Einrichtung (Diskette ohne RD2.INI): I"; sleep 1; Q "sendkey i"; }
schritt "RDFORMAT: Merker gefunden" "SHELL startet RDFORMAT /W" 1 120
schritt "RDFORMAT: fertig" "C: als FAT32 formatiert" 1 300 && { sleep 2; Q "sendkey ret"; }
schritt "INSTALL: fortgesetzt" "INSTALL macht von selbst weiter" 1 60
ok "INSTALL nur einmal" "[ \$(tr -d '\r' < $SER | grep -ac 'INSTALL: Start') -le 1 ]"
waitser "Diskette 2 (RD2 386 2)" 1 200 && { echo "ok     Diskette 2"; Q "change floppy0 obj/i32_b.img"; sleep 2; Q "sendkey ret"; } || { echo "FEHLER keine Meldung Diskette 2"; F=1; }
waitser "Diskette 1 (RD2SYSTEM)" 1 200 && { echo "ok     Diskette 1 zurueck"; Q "change floppy0 obj/i32_a.img"; sleep 2; Q "sendkey ret"; } || { echo "FEHLER keine Meldung Diskette 1"; F=1; }
# FAT32-Kern: unten per Vergleich von C:\KERNEL.SYS geprueft
waitser "startf" 1 30 && { sleep 1; Q "sendkey ret"; }
waitser "Diskette 1 drin lassen" 1 30 && { sleep 1; Q "sendkey ret"; }
waitser "ist installiert" 1 150 && { echo "ok     Schlussmeldung"; sleep 1; Q "sendkey tab" sleep0.3 "sendkey ret"; } || { echo "FEHLER keine Schlussmeldung"; F=1; }
sleep 3; Q quit; wait $QP 2>/dev/null; cp $SER obj/com1_i32_2.txt
dd if=$HD of=obj/i32_part.img bs=512 skip=63 count=$NSEK 2>/dev/null
ok "Startsektor FAT32" "[ \"\$(dd if=obj/i32_part.img bs=1 skip=82 count=5 2>/dev/null)\" = FAT32 ]"
ok "fsck.fat sauber" "fsck.fat -n obj/i32_part.img >/dev/null 2>&1"
ok "C:\\KERNEL.SYS ist der FAT32-Kern" "mcopy -n -i $P ::/KERNEL.SYS obj/i32_k.sys 2>/dev/null && cmp -s obj/i32_k.sys src/disk/fdkern/F32/KERNEL.SYS"
m=""; for f in RD2/RD2.EXE RD2/SHELL.EXE RD2/RDFORMAT.EXE RD2/RD32.MOD CONFIG.SYS; do mdir -i $P ::/$f >/dev/null 2>&1 || m="$m $f"; done
ok "RD2 auf C:${m:+ - fehlt:$m}" "[ -z '$m' ]"
ok "alte Dateien weg" "! mdir -i $P ::/SPIELE >/dev/null 2>&1"
echo "=== Lauf 3: Start von C:"
rm -f $S; timeout 100 qemu-system-i386 -cpu pentium -m 16 -display none -no-reboot -drive file=$HD,format=raw,if=ide,index=0 -boot c -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!; g=0
for i in $(seq 70); do sleep 1; Q "screendump obj/i32_c.ppm"; python3 -c "from PIL import Image; import sys; sys.exit(0 if Image.open('obj/i32_c.ppm').size==(640,480) else 1)" 2>/dev/null && { g=1; break; }; done
ok "startet von C: (FAT32, FreeDOS-Kern) bis zur Oberflaeche" "[ $g = 1 ]"
Q quit; wait $QP 2>/dev/null
[ $F = 0 ] && echo "install32.sh: alles ok" || echo "install32.sh: FEHLER"
exit $F
