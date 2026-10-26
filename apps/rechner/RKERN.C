/* RKERN.C - Rechenkern des Rechners. Wie RD1 calc.c (gleiche Tasten, gleiche Anzeige "%.10g"), aber mit
 * Dezimalziffern statt 64-Bit-Zahlen: Wert = 0.d[0]d[1]..d[ND-1] * 10^e, d[0] != 0 (ausser 0).
 * Kleiner auf 8086 (keine 64-Bit-Hilfsroutinen), genau im Dezimalsystem (0,1 + 0,2 = 0,3). */
#include <string.h>
#include "RKERN.H"

#define ND 20
typedef unsigned char U8;
typedef struct { U8 d[ND]; short e; U8 neg; } DEC;

static void norm(DEC *a, U8 *w, short n, short e) /* w[0..n-1] Ziffern (Komma vor w[0]) -> a, runden */
{
    short i, k = 0;
    while (k < n && !w[k]) k++;
    if (k == n) { memset(a, 0, sizeof *a); return; }
    e -= k;
    for (i = 0; i < ND; i++) a->d[i] = (U8)(k + i < n ? w[k + i] : 0);
    if (k + ND < n && w[k + ND] >= 5) {            /* aufrunden */
        for (i = ND - 1; i >= 0 && ++a->d[i] == 10; i--) a->d[i] = 0;
        if (i < 0) { a->d[0] = 1; e++; }
    }
    a->e = e;
}
static short cmpm(const U8 *a, const U8 *b, short n) { short i; for (i = 0; i < n; i++) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1; return 0; }
static DEC add(DEC a, DEC b)
{
    U8 x[ND + 2], y[ND + 2]; short i, s, c; DEC t;
    if (!b.d[0]) return a;
    if (!a.d[0]) return b;
    if (a.e < b.e) { t = a; a = b; b = t; }
    s = a.e - b.e;
    memset(x, 0, sizeof x); memset(y, 0, sizeof y);
    for (i = 0; i < ND; i++) { x[i + 1] = a.d[i]; if (i + 1 + s < ND + 2) y[i + 1 + s] = b.d[i]; }
    if (a.neg == b.neg) {
        for (i = ND + 1, c = 0; i >= 0; i--) { c += x[i] + y[i]; x[i] = (U8)(c % 10); c /= 10; }
    } else {
        if (cmpm(x, y, ND + 2) < 0) { U8 z[ND + 2]; memcpy(z, x, sizeof z); memcpy(x, y, sizeof z); memcpy(y, z, sizeof z); a.neg = b.neg; }
        for (i = ND + 1, c = 0; i >= 0; i--) { c = x[i] - y[i] - c; x[i] = (U8)(c < 0 ? c + 10 : c); c = c < 0; }
    }
    t.neg = a.neg; norm(&t, x, ND + 2, a.e + 1);
    if (!t.d[0]) t.neg = 0;
    return t;
}
static DEC mul(DEC a, DEC b)
{
    unsigned short p[2 * ND]; U8 w[2 * ND]; short i, j; unsigned short c; DEC t;
    memset(p, 0, sizeof p);
    for (i = 0; i < ND; i++) if (a.d[i]) for (j = 0; j < ND; j++) p[i + j + 1] += a.d[i] * b.d[j];
    for (i = 2 * ND - 1, c = 0; i >= 0; i--) { c += p[i]; w[i] = (U8)(c % 10); c /= 10; }
    t.neg = a.neg != b.neg; norm(&t, w, 2 * ND, a.e + b.e);
    if (!t.d[0]) t.neg = 0;
    return t;
}
static DEC dvd(DEC a, DEC b)                      /* b != 0: schriftlich teilen */
{
    U8 r[ND + 1], q[ND + 2]; short i, k, c; DEC t;
    if (!a.d[0]) return a;
    r[0] = 0; memcpy(r + 1, a.d, ND);
    for (k = 0; k < ND + 2; k++) {
        q[k] = 0;
        for (;;) {                                /* r >= b (b um eine Stelle versetzt: b0 = 0, b1.. = b.d) */
            U8 bb[ND + 1]; bb[0] = 0; memcpy(bb + 1, b.d, ND);
            if (cmpm(r, bb, ND + 1) < 0) break;
            for (i = ND, c = 0; i >= 0; i--) { c = r[i] - bb[i] - c; r[i] = (U8)(c < 0 ? c + 10 : c); c = c < 0; }
            q[k]++;
        }
        for (i = 0; i < ND; i++) r[i] = r[i + 1];   /* naechste Stelle */
        r[ND] = 0;
    }
    t.neg = a.neg != b.neg; norm(&t, q, ND + 2, a.e - b.e + 1);
    return t;
}
static DEC parse(const char *s)
{
    U8 w[ND]; short n = 0, e = 0, frac = 0, x = 0, xn = 0; DEC v; U8 neg = 0;
    if (*s == '-') { neg = 1; s++; }
    for (; (*s >= '0' && *s <= '9') || (*s == '.' && !frac); s++) {
        if (*s == '.') { frac = 1; continue; }
        if (n < ND) { w[n++] = (U8)(*s - '0'); if (!frac) e++; } else if (!frac) e++;
    }
    if (*s == 'e') {                              /* nur fuer eigene Ausgaben 1.2e+15 */
        s++;
        if (*s == '-') { xn = 1; s++; } else if (*s == '+') s++;
        while (*s >= '0' && *s <= '9') x = (short)(x * 10 + (*s++ - '0'));
        e = (short)(e + (xn ? -x : x));
    }
    norm(&v, w, n, e); v.neg = (U8)(v.d[0] ? neg : 0);
    return v;
}
/* wie printf("%.10g"); 0 = zu gross */
static int format(char *out, DEC v)
{
    char dg[10]; short e, i, n = 10, p = 0; DEC r;
    if (!v.d[0]) { strcpy(out, "0"); return 1; }
    memcpy(r.d, v.d, ND); r.e = v.e;              /* auf 10 Stellen runden */
    if (r.d[10] >= 5) { for (i = 9; i >= 0 && ++r.d[i] == 10; i--) r.d[i] = 0; if (i < 0) { r.d[0] = 1; r.e++; } }
    for (i = 0; i < 10; i++) dg[i] = (char)('0' + r.d[i]);
    e = (short)(r.e - 1);                         /* Exponent der ersten Ziffer */
    if (e > 99) return 0;
    if (e < -99) { strcpy(out, "0"); return 1; }
    if (v.neg) out[p++] = '-';
    if (e < -4 || e >= 10) {                      /* wissenschaftlich */
        while (n > 1 && dg[n - 1] == '0') n--;
        out[p++] = dg[0];
        if (n > 1) { out[p++] = '.'; for (i = 1; i < n; i++) out[p++] = dg[i]; }
        out[p++] = 'e'; out[p++] = e < 0 ? '-' : '+';
        if (e < 0) e = -e;
        out[p++] = (char)('0' + e / 10 % 10); out[p++] = (char)('0' + e % 10);
        out[p] = 0;
        return 1;
    }
    if (e < 0) {
        out[p++] = '0'; out[p++] = '.';
        for (i = -1; i > e; i--) out[p++] = '0';
        for (i = 0; i < n; i++) out[p++] = dg[i];
    } else {
        for (i = 0; i <= e; i++) out[p++] = dg[i];
        out[p++] = '.';
        for (; i < n; i++) out[p++] = dg[i];
    }
    while (out[p - 1] == '0') p--;                /* Nullen und Punkt am Ende weg */
    if (out[p - 1] == '.') p--;
    out[p] = 0;
    return 1;
}
static DEC root(DEC v)                            /* Newton-Verfahren */
{
    DEC x, nx, half; short i;
    if (!v.d[0]) return v;
    memset(&half, 0, sizeof half); half.d[0] = 5;
    memset(&x, 0, sizeof x); x.d[0] = 1; x.e = (short)(v.e / 2 + 1);
    for (i = 0; i < 80; i++) {
        nx = mul(add(x, dvd(v, x)), half);
        if (!memcmp(&nx, &x, sizeof x)) break;
        x = nx;
    }
    return x;
}

