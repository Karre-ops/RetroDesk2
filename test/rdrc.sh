#!/bin/bash
# RDRC.EXE (DOS-Fassung des Ressourcen-Compilers) gegen tools/rdrc.py + tools/rd2link.py: jede .RC im Baum wird
# mit beiden uebersetzt und an eine Kopie von LEER.EXE gehaengt, die EXE muessen byte-gleich sein. RDRC laeuft in
# DOSBox. Dazu: zweimal linken (Anhang ersetzt), "-" ohne Ressourcen, Fehlermeldung mit Zeilennummer. Ca. 20 s.
cd "$(dirname "$0")/.."
F=0; W=obj/rdrct
python3 -c "import shutil; shutil.rmtree('$W', True)"; mkdir -p $W
[ -f out/RDRC.EXE ] || { echo "rdrc: out/RDRC.EXE fehlt (build.sh)"; exit 1; }
cp out/RDRC.EXE $W/; B=out/LEER.EXE
TYP=(P M D); i=0; : > $W/T.BAT
for rc in $(find apps disk lng shell -name "*.RC" | sort); do
  i=$((i+1)); d=$(printf "D%02d" $i); mkdir $W/$d; cp $(dirname $rc)/* $W/$d/ 2>/dev/null
  r=$(basename $rc); t=${TYP[$((i % 3))]}; s=$((2048 + i * 16)); n="Test $i ÄÖÜ"
  cp $B $W/$d/P.EXE; cp $B $W/$d/R.EXE
  python3 tools/rdrc.py $rc $W/$d/p.res >/dev/null && python3 tools/rd2link.py $W/$d/P.EXE "$n" --type $t --stack $s --kb $i --ver 0x0203 --flags $i --res $W/$d/p.res >/dev/null
  printf 'cd \\%s\r\nC:\\RDRC.EXE %s R.EXE "%s" /T:%s /S:%d /K:%d /V:0x0203 /F:%d > R.TXT\r\n' $d $r "$n" $t $s $i $i | iconv -f utf-8 -t cp437 >> $W/T.BAT
done
N=$i
# zweimal: zweiter Lauf ersetzt den Anhang; "-" ohne Ressourcen; Fehler
mkdir $W/X; cp apps/notiz/NOTIZ.RC $W/X/; cp $B $W/X/Z.EXE; cp $B $W/X/O.EXE; cp $B $W/X/PO.EXE; cp $B $W/X/PZ.EXE
python3 tools/rdrc.py apps/notiz/NOTIZ.RC $W/X/z.res >/dev/null; python3 tools/rd2link.py $W/X/PZ.EXE "Notiz" --res $W/X/z.res >/dev/null
python3 tools/rd2link.py $W/X/PO.EXE "Ohne" >/dev/null
printf 'MENU 1\r\n  POPUP "Datei"\r\n    ITEM "Ende" 10 BLAU\r\n  ENDE\r\nENDE\r\n' > $W/X/BOES.RC
printf 'cd \\X\r\nC:\\RDRC.EXE NOTIZ.RC Z.EXE "Erst"\r\nC:\\RDRC.EXE NOTIZ.RC Z.EXE "Notiz" > Z.TXT\r\nC:\\RDRC.EXE - O.EXE "Ohne" > O.TXT\r\nC:\\RDRC.EXE BOES.RC O2.EXE "x" > B.TXT\r\nif errorlevel 1 echo FEHLERCODE >> B.TXT\r\n' >> $W/T.BAT
printf 'cd \\\r\necho ENDE > ENDE.TXT\r\nexit\r\n' >> $W/T.BAT
for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in dosbox*) kill ${p#/proc/} 2>/dev/null;; esac; done
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 100 dosbox -noconsole -c "mount c $W" -c "c:" -c "T.BAT" -exit >/dev/null 2>&1
[ -f $W/ENDE.TXT ] || { echo "FEHLER DOSBox-Lauf nicht fertig"; F=1; }
ok=0
for i in $(seq $N); do d=$(printf "D%02d" $i)
  if cmp -s $W/$d/P.EXE $W/$d/R.EXE && grep -q "Ressourcen" $W/$d/R.TXT 2>/dev/null; then ok=$((ok+1))
  else echo "FEHLER $d $(ls $W/$d/*.RC | xargs -n1 basename): $(tr -d '\r' < $W/$d/R.TXT 2>/dev/null | head -1)"; F=1; fi
done
[ $ok = $N ] && echo "ok     alle $N .RC byte-gleich mit rdrc.py + rd2link.py (Typ P/M/D, Stapel, KB, Version, Flags, Name mit Umlauten)"
cmp -s $W/X/Z.EXE $W/X/PZ.EXE && echo "ok     zweites Linken ersetzt den Anhang" || { echo "FEHLER zweites Linken"; F=1; }
cmp -s $W/X/O.EXE $W/X/PO.EXE && echo "ok     RDRC - ohne Ressourcen" || { echo "FEHLER RDRC -"; F=1; }
B2=$(tr -d '\r' < $W/X/B.TXT 2>/dev/null | tr '\n' ' ')
if echo "$B2" | grep -q "BOES.RC(3): unbekannt: BLAU.*FEHLERCODE"; then echo "ok     Fehler mit Zeile: $B2"; else echo "FEHLER Fehlermeldung: $B2"; F=1; fi
[ $F = 0 ] && echo "RDRC: alles ok" || echo "RDRC: FEHLER"
exit $F
