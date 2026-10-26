/* Treiber fuer den Vergleich mit dem RD1-Rechenkern: je Zeile Tasten, Ausgabe die Anzeige (test/rechner_kern.sh) */
#include <stdio.h>
#include <string.h>
void calc_reset(void); void calc_key(char); extern char calc_disp[];
int main(void){ char l[256]; while(fgets(l,sizeof l,stdin)){ char*p; calc_reset(); for(p=l;*p&&*p!='\n';p++) calc_key(*p=='b'?'\b':*p); puts(calc_disp);} return 0;}
