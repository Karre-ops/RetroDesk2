#include <string.h>
#include "calc.h"

/* Dezimal-Arithmetik ohne Gleitkomma: Wert = m * 10^e, m < 10^18 (18 Stellen).
 * Spart rund 18 KB in RDESK, weil 8087-Emulator und printf-Gleitkomma entfallen. */
typedef unsigned long long U64;
typedef struct { U64 m; int e; char neg; } DEC;

#define E17 100000000000000000ULL
#define E18 1000000000000000000ULL

static void canon(DEC *a)
{
    while (a->m >= E18) { U64 r = a->m % 10; a->m /= 10; a->e++; if (r >= 5) a->m++; }
    if (!a->m) { a->e = 0; a->neg = 0; return; }
    while (a->m % 10 == 0) { a->m /= 10; a->e++; }
}

static DEC add(DEC a, DEC b)
{
    DEC t;
    if (!b.m) return a;
    if (!a.m) return b;
    if (a.e < b.e) { t = a; a = b; b = t; }
    while (a.e > b.e && a.m < E17) { a.m *= 10; a.e--; }
    while (a.e > b.e) { U64 r = b.m % 10; b.m /= 10; b.e++; if (r >= 5) b.m++; }
    if (a.neg == b.neg) a.m += b.m;
    else if (a.m >= b.m) a.m -= b.m;
    else { a.m = b.m - a.m; a.neg = b.neg; }
    canon(&a);
    return a;
}

#define LO(x) ((x) & 0xFFFFFFFFULL)
static DEC mul(DEC a, DEC b)                 /* 64x64 -> 128 Bit in 32-Bit-Teilen */
{
    U64 a0 = LO(a.m), a1 = a.m >> 32, b0 = LO(b.m), b1 = b.m >> 32;
    U64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    U64 mid = (p00 >> 32) + LO(p01) + LO(p10);
    U64 hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32), w[4];
    int i, r = 0;
    DEC c;
    w[0] = hi >> 32; w[1] = LO(hi); w[2] = LO(mid); w[3] = LO(p00);
    c.e = a.e + b.e; c.neg = a.neg != b.neg;
    while (w[0] || w[1] || (w[2] << 32 | w[3]) >= E18) {      /* 128 Bit / 10 */
        U64 rem = 0;
        for (i = 0; i < 4; i++) { U64 cur = rem << 32 | w[i]; w[i] = cur / 10; rem = cur % 10; }
        r = (int)rem; c.e++;
    }
    c.m = w[2] << 32 | w[3];
    if (r >= 5) c.m++;
    canon(&c);
    return c;
}

static DEC dvd(DEC a, DEC b)           /* b != 0 */
{
    U64 q, r;
    DEC c;
    c.neg = a.neg != b.neg;
    c.e = a.e - b.e;
    if (!a.m) return a;
    while (a.m < E17) { a.m *= 10; c.e--; }
    q = a.m / b.m; r = a.m % b.m;
    while (q < E17) { r *= 10; q = q * 10 + r / b.m; r %= b.m; c.e--; }
    if (r * 2 >= b.m) q++;
    c.m = q;
    canon(&c);
    return c;
}

static int digits(U64 m) { int n = 0; while (m) { m /= 10; n++; } return n; }

static DEC parse(const char *s)
{
    DEC v;
    int frac = 0;
    v.m = 0; v.e = 0; v.neg = 0;
    if (*s == '-') { v.neg = 1; s++; }
    for (; (*s >= '0' && *s <= '9') || (*s == '.' && !frac); s++) {
        if (*s == '.') { frac = 1; continue; }
        if (v.m < E17) { v.m = v.m * 10 + (*s - '0'); if (frac) v.e--; }
        else if (!frac) v.e++;
    }
    if (*s == 'e') {                         /* nur fuer eigene Ausgaben 1.2e+15 */
        int x = 0, xn = 0;
        s++;
        if (*s == '-') { xn = 1; s++; } else if (*s == '+') s++;
        while (*s >= '0' && *s <= '9') x = x * 10 + (*s++ - '0');
        v.e += xn ? -x : x;
    }
    canon(&v);
    return v;
}

/* wie printf("%.10g"); 0 = zu gross */
static int format(char *out, DEC v)
{
    char dg[12];
    int e, i, n, p = 0, nd;
    U64 m = v.m;
    if (!m) { strcpy(out, "0"); return 1; }
    nd = digits(m);
    e = v.e;
    while (nd > 10) { U64 r = m % 10; m /= 10; e++; nd--; if (r >= 5 && nd == 10) m++; }
    if (m >= 10000000000ULL) { m /= 10; e++; }   /* Uebertrag beim Runden */
    nd = digits(m);
    e += nd - 1;                             /* Exponent der ersten Ziffer */
    if (e > 99) return 0;
    if (e < -99) { strcpy(out, "0"); return 1; }
    for (i = nd - 1; i >= 0; i--) { dg[i] = (char)('0' + (int)(m % 10)); m /= 10; }
    for (i = nd; i < 10; i++) dg[i] = '0';
    if (v.neg) out[p++] = '-';
    n = 10;
    if (e < -4 || e >= 10) {                 /* wissenschaftlich */
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
    while (out[p - 1] == '0') p--;           /* Nullen und Punkt am Ende weg */
    if (out[p - 1] == '.') p--;
    out[p] = 0;
    return 1;
}

static DEC root(DEC v)                       /* Newton-Verfahren */
{
    DEC x, nx, half;
    int i;
    if (!v.m) return v;
    half.m = 5; half.e = -1; half.neg = 0;
    x.m = 1; x.neg = 0; x.e = (digits(v.m) + v.e) / 2;
    for (i = 0; i < 80; i++) {
        nx = mul(add(x, dvd(v, x)), half);
        if (nx.m == x.m && nx.e == x.e) break;
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
    case '-': v.neg = !v.neg; if (!v.m) v.neg = 0; acc = add(acc, v); break;
    case '*': acc = mul(acc, v); break;
    case '/':
        if (!v.m) { fail(); return; }
        acc = dvd(acc, v); break;
    default: acc = v;
    }
}

void calc_reset(void)
{
    acc.m = 0; acc.e = 0; acc.neg = 0; calc_op = 0; err = 0;
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
        v.e -= 2;
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
