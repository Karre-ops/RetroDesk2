#!/bin/bash
# Punkt 8: RDSHELL.EXE (DOS-Fenster), QEMU 1 MB ohne XMS (Diskettenbetrieb), VGA. Testskript A:\SK.TXT, eine Zeile
# je Sekunde wie getippt: VER, DIR, MD, CD, COPY, DIR /W, REN, TYPE, DEL, CD.., RD, Fehler (DIR auf Geloeschtes,
# Laufwerk Q:), unbekannter Befehl ohne COMMAND.COM, DOS-Programm ueber [DOS] Pfad= (ohne XMS/Festplatte: Hinweis
# statt Absturz), VOL, MEM, DATE, ECHO. Danach CGA: VER, DIR /W, Bild. Prueft COM1, Dateien danach und Bilder.
cd "$(dirname "$0")/.."; export LC_ALL=C
IMG=obj/rdshell.img; SER=obj/rdshell_com1.txt; SOCK=/tmp/qmon_rdsh.sock; F=0
cat > obj/ae_rdshell.txt <<'AE'
@echo off
rem RDSHELL.EXE FREIMEM.EXE (fuer mkdisk)
A:\RD2\RD.EXE /NOMAUS /VGA A:\RD2\RDSHELL.EXE T A:\SK.TXT
A:\RD2\RD.EXE /NOMAUS /CGA A:\RD2\RDSHELL.EXE T A:\SK2.TXT
A:\FDAPM.COM POWEROFF
AE
FMT=720 bash tools/mkdisk.sh $IMG obj/ae_rdshell.txt >/dev/null
python3 -c "f=open('$IMG','r+b'); f.seek(0x27); f.write(bytes([0x78,0x56,0x34,0x12]))"   # feste Seriennummer 1234-5678 (Bild, VOL)
mmd -i $IMG ::/BIN 2>/dev/null; mcopy -o -i $IMG out/FREIMEM.EXE ::/BIN/; mdel -i $IMG ::/RD2/FREIMEM.EXE 2>/dev/null
printf '[DOS]\r\nPfad=A:\\BIN\r\nComspec=A:\\NIX.COM\r\n' > obj/rdsh_ini.txt; mcopy -o -i $IMG obj/rdsh_ini.txt ::/RD2/RD2.INI
printf '%s\r\n' "ver" "dir" "md test" "cd test" "copy a:\\rd2\\rd2.ini" "copy rd2.ini zwei.txt" "dir /w" \
  "ren zwei.txt drei.txt" "type drei.txt" "del rd2.ini" "del drei.txt" "cd.." "rd test" "dir test" "q:" \
  "gibtsnicht" "cd \\" "freimem a:\\frei.txt hallo" "cd rd2" "vol" "mem" "date" "echo fertig" > obj/rdsh_sk.txt
printf '%s\r\n' "ver" "cd \\" "dir /w" "echo fertig" > obj/rdsh_sk2.txt
mcopy -o -i $IMG obj/rdsh_sk.txt ::/SK.TXT; mcopy -o -i $IMG obj/rdsh_sk2.txt ::/SK2.TXT
rm -f $SER $SOCK obj/rdsh?_*.ppm; touch $SER
timeout 240 qemu-system-i386 -m 1 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 -boot a \
  -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
QP=$!
sed "s#/tmp/qmon.sock#$SOCK#" ../test/qemu/qmon.py > obj/qmon_rdsh.py
Q() { python3 obj/qmon_rdsh.py "$@" 2>/dev/null; }
waitn() { for i in $(seq 450); do [ "$(tr -d '\r' < $SER | grep -c -- "$1" || true)" -ge "$2" ] && return 0; sleep 0.2; done; echo "FEHLER: warte auf $1 ($2)"; return 1; }
waitn "RDSHELL: | A:.*>rd test" 1 && Q "screendump obj/rdsh1_vga.ppm"
waitn "RDSHELL: | fertig" 1 && Q "screendump obj/rdsh2_vga.ppm"
waitn "RDSHELL: | fertig" 2 && Q "screendump obj/rdsh3_cga.ppm"
wait $QP
L=$(tr -d '\r' < $SER)
chk() { if echo "$L" | grep -q -- "$1"; then echo "ok     $2"; else echo "FEHLER $2"; F=1; fi; }
chk "RDSHELL: | A:\\\\>ver" "Eingabe mit Prompt im Verlauf"
chk "RDSHELL: | RetroDesk 2 (Kern [0-9]\.[0-9][0-9]), DOS [0-9]\.[0-9][0-9]" "VER"
chk "RDSHELL: |  Verzeichnis von A:\\\\" "DIR Kopf"
chk "RDSHELL: | KERNEL   SYS .* [0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]  [0-9][0-9]:[0-9][0-9]" "DIR Zeile mit Groesse, Datum, Zeit"
chk "RDSHELL: |   [0-9]* Datei(en) .* Bytes" "DIR Summe"
chk "Bytes frei" "DIR freier Platz"
chk "RDSHELL: | A:\\\\TEST>copy" "MD und CD"
chk "RDSHELL: | 1 Datei(en) kopiert" "COPY"
chk "RDSHELL: | \[\.\.\]           RD2.INI        ZWEI.TXT" "DIR /W"
chk "RDSHELL: | Pfad=A:" "REN und TYPE"
chk "RDSHELL: | A:\\\\>rd test" "CD.."
chk "RDSHELL: | Datei nicht gefunden" "DIR auf geloeschtes Verzeichnis"
chk "RDSHELL: | Laufwerk nicht bereit" "Laufwerk Q: fehlt"
chk "RDSHELL: | Befehl oder Dateiname nicht gefunden" "unbekannter Befehl ohne COMMAND.COM"
chk "RDSHELL: RunDos A:\\\\BIN\\\\FREIMEM.EXE a:\\\\frei.txt hallo" "Programm ueber [DOS] Pfad= gefunden"
chk "RDSHELL: | DOS-Programme brauchen XMS" "ohne XMS/Festplatte: Hinweis"
chk "RDSHELL: |  Datentr.ger in Laufwerk A" "VOL"
chk "RDSHELL: |  Seriennummer 1234-5678" "VOL Seriennummer"
chk "RDSHELL: | Speicher gesamt .* KB" "MEM"
chk "RDSHELL: | Datum: [0-9][0-9]\.[0-9][0-9]\.20[0-9][0-9]" "DATE"
chk "RDSHELL: | fertig" "ECHO"
[ "$(echo "$L" | grep -c "RDSHELL: Ende")" -ge 2 ] && echo "ok     beide Skripte zu Ende (VGA, CGA)" || { echo "FEHLER Skriptende"; F=1; }
[ "$(echo "$L" | grep -c "Speicher vollstaendig zurueck")" -ge 2 ] && echo "ok     Speicher zurueck (2 Laeufe)" || { echo "FEHLER Speicher"; F=1; }
mdir -i $IMG ::/RD2/TEST >/dev/null 2>&1 && { echo "FEHLER TEST liegt noch da"; F=1; } || echo "ok     TEST wieder geloescht"
for b in rdsh1_vga rdsh2_vga rdsh3_cga; do
  if [ -f test/ref/$b.png ]; then python3 test/bildvergleich.py obj/$b.ppm test/ref/$b.png || F=1; else [ -f obj/$b.ppm ] && echo "neu    obj/$b.ppm (keine Referenz)" || { echo "FEHLER $b fehlt"; F=1; }; fi
done
[ $F = 0 ] && echo "RDSHELL: ok" || { echo "RDSHELL: FEHLER"; echo "$L" | grep "RDSHELL\|RD: Fehler" | tail -50; }
exit $F
