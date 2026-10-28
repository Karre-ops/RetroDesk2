#!/bin/bash
# Punkt 4: Zubehoer Optionen (OPTIONEN.EXE T Ordner), nur Tastatur. Echte CONFIG.SYS bleibt unberuehrt (Testordner).
# 1 VGA T1: Systemklaenge aus, Start aus -> RD-Zeile und DOSKEY (steht in AUTOEXEC) "REM RD2 ", alte Shell wieder aktiv.
# 2 VGA T1: Start an -> CONFIG.SYS wieder genau wie vorher.  3 CGA T2: kein COMMAND.COM -> Aus abgelehnt, Datei gleich.
cd "$(dirname "$0")/.."
IMG=obj/optionen.img; SER=obj/optionen_com1.txt; SOCK=/tmp/qmon_optionen.sock
cat > obj/ae_optionen.txt <<'AE'
@echo off
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\OPTIONEN.EXE T A:\T1\
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\OPTIONEN.EXE T A:\T1\
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\OPTIONEN.EXE T A:\T2\
A:\FDAPM.COM POWEROFF
AE
bash tools/mkdisk.sh $IMG obj/ae_optionen.txt
for f in RDMAUS.COM RDFORMAT.EXE RDFDISK.EXE RDSYS.EXE RD2SETUP.EXE; do mdel -i $IMG ::/RD2/$f 2>/dev/null; done   # Platz zum Speichern (Diskette sonst voll, Test braucht sie nicht)
mkdir -p obj/opt/T1 obj/opt/T2
printf 'FILES=20\r\nREM SHELL=A:\\T1\\COMMAND.COM /P\r\nSHELL=A:\\RD2\\RD.EXE /P A:\\RD2\\SHELL.EXE\r\nINSTALL=A:\\RDMAUS.COM\r\nINSTALL=A:\\T1\\DOSKEY.COM\r\n' > obj/opt/T1/CONFIG.SYS
printf '@ECHO OFF\r\nDOSKEY.COM\r\n' > obj/opt/T1/AUTOEXEC.BAT; printf 'x' > obj/opt/T1/COMMAND.COM
printf 'SHELL=A:\\RD2\\RD.EXE /P A:\\RD2\\SHELL.EXE\r\n' > obj/opt/T2/CONFIG.SYS
mmd -i $IMG ::/T1 ::/T2; mcopy -i $IMG obj/opt/T1/* ::/T1/; mcopy -i $IMG obj/opt/T2/* ::/T2/
rm -f $SER $SOCK obj/optionen?_*.ppm
timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_optionen.py
Q() { python3 obj/qmon_optionen.py "$@"; }
waitn() { for i in $(seq 600); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; return 1; }
lauf() {
  waitn "OPTIONEN: bereit" 1 || { echo "FEHLER: Optionen 1 startet nicht"; return; }; sleep 3
  Q "sendkey alt-k" sleep0.3 "sendkey alt-s" sleep0.6 "screendump obj/optionen1_vga.ppm" sleep0.3 "sendkey ret"
  waitn "OPTIONEN: gespeichert" 1 || echo "FEHLER: Lauf 1 speichert nicht"
  waitn "OPTIONEN: bereit" 2 || { echo "FEHLER: Optionen 2 startet nicht"; return; }; sleep 3
  Q "sendkey ctrl" sleep0.4 "sendkey alt-s" sleep0.4 "sendkey ret"
  waitn "OPTIONEN: gespeichert" 2 || echo "FEHLER: Lauf 2 speichert nicht"
  waitn "OPTIONEN: bereit" 3 || { echo "FEHLER: Optionen 3 startet nicht"; return; }; sleep 3
  Q "sendkey ctrl" sleep0.4 "sendkey alt-s" sleep0.4 "sendkey ret"; sleep 2.5
  Q "screendump obj/optionen2_cga.ppm" sleep0.3 "sendkey ret" sleep1.0 "sendkey esc"
}
lauf
wait $QP
tr -d '\r' < $SER | grep -A1 -E "OPTIONEN|RD: (Speicher vollstaendig|SPEICHER|Fehler)" | grep -v "^--"
pr() { [ "$2" = "$3" ] && echo "ok     $1" || { echo "FEHLER $1"; echo "  ist:  $(printf %q "$2")"; echo "  soll: $(printf %q "$3")"; }; }
z=$(tr -d '\r' < $SER | grep -c "^OPTIONEN: bereit, Start an")
pr "Start dreimal richtig erkannt (an, aus, an)" "$(tr -d '\r' < $SER | grep '^OPTIONEN: bereit' | cut -c19- | tr '\n' ' ')" "Start an Start aus Start an "
mcopy -o -i $IMG ::/T1/CONFIG.SYS obj/opt/t1_neu.sys; mcopy -o -i $IMG ::/T2/CONFIG.SYS obj/opt/t2_neu.sys
pr "T1 aus: RD-Zeile und DOSKEY REM RD2, alte Shell aktiv" "$(tr -d '\r' < $SER | grep -A5 '^OPTIONEN: bereit, Start an' | head -6 | tail -5 | tr '\n' '|')" 'FILES=20|SHELL=A:\T1\COMMAND.COM /P|REM RD2 SHELL=A:\RD2\RD.EXE /P A:\RD2\SHELL.EXE|INSTALL=A:\RDMAUS.COM|REM RD2 INSTALL=A:\T1\DOSKEY.COM|'
pr "T1 nach Aus und An wie vorher" "$(md5sum < obj/opt/t1_neu.sys)" "$(md5sum < obj/opt/T1/CONFIG.SYS)"
pr "T2 abgelehnt, unveraendert" "$(md5sum < obj/opt/t2_neu.sys)" "$(md5sum < obj/opt/T2/CONFIG.SYS)"
pr "Rueckgaben: Aus 0, An 0, ohne Shell 2" "$(tr -d '\r' < $SER | grep 'OPTIONEN: CONFIG.SYS' | cut -c22- | tr '\n' ' ')" "0x00000000 0x00000000 0x00000002 "
pr "RD2.INI Systemklaenge aus" "$(mtype -i $IMG ::/RD2/RD2.INI 2>/dev/null | tr -d '\r' | grep -i '^Systemklang=')" "Systemklang=0"
[ -f test/ref/optionen1_vga.png ] && python3 test/bildvergleich.py obj/optionen1_vga.ppm test/ref/optionen1_vga.png obj/optionen2_cga.ppm test/ref/optionen2_cga.png
exit 0
