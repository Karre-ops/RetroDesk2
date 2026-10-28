/* RetroDesk - Rechenkern des Taschenrechners (ohne DOS, auch am PC testbar) */
#ifndef CALC_H
#define CALC_H

extern char calc_disp[24];    /* Anzeige */
extern char calc_op;          /* anstehende Rechenart oder 0 */

void calc_reset(void);
/* Tasten: 0-9 . + - * / = C (loeschen) S (Wurzel) % N (Vorzeichen) \b */
void calc_key(char k);

#endif
