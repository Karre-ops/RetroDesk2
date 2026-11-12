#!/bin/bash
# Systemdateien in der Shell (24.10.2026, Karsten): Loeschen, Ersetzen, Verschieben, Umbenennen fragen noch
# einmal, "Nein" ist vorgewaehlt. Ablauf (VGA, nur Tastatur, Wurzel A:\ sortiert: RD2, AUTOEXEC.BAT, COMMAND.COM,
# CONFIG.SYS, FDAPM.COM, FDAPM.TXT, KERNEL.SYS; Buchstabe springt zum naechsten Eintrag):
#   1. KERNEL.SYS loeschen: Rueckfrage Ja, Systemwarnung, Eingabe (= Nein) -> bleibt.
#   2. Ordner RD2 umbenennen: Systemwarnung, Esc -> kein Eingabefeld, Name bleibt.
#   3. AUTOEXEC.BAT loeschen: Rueckfrage Ja, Systemwarnung, Tab + Eingabe (= Ja) -> geloescht.
#   4. FDAPM.COM loeschen: keine Systemwarnung.
cd "$(dirname "$0")/.."
IMG=obj/sysd.img; SER=obj/sysd_com1.txt; SOCK=/tmp/qmon_sysd.sock
printf '@echo off\nA:\\RD2\\RD2.EXE /NOMAUS /VGA A:\\RD2\\SHELL.EXE\nA:\\FDAPM.COM POWEROFF\n' > obj/ae_sysd.txt
bash tools/mkdisk.sh $IMG obj/ae_sysd.txt >/dev/null 2>&1
mcopy -o -i $IMG obj/ae_sysd.txt ::/FDAPM.TXT     # Opfer fuer Fall 4 (FDAPM.COM braucht der Test zum Ausschalten)
rm -f $SER $SOCK obj/sysd_*.ppm; touch $SER
timeout 240 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_sysd.py
Q() { python3 obj/qmon_sysd.py "$@"; }
waitn() { for i in $(seq 400); do [ "$(tr -d '\r' < $SER | grep -c "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
ok=1
waitn "SHELL: bereit" 1 && sleep 3 && Q "sendkey ret"
waitn 'SHELL: Ordner A:\\ ' 1 && sleep 2 && Q "sendkey k" sleep0.3 "sendkey f8" sleep1.5 "sendkey ret"
waitn "SHELL: Systemwarnung" 1 && sleep 1.5 && Q "screendump obj/sysd_warnung.ppm" sleep0.3 "sendkey ret"
sleep 2; Q "sendkey kp_7" sleep0.3 "sendkey f2"
waitn "SHELL: Systemwarnung" 2 && sleep 1.5 && Q "sendkey esc"
sleep 2; Q "sendkey kp_7" sleep0.3 "sendkey a" sleep0.3 "sendkey f8" sleep1.5 "sendkey ret"
waitn "SHELL: Systemwarnung" 3 && sleep 1.5 && Q "sendkey tab" sleep0.5 "sendkey ret"
waitn "SHELL: geloescht" 1 && sleep 2 && Q "sendkey kp_7" sleep0.3 "sendkey f" sleep0.3 "sendkey f" sleep0.3 "sendkey f8" sleep1.5 "sendkey ret"
waitn "SHELL: geloescht" 2 && sleep 2 && Q "sendkey alt-spc" sleep0.8 "sendkey b" sleep1.5 "sendkey ret"
waitn "SHELL: Ende" 1 || ok=0
sleep 3; kill $QP 2>/dev/null; wait $QP 2>/dev/null   # AUTOEXEC.BAT ist geloescht, FDAPM laeuft nicht mehr
S=$(tr -d '\r' < $SER)
chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; ok=0; fi; }
chk "1 KERNEL.SYS bleibt (Nein vorgewaehlt)"  "mdir -i $IMG ::/KERNEL.SYS >/dev/null 2>&1"
chk "2 Ordner RD2 heisst noch RD2"             "mdir -i $IMG ::/RD2/RD2.EXE >/dev/null 2>&1"
chk "3 AUTOEXEC.BAT nach Ja geloescht"         "! mdir -i $IMG ::/AUTOEXEC.BAT >/dev/null 2>&1"
chk "4 FDAPM.TXT ohne Systemwarnung geloescht" "! mdir -i $IMG ::/FDAPM.TXT >/dev/null 2>&1"
chk "genau 3 Systemwarnungen"                  "[ \$(echo \"\$S\" | grep -c 'SHELL: Systemwarnung') = 3 ]"
chk "Speicher zurueck"                         "echo \"\$S\" | grep -q 'Speicher vollstaendig zurueck'"
[ $ok = 1 ] && echo "Systemdateien: alles ok" || echo "Systemdateien: Fehler"
[ $ok = 1 ]
