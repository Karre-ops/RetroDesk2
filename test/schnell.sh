#!/bin/bash
# Schnelltest vor jedem Commit (25.10.2026): Bildvergleich CGA/VGA (gdi.sh), Shell CGA/VGA (desk.sh),
# Dialoge (dlg.sh). Etwa 2 Minuten. Voller Lauf vor Releases und nach Kern-/Speicheraenderungen.
cd "$(dirname "$0")/.."
f=0
for t in gdi desk dlg; do
  o=$(timeout 300 bash test/$t.sh 2>&1); n=$(echo "$o" | grep -c '^ok'); b=$(echo "$o" | grep -c 'FEHLER')
  [ "$b" = 0 ] && [ "$n" -gt 0 ] && echo "ok     $t ($n ok)" || { echo "FEHLER $t ($n ok, $b Fehler)"; echo "$o" | grep FEHLER | head -3; f=1; }
done
exit $f
