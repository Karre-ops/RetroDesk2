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
$W -fo=obj/swap.obj kern/SWAP.ASM
$W -fo=obj/rdswap.obj kern/RDSWAP.ASM
wlink option quiet format raw bin name $O/RDSWAP.BIN file obj/rdswap.obj option offset=0 2>&1 | grep -v "W1014\|W1023" || true
cp ../test/qemu/CTMOUSE.EXE $O/CTMOUSE.EXE   # CuteMouse (GPL, unveraendert), siehe docs/LIZENZ.md
# Kern: Small, SS != DS (API-Aufrufe kommen mit dem Stack des Programms)
for c in RD MEM TASK FILE MOD RUNDOS; do wcc -0 -ms -zu -os -s -zq -w4 -bt=dos -i=sdk -fo=obj/k_$c.obj kern/$c.C; done
wlink option quiet system dos name $O/RD.EXE file obj/k_RD.obj,obj/k_MEM.obj,obj/k_TASK.obj,obj/k_FILE.obj,obj/k_MOD.obj,obj/k_RUNDOS.obj,obj/kasm.obj,obj/swap.obj,obj/notsup.obj option map=obj/rd.map >/dev/null
# Programme: Medium, ohne Watcom-Startcode
app() {   # $1 Quelle  $2 EXE  $3 Anzeigename
  wcc -0 -mm $APPFLAGS -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/$2.obj $1
  wlink option quiet format dos name $O/$2 file obj/rd2start.obj,obj/$2.obj library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs${APPSTACK:+,stack=$APPSTACK},map=obj/$2.map >/dev/null
  if [ -f "${1%.C}.RC" ]; then python3 tools/rdrc.py "${1%.C}.RC" obj/$2.res >/dev/null || exit 1; python3 tools/rd2link.py $O/$2 "$3" --res obj/$2.res >/dev/null; else python3 tools/rd2link.py $O/$2 "$3" >/dev/null; fi
}
cp apps/test/RESTEST.RDB $O/   # fuer BmpLoadFile im RESTEST
for l in DE EN; do python3 tools/rdrc.py --lng lng/$l.RC $O/$l.LNG >/dev/null || exit 1; done   # Systemtexte
# Module: Medium, -zu (laufen mit dem Stack des Aufrufers)
modul() {   # $1 Quelle  $2 Datei  $3 Anzeigename  $4 Flags  [$5 Typ M/D  $6 Compiler-Optionen]
  wcc -0 -mm -zu -os -s -zq -w4 -bt=dos -zl -i=sdk $6 -fo=obj/$2.obj $1
  wlink option quiet format dos name $O/$2 file obj/rd2mod.obj,obj/$2.obj library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,stack=16,map=obj/$2.map disable 1172 >/dev/null
  mv $O/$2.exe $O/$2 2>/dev/null || true
  python3 tools/rd2link.py $O/$2 "$3" --type ${5:-M} --flags $4 >/dev/null
}
app apps/leer/LEER.C LEER.EXE "Leer"
modul apps/test/TESTMOD.C TESTMOD.MOD "Testmodul" 0x0080
modul apps/test/TESTMOD.C TESTMOD2.MOD "Testmodul2" 0x0080
modul gdi/RDGDI.C RDGDI.MOD "RDGDI" 0 M -i=gdi
modul kern/RDKX.C RDKX.MOD "Kern Zusatz" 0x0080 M
modul kern/RDAUSL.C RDAUSL.MOD "Auslagern" 0 M
modul user/RDUSER.C RDUSER.MOD "RDUSER" 0 M "-i=gdi -ml -r"
modul gdi/RDGDIX.C RDGDIX.MOD "RDGDI Zusatz" 0x0080 M -i=gdi
modul ctl/RDCTL.C RDCTL.MOD "Steuerelemente" 0x0080 M "-ml -r"
modul ctl/RDCTLX.C RDCTLX.MOD "Dateidialog" 0x0080 M "-ml -r"
modul disk/RDDISK.C RDDISK.MOD "Laufwerke" 0x0080 M -i=disk
modul drv/GFX1.C CGA.DRV "CGA" 0 D -dCGA
modul drv/GFX1.C HGC.DRV "Hercules" 0 D -dHGC
modul drv/GFX1.C VGAM.DRV "VGA mono" 0 D -dVGAM
for t in MODTEST FILETEST HAENGT STRESS GDITEST GFXWAHL TEXTTEST INPTEST RESTEST WINTEST MENUTEST ZEIGTEST DLGTEST DISKFRAG DOPPFMT; do app apps/test/$t.C $t.EXE "$t"; done
app shell/SHELL.C SHELL.EXE "Shell"
app apps/uhr/UHR.C UHR.EXE "Uhr"   # voller Stapel: GetDC laedt Treiber und Module auf dem Stapel des Programms
# RDFORMAT: RD2-Oberflaeche + FreeDOS FORMAT 0.92a (GPL v2); FreeDOS-Dateien mit disk/fdshim.h (DOS ueber RDDISK.MOD)
FDF="createfs floppy hdisk main savefs bcread prf userint driveio getopt init recordbc uformat msghlpr"
for f in $FDF; do wcc -0 -mm -zp1 -os -s -zq -w0 -bt=dos -zl -fi=disk/fdshim.h -dRD2 -dNOCATS -i=disk/fdformat -fo=obj/fdf_$f.obj disk/fdformat/$f.c || exit 1; done
wcc -0 -mm -os -s -zq -w4 -bt=dos -zl -i=sdk -i=disk $RDF_DEF -fo=obj/RDFORMAT.EXE.obj disk/RDFORMAT.C || exit 1
wlink option quiet format dos name $O/RDFORMAT.EXE file obj/rd2start.obj,obj/RDFORMAT.EXE.obj$(for f in $FDF; do printf ",obj/fdf_$f.obj"; done) library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/RDFORMAT.EXE.map >/dev/null || { echo "RDFORMAT: Linkfehler"; wlink option quiet format dos name $O/RDFORMAT.EXE file obj/rd2start.obj,obj/RDFORMAT.EXE.obj$(for f in $FDF; do printf ",obj/fdf_$f.obj"; done) library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs 2>&1 | head -20; exit 1; }
python3 tools/rdrc.py disk/RDFORMAT.RC obj/RDFORMAT.res >/dev/null || exit 1; python3 tools/rd2link.py $O/RDFORMAT.EXE "Formatieren" --res obj/RDFORMAT.res >/dev/null
# RDFDISK: RD2-Oberflaeche + Free FDISK 1.3 (GPL v2): pcompute.c, pdiskio.c, Startcode bootnorm.asm (NASM)
FDK="pcompute pdiskio"
for f in $FDK; do wcc -0 -mm -zp1 -os -s -zq -w0 -bt=dos -zl -fi=disk/fdshim.h -dRD2 -i=disk/fdfdisk -fo=obj/fdk_$f.obj disk/fdfdisk/$f.c || exit 1; done
wcc -0 -mm -zp1 -os -s -zq -w0 -bt=dos -zl -fi=disk/fdshim.h -dRD2 -i=disk/fdfdisk -fo=obj/fdk_glue.obj disk/fdglue.c || exit 1
nasm -f bin -o obj/bootnorm.bin disk/fdfdisk/bootnorm.asm || exit 1
python3 -c "d=open('obj/bootnorm.bin','rb').read(); open('obj/bootnorm.c','w').write('char bootnormal_code[] = {'+','.join(str(b) for b in d)+'};\n')"
wcc -0 -mm -os -s -zq -bt=dos -zl -fo=obj/fdk_boot.obj obj/bootnorm.c || exit 1
wcc -0 -mm -os -s -zq -w4 -bt=dos -zl -i=sdk -i=disk -fo=obj/RDFDISK.EXE.obj disk/RDFDISK.C || exit 1
wlink option quiet format dos name $O/RDFDISK.EXE file obj/rd2start.obj,obj/RDFDISK.EXE.obj,obj/fdk_glue.obj,obj/fdk_boot.obj$(for f in $FDK; do printf ",obj/fdk_$f.obj"; done) library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/RDFDISK.EXE.map >/dev/null || { wlink option quiet format dos name $O/RDFDISK.EXE file obj/rd2start.obj,obj/RDFDISK.EXE.obj,obj/fdk_glue.obj,obj/fdk_boot.obj$(for f in $FDK; do printf ",obj/fdk_$f.obj"; done) library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/RDFDISK.EXE.map | grep -v "^$" | head -20; exit 1; }
python3 tools/rdrc.py disk/RDFDISK.RC obj/RDFDISK.res >/dev/null || exit 1; python3 tools/rd2link.py $O/RDFDISK.EXE "Festplatte" --res obj/RDFDISK.res >/dev/null
# Leistungsvergleich RD1 gegen RD2 (test/bench.sh): BENCH2 ueber die RD2-API, BENCH1 mit dem RD1-Renderer aus main
app bench/BENCH2.C BENCH2.EXE "Bench2"
( T=obj/bench1; mkdir -p $T; rm -f $T/*; cp bench/BENCH1.C $T/bench1.c; cp ../gfx.c ../mouse.c ../gfx.h ../mouse.h $T/
  cd $T && wcl -0 -ms -os -s -fpi -bt=dos -zq -fe=BENCH1.EXE bench1.c gfx.c mouse.c >/dev/null ) || echo "FEHLER BENCH1"
APPFLAGS=-fpi87 app apps/test/WURM.C WURM.EXE "Wurm"
for t in PING PONG RECHNE MEMTEST; do app apps/test/$t.C $t.EXE "$t"; done
app apps/test/DOSTEST.C DOSTEST.EXE "DOSTEST"
app apps/test/AUSLTEST.C AUSLTEST.EXE "AUSLTEST"
wcl -0 -ms -os -s -zq -bt=dos -fe=$O/FREIMEM.EXE -fo=obj/freimem.obj apps/test/FREIMEM.C >/dev/null
wcl -0 -ms -os -s -zq -bt=dos -fe=$O/TSRTEST.EXE -fo=obj/tsrtest.obj apps/test/TSRTEST.C >/dev/null
python3 tools/mkfont.py $O >/dev/null
# Einrichtung: normales DOS-Programm (Small, Watcom-Bibliothek), laeuft ohne RetroDesk
$W -fpi87 -fo=obj/setasm.obj setup/SETASM.ASM
# RDSYS.EXE: FreeDOS SYS (GPL v2) unveraendert als eigenes DOS-Programm, RD2SETUP ruft es mit /UPDATE (Festplatte startfaehig)
mkdir -p obj/sys; for t in 12 16; do nasm -dISFAT$t -i disk/fdsys/ disk/fdsys/boot.asm -o obj/sys/fat${t}com.bin || exit 1
  python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print('unsigned char %s[] = {' % sys.argv[2]); print(','.join('0x%02X' % c for c in d)); print('};')" obj/sys/fat${t}com.bin fat${t}com > obj/sys/fat${t}com.h; done
for f in sys fdkrncfg talloc kernel/prf; do wcc -0 -ms -zp1 -os -s -zq -bt=dos -DFORSYS -i=disk/fdsys/hdr -i=obj/sys -fo=obj/sys/${f##*/}.obj disk/fdsys/$f.c >/dev/null || exit 1; done
wlink option quiet system dos name $O/RDSYS.EXE file obj/sys/sys.obj,obj/sys/fdkrncfg.obj,obj/sys/talloc.obj,obj/sys/prf.obj >/dev/null || exit 1
wcc -0 -ms -os -s -zq -w4 -bt=dos -fo=obj/rd2setup.obj setup/RD2SETUP.C
wlink option quiet system dos name $O/RD2SETUP.EXE file obj/rd2setup.obj,obj/setasm.obj option map=obj/rd2setup.map >/dev/null
# G12-Test: LEER.EXE mit Mindestzuteilung FFFFh (Bild + minalloc laeuft ueber 64K Paragraphen), muss RD_ERR_FORMAT liefern
python3 -c "import sys; d=bytearray(open('$O/LEER.EXE','rb').read()); d[10:12]=b'\xff\xff'; open('$O/UEBER.EXE','wb').write(d)"
python3 tools/budget.py $O ${1:+$1}
