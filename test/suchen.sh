#!/bin/bash
# Dateien suchen (SHELL, Strg+F): ab A:\ nach *.DRV und RD?.EXE, Trefferzahl gegen mtools, "Ordner oeffnen" -> A:\RD2\.
cd "$(dirname "$0")/.."
IMG=obj/such.img; SER=obj/such_com1.txt; S=/tmp/qmon_such.sock; export QMON=$S; F=0
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_such.txt
bash tools/mkdisk.sh $IMG obj/ae_such.txt >/dev/null 2>&1
n1=$(mdir -i $IMG -/ -b :: 2>/dev/null | grep -ci '\.drv$'); n2=$(mdir -i $IMG -/ -b :: 2>/dev/null | grep -ciE '/RD.?\.EXE$')
rm -f $SER $S; touch $SER
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 250); do c=$(tr -d '\r' < $SER | grep -c "$1"); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
tipp() { local s=$1 i c; for ((i = 0; i < ${#s}; i++)); do c=${s:i:1}; case $c in .) c=dot;; '*') c=shift-8;; '?') c=shift-slash;; *) c=$(echo $c | tr A-Z a-z);; esac; Q "sendkey $c"; sleep 0.15; done; }
such() { Q "sendkey ctrl-f"; sleep 1.5; for i in 1 2 3 4 5 6 7 8 9 10 11 12; do Q "sendkey backspace"; sleep 0.1; done; tipp "$1"; Q "sendkey ret"; }
waitn "SHELL: bereit"; sleep 3; Q "sendkey ret"; waitn 'SHELL: Ordner A:\\ '; sleep 3
such "*.DRV"
if waitn "^$n1 Treffer in A:" ; then echo "ok     *.DRV: $n1 Treffer wie auf der Diskette"; else echo "FEHLER *.DRV (soll $n1)"; tr -d '\r' < $SER | grep -A1 "SHELL: Suche" | tail -2; F=1; fi
sleep 1; Q "sendkey esc"; sleep 1
such "RD?.EXE"
if waitn "^$n2 Treffer in A:"; then echo "ok     RD?.EXE: $n2 Treffer (? = ein Zeichen)"; else echo "FEHLER RD?.EXE (soll $n2)"; tr -d '\r' < $SER | grep -A1 "SHELL: Suche" | tail -2; F=1; fi
sleep 1; Q "screendump obj/such1.ppm"; Q "sendkey ret"
if waitn "SHELL: Ordner A:\\\\RD2\\\\ "; then echo "ok     Ordner oeffnen zeigt A:\\RD2\\"; else echo "FEHLER Ordner oeffnen"; F=1; fi
Q "quit"; wait $QP 2>/dev/null
[ -f obj/such1.ppm ] && python3 -c "from PIL import Image; Image.open('obj/such1.ppm').save('obj/such1.png')"
[ $F = 0 ] && echo "Suchen: alles ok"; exit $F