char calc_disp[24];
char calc_op;

static DEC acc;
static int fresh;     /* naechste Ziffer beginnt neue Eingabe */
static int pend;      /* Anzeige enthaelt noch nicht verrechneten Wert */
static int err;

static DEC cur(void) { return parse(calc_disp); }

static void fail(void) { err = 1; strcpy(calc_disp, "Fehler"); }

static void show(DEC v)
{
    if (!format(calc_disp, v)) fail();
}

static void apply(DEC v)
{
    switch (calc_op) {
    case '+': acc = add(acc, v); break;
    case '-': v.neg = (U8)(!v.neg && v.d[0]); acc = add(acc, v); break;
    case '*': acc = mul(acc, v); break;
    case '/':
        if (!v.d[0]) { fail(); return; }
        acc = dvd(acc, v); break;
    default: acc = v;
    }
}
void calc_reset(void)
{
    memset(&acc, 0, sizeof acc); calc_op = 0; err = 0;
    strcpy(calc_disp, "0");
    fresh = 1; pend = 1;
}

void calc_key(char k)
{
    int n = (int)strlen(calc_disp);
    DEC v;

    if (err && k != 'C') {
        calc_reset();
        if (!((k >= '0' && k <= '9') || k == '.')) return;
    }
    if ((k >= '0' && k <= '9') || k == '.') {
        if (fresh) { strcpy(calc_disp, "0"); fresh = 0; n = 1; }
        pend = 1;
        if (k == '.') { if (!strchr(calc_disp, '.') && n < 14) strcat(calc_disp, "."); return; }
        if (!strcmp(calc_disp, "0")) { calc_disp[0] = k; return; }
        if (!strcmp(calc_disp, "-0")) { calc_disp[1] = k; return; }
        if (n < 14) { calc_disp[n] = k; calc_disp[n + 1] = 0; }
        return;
    }
    switch (k) {
    case '+': case '-': case '*': case '/':
        if (pend) { if (calc_op) apply(cur()); else acc = cur(); }
        if (err) return;
        calc_op = k; pend = 0; fresh = 1;
        show(acc);
        break;
    case '=':
        if (calc_op && pend) apply(cur());
        if (err) return;
        if (calc_op) show(acc);
        calc_op = 0; pend = 1; fresh = 1;
        break;
    case 'C':
        calc_reset();
        break;
    case 'S':
        v = cur();
        if (v.neg) { fail(); return; }
        show(root(v)); pend = 1; fresh = 1;
        break;
    case '%':
        v = cur();
        v.e -= 2; if (!v.d[0]) v.e = 0;
        show(calc_op ? mul(acc, v) : v); pend = 1; fresh = 1;
        break;
    case 'N':
        if (!strcmp(calc_disp, "0")) break;
        if (calc_disp[0] == '-') memmove(calc_disp, calc_disp + 1, n);
        else if (n < 15) { memmove(calc_disp + 1, calc_disp, n + 1); calc_disp[0] = '-'; }
        pend = 1;
        break;
    case '\b':
        if (fresh) break;
        if (n > 1 && !(n == 2 && calc_disp[0] == '-')) calc_disp[n - 1] = 0;
        else strcpy(calc_disp, "0");
        break;
    }
}
