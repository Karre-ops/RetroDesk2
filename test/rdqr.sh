#!/bin/bash
# RDQR.EXE (Datei-Eingang) mit NE2000 in QEMU, Host leitet Port 18080 auf Port 80 des Gasts. VGA, Testmodus (Schluessel "test"):
# Bild mit QR-Code (OpenCV muss die Adresse lesen), Seite per GET, falscher Schluessel 404, zwei Uploads per POST (einer
# gross, 40 KB), gleicher Name zweimal (kein Ueberschreiben), Esc beendet. Pruefung: Dateien im Image, COM1, Speicher.
cd "$(dirname "$0")/.."
IMG=obj/rdqr.img; SER=obj/rdqr_com1.txt; SOCK=/tmp/qmon_rdqr.sock
cat > obj/ae_rdqr.txt <<'AE'
@echo off
rem RDTCP.MOD (fuer mkdisk)
A:\NE2000.COM 0x60 3 0x300
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDQR.EXE T
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_rdqr.txt >/dev/null
mcopy -o -i $IMG test/qemu/pkt/NE2000.COM ::/
mcopy -o -i $IMG out/RDQR.EXE ::/RD2/
mmd -i $IMG ::/EINGANG 2>/dev/null
printf '[Netz]\r\nDHCP=1\r\n[QR]\r\nOrdner=A:\\EINGANG\r\n' > obj/rdqr_ini.txt
mcopy -o -i $IMG obj/rdqr_ini.txt ::/RD2/RD2.INI
rm -f $SER $SOCK obj/rdqr*_vga.ppm obj/rdqr_*.bin
head -c 40000 /dev/urandom > obj/rdqr_gross.bin; printf 'Hallo vom Smartphone\n' > obj/rdqr_klein.bin
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -netdev user,id=n0,hostfwd=tcp:127.0.0.1:18080-:80 -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_rdqr.py
Q() { python3 obj/qmon_rdqr.py "$@" 2>/dev/null; }
waitn() { for i in $(seq 300); do [ "$(grep -ac "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; echo "FEHLER: warte auf $1 ($2)"; return 1; }
U=http://127.0.0.1:18080
lauf() {
  waitn "RDQR: bereit" 1 || return; sleep 2
  Q "screendump obj/rdqr1_vga.ppm"
  curl -s -m 20 -o obj/rdqr_seite.html -w '%{http_code}' $U/test/ > obj/rdqr_get.txt
  curl -s -m 20 -o /dev/null -w '%{http_code}' $U/abcd/ > obj/rdqr_404.txt
  curl -s -m 30 -o /dev/null -w '%{http_code}' --data-binary @obj/rdqr_klein.bin "$U/test/u?n=hallo.txt" > obj/rdqr_p1.txt
  curl -s -m 60 -o /dev/null -w '%{http_code}' --data-binary @obj/rdqr_gross.bin "$U/test/u?n=Urlaubsfoto%20gross.jpg" > obj/rdqr_p2.txt
  curl -s -m 30 -o /dev/null -w '%{http_code}' --data-binary @obj/rdqr_klein.bin "$U/test/u?n=hallo.txt" > obj/rdqr_p3.txt
  sleep 1.5; Q "screendump obj/rdqr2_vga.ppm" sleep0.3 "sendkey esc"
  waitn "RDQR: Ende" 1 || return
}
lauf
wait $QP
S=$(tr -d '\r' < $SER); grep -a "RDQR" <<< "$S" | head -30
mdir -i $IMG ::/EINGANG
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; fi; }
ok "QR-Code auf dem Bildschirm lesbar" "python3 -c \"import cv2,sys; i=cv2.imread('obj/rdqr1_vga.ppm',0); d,_,_=cv2.QRCodeDetector().detectAndDecode(cv2.resize(i,None,fx=2,fy=2,interpolation=cv2.INTER_NEAREST)); print('  QR:',d); sys.exit(0 if d.startswith('http://10.0.2.15') else 1)\""
ok "Seite (GET 200)" "[ \"\$(cat obj/rdqr_get.txt)\" = 200 ] && grep -q XMLHttpRequest obj/rdqr_seite.html"
ok "falscher Schluessel 404" "[ \"\$(cat obj/rdqr_404.txt)\" = 404 ]"
ok "Uploads 200" "[ \"\$(cat obj/rdqr_p1.txt obj/rdqr_p2.txt obj/rdqr_p3.txt)\" = 200200200 ]"
ok "kleine Datei gleich" "mtype -i $IMG ::/EINGANG/HALLO.TXT | cmp -s - obj/rdqr_klein.bin"
ok "grosse Datei gleich (40 KB)" "mdir -i $IMG -b ::/EINGANG | grep -iq URLAUBS && mtype -i $IMG \"::/EINGANG/\$(mdir -i $IMG -b ::/EINGANG | grep -i urlaubs | sed 's#.*/##')\" | cmp -s - obj/rdqr_gross.bin"
ok "gleicher Name nicht ueberschrieben" "[ \$(mdir -i $IMG -b ::/EINGANG | grep -ic hallo) -ge 2 ]"
ok "Speicher zurueck" "grep -aq 'Speicher vollstaendig zurueck' <<< \"\$S\""
exit 0
