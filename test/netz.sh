#!/bin/bash
# Punkt 8: RDTCP.MOD mit NE2000 (Crynwr-Paketreiber) in QEMU, Netz "user" (DHCP 10.0.2.15, Host 10.0.2.2).
# Auf dem Host: TCP-Echo 7007, UDP-Echo 7008 (Grossbuchstaben), Verbindung zum Lauscher (Host 12121 -> RD2 2121).
cd "$(dirname "$0")/.."
IMG=obj/netz.img; SER=obj/netz_com1.txt; SOCK=/tmp/qmon_netz.sock; HOSTLOG=obj/netz_host.txt
cat > obj/ae_netz.txt <<'AE'
@echo off
A:\NE2000.COM 0x60 3 0x300
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\NETTEST.EXE
A:\FDAPM.COM POWEROFF
AE
NUR="${NUR:-RD.EXE RDGDI.MOD RDUSER.MOD RDKX.MOD RDCTL.MOD RDSND.MOD SPEAKER.DRV VGAM.DRV SYS16.FNT SYS8.FNT SYS14.FNT DE.LNG RDTCP.MOD NETTEST.EXE}" bash tools/mkdisk.sh $IMG obj/ae_netz.txt
mcopy -o -i $IMG test/qemu/pkt/NE2000.COM ::/
rm -f $SER $SOCK $HOSTLOG
python3 - > $HOSTLOG 2>&1 <<'PY' &
import socket, threading, time, sys
def tcp_echo():
    s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', 7007)); s.listen(1)
    c, _ = s.accept(); n = 0
    while True:
        d = c.recv(4096)
        if not d: break
        n += len(d); c.sendall(d)
    print('host: TCP-Echo', n, flush=True); c.close()
def udp_echo():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(('127.0.0.1', 7008))
    d, a = s.recvfrom(2000); s.sendto(d.upper(), a); print('host: UDP', d, flush=True)
def lauscher():
    for i in range(240):
        try:
            if 'NETTEST: lauscht' in open('obj/netz_com1.txt', errors='replace').read(): break
        except FileNotFoundError: pass
        time.sleep(0.5)
    time.sleep(1)
    c = socket.create_connection(('127.0.0.1', 12121), timeout=20); c.sendall(b'hallo\r\n')
    d = b''
    try:
        while True:
            x = c.recv(100)
            if not x: break
            d += x
    except Exception as e: print('host: Fehler', e, flush=True)
    print('host: Lauscher Antwort', d, flush=True)
for f in (tcp_echo, udp_echo, lauscher): threading.Thread(target=f, daemon=True).start()
time.sleep(170)
PY
HP=$!
sleep 0.5
timeout 160 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -netdev user,id=n0,hostfwd=tcp:127.0.0.1:12121-:2121 -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 -object filter-dump,id=f1,netdev=n0,file=obj/netz.pcap \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1
kill $HP 2>/dev/null; wait $HP 2>/dev/null
tr -d '\r' < $SER | grep -A1 -E "NETTEST|RDTCP|RD: (Fehler|SPEICHER|alle)" | grep -v "^--"
cat $HOSTLOG
S=$(tr -d '\r' < $SER)
ok() { grep -q "$2" <<< "$S" && echo "ok     $1" || echo "FEHLER $1"; }
ok "DHCP: Netz bereit" "RDTCP: Netz bereit"
grep -A1 "RDTCP: Netz bereit" <<< "$S" | grep -q "10.0.2.15" && echo "ok     Adresse 10.0.2.15" || echo "FEHLER Adresse"
grep -A1 "NETTEST: Echo Bytes" <<< "$S" | grep -q "00004E20" && grep -A1 "NETTEST: Echo Fehler" <<< "$S" | grep -q "00000000" && echo "ok     TCP-Echo 20000 Bytes fehlerfrei" || echo "FEHLER TCP-Echo"
grep -q "host: UDP b'ping RD2'" $HOSTLOG && grep -A1 "NETTEST: UDP Antwort" <<< "$S" | grep -q "PING RD2" && echo "ok     UDP hin und zurueck" || echo "FEHLER UDP"
grep -q "Lauscher Antwort b'welt" $HOSTLOG && grep -A1 "angenommen, Text" <<< "$S" | grep -q hallo && echo "ok     Lauschen und Annehmen" || echo "FEHLER Lauschen"
grep -A1 "NETTEST: Ende, Fehler" <<< "$S" | grep -q "00000000" && echo "ok     NETTEST ohne Fehler" || echo "FEHLER NETTEST meldet Fehler"
grep -q "Speicher vollstaendig zurueck" <<< "$S" && echo "ok     Speicher zurueck" || echo "FEHLER Speicher"
grep -A1 "NETTEST: Resolve Ergebnis" <<< "$S" | tail -1 | grep -q "00000000" && echo "info   DNS example.com aufgeloest" || echo "info   DNS example.com nicht aufgeloest (Internet?)"
exit 0
