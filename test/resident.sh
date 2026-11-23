#!/bin/bash
# Startart in der Shell (PLAN_DOS 3), QEMU mit Festplatte C: (RD2 in C:\RD2, Auslagern in eine Datei):
# 1. TSRTEST.EXE (bleibt resident) aus C:\ starten: Startart Schnell, der Rest entfernt es, die Shell fragt, Ja
#    -> RD2.INI [Startart] C:\TSRTEST.EXE=Voll
# 2. nochmal starten: jetzt allein (RD2 endet, TSR bleibt, RD2 startet neu, keine Entfernen-Meldung mehr)
# 3. Eigenschaften (Alt+Eingabe) zeigen die Startart, Automatisch waehlen -> Eintrag leer
cd "$(dirname "$0")/.."
IMG=obj/res.img; HD=obj/res_hd.img; SER=obj/res_com1.txt; S=/tmp/qmon_res.sock; export QMON=$S; F=0; P=$HD@@32256
printf '@echo off\nC:\\RD2\\RD2.EXE /NOMAUS /VGA C:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_res.txt
bash tools/mkdisk.sh $IMG obj/ae_res.txt >/dev/null 2>&1; bash tools/mkhd.sh $HD >/dev/null
python3 -c "import shutil; shutil.rmtree('obj/res_rd2', True)"; mkdir -p obj/res_rd2
mcopy -n -i $IMG "::/RD2/*" obj/res_rd2/ && mmd -i $P ::/RD2 && mcopy -i $P obj/res_rd2/* ::/RD2/ && mcopy -i $P out/TSRTEST.EXE ::/ && printf '[System]\r\n' > obj/res_rd2.ini && mcopy -o -i $P obj/res_rd2.ini ::/RD2/RD2.INI || { echo "FEHLER Festplatte"; exit 1; }
rm -f $SER $S
timeout 240 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -drive file=$HD,format=raw,if=ide,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq ${3:-300}); do c=$(tr -d '\r' < $SER 2>/dev/null | grep -c "$1"); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
ok() { if waitn "$1" ${3:-1} ${4:-300}; then echo "ok     $2"; else echo "FEHLER $2"; tr -d '\r' < $SER | tail -3 | sed 's/^/       /'; F=1; fi; }
ini() { mtype -i $P ::/RD2/RD2.INI 2>/dev/null | tr -d '\r'; }
oeffne_c() { waitn "SHELL: bereit" $1 400; sleep 3; Q "sendkey down"; sleep 0.5; Q "sendkey ret"; waitn 'SHELL: Ordner C:\\ ' $1 100; sleep 2; }
oeffne_c 1
ok 'SHELL: Ordner C:\\ ' "Shell oeffnet C:"
Q "sendkey t"; sleep 0.5; Q "sendkey ret"
ok "SHELL: resident" "TSRTEST: Shell fragt nach der Startart" 1 400
sleep 2; Q "screendump obj/res1.ppm"; Q "sendkey j"; sleep 3
Q "sendkey t"; sleep 0.5; Q "sendkey ret"
oeffne_c 2
ok "SHELL: bereit" "zweiter Start: RD2 endet, TSRTEST laeuft allein, RD2 startet neu" 2
ini | grep -qi '^C:\\TSRTEST.EXE=Voll' && echo "ok     RD2.INI [Startart] (vor dem Start geschrieben) C:\\TSRTEST.EXE=Voll" || { echo "FEHLER RD2.INI ohne Startart"; ini | grep -i -A2 startart | sed 's/^/       /'; F=1; }
[ "$(tr -d '\r' < $SER | grep -c 'SHELL: resident')" = 1 ] && echo "ok     allein: TSR durfte bleiben, keine zweite Rueckfrage" || { echo "FEHLER zweite Rueckfrage"; F=1; }
Q "sendkey t"; sleep 0.5; Q "sendkey alt-ret"
ok "SHELL: Eigenschaften" "Eigenschaften von TSRTEST.EXE" 1 50
sleep 2; Q "screendump obj/res2.ppm"; Q "sendkey alt-t"; sleep 0.5; Q "sendkey ret"
ok "SHELL: Startart Auto" "Eigenschaften: Startart Automatisch gewaehlt" 1 50
sleep 1; Q "sendkey alt-f4"; sleep 1; Q "sendkey alt-f4"; sleep 1; Q "sendkey j"; waitn "RD2: Speicher" 1 100; sleep 2
ini | grep -qi '^C:\\TSRTEST.EXE=$' && echo "ok     RD2.INI: Eintrag wieder leer (automatisch)" || { echo "FEHLER RD2.INI nach Eigenschaften"; ini | grep -i -A2 startart | sed 's/^/       /'; F=1; }
kill $QP 2>/dev/null; wait $QP 2>/dev/null
for b in res1 res2; do [ -f obj/$b.ppm ] && python3 -c "from PIL import Image; Image.open('obj/$b.ppm').save('obj/$b.png')"; done
[ $F = 0 ] && echo "Startart Shell: alles ok"; exit $F
