#!/bin/bash
# Punkt 8: RDNET.EXE (Netzwerk-Einstellungen) mit NE2000 in QEMU, nur Tastatur.
# VGA: Seite 1 "Booten laden" an, Seite 4 FTP/IRC-Server 10.0.2.2, Seite 3 Zeitserver 10.0.2.2, Seite 2 ansehen,
# Testen (Host: FTP-Begruessung Port 21, IRC 6667, SNTP 123 mit 26.10.2026 10:30:00 UTC -> Berlin 11:30:00 Winterzeit), OK.
# CGA: Seite 1 und 4 im Bild (200 Zeilen). Pruefung: RD2.INI-Werte, INSTALL=-Zeile in CONFIG.SYS, Testzeilen, Bilder.
cd "$(dirname "$0")/.."
IMG=obj/rdnet.img; SER=obj/rdnet_com1.txt; SOCK=/tmp/qmon_rdnet.sock; HOSTLOG=obj/rdnet_host.txt
cat > obj/ae_rdnet.txt <<'AE'
@echo off
A:\NE2000.COM 0x60 3 0x300
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDNET.EXE T A:\T1\
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\RDNET.EXE T A:\T1\
A:\FDAPM.COM POWEROFF
AE
NUR="RD.EXE RDGDI.MOD RDUSER.MOD RDKX.MOD RDCTL.MOD RDCTLX.MOD RDSND.MOD SPEAKER.DRV VGAM.DRV CGA.DRV SYS16.FNT SYS8.FNT SYS14.FNT DE.LNG RDTCP.MOD RDNET.EXE" bash tools/mkdisk.sh $IMG obj/ae_rdnet.txt
mcopy -o -i $IMG ../test/qemu/pkt/NE2000.COM ::/
mkdir -p obj/rdn/T1; printf 'FILES=20\r\nSHELL=A:\\RD2\\RD.EXE /P A:\\RD2\\SHELL.EXE\r\n' > obj/rdn/T1/CONFIG.SYS
mmd -i $IMG ::/T1 2>/dev/null; mcopy -o -i $IMG obj/rdn/T1/CONFIG.SYS ::/T1/
rm -f $SER $SOCK $HOSTLOG obj/rdnet?_*.ppm
python3 - > $HOSTLOG 2>&1 <<'PY' &
import socket, threading, time, struct, datetime
def ftp():
    s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', 21)); s.listen(2)
    while True:
        c, _ = s.accept(); c.sendall(b'220 RD2-Testserver bereit\r\n'); print('host: FTP verbunden', flush=True); time.sleep(1); c.close()
def irc():
    s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', 6667)); s.listen(2)
    while True:
        c, _ = s.accept(); print('host: IRC verbunden', flush=True); time.sleep(1); c.close()
def ntp():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(('127.0.0.1', 123))
    t = int(datetime.datetime(2026, 10, 26, 10, 30, 0, tzinfo=datetime.timezone.utc).timestamp()) + 2208988800
    while True:
        d, a = s.recvfrom(100); r = bytearray(48); r[0] = 0x1C; r[1] = 2; r[40:44] = struct.pack('>I', t); s.sendto(bytes(r), a); print('host: NTP', len(d), flush=True)
