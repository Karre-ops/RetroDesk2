#!/usr/bin/env python3
# Belegungsdateien fuer RDKEYB (31.10.2026): out/DE.KBD, out/US.KBD, out/PC10.KBD.
# Aufbau: 'RDKBD',1, dann 178 Bytes wie im residenten RDKEYB: yz (1 = Strg/Alt+y/z tauschen),
# tab (Scancodes 02h..35h je normal, Umschalt, AltGr; 0 = BIOS-Zeichen lassen, 1..3 = Tottaste ^ ´ `),
# tab56 (Taste 56h <>| der 102er-Tastatur), totab (Tottasten: a e i o u, Zeichen allein).
# DE kommt aus RDKEYB.COM selbst (eine Quelle), die anderen sind Abwandlungen davon.
import sys
O = sys.argv[1] if len(sys.argv) > 1 else 'out'
com = open(f'{O}/RDKEYB.COM', 'rb').read()
i = com.index(b'RDKB') + 22                      # sig, modus, e0, altgr, tot, alt9, alt15, mit4f, letzt, alt2f
LEN = 1 + 52 * 3 + 3 + 18
de = bytearray(com[i:i + LEN])
assert de[0] == 1 and de[1 + 0x0C * 3 - 6] == 0xE1, 'RDKEYB.COM: DE-Tabelle nicht gefunden'
def setz(t, sc, n, s, a):
    o = 1 + (sc - 2) * 3
    t[o:o + 3] = bytes([n, s, a])
def schreib(name, t):
    assert len(t) == LEN
    open(f'{O}/{name}.KBD', 'wb').write(b'RDKBD\x01' + bytes(t))
schreib('DE', de)
# US: nichts ersetzen, kein y/z-Tausch (Tottasten-Tabelle bleibt, wird nie benutzt)
us = bytearray(de); us[0] = 0; us[1:1 + 52 * 3 + 3] = bytes(52 * 3 + 3)
schreib('US', us)
# PC10: Commodore PC10-III (XT-Tastatur, 83 Tasten), Belegung wie aufgedruckt (Karsten, 31.10.2026).
# Keine Tottasten. AltGr = Strg+Alt (eine AltGr-Taste gibt es nicht). # sitzt auf 29h, <> auf 2Bh (XT-Plaetze).
pc = bytearray(de)
C = lambda c: ord(c) if isinstance(c, str) else c
for sc, n, s, a in [
    (0x03, 0, '"', '@'),        # 2 " @
    (0x04, 0, 0x15, 0),         # 3 §
    (0x05, 0, 0, 0xF8),         # 4 $ °
    (0x08, 0, '/', '|'),        # 7 / ¦ (als |)
    (0x09, 0, '(', 0),          # 8 (
    (0x0A, 0, ')', 0),          # 9 )
    (0x0B, 0, '=', 0),          # 0 =
    (0x0C, 0xE1, '?', '/'),     # ß ? /
    (0x0D, 0x27, '`', '~'),     # ´ ` ~ (´ als ', CP437 hat kein ´)
    (0x10, 0, 0, 0),            # q (kein @)
    (0x1A, 0x81, 0x9A, '['),    # ü Ü [
    (0x1B, '+', '*', ']'),      # + * ]
    (0x28, 0x84, 0x8E, '{'),    # ä Ä {
    (0x29, '#', '^', '}'),      # # ^ }
    (0x2B, '<', '>', '\\'),     # < > \
    (0x32, 0, 0, 0),            # m (kein µ)
]:
    setz(pc, sc, C(n), C(s), C(a))
schreib('PC10', pc)
