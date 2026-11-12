#!/bin/bash
# Papierkorb: SHELL loescht auf C: (Festplatte) WEG.TXT -> C:\PAPIERK\WEG.TXT + PAPIERK.LST; PAPIERK.EXE stellt sie
# wieder her. Auf A: (Diskette) wird weiter direkt geloescht. Platte: Kopie von obj/satz_hd.img (test/satz.sh).
cd "$(dirname "$0")/.."
IMG=obj/pk.img; HD=obj/pk_hd.img; SER=obj/pk_com1.txt; S=/tmp/qmon_pk.sock; export QMON=$S; F=0
[ -f obj/satz_hd.img ] || { echo "FEHLER obj/satz_hd.img fehlt (erst test/satz.sh)"; exit 1; }
cp obj/satz_hd.img $HD
O=$(python3 -c "import struct;d=open('$HD','rb').read(512);print(struct.unpack('<I',d[454:458])[0]*512)")
C="-i $HD@@$O"; echo weg > obj/WEG.TXT; mcopy -o $C obj/WEG.TXT ::/; mdeltree $C ::/PAPIERK 2>/dev/null
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\PAPIERK.EXE C\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_pk.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') PAPIERK.EXE" bash tools/mkdisk.sh $IMG obj/ae_pk.txt >/dev/null 2>&1
rm -f $SER $S; touch $SER
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -drive file=$HD,format=raw,if=ide,index=0 -boot a \
  -serial file:$SER -monitor unix:$S,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { timeout 10 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
waitn() { local c; for i in $(seq 250); do c=$(tr -d '\r' < $SER | grep -c "$1"); [ "${c:-0}" -ge "${2:-1}" ] && return 0; sleep 0.2; done; return 1; }
ok() { if waitn "$1" ${3:-1}; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
waitn "SHELL: bereit"; sleep 3; Q "sendkey kp_2"; sleep 0.5; Q "sendkey ret"
ok 'SHELL: Ordner C:\\ ' "Ordner C:\\ offen"
sleep 3; Q "sendkey end"; sleep 0.5; Q "sendkey delete"; sleep 1.5; Q "screendump obj/pk1.ppm"; Q "sendkey ret"
ok "SHELL: Papierkorb" "Entf auf der Festplatte legt in den Papierkorb"
sleep 2; Q "sendkey alt-spc"; sleep 0.8; Q "sendkey b"; sleep 1.5; Q "sendkey ret"
ok "PAPIERK: Eintraege 0x00000001" "PAPIERK.EXE zeigt 1 Eintrag"
sleep 2; Q "screendump obj/pk2.ppm"; Q "sendkey ret"
ok "PAPIERK: zurueck" "Wiederherstellen"
tr -d '\r' < $SER | grep -q "^C:\\\\WEG.TXT$" && echo "ok     Originalpfad C:\\WEG.TXT" || { echo "FEHLER Originalpfad"; F=1; }
ok "PAPIERK: Eintraege 0x00000000" "Liste danach leer"
sleep 1; Q "sendkey esc"; wait $QP 2>/dev/null
mdir $C -b ::/WEG.TXT 2>/dev/null | grep -qi "WEG.TXT" && echo "ok     C:\\WEG.TXT wieder da" || { echo "FEHLER WEG.TXT nicht zurueck"; F=1; }
mdir $C -b ::/PAPIERK/ 2>/dev/null | grep -qi "WEG.TXT\|PAPIERK.LST" && { echo "FEHLER Reste im Papierkorb"; mdir $C ::/PAPIERK; F=1; } || echo "ok     Papierkorb leer"
for i in 1 2; do [ -f obj/pk$i.ppm ] && python3 -c "from PIL import Image; Image.open('obj/pk$i.ppm').save('obj/pk$i.png')"; done
[ $F = 0 ] && echo "Papierkorb: alles ok"; exit $F
