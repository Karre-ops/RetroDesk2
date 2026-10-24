#!/bin/bash
# RunDos (SPEC 14): DOSTEST lagert RD2 aus, startet FREIMEM.EXE (normales DOS-Programm) und
# kommt zurueck. Laeufe: VGA/386 mit XMS, CGA/8086 ohne XMS (Auslagerdatei auf C:),
# Hercules/8086 ohne /D. Prueft Rueckgabecode 42, Befehlszeile, unveraenderte Daten,
# Grafik danach, Fehler bei fehlendem Programm, Speicher zurueck, Auslagerdatei geloescht,
# und wie viel Speicher das DOS-Programm bekommt. Zusaetzlich TSRTEST (bleibt resident, Audit M10) mit XMS
# und mit Auslagerdatei: der Rest entfernt es, Vektor 60h und freier Speicher wie vorher.
cd "$(dirname "$0")/.."
R=obj/dbxr; F=0
LAEUFE=${LAEUFE:-"vgaonly,386,true,/D cga,8086,false,/D hercules,8086,false,- vgaonly,386,true,/D,TSR cga,8086,false,/D,TSR"}
for lauf in $LAEUFE; do lauf=${lauf//,/ }
  set -- $lauf; D=$4; [ $D = - ] && D=""; P=FREIMEM; [ "$5" = TSR ] && P=TSRTEST
  python3 -c "import shutil; shutil.rmtree('$R', True)"; mkdir -p $R/RD2
  cp out/RD.EXE out/*.MOD out/*.DRV out/RDSWAP.BIN out/DOSTEST.EXE out/FREIMEM.EXE out/TSRTEST.EXE $R/RD2/
  printf '[sdl]\nfullscreen=false\n[dosbox]\nmachine=%s\nmemsize=4\n[cpu]\ncore=normal\ncputype=%s\ncycles=fixed 4000\n[mixer]\nnosound=true\n[speaker]\npcspeaker=false\n[serial]\nserial1=file file:'$PWD/$R'/com1.txt\n[dos]\nxms=%s\numb=false\n' $1 $2 $3 > $R/dbx.conf
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -s KILL ${TO:-200} dosbox -conf $R/dbx.conf -c "mount c $R" -c "c:" -c "cd \\RD2" \
    -c "C:\\RD2\\RD.EXE $D DOSTEST.EXE C:\\RD2\\$P.EXE C:\\FREI.TXT > \\LOG.TXT" -c exit >/dev/null 2>&1
  L=$(tr -d '\r' < $R/LOG.TXT); FR=$(tr -d '\r' < $R/FREI.TXT 2>/dev/null | head -1)
  chk() { if echo "$L" | grep -q "$1"; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
  if [ $P = TSRTEST ]; then
    echo "=== $1 $2 xms=$3: residentes Programm"
    chk "residentes Programm entfernt" "Hinweis des Rests"
    chk "DOSTEST: Vektor 60h gleich 0x00000001" "Vektor 60h zurueck"
    chk "DOSTEST: Abweichungen 0x00000000" "Daten unveraendert"
    chk "DOSTEST: Grafik nachher 0x00000001" "Grafik danach"
    chk "DOSTEST: fertig" "zurueck im RD2-Programm"
    V=$(echo "$L" | grep "frei vorher" | awk '{print $NF}'); N=$(echo "$L" | grep "frei danach" | awk '{print $NF}')
    [ -n "$V" ] && [ "$V" = "$N" ] && echo "ok     freier Speicher wie vorher ($V)" || { echo "FEHLER freier Speicher $V / $N"; F=1; }
    chk "Speicher vollstaendig zurueck" "Speicher zurueck"
    [ -f $R/RD2/RD2SWAP.\$\$\$ ] && { echo "FEHLER Auslagerdatei liegt noch da"; F=1; }
    continue
  fi
  echo "=== $1 $2 xms=$3 ${D:-ohne /D}: DOS-Programm bekam ${FR#FREI }"
  if [ -n "$D" ]; then
    chk "DOSTEST: Rueckgabe 0x0000002A" "Rueckgabecode 42"
    chk "DOSTEST: Vektor 60h gleich 0x00000001" "Vektoren unberuehrt"
    chk "DOSTEST: Abweichungen 0x00000000" "Daten unveraendert"
    chk "DOSTEST: Grafik nachher 0x00000001" "Grafik danach"
    chk "DOSTEST: fehlendes Programm 0x0000FFFF" "fehlendes Programm abgelehnt"
    chk "DOSTEST: fertig" "zurueck im RD2-Programm"
  fi
  chk "Speicher vollstaendig zurueck" "Speicher zurueck"
  grep -q "ARG hallo" $R/FREI.TXT 2>/dev/null && echo "ok     Befehlszeile" || { echo "FEHLER Befehlszeile"; F=1; }
  [ -f $R/RD2/RD2SWAP.\$\$\$ ] && { echo "FEHLER Auslagerdatei liegt noch da"; F=1; }
done
[ $F = 0 ] && echo "RunDos: alles ok"
exit $F
