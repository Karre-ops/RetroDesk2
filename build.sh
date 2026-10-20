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
$W -fo=obj/rd2mod.obj   sdk/RD2MOD.ASM
rm -f obj/lib_*.obj $O/RD2.LIB
for a in sdk/lib/F*.ASM; do b=$(basename $a .ASM); $W -fo=obj/lib_$b.obj $a; done
wlib -q -n $O/RD2.LIB obj/lib_*.obj
$W -fo=obj/notsup.obj   kern/NOTSUP.ASM
$W -fpi87 -fo=obj/kasm.obj kern/KASM.ASM
# Kern: Small, SS != DS (API-Aufrufe kommen mit dem Stack des Programms)
for c in RD MEM TASK FILE MOD; do wcc -0 -ms -zu -os -s -zq -w4 -bt=dos -i=sdk -fo=obj/k_$c.obj kern/$c.C; done
wlink option quiet system dos name $O/RD.EXE file obj/k_RD.obj,obj/k_MEM.obj,obj/k_TASK.obj,obj/k_FILE.obj,obj/k_MOD.obj,obj/kasm.obj,obj/notsup.obj option map=obj/rd.map >/dev/null
# Programme: Medium, ohne Watcom-Startcode
app() {   # $1 Quelle  $2 EXE  $3 Anzeigename
  wcc -0 -mm $APPFLAGS -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/$2.obj $1
  wlink option quiet format dos name $O/$2 file obj/rd2start.obj,obj/$2.obj library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/$2.map >/dev/null
  python3 tools/rd2link.py $O/$2 "$3" >/dev/null
}
# Module: Medium, -zu (laufen mit dem Stack des Aufrufers)
modul() {   # $1 Quelle  $2 Datei  $3 Anzeigename  $4 Flags
  wcc -0 -mm -zu -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/$2.obj $1
  wlink option quiet format dos name $O/$2 file obj/rd2mod.obj,obj/$2.obj library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/$2.map >/dev/null
  mv $O/$2.exe $O/$2 2>/dev/null || true
  python3 tools/rd2link.py $O/$2 "$3" --type M --flags $4 >/dev/null
}
app apps/leer/LEER.C LEER.EXE "Leer"
modul apps/test/TESTMOD.C TESTMOD.MOD "Testmodul" 0x0080
for t in MODTEST FILETEST HAENGT STRESS; do app apps/test/$t.C $t.EXE "$t"; done
APPFLAGS=-fpi87 app apps/test/WURM.C WURM.EXE "Wurm"
for t in PING PONG RECHNE MEMTEST; do app apps/test/$t.C $t.EXE "$t"; done
python3 tools/budget.py $O ${1:+$1}
