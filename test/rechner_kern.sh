#!/bin/bash
# Rechenkern des Rechners (apps/rechner/RKERN.C) am PC pruefen:
# 1. feste Faelle wie RD1 test/calc_test.c  2. 20000 Zufallsfolgen gegen den RD1-Kern (../calc.c), Anzeige muss gleich sein
cd "$(dirname "$0")/.."
gcc -O1 -o obj/rechner_kern test/rechner_kern.c -x c apps/rechner/RKERN.C && obj/rechner_kern || exit 1
[ -f ../calc.c ] || { echo "(RD1 calc.c fehlt, kein Vergleich)"; exit 0; }
sed 's|#include "calc.h"|#include "calc.h"|' ../calc.c > obj/rd1calc.c
gcc -O1 -I.. -o obj/rechner_alt test/rechner_vgl.c obj/rd1calc.c && gcc -O1 -o obj/rechner_neu test/rechner_vgl.c -x c apps/rechner/RKERN.C || exit 1
python3 -c "
import random; random.seed(7)
K='0123456789.+-*/=%SNCb'; W=[8]*10+[1,2,2,2,2,3,1,1,1,0.3,0.5]
for _ in range(20000): print(''.join(random.choices(K,W,k=random.randint(1,25))))" > obj/rechner_folgen.txt
obj/rechner_alt < obj/rechner_folgen.txt > obj/rechner_alt.txt; obj/rechner_neu < obj/rechner_folgen.txt > obj/rechner_neu.txt
n=$(paste -d'|' obj/rechner_folgen.txt obj/rechner_alt.txt obj/rechner_neu.txt | awk -F'|' '$2!=$3' | tee obj/rechner_abw.txt | wc -l)
[ "$n" = 0 ] && echo "ok     20000 Zufallsfolgen wie RD1" || { echo "FEHLER $n Abweichungen zu RD1 (obj/rechner_abw.txt)"; head -5 obj/rechner_abw.txt; exit 1; }
