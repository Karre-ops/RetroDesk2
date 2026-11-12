#!/bin/bash
# Diskettentest: RD2 von einer 360-KB-Diskette booten (Verteilformat seit 24.10.2026) (QEMU, nur Laufwerk A:, keine Festplatte).
# Phase 1: LEER.EXE direkt unter DOS und ueber den Kern.
# Phase 2: PING startet PONG, RECHNE, MEMTEST: Nachrichten, Zeitgeber, Zeitscheiben, Speicher.
#          Einmal mit Zeitscheiben, einmal mit /KOOP (dann muss RECHNE alles aufhalten).
cd "$(dirname "$0")/.."
IMG=obj/sys360.img; SER=obj/com1.txt
cat > obj/autoexec.txt <<'AE'
@echo off
A:\RD2\LEER.EXE > A:\DIREKT.TXT
if errorlevel 1 echo FEHLERCODE1 >> A:\DIREKT.TXT
A:\RD2\RD2.EXE A:\RD2\LEER.EXE hallo welt > A:\RD.TXT
echo === PRAEEMPTIV
A:\RD2\RD2.EXE A:\RD2\PING.EXE > A:\RD2.TXT
echo === KOOP
A:\RD2\RD2.EXE /KOOP A:\RD2\PING.EXE > A:\RD3.TXT
A:\RD2\RD2.EXE A:\RD2\FILETEST.EXE
A:\RD2\RD2.EXE A:\RD2\RESTEST.EXE
A:\RD2\RD2.EXE A:\RD2\MODTEST.EXE
A:\RD2\RD2.EXE A:\RD2\HAENGT.EXE
A:\RD2\RD2.EXE A:\RD2\STRESS.EXE
A:\FDAPM.COM POWEROFF
AE
SYS=$(awk '$4=="S" && $1!="RDSYS.EXE" && $1!="SHELL.EXE"{print $1}' tools/budget.txt | tr '\n' ' ')   # ohne SHELL (hier nicht gebraucht), damit es mit FreeDOS auf 360 KB passt
NUR="$SYS LEER.EXE PING.EXE PONG.EXE RECHNE.EXE MEMTEST.EXE FILETEST.EXE RESTEST.EXE RESTEST.RDB MODTEST.EXE TESTMOD.MOD TESTMOD2.MOD HAENGT.EXE STRESS.EXE WURM.EXE" \
  FMT=360 bash tools/mkdisk.sh $IMG obj/autoexec.txt   # prueft das Verteilformat (DriveInfo 360 KB)
