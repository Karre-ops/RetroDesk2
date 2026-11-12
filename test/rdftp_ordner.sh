#!/bin/bash
# Dateimanager (31.10.2026, Karsten: "keine Ordner kopieren"): Ordner samt Inhalt kopieren und loeschen, VGA, nur Tastatur.
# 1 Platte zu Platte: F7 Ordner ZIEL, rechts hinein, links ORD (A.TXT, UNTER\B.BIN) mit F5 nach ZIEL, rechts ZIEL\ORD mit F8 loeschen.
# 2 FTP (Host test/ftpserver.py): ORD hochladen (MKD, STOR mit Pfaden), Serverordner "srvdir" (x.txt, "sub dir"/y.bin) laden
#   (LIST mit Pfad, 8.3-Namen SRVDIR\SUBDIR\Y.BIN). Pruefung: Inhalte auf beiden Seiten gleich, ZIEL\ORD weg.
cd "$(dirname "$0")/.."
IMG=obj/rdfo.img; SER=obj/rdfo_com1.txt; SOCK=/tmp/qmon_rdfo.sock; HOSTLOG=obj/rdfo_host.txt; R=obj/rdfo_root; F=0
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*) kill ${p#/proc/} 2>/dev/null;; esac; done
for p in /proc/[0-9]*; do case "$(tr '\0' ' ' < $p/cmdline 2>/dev/null)" in "python3 test/ftpserver.py "*) kill ${p#/proc/} 2>/dev/null;; esac; done
printf '@echo off\r\nrem RDTCP.MOD (fuer mkdisk)\r\nA:\\NE2000.COM 0x60 3 0x300\r\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\RDFTP.EXE T A:\\T1\\\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_rdfo.txt
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_rdfo.txt >/dev/null 2>&1
mcopy -o -i $IMG test/qemu/pkt/NE2000.COM ::/
rm -rf obj/rdfo_root obj/rdfo_t1 obj/rdfo_aus; mkdir -p "obj/rdfo_root/srvdir/sub dir" obj/rdfo_t1/ORD/UNTER obj/rdfo_aus
python3 - <<'PY'
import random
r = random.Random(31)
open('obj/rdfo_t1/ORD/A.TXT', 'wb').write(b'Ordner kopieren\r\n' * 40)
open('obj/rdfo_t1/ORD/UNTER/B.BIN', 'wb').write(bytes(r.randrange(256) for _ in range(6000)))
open('obj/rdfo_root/srvdir/x.txt', 'wb').write(b'vom Server\r\n' * 30)
open('obj/rdfo_root/srvdir/sub dir/y.bin', 'wb').write(bytes(r.randrange(256) for _ in range(4500)))
open('obj/rdfo_ini.txt', 'wb').write(b'[Netz]\r\nDHCP=1\r\n[FTP]\r\nServer=10.0.2.2\r\nPort=2122\r\nBenutzer=retro\r\nKennwort=geheim\r\n')
PY
mmd -i $IMG ::/T1; mcopy -s -i $IMG obj/rdfo_t1/ORD ::/T1/; mcopy -o -i $IMG obj/rdfo_ini.txt ::/RD2/RD2.INI
rm -f $SER $SOCK $HOSTLOG
python3 test/ftpserver.py $R 2122 > $HOSTLOG 2>&1 &
HP=$!; sleep 0.5
timeout 140 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -netdev user,id=n0 -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_rdfo.py
Q() { python3 obj/qmon_rdfo.py "$@" 2>/dev/null; }
waitn() { for i in $(seq 300); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; echo "FEHLER: warte auf $1 ($2)"; return 1; }
lauf() {
  waitn "RDFTP: bereit" 1 || return; sleep 3
  Q "sendkey f7"; sleep 1.5; Q "sendkey z" sleep0.15 "sendkey i" sleep0.15 "sendkey e" sleep0.15 "sendkey l" sleep0.3 "sendkey ret"
  waitn "RDFTP: rlokal" 2 || return; sleep 1
  Q "sendkey tab" sleep0.4 "sendkey kp_7" sleep0.3 "sendkey kp_2" sleep0.3 "sendkey kp_2" sleep0.4 "sendkey ret"   # rechts: .. ORD ZIEL
  waitn "RDFTP: rlokal" 3 || return; sleep 1
  Q "sendkey tab" sleep0.4 "sendkey kp_7" sleep0.3 "sendkey kp_2" sleep0.4 "sendkey f5"   # links ORD
  waitn "RDFTP: rlokal" 4 || return; sleep 1
  Q "sendkey tab" sleep0.4 "sendkey kp_7" sleep0.3 "sendkey kp_2" sleep0.4 "sendkey f8"   # rechts ZIEL\ORD loeschen
  waitn "RDFTP: rlokal" 5 || return; sleep 1
  Q "sendkey alt-v"
  waitn "RDFTP: rechts" 1 || return; sleep 2
  Q "sendkey tab" sleep0.4 "sendkey kp_7" sleep0.3 "sendkey kp_2" sleep0.4 "sendkey f5"   # links ORD hochladen
  waitn "RDFTP: rechts" 2 || return; sleep 1.5
  Q "sendkey tab" sleep0.4 "sendkey kp_7" sleep0.3 "sendkey kp_2" sleep0.4 "sendkey f5"   # rechts: ORD srvdir -> srvdir laden
  waitn "RDFTP: geladen" 2 || return; sleep 1.5   # Serverliste bleibt, keine neue Liste
  Q "screendump obj/rdfo_vga.ppm" sleep0.3 "sendkey esc"
  waitn "RDFTP: Ende" 1
}
lauf || F=1
wait $QP; kill $HP 2>/dev/null; wait $HP 2>/dev/null
S=$(tr -d '\r' < $SER)
mcopy -o -i $IMG ::/T1/SRVDIR/X.TXT ::/T1/SRVDIR/SUBDIR/Y.BIN obj/rdfo_aus/ 2>/dev/null
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
ok "Platte zu Platte: ORD samt UNTER\\B.BIN nach ZIEL kopiert" "grep -q 'RDFTP: kopiert A:.T1.ZIEL.ORD.UNTER.B.BIN' <<< \"\$S\" && grep -q 'RDFTP: kopiert A:.T1.ZIEL.ORD.A.TXT' <<< \"\$S\""
ok "ZIEL\\ORD samt Inhalt geloescht, ZIEL bleibt" "mdir -i $IMG ::/T1/ZIEL >/dev/null 2>&1 && ! mdir -i $IMG ::/T1/ZIEL/ORD >/dev/null 2>&1"
ok "Original ORD unveraendert" "mdir -i $IMG ::/T1/ORD/UNTER/B.BIN >/dev/null 2>&1"
ok "FTP hochladen: ORD/A.TXT gleich" "cmp -s $R/ORD/A.TXT obj/rdfo_t1/ORD/A.TXT"
ok "FTP hochladen: ORD/UNTER/B.BIN gleich" "cmp -s $R/ORD/UNTER/B.BIN obj/rdfo_t1/ORD/UNTER/B.BIN"
ok "FTP laden: SRVDIR\\X.TXT gleich" "cmp -s obj/rdfo_aus/X.TXT $R/srvdir/x.txt"
ok "FTP laden: 'sub dir' als SUBDIR\\Y.BIN gleich" "cmp -s obj/rdfo_aus/Y.BIN '$R/srvdir/sub dir/y.bin'"
ok "Speicher zurueck" "grep -q 'Speicher vollstaendig zurueck' <<< \"\$S\""
[ $F = 0 ] && echo "Dateimanager Ordner: alles ok" || { grep -E "RDFTP" <<< "$S" | tail -30; grep -vE "^host: < (USER|TYPE|PWD|PASV)" $HOSTLOG | tail -20; }
exit $F
