#!/usr/bin/env python3
# comlausch.py PORT DATEI  - nimmt eine TCP-Verbindung an (Bochs com1: mode=socket-client) und schreibt sofort in DATEI
import socket, sys
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(('127.0.0.1', int(sys.argv[1]))); srv.listen(1); srv.settimeout(30)
c, _ = srv.accept(); f = open(sys.argv[2], 'wb', buffering=0)
while True:
    b = c.recv(4096)
    if not b: break
    f.write(b)
