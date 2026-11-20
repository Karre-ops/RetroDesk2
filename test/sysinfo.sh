#!/bin/bash
# SysInfo (API 2.3) und 3D-Kartenerkennung (PLAN_32BIT Schritt 1, 19.11.2026), QEMU:
# 1. RD2SETUP auf einem Pentium mit zusaetzlicher ATI Rage 128 Pro (QEMU ati-vga, PCI 1002:5046):
#    die Liste zeigt die Karte, S speichert 3D=RAGE128 und 3DPCI=1002:5046 in RD2.INI.
# 2. SYSTEST.EXE liest SysInfo: Klang, Netz und 3D aus RD2.INI, Speicherwerte, Diskettenbetrieb.
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
NUR="$(awk '$4=="S" && $1!="SHELL.EXE" && $1!="SHELLX.MOD" && $1!="RDCTLX.MOD" {print $1}' tools/budget.txt | tr '\n' ' ') SYSTEST.EXE" SETUP=1 bash tools/mkdisk.sh $IMG obj/ae_sysinfo1.txt >/dev/null 2>&1
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
echo "$S" | grep -a "^SYSTEST" | sed 's/^/       /'
[ $F = 0 ] && echo "SysInfo: alles ok" || echo "SysInfo: FEHLER"
exit $F
