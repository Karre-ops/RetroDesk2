#!/bin/bash
# Voller Testsatz: alle test/*.sh nacheinander (ohne Messungen und schnell.sh), je hoechstens 6 Minuten.
# Ergebnis: obj/voll/<name>.txt je Test, Zusammenfassung obj/voll/ERGEBNIS.txt
cd "$(dirname "$0")/.."
mkdir -p obj/voll; [ -n "$1" ] || : > obj/voll/ERGEBNIS.txt   # $1: ab diesem Test weitermachen
# bootlw braucht obj/inst_hd.img aus install.sh, dosshell obj/ifmt_hd.img aus install_fmt.sh: laufen direkt danach
LISTE=$(ls test/*.sh | grep -v '/bootlw.sh$\|/dosshell.sh$' | sed 's|test/install.sh|test/install.sh test/bootlw.sh|; s|test/install_fmt.sh|test/install_fmt.sh test/dosshell.sh|')
for t in $LISTE; do
    n=$(basename $t .sh)
    [ -n "$1" ] && [[ "$n" < "$1" ]] && [ "$n" != bootlw ] && continue
    [ -n "$1" ] && [ "$n" = bootlw ] && [[ "install" < "$1" ]] && continue
    case $n in voll|schnell|bench|bench_modi|kurz) continue;; esac
    for p in /proc/[0-9]*; do case "$(cat $p/comm 2>/dev/null)" in qemu*|dosbox) kill ${p#/proc/} 2>/dev/null;; esac; done
    a=$(date +%s)
    timeout 360 bash $t > obj/voll/$n.txt 2>&1 < /dev/null; rc=$?
    f=$(grep -c "^FEHLER" obj/voll/$n.txt); o=$(grep -c "^ok" obj/voll/$n.txt)
    if [ $rc -eq 0 ] && [ $f -eq 0 ]; then e=ok; else e=FEHLER; fi
    printf "%-7s %-16s %3d ok %3d Fehler  rc=%d  %ds\n" $e $n $o $f $rc $(( $(date +%s) - a )) >> obj/voll/ERGEBNIS.txt
done
echo fertig >> obj/voll/ERGEBNIS.txt
