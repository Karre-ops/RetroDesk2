#!/bin/bash
# Punkt 8: Netzzeit. NE2000 in QEMU, Host-SNTP auf 10.0.2.2:123 mit 26.10.2026 10:30:00 UTC (Berlin: 11:30 Winterzeit).
# 1. UHR.EXE T (VGA): Alt+N stellt die Uhr vom Netz, Bild, Esc.
# 2. UHR.EXE N: still beim Start, ohne Fenster, schreibt [Netz] Gestellt=26.10.2026.
# 3. UHR.EXE N noch einmal: heute schon gestellt, kein Netz.
# 4. SHELL.EXE mit [Netz] Zeit=1 startet UHR.EXE N (heute schon gestellt), dann QEMU aus.
cd "$(dirname "$0")/.."
IMG=obj/netzzeit.img; SER=obj/netzzeit_com1.txt; SOCK=/tmp/qmon_nz.sock; HOSTLOG=obj/netzzeit_host.txt
cat > obj/ae_nz.txt <<'AE'
@echo off
rem RDTCP.MOD (fuer mkdisk)
A:\NE2000.COM 0x60 3 0x300
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\UHR.EXE T
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\UHR.EXE N
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\UHR.EXE N
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\SHELL.EXE
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_nz.txt >/dev/null   # 360 KB reichen nicht fuer Shell, Netz und Paketreiber
mcopy -o -i $IMG test/qemu/pkt/NE2000.COM ::/
rm -f obj/nz_ini.txt; mcopy -o -i $IMG ::/RD2/RD2.INI obj/nz_ini.txt 2>/dev/null
python3 - <<'PY'
import os
s = (open('obj/nz_ini.txt', 'rb').read() if os.path.exists('obj/nz_ini.txt') else b'').decode('latin-1').replace('\r\n', '\n').rstrip('\n').split('\n')
s = [l for l in s if l and not l.startswith('\x1a')]
s += ['[Netz]', 'DHCP=1', 'NTP=10.0.2.2', 'Zone=60', 'Sommer=1', 'Zeit=1']
open('obj/nz_ini.txt', 'wb').write(('\r\n'.join(s) + '\r\n').encode('latin-1'))
PY
mcopy -o -i $IMG obj/nz_ini.txt ::/RD2/RD2.INI
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
timeout 240 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -netdev user,id=n0 -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_nz.py
Q() { python3 obj/qmon_nz.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; echo "FEHLER: warte auf $1 ($2)"; return 1; }
lauf() {
  waitn "UHR: bereit" 1 || return; sleep 3
  Q "sendkey alt-n"
  waitn "UHR: Netzzeit" 1 || return; sleep 2
  Q "screendump obj/netzzeit1_vga.ppm" sleep0.3 "sendkey esc"
  waitn "UHR: Netzzeit" 2 || return
  waitn "UHR: heute schon" 1 || return
  waitn "SHELL: bereit" 1 || return
  waitn "UHR: heute schon" 2 || return; sleep 2
  Q "screendump obj/netzzeit2_vga.ppm"; kill $QP 2>/dev/null   # quit ueber den Monitor wartet ewig auf Antwort
}
lauf
sleep 1; kill $QP 2>/dev/null; wait $QP 2>/dev/null; kill $HP 2>/dev/null; wait $HP 2>/dev/null
S=$(tr -d '\r' < $SER)
grep -A1 -E "^UHR: (Start|Netzzeit|heute)" <<< "$S" | grep -v "^--"; grep -E "RDTCP" <<< "$S"; cat $HOSTLOG
mcopy -o -i $IMG ::/RD2/RD2.INI obj/nz_ini_nach.txt 2>/dev/null
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; fi; }
ok "Alt+N: DOS-Zeit 26.10.2026 11:30 (Berlin, Winterzeit)" "grep -A1 '^UHR: Netzzeit$' <<< \"\$S\" | sed -n 2p | grep -qE '^26.10.2026 11:30:0'"
ok "Start still: gestellt, Gestellt= in RD2.INI" "[ \$(grep -c '^UHR: Netzzeit$' <<< \"\$S\") = 2 ] && tr -d '\r' < obj/nz_ini_nach.txt | grep -q '^Gestellt=26.10.2026'"
ok "Zweiter Start am selben Tag ohne Netz" "[ \$(grep -c 'UHR: heute schon gestellt' <<< \"\$S\") = 2 ] && [ \$(grep -c '^host: NTP' $HOSTLOG) = 2 ]"
ok "Shell startet UHR N bei Zeit=1" "grep -A1 'UHR: Start' <<< \"\$S\" | grep -c '^N\$' | grep -q 3"
ok "Speicher zurueck (3 Laeufe)" "[ \$(grep -c 'Speicher vollstaendig zurueck' <<< \"\$S\") -ge 3 ]"
[ -f test/ref/netzzeit1_vga.png ] && python3 test/bildvergleich.py obj/netzzeit1_vga.ppm test/ref/netzzeit1_vga.png
exit 0
