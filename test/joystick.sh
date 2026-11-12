#!/bin/bash
# Joystick (RD2.LIB): 1) QEMU ohne Gameport -> JoyRead meldet 8 (RD_ERR_NOTSUP), haengt nicht.
# 2) DOSBox mit joysticktype=2axis (ohne echten Knueppel: Mitte, keine Taste) -> 0, x=y=0, Tasten 0.
cd "$(dirname "$0")/.."
F=0; SER=obj/joy_com1.txt; IMG=obj/joy.img
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /CGA A:\\RD2\\JOYTEST.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_joy.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') JOYTEST.EXE" FMT=720 bash tools/mkdisk.sh $IMG obj/ae_joy.txt >/dev/null 2>&1
rm -f $SER; timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a -serial file:$SER >/dev/null 2>&1
if grep -q "JOYTEST: Ende" $SER && [ "$(grep -c 'JOYTEST: [AB] 8 0 0 0' $SER)" = 6 ]; then echo "ok     QEMU ohne Gameport: 6x Fehler 8, kein Haenger"; else echo "FEHLER QEMU"; grep JOYTEST $SER; F=1; fi
R=$PWD/obj/joydbx; python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
for f in $(awk '$4=="S" && $1!="RD2SETUP.EXE"{print $1}' tools/budget.txt) JOYTEST.EXE; do [ -f out/$f ] && cp out/$f $R/RD2/; done
printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=vgaonly\nmemsize=1\n[cpu]\ncore=normal\ncputype=386\ncycles=fixed 3000\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[joystick]\njoysticktype=2axis\ntimed=true\n[serial]\nserial1=file file:%s/com1.txt\n' "$R" > $R/dbx.conf
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 120 dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" -c "C:\\RD2\\RD2.EXE /D /NOMAUS /VGA JOYTEST.EXE > \\LOG.TXT" -c "exit" >/dev/null 2>&1
if grep -q "JOYTEST: A 0 0 0 0" $R/RD2/JOY.TXT; then echo "ok     DOSBox 2axis: A misst, Mitte 0/0, keine Taste"; else echo "FEHLER DOSBox"; cat $R/RD2/JOY.TXT; F=1; fi
[ $F = 0 ] && echo "Joystick: alles ok"; exit $F
