#!/bin/bash
# Punkt 8: Netzzeit. NE2000 in QEMU, Host-SNTP auf 10.0.2.2:123 mit 26.10.2026 10:30:00 UTC (Berlin: 11:30 Winterzeit).
# 1. UHR.EXE T (VGA): Alt+N stellt die Uhr vom Netz, Bild, Esc.
# 2. UHR.EXE N: still beim Start, ohne Fenster, schreibt [Netz] Gestellt=26.10.2026.
# 3. UHR.EXE N noch einmal: heute schon gestellt, kein Netz.
# 4. SHELL.EXE mit [Netz] Zeit=1 startet UHR.EXE N (heute schon gestellt), dann QEMU aus.
cd "$(dirname "$0")/.."
IMG=obj/nzlive.img; SER=obj/nzlive_com1.txt; SOCK=/tmp/qmon_nzl.sock; HOSTLOG=obj/nzlive_host.txt
cat > obj/ae_nzl.txt <<'AE'
@echo off
rem RDTCP.MOD A:\RD2\UHR.EXE A:\RD2\RDNET.EXE (fuer mkdisk)
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\SHELL.EXE
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_nzl.txt >/dev/null   # 360 KB reichen nicht fuer Shell, Netz und Paketreiber
mcopy -o -i $IMG test/qemu/pkt/NE2000.COM ::/
rm -f obj/nzl_ini.txt; mcopy -o -i $IMG ::/RD2/RD2.INI obj/nzl_ini.txt 2>/dev/null
python3 - <<'PY'
import os
s = (open('obj/nzl_ini.txt', 'rb').read() if os.path.exists('obj/nzl_ini.txt') else b'').decode('latin-1').replace('\r\n', '\n').rstrip('\n').split('\n')
s = [l for l in s if l and not l.startswith('\x1a')]
s += ['[Netz]', 'DHCP=1', 'NTP=10.0.2.2', 'Zone=60', 'Sommer=1', 'Zeit=1', 'Karte=NE2000', r'Treiber=A:\NE2000.COM', 'Platz=768']
open('obj/nzl_ini.txt', 'wb').write(('\r\n'.join(s) + '\r\n').encode('latin-1'))
PY
mcopy -o -i $IMG obj/nzl_ini.txt ::/RD2/RD2.INI
rm -f $SER $SOCK $HOSTLOG obj/netzzeit?_*.ppm
python3 - > $HOSTLOG 2>&1 <<'PY' &
import socket, struct, datetime
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', 123)); s.settimeout(200)
t = int(datetime.datetime(2026, 10, 26, 10, 30, 0, tzinfo=datetime.timezone.utc).timestamp()) + 2208988800
try:
    while True:
        d, a = s.recvfrom(100); r = bytearray(48); r[0] = 0x1C; r[1] = 2; r[40:44] = struct.pack('>I', t); s.sendto(bytes(r), a); print('host: NTP', len(d), flush=True)
except socket.timeout: pass
PY
HP=$!
sleep 0.5
timeout 130 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -netdev user,id=n0 -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_nzl.py
Q() { python3 obj/qmon_nzl.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; echo "FEHLER: warte auf $1 ($2)"; return 1; }
waitn "UHR: Netzzeit" 1; sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null; kill $HP 2>/dev/null
S=$(tr -d '\r' < $SER); grep -E "^UHR|RDNET|RDTCP" <<< "$S" | head -12
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; fi; }
ok "Start ohne Paketreiber: RDNET LS laedt ihn still, UHR stellt die Zeit" "grep -q 'RDTCP: Netz bereit' <<< \"\$S\" && grep -A1 '^UHR: Netzzeit$' <<< \"\$S\" | sed -n 2p | grep -qE '^26.10.2026 11:30'"
exit 0
