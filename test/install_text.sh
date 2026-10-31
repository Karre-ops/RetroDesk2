#!/bin/bash
# Installation mit RD2SETUP von der Diskette (Taste T, Text-Notweg; grafisch: test/install.sh), zwei Faelle (G21, G22):
# 1. Bestehendes DOS: Ziel C:\SPIELE\RD2 (Zwischenordner fehlt), CONFIG.SYS mit SHELL=COMMAND.COM und
#    AUTOEXEC.BAT mit Maustreiber. Rueckfragen: Shell eintragen (J), Maustreiber uebernehmen (J).
#    Erwartet: Dateien, RD2.INI, CONFIG.SYS mit REM SHELL=..., SHELL=C:\SPIELE\RD2\RD.EXE /P,
#    INSTALL=C:\TREIBER\MOUSE.COM /S50, kein RDMAUS, Sicherung CONFIG.RDA, AUTOEXEC.BAT unveraendert.
# 2. Leere Festplatte: Ziel C:\RD2, keine Rueckfragen, CONFIG.SYS neu mit SHELL= und INSTALL=RDMAUS /P (PS/2 in QEMU).
cd "$(dirname "$0")/.."
IMG=obj/inst720.img; HD=obj/inst_hd.img; F=0
printf '@echo off\r\nA:\\RD2\\RD2SETUP.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' | tr -d '\r' > obj/ae_inst.txt
SETUP=1 bash tools/mkdisk.sh $IMG obj/ae_inst.txt
Q="python3 test/qemu/qmon.py"
waitscr() { for i in $(seq ${2:-30}); do $Q text 2>/dev/null | grep -q "$1" && return 0; sleep 1; done; return 1; }
starten() {
  rm -f /tmp/qmon_i.sock
  timeout 250 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -drive file=$HD,format=raw,index=0,media=disk -boot a \
    -monitor unix:/tmp/qmon_i.sock,server,nowait >/dev/null 2>&1 &
  QP=$!; export QMON=/tmp/qmon_i.sock
  waitscr "Einrichtung" 120 || { echo "FEHLER keine Einrichtung"; F=1; }
}
ende() { waitscr "CONFIG.SYS" 30; waitscr "startfaehig machen" 30 && { $Q "sendkey n" >/dev/null 2>&1; waitscr "nicht startfaehig gemacht" 10 || { echo "FEHLER Frage startfaehig ($1)"; F=1; }; } || { echo "FEHLER keine Frage startfaehig ($1)"; F=1; }; sleep 1; $Q text > obj/schirm_install$1.txt 2>/dev/null
  # Disketten 2/3 fehlen auf der Testdiskette: Frage "einlegen" mit Esc auslassen (je Diskette eine)
  for i in 1 2; do $Q text 2>/dev/null | grep -q "einlegen: Enter" || break; $Q "sendkey esc" >/dev/null 2>&1; sleep 1; done
  $Q text > obj/schirm_install$1.txt 2>/dev/null; $Q "sendkey esc" sleep2 "sendkey n" >/dev/null 2>&1; wait $QP; }
dateien() { mdir $C -b ::/$1 > obj/inst_dir.txt 2>&1
  for d in RD.EXE RDGDI.MOD RDSWAP.BIN RDMAUS.COM RD2SETUP.EXE RD2.INI; do grep -qi "/$d" obj/inst_dir.txt && echo "ok     $1/$d" || { echo "FEHLER $1/$d fehlt"; F=1; }; done; }
pruef() { grep -q "$1" obj/inst_cs.txt && echo "ok     CONFIG.SYS: $2" || { echo "FEHLER CONFIG.SYS: $2"; F=1; }; }

