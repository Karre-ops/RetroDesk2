#!/usr/bin/env python3
"""mkrdrcdef.py AUS.H - Stilnamen fuer tools/RDRC.C aus sdk/RD2.H (gleiche Auswahl wie defines() in rdrc.py)"""
import re, sys, os
h = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sdk", "RD2.H")
d = {}
for l in open(h, encoding="latin-1"):
    m = re.match(r"#define\s+((?:WS|ES)_\w+)\s+(0x[0-9A-Fa-f]+|\d+)UL?", l)
    if m: d[m[1]] = int(m[2], 0)
d.update(STANDARD=d.get("ES_DEFAULT", 0x10000), AN=d.get("ES_CHECKED", 0x20000), GRAU=d["WS_DISABLED"])
o = ["/* erzeugt von tools/mkrdrcdef.py aus sdk/RD2.H */"]
for n in ("WS_DIALOG", "WS_TITLE", "WS_CHILD", "WS_VISIBLE"): o.append(f"#define {n}_V 0x{d[n]:08X}UL")
o.append("static const struct { char *n; unsigned long v; } stile[] = {")
o += [f'    {{ "{k}", 0x{v:08X}UL }},' for k, v in d.items()]
o.append("    { 0, 0 } };")
open(sys.argv[1], "w").write("\n".join(o) + "\n")
