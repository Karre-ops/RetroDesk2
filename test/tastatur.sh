#!/bin/bash
# RDKEYB.COM (DE/US): in DOS (KEYTEST.COM liest INT 16h) und in RD2 (INPTEST). Doppelt laden schaltet nur um.
cd "$(dirname "$0")/.."
IMG=obj/tast.img; SER=obj/tast_com1.txt; S=/tmp/qmon_tast.sock; export QMON=$S; F=0
export WATCOM=${WATCOM:-/opt/ow} PATH=/opt/ow/binl64:/opt/ow/binl:$PATH INCLUDE=/opt/ow/h
(cd obj && wasm -q -0 -fo=keytest.obj ../test/KEYTEST.ASM && wlink option quiet format dos com name keytest.com file keytest.obj) || exit 1
cat > obj/ae_tast.txt <<'AE'
@echo off
A:\RD2\RDKEYB.COM DE > A:\K1.TXT
A:\RD2\RDKEYB.COM DE > A:\K2.TXT
A:\KEYTEST.COM
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\INPTEST.EXE
A:\FDAPM.COM POWEROFF
AE
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') INPTEST.EXE" FMT=720 bash tools/mkdisk.sh $IMG obj/ae_tast.txt >/dev/null 2>&1
mcopy -o -i $IMG out/RDKEYB.COM ::/RD2/; mcopy -o -i $IMG obj/keytest.com ::/KEYTEST.COM
rm -f $SER $S
timeout 150 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 ../test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 300); do c=$(grep -c "$1" $SER 2>/dev/null); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
waitn "KEYTEST: bereit" || { echo "FEHLER KEYTEST startet nicht"; F=1; }
for k in z y shift-2 semicolon apostrophe bracket_left minus shift-minus grave_accent a equal e shift-equal a grave_accent spc ctrl-alt-q alt_r-q less alt_r-less shift-0 ctrl-z ctrl-alt-f1 z ctrl-alt-f2 z delete home esc; do Q "sendkey $k"; sleep 0.3; done
waitn "K 011B"
erw="2C79 157A 0322 2794 2884 1A81 0CE1 0C3F 1E83 1282 1E85 395E 1040 1040 563C 567C 0B3D 1519 2C7A 2C79 5300 4700"
ist=$(grep -o "K [0-9A-F]*" $SER | cut -c3- | grep -v 011B | tr '\n' ' ' | sed 's/ $//')
if [ "$ist" = "$erw" ]; then echo "ok     DOS: deutsche Belegung (y/z, Umlaute, ß, Tottasten, AltGr, <>|, Strg+Z, Umschalten US/DE)"
else echo "FEHLER DOS-Belegung"; echo "  soll: $erw"; echo "  ist:  $ist"; F=1; fi
sleep 8; for k in semicolon z esc; do Q "sendkey $k"; sleep 0.5; done
wait $QP 2>/dev/null
if [ "$(grep "INPTEST: KEY" $SER | head -2 | tr -d "\r" | cut -d" " -f3 | tr "\n" " ")" = "0x00000094 0x00000079 " ]; then echo "ok     RD2 (INPTEST): ö und y kommen an"; else echo "FEHLER RD2"; grep "INPTEST: KEY" $SER | head -3; F=1; fi
mtype -i $IMG ::/K1.TXT | grep -q "geladen: DE" && mtype -i $IMG ::/K2.TXT | grep -q "umgeschaltet" && echo "ok     zweiter Aufruf schaltet nur um" || { echo "FEHLER Doppelladen"; F=1; }
[ $F = 0 ] && echo "Tastatur: alles ok"
exit $F
