#!/usr/bin/env python3
# Kleiner RFB-(VNC-)Client fuer Bochs (display_library: rfb): schickt Mausereignisse.
# rfbmaus.py PORT befehl...   befehle: w<s> (warten), f<datei>:<text> (warten bis Text in Datei, max. 60 s), m<x>,<y> (Zeiger absolut), d<maske> (Tasten: 1 links, 2 Mitte, 4 rechts), c (Mausfang an/aus: Strg+Mitte)
import socket, struct, sys, time
port = int(sys.argv[1]); s = None
for i in range(60):
    try: s = socket.create_connection(('127.0.0.1', port), 2); break
    except OSError: time.sleep(0.5)
if not s: sys.exit('keine Verbindung')
def rd(n):
    b = b''
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c: raise SystemExit('Verbindung zu')
        b += c
    return b
ver = rd(12); s.sendall(b'RFB 003.003\n')
sec = struct.unpack('>I', rd(4))[0]
if sec != 1: sys.exit('Sicherheit %d' % sec)
s.sendall(b'\x01')
w, h = struct.unpack('>HH', rd(4)); rd(16); n = struct.unpack('>I', rd(4))[0]; rd(n)
s.settimeout(0.05)
x, y, m = w // 2, h // 2, 0
def ptr():
    s.sendall(struct.pack('>BBHH', 5, m, x, y))
def leer():
    try:
        while s.recv(65536): pass
    except OSError: pass
for c in sys.argv[2:]:
    if c[0] == 'w': t = time.time() + float(c[1:])
    else: t = 0
    if c[0] == 'f':
        fn, tx = c[1:].split(':', 1); t = time.time() + 60
        while time.time() < t:
            try:
                if tx in open(fn, errors='replace').read(): break
            except OSError: pass
            leer(); time.sleep(0.2)
        else: sys.exit('Text %r kommt nicht' % tx)
        t = 0
    elif c[0] == 'c':
        s.sendall(struct.pack('>BBHI', 4, 1, 0, 0xffe3)); time.sleep(0.1)
        s.sendall(struct.pack('>BBHH', 5, 2, x, y)); time.sleep(0.1)
        s.sendall(struct.pack('>BBHH', 5, 0, x, y)); time.sleep(0.1)
        s.sendall(struct.pack('>BBHI', 4, 0, 0, 0xffe3))
    elif c[0] == 'm': x, y = [int(v) for v in c[1:].split(',')]; ptr()
    elif c[0] == 'd': m = int(c[1:]); ptr()
    while time.time() < t: leer(); time.sleep(0.05)
    leer(); time.sleep(0.15)
print('rfb %dx%d ok' % (w, h))
