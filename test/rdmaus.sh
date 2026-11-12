#!/bin/bash
# Eigener Maustreiber RDMAUS.COM statt CTMOUSE. Lauf 1: PS/2 (automatisch erkannt), Lauf 2: serielle
# Microsoft-Maus an COM2 (QEMU msmouse, /S2). Je Lauf: Rechtsklick auf Laufwerk A: oeffnet das Kontextmenue,
# Doppelklick oeffnet den Ordner A:\ (RDMAUS: 1 QEMU-Einheit = 1 Pixel waagrecht, 0,5 senkrecht, also 3x der CTMOUSE-Weg aus desk3.sh).
# Zur Ecke oben links in Schritten von 100 (die serielle QEMU-Maus kappt jede Bewegung auf 127).
# Lauf 3 (grafik): Treiberzeiger in DOS-Programmen (ZTEST.COM aus test/dos, Modus 3, 6, 4, 13h, 12h, dann Funktion 10h).
# Aufruf: test/rdmaus.sh [ps2|seriell|grafik]   (ohne Angabe alle drei, je ca. 40-60 s)
cd "$(dirname "$0")/.."
F=0
diffpx() { python3 -c "
from PIL import Image, ImageChops
a=Image.open('$1').convert('L'); b=Image.open('$2').convert('L'); print(sum(1 for v in ImageChops.difference(a,b).tobytes() if v))"; }
lauf() {   # $1 Name, $2 RDMAUS-Schalter, $3 QEMU-Zusatz
  IMG=obj/rdm_$1.img; SER=obj/rdm_$1_com1.txt; SOCK=/tmp/qmon_rdm.sock
  printf '@echo off\nA:\\RDMAUS.COM %s\nA:\\RD2\\RD2.EXE /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' "$2" > obj/ae_rdm.txt
  NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ')" bash tools/mkdisk.sh $IMG obj/ae_rdm.txt >/dev/null 2>&1
  mcopy -o -i $IMG out/RDMAUS.COM ::/ 2>/dev/null || mcopy -o -i $IMG obj/rdm_t.com ::/RDMAUS.COM
  rm -f $SER $SOCK; touch $SER
  timeout 70 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
    -serial file:$SER $3 -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_rdm.py
  Q() { timeout 20 python3 obj/qmon_rdm.py "$@" >/dev/null 2>&1; }
  waitn() { for i in $(seq 250); do [ "$(tr -d '\r' < $SER | grep -ac "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; return 1; }
  if waitn "SHELL: bereit" 1; then
    tr -d '\r' < $SER | grep -a "maus\|Maus" | head -2
    sleep 2; Q "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 "mouse_move -100 -100" sleep0.15 sleep0.5 "mouse_move 36 93" sleep0.8 "screendump obj/rdm_$1_vor.ppm" sleep0.3 \
      "mouse_button 2" sleep0.2 "mouse_button 0" sleep1.2 "screendump obj/rdm_$1_pop.ppm" sleep0.3 "sendkey esc" sleep1.0
    n=$(diffpx obj/rdm_$1_vor.ppm obj/rdm_$1_pop.ppm); [ "$n" -gt 3000 ] && echo "ok     $1: Rechtsklick oeffnet das Kontextmenue ($n Pixel)" || { echo "FEHLER $1: kein Kontextmenue ($n Pixel)"; F=1; }
    Q "mouse_button 1" sleep0.1 "mouse_button 0" sleep0.2 "mouse_button 1" sleep0.1 "mouse_button 0" sleep3
    tr -d '\r' < $SER | grep -aq 'SHELL: Ordner A:' && echo "ok     $1: Doppelklick oeffnet A:\\" || { echo "FEHLER $1: kein Doppelklick"; F=1; }
    Q "sendkey alt-f4" sleep1 "sendkey alt-f4" sleep1 "sendkey ret"
  else echo "FEHLER $1: Shell startet nicht"; F=1; fi
  sleep 3; kill $QP 2>/dev/null; wait $QP 2>/dev/null
}
grafik() {   # Treiberzeiger fuer DOS-Programme: ZTEST.COM setzt Modus 3, 6, 4, 13h, 12h und zeigt den Zeiger
  IMG=obj/rdm_grafik.img; SOCK=/tmp/qmon_rdm.sock
  printf '@echo off\nA:\\RDMAUS.COM\nA:\\ZTEST.COM\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_rdmg.txt
  NUR="RD2.EXE" bash tools/mkdisk.sh $IMG obj/ae_rdmg.txt >/dev/null 2>&1
  mcopy -o -i $IMG out/RDMAUS.COM ::/; mcopy -o -i $IMG obj/ZTEST.COM ::/
  rm -f $SOCK
  timeout 70 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
    -serial null -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_rdm.py
  Q() { timeout 20 python3 obj/qmon_rdm.py "$@" >/dev/null 2>&1; }
  sleep 14
  for m in 03 06 04 13 12; do
    Q "screendump obj/rdm_g$m.ppm" sleep0.3 "mouse_move 30 0" sleep0.5 "screendump obj/rdm_g${m}b.ppm" sleep0.3 "sendkey spc" sleep1.2
    r=$(python3 -c "
from PIL import Image
def lit(f):
    a=Image.open(f).convert('L'); w,h=a.size; p=a.load(); bg=p[0,h-1]
    pts=[(x,y) for y in range(h) for x in range(w) if p[x,y]!=bg]
    return len(pts), (max(x for x,y in pts) if pts else -1), w   # rechter Rand: der DOS-Textcursor steht links oben
n,x,w=lit('obj/rdm_g$m.ppm'); n2,x2,_=lit('obj/rdm_g${m}b.ppm'); print(n,x,n2,x2,w)")
    set -- $r
    if [ "$1" -ge 30 ] && [ "$1" -le 2000 ] && [ "$4" -gt "$2" ]; then echo "ok     Modus ${m}h: Zeiger sichtbar ($1 Punkte), bewegt sich (x $2 -> $4)"
    else echo "FEHLER Modus ${m}h: $r"; F=1; fi
  done
  Q "screendump obj/rdm_f10a.ppm" sleep0.3 "sendkey spc" sleep1.2 "screendump obj/rdm_f10b.ppm" sleep0.3 "sendkey spc" sleep1
  r=$(python3 -c "
from PIL import Image
def lit(f):
    a=Image.open(f).convert('L'); w,h=a.size; p=a.load(); bg=p[0,h-1]
    return sum(1 for y in range(h) for x in range(w) if p[x,y]!=bg)
print(lit('obj/rdm_f10a.ppm'), lit('obj/rdm_f10b.ppm'))")
  set -- $r
  if [ "$1" = 0 ] && [ "$2" -ge 30 ]; then echo "ok     Funktion 10h: Zeiger im Bereich aus, nach Funktion 1 wieder da ($2 Punkte)"
  else echo "FEHLER Funktion 10h: $r"; F=1; fi
  sleep 2; kill $QP 2>/dev/null; wait $QP 2>/dev/null
}
case "$1" in ps2) lauf ps2 "" "";; seriell) lauf seriell "/S2" "-serial msmouse";; grafik) grafik;;
  *) lauf ps2 "" ""; lauf seriell "/S2" "-serial msmouse"; grafik;; esac
[ $F = 0 ] && echo "RDMAUS: alles ok"; exit $F
