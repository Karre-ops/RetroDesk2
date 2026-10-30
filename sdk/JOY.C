/* JOY.C - JoyRead/JoyCalibrate (API 0x12F/0x130) in RD2.LIB: laufen im Programm, Kern und Module wachsen nicht.
 * Gameport 201h: Schreiben startet die vier Monoflops, Bit 0..3 fallen nach der Zeit ab, die der Widerstand der
 * Achse bestimmt (bis ca. 1,1 ms). Gemessen wird mit dem Zeitgeber (PIT Kanal 0), damit die Werte auf XT und
 * Pentium gleich sind. Tasten: Bit 4..7, 0 = gedrueckt.
 * Joystick 0 = A (Bit 0/1, Tasten 4/5), 1 = B (Bit 2/3, Tasten 6/7).
 * JOYSTATE: x, y -100..100 (Mitte 0, links/oben negativ), tasten Bit 0 = Feuer 1, Bit 1 = Feuer 2.
 * Mitte: erste Messung oder JoyCalibrate (Knueppel loslassen). Grenzen lernt JoyRead mit, sobald der Knueppel
 * weiter ausschlaegt als erwartet. Kein Joystick (Achse faellt nie ab): RD_ERR_NOTSUP.
 * Uebersetzt mit -zu: auch aus Modulen (SS != DS) aufrufbar, alle Zeiger far. */
#include <conio.h>
#include <i86.h>
#include "RD2.H"
#pragma intrinsic(inp, outp)

#define ZEITMAX 7000                              /* PIT-Schritte (Modus 3 zaehlt in Zweierschritten): ca. 2,9 ms */
static WORD mi[2][2], mn[2][2], mx[2][2]; static BYTE kal[2];

static WORD pit(void) { WORD l; outp(0x43, 0); l = inp(0x40); return l | ((WORD)inp(0x40) << 8); }
static WORD messen(WORD nr, WORD FAR *a, BYTE FAR *t)     /* a[0] x, a[1] y in PIT-Schritten */
{
    BYTE bx = (BYTE)(nr ? 4 : 1), by = (BYTE)(nr ? 8 : 2), offen, b; WORD t0, z = 0, w;
    for (w = 0; w < 2000 && (inp(0x201) & (bx | by)); w++) ;   /* vorige Messung abklingen lassen */
    a[0] = a[1] = 0; offen = (BYTE)(bx | by);
    _disable();
    outp(0x201, 0xFF); t0 = pit();
    do {
        b = (BYTE)inp(0x201); z = (WORD)(t0 - pit());
        if ((offen & bx) && !(b & bx)) { a[0] = z; offen &= (BYTE)~bx; }
        if ((offen & by) && !(b & by)) { a[1] = z; offen &= (BYTE)~by; }
    } while (offen && z < ZEITMAX);
    _enable();
    *t = (BYTE)((~inp(0x201) >> (nr ? 6 : 4)) & 3);
    if (offen || !a[0] || !a[1]) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }
    return 0;
}
static void mitte(WORD nr, const WORD FAR *a)
{
    WORD k;
    for (k = 0; k < 2; k++) { mi[nr][k] = a[k]; mn[nr][k] = (WORD)(a[k] / 8); mx[nr][k] = (WORD)(a[k] * 2 - a[k] / 8); }
    kal[nr] = 1;
}
static short wert(WORD nr, WORD k, WORD v)
{
    long r; WORD m = mi[nr][k];
    if (v < mn[nr][k]) mn[nr][k] = v;
    if (v > mx[nr][k]) mx[nr][k] = v;
    if (v < m) r = -100L * (long)(m - v) / (long)(m - mn[nr][k] ? m - mn[nr][k] : 1);
    else       r =  100L * (long)(v - m) / (long)(mx[nr][k] - m ? mx[nr][k] - m : 1);
    if (r > -4 && r < 4) r = 0;                   /* tote Zone um die Mitte */
    if (r > 100) r = 100; if (r < -100) r = -100;
    return (short)r;
}
WORD RDAPI JoyRead(WORD nr, JOYSTATE FAR *js)
{
    WORD a[2], e; BYTE t;
    if (nr > 1) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }
    if ((e = messen(nr, a, &t)) != 0) return e;
    if (!kal[nr]) mitte(nr, a);
    if (js) { js->x = wert(nr, 0, a[0]); js->y = wert(nr, 1, a[1]); js->tasten = t; }
    return 0;
}
WORD RDAPI JoyCalibrate(WORD nr)
{
    WORD a[2], e; BYTE t;
    if (nr > 1) { SetLastError(RD_ERR_NOTSUP); return RD_ERR_NOTSUP; }
    if ((e = messen(nr, a, &t)) != 0) return e;
    mitte(nr, a);
    return 0;
}
