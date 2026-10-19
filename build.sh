#!/bin/bash
# RD2 bauen: SDK (RD2START.OBJ, RD2LIB.OBJ), Kern RD.EXE, Programme; Ergebnis in rd2/out, Budgetpruefung.
set -e
cd "$(dirname "$0")"
export WATCOM=${WATCOM:-/opt/ow} PATH=/opt/ow/binl64:/opt/ow/binl:$PATH INCLUDE=/opt/ow/h
O=out; mkdir -p $O obj
python3 tools/apicheck.py >/dev/null
python3 tools/mkapi.py >/dev/null
W="wasm -q -0"
$W -fo=obj/rd2start.obj sdk/RD2START.ASM
rm -f obj/lib_*.obj $O/RD2.LIB
for a in sdk/lib/F*.ASM; do b=$(basename $a .ASM); $W -fo=obj/lib_$b.obj $a; done
wlib -q -n $O/RD2.LIB obj/lib_*.obj
$W -fo=obj/notsup.obj   kern/NOTSUP.ASM
$W -fo=obj/enter.obj    kern/ENTER.ASM
# Kern: Small, SS != DS (API-Aufrufe kommen mit dem Stack des Programms)
wcc -0 -ms -zu -os -s -zq -w4 -bt=dos -i=sdk -fo=obj/rd.obj kern/RD.C
wlink option quiet system dos name $O/RD.EXE file obj/rd.obj,obj/enter.obj,obj/notsup.obj option map=obj/rd.map >/dev/null
# Programme: Medium, ohne Watcom-Startcode
app() {   # $1 Quelle  $2 EXE  $3 Anzeigename
  wcc -0 -mm -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/$2.obj $1
  wlink option quiet format dos name $O/$2 file obj/rd2start.obj,obj/$2.obj library $O/RD2.LIB option nodefaultlibs,map=obj/$2.map >/dev/null
  python3 tools/rd2link.py $O/$2 "$3" >/dev/null
}
app apps/leer/LEER.C LEER.EXE "Leer"
python3 tools/budget.py $O ${1:+$1}
