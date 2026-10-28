# QEMU-Monitor steuern: python3 qmon.py "change floppy0 x.img" sleep2 "sendkey ret" text mode
import socket, time, os, sys
SOCK = os.environ.get('QMON', '/tmp/qmon.sock')

def mon(cmd):
    s = socket.socket(socket.AF_UNIX); s.connect(SOCK)
    b = b''
    while not b.endswith(b'(qemu) '): b += s.recv(4096)
    s.send((cmd + '\n').encode()); b = b''
    while b'(qemu) ' not in b: b += s.recv(4096)
    s.close()
def mode():
    mon('screendump /tmp/qd.ppm')
    return tuple(int(x) for x in open('/tmp/qd.ppm', 'rb').read(30).split()[1:3])
def text():
    s = socket.socket(socket.AF_UNIX); s.connect(SOCK)
    b = b''
    while not b.endswith(b'(qemu) '): b += s.recv(4096)
    s.send(b'xp /2000hx 0xb8000\n'); b = b''
    while not b.rstrip().endswith(b'(qemu)'): b += s.recv(65536)
    s.close()
    w = [int(t, 16) for t in b.decode('latin-1').replace('\r', '').split() if t.startswith('0x') and len(t) == 6]
    ch = ''.join(chr(x & 0xFF) if 32 <= (x & 0xFF) < 127 else ' ' for x in w)
    return '\n'.join(ch[i:i+80].rstrip() for i in range(0, len(ch), 80) if ch[i:i+80].strip())

if __name__ == '__main__':
    for c in sys.argv[1:]:
        if c == 'mode': print(mode())
        elif c == 'text': print(text())
        elif c.startswith('sleep'): time.sleep(float(c[5:]))
        else: mon(c)
