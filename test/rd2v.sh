#!/bin/bash
# PLAN_V86 Teil 1a: RD2V als residenter DPMI-Host in QEMU (386+). RD2V.COM laden, DPMITEST zweimal
# (Wiedereinstieg nach dem Ende), dazwischen DPMITEST A (HLT in Ring 3: nur der Client endet), dann zweites RD2V (muss "schon ein DPMI-Host" melden). Ausgabe ueber COM1.
cd "$(dirname "$0")/.."
SER=obj/rd2v_com1.txt; IMG=obj/rd2v_a.img; F=0
nasm -f bin -o obj/RD2V.COM src/rd2v/RD2V.ASM || exit 1
nasm -f bin -o obj/DPMITEST.COM test/dos/DPMITEST.ASM || exit 1
printf '@echo off\r\nA:\\RD2V.COM\r\nA:\\DPMITEST.COM\r\nA:\\DPMITEST.COM A\r\nA:\\DPMITEST.COM\r\nA:\\RD2V.COM\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_rd2v.txt
FMT=720 NUR=" " bash tools/mkdisk.sh $IMG obj/ae_rd2v.txt >/dev/null 2>&1
mcopy -o -i $IMG obj/RD2V.COM obj/DPMITEST.COM ::/
rm -f $SER; touch $SER
timeout 90 qemu-system-i386 -m 4 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER >/dev/null 2>&1
tr -d '\r' < $SER > obj/rd2v_com1_klar.txt
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
n() { grep -c "$1" obj/rd2v_com1_klar.txt; }
ok "RD2V geladen" "[ $(n 'RD2V: DPMI-Host geladen') = 1 ]"
for z in "Version 0.90" "INT 12h gespiegelt" "unbekannte Funktion abgelehnt" "Ende"; do
  ok "zweimal: $z" "[ $(n "DPMITEST: $z") = 2 ]"
done
ok "dreimal Schutzmodus und Ring 3" "[ $(n 'DPMITEST: Ring 3') = 3 ]"
ok "Ausnahme beendet nur den Client" "[ $(n 'RD2V: Ausnahme, Client beendet') = 1 ] && grep -A1 'HLT in Ring 3' obj/rd2v_com1_klar.txt | grep -q 'Ausnahme'"
ok "danach wieder nutzbar" "tail -3 obj/rd2v_com1_klar.txt | grep -q 'DPMITEST: Ende\\|schon ein'"
ok "kein FEHLER" "[ $(n 'FEHLER') = 0 ]"
ok "zweites RD2V abgelehnt" "[ $(n 'schon ein DPMI-Host') = 1 ]"
[ $F = 0 ] && echo "rd2v.sh: alles ok" || { echo "rd2v.sh: FEHLER"; cat obj/rd2v_com1_klar.txt | tail -15; }
exit $F
