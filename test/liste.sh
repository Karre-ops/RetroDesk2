#!/bin/bash
# Listenansicht (Checkliste Punkt 3, VGA): Ordner A:\ oeffnen, Ansicht > Liste (F10, rechts, L), Bild, Pfeil runter,
# Bild, zurueck auf Symbole. Prueft: Sortierung (Ordner vorn, dann Namen; erster und letzter Eintrag im Protokoll
# gegen mdir), Haken im Menue umgeschaltet, Bildvergleich obj/liste_{auf,runter}.ppm.
cd "$(dirname "$0")/.."
IMG=obj/liste.img; SER=obj/liste_com1.txt; SOCK=/tmp/qmon_liste.sock
printf '@echo off\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_liste.txt
bash tools/mkdisk.sh $IMG obj/ae_liste.txt >/dev/null
rm -f $SER $SOCK obj/liste_*.ppm; touch $SER
timeout 150 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_liste.py
Q() { python3 obj/qmon_liste.py "$@"; }
waitn() { for i in $(seq 400); do [ "$(tr -d '\r' < $SER | grep -c "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
ok=1
waitn "SHELL: bereit" 1 && sleep 3 && Q "sendkey ret"
waitn 'SHELL: Ordner A:\\ ' 1 && sleep 2 && Q "sendkey f10" sleep0.6 "sendkey kp_6" sleep0.8 "screendump obj/liste_menu.ppm" "sendkey l"
waitn "SHELL: Ansicht 0x00000001" 1 && sleep 2 && Q "screendump obj/liste_auf.ppm" "sendkey kp_2" sleep1 "sendkey kp_2" sleep1.5 "screendump obj/liste_runter.ppm" "sendkey f10" sleep0.6 "sendkey kp_6" sleep0.8 "sendkey s"
waitn "SHELL: Ansicht 0x00000000" 1 || ok=0
sleep 1.5; Q "sendkey alt-f4" sleep1.5 "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
waitn "SHELL: Ende" 1 || ok=0
wait $QP
L=$(tr -d '\r' < $SER)
echo "$L" | grep -A2 "SHELL: Ordner" | head -3; echo "$L" | grep -E "SHELL: (Ansicht|Ende)|RD: (Speicher voll|Fehler)"
# erwartet: Ordner vorn, dann Dateien, je nach Namen
mapfile -t N < <(echo "$L" | grep -B2 -m1 'SHELL: Ordner A:\\ ' | sed -n 1,2p)   # Shell meldet ersten und letzten Eintrag vor "Ordner"
DIRS=$(mdir -i $IMG ::/ | awk '$2=="<DIR>"{print $1}' | LC_ALL=C sort)
FILES=$(mdir -i $IMG ::/ | awk 'NF>=4 && $2!="<DIR>" && $1!="Volume" && $1!="Directory" && $2!="files" && $2!="bytes" {print ($2 ~ /^[0-9]/ ? $1 : $1"."$2)}' | LC_ALL=C sort)
E1=$(echo "$DIRS" | head -1); EL=$(echo "$FILES" | tail -1)
chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; ok=0; fi; }
chk "erster Eintrag ${N[0]} = Ordner $E1"   "[ \"${N[0]}\" = \"$E1\" ]"
chk "letzter Eintrag ${N[1]} = Datei $EL"   "[ \"${N[1]}\" = \"$EL\" ]"
chk "Ansicht umgeschaltet und zurueck"       "echo \"\$L\" | grep -q 'Ansicht 0x00000001' && echo \"\$L\" | grep -q 'Ansicht 0x00000000'"
chk "Speicher vollstaendig zurueck"          "echo \"\$L\" | grep -q 'Speicher vollstaendig zurueck'"
[ -f test/ref/liste_auf.png ] && for b in auf runter; do python3 test/bildvergleich.py obj/liste_$b.ppm test/ref/liste_$b.png || ok=0; done
[ $ok = 1 ] && echo "Liste: alles ok" || { echo "Liste: FEHLER"; exit 1; }
