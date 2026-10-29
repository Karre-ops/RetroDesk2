#!/bin/bash
# Pascal auf RD2: sdk/RD2.PAS (tools/mkpas.py) und der Vorspann sdk/RD2PAS.ASM mit Turbo Pascal 5.5 (in DOSBox)
# und Free Pascal i8086-msdos (Cross). PTEST (API, Befehlszeile, ParamStr, Heap, Exitcode 42) und PHALLO
# (Fenster, Zeichnen, zwei Mausklicks, Esc, Exitcode = Klicks) laufen nacheinander in einem QEMU-Start. Ca. 70 s.
# Werkzeuge (nicht im Repo): TP55=/tmp/tpw (TPC.EXE, TURBO.TPL; sonst von Embarcadero geladen), FPC8086=/opt/fpc8086.
cd "$(dirname "$0")/.."
F=0; T=obj/pas; TP=${TP55:-/tmp/tpw}; FP=${FPC8086:-/opt/fpc8086}/lib/fpc/3.2.2
export PATH=/opt/ow/binl64:/opt/ow/binl:$PATH WATCOM=/opt/ow
python3 -c "import shutil; shutil.rmtree('$T', True)"; mkdir -p $T/tp $T/fp
python3 tools/mkpas.py $T/RD2.PAS 2>/dev/null && nasm -f bin -o $T/RD2PAS.BIN sdk/RD2PAS.ASM || { echo "FEHLER Unit/Vorspann"; exit 1; }
if [ ! -f $TP/TPC.EXE ]; then
  mkdir -p $TP && curl -sL -o $TP/tp55.zip http://altd.embarcadero.com/download/museum/tp55.zip && \
  python3 -c "import zipfile; z=zipfile.ZipFile('$TP/tp55.zip'); [open('$TP/'+n.split('/')[-1],'wb').write(z.read(n)) for n in ('Disk1/TPC.EXE','Disk1/TURBO.TPL')]"
fi
# Turbo Pascal 5.5 in DOSBox
cp $TP/TPC.EXE $TP/TURBO.TPL $T/RD2.PAS apps/pascal/PTEST.PAS apps/pascal/PHALLO.PAS $T/tp/
printf 'TPC /B PTEST.PAS > T1.TXT\r\nTPC /B PHALLO.PAS > T2.TXT\r\nexit\r\n' > $T/tp/T.BAT
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 60 dosbox -noconsole -c "mount c $T/tp" -c "c:" -c "T.BAT" -exit >/dev/null 2>&1
if [ -f $T/tp/PTEST.EXE ] && [ -f $T/tp/PHALLO.EXE ]; then echo "ok     Turbo Pascal 5.5: RD2.PAS, PTEST, PHALLO uebersetzt"
else echo "FEHLER Turbo Pascal: $(tr -d '\r' < $T/tp/T1.TXT | tail -2) $(tr -d '\r' < $T/tp/T2.TXT | tail -2)"; F=1; fi
# Free Pascal
cp $T/RD2.PAS apps/pascal/PTEST.PAS apps/pascal/PHALLO.PAS $T/fp/
( cd $T/fp && for p in PTEST PHALLO; do $FP/ppcross8086 -Tmsdos -WmLarge -Fu$FP/units/msdos/rtl -O2 -XX -CX $p.PAS > $p.log 2>&1; done )
if [ -f $T/fp/PTEST.exe ] && [ -f $T/fp/PHALLO.exe ]; then echo "ok     Free Pascal i8086: RD2.PAS, PTEST, PHALLO uebersetzt"
else echo "FEHLER Free Pascal: $(grep -h -i "error\|fatal" $T/fp/*.log | head -2)"; F=1; fi
# Vorspann + RD2-Block (rd2link.py --pas; RDRC.EXE /PAS prueft test/rdrc.sh)
for x in "tp/PTEST.EXE PTESTT" "tp/PHALLO.EXE PHALLOT" "fp/PTEST.exe PTESTF" "fp/PHALLO.exe PHALLOF"; do set -- $x
  cp $T/$1 $T/$2.EXE 2>/dev/null && python3 tools/rd2link.py $T/$2.EXE "Pascal $2" --pas $T/RD2PAS.BIN >/dev/null || F=1
