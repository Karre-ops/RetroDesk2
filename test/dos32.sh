#!/bin/bash
# PLAN_32BIT Schritt 3: 32-Bit-DOS-Programme ueber RunDos (RD2 lagert sich aus, kommt zurueck).
# D32TEST (apps/test32) mit DOS/4GW, DOS32A, PMODE/W, CauseWay: 2 MB ueber 1 MB, Textschirm, Maus, Tastatur,
# Rueckgabe 42. Dazu ein DJGPP-Programm (DTOU.EXE aus djdev205) mit dem beigelegten DPMI-Host CWSDPMI.EXE.
# DOSBox 386, 16 MB, XMS. Danach prueft DOSTEST: Daten unveraendert, Grafik, zurueck im RD2-Programm.
cd "$(dirname "$0")/.."
R=obj/dbx32; F=0; T=obj/t32; X=tools/dos32
mkdir -p $T; ( cd $T && export WATCOM=/opt/ow PATH=/opt/ow/binl64:/opt/ow/binl:/opt/ow/binw:$PATH INCLUDE=/opt/ow/h
  for s in "dos4g 4G" "dos32a 32A" "pmodew PMW" "causeway CW"; do set -- $s; wcl386 -zq -cc -bt=dos -l=$1 -ox -fe=D32_$2.EXE ../../apps/test32/D32TEST.C >/dev/null 2>&1 || echo "FEHLER Bau D32_$2"; done )
chk() { if echo "$L" | grep -q "$1"; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
for p in D32_4G D32_32A D32_PMW D32_CW DTOU; do
  python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  cp out/RD2.EXE out/*.MOD out/*.DRV out/RDSWAP.BIN out/DOSTEST.EXE $R/RD2/
  cp /opt/ow/binw/dos4gw.exe $R/RD2/DOS4GW.EXE; cp /opt/ow/binw/dos32a.exe $R/RD2/DOS32A.EXE
  cp out/CWSDPMI.EXE $X/DTOU.EXE $R/RD2/; [ $p != DTOU ] && cp $T/$p.EXE $R/RD2/
  printf 'Zeile1\r\nZeile2\r\n' > $R/FREI.TXT
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=vgaonly\nmemsize=16\n[cpu]\ncore=normal\ncputype=386\ncycles=fixed 20000\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[dos]\nxms=true\nems=false\numb=false\n' > $R/dbx.conf
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -s KILL ${TO:-200} dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "set PATH=C:\\RD2;Z:\\" -c "cd \\RD2" \
    -c "C:\\RD2\\RD2.EXE /D DOSTEST.EXE C:\\RD2\\$p.EXE C:\\FREI.TXT > \\LOG.TXT" -c exit >/dev/null 2>&1
  L=$(tr -d '\r' < $R/LOG.TXT 2>/dev/null); A=$(cat $R/FREI.TXT 2>/dev/null)
  echo "=== $p"
  if [ $p = DTOU ]; then
    chk "DOSTEST: Rueckgabe" "DJGPP-Programm lief (CWSDPMI)"
    if [ "$A" = "$(printf 'Zeile1\nZeile2')" ] && ! grep -q $'\r' $R/FREI.TXT; then echo "ok     DTOU hat die Datei umgewandelt"; else echo "FEHLER DTOU hat die Datei nicht umgewandelt"; F=1; fi
  else
    chk "DOSTEST: Rueckgabe 0x0000002A" "Rueckgabecode 42"
    for z in "D32: start, Zeigergroesse 4" "Abweichungen 0" "ueber 1 MB ja" "Textschirm ok" "Tastatur ok" "D32: Ende"; do
      echo "$A" | grep -q "$z" && echo "ok     $z" || { echo "FEHLER $z"; F=1; }; done
    echo "       $(echo "$A" | grep 'Maus\|KB bei' | tr '\n' ' ')"
  fi
  chk "DOSTEST: Abweichungen 0x00000000" "RD2-Daten unveraendert"
  chk "DOSTEST: Grafik nachher 0x00000001" "Grafik danach"
  chk "DOSTEST: fertig" "zurueck im RD2-Programm"
  [ -f $R/RD2/RD2SWAP.\$\$\$ ] && { echo "FEHLER Auslagerdatei liegt noch da"; F=1; }
done
# ---- QEMU (echtes BIOS): RDKEYB DE und RDMAUS (PS/2) aus dem geschuetzten Modus, ohne RD2 ----
IMG=obj/dos32.img; SOCK=/tmp/qmon_d32.sock; export QMON=$SOCK
printf '@echo off\r\nA:\\RD2\\RDKEYB.COM DE\r\nA:\\RD2\\RDMAUS.COM\r\nset D32KEY=1\r\nset PATH=A:\\RD2\r\nfor %%%%p in (4G 32A PMW CW) do A:\\RD2\\D32_%%%%p.EXE A:\\K%%%%p.TXT\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_d32.txt
NUR="RDKEYB.COM RDMAUS.COM DE.KBD" FMT=1440 bash tools/mkdisk.sh $IMG obj/ae_d32.txt >/dev/null 2>&1
mcopy -o -i $IMG /opt/ow/binw/dos4gw.exe ::/RD2/DOS4GW.EXE; mcopy -o -i $IMG /opt/ow/binw/dos32a.exe ::/RD2/DOS32A.EXE
for p in 4G 32A PMW CW; do mcopy -o -i $IMG $T/D32_$p.EXE ::/RD2/; done
rm -f $SOCK; timeout 200 qemu-system-i386 -m 8 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
for p in 4G 32A PMW CW; do
  for i in $(seq 150); do mtype -i $IMG ::/K$p.TXT 2>/dev/null | grep -q "D32: warte" && break; sleep 0.5; done
  sleep 1; python3 test/qemu/qmon.py "mouse_move 40 30" sleep0.5 "mouse_move 40 30" sleep0.5 "sendkey y" >/dev/null 2>&1
  for i in $(seq 60); do mtype -i $IMG ::/K$p.TXT 2>/dev/null | grep -q "D32: Bewegung" && break; sleep 0.5; done
  A=$(mtype -i $IMG ::/K$p.TXT 2>/dev/null | tr -d '\r'); echo "=== QEMU D32_$p: $(echo "$A" | grep 'Taste\|Bewegung\|Maus' | tr '\n' ' ')"
  echo "$A" | grep -q "D32: Taste 7A" && echo "ok     RDKEYB DE: y-Taste ergibt z" || { echo "FEHLER RDKEYB (Taste)"; F=1; }
  echo "$A" | grep -q "D32: Maus FFFF" && echo "ok     RDMAUS meldet sich" || { echo "FEHLER RDMAUS fehlt"; F=1; }
  echo "$A" | grep -q "D32: Bewegung 0 0" && { echo "FEHLER RDMAUS: keine Bewegung"; F=1; } || echo "ok     RDMAUS: Bewegung kommt an"
done
sleep 2; kill $QP 2>/dev/null; wait $QP 2>/dev/null
[ $F = 0 ] && echo "dos32: alles ok" || echo "dos32: FEHLER"
exit $F
