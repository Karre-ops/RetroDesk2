#!/bin/bash
# Punkt 8: RDIRC.EXE mit NE2000 in QEMU, nur Tastatur, Host-IRC-Server test/ircserver.py (Port 6667).
# VGA: verbindet selbst (PASS, Nick abgelehnt -> RD2_), PONG, Kanal #retro, Umlaute UTF-8 -> CP437, ACTION, private
# Nachricht, VERSION-Antwort; tippt "Hallo aus RD2", "/me tippt", "/msg bob Gruss", "/nick Karsten", Bild auf, Esc, Esc.
# CGA: verbinden, Bild. Pruefung im Serverprotokoll und auf COM1, Bilder.
cd "$(dirname "$0")/.."
IMG=obj/rdirc.img; SER=obj/rdirc_com1.txt; SOCK=/tmp/qmon_rdirc.sock; HOSTLOG=obj/rdirc_host.txt
cat > obj/ae_rdirc.txt <<'AE'
@echo off
rem RDTCP.MOD (fuer mkdisk)
A:\NE2000.COM 0x60 3 0x300
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDIRC.EXE T
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\RDIRC.EXE T
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_rdirc.txt >/dev/null
mcopy -o -i $IMG test/qemu/pkt/NE2000.COM ::/
printf '[Netz]\r\nDHCP=1\r\n[IRC]\r\nServer=10.0.2.2\r\nPort=6667\r\nNick=RD2\r\nKanal=#retro\r\nKennwort=geheim\r\n' > obj/rdirc_ini.txt
mcopy -o -i $IMG obj/rdirc_ini.txt ::/RD2/RD2.INI
rm -f $SER $SOCK $HOSTLOG obj/rdirc?_*.ppm
for p in /proc/[0-9]*; do case "$(tr '\0' ' ' < $p/cmdline 2>/dev/null)" in "python3 test/ircserver.py "*) kill ${p#/proc/} 2>/dev/null;; esac; done
(python3 test/ircserver.py 6667 > $HOSTLOG 2>&1; python3 test/ircserver.py 6667 >> $HOSTLOG 2>&1) &
HP=$!
sleep 0.5
timeout 200 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -netdev user,id=n0 -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_rdirc.py
Q() { python3 obj/qmon_rdirc.py "$@" 2>/dev/null; }
tipp() { python3 - "$1" <<'PY'
import sys, subprocess
m={' ':'spc','/':'slash','#':'shift-3','-':'minus','.':'dot',',':'comma'}
a=[]
for ch in sys.argv[1]:
    a += ['sendkey ' + (m.get(ch) or ('shift-' + ch.lower() if ch.isupper() else ch)), 'sleep0.06']
a += ['sendkey ret']
subprocess.run(['python3', 'obj/qmon_rdirc.py'] + a, stderr=subprocess.DEVNULL)
PY
}
waitn() { for i in $(seq 300); do [ "$(grep -ac "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; echo "FEHLER: warte auf $1 ($2)"; return 1; }
lauf() {
  waitn "RDIRC: angemeldet" 1 || return
  waitn "RDIRC: | \*bob\* geheime" 1 || return; sleep 2
  Q "screendump obj/rdirc1_vga.ppm"
  tipp "Hallo aus RD2"
  waitn "RDIRC: | <alice> Dies ist" 1 || return; sleep 1.5
  Q "screendump obj/rdirc2_vga.ppm"
  tipp "/me tippt auf dem XT"; sleep 0.5
  tipp "/msg bob Gruss"; sleep 0.5
  tipp "/nick Karsten"
  waitn "RDIRC: | -!- RD2_" 1 || return; sleep 1
  Q "sendkey pgup"; sleep 1.5
  Q "screendump obj/rdirc3_vga.ppm" sleep0.3 "sendkey esc"
  waitn "RDIRC: Ende" 1 || return
  waitn "RDIRC: angemeldet" 2 || return; sleep 3
  Q "screendump obj/rdirc4_cga.ppm" sleep0.3 "sendkey esc"
}
lauf
wait $QP; kill $HP 2>/dev/null; wait $HP 2>/dev/null
S=$(tr -d '\r' < $SER)
grep -aE "RDIRC" <<< "$S" | head -80
cat $HOSTLOG | head -60
H=$(cat $HOSTLOG)
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; fi; }
ok "PASS vor NICK" "grep -q 'host: < PASS geheim' <<< \"\$H\""
ok "Nick belegt -> RD2_" "grep -q 'host: < NICK RD2_' <<< \"\$H\""
ok "PONG" "grep -q 'host: < PONG :abc123' <<< \"\$H\""
ok "JOIN #retro nach 001" "grep -q 'host: < JOIN #retro' <<< \"\$H\""
ok "VERSION-Antwort" "grep -q 'NOTICE bob :.VERSION RetroDesk 2' <<< \"\$H\""
ok "Text an den Kanal" "grep -q 'host: < PRIVMSG #retro :Hallo aus RD2' <<< \"\$H\""
ok "/me als ACTION" "grep -q 'PRIVMSG #retro :.ACTION tippt auf dem XT' <<< \"\$H\""
ok "/msg privat" "grep -q 'host: < PRIVMSG bob :Gruss' <<< \"\$H\""
ok "/nick" "grep -q 'host: < NICK Karsten' <<< \"\$H\" && grep -aq 'RDIRC: | -!- RD2_ ' <<< \"\$S\""
ok "QUIT beim Schliessen (2 Laeufe)" "[ \$(grep -c 'host: < QUIT :RetroDesk 2' <<< \"\$H\") -ge 2 ]"
ok "Umlaute UTF-8 -> CP437" "python3 -c \"import sys; d=open('$SER','rb').read(); sys.exit(0 if b'sch\\x94n dass' in d and b'Gr\\x81\\xe1e' in d else 1)\""
ok "ACTION und privat angezeigt" "grep -aq 'RDIRC: | \* alice winkt' <<< \"\$S\" && grep -aq 'RDIRC: | \*bob\* geheime Nachricht' <<< \"\$S\""
ok "Speicher zurueck (2 Laeufe)" "[ \$(grep -c 'Speicher vollstaendig zurueck' <<< \"\$S\") -ge 2 ]"
[ -f test/ref/rdirc1_vga.png ] && python3 test/bildvergleich.py obj/rdirc1_vga.ppm test/ref/rdirc1_vga.png obj/rdirc2_vga.ppm test/ref/rdirc2_vga.png obj/rdirc3_vga.ppm test/ref/rdirc3_vga.png obj/rdirc4_cga.ppm test/ref/rdirc4_cga.png
exit 0
