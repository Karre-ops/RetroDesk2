#!/bin/bash
# Ansicht > Nach Typ (SHELL): Ordner A:\ nach Endung sortiert (erster/letzter Eintrag gegen mtools), dann Pfeiltasten:
# nur alte und neue Zelle werden neu gezeichnet. Pruefung: Bild nach den Pfeilen = Bild nach vollem Neuzeichnen
# (Ansicht Liste und zurueck), ohne Menueleiste (Uhr). Danach Nach Name zurueck.
cd "$(dirname "$0")/.."
IMG=obj/sort.img; SER=obj/sort_com1.txt; S=/tmp/qmon_sort.sock; export QMON=$S; F=0
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_sort.txt
bash tools/mkdisk.sh $IMG obj/ae_sort.txt >/dev/null 2>&1
for f in AAA.ZZZ BBB.ABC MMM.TXT; do echo x > obj/$f; mcopy -o -i $IMG obj/$f ::/; done
ERST=RD2; LETZT=AAA.ZZZ                         # einziger Ordner vorn; Endung ZZZ ist die letzte
rm -f $SER $S; touch $SER
timeout 130 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 250); do c=$(tr -d '\r' < $SER | grep -c "$1"); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
waitn "SHELL: bereit"; sleep 3; Q "sendkey ret"; waitn "SHELL: Ordner A:"; sleep 2
Q "sendkey f10"; sleep 0.6; Q "sendkey kp_6"; sleep 0.8; Q "sendkey t"
if waitn "SHELL: Sortierung 0x00000001"; then
  sleep 0.5; L=$(tr -d '\r' < $SER | grep -A2 "Sortierung 0x00000001" | tail -2 | tr '\n' ' ')
  [ "$L" = "$ERST $LETZT " ] && echo "ok     nach Typ: erster $ERST, letzter $LETZT" || { echo "FEHLER nach Typ: $L (soll $ERST $LETZT)"; F=1; }
else echo "FEHLER keine Sortierung"; F=1; fi
sleep 1; Q "sendkey kp_6" sleep0.4 "sendkey kp_6" sleep0.4 "sendkey kp_2" sleep0.4 "sendkey kp_4"; sleep 1.5; Q "screendump obj/sort_pfeil.ppm"
Q "sendkey f10"; sleep 0.6; Q "sendkey kp_6"; sleep 0.8; Q "sendkey l"; sleep 1.5; Q "sendkey f10"; sleep 0.6; Q "sendkey kp_6"; sleep 0.8; Q "sendkey s"; sleep 2
Q "screendump obj/sort_voll.ppm"
python3 -c "
from PIL import Image, ImageChops
a=Image.open('obj/sort_pfeil.ppm').convert('RGB'); b=Image.open('obj/sort_voll.ppm').convert('RGB')
w,h=a.size; bb=ImageChops.difference(a.crop((0,20,w,h)),b.crop((0,20,w,h))).getbbox(); a.save('obj/sort_pfeil.png')
print('ok     Pfeiltasten: Bild gleich dem vollen Neuzeichnen' if bb is None else 'FEHLER Pfeiltasten-Bild weicht ab %s' % (bb,))" | tee /tmp/sort_cmp.txt
grep -q FEHLER /tmp/sort_cmp.txt && F=1
Q "sendkey f10"; sleep 0.6; Q "sendkey kp_6"; sleep 0.8; Q "sendkey a"
waitn "SHELL: Sortierung 0x00000000" && echo "ok     zurueck nach Name" || { echo "FEHLER nach Name"; F=1; }
Q "quit"; wait $QP 2>/dev/null
[ $F = 0 ] && echo "Sortieren: alles ok"; exit $F
