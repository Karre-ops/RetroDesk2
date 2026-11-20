#!/bin/bash
# SysInfo (API 2.3) und 3D-Kartenerkennung (PLAN_32BIT Schritt 1, 19.11.2026), QEMU:
# 1. RD2SETUP auf einem Pentium mit zusaetzlicher ATI Rage 128 Pro (QEMU ati-vga, PCI 1002:5046):
#    die Liste zeigt die Karte, S speichert 3D=RAGE128 und 3DPCI=1002:5046 in RD2.INI.
# 2. SYSTEST.EXE liest SysInfo: Klang, Netz und 3D aus RD2.INI, Speicherwerte, Diskettenbetrieb; BedarfCheck fuer
#    XB1.EXE (BEDARF passt), XB2.EXE (Farben, XMS, Klang MIDI, RAM fehlen), LEER.EXE und eine .COM (ohne BEDARF).
# 3. Shell: XB2.EXE im Ordner starten -> Meldung, kein Start; XB1.EXE startet.
cd "$(dirname "$0")/.."
IMG=obj/sysinfo.img; SER=obj/sysinfo_com1.txt; SOCK=/tmp/qmon_sysinfo.sock; F=0
Q="python3 test/qemu/qmon.py"; export QMON=$SOCK
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
waitscr() { for i in $(seq ${2:-30}); do $Q text 2>/dev/null | grep -q "$1" && return 0; sleep 1; done; return 1; }
starten() {
  rm -f $SOCK $SER
  timeout 200 qemu-system-i386 -cpu pentium -m 4 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
    -device ati-vga,model=rage128p -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
}
# ---- 1. Erkennung in RD2SETUP ----
printf '@echo off\r\nA:\\RD2\\RD2SETUP.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' | tr -d '\r' > obj/ae_sysinfo1.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') SYSTEST.EXE LEER.EXE" SETUP=1 FMT=720 bash tools/mkdisk.sh $IMG obj/ae_sysinfo1.txt >/dev/null 2>&1
# Testprogramme mit BEDARF: Kopien von LEER.EXE mit eigener Ressource
printf 'BEDARF CPU 386 FPU NETZ 3D BESCHLEUNIGER KLANG PCM KLANG OPL2\n' > obj/xb1.rc; printf 'BEDARF FARBEN 256 XMS 1024 KLANG MIDI RAM 9999\n' > obj/xb2.rc
for n in 1 2; do cp out/LEER.EXE obj/XB$n.EXE; python3 tools/rdrc.py obj/xb$n.rc obj/xb$n.res >/dev/null && python3 tools/rd2link.py obj/XB$n.EXE "XB$n" --res obj/xb$n.res >/dev/null; mcopy -o -i $IMG obj/XB$n.EXE ::/RD2/; done
starten
waitscr "Einrichtung" 120; waitscr "3D-Karte" 20; $Q text > obj/schirm_sysinfo.txt 2>/dev/null
$Q "sendkey s" sleep2 >/dev/null 2>&1; waitscr "Gespeichert" 20
$Q "sendkey esc" sleep2 "sendkey n" >/dev/null 2>&1
wait $QP
pruef "RD2SETUP zeigt die 3D-Karte" "grep -q 'ATI Rage 128 (PCI 1002:5046)' obj/schirm_sysinfo.txt"
mtype -i $IMG ::/RD2/RD2.INI 2>/dev/null | tr -d '\r' > obj/sysinfo_ini.txt
pruef "RD2.INI 3D=RAGE128" "grep -q '^3D=RAGE128$' obj/sysinfo_ini.txt"
pruef "RD2.INI 3DPCI=1002:5046" "grep -q '^3DPCI=1002:5046$' obj/sysinfo_ini.txt"
# ---- 2. SysInfo ----
printf '[System]\r\nGrafik=VGA\r\nSound=SB\r\nSB=A220 I5 D1\r\nNetz=PAKET\r\n3D=RAGE128\r\n3DPCI=1002:5046\r\nMaus=KEINE\r\n' > obj/sysinfo_rd2.ini
mcopy -o -i $IMG obj/sysinfo_rd2.ini ::/RD2/RD2.INI
printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS A:\\RD2\\SYSTEST.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_sysinfo2.txt
mcopy -o -i $IMG obj/ae_sysinfo2.txt ::/AUTOEXEC.BAT
starten
for i in $(seq 90); do grep -aq "SYSTEST: Ende\|RD2: Fehler" $SER 2>/dev/null && break; sleep 1; done; sleep 2
kill $QP 2>/dev/null; wait $QP 2>/dev/null
S=$(tr -d '\r' < $SER)
w() { echo "$S" | grep -a "^SYSTEST: $1 0x" | head -1 | sed 's/.* 0x//'; }
pruef "SysInfo lehnt zu kleine Groesse ab" "! echo \"\$S\" | grep -q 'zu kleine Groesse angenommen'"
pruef "SysInfo Rueckgabe 0" "[ \"$(w Rueckgabe)\" = 00000000 ]"
pruef "cpu Pentium (7)" "[ \"$(w cpu)\" = 00000007 ]"
pruef "kb_konv > 0" "[ \"$(w kb_konv)\" != 00000000 ] && [ -n \"$(w kb_konv)\" ]"
pruef "kb_frei > 0" "[ \"$(w kb_frei)\" != 00000000 ] && [ -n \"$(w kb_frei)\" ]"
pruef "sound SB (Lautsprecher, OPL2, PCM)" "[ \"$(w sound)\" = 0000000B ]"
pruef "netz 1" "[ \"$(w netz)\" = 00000001 ]"
pruef "festplatte 0 (Diskette)" "[ \"$(w festplatte)\" = 00000000 ]"
pruef "d3 RAGE128 (7)" "[ \"$(w d3)\" = 00000007 ]"
pruef "d3 PCI 1002:5046" "[ \"$(w d3_hersteller)\" = 00001002 ] && [ \"$(w d3_geraet)\" = 00005046 ]"
bd() { echo "$S" | grep -a -A1 "^A:.*$1\$" | grep -a "SYSTEST: Bedarf" | head -1 | sed 's/.* 0x//'; }
pruef "BedarfCheck XB1 passt (386, 8087, Netz, 3D-Karte, PCM, OPL2)" "[ \"$(bd 'XB1.EXE')\" = 00000000 ]"
pruef "BedarfCheck XB2: Farben, RAM, XMS, Klang fehlen (0x3C)" "[ \"$(bd 'XB2.EXE')\" = 0000003C ]"
pruef "BedarfCheck ohne BEDARF (LEER, .COM, fehlende Datei) = 0" "[ \"$(bd 'LEER.EXE')\" = 00000000 ] && [ \"$(bd 'FDAPM.COM')\" = 00000000 ] && [ \"$(bd 'GIBTSNICHT.EXE')\" = 00000000 ]"
echo "$S" | grep -a "^SYSTEST" | sed 's/^/       /'
# ---- 3. Shell: Start mit Bedarfspruefung ----
printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS A:\\RD2\\SHELL.EXE\r\nA:\\FDAPM.COM POWEROFF\r\n' > obj/ae_sysinfo3.txt
mcopy -o -i $IMG obj/ae_sysinfo3.txt ::/AUTOEXEC.BAT
starten
waitn() { for i in $(seq ${3:-150}); do [ "$(tr -d '\r' < $SER | grep -c "$1")" -ge "${2:-1}" ] && return 0; sleep 0.4; done; return 1; }
waitn "SHELL: bereit" && sleep 3 && $Q "sendkey ret" >/dev/null 2>&1
waitn 'SHELL: Ordner A:\\ ' && sleep 2 && $Q "sendkey ret" >/dev/null 2>&1
waitn 'SHELL: Ordner A:\\RD2\\ ' && sleep 2 && $Q "sendkey x" sleep0.8 "sendkey kp_2" sleep0.8 "sendkey ret" >/dev/null 2>&1
waitn "SHELL: Bedarf fehlt" 1 60 && sleep 2 && $Q "screendump obj/sysinfo_bedarf.ppm" sleep0.3 "sendkey ret" sleep1.5 "sendkey x" sleep0.8 "sendkey ret" >/dev/null 2>&1
waitn "LEER: Kern-API" 1 60; sleep 1
kill $QP 2>/dev/null; wait $QP 2>/dev/null
L=$(tr -d '\r' < $SER)
pruef "Shell: XB2 meldet fehlenden Bedarf (0x3C), startet nicht" "echo \"\$L\" | grep -q 'SHELL: Bedarf fehlt 0x0000003C' && ! echo \"\$L\" | grep -A2 'XB2.EXE' | grep -q 'LEER: Kern-API'"
pruef "Shell: XB1 startet" "echo \"\$L\" | grep -A3 'XB1.EXE' | grep -q 'LEER: Kern-API'"
[ $F = 0 ] && echo "SysInfo: alles ok" || echo "SysInfo: FEHLER"
exit $F
