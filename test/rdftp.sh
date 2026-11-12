#!/bin/bash
# Punkt 8: RDFTP.EXE (FTP als zweites Dateifenster) mit NE2000 in QEMU, nur Tastatur, Host-FTP-Server test/ftpserver.py.
# VGA: Alt+V verbinden (retro/geheim), [pub] oeffnen, GROSS.BIN (20000) laden, ".." zurueck, langen Namen laden
# (als EINSEHRL.TXT), links HOCH.TXT (5000) hochladen, rechts LIESMICH.TXT loeschen, Ordner "neu" anlegen, Esc.
# CGA (30.10.2026): ohne Netz F7 Ordner, Mehrfachauswahl + F5 Platte zu Platte, Umschalt+F10 Neue Textdatei, dann
# verbinden und zwei markierte Serverdateien mit F5 laden.
# Pfeile ueber den Ziffernblock (kp_2, kp_7): QEMU verdoppelt graue E0-Tasten, weil der Kern-INT-9 Port 60h liest
# (in QEMU zerstoerend, auf echter Hardware nicht). CGA: verbinden, Bild. Pruefung: Dateien auf beiden Seiten gleich, Bilder.
cd "$(dirname "$0")/.."
IMG=obj/rdftp.img; SER=obj/rdftp_com1.txt; SOCK=/tmp/qmon_rdftp.sock; HOSTLOG=obj/rdftp_host.txt; R=obj/ftproot
cat > obj/ae_rdftp.txt <<'AE'
@echo off
rem RDTCP.MOD (fuer mkdisk)
A:\NE2000.COM 0x60 3 0x300
A:\RD2\RD2.EXE /NOMAUS /VGA A:\RD2\RDFTP.EXE T A:\T1\
A:\RD2\RD2.EXE /NOMAUS /CGA A:\RD2\RDFTP.EXE T A:\T1\
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_rdftp.txt >/dev/null
mcopy -o -i $IMG test/qemu/pkt/NE2000.COM ::/
rm -rf obj/ftproot obj/rdftp_t1 obj/rdftp_aus; mkdir -p obj/ftproot/pub obj/rdftp_t1 obj/rdftp_aus
python3 - <<'PY'
import random
r = random.Random(8)
open('obj/ftproot/LIESMICH.TXT', 'wb').write(b'RD2 FTP-Test\r\n' * 7)
open('obj/ftproot/ein sehr langer Dateiname.txt', 'wb').write(bytes(r.randrange(256) for _ in range(3000)))
open('obj/ftproot/pub/GROSS.BIN', 'wb').write(bytes(r.randrange(256) for _ in range(20000)))
open('obj/rdftp_t1/HOCH.TXT', 'wb').write(bytes(r.randrange(256) for _ in range(5000)))
open('obj/rdftp_ini.txt', 'wb').write(b'[Netz]\r\nDHCP=1\r\n[FTP]\r\nServer=10.0.2.2\r\nPort=2121\r\nBenutzer=retro\r\nKennwort=geheim\r\n')
PY
mmd -i $IMG ::/T1 2>/dev/null; mcopy -o -i $IMG obj/rdftp_t1/HOCH.TXT ::/T1/; mcopy -o -i $IMG obj/rdftp_ini.txt ::/RD2/RD2.INI
rm -f $SER $SOCK $HOSTLOG obj/rdftp?_*.ppm
for p in /proc/[0-9]*; do case "$(tr '\0' ' ' < $p/cmdline 2>/dev/null)" in "python3 test/ftpserver.py "*) kill ${p#/proc/} 2>/dev/null;; esac; done   # alter Server von abgebrochenem Lauf
python3 test/ftpserver.py $R 2121 > $HOSTLOG 2>&1 &
HP=$!
sleep 0.5
timeout 270 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -netdev user,id=n0 -device ne2k_isa,iobase=0x300,irq=3,netdev=n0 \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" test/qemu/qmon.py > obj/qmon_rdftp.py
Q() { python3 obj/qmon_rdftp.py "$@" 2>/dev/null; }
waitn() { for i in $(seq 300); do [ "$(grep -c "$1" $SER 2>/dev/null || true)" -ge "$2" ] 2>/dev/null && return 0; sleep 0.2; done; echo "FEHLER: warte auf $1 ($2)"; return 1; }
lauf() {
  waitn "RDFTP: bereit" 1 || return; sleep 3
  Q "screendump obj/rdftp1_vga.ppm" sleep0.3 "sendkey alt-v"
  waitn "RDFTP: rechts" 1 || return; sleep 2
  Q "screendump obj/rdftp2_vga.ppm" sleep0.3 "sendkey ret"
  waitn "RDFTP: rechts" 2 || return; sleep 1
  Q "sendkey kp_2" sleep0.3 "sendkey alt-k"
  waitn "RDFTP: geladen" 1 || return; sleep 1
  Q "sendkey kp_7" sleep0.3 "sendkey ret"
  waitn "RDFTP: rechts" 3 || return; sleep 1
  Q "sendkey kp_2" sleep0.3 "sendkey alt-k"
  waitn "RDFTP: geladen" 2 || return; sleep 1
  Q "sendkey tab" sleep0.3 "sendkey h" sleep0.3 "sendkey alt-k"
  waitn "RDFTP: gesendet" 1 || return; waitn "RDFTP: rechts" 4 || return; sleep 1
  Q "sendkey tab" sleep0.3 "sendkey l" sleep0.3 "sendkey alt-l"
  waitn "RDFTP: rechts" 5 || return; sleep 1
  Q "sendkey alt-n"; sleep 1.5; Q "sendkey n" sleep0.15 "sendkey e" sleep0.15 "sendkey u" sleep0.3 "sendkey ret"
  waitn "RDFTP: rechts" 6 || return; sleep 1.5
  Q "screendump obj/rdftp3_vga.ppm" sleep0.3 "sendkey esc"
  waitn "RDFTP: Ende" 1 || return
  waitn "RDFTP: bereit" 2 || return; sleep 3
  # zweiseitig ohne Netz: F7 Ordner SUB, rechts hinein, links zwei Dateien markieren (Leertaste), F5 kopiert beide
  Q "sendkey f7"; sleep 1.5; Q "sendkey s" sleep0.15 "sendkey u" sleep0.15 "sendkey b" sleep0.3 "sendkey ret"
  waitn "RDFTP: rlokal" 3 || return; sleep 1
  Q "sendkey tab" sleep0.4 "sendkey kp_2" sleep0.4 "sendkey ret"
  waitn "RDFTP: rlokal" 4 || return; sleep 1
  Q "sendkey tab" sleep0.4 "sendkey kp_2" sleep0.4 "sendkey kp_2" sleep0.4 "sendkey spc" sleep0.8 "sendkey spc" sleep0.8 "sendkey f5"
  waitn "RDFTP: rlokal" 5 || return; sleep 1
  # Umschalt+F10: Menue, T = Neue Textdatei LIES
  Q "sendkey shift-f10"; sleep 1.2; Q "sendkey t"; sleep 1.5; Q "sendkey l" sleep0.15 "sendkey i" sleep0.15 "sendkey e" sleep0.15 "sendkey s" sleep0.3 "sendkey ret"
  waitn "RDFTP: links 5" 1 || return; sleep 1
  Q "sendkey alt-v"
  waitn "RDFTP: rechts" 7 || return; sleep 2
  # Server: zwei Dateien markieren, F5 laedt beide nacheinander
  Q "sendkey kp_2" sleep0.4 "sendkey kp_2" sleep0.4 "sendkey spc" sleep0.8 "sendkey spc" sleep0.8 "sendkey f5"
  waitn "RDFTP: geladen" 4 || return; sleep 2
  Q "screendump obj/rdftp4_cga.ppm" sleep0.3 "sendkey esc"
}
lauf
wait $QP; kill $HP 2>/dev/null; wait $HP 2>/dev/null
S=$(tr -d '\r' < $SER)
grep -E "RDFTP|RDTCP" <<< "$S"
grep -vE "^host: < (USER|TYPE|PWD|PASV)" $HOSTLOG | head -40
mcopy -o -i $IMG ::/T1/GROSS.BIN ::/T1/EINSEHRL.TXT obj/rdftp_aus/ 2>/dev/null
ok() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; fi; }
ok "Anmelden retro/geheim, Liste /" "grep -q 'RDFTP: verbunden /' <<< \"\$S\""
ok "GROSS.BIN geladen, 20000 Bytes gleich" "cmp -s obj/rdftp_aus/GROSS.BIN obj/ftproot/pub/GROSS.BIN"
ok "Langer Name als EINSEHRL.TXT gleich" "cmp -s obj/rdftp_aus/EINSEHRL.TXT 'obj/ftproot/ein sehr langer Dateiname.txt'"
ok "HOCH.TXT hochgeladen, 5000 Bytes gleich" "cmp -s obj/ftproot/HOCH.TXT obj/rdftp_t1/HOCH.TXT"
ok "LIESMICH.TXT geloescht" "[ ! -e obj/ftproot/LIESMICH.TXT ]"
ok "Ordner neu angelegt" "[ -d obj/ftproot/neu ]"
T1=$(mdir -b -i $IMG ::/T1 2>/dev/null | tr -d '\r'); SUB=$(mdir -b -i $IMG ::/T1/SUB 2>/dev/null | tr -d '\r')
ok "zweiseitig: F7 Ordner SUB, zwei markierte Dateien mit F5 nach SUB kopiert" "grep -q EINSEHRL.TXT <<< \"\$SUB\" && grep -q GROSS.BIN <<< \"\$SUB\" && [ \$(grep -c 'RDFTP: kopiert' <<< \"\$S\") -eq 2 ]"
ok "Rechtsklickmenue (Umschalt+F10): Neue Textdatei LIES.TXT" "grep -q LIES.TXT <<< \"\$T1\""
ok "Server: zwei markierte Dateien nacheinander geladen" "[ \$(grep -c 'RDFTP: geladen' <<< \"\$S\") -eq 4 ]"
ok "Speicher zurueck (2 Laeufe)" "[ \$(grep -c 'Speicher vollstaendig zurueck' <<< \"\$S\") -ge 2 ]"
[ -f test/ref/rdftp1_vga.png ] && python3 test/bildvergleich.py obj/rdftp1_vga.ppm test/ref/rdftp1_vga.png obj/rdftp2_vga.ppm test/ref/rdftp2_vga.png obj/rdftp3_vga.ppm test/ref/rdftp3_vga.png obj/rdftp4_cga.ppm test/ref/rdftp4_cga.png
exit 0
