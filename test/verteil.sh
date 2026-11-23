#!/bin/bash
# Verteilformen (tools/mkverteil.sh): Installation aus dem 1,44-MB-Satz (zwei Disketten, Pentium) und von der CD
# (2,88-MB-Abzug, schreibgeschuetzt) auf eine Platte mit vorhandenem DOS (kein SYS); USB-Abbild startet bis zur
# Einrichtung. Je Installation: Meldungen, keine Frage nach Diskette 5, Dateien samt RD2/32, SDK und SDK32 auf C:.
cd "$(dirname "$0")/.."
F=0; S=/tmp/qmon_vt.sock; SER=obj/com1_vt.txt; export QMON=$S; V=out/verteilung
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
T() { timeout 10 python3 test/qemu/qmon.py text 2>/dev/null; }
waitscr() { for i in $(seq ${2:-30}); do T | grep -q "$1" && return 0; sleep 1; done; return 1; }
waitser() { for i in $(seq ${3:-60}); do [ "$(tr -d '\r' < $SER | grep -c "$1")" -ge "${2:-1}" ] && return 0; sleep 1; done; return 1; }
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
[ -f $V/4_CD_USB/RD2_CD.ISO ] || bash tools/mkverteil.sh >/dev/null || exit 1
platte() { HD=obj/vt_hd.img; bash tools/mkhd.sh $HD >/dev/null; dd if=test/daten/mbr_rdfdisk.bin of=$HD conv=notrunc 2>/dev/null
  printf 'x' > obj/io.tmp; mcopy -i $HD@@32256 obj/io.tmp ::/IO.SYS; }   # Platte hat schon ein DOS: kein SYS
start() { rm -f $S $SER; touch $SER
  timeout 300 qemu-system-i386 -cpu pentium -m 16 -display none -no-reboot $1 -drive file=$HD,format=raw,if=ide,index=0 \
    -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 & QP=$!
  waitscr "Erster Start" 90; sleep 1; Q "sendkey i"; waitser "INSTALL: Start" 1 90 && { sleep 3; Q "sendkey ret"; }; }
pruef() {   # $1 Name
  waitser "ist installiert" 1 120 && { sleep 1; Q "sendkey ret"; }
  ok "$1: Kopieren ohne Fehler" "tr -d '\r' < $SER | grep -q 'INSTALL: Fehler 0x00000000'"
  ok "$1: keine Frage nach Diskette 5" "! tr -d '\r' < $SER | grep -q 'Diskette 5 (RD2'"
  sleep 2; Q quit; wait $QP 2>/dev/null; local m=""
  for f in RD2/RD2.EXE RD2/SHELL.EXE RD2/RDFDISK.EXE RD2/RDNET.EXE RD2/RDFTP.EXE RD2/RD32.MOD RD2/CWSDPMI.EXE RD2/WUERFEL.EXE RD2/SDK/RD2.H RD2/SDK/RD2.LIB RD2/SDK32/RD2_32.H RD2/SDK32/RD32MOD.EXE CONFIG.SYS; do
    mdir -i $HD@@32256 ::/$f >/dev/null 2>&1 || m="$m $f"; done
  ok "$1: alles auf C: (RD2, Netz, RD2/32, SDK, SDK32)${m:+ - fehlt:$m}" "[ -z '$m' ]"
  ok "$1: CONFIG.SYS mit RD2 als Shell und CWSDPMI" "mtype -i $HD@@32256 ::/CONFIG.SYS | tr -d '\r' | grep -q 'SHELL=C:.RD2.RD2.EXE' && mtype -i $HD@@32256 ::/CONFIG.SYS | grep -q CWSDPMI"
}
echo "=== 1,44-MB-Satz (Pentium)"
platte; cp $V/2_386_144MB/RD2_386A.IMG obj/vt_a.img; cp $V/2_386_144MB/RD2_386B.IMG obj/vt_b.img
start "-drive file=obj/vt_a.img,format=raw,if=floppy,index=0 -boot a"
if waitser "Diskette 2 (RD2 386 2)" 1 150; then echo "ok     Meldung: Diskette 2 (RD2 386 2)"; Q "change floppy0 obj/vt_b.img"; sleep 2; Q "sendkey ret"
else echo "FEHLER keine Meldung fuer Diskette 2 (RD2 386 2)"; F=1; fi
if waitser "Diskette 1 (RD2SYSTEM)" 1 150; then echo "ok     Meldung: Diskette 1 wieder einlegen"; Q "change floppy0 obj/vt_a.img"; sleep 2; Q "sendkey ret"
else echo "FEHLER keine Meldung fuer Diskette 1"; F=1; fi
pruef "1,44 MB"
echo "=== CD (2,88-MB-Abzug, schreibgeschuetzt)"
platte; start "-cdrom $V/4_CD_USB/RD2_CD.ISO -boot d"
ok "CD: kein Diskettenwechsel" "! tr -d '\r' < $SER | grep -q 'Diskette [0-9] (RD2'"
pruef "CD"
echo "=== USB-Abbild"
cp $V/4_CD_USB/RD2_USB.IMG obj/vt_usb.img; rm -f $S; timeout 120 qemu-system-i386 -cpu pentium -m 16 -display none -no-reboot -drive file=obj/vt_usb.img,format=raw,if=ide,index=0 -boot c -monitor unix:$S,server,nowait >/dev/null 2>&1 & QP=$!
ok "USB: startet von C: bis zur Einrichtung" "waitscr 'Erster Start' 90"
Q quit; wait $QP 2>/dev/null
[ $F = 0 ] && echo "Verteilformen: alles ok"; exit $F