rm -f obj/andere.img; # andere Diskette absichtlich mit DERSELBEN Seriennummer (passiert real, wenn zwei Disketten
# in derselben Sekunde formatiert werden): der Kern muss sie am Namen unterscheiden
SN=$(python3 -c "import sys;d=open('$IMG','rb').read(48);print(d[0x27:0x2b][::-1].hex().upper())")
mformat -C -f 360 -v ANDERE -N $SN -i obj/andere.img ::
rm -f $SER /tmp/qmon.sock
timeout 300 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:/tmp/qmon.sock,server,nowait >/dev/null 2>&1 &
QP=$!
Q="python3 test/qemu/qmon.py"
waitser() { for i in $(seq ${2:-60}); do grep -q "$1" $SER 2>/dev/null && return 0; sleep 1; done; return 1; }
waitscr() { for i in $(seq ${2:-30}); do $Q text 2>/dev/null | grep -q "$1" && return 0; sleep 1; done; return 1; }
# MODTEST: Diskette wechseln, wenn das Programm es sagt; Kern muss dann nach RD2SYSTEM fragen
waitser "MODTEST: jetzt Diskette wechseln" 150 && $Q "change floppy0 obj/andere.img"
waitscr "RD2SYSTEM in Laufwerk A: einlegen" 90 && SAWASK=1; $Q text > obj/schirm_abfrage.txt 2>/dev/null
$Q "eject -f floppy0" "sendkey ret" sleep3                       # leeres Laufwerk: wieder fragen, kein DOS-Text
$Q text 2>/dev/null | grep -qiE "abort|retry|fail|critical|kritisch" && SAWDOS=1
$Q "change floppy0 $IMG" sleep1 "sendkey ret"
# HAENGT: nach 6 s Strg+Alt+Rueckschritt
waitser "HAENGT: haenge jetzt" 60 && sleep 6 && $Q "sendkey ctrl-alt-backspace"
wait $QP
echo "qemu rc=$?"
F=0; chk() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=$((F+1)); fi; }
D=$(mtype -i $IMG ::/DIREKT.TXT 2>/dev/null | tr -d '\r')
S=$(tr -d '\r' < $SER 2>/dev/null)
P1=${S%%RD: RetroDesk 2 Kern (Phase 2)*}               # bis zum zweiten Kernstart: LEER
REST=${S#*Zeitscheiben an}; P2=${REST%%RD: nur kooperativ*}; P3=${S#*RD: nur kooperativ}
echo "--- Phase 1"
chk "LEER.EXE unter DOS meldet 'benoetigt RetroDesk 2'" '[[ "$D" == *"tigt RetroDesk 2."* ]]'
chk "LEER.EXE unter DOS endet mit Fehlercode 1"       '[[ "$D" == *FEHLERCODE1* ]]'
chk "LEER sieht API 2.0"                 '[[ "$S" == *"LEER: Kern-API 0x00000200"* ]]'
chk "Befehlszeile kommt an"              '[[ "$S" == *"hallo welt"* ]]'
chk "fehlende Funktion: 0 und RD_ERR_NOTSUP" '[[ "$S" == *"NetCaps fehlt noch (erwartet)"* ]]'
chk "LEER endet mit 42"                  '[[ "$S" == *"Task beendet: 0x0001002A"* ]]'
echo "--- Phase 2 mit Zeitscheiben"
chk "PING bekommt 5 Antworten"           '[[ "$P2" == *"PING: Antwort 0x00000005"* ]]'
chk "PONG hat 5 beantwortet"             '[[ "$P2" == *"PONG: Ende, beantwortet 0x00000005"* ]]'
chk "Zeitgeber, waehrend RECHNE rechnet" '[[ "$P2" == *"PING: Timer ok, RECHNE rechnet noch"* ]]'
chk "RECHNE fertig"                      '[[ "$P2" == *"RECHNE: fertig"* ]]'
chk "Speicher gefuellt (4-KB-Bloecke)"   '[[ "$P2" =~ "MEMTEST: 4-KB-Bloecke 0x000000"[0-9A-F][0-9A-F] ]]'
chk "65000 Bytes nach Verdichten"        '! [[ "$P2" == *"MEMTEST: 65000-Byte-Block 0x00000000"* ]] && [[ "$P2" == *"MEMTEST: 65000-Byte-Block"* ]]'
chk "Inhalte nach Verdichten richtig"    '[[ "$P2" == *"MEMTEST: falsche Bloecke nach Verdichten 0x00000000"* ]]'
chk "verwerfbarer Block verworfen"       '[[ "$P2" == *"MEMTEST: verwerfbarer Block verworfen"* ]]'
chk "MEMTEST gibt alles frei"            '[[ "$P2" == *"MEMTEST: alles frei"* ]]'
chk "PING endet mit 7"                   '[[ "$P2" =~ "Task beendet: 0x000"[0-9A-F]"0007" ]]'
chk "Kern: Speicher vollstaendig zurueck" '[[ "$P2" == *"Speicher vollstaendig zurueck"* ]]'
echo "--- Phase 2 kooperativ (/KOOP)"
chk "ohne Zeitscheiben wartet alles auf RECHNE" '[[ "$P3" == *"PING: Timer ok, RECHNE war schon fertig"* ]]'
chk "Kern: Speicher vollstaendig zurueck" '[[ "$P3" == *"Speicher vollstaendig zurueck"* ]]'
echo "--- Phase 2 Dateien, Profile, Ablage"
P4=$(echo "$S" | grep -A1 "^FILETEST:" )
chk "alle FILETEST-Pruefungen ok"        '[[ "$S" == *"FILETEST: Ende"* ]] && ! [[ "$P4" == *"-> FEHLER"* ]]'
echo "$S" | grep -B1 -- "-> FEHLER" | grep "^FILETEST" | sed 's/^/       /'
P5=$(echo "$S" | grep -A1 "^RESTEST:" )
chk "alle RESTEST-Pruefungen ok (Ressourcen, Texte, Zeit)" '[[ "$S" == *"RESTEST: Ende"* ]] && ! [[ "$P5" == *"-> FEHLER"* ]]'
echo "$S" | grep -B1 -- "-> FEHLER" | grep "^RESTEST" | sed 's/^/       /'
INI=$(mtype -i $IMG ::/TEST.INI 2>/dev/null | tr -d '\r')
chk "TEST.INI auf der Diskette richtig"  '[[ "$INI" == "[Benutzer]"$'"'"'\n'"'"'"Name=PC10-III"$'"'"'\n'"'"'"Zahl=42"$'"'"'\n'"'"'"[Rechner]"$'"'"'\n'"'"'"CPU=8088" ]]'
echo "--- Phase 2 Module und Diskettenwechsel"
chk "Modul: Quadrat(7) = 49"             '[[ "$S" == *"MODTEST: Quadrat(7) 0x00000031"* ]]'
chk "Reserve: Fuellen verdraengt Modul nicht" '[[ "$S" == *"MODTEST: Modul nach Fuellen noch da"* ]]'
chk "zweites Modul laeuft in der Reserve"  '[[ "$S" == *"MODTEST: zweites Modul 0x00000019"* ]]'
chk "es verdraengt das erste"            'echo "$S" | tr -d "\r" | grep -qx "RD: Modul verworfen: Testmodul"'
chk "Speicher voll: Nachladen klappt trotzdem" '[[ "$S" == *"MODTEST: Quadrat(8) 0x00000040"* ]]'
chk "Kern fragt nach Diskette RD2SYSTEM" '[ "$SAWASK" = 1 ]'
chk "leeres Laufwerk: keine DOS-Meldung" '[ "$SAWDOS" != 1 ]'
chk "nachgeladen, Quadrat(8) = 64"       '[[ "$S" == *"RD: Modul nachgeladen"* ]] && [[ "$S" == *"MODTEST: Quadrat(8) 0x00000040"* ]]'
chk "Moduldaten nach Nachladen neu"      '[[ "$S" == *"MODTEST: Aufrufe nach Nachladen 0x00000001"* ]]'
echo "--- Phase 2 Notbeenden"
chk "Strg+Alt+Rueckschritt beendet HAENGT" '[[ "$S" == *"RD: Notbeenden Task 0x00000001"* ]] && [[ "$S" == *"Task beendet (Kill): 0x0001FFFF"* ]]'
T=${S##*HAENGT: haenge jetzt}
chk "danach Speicher vollstaendig zurueck" '[[ "$T" == *"Speicher vollstaendig zurueck"* ]]'
echo "--- Dauertest (5 Tasks: Speicher, Nachrichten, 8087)"
T=${S##*HAENGT: haenge jetzt}; T=${T#*Zeitscheiben an}
chk "STRESS: alle Wuermer ok"            '[[ "$T" == *"STRESS: alle Wuermer ok"* ]]'
chk "STRESS: 100 Nachrichten"            '[[ "$T" == *"STRESS: Nachrichten 0x00000064"* ]]'
chk "STRESS: Speicher vollstaendig zurueck" '[[ "$T" == *"Speicher vollstaendig zurueck"* ]]'
echo "$T" | grep -E "WURM|STRESS: Dauer" | sed 's/^/       /'
python3 tools/budget.py out $IMG | tail -2
echo "--- COM1 ab MODTEST ---"; echo "${S#*FILETEST: Ende}" | head -40
[ $F = 0 ] && echo "Diskettentest: alles ok" || echo "Diskettentest: $F Fehler"
exit $F
