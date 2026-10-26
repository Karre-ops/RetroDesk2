/* RetroDesk 2 - Rechenkern (aus RetroDesk 1 calc.c, ohne DOS, auch am PC testbar: test/rechner_kern.sh) */
#ifndef RKERN_H
#define RKERN_H

extern char calc_disp[24];    /* Anzeige */
extern char calc_op;          /* anstehende Rechenart oder 0 */

void calc_reset(void);
/* Tasten: 0-9 . + - * / = C (loeschen) S (Wurzel) % N (Vorzeichen) \b */
void calc_key(char k);

#endif
