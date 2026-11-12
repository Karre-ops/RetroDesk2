#!/bin/bash
# Gesamtkette leere Festplatte -> RD2 installiert, drei Starts von Diskette auf derselben 20-MB-Platte:
# 1. RDFDISK: Anlegen (ganze Platte), Schreiben  -> C: primaer, aktiv
# 2. RDFORMAT C: (Schnell)                         -> FAT16, fsck sauber
# 3. RD2SETUP: T (Textinstallation) nach C:\RD2         -> Dateien, CONFIG.SYS mit SHELL= und INSTALL=RDMAUS /P, startfaehig (J)
# 4. Start von der Festplatte ohne Diskette          -> FreeDOS-Kern, RD2 als Shell, Desktop
# Nur Tastatur. Bilder obj/kette{1..3}.ppm.
cd "$(dirname "$0")/.."
H=obj/kette_hd.img; SOCK=/tmp/qmon_kette.sock; export QMON=$SOCK; F=0
Q() { python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { for i in $(seq ${3:-600}); do [ "$(grep -ac "$1" $2 2>/dev/null)" -ge "${4:-1}" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
waitscr() { for i in $(seq ${2:-30}); do python3 test/qemu/qmon.py text 2>/dev/null | grep -q "$1" && return 0; sleep 1; done; return 1; }
starten() {  # $1 Diskette, $2 serielles Protokoll
  rm -f $SOCK $2
  timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$1,format=raw,if=floppy,index=0 \
    -drive file=$H,format=raw,if=ide,index=0 -boot a -serial file:$2 -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
}
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
python3 -c "open('$H','wb').write(bytes(40*16*63*512))"
NUR0="$(awk '$4=="S" && $1!="RDSYS.EXE"{print $1}' tools/budget.txt | tr '\n' ' ') RDDISK.MOD"

# ---- 1. Partitionieren ----
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\RDFDISK.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_k1.txt
NUR="$NUR0 RDFDISK.EXE" bash tools/mkdisk.sh obj/kette_a1.img obj/ae_k1.txt >/dev/null 2>&1
S=obj/kette1_com1.txt; starten obj/kette_a1.img $S
if waitn "RDFDISK: Liste bereit" $S; then
  sleep 3; Q "sendkey alt-a"; waitn "RDFDISK: Liste bereit" $S 100 && sleep 2 && Q "sendkey ret"
  waitn "RDFDISK: Liste bereit" $S 300 2 && sleep 2.5 && Q "screendump obj/kette1.ppm" sleep0.3 "sendkey alt-s"
  waitn "RDFDISK: Meldung 13" $S 100 && sleep 2 && Q "sendkey ret"
  waitn "RDFDISK: Meldung 14" $S 300 && sleep 2 && Q "sendkey ret"
fi
wait $QP
pruef "1 RDFDISK geschrieben" "grep -aq 'RDFDISK: geschrieben' $S"
pruef "1 RDFDISK startet neu (M14)" "grep -aq 'RDFDISK: Neustart' $S"
python3 - $H <<'PY' && echo "ok     1 MBR: eine Partition, aktiv, FAT16" || { echo "FEHLER 1 MBR"; F=1; }
import sys, struct
d = open(sys.argv[1], 'rb').read(512); e = d[446:462]
sys.exit(0 if d[510:] == b'\x55\xaa' and e[0] == 0x80 and e[4] in (4, 6) and struct.unpack('<I', e[8:12])[0] == 63 and not any(d[462:510]) else 1)
PY
OFS=$((63*512)); NSEK=$(python3 -c "import struct;print(struct.unpack('<I',open('$H','rb').read(512)[458:462])[0])")

# ---- 2. Formatieren ----
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\RDFORMAT.EXE C:\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_k2.txt
NUR="$NUR0 RDFORMAT.EXE" bash tools/mkdisk.sh obj/kette_a2.img obj/ae_k2.txt >/dev/null 2>&1
S=obj/kette2_com1.txt; starten obj/kette_a2.img $S
if waitn "RDFORMAT: bereit" $S; then
  sleep 3; Q "screendump obj/kette2.ppm" sleep0.3
  for v in 1 2 3; do Q "sendkey ret"; waitn "RDFORMAT: Meldung 22" $S 100 && break; done   # Dialog unter Last spaeter da (25.10.)
  sleep 2 && Q "sendkey ret"
  waitn "RDFORMAT: \(fertig\|Fehler\|Meldung 33\)" $S 2000 && sleep 3 && Q "sendkey ret"
fi
wait $QP
pruef "2 RDFORMAT fertig" "grep -aq 'RDFORMAT: fertig' $S"
dd if=$H of=obj/kette_part.img bs=512 skip=63 count=$NSEK 2>/dev/null
pruef "2 FAT16 sauber (fsck)" "fsck.fat -n obj/kette_part.img >/dev/null 2>&1"

# ---- 3. Installieren ----
printf '@echo off\r\nA:\\RD2\\RD2SETUP.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' | tr -d '\r' > obj/ae_k3.txt
SETUP=1 bash tools/mkdisk.sh obj/kette_a3.img obj/ae_k3.txt >/dev/null 2>&1
S=obj/kette3_com1.txt; starten obj/kette_a3.img $S
waitscr "Einrichtung" 120 && { Q "sendkey t" sleep2 "sendkey ret"; waitscr "startfaehig machen" 90 && Q "sendkey j"; waitscr "Festplatte ist startfaehig\|FEHLER: Festplatte" 60; sleep 1; python3 test/qemu/qmon.py text > obj/schirm_kette3.txt 2>/dev/null; Q "screendump obj/kette3.ppm" sleep0.3 "sendkey esc" sleep2 "sendkey n"; }
wait $QP
C="-i $H@@$OFS"
mdir $C -b ::/RD2 > obj/kette_dir.txt 2>&1
for d in RD2.EXE RDGDI.MOD RDSWAP.BIN RDMAUS.COM RD2SETUP.EXE RD2.INI SHELL.EXE; do pruef "3 C:\\RD2\\$d" "grep -qi '/$d' obj/kette_dir.txt"; done
mtype $C ::/CONFIG.SYS 2>/dev/null | tr -d '\r' > obj/kette_cs.txt
pruef "3 CONFIG.SYS SHELL=C:\\RD2\\RD2.EXE /P" "grep -q '^SHELL=C:\\\\RD2\\\\RD2.EXE /P C:\\\\RD2\\\\SHELL.EXE$' obj/kette_cs.txt"
pruef "3 CONFIG.SYS INSTALL=RDMAUS /P" "grep -q '^INSTALL=C:\\\\RD2\\\\RDMAUS.COM /P$' obj/kette_cs.txt"
dd if=$H of=obj/kette_part.img bs=512 skip=63 count=$NSEK 2>/dev/null
pruef "3 FAT16 sauber nach Installation" "fsck.fat -n obj/kette_part.img >/dev/null 2>&1"
pruef "3 Meldung startfaehig" "grep -q 'Festplatte ist startfaehig' obj/schirm_kette3.txt"
pruef "3 C:\KERNEL.SYS" "mdir $C -b :: 2>/dev/null | grep -qi '/KERNEL.SYS'"
pruef "3 Bootsektor FreeDOS (55AA, FAT16)" "python3 -c \"import sys;d=open('$H','rb').read()[63*512:64*512];sys.exit(0 if d[510:]==b'\\x55\\xaa' and d[54:59]==b'FAT16' and d[0]==0xEB else 1)\""

# ---- 4. Start von der Festplatte (ohne Diskette) ----
S=obj/kette4_com1.txt; rm -f $SOCK $S
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$H,format=raw,if=ide,index=0 -boot c \
  -serial file:$S -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
if waitn "SHELL: bereit\|Aufruf: RD" $S 600; then sleep 5; Q "screendump obj/kette4.ppm"; fi
sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null
pruef "4 RD2 startet von Festplatte" "grep -aq 'RD: Zeitscheiben an' $S"
pruef "4 Desktop (SHELL) laeuft" "grep -aq 'SHELL: bereit' $S"
[ $F = 0 ] && echo "kette.sh: alles ok" || echo "kette.sh: FEHLER"
exit $F
