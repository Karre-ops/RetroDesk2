#!/bin/bash
# Audit K2: Diskettenabfrage des Kerns im Grafikbetrieb (VGA, CGA, HGC). Bild mit Kasten, Esc, Bild danach.
cd "$(dirname "$0")/.."
IMG=obj/dfrag.img; SER=obj/dfrag_com1.txt; SOCK=/tmp/qmon_dfrag.sock
cat > obj/ae_dfrag.txt <<AE
@echo off
A:\\RD2\\RD2.EXE /VGA A:\\RD2\\DISKFRAG.EXE
A:\\RD2\\RD2.EXE /CGA A:\\RD2\\DISKFRAG.EXE
A:\\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_dfrag.txt >/dev/null
rm -f $SER $SOCK obj/dfrag_*.ppm
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_dfrag.py
Q() { python3 obj/qmon_dfrag.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
F=0
for n in 1 2; do
  K=$([ $n = 1 ] && echo vga || echo cga)
  waitn "Bitte Diskette RD2 PROG" $n || { echo "FEHLER: keine Abfrage ($K)"; F=1; break; }
  sleep 1.5; Q "screendump obj/dfrag_frage_$K.ppm" sleep0.3 "sendkey esc"
  waitn "DISKFRAG: nach Esc" $n || { echo "FEHLER: Esc wirkt nicht ($K)"; F=1; break; }
  sleep 1; Q "screendump obj/dfrag_danach_$K.ppm"
  waitn "DISKFRAG: Ende" $n || { echo "FEHLER: kein Ende ($K)"; F=1; }
done
wait $QP
S=$(tr -d '\r' < $SER)
echo "$S" | grep -E "DISKFRAG|Bitte Diskette|RD: (Esc|Enter)" | head -12
[[ "$S" == *"DISKFRAG: Esc -> 0x00000005"* ]] && echo "ok     Esc liefert RD_ERR_NODISK" || { echo "FEHLER Esc-Ergebnis"; F=1; }
python3 - <<'PY' || F=1
import sys
def lies(p):
    d=open(p,'rb').read(); k=d.split(b'\n',3); w,h=map(int,k[1].split()); return w,h,k[3]
bad=0
for K in ('vga','cga'):
    try: w,h,a=lies(f'obj/dfrag_frage_{K}.ppm'); _,_,b=lies(f'obj/dfrag_danach_{K}.ppm')
    except Exception as e: print('FEHLER Bild',K,e); bad=1; continue
    # Kasten: Mitte des Bildes weiss mit schwarzem Rand und Text; danach anders (Fenster/Schreibtisch)
    mitte=sum(1 for y in range(h//2-20,h//2+20) for x in range(w//2-100,w//2+100) if a[(y*w+x)*3]>128)
    diff=sum(1 for i in range(0,len(a),3) if a[i]!=b[i])
    texts=sum(1 for y in range(h//2-20,h//2+20) for x in range(w//2-100,w//2+100) if a[(y*w+x)*3]<128)
    ok=mitte>3000 and texts>100 and diff>1000
    print(('ok    ' if ok else 'FEHLER'),f'{K}: Kasten hell {mitte}, Text {texts}, nach Esc {diff} Pixel anders')
    bad|=not ok
sys.exit(bad)
PY
[ $F = 0 ] && echo "Diskettenabfrage: alles ok" || echo "Diskettenabfrage: Fehler"
exit $F
