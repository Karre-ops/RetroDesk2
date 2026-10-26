# Kleiner FTP-Server fuer test/rdftp.sh: python3 test/ftpserver.py WURZEL PORT
# Passiv mit Adresse 10.0.2.2 (QEMU-Benutzernetz), Unix-LIST, USER retro / PASS geheim.
import socket, threading, os, sys
wurzel, port = os.path.abspath(sys.argv[1]), int(sys.argv[2])
def log(*a): print('host:', *a, flush=True)
def sitzung(c):
    f = c.makefile('rb'); cwd = '/'; pas = None; user = ''
    def tx(s): c.sendall((s + '\r\n').encode('latin-1'))
    def pfad(n): return os.path.normpath(os.path.join(wurzel + cwd, n)) if not n.startswith('/') else os.path.normpath(wurzel + n)
    tx('220 RD2-Testserver bereit')
    while True:
        z = f.readline()
        if not z: break
        z = z.decode('latin-1').rstrip('\r\n'); k, _, a = z.partition(' '); k = k.upper()
        log('<', z if k != 'PASS' else 'PASS ***')
        if k == 'USER': user = a; tx('331 Kennwort bitte')
        elif k == 'PASS': tx('230 angemeldet' if (user, a) == ('retro', 'geheim') else '530 falsch')
        elif k == 'TYPE': tx('200 Typ ' + a)
        elif k == 'SYST': tx('215 UNIX Type: L8')
        elif k == 'PWD': tx('257 "%s" ist hier' % cwd)
        elif k in ('CWD', 'CDUP'):
            p = pfad('..' if k == 'CDUP' else a)
            if os.path.isdir(p) and p.startswith(wurzel):
                cwd = p[len(wurzel):] or '/'
                if not cwd.startswith('/'): cwd = '/' + cwd
                tx('250 ok')
            else: tx('550 kein Ordner')
        elif k == 'PASV':
            pas = socket.socket(); pas.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); pas.bind(('127.0.0.1', 0)); pas.listen(1)
            pp = pas.getsockname()[1]; tx('227 Entering Passive Mode (10,0,2,2,%d,%d)' % (pp >> 8, pp & 255))
        elif k in ('LIST', 'RETR', 'STOR'):
            if not pas: tx('425 erst PASV'); continue
            p = pfad(a) if a else wurzel + cwd
            if k == 'RETR' and not os.path.isfile(p): tx('550 fehlt'); pas.close(); pas = None; continue
            tx('150 Daten kommen'); d, _ = pas.accept(); pas.close(); pas = None
            if k == 'LIST':
                for n in sorted(os.listdir(p)):
                    q = os.path.join(p, n); dz = os.path.isdir(q)
                    d.sendall(('%s 1 retro retro %8d Oct 26 10:00 %s\r\n' % ('drwxr-xr-x' if dz else '-rw-r--r--', 4096 if dz else os.path.getsize(q), n)).encode('latin-1'))
            elif k == 'RETR': d.sendall(open(p, 'rb').read())
            else:
                b = b''
                while True:
                    x = d.recv(4096)
                    if not x: break
                    b += x
                open(p, 'wb').write(b); log('STOR', a, len(b))
            d.close(); tx('226 fertig')
        elif k == 'DELE':
            p = pfad(a)
            if os.path.isfile(p): os.remove(p); tx('250 geloescht')
            else: tx('550 fehlt')
        elif k == 'MKD': os.mkdir(pfad(a)); tx('257 "%s" angelegt' % a)
        elif k == 'RMD': os.rmdir(pfad(a)); tx('250 entfernt')
        elif k == 'QUIT': tx('221 tschuess'); break
        else: tx('502 kenne ich nicht')
    c.close(); log('Sitzung zu')
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', port)); s.listen(4); s.settimeout(250)
try:
    while True:
        c, _ = s.accept(); log('verbunden'); threading.Thread(target=sitzung, args=(c,), daemon=True).start()
except socket.timeout: pass
