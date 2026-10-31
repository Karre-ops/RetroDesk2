#!/bin/bash
# RDKEYB mit Belegungsdatei (31.10.2026): PC10.KBD wie aufgedruckt (# auf 29h, <> auf 2Bh, keine Tottasten,
# AltGr = Strg+Alt), danach lebend zurueck auf DE (zweiter Aufruf kopiert die Tabelle), fehlende Datei -> DE.
cd "$(dirname "$0")/.."
IMG=obj/tkbd.img; SER=obj/tkbd_com1.txt; S=/tmp/qmon_tkbd.sock; export QMON=$S; F=0
export WATCOM=${WATCOM:-/opt/ow} PATH=/opt/ow/binl64:/opt/ow/binl:$PATH INCLUDE=/opt/ow/h
(cd obj && wasm -q -0 -fo=keytest.obj ../test/KEYTEST.ASM && wlink option quiet format dos com name keytest.com file keytest.obj) || exit 1
cat > obj/ae_tkbd.txt <<'AE'
@echo off
A:\RD2\RDKEYB.COM A:\RD2\PC10.KBD > A:\K1.TXT
A:\KEYTEST.COM
A:\RD2\RDKEYB.COM DE > A:\K2.TXT
A:\KEYTEST.COM
A:\RD2\RDKEYB.COM A:\NIX > A:\K3.TXT
A:\FDAPM.COM POWEROFF
AE
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ')" FMT=720 bash tools/mkdisk.sh $IMG obj/ae_tkbd.txt >/dev/null 2>&1
mcopy -o -i $IMG out/RDKEYB.COM out/PC10.KBD ::/RD2/; mcopy -o -i $IMG obj/keytest.com ::/KEYTEST.COM
rm -f $SER $S
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 300); do c=$(grep -c "$1" $SER 2>/dev/null); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
zeichen() { grep -o "K [0-9A-F]*" $SER | cut -c3- | awk -v n=$1 '$0=="011B"{b++; next} b==n{print substr($0,3,2)}' | tr '\n' ' ' | sed 's/ $//'; }
waitn "KEYTEST: bereit" || { echo "FEHLER KEYTEST startet nicht"; F=1; }
for k in grave_accent shift-grave_accent ctrl-alt-grave_accent backslash shift-backslash ctrl-alt-backslash minus ctrl-alt-minus \
  equal shift-equal ctrl-alt-equal ctrl-alt-2 ctrl-alt-4 ctrl-alt-7 ctrl-alt-apostrophe ctrl-alt-bracket_left ctrl-alt-bracket_right \
  shift-3 z equal e esc; do Q "sendkey $k"; sleep 0.3; done
waitn "KEYTEST: bereit" 2 || { echo "FEHLER zweites KEYTEST startet nicht"; F=1; }
for k in grave_accent a backslash z esc; do Q "sendkey $k"; sleep 0.3; done
wait $QP 2>/dev/null
erw="23 5E 7D 3C 3E 5C E1 2F 27 60 7E 40 F8 7C 7B 5B 5D 15 79 27 65"; ist=$(zeichen 0)
[ "$ist" = "$erw" ] && echo "ok     PC10.KBD: # ^ } < > \\ ß / ' \` ~ @ ° | { [ ] § y/z, keine Tottaste" || { echo "FEHLER PC10-Belegung"; echo "  soll: $erw"; echo "  ist:  $ist"; F=1; }
erw="83 23 79"; ist=$(zeichen 1)
[ "$ist" = "$erw" ] && echo "ok     RDKEYB DE schaltet lebend zurueck (^ ist wieder Tottaste, # auf 2Bh)" || { echo "FEHLER Rueckschalten DE"; echo "  soll: $erw"; echo "  ist:  $ist"; F=1; }
mtype -i $IMG ::/K1.TXT | grep -q "geladen: A:.RD2.PC10.KBD" && echo "ok     Meldung: geladen: A:\\RD2\\PC10.KBD" || { echo "FEHLER Meldung K1"; mtype -i $IMG ::/K1.TXT; F=1; }
mtype -i $IMG ::/K3.TXT | grep -q "NIX.KBD nicht lesbar" && echo "ok     fehlende Datei: Hinweis, DE bleibt" || { echo "FEHLER Meldung K3"; mtype -i $IMG ::/K3.TXT; F=1; }
[ $F = 0 ] && echo "Tastatur mit Belegungsdatei: alles ok"
exit $F
