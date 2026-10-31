#!/bin/bash
# Optionen > Tastatur (31.10.2026): Commodore PC10 waehlen -> laufendes RDKEYB bekommt PC10.KBD sofort (KEYTEST: # auf 29h,
# < auf 2Bh), RDKEYB-Zeile der CONFIG.SYS (Testordner T1) zeigt auf PC10.KBD, RD2.INI Tastatur=PC10. Danach zurueck auf Deutsch.
cd "$(dirname "$0")/.."
IMG=obj/optkbd.img; SER=obj/optkbd_com1.txt; S=/tmp/qmon_optkbd.sock; export QMON=$S; F=0
export WATCOM=${WATCOM:-/opt/ow} PATH=/opt/ow/binl64:/opt/ow/binl:$PATH INCLUDE=/opt/ow/h
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*) kill ${p#/proc/} 2>/dev/null;; esac; done
(cd obj && wasm -q -0 -fo=keytest.obj ../test/KEYTEST.ASM && wlink option quiet format dos com name keytest.com file keytest.obj) || exit 1
cat > obj/ae_optkbd.txt <<'AE'
@echo off
A:\RD2\RDKEYB.COM DE
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\OPTIONEN.EXE T A:\T1\
A:\KEYTEST.COM
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\OPTIONEN.EXE T A:\T1\
A:\KEYTEST.COM
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_optkbd.txt >/dev/null 2>&1
for f in RDMAUS.COM RDFORMAT.EXE RDFDISK.EXE RDSYS.EXE RD2SETUP.EXE; do mdel -i $IMG ::/RD2/$f 2>/dev/null; done
mkdir -p obj/optkbd; printf 'SHELL=A:\\RD2\\RD.EXE /P A:\\RD2\\SHELL.EXE\r\nINSTALL=A:\\RD2\\RDKEYB.COM DE\r\n' > obj/optkbd/CONFIG.SYS
mmd -i $IMG ::/T1; mcopy -i $IMG obj/optkbd/CONFIG.SYS ::/T1/; mcopy -o -i $IMG obj/keytest.com ::/KEYTEST.COM
rm -f $SER $S
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 400); do c=$(grep -c "$1" $SER 2>/dev/null); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
zeichen() { grep -o "K [0-9A-F]*" $SER | cut -c3- | awk -v n=$1 '$0=="011B"{b++; next} b==n{print substr($0,3,2)}' | tr '\n' ' ' | sed 's/ $//'; }
waitn "OPTIONEN: bereit" 1 || { echo "FEHLER Optionen 1 startet nicht"; F=1; }; sleep 3
Q "sendkey alt-4" sleep0.8 "sendkey alt-c" sleep0.5 "screendump obj/optkbd_vga.ppm" sleep0.3 "sendkey ret"
waitn "KEYTEST: bereit" 1 || { echo "FEHLER KEYTEST 1"; F=1; }
for k in grave_accent backslash equal esc; do Q "sendkey $k"; sleep 0.3; done
waitn "OPTIONEN: bereit" 2 || { echo "FEHLER Optionen 2 startet nicht"; F=1; }; sleep 3
mcopy -o -i $IMG ::/T1/CONFIG.SYS obj/optkbd/neu.sys 2>/dev/null; mcopy -o -i $IMG ::/RD2/RD2.INI obj/optkbd/neu.ini 2>/dev/null
Q "sendkey alt-4" sleep0.8 "sendkey alt-d" sleep0.5 "sendkey ret"
waitn "KEYTEST: bereit" 2 || { echo "FEHLER KEYTEST 2"; F=1; }
for k in grave_accent a backslash esc; do Q "sendkey $k"; sleep 0.3; done
wait $QP 2>/dev/null
ist=$(zeichen 0); [ "$ist" = "23 3C 27" ] && echo "ok     Optionen PC10: sofort aktiv (# < ')" || { echo "FEHLER PC10 nicht aktiv: $ist"; F=1; }
grep -q "^INSTALL=A:.RD2.RDKEYB.COM A:.RD2.PC10.KBD" <(tr -d '\r' < obj/optkbd/neu.sys) && echo "ok     CONFIG.SYS: RDKEYB.COM A:\\RD2\\PC10.KBD" || { echo "FEHLER CONFIG.SYS"; cat obj/optkbd/neu.sys; F=1; }
tr -d '\r' < obj/optkbd/neu.ini | grep -qi "^Tastatur=PC10" && echo "ok     RD2.INI Tastatur=PC10" || { echo "FEHLER RD2.INI"; F=1; }
ist=$(zeichen 1); [ "$ist" = "83 23" ] && echo "ok     zurueck auf Deutsch: sofort aktiv (^a = â, #)" || { echo "FEHLER DE nicht aktiv: $ist"; F=1; }
mcopy -o -i $IMG ::/T1/CONFIG.SYS obj/optkbd/neu2.sys; tr -d '\r' < obj/optkbd/neu2.sys | grep -q "^INSTALL=A:.RD2.RDKEYB.COM DE$" && echo "ok     CONFIG.SYS wieder RDKEYB.COM DE" || { echo "FEHLER CONFIG.SYS DE"; cat obj/optkbd/neu2.sys; F=1; }
python3 test/bildvergleich.py obj/optkbd_vga.ppm test/ref/optkbd_vga.png | grep -q "^ok" && echo "ok     Bild Karte Tastatur (3 Belegungen)" || { echo "FEHLER Bild Karte Tastatur"; F=1; }
[ $F = 0 ] && echo "Optionen Tastatur: alles ok"
exit $F
