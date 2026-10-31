#!/bin/bash
# Notausgang (31.10.2026, Karsten): Umschalttaste beim Start von RD2 als Shell -> COMMAND.COM statt RD2, EXIT startet RD2.
# Braucht obj/satz_hd.img aus test/satz.sh, setzt das aktuelle RD.EXE und FreeCOM als C:\COMMAND.COM ein.
cd "$(dirname "$0")/.."
F=0; S=/tmp/qmon_nta.sock; export QMON=$S
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
T() { timeout 10 python3 test/qemu/qmon.py text 2>/dev/null; }
gfx() { for i in $(seq ${1:-60}); do sleep 1; Q "screendump obj/nta.ppm"; python3 -c "from PIL import Image; import sys; sys.exit(0 if Image.open('obj/nta.ppm').size==(640,480) else 1)" 2>/dev/null && return 0; done; return 1; }
[ -f obj/satz_hd.img ] || { echo "FEHLER obj/satz_hd.img fehlt (erst test/satz.sh)"; exit 1; }
HD=obj/nta_hd.img; cp obj/satz_hd.img $HD; P=$HD@@32256
mcopy -o -i $P out/RD.EXE ::/RD2/RD.EXE
mcopy -i test/qemu/boot.img ::/FREEDOS/BIN/COMMAND.COM /tmp/nta_cmd.com && mcopy -o -i $P /tmp/nta_cmd.com ::/COMMAND.COM
rm -f $S
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$HD,format=raw,if=ide,index=0 -boot c \
  -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
( for i in $(seq 60); do Q "sendkey shift 3000"; sleep 0.5; done ) & SP=$!
ok=0; for i in $(seq 40); do sleep 1; T | grep -q "EXIT startet RetroDesk 2 wieder" && { ok=1; break; }; done
[ $ok = 1 ] && echo "ok     Umschalt beim Start: Hinweis EXIT statt Oberflaeche" || { echo "FEHLER kein Notausgang"; T | grep -v "^ *$" | tail -4; F=1; }
kill $SP 2>/dev/null; sleep 4; T | grep -v "^ *$" | tail -3 > obj/nta_prompt.txt
grep -q 'C:\\.*>' obj/nta_prompt.txt && echo "ok     DOS-Eingabe (COMMAND.COM) da" || { echo "FEHLER keine DOS-Eingabe"; cat obj/nta_prompt.txt; F=1; }
Q "sendkey e" "sendkey x" "sendkey i" "sendkey t" "sendkey ret"
gfx 60 && echo "ok     EXIT: RD2 startet" || { echo "FEHLER RD2 nach EXIT"; F=1; }
Q quit; wait $QP 2>/dev/null
[ $F = 0 ] && echo "Notausgang: alles ok"; exit $F
