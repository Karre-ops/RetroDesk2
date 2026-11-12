#!/bin/bash
# RD2 bauen: SDK (RD2START.OBJ, RD2LIB.OBJ), Kern RD2.EXE, Programme; Ergebnis in rd2/out, Budgetpruefung.
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
for a in sdk/lib/F*.ASM; do b=$(basename $a .ASM); case $b in F12F|F130) continue;; esac; $W -fo=obj/lib_$b.obj $a; done   # F12F/F130 (JoyRead/JoyCalibrate): Joystick in C (JOY.C)
wcc -0 -mm -zu -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/lib_WAV.obj sdk/WAV.C   # Hilfen in C (laufen im Programm)
wcc -0 -mm -zu -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/lib_JOY.obj sdk/JOY.C   # Joystick (Gameport 201h, PIT)
wlib -q -n $O/RD2.LIB obj/lib_*.obj
$W -fo=obj/notsup.obj   src/kern/NOTSUP.ASM
$W -fpi87 -fo=obj/kasm.obj src/kern/KASM.ASM
$W -fo=obj/swap.obj src/kern/SWAP.ASM
$W -fo=obj/rdswap.obj src/kern/RDSWAP.ASM
wlink option quiet format raw bin name $O/RDSWAP.BIN file obj/rdswap.obj option offset=0 2>&1 | grep -v "W1014\|W1023" || true
python3 -c "import sys; d=open('$O/RDSWAP.BIN','rb').read(); assert not any(d[:400]) and len(d)<=2240, 'RDSWAP: Code ueber CODEMAX'; open('$O/RDSWAP.BIN','wb').write(d[400:])"   # leeren Datenbereich abschneiden, Kern laedt nach S_DATA
(cd obj && wasm -q -0 -fo=rdkeyb.obj ../src/drv/RDKEYB.ASM && wlink option quiet format dos com name ../$O/RDKEYB.COM file rdkeyb.obj) && python3 tools/mkkbd.py $O   # Tastatur DE/US, Belegungen DE/US/PC10.KBD
(cd obj && wasm -q -0 -fo=rdmaus.obj ../src/drv/RDMAUS.ASM && wlink option quiet format dos com name ../$O/RDMAUS.COM file rdmaus.obj)   # Maus: Bus, InPort, PS/2, seriell
nasm -f bin -o obj/ZTEST.COM test/dos/ZTEST.ASM   # Testprogramm fuer test/rdmaus.sh
nasm -f bin -o obj/OBEN.COM test/dos/OBEN.ASM     # Testprogramm fuer test/rundos.sh: beschreibt den Speicher bis A000h
# Kern: Small, SS != DS (API-Aufrufe kommen mit dem Stack des Programms)
for c in RD MEM TASK FILE MOD RUNDOS; do wcc -0 -ms -zu -os -ol -s -zq -w4 -bt=dos -i=sdk -fo=obj/k_$c.obj src/kern/$c.C; done
wlink option quiet system dos name $O/RD2.EXE file obj/k_RD.obj,obj/k_MEM.obj,obj/k_TASK.obj,obj/k_FILE.obj,obj/k_MOD.obj,obj/k_RUNDOS.obj,obj/kasm.obj,obj/swap.obj,obj/notsup.obj option map=obj/rd.map >/dev/null
# Programme: Medium, ohne Watcom-Startcode
app() {   # $1 Quelle  $2 EXE  $3 Anzeigename
  wcc -0 -mm $APPFLAGS -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/$2.obj $1
  local st=obj/rd2start.obj; if [ -n "$APPSTACK" ]; then st=obj/rd2start_$APPSTACK.obj; $W -dSTK=$APPSTACK -fo=$st sdk/RD2START.ASM; fi   # eigene Stackgroesse (stacktop richtig)
  wlink option quiet format dos name $O/$2 file $st,obj/$2.obj$APPOBJ library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/$2.map >/dev/null
  if [ -f "${1%.C}.RC" ]; then python3 tools/rdrc.py "${1%.C}.RC" obj/$2.res >/dev/null || exit 1; python3 tools/rd2link.py $O/$2 "$3" --res obj/$2.res >/dev/null; else python3 tools/rd2link.py $O/$2 "$3" >/dev/null; fi
}
cp apps/test/RESTEST.RDB $O/   # fuer BmpLoadFile im RESTEST
for l in DE EN; do python3 tools/rdrc.py --lng src/lng/$l.RC $O/$l.LNG >/dev/null || exit 1; done   # Systemtexte
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
modul src/gdi/RDGDI.C RDGDI.MOD "RDGDI" 0 M -i=gdi
modul src/kern/RDKX.C RDKX.MOD "Kern Zusatz" 0x0080 M
modul src/kern/RDAUSL.C RDAUSL.MOD "Auslagern" 0 M "-r -oa -ol"
modul src/user/RDUSER.C RDUSER.MOD "RDUSER" 0 M "-i=gdi -ml -r -oa -ol"
modul src/gdi/RDGDIX.C RDGDIX.MOD "RDGDI Zusatz" 0x0080 M -i=gdi
modul src/ctl/RDCTL.C RDCTL.MOD "Steuerelemente" 0x0080 M "-ml -r"
modul src/ctl/RDCTLX.C RDCTLX.MOD "Dateidialog" 0x0080 M "-ml -r -oa -ol"
modul src/disk/RDDISK.C RDDISK.MOD "Laufwerke" 0x0080 M -i=disk
modul src/drv/GFX1.C CGA.DRV "CGA" 0 D -dCGA
modul src/drv/GFX1.C HGC.DRV "Hercules" 0 D -dHGC
modul src/drv/VGA16.C VGA16.DRV "VGA 16 Farben" 0 D -dVGA16
modul src/drv/VGA16.C EGA.DRV "EGA 16 Farben" 0 D -dEGA
modul src/drv/VGA16.C EGA320.DRV "EGA 320x200 16 Farben" 0 D -dEGA320
modul src/drv/VESA.C VESA.DRV "VESA 256 Farben" 0 D -3
python3 tools/mkfarbsym.py $O/FARBSYM.DAT SHELL:1:src/shell/farbe/FLOPPY.TXT SHELL:2:src/shell/farbe/HDISK.TXT SHELL:3:src/shell/farbe/ORDNER.TXT SHELL:4:src/shell/farbe/PROG.TXT SHELL:5:src/shell/farbe/DOK.TXT RDEDIT:1:src/shell/farbe/prog/RDEDIT.TXT NOTIZ:1:src/shell/farbe/prog/NOTIZ.TXT RECHNER:1:src/shell/farbe/prog/RECHNER.TXT UHR:1:src/shell/farbe/prog/UHR.TXT RDZIP:1:src/shell/farbe/prog/RDZIP.TXT RDFTP:1:src/shell/farbe/prog/RDFTP.TXT RDIRC:1:src/shell/farbe/prog/RDIRC.TXT RDNET:1:src/shell/farbe/prog/RDNET.TXT RDSHELL:1:src/shell/farbe/prog/RDSHELL.TXT TASKMAN:1:src/shell/farbe/prog/TASKMAN.TXT DPRUEF:1:src/shell/farbe/prog/DPRUEF.TXT DKOPIE:1:src/shell/farbe/prog/DKOPIE.TXT HILFE:1:src/shell/farbe/prog/HILFE.TXT OPTIONEN:1:src/shell/farbe/prog/OPTIONEN.TXT RDBILD:1:src/shell/farbe/prog/RDBILD.TXT RDQR:1:src/shell/farbe/prog/RDQR.TXT RDTRON:1:src/shell/farbe/prog/RDTRON.TXT >/dev/null || exit 1   # farbige Symbole (Diskette 2)
modul src/drv/GFX1.C VGAM.DRV "VGA mono" 0 D "-dVGAM -oa"   # -oa: 11 Bytes, haelt VGAM.DRV bei 4 KB (ein Cluster auf der Systemdiskette)
modul src/drv/GFX1.C EGAM.DRV "EGA mono" 0 D "-dEGAM -oa"
modul src/drv/VGA16.C EGA4.DRV "EGA 4 Farben" 0 D -dEGA4
modul src/drv/VGA16.C VGA4.DRV "VGA 4 Farben" 0 D -dVGA4
modul src/drv/VGA16.C EGA200.DRV "EGA 640x200 16 Farben" 0 D -dEGA200
modul src/drv/VGA16.C SVGA16.DRV "VESA 800x600 16 Farben" 0 D -dSVGA16
modul src/drv/VESA.C MCGA.DRV "MCGA 256 Farben" 0 D -dMCGA
modul src/drv/VESA.C VESA800.DRV "VESA 800x600 256" 0 D "-3 -dV800"
modul src/drv/VESA.C VESA1024.DRV "VESA 1024x768 256" 0 D "-3 -dV1024"
modul src/drv/CGA4.C CGA4.DRV "CGA 4 Farben" 0 D
modul src/snd/RDSND.C RDSND.MOD "Klang" 0 M
modul src/snd/RDMUS.C RDMUS.MOD "Musik" 0 M
modul src/drv/SND.C SPEAKER.DRV "PC-Lautsprecher" 0 D -dSPK
modul src/drv/SND.C ADLIB.DRV "AdLib" 0 D -dADL
modul src/drv/SND.C SB.DRV "Sound Blaster" 0 D -dSB
modul src/drv/SND.C COVOX.DRV "Covox" 0 D -dCOVOX
# Drucken: RDPRN.MOD (Dialog, Baender, LPT/Datei) laedt RDKX bei PrnOpen; Treiber EPSON9.DRV
modul src/prn/RDPRN.C RDPRN.MOD "Drucken" 0 M "-ml -r"
modul src/drv/PRN.C EPSON9.DRV "Epson 9-Nadel" 0 D -dEPSON9
# Netz: RDTCP.MOD (TCP/IP) mit Empfangsroutine src/net/PKT.ASM
$W -fo=obj/pkt.obj src/net/PKT.ASM
wcc -0 -mm -zu -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/RDTCP.MOD.obj src/net/RDTCP.C
wlink option quiet format dos name $O/RDTCP.MOD file obj/rd2mod.obj,obj/RDTCP.MOD.obj,obj/pkt.obj library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,stack=16,map=obj/RDTCP.MOD.map disable 1172 >/dev/null
mv $O/RDTCP.MOD.exe $O/RDTCP.MOD 2>/dev/null || true
python3 tools/rd2link.py $O/RDTCP.MOD "TCP/IP" --type M --flags 0 >/dev/null
for t in MODTEST FILETEST HAENGT STRESS GDITEST GFXWAHL TEXTTEST INPTEST RESTEST WINTEST JOYTEST MENUTEST ZEIGTEST DLGTEST DISKFRAG DOPPFMT; do app apps/test/$t.C $t.EXE "$t"; done
modul src/shell/SHELLX.C SHELLX.MOD "Shell-Zusatz" 0x0080 M "-ml -r -oa -ol"   # Info, Laufwerksinfo, Eigenschaften, Suchen (verwerfbar)
APPSTACK=3584 app src/shell/SHELL.C SHELL.EXE "Shell"   # Stack 3,5 KB (31.10.: desk.sh misst bis 2,7 KB belegt, COM1 "SHELL: Stapel frei")
app apps/hilfe/HILFE.C HILFE.EXE "Hilfe"   # F1: HelpShow -> RDKX -> RDPRN.MOD Nr. 6 -> HILFE.EXE
iconv -f UTF-8 -t CP437 docs/RD2_HLP.txt > $O/RD2.HLP
app apps/uhr/UHR.C UHR.EXE "Uhr"   # voller Stapel: GetDC laedt Treiber und Module auf dem Stapel des Programms
app apps/notiz/NOTIZ.C NOTIZ.EXE "Notizblock"
app apps/net/RDNET.C RDNET.EXE "Netzwerk"
app apps/net/RDFTP.C RDFTP.EXE "FTP"
app apps/net/RDIRC.C RDIRC.EXE "IRC"
app apps/dos/RDSHELL.C RDSHELL.EXE "DOS-Fenster"
app apps/rechner/RECHNER.C RECHNER.EXE "Rechner"
app apps/optionen/OPTIONEN.C OPTIONEN.EXE "Optionen"
app apps/papierk/PAPIERK.C PAPIERK.EXE "Muell"
app apps/taskman/TASKMAN.C TASKMAN.EXE "Taskmanager"
app apps/dkopie/DKOPIE.C DKOPIE.EXE "Diskette kopieren"
app apps/install/INSTALL.C INSTALL.EXE "Installation"
app apps/hallo/HALLO.C HALLO.EXE "Hallo"
app apps/dpruef/DPRUEF.C DPRUEF.EXE "Datentraeger pruefen"
app apps/schoner/SCHONER.C SCHONER.EXE "Bildschirmschoner"
app apps/edit/RDEDIT.C RDEDIT.EXE "Editor"
app apps/rdtron/RDTRON.C RDTRON.EXE "H-Tron"   # Spiel nach H-TRON (RobertK 2016, MRalser 2026); RDTRON statt HTRON, damit es nicht mit der DOS-Fassung Htron.exe kollidiert; GPL, auf Diskette 3
for z in INFLATE DEFLATE CRC; do wcc -0 -mm -os -s -zq -w4 -bt=dos -zl -i=sdk -fo=obj/zk_$z.obj apps/zip/$z.C || exit 1; done   # ZIP-Kern (auch test/zip_kern.c)
APPOBJ=",obj/zk_INFLATE.obj,obj/zk_DEFLATE.obj,obj/zk_CRC.obj" app apps/zip/RDZIP.C RDZIP.EXE "ZIP"
# RDFORMAT: RD2-Oberflaeche + FreeDOS FORMAT 0.92a (GPL v2); FreeDOS-Dateien mit src/disk/fdshim.h (DOS ueber RDDISK.MOD)
FDF="createfs floppy hdisk main savefs bcread prf userint driveio getopt init recordbc uformat msghlpr"
for f in $FDF; do wcc -0 -mm -zp1 -os -s -zq -w0 -bt=dos -zl -fi=src/disk/fdshim.h -dRD2 -dNOCATS -i=src/disk/fdformat -fo=obj/fdf_$f.obj src/disk/fdformat/$f.c || exit 1; done
wcc -0 -mm -os -s -zq -w4 -bt=dos -zl -i=sdk -i=disk $RDF_DEF -fo=obj/RDFORMAT.EXE.obj src/disk/RDFORMAT.C || exit 1
wlink option quiet format dos name $O/RDFORMAT.EXE file obj/rd2start.obj,obj/RDFORMAT.EXE.obj$(for f in $FDF; do printf ",obj/fdf_$f.obj"; done) library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/RDFORMAT.EXE.map >/dev/null || { echo "RDFORMAT: Linkfehler"; wlink option quiet format dos name $O/RDFORMAT.EXE file obj/rd2start.obj,obj/RDFORMAT.EXE.obj$(for f in $FDF; do printf ",obj/fdf_$f.obj"; done) library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs 2>&1 | head -20; exit 1; }
python3 tools/rdrc.py src/disk/RDFORMAT.RC obj/RDFORMAT.res >/dev/null || exit 1; python3 tools/rd2link.py $O/RDFORMAT.EXE "Formatieren" --res obj/RDFORMAT.res >/dev/null
# RDFDISK: RD2-Oberflaeche + Free FDISK 1.3 (GPL v2): pcompute.c, pdiskio.c, Startcode bootnorm.asm (NASM)
FDK="pcompute pdiskio"
for f in $FDK; do wcc -0 -mm -zp1 -os -s -zq -w0 -bt=dos -zl -fi=src/disk/fdshim.h -dRD2 -i=src/disk/fdfdisk -fo=obj/fdk_$f.obj src/disk/fdfdisk/$f.c || exit 1; done
wcc -0 -mm -zp1 -os -s -zq -w0 -bt=dos -zl -fi=src/disk/fdshim.h -dRD2 -i=src/disk/fdfdisk -fo=obj/fdk_glue.obj src/disk/fdglue.c || exit 1
nasm -f bin -o obj/bootnorm.bin src/disk/fdfdisk/bootnorm.asm || exit 1
python3 -c "d=open('obj/bootnorm.bin','rb').read(); open('obj/bootnorm.c','w').write('char bootnormal_code[] = {'+','.join(str(b) for b in d)+'};\n')"
wcc -0 -mm -os -s -zq -bt=dos -zl -fo=obj/fdk_boot.obj obj/bootnorm.c || exit 1
wcc -0 -mm -os -s -zq -w4 -bt=dos -zl -i=sdk -i=disk -fo=obj/RDFDISK.EXE.obj src/disk/RDFDISK.C || exit 1
wlink option quiet format dos name $O/RDFDISK.EXE file obj/rd2start.obj,obj/RDFDISK.EXE.obj,obj/fdk_glue.obj,obj/fdk_boot.obj$(for f in $FDK; do printf ",obj/fdk_$f.obj"; done) library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/RDFDISK.EXE.map >/dev/null || { wlink option quiet format dos name $O/RDFDISK.EXE file obj/rd2start.obj,obj/RDFDISK.EXE.obj,obj/fdk_glue.obj,obj/fdk_boot.obj$(for f in $FDK; do printf ",obj/fdk_$f.obj"; done) library $O/RD2.LIB,$WATCOM/lib286/dos/clibm.lib option nodefaultlibs,map=obj/RDFDISK.EXE.map | grep -v "^$" | head -20; exit 1; }
python3 tools/rdrc.py src/disk/RDFDISK.RC obj/RDFDISK.res >/dev/null || exit 1; python3 tools/rd2link.py $O/RDFDISK.EXE "Festplatte" --res obj/RDFDISK.res >/dev/null
# Leistungsvergleich RD1 gegen RD2 (test/bench.sh): BENCH2 ueber die RD2-API, BENCH1 mit dem RD1-Renderer aus main
app test/bench/BENCH2.C BENCH2.EXE "Bench2"
( T=obj/bench1; mkdir -p $T; rm -f $T/*; cp test/bench/BENCH1.C $T/bench1.c; cp docs/rd1ref/gfx.c docs/rd1ref/mouse.c docs/rd1ref/gfx.h docs/rd1ref/mouse.h $T/
  cd $T && wcl -0 -ms -os -s -fpi -bt=dos -zq -fe=BENCH1.EXE bench1.c gfx.c mouse.c >/dev/null ) || echo "FEHLER BENCH1"
APPFLAGS=-fpi87 app apps/test/WURM.C WURM.EXE "Wurm"
for t in PING PONG RECHNE MEMTEST; do app apps/test/$t.C $t.EXE "$t"; done
app apps/test/DOSTEST.C DOSTEST.EXE "DOSTEST"
app apps/test/AUSLTEST.C AUSLTEST.EXE "AUSLTEST"
app apps/test/SWAPTEST.C SWAPTEST.EXE "Speicher-Test"
app apps/test/PRNTEST.C PRNTEST.EXE "PRNTEST"
app apps/test/SNDTEST.C SNDTEST.EXE "SNDTEST"
app apps/test/NETTEST.C NETTEST.EXE "NETTEST"
wcl -0 -ms -os -s -zq -bt=dos -fe=$O/FREIMEM.EXE -fo=obj/freimem.obj apps/test/FREIMEM.C >/dev/null
wcl -0 -ms -os -s -zq -bt=dos -fe=$O/TSRTEST.EXE -fo=obj/tsrtest.obj apps/test/TSRTEST.C >/dev/null
python3 tools/mkpas.py $O/RD2.PAS 2>/dev/null || echo "FEHLER RD2.PAS"   # Unit RD2 fuer Turbo Pascal und Free Pascal (SDK)
# RDRC.EXE: Ressourcen-Compiler und Linker fuer das SDK (DOS-Fassung von rdrc.py + rd2link.py)
nasm -f bin -o obj/RD2PAS.BIN sdk/RD2PAS.ASM && python3 -c "d=open('obj/RD2PAS.BIN','rb').read(); open('obj/rd2pas.h','w').write('static const unsigned char rd2pas[%d] = {' % len(d) + ','.join(str(b) for b in d) + '};\\n')" && python3 tools/mkrdrcdef.py obj/rdrcdef.h && wcl -0 -ms -os -s -zq -w4 -bt=dos -i=obj -fe=$O/RDRC.EXE -fo=obj/rdrc.obj tools/RDRC.C >/dev/null || echo "FEHLER RDRC"
python3 tools/mkfont.py $O >/dev/null
python3 tools/mkschmal.py $O                  # SYSK.FNT: schmale Schrift fuer 320 Punkte Breite
# Einrichtung: normales DOS-Programm (Small, Watcom-Bibliothek), laeuft ohne RetroDesk
$W -fpi87 -fo=obj/setasm.obj src/setup/SETASM.ASM
# RDSYS.EXE: FreeDOS SYS (GPL v2) unveraendert als eigenes DOS-Programm, RD2SETUP ruft es mit /UPDATE (Festplatte startfaehig)
mkdir -p obj/sys; for t in 12 16; do nasm -dISFAT$t -i src/disk/fdsys/ src/disk/fdsys/boot.asm -o obj/sys/fat${t}com.bin || exit 1
  python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print('unsigned char %s[] = {' % sys.argv[2]); print(','.join('0x%02X' % c for c in d)); print('};')" obj/sys/fat${t}com.bin fat${t}com > obj/sys/fat${t}com.h; done
for f in sys fdkrncfg talloc kernel/prf; do wcc -0 -ms -zp1 -os -s -zq -bt=dos -DFORSYS -i=src/disk/fdsys/hdr -i=obj/sys -fo=obj/sys/${f##*/}.obj src/disk/fdsys/$f.c >/dev/null || exit 1; done
wlink option quiet system dos name $O/RDSYS.EXE file obj/sys/sys.obj,obj/sys/fdkrncfg.obj,obj/sys/talloc.obj,obj/sys/prf.obj >/dev/null || exit 1
# COMMAND.COM: FreeCOM 0.85a deutsch (GPL v2) unveraendert, Diskette 3; INSTALL kopiert es nach C:\RD2 (31.10.2026)
cp src/disk/freecom/COMMAND.COM $O/COMMAND.COM
wcc -0 -ms -os -s -zq -w4 -bt=dos -fo=obj/rd2setup.obj src/setup/RD2SETUP.C
wlink option quiet system dos name $O/RD2SETUP.EXE file obj/rd2setup.obj,obj/setasm.obj option map=obj/rd2setup.map >/dev/null
# G12-Test: LEER.EXE mit Mindestzuteilung FFFFh (Bild + minalloc laeuft ueber 64K Paragraphen), muss RD_ERR_FORMAT liefern
python3 -c "import sys; d=bytearray(open('$O/LEER.EXE','rb').read()); d[10:12]=b'\xff\xff'; open('$O/UEBER.EXE','wb').write(d)"
python3 tools/budget.py $O ${1:+$1}
