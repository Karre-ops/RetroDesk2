#!/bin/bash
# PLAN_API Luecke 3: Mindestausbau 512 KB. KB512.COM (test/m512) laesst DOS nur 512 KB (wie ein PC mit 512 KB, DOS-Kern
# unten wie dort). Dann nacheinander: PING (4 Programme, Nachrichten, Zeitscheiben), STRESS (5 Wuermer mit Speicher),
# MIXTEST (Sound Blaster mit Mischer), Schreibtisch mit SHELL. Jeder Lauf muss den Speicher vollstaendig zurueckgeben.
cd "$(dirname "$0")/.."
IMG=obj/min512.img; SER=obj/min512_com1.txt; F=0
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
nasm -f bin -o obj/KB512.COM test/m512/KB512.ASM || exit 1
printf '@echo off\r\nA:\\KB512.COM\r\nA:\\RD2\\RD2.EXE /NOMAUS /CGA A:\\RD2\\PING.EXE\r\nA:\\RD2\\RD2.EXE /NOMAUS /CGA A:\\RD2\\STRESS.EXE\r\nA:\\RD2\\RD2.EXE /NOMAUS /CGA A:\\RD2\\MIXTEST.EXE 6\r\nA:\\RD2\\RD2.EXE /NOMAUS /CGA A:\\RD2\\SHELL.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_min512.txt
FMT=1440 NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') PING.EXE PONG.EXE RECHNE.EXE MEMTEST.EXE STRESS.EXE WURM.EXE MIXTEST.EXE SB.DRV RDMIX.MOD" bash tools/mkdisk.sh $IMG obj/ae_min512.txt >/dev/null 2>&1
mcopy -o -i $IMG obj/KB512.COM ::/
printf '[System]\r\nSound=SB\r\nSB=A220 I5 D1 H5\r\n' > obj/min512_ini.txt; mcopy -o -i $IMG obj/min512_ini.txt ::/RD2/RD2.INI
rm -f $SER; touch $SER
timeout 240 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -audiodev none,id=a0 -device sb16,audiodev=a0 -serial file:$SER >/dev/null 2>&1 &
QP=$!
for i in $(seq 600); do grep -aq "SHELL: bereit" $SER && break; sleep 0.3; done; sleep 3; kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
FR=$(echo "$L" | grep -a "RD2: Speicher frei" | head -1 | sed 's/.*0x//'); echo "       RD2 meldet beim Start frei: $((16#${FR:-0})) Bytes"
pruef "nur 512 KB: RD2 sieht unter 470 KB frei (DOS-Kern belegt den Rest)" "[ $((16#${FR:-0})) -lt 480000 ] && [ $((16#${FR:-0})) -gt 100000 ]"
pruef "PING: 5 Antworten (Multitasking mit 4 Programmen)" "echo \"\$L\" | grep -q 'PING: Antwort 0x00000005'"
pruef "STRESS: alle Wuermer ok" "echo \"\$L\" | grep -q 'STRESS: alle Wuermer ok'"
pruef "MIXTEST: Mischer ohne Fehler" "echo \"\$L\" | grep -q 'MIXTEST: fertig' && echo \"\$L\" | grep -q 'MIXTEST: Fehler 0x00000000'"
pruef "Schreibtisch (SHELL) startet" "echo \"\$L\" | grep -q 'SHELL: bereit'"
pruef "Speicher nach PING, STRESS, MIXTEST vollstaendig zurueck" "[ \$(echo \"\$L\" | grep -c 'RD2: Speicher vollstaendig zurueck') -ge 3 ]"
echo "$L" | grep -aE "FEHLER|NOMEM|zu wenig" | head -5 | sed 's/^/       /'
[ $F = 0 ] && echo "min512: alle ok" || echo "min512: FEHLER"; exit $F