for f in (ftp, irc, ntp): threading.Thread(target=f, daemon=True).start()
time.sleep(280)
PY
HP=$!
sleep 0.5
timeout 270 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -netdev user,id=n0 -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_rdnet.py
Q() { python3 obj/qmon_rdnet.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; echo "FEHLER: warte auf $1 ($2)"; return 1; }
tippe() { local a=() c k; for ((i=0; i<${#1}; i++)); do c=${1:i:1}; case $c in .) k=dot;; *) k=$c;; esac; a+=("sendkey $k" sleep0.15); done; Q "${a[@]}"; }
lauf() {
  waitn "RDNET: Seite 1" 1 || return; sleep 3
  Q "screendump obj/rdnet1_vga.ppm" sleep0.3 "sendkey alt-b" sleep0.4 "sendkey alt-4"
  waitn "RDNET: Seite 4" 1 || return; sleep 2
  Q "sendkey alt-s" sleep0.3; tippe 10.0.2.2; Q "sendkey alt-v" sleep0.3; tippe 10.0.2.2; Q sleep0.5 "screendump obj/rdnet4_vga.ppm" sleep0.3 "sendkey alt-3"
  waitn "RDNET: Seite 3" 1 || return; sleep 2
  Q "sendkey alt-s" sleep0.3; tippe 10.0.2.2; Q sleep0.5 "screendump obj/rdnet3_vga.ppm" sleep0.3 "sendkey alt-2"
  waitn "RDNET: Seite 2" 1 || return; sleep 2
  Q "screendump obj/rdnet2_vga.ppm" sleep0.3 "sendkey alt-t"
  waitn "RDNET: Test: Fertig" 1 || return; sleep 1.5
  Q "screendump obj/rdnet5_vga.ppm" sleep0.3 "sendkey ret"
  waitn "RDNET: Test zu" 1 || return; sleep 2
  Q "sendkey ret"
  waitn "RDNET: Ende" 1 || return
  waitn "RDNET: Seite 1" 2 || return; sleep 3
  Q "screendump obj/rdnet6_cga.ppm" sleep0.3 "sendkey alt-4"
  waitn "RDNET: Seite 4" 2 || return; sleep 2
  Q "screendump obj/rdnet7_cga.ppm" sleep0.3 "sendkey esc"
}
lauf
wait $QP; kill $HP 2>/dev/null; wait $HP 2>/dev/null
S=$(tr -d '\r' < $SER)
grep -E "RDNET|RDTCP" <<< "$S"
cat $HOSTLOG
mcopy -o -i $IMG ::/RD2/RD2.INI obj/rdnet_ini.txt 2>/dev/null; mcopy -o -i $IMG ::/T1/CONFIG.SYS obj/rdnet_cfg.txt 2>/dev/null
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; fi; }
ok "INSTALL=-Zeile in CONFIG.SYS" "grep -q 'INSTALL=A:.T1.NET.NE2000.COM 0x60 3 0x300' obj/rdnet_cfg.txt && grep -q 'SHELL=A:' obj/rdnet_cfg.txt"
ok "RD2.INI FTP/IRC/NTP/Zone" "tr -d '\r' < obj/rdnet_ini.txt | grep -q '^Server=10.0.2.2' && tr -d '\r' < obj/rdnet_ini.txt | grep -q '^NTP=10.0.2.2' && tr -d '\r' < obj/rdnet_ini.txt | grep -q '^Zone=60'"
ok "Test: Netz mit DHCP" "grep -q 'Test: Adresse: 10.0.2.15 (DHCP)' <<< \"\$S\""
ok "Test: FTP-Begruessung" "grep -q 'Test: FTP: 220 RD2-Testserver' <<< \"\$S\""
ok "Test: IRC erreichbar" "grep -q 'Test: IRC-Server erreichbar' <<< \"\$S\""
ok "Test: Zeit 26.10.2026 11:30:00 (Berlin, Winterzeit)" "grep -q 'Test: Zeit: 26.10.2026 11:30:0' <<< \"\$S\""
ok "Speicher zurueck (2 Laeufe)" "[ \$(grep -c 'Speicher vollstaendig zurueck' <<< \"\$S\") -ge 2 ]"
for f in obj/rdnet?_*.ppm; do [ -f "$f" ] && python3 -c "from PIL import Image; Image.open('$f').save('${f%.ppm}.png')"; done
# Seite 2: aktuelle Adresse/Lease wechselt, Feld ausgeblendet
[ -f test/ref/rdnet1_vga.png ] && BV_MASKE="rdnet2:180,320,330,336" python3 test/bildvergleich.py $(for f in obj/rdnet?_*.png; do echo $f test/ref/$(basename $f); done)
exit 0