# ---- Fall 1: bestehendes DOS ---- (FALL=1 oder FALL=2: nur einen Fall, je ca. 2 Min.)
if [ "${FALL:-}" != 2 ]; then
bash tools/mkhd.sh $HD; C="-i $HD@@32256"
printf 'FILES=20\r\nSHELL=C:\\COMMAND.COM C:\\ /P\r\n' > obj/cs_alt.tmp; mcopy $C obj/cs_alt.tmp ::/CONFIG.SYS
printf '@ECHO OFF\r\nPATH A:\\\r\nLH C:\\TREIBER\\MOUSE.COM /S50\r\nREM Ende\r\n' > obj/ae_alt.tmp; mcopy $C obj/ae_alt.tmp ::/AUTOEXEC.BAT
mmd $C ::/TREIBER; mcopy $C test/qemu/CTMOUSE.EXE ::/TREIBER/MOUSE.COM
starten
$Q "sendkey t" sleep1 "sendkey backspace" "sendkey backspace" "sendkey backspace" >/dev/null 2>&1
for k in s p i e l e backslash r d 2 ret; do $Q "sendkey $k" >/dev/null 2>&1; done
waitscr "als Shell in CONFIG.SYS eintragen" 90 && $Q "sendkey j" sleep1 >/dev/null 2>&1 || { echo "FEHLER keine Rueckfrage Shell"; F=1; }
waitscr "MOUSE.COM /S50" 20 && $Q "sendkey j" sleep1 >/dev/null 2>&1 || { echo "FEHLER keine Rueckfrage Maustreiber"; F=1; }
ende 1
dateien SPIELE/RD2
mtype $C ::/CONFIG.SYS | tr -d '\r' > obj/inst_cs.txt
pruef "^FILES=20" "alte Zeile bleibt"
pruef "^REM SHELL=C:\\\\COMMAND.COM" "alte Shell als REM"
pruef "^SHELL=C:\\\\SPIELE\\\\RD2\\\\RD.EXE /P C:\\\\SPIELE\\\\RD2\\\\SHELL.EXE$" "SHELL=RD.EXE /P"
pruef "^INSTALL=C:\\\\TREIBER\\\\MOUSE.COM /S50$" "Maustreiber aus AUTOEXEC.BAT (ohne LH)"
grep -q RDMAUS obj/inst_cs.txt && { echo "FEHLER CONFIG.SYS: RDMAUS trotz eigenem Maustreiber"; F=1; } || echo "ok     CONFIG.SYS: kein zweiter Maustreiber"
mtype $C ::/CONFIG.RDA | tr -d '\r' | grep -q "^SHELL=C:\\\\COMMAND.COM" && echo "ok     Sicherung CONFIG.RDA" || { echo "FEHLER keine CONFIG.RDA"; F=1; }
mtype $C ::/AUTOEXEC.BAT | cmp -s - obj/ae_alt.tmp && echo "ok     AUTOEXEC.BAT unveraendert" || { echo "FEHLER AUTOEXEC.BAT veraendert"; F=1; }
grep -q "PATH-Zeilen" obj/schirm_install1.txt && echo "ok     Hinweis SET/PATH" || { echo "FEHLER kein Hinweis SET/PATH"; F=1; }
cp obj/inst_cs.txt obj/inst_cs1.txt
fi

# ---- Fall 2: leere Festplatte ----
if [ "${FALL:-}" != 1 ]; then
bash tools/mkhd.sh $HD; C="-i $HD@@32256"
starten
for i in 1 2 3 4 5 6 7 8 9; do $Q "sendkey down" >/dev/null 2>&1; done; $Q "sendkey left" sleep1 >/dev/null 2>&1   # Tastatur: DE -> PC10 (31.10.)
$Q text 2>/dev/null | grep -q "Commodore PC10" && echo "ok     Setup: Tastatur Commodore PC10 waehlbar" || { echo "FEHLER Tastatur PC10 nicht im Setup"; F=1; }
$Q "sendkey t" sleep2 "sendkey ret" >/dev/null 2>&1
ende 2
grep -q "Uebernehmen\|eintragen (J/N)" obj/schirm_install2.txt && { echo "FEHLER Rueckfrage bei Neuinstallation"; F=1; } || echo "ok     keine Rueckfrage"
grep -q "CONFIG.SYS angelegt" obj/schirm_install2.txt && echo "ok     Meldung CONFIG.SYS angelegt" || { echo "FEHLER Meldung"; F=1; }
dateien RD2
mtype $C ::/CONFIG.SYS | tr -d '\r' > obj/inst_cs.txt
pruef "^SHELL=C:\\\\RD2\\\\RD.EXE /P C:\\\\RD2\\\\SHELL.EXE$" "SHELL=RD.EXE /P"
pruef "^INSTALL=C:\\\\RD2\\\\RDMAUS.COM /P$" "RDMAUS /P fuer die PS/2-Maus"
[ "$(wc -l < obj/inst_cs.txt)" = 3 ] && grep -q "^INSTALL=C:.RD2.RDKEYB.COM C:.RD2.PC10.KBD$" obj/inst_cs.txt && mdir $C ::/RD2/PC10.KBD >/dev/null 2>&1 && echo "ok     CONFIG.SYS: Shell, RDKEYB C:\\RD2\\PC10.KBD (Datei kopiert), RDMAUS" || { echo "FEHLER CONFIG.SYS hat Fremdes"; cat obj/inst_cs.txt; F=1; }
mtype $C ::/AUTOEXEC.BAT >/dev/null 2>&1 && { echo "FEHLER AUTOEXEC.BAT angelegt"; F=1; } || echo "ok     keine AUTOEXEC.BAT"
fi
[ $F = 0 ] && echo "Installation: alles ok"
exit $F
