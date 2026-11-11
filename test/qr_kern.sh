#!/bin/bash
# qr_kern.sh - QR-Erzeuger von RDQR (apps/net/QR.C) mit gcc bauen und jede Probe mit OpenCV lesen (pip: opencv-python-headless).
cd "$(dirname "$0")/.."
gcc -O1 -Wall -o obj/qr_kern -x c test/qr_kern.c -x c apps/net/QR.C || exit 1
f=0
for t in "http://10.0.0.5/abcd/" "http://192.168.50.23/k7f3/" "http://192.168.250.254/zzzz/" "http://172.16.100.200/q0w9/" "HTTP://1.2.3.4/A/" "$(printf 'x%.0s' $(seq 53))"; do
  obj/qr_kern "$t" obj/qr.pgm || { echo "FEHLER Erzeugen: $t"; f=1; continue; }
  r=$(python3 -c "import cv2,sys; d,_,_=cv2.QRCodeDetector().detectAndDecode(cv2.imread('obj/qr.pgm',0)); print(d)")
  if [ "$r" = "$t" ]; then echo "ok   $t"; else echo "FEHLER $t -> '$r'"; f=1; fi
done
obj/qr_kern "$(printf 'x%.0s' $(seq 54))" obj/qr.pgm && { echo "FEHLER: 54 Zeichen angenommen"; f=1; } || echo "ok   54 Zeichen abgelehnt"
exit $f