done
# Lauf: vier Programme nacheinander, Klicks ins PHALLO-Fenster (100,100)-(540,300)
IMG=obj/pas.img; SER=obj/com1_pas.txt; SOCK=/tmp/qmon_pas.sock
printf '@echo off\r\nA:\\RDMAUS.COM\r\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\PTESTT.EXE eins zwei\r\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\PTESTF.EXE eins zwei\r\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\PHALLOT.EXE T\r\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\PHALLOF.EXE T\r\nA:\\RD2\\RD.EXE /VGA A:\\RD2\\HALLO.EXE T\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_pas.txt
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_pas.txt >/dev/null 2>&1
mcopy -o -i $IMG out/RDMAUS.COM ::/; for p in PTESTT PTESTF PHALLOT PHALLOF; do mcopy -o -i $IMG $T/$p.EXE ::/RD2/; done; mcopy -o -i $IMG out/HALLO.EXE ::/RD2/
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*) kill ${p#/proc/} 2>/dev/null;; esac; done
rm -f $SER $SOCK; touch $SER
timeout 120 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
Q() { QMON=$SOCK timeout 20 python3 test/qemu/qmon.py "$@" >/dev/null 2>&1; }
warte() { for i in $(seq 60); do [ "$(grep -c "$1" $SER)" -ge "$2" ] && return 0; sleep 1; done; return 1; }
for n in 1 2 3; do
  if [ $n = 3 ]; then m="CHALLO: Fenster da"; k=1; else m="PHALLO: Fenster da"; k=$n; fi
  if warte "$m" $k; then
    sleep 1.5
    Q "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.2 "mouse_move -400 -400" sleep0.4 \
      "mouse_move 300 400" sleep0.6 "mouse_button 1" sleep0.3 "mouse_button 0" sleep0.8 "mouse_button 1" sleep0.3 "mouse_button 0" sleep1.2 \
      "screendump obj/phallo$n.ppm" sleep0.3 "sendkey esc"
  fi
done
wait $QP
S=$(tr -d '\r' < $SER)
z() { echo "$S" | grep -c "$1"; }
pr() { if [ "$1" = 1 ]; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
pr "$( [ $(z 'PASCAL: Param1=eins') = 2 ] && [ $(z 'PASCAL: Heap 0x00003039') = 2 ] && [ $(z 'PASCAL: Kern-API 0x00000100') = 2 ] && echo 1)" "PTEST (TP, FPC): Kern-API, Befehlszeile, ParamStr, Heap"
pr "$( [ $(z 'Task beendet: 0x0001002A') = 2 ] && echo 1)" "PTEST (TP, FPC): Exitcode 42 ueber Halt"
pr "$( [ $(z 'PHALLO: Paint 2') = 2 ] && echo 1)" "PHALLO (TP, FPC): Fenster, Zeichnen, zwei Klicks gezaehlt"
pr "$( [ $(z 'Task beendet: 0x00010002') -ge 2 ] && echo 1)" "PHALLO (TP, FPC): Esc schliesst, Exitcode 2"
pr "$( [ $(z 'CHALLO: Paint') -ge 2 ] && echo "$S" | grep -q "^Klicks: 2" && echo 1)" "HALLO (C-Beispiel aus dem SDK): Fenster, Zeichnen, zwei Klicks"
pr "$( [ $(z 'Task beendet: 0x00010002') = 3 ] && echo 1)" "HALLO: Esc schliesst, Exitcode 2"
pr "$( [ $(z 'Speicher vollstaendig zurueck') = 5 ] && echo 1)" "Speicher nach allen fuenf Programmen vollstaendig zurueck"
[ $F = 0 ] && echo "PASCAL: alles ok" || echo "PASCAL: FEHLER"
exit $F
