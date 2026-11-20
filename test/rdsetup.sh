#!/bin/bash
# RDSETUP (PLAN_32BIT Schritt 1, 20.11.2026), QEMU 486 mit Festplatte, RD2 von Diskette:
# 1. Paket A:\PAKET\SETUP.INF nach C:\SPIELE\TESTPAK installieren (Zwischenordner fehlt): Dateien ohne oder mit erfuellter
#    Bedingung (386 auf dem 486) kommen mit, FARBE (VGA mono), 3D (keine Karte) und KLANG (Lautsprecher) nicht;
#    RDSETUP.LST im Ziel, Eintrag in PROGRAMM.LST mit Schreibtisch=1.
# 2. RDSETUP /D C:\SPIELE\TESTPAK\RDSETUP.LST: Dateien, Liste und Ordner weg, Eintrag in PROGRAMM.LST weg.
cd "$(dirname "$0")/.."
IMG=obj/rdsetup.img; HD=obj/rdsetup_hd.img; SER=obj/rdsetup_com1.txt; SOCK=/tmp/qmon_rdsetup.sock; F=0
Q="python3 test/qemu/qmon.py"; export QMON=$SOCK; C="-i $HD@@32256"
pruef() { if eval "$2"; then echo "ok     $1"; else echo "FEHLER $1"; F=1; fi; }
waitn() { for i in $(seq ${2:-150}); do grep -aq "$1" $SER 2>/dev/null && return 0; sleep 0.4; done; echo "FEHLER: warte vergeblich auf '$1'"; return 1; }
lauf() {   # $1 = Befehlszeile fuer RD2, dann Tasten nach den Meldungen $2 und $3
  printf '@echo off\r\nA:\\RD2\\RD2.EXE /NOMAUS %s\r\nA:\\FDAPM.COM POWEROFF\r\n' "$1" > obj/ae_rdsetup.txt; mcopy -o -i $IMG obj/ae_rdsetup.txt ::/AUTOEXEC.BAT
  rm -f $SOCK $SER; touch $SER
  timeout 200 qemu-system-i386 -cpu 486 -m 4 -display none -no-reboot -drive file=$IMG,format=raw,if=floppy,index=0 \
    -drive file=$HD,format=raw,index=0,media=disk -boot a -serial file:$SER -monitor unix:$SOCK,server,nowait >/dev/null 2>&1 &
  QP=$!
  waitn "$2" && sleep 3 && $Q "sendkey ret" >/dev/null 2>&1
  waitn "$3" && sleep 2 && $Q "sendkey ret" >/dev/null 2>&1
  for i in $(seq 60); do kill -0 $QP 2>/dev/null || break; sleep 0.5; done; kill $QP 2>/dev/null; wait $QP 2>/dev/null
}
printf '@echo off\r\n' > obj/ae_rdsetup.txt
NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') RDSETUP.EXE LEER.EXE" FMT=720 bash tools/mkdisk.sh $IMG obj/ae_rdsetup.txt >/dev/null 2>&1
printf '[System]\r\nGrafik=VGA\r\nSound=SPEAKER\r\nNetz=KEIN\r\n3D=KEINE\r\nMaus=KEINE\r\n' > obj/rdsetup_rd2.ini; mcopy -o -i $IMG obj/rdsetup_rd2.ini ::/RD2/RD2.INI
bash tools/mkhd.sh $HD >/dev/null
# Paket
mmd -i $IMG ::/PAKET
printf 'BEDARF CPU 8088\n' > obj/rs8.rc; cp out/LEER.EXE obj/XB8.EXE; python3 tools/rdrc.py obj/rs8.rc obj/rs8.res >/dev/null; python3 tools/rd2link.py obj/XB8.EXE "XB8" --res obj/rs8.res >/dev/null
for n in DATEN XB386 FARB GLIDE KLANG; do printf "$n\r\n" > obj/rs_$n.dat; done
mcopy -i $IMG obj/XB8.EXE ::/PAKET/; mcopy -i $IMG obj/rs_DATEN.dat ::/PAKET/DATEN.DAT; mcopy -i $IMG obj/rs_XB386.dat ::/PAKET/XB386.EXE
mcopy -i $IMG obj/rs_FARB.dat ::/PAKET/FARB.DAT; mcopy -i $IMG obj/rs_GLIDE.dat ::/PAKET/GLIDE.DXE; mcopy -i $IMG obj/rs_KLANG.dat ::/PAKET/KLANG.DAT
printf '[Paket]\r\nName=Testpaket\r\nVersion=1.0\r\nZiel=C:\\SPIELE\\TESTPAK\r\nProgramm=XB8.EXE\r\nSchreibtisch=1\r\n[Dateien]\r\n1=XB8.EXE\r\n2=DATEN.DAT\r\n3=XB386.EXE 386\r\n4=FARB.DAT FARBE\r\n5=GLIDE.DXE 3D\r\n6=KLANG.DAT KLANG\r\n' > obj/rs_setup.inf
mcopy -i $IMG obj/rs_setup.inf ::/PAKET/SETUP.INF
# ---- 1. installieren ----
lauf 'A:\RD2\RDSETUP.EXE A:\PAKET\SETUP.INF' "RDSETUP: Paket" "RDSETUP: installiert"
L=$(tr -d '\r' < $SER)
for f in XB8.EXE DATEN.DAT XB386.EXE RDSETUP.LST; do pruef "installiert: $f" "mdir $C ::/SPIELE/TESTPAK/$f >/dev/null 2>&1"; done
for f in FARB.DAT GLIDE.DXE KLANG.DAT; do pruef "weggelassen: $f" "! mdir $C ::/SPIELE/TESTPAK/$f >/dev/null 2>&1"; done
pruef "Protokoll: 3 Dateien installiert, 3 weggelassen" "[ \$(echo \"\$L\" | grep -c 'RDSETUP: weggelassen') = 3 ] && echo \"\$L\" | grep -q 'RDSETUP: installiert 0x00000003'"
mtype -i $IMG ::/RD2/PROGRAMM.LST 2>/dev/null | tr -d '\r' > obj/rdsetup_lst1.txt
pruef "PROGRAMM.LST: [XB8] Datei=C:\\SPIELE\\TESTPAK\\XB8.EXE, Schreibtisch=1" "grep -q '^\[XB8\]' obj/rdsetup_lst1.txt && grep -qi '^Datei=C:.SPIELE.TESTPAK.XB8.EXE' obj/rdsetup_lst1.txt && grep -q '^Schreibtisch=1' obj/rdsetup_lst1.txt"
# ---- 2. entfernen ----
lauf 'A:\RD2\RDSETUP.EXE /D C:\SPIELE\TESTPAK\RDSETUP.LST' "RDSETUP: entfernen" "RDSETUP: entfernt"
pruef "entfernt: Ordner C:\\SPIELE\\TESTPAK weg" "! mdir $C ::/SPIELE/TESTPAK >/dev/null 2>&1"
pruef "C:\\SPIELE bleibt" "mdir $C ::/SPIELE >/dev/null 2>&1"
mtype -i $IMG ::/RD2/PROGRAMM.LST 2>/dev/null | tr -d '\r' > obj/rdsetup_lst2.txt
pruef "PROGRAMM.LST: kein Eintrag mehr fuer XB8" "! grep -qi 'TESTPAK' obj/rdsetup_lst2.txt"
[ $F = 0 ] && echo "RDSETUP: alles ok" || echo "RDSETUP: FEHLER"
exit $F
