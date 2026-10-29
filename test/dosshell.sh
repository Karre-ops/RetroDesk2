#!/bin/bash
# RD2 als Shell beenden (29.10.2026): statt "RetroDesk ist beendet" startet COMMAND.COM (hier FreeCOM auf C:\),
# EXIT holt RD2 zurueck. Braucht obj/satz_hd.img aus test/satz.sh (installierte Platte), setzt das aktuelle RD.EXE ein.
cd "$(dirname "$0")/.."
F=0; S=/tmp/qmon_dsh.sock; export QMON=$S
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
T() { timeout 10 python3 test/qemu/qmon.py text 2>/dev/null; }
waitscr() { for i in $(seq ${2:-30}); do T | grep -q "$1" && return 0; sleep 1; done; return 1; }
gfx() { for i in $(seq ${1:-60}); do sleep 1; Q "screendump obj/dsh.ppm"; python3 -c "from PIL import Image; import sys; sys.exit(0 if Image.open('obj/dsh.ppm').size==(640,480) else 1)" 2>/dev/null && return 0; done; return 1; }
[ -f obj/satz_hd.img ] || { echo "FEHLER obj/satz_hd.img fehlt (erst test/satz.sh)"; exit 1; }
HD=obj/dsh_hd.img; cp obj/satz_hd.img $HD; P=$HD@@32256
mcopy -o -i $P out/RD.EXE ::/RD2/RD.EXE
mcopy -i test/qemu/boot.img ::/FREEDOS/BIN/COMMAND.COM /tmp/dsh_cmd.com && mcopy -o -i $P /tmp/dsh_cmd.com ::/COMMAND.COM
rm -f $S
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$HD,format=raw,if=ide,index=0 -boot c \
  -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
gfx 60 && echo "ok     startet als Shell bis zur Oberflaeche" || { echo "FEHLER keine Oberflaeche"; F=1; }
sleep 4; Q "sendkey alt-spc" sleep1 "sendkey b" sleep1.5 "sendkey ret"
waitscr "EXIT startet RetroDesk 2 wieder" 30 && echo "ok     Beenden: Hinweis EXIT" || { echo "FEHLER kein Hinweis"; T | grep -v "^ *$" | tail -5; F=1; }
sleep 4; T | grep -v "^ *$" | tail -3 > obj/dsh_prompt.txt
grep -q 'C:\\.*>' obj/dsh_prompt.txt && echo "ok     DOS-Eingabe (COMMAND.COM) da" || { echo "FEHLER keine DOS-Eingabe"; cat obj/dsh_prompt.txt; F=1; }
Q "sendkey e" "sendkey x" "sendkey i" "sendkey t" "sendkey ret"
gfx 60 && echo "ok     EXIT: RD2 kommt zurueck" || { echo "FEHLER RD2 nach EXIT"; T | grep -v "^ *$" | tail -4; F=1; }
Q quit; wait $QP 2>/dev/null
[ $F = 0 ] && echo "DOS-Eingabe nach Beenden: alles ok"
