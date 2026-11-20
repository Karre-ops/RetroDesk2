# Hilfe fuer die Installationstests (PLAN_32BIT, 20.11.2026): zwei RD2-Testdateien mit BEDARF auf ein Diskettenbild.
# XB8.EXE (CPU 8088) wird installiert, XB586.EXE (CPU 586) auf dem 486 (QEMU -cpu 486) weggelassen.
# Aufruf: . test/xbedarf.sh; xbedarf BILD
xbedarf() {
  printf 'BEDARF CPU 8088\n' > obj/xb8.rc; printf 'BEDARF CPU 586\n' > obj/xb586.rc
  for n in 8 586; do cp out/LEER.EXE obj/XB$n.EXE; python3 tools/rdrc.py obj/xb$n.rc obj/xb$n.res >/dev/null && python3 tools/rd2link.py obj/XB$n.EXE "XB$n" --res obj/xb$n.res >/dev/null
    mcopy -o -i $1 obj/XB$n.EXE ::/RD2/ || echo "FEHLER XB$n.EXE passt nicht auf $1"; done
}
xbpruef() {   # $1 = mtools-Ziel (z. B. "-i obj/x.img@@32256"), $2 = Ordner
  mdir $1 ::/$2/XB8.EXE >/dev/null 2>&1 && echo "ok     $2/XB8.EXE (BEDARF CPU 8088) installiert" || { echo "FEHLER $2/XB8.EXE fehlt"; F=1; }
  mdir $1 ::/$2/XB586.EXE >/dev/null 2>&1 && { echo "FEHLER $2/XB586.EXE (BEDARF CPU 586) trotz 486 installiert"; F=1; } || echo "ok     $2/XB586.EXE auf dem 486 weggelassen"
}
