/* RD32C.C - C-Laufzeit fuer RD2/32-Programme (RD32C.LIB, PLAN_32BIT, 21.11.2026).
 * string, ctype, stdlib (Speicher aus rd32_alloc), stdio (Dateien ueber die RD2-API, Textmodus mit CR/LF,
 * stdout/stderr zeilenweise ins RD2-Protokoll), printf-Familie mit Gleitkomma, sscanf, time.
 * Gleitkomma rechnet Open Watcom (-fpc, clib3r/math3r), mit 387 in Hardware, sonst in Software. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2_32.H"
#include "rd32c/stdio.h"
#include "rd32c/stdlib.h"
#include "rd32c/string.h"
#include "rd32c/ctype.h"
#include "rd32c/time.h"
#include "rd32c/setjmp.h"

extern jmp_buf rd32_jb; extern int rd32_ecode; extern void (*rd32_beiende)(void);

/* ---- string --------------------------------------------------------------------------------------------- */
void *memcpy(void *z, const void *q, size_t n) { char *d = z; const char *s = q; while (n--) *d++ = *s++; return z; }
void *memmove(void *z, const void *q, size_t n)
{
    char *d = z; const char *s = q;
    if (d < s) while (n--) *d++ = *s++; else { d += n; s += n; while (n--) *--d = *--s; }
    return z;
}
void *memset(void *z, int c, size_t n) { char *d = z; while (n--) *d++ = (char)c; return z; }
int memcmp(const void *a, const void *b, size_t n) { const BYTE *p = a, *q = b; for (; n; n--, p++, q++) if (*p != *q) return *p - *q; return 0; }
void *memchr(const void *p, int c, size_t n) { const BYTE *s = p; for (; n; n--, s++) if (*s == (BYTE)c) return (void *)s; return 0; }
size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }
char *strcpy(char *z, const char *q) { char *d = z; while ((*d++ = *q++) != 0) ; return z; }
char *strncpy(char *z, const char *q, size_t n) { char *d = z; for (; n && *q; n--) *d++ = *q++; for (; n; n--) *d++ = 0; return z; }
char *strcat(char *z, const char *q) { strcpy(z + strlen(z), q); return z; }
char *strncat(char *z, const char *q, size_t n) { char *d = z + strlen(z); for (; n && *q; n--) *d++ = *q++; *d = 0; return z; }
int strcmp(const char *a, const char *b) { for (; *a && *a == *b; a++, b++) ; return (BYTE)*a - (BYTE)*b; }
int strncmp(const char *a, const char *b, size_t n) { for (; n && *a && *a == *b; n--, a++, b++) ; return n ? (BYTE)*a - (BYTE)*b : 0; }
int stricmp(const char *a, const char *b) { for (; *a && tolower(*a) == tolower(*b); a++, b++) ; return tolower((BYTE)*a) - tolower((BYTE)*b); }
int strnicmp(const char *a, const char *b, size_t n) { for (; n && *a && tolower(*a) == tolower(*b); n--, a++, b++) ; return n ? tolower((BYTE)*a) - tolower((BYTE)*b) : 0; }
char *strchr(const char *s, int c) { for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return 0; } }
char *strrchr(const char *s, int c) { const char *r = 0; for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char *)r; } }
char *strstr(const char *s, const char *t) { size_t n = strlen(t); for (; *s; s++) if (!strncmp(s, t, n)) return (char *)s; return n ? 0 : (char *)s; }
char *strpbrk(const char *s, const char *t) { for (; *s; s++) if (strchr(t, *s)) return (char *)s; return 0; }
size_t strspn(const char *s, const char *t) { size_t n = 0; while (s[n] && strchr(t, s[n])) n++; return n; }
size_t strcspn(const char *s, const char *t) { size_t n = 0; while (s[n] && !strchr(t, s[n])) n++; return n; }
char *strtok(char *s, const char *t)
{
    static char *w; char *a;
    if (s) w = s;
    if (!w) return 0;
    w += strspn(w, t);
    if (!*w) { w = 0; return 0; }
    a = w; w += strcspn(w, t);
    if (*w) *w++ = 0; else w = 0;
    return a;
}
char *strdup(const char *s) { char *d = malloc(strlen(s) + 1); return d ? strcpy(d, s) : 0; }
char *strupr(char *s) { char *p; for (p = s; *p; p++) *p = (char)toupper(*p); return s; }
char *strlwr(char *s) { char *p; for (p = s; *p; p++) *p = (char)tolower(*p); return s; }
char *strerror(int e) { return e ? "Fehler" : "kein Fehler"; }

/* ---- ctype (ASCII) -------------------------------------------------------------------------------------- */
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isalpha(int c) { return isupper(c) || islower(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int isprint(int c) { return c >= 32 && c < 127; }
int isgraph(int c) { return c > 32 && c < 127; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int toupper(int c) { return islower(c) ? c - 32 : c; }
int tolower(int c) { return isupper(c) ? c + 32 : c; }

/* ---- Speicher: Bereiche aus rd32_alloc, Bloecke mit Kopf (Groesse in DWORD, immer gerade, | 1 = belegt), Freies beim Suchen zusammenlegen -- */
#define BEREICHE 12
static struct { DWORD *a; DWORD n; } ber[BEREICHE];   /* n in DWORD, letzter Kopf 0 = Ende */
static DWORD nber, naechst = 65536UL;
static void *suche(DWORD w)                      /* w = Woerter (DWORD) mit Kopf */
{
    DWORD i, *p, *q;
    for (i = 0; i < nber; i++)
        for (p = ber[i].a; *p; p += *p & ~1UL) {
            if (*p & 1) continue;
            for (q = p + *p; *q && !(*q & 1); q = p + *p) *p += *q;   /* folgende freie Bloecke anhaengen */
            if (*p >= w) {
                if (*p - w >= 4) { p[w] = *p - w; *p = w; }
                *p |= 1;
                return p + 1;
            }
        }
    return 0;
}
void *malloc(size_t n)
{
    DWORD w = ((n + 3) / 4 + 2) & ~1UL, g; void *r;
    if ((r = suche(w)) != 0) return r;
    if (nber == BEREICHE) return 0;
    g = naechst; while (g < (w + 2) * 4) g *= 2;
    if (!(ber[nber].a = rd32_alloc(g))) { g = (w + 2) * 4; if (!(ber[nber].a = rd32_alloc(g))) return 0; }
    ber[nber].n = g / 4; ber[nber].a[0] = g / 4 - 2; ber[nber].a[g / 4 - 2] = 0; nber++;
    if (naechst < 1048576UL) naechst *= 2;
    return suche(w);
}
void free(void *p) { if (p) ((DWORD *)p)[-1] &= ~1UL; }
void *calloc(size_t n, size_t g) { void *p = malloc(n * g); if (p) memset(p, 0, n * g); return p; }
void *realloc(void *p, size_t n)
{
    DWORD *k, w = ((n + 3) / 4 + 2) & ~1UL, alt; void *q;
    if (!p) return malloc(n);
    if (!n) { free(p); return 0; }
    k = (DWORD *)p - 1; alt = *k & ~1UL;
    while (k[alt] && !(k[alt] & 1)) { alt += k[alt]; *k = alt | 1; }   /* freie Nachbarn dazunehmen */
    if (alt >= w) { if (alt - w >= 4) { k[w] = alt - w; *k = w | 1; } return p; }
    if (!(q = malloc(n))) return 0;
    memcpy(q, p, (alt - 1) * 4); free(p);
    return q;
}

/* ---- stdlib --------------------------------------------------------------------------------------------- */
static int ziffer(int c) { return isdigit(c) ? c - '0' : isalpha(c) ? tolower(c) - 'a' + 10 : 99; }
unsigned long strtoul(const char *s, char **e, int b)
{
    unsigned long v = 0; const char *a, *s0 = s; int d, neg = 0;
    while (isspace(*s)) s++;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    if ((!b || b == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && isxdigit(s[2])) { s += 2; b = 16; }
    else if (!b) b = *s == '0' ? 8 : 10;
    for (a = s; (d = ziffer(*s)) < b; s++) v = v * b + d;
    if (e) *e = (char *)(s == a ? s0 : s);
    return neg ? (unsigned long)(-(long)v) : v;
}
long strtol(const char *s, char **e, int b) { return (long)strtoul(s, e, b); }
int atoi(const char *s) { return (int)strtol(s, 0, 10); }
long atol(const char *s) { return strtol(s, 0, 10); }
double strtod(const char *s, char **e)
{
    double v = 0, f = 1; int neg = 0, ex = 0, en = 0, n = 0; const char *a = s;
    while (isspace(*s)) s++;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    for (; isdigit(*s); s++, n++) v = v * 10 + (*s - '0');
    if (*s == '.') for (s++; isdigit(*s); s++, n++) { f /= 10; v += (*s - '0') * f; }
    if (n && (*s == 'e' || *s == 'E')) {
        const char *t = s + 1; int en2 = 0;
        if (*t == '-' || *t == '+') en2 = *t++ == '-';
        if (isdigit(*t)) { en = en2; for (s = t; isdigit(*s); s++) ex = ex * 10 + (*s - '0'); }
    }
    while (ex--) v = en ? v / 10 : v * 10;
    if (e) *e = (char *)(n ? s : a);
    return neg ? -v : v;
}
double atof(const char *s) { return strtod(s, 0); }
int abs(int v) { return v < 0 ? -v : v; }
long labs(long v) { return v < 0 ? -v : v; }
div_t div(int a, int b) { div_t r; r.quot = a / b; r.rem = a % b; return r; }
ldiv_t ldiv(long a, long b) { ldiv_t r; r.quot = a / b; r.rem = a % b; return r; }
static unsigned long saat = 1;
int rand(void) { saat = saat * 1103515245UL + 12345; return (int)((saat >> 16) & RAND_MAX); }
void srand(unsigned s) { saat = s; }
static void tausch(char *a, char *b, size_t g) { char t; while (g--) { t = *a; *a++ = *b; *b++ = t; } }
void qsort(void *b, size_t n, size_t g, int (*cmp)(const void *, const void *))
{
    char *a = b, *p; size_t i, l;
    while (n > 1) {                               /* Quicksort, kleinere Haelfte rekursiv */
        tausch(a, a + (n / 2) * g, g); p = a;
        for (i = 1, l = 0; i < n; i++) if (cmp(a + i * g, p) < 0) { l++; tausch(a + l * g, a + i * g, g); }
        tausch(a, a + l * g, g);
        if (l < n - l - 1) { qsort(a, l, g, cmp); a += (l + 1) * g; n -= l + 1; }
        else { qsort(a + (l + 1) * g, n - l - 1, g, cmp); n = l; }
    }
}
void *bsearch(const void *k, const void *b, size_t n, size_t g, int (*cmp)(const void *, const void *))
{
    size_t lo = 0, hi = n; int c;
    while (lo < hi) { size_t m = (lo + hi) / 2; const char *p = (const char *)b + m * g;
        if (!(c = cmp(k, p))) return (void *)p; if (c < 0) hi = m; else lo = m + 1; }
    return 0;
}
static void (*ae[8])(void); static int nae;
static void alle_zu(void);
int atexit(void (*f)(void)) { if (nae == 8) return -1; ae[nae++] = f; rd32_beiende = alle_zu; return 0; }
void exit(int code) { rd32_ecode = code; longjmp(rd32_jb, 1); }
void abort(void) { exit(3); }
char *getenv(const char *name) { (void)name; return 0; }
char *ultoa(unsigned long v, char *s, int b)
{
    char t[34]; int n = 0; char *d = s;
    do { t[n++] = "0123456789abcdefghijklmnopqrstuvwxyz"[v % b]; v /= b; } while (v);
    while (n) *d++ = t[--n];
    *d = 0; return s;
}
char *ltoa(long v, char *s, int b) { if (v < 0 && b == 10) { *s = '-'; ultoa((unsigned long)-v, s + 1, b); } else ultoa((unsigned long)v, s, b); return s; }
char *itoa(int v, char *s, int b) { return ltoa(v, s, b); }

/* ---- Formatieren -------------------------------------------------------------------------------------- */
typedef struct { char *p; size_t n, max; FILE *f; } AUS;
static void ac(AUS *o, char c) { if (o->f) fputc(c, o->f); else if (o->n + 1 < o->max) o->p[o->n] = c; o->n++; }
static void feld(AUS *o, const char *s, int len, int breite, int links, char fuell, const char *vor)
{
    int vn = (int)strlen(vor), pad = breite - len - vn;
    if (!links && fuell == ' ') while (pad-- > 0) ac(o, ' ');
    while (*vor) ac(o, *vor++);
    if (!links && fuell == '0') while (pad-- > 0) ac(o, '0');
    while (len--) ac(o, *s++);
    if (links) while (pad-- > 0) ac(o, ' ');
}
static int gleit(char *t, double v, int prec, char art, int alt)   /* ohne Vorzeichen; Laenge */
{
    int n = 0, e = 0, i, d; double r; unsigned __int64 ip; char z[24]; int nz;
    if (v != v) { strcpy(t, "nan"); return 3; }
    if (v > 1.7e308) { strcpy(t, "inf"); return 3; }
    if (art == 'g' || art == 'G') {
        int p = prec ? prec : 1; double w = v;
        if (w != 0) { while (w >= 10) { w /= 10; e++; } while (w < 1) { w *= 10; e--; } }
        r = 0.5; for (i = 1; i < p; i++) r /= 10;   /* Rundung kann den Exponenten erhoehen */
        if (w + r >= 10) e++;
        if (e < -4 || e >= p) { n = gleit(t, v, p - 1, art == 'g' ? 'e' : 'E', alt); }
        else n = gleit(t, v, p - 1 - e, 'f', alt);
        if (!alt && strchr(t, '.')) {            /* Nullen am Ende weg */
            char *x = strchr(t, 'e'); char ex[8] = ""; if (!x) x = strchr(t, 'E');
            if (x) { strcpy(ex, x); *x = 0; }
            n = (int)strlen(t); while (t[n - 1] == '0') n--; if (t[n - 1] == '.') n--;
            t[n] = 0; strcat(t, ex); n = (int)strlen(t);
        }
        return n;
    }
    if (art == 'e' || art == 'E') {
        if (v != 0) { while (v >= 10) { v /= 10; e++; } while (v < 1) { v *= 10; e--; } }
        r = 0.5; for (i = 0; i < prec; i++) r /= 10;
        v += r; if (v >= 10) { v /= 10; e++; }
        d = (int)v; t[n++] = (char)('0' + d); v -= d;
        if (prec || alt) t[n++] = '.';
        for (i = 0; i < prec; i++) { v *= 10; d = (int)v; if (d > 9) d = 9; t[n++] = (char)('0' + d); v -= d; }
        t[n++] = art; t[n++] = e < 0 ? '-' : '+'; if (e < 0) e = -e;
        if (e >= 100) t[n++] = (char)('0' + e / 100);
        t[n++] = (char)('0' + e / 10 % 10); t[n++] = (char)('0' + e % 10); t[n] = 0;
        return n;
    }
    r = 0.5; for (i = 0; i < prec; i++) r /= 10;
    v += r;
    if (v >= 1.8e19) return gleit(t, v, prec, 'e', alt);
    ip = (unsigned __int64)v; v -= (double)ip;
    nz = 0; do { z[nz++] = (char)('0' + (int)(ip % 10)); ip /= 10; } while (ip);
    while (nz) t[n++] = z[--nz];
    if (prec || alt) t[n++] = '.';
    for (i = 0; i < prec; i++) { v *= 10; d = (int)v; if (d > 9) d = 9; if (d < 0) d = 0; t[n++] = (char)('0' + d); v -= d; }
    t[n] = 0;
    return n;
}
static int formatiere(AUS *o, const char *f, va_list a)
{
    char t[352], vor[3]; const char *s; int links, plus, leer, alt, breite, prec, lang, n; char fuell, c;
    for (; *f; f++) {
        if (*f != '%') { ac(o, *f); continue; }
        links = plus = leer = alt = 0; fuell = ' '; breite = 0; prec = -1; lang = 0;
        for (;; f++) { c = f[1];
            if (c == '-') links = 1; else if (c == '+') plus = 1; else if (c == ' ') leer = 1; else if (c == '#') alt = 1;
            else if (c == '0') fuell = '0'; else break; }
        f++;
        if (*f == '*') { breite = va_arg(a, int); if (breite < 0) { links = 1; breite = -breite; } f++; }
        else while (isdigit(*f)) breite = breite * 10 + *f++ - '0';
        if (*f == '.') { f++; prec = 0; if (*f == '*') { prec = va_arg(a, int); f++; } else while (isdigit(*f)) prec = prec * 10 + *f++ - '0'; }
        while (*f == 'l' || *f == 'h' || *f == 'L') { if (*f == 'l') lang++; f++; }
        if (links) fuell = ' ';
        vor[0] = 0; c = *f;
        switch (c) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'p': {
            unsigned long v; int neg = 0, b = c == 'o' ? 8 : (c == 'x' || c == 'X' || c == 'p') ? 16 : 10;
            if (c == 'p') { v = (unsigned long)va_arg(a, void *); if (prec < 0) prec = 8; }
            else if (c == 'd' || c == 'i') { long w = va_arg(a, long); if (w < 0) { neg = 1; v = (unsigned long)-w; } else v = (unsigned long)w; }
            else v = va_arg(a, unsigned long);
            ultoa(v, t, b); if (c == 'X') strupr(t);
            n = (int)strlen(t);
            if (prec >= 0) { fuell = ' '; if (prec == 0 && !v) n = 0; while (n < prec) { memmove(t + 1, t, (size_t)n + 1); t[0] = '0'; n++; } }
            if (neg) strcpy(vor, "-"); else if (plus && (c == 'd' || c == 'i')) strcpy(vor, "+"); else if (leer && (c == 'd' || c == 'i')) strcpy(vor, " ");
            if (alt && v && (c == 'x' || c == 'X')) strcpy(vor, c == 'x' ? "0x" : "0X");
            if (alt && c == 'o' && t[0] != '0') { memmove(t + 1, t, (size_t)n + 1); t[0] = '0'; n++; }
            feld(o, t, n, breite, links, fuell, vor); break; }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
            double v = va_arg(a, double);
            if (v < 0) { strcpy(vor, "-"); v = -v; } else if (plus) strcpy(vor, "+"); else if (leer) strcpy(vor, " ");
            n = gleit(t, v, prec < 0 ? 6 : prec > 40 ? 40 : prec, c == 'F' ? 'f' : c, alt);
            feld(o, t, n, breite, links, fuell, vor); break; }
        case 'c': t[0] = (char)va_arg(a, int); feld(o, t, 1, breite, links, ' ', ""); break;
        case 's': s = va_arg(a, const char *); if (!s) s = "(null)";
            n = (int)strlen(s); if (prec >= 0 && n > prec) n = prec; feld(o, s, n, breite, links, ' ', ""); break;
        case 'n': *va_arg(a, int *) = (int)o->n; break;
        case 0: f--; break;
        default: ac(o, c);
        }
    }
    if (!o->f && o->max) o->p[o->n < o->max ? o->n : o->max - 1] = 0;
    return (int)o->n;
}
int vsnprintf(char *s, size_t n, const char *f, va_list a) { AUS o; o.p = s; o.n = 0; o.max = n; o.f = 0; return formatiere(&o, f, a); }
int vsprintf(char *s, const char *f, va_list a) { return vsnprintf(s, 0x7FFFFFFF, f, a); }
int snprintf(char *s, size_t n, const char *f, ...) { va_list a; int r; va_start(a, f); r = vsnprintf(s, n, f, a); va_end(a); return r; }
int sprintf(char *s, const char *f, ...) { va_list a; int r; va_start(a, f); r = vsnprintf(s, 0x7FFFFFFF, f, a); va_end(a); return r; }
int vfprintf(FILE *fp, const char *f, va_list a) { AUS o; o.p = 0; o.n = 0; o.max = 0; o.f = fp; return formatiere(&o, f, a); }
int fprintf(FILE *fp, const char *f, ...) { va_list a; int r; va_start(a, f); r = vfprintf(fp, f, a); va_end(a); return r; }
int vprintf(const char *f, va_list a) { return vfprintf(stdout, f, a); }
int printf(const char *f, ...) { va_list a; int r; va_start(a, f); r = vfprintf(stdout, f, a); va_end(a); return r; }

int sscanf(const char *s, const char *f, ...)
{
    va_list a; int n = 0, lang, breite; char *e; const char *anf = s;
    va_start(a, f);
    for (; *f; f++) {
        if (isspace(*f)) { while (isspace(*s)) s++; continue; }
        if (*f != '%') { if (*s != *f) break; s++; continue; }
        f++; breite = 0; lang = 0;
        while (isdigit(*f)) breite = breite * 10 + *f++ - '0';
        while (*f == 'l' || *f == 'h') { if (*f == 'l') lang = 1; f++; }
        if (*f == '%') { if (*s != '%') break; s++; continue; }
        if (*f == 'n') { *va_arg(a, int *) = (int)(s - anf); continue; }
        if (*f != 'c') while (isspace(*s)) s++;
        if (!*s) break;
        if (*f == 'd' || *f == 'i' || *f == 'u' || *f == 'x' || *f == 'o') {
            long v = strtol(s, &e, *f == 'x' ? 16 : *f == 'o' ? 8 : *f == 'i' ? 0 : 10);
            if (e == s) break;
            s = e; *va_arg(a, long *) = v; n++;
        } else if (*f == 'f' || *f == 'e' || *f == 'g') {
            double v = strtod(s, &e); if (e == s) break;
            s = e; if (lang) *va_arg(a, double *) = v; else *va_arg(a, float *) = (float)v; n++;
        } else if (*f == 's') {
            char *d = va_arg(a, char *); int k = 0;
            while (*s && !isspace(*s) && (!breite || k < breite)) { *d++ = *s++; k++; }
            *d = 0; n++;
        } else if (*f == 'c') {
            char *d = va_arg(a, char *); int k = breite ? breite : 1;
            while (k-- && *s) *d++ = *s++;
            n++;
        } else break;
    }
    va_end(a);
    return n ? n : (*s ? 0 : EOF);
}

/* ---- stdio: Dateien ueber die RD2-API (Puffer 512 Bytes), Textmodus: CR weg beim Lesen, LF -> CR LF beim Schreiben -- */
struct _rd32_file { short h; BYTE offen, text, schreib, eof, fehler, belegt; int zurueck; WORD n, pos; long fpos; BYTE puf[512]; };
static struct _rd32_file dat[FOPEN_MAX + 3];      /* im BSS: kostet in der Datei nichts; 0-2 beim ersten Gebrauch */
static void std_an(void)
{
    int i;
    if (dat[0].offen) return;
    for (i = 0; i < 3; i++) { dat[i].h = (short)(i ? -2 : -3); dat[i].offen = 1; dat[i].text = dat[i].schreib = (BYTE)(i != 0); dat[i].zurueck = EOF; }
}
FILE *const stdin = &dat[0], *const stdout = &dat[1], *const stderr = &dat[2];
static int leeren(FILE *f)
{
    if (f->h == -2) { if (f->n) { f->puf[f->n] = 0; DbgPrint((LPCSTR)f->puf); f->n = 0; } return 0; }
    if (f->h < 0 || !f->schreib || !f->n) return 0;
    if (FileWrite(f->h, f->puf, f->n) != f->n) { f->fehler = 1; return EOF; }
    f->fpos += f->n; f->n = 0;
    return 0;
}
static void lesemodus(FILE *f) { if (f->schreib) { leeren(f); f->schreib = 0; } }
FILE *fopen(const char *name, const char *mode)
{
    int i, plus = strchr(mode, '+') != 0; FILE *f = 0;
    std_an();
    for (i = 3; i < FOPEN_MAX + 3; i++) if (!dat[i].offen) { f = &dat[i]; break; }
    if (!f) return 0;
    memset(f, 0, sizeof *f); f->zurueck = EOF;
    if (mode[0] == 'r') f->h = FileOpen(name, plus ? FO_RW : FO_READ);
    else if (mode[0] == 'w') f->h = FileCreate(name, 0);
    else if (mode[0] == 'a') { f->h = FileOpen(name, FO_RW); if (f->h < 0) f->h = FileCreate(name, 0); if (f->h >= 0) f->fpos = FileSeek(f->h, 0, FS_END); }
    else return 0;
    if (f->h < 0) return 0;
    f->offen = 1; f->text = strchr(mode, 'b') == 0;
    rd32_beiende = alle_zu;
    return f;
}
int fflush(FILE *f)
{
    int i, r = 0;
    std_an();
    if (!f) { for (i = 1; i < FOPEN_MAX + 3; i++) if (dat[i].offen && leeren(&dat[i])) r = EOF; return r; }
    return leeren(f);
}
int fclose(FILE *f)
{
    int r;
    if (!f || !f->offen || f->h < 0) return EOF;
    r = leeren(f); FileClose(f->h); f->offen = 0;
    return r;
}
static void alle_zu(void) { int i; while (nae) ae[--nae](); for (i = 1; i < FOPEN_MAX + 3; i++) if (dat[i].offen) { if (i < 3) leeren(&dat[i]); else fclose(&dat[i]); } }
static int roh_lies(FILE *f)
{
    if (f->pos >= f->n) {
        if (f->h < 0 || f->eof) { f->eof = 1; return EOF; }
        f->fpos += f->n; f->pos = 0;
        f->n = FileRead(f->h, f->puf, sizeof f->puf);
        if (!f->n || f->n == 0xFFFF) { f->n = 0; f->eof = 1; return EOF; }
    }
    return f->puf[f->pos++];
}
int fgetc(FILE *f)
{
    int c;
    std_an();
    if (!f || !f->offen || f->h == -3) return EOF;
    if (f->zurueck != EOF) { c = f->zurueck; f->zurueck = EOF; return c; }
    lesemodus(f);
    do c = roh_lies(f); while (f->text && c == '\r');
    if (f->text && c == 26) { f->eof = 1; return EOF; }
    return c;
}
int ungetc(int c, FILE *f) { if (!f || c == EOF) return EOF; f->zurueck = c; f->eof = 0; return c; }
static int roh_schreib(FILE *f, int c)
{
    rd32_beiende = alle_zu;
    if (!f->schreib) { if (f->h >= 0) { if (f->n > f->pos) FileSeek(f->h, f->fpos + f->pos, FS_SET); f->fpos += f->pos; } f->n = f->pos = 0; f->schreib = 1; }
    f->puf[f->n++] = (BYTE)c;
    if (f->n >= (f->h == -2 ? 120 : sizeof f->puf)) return leeren(f);
    return 0;
}
int fputc(int c, FILE *f)
{
    std_an();
    if (!f || !f->offen || f->h == -3) return EOF;
    if (f->h == -2) { if (c == '\n') { f->schreib = 1; leeren(f); return c; } if (c == '\r') return c; }
    if (f->text && c == '\n' && roh_schreib(f, '\r')) return EOF;
    return roh_schreib(f, c) ? EOF : (BYTE)c;
}
int getchar(void) { return EOF; }
int putchar(int c) { return fputc(c, stdout); }
char *fgets(char *s, int n, FILE *f)
{
    int i = 0, c = 0;
    while (i < n - 1 && (c = fgetc(f)) != EOF) { s[i++] = (char)c; if (c == '\n') break; }
    s[i] = 0;
    return i ? s : 0;
}
int fputs(const char *s, FILE *f) { if (!f) return 0; while (*s) if (fputc(*s++, f) == EOF) return EOF; return 0; }
int puts(const char *s) { fputs(s, stdout); return fputc('\n', stdout); }
size_t fread(void *p, size_t g, size_t n, FILE *f)
{
    BYTE *d = p; size_t i, k; int c;
    if (!g || !n || !f) return 0;
    if (!f->text && f->zurueck == EOF && f->h >= 0) {   /* binaer: Puffer, dann gross direkt */
        size_t alle = g * n, r = 0; WORD w;
        lesemodus(f);
        while (r < alle && f->pos < f->n) d[r++] = f->puf[f->pos++];
        while (r < alle && !f->eof) {
            if (alle - r >= sizeof f->puf) {
                f->fpos += f->n; f->n = f->pos = 0;
                w = FileRead(f->h, d + r, (WORD)(alle - r > 32768U ? 32768U : alle - r));
                if (!w || w == 0xFFFF) { f->eof = 1; break; }
                f->fpos += w; r += w;
            } else if ((c = roh_lies(f)) == EOF) break; else d[r++] = (BYTE)c;
        }
        return r / g;
    }
    for (i = 0; i < n; i++) for (k = 0; k < g; k++) { if ((c = fgetc(f)) == EOF) return i; *d++ = (BYTE)c; }
    return n;
}
size_t fwrite(const void *p, size_t g, size_t n, FILE *f)
{
    const BYTE *q = p; size_t i, k;
    if (!g || !n || !f) return 0;
    if (!f->text && f->h >= 0 && g * n >= sizeof f->puf) {   /* binaer und gross: direkt */
        size_t alle = g * n, r = 0; WORD w;
        if (!f->schreib) roh_schreib(f, 0), f->n = 0;
        leeren(f);
        while (r < alle) { w = (WORD)(alle - r > 32768U ? 32768U : alle - r);
            if (FileWrite(f->h, q + r, w) != w) { f->fehler = 1; break; } r += w; f->fpos += w; }
        return r / g;
    }
    for (i = 0; i < n; i++) for (k = 0; k < g; k++) if (fputc(*q++, f) == EOF) return i;
    return n;
}
int fseek(FILE *f, long pos, int art)
{
    long z;
    if (!f || f->h < 0) return -1;
    if (art == SEEK_CUR) { pos += ftell(f); art = SEEK_SET; }
    leeren(f); f->schreib = 0; f->n = f->pos = 0; f->eof = 0; f->zurueck = EOF;
    if ((z = FileSeek(f->h, pos, (WORD)art)) < 0) return -1;
    f->fpos = z;
    return 0;
}
long ftell(FILE *f) { if (!f || f->h < 0) return -1; return f->fpos + (f->schreib ? f->n : f->pos) - (f->zurueck != EOF); }
void rewind(FILE *f) { fseek(f, 0, SEEK_SET); if (f) f->fehler = 0; }
int feof(FILE *f) { return f && f->eof && f->zurueck == EOF; }
int ferror(FILE *f) { return f && f->fehler; }
void clearerr(FILE *f) { if (f) f->eof = f->fehler = 0; }
int remove(const char *name) { return FileDelete(name) ? -1 : 0; }
int rename(const char *alt, const char *neu) { return FileRename(alt, neu) ? -1 : 0; }

/* ---- time ---------------------------------------------------------------------------------------------- */
static const short mt[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static long tage(int j, int m, int t) { long d = (j - 1970) * 365L + (j - 1969) / 4 + mt[m] + t - 1; if (m > 1 && !(j % 4)) d++; return d; }
clock_t clock(void) { return TickCount(); }
time_t time(time_t *t)
{
    RDTIME r; time_t v;
    TimeGet(&r);
    v = ((tage(r.jahr, r.monat - 1, r.tag) * 24 + r.std) * 60 + r.min) * 60 + r.sek;
    if (t) *t = v;
    return v;
}
double difftime(time_t a, time_t b) { return (double)(a - b); }
time_t mktime(struct tm *t) { return ((tage(t->tm_year + 1900, t->tm_mon, t->tm_mday) * 24 + t->tm_hour) * 60 + t->tm_min) * 60 + t->tm_sec; }
struct tm *localtime(const time_t *t)
{
    static struct tm r; long d = *t / 86400, s = *t % 86400; int j = 1970, m;
    r.tm_sec = (int)(s % 60); r.tm_min = (int)(s / 60 % 60); r.tm_hour = (int)(s / 3600); r.tm_wday = (int)((d + 4) % 7);
    while (d >= (j % 4 ? 365 : 366)) { d -= j % 4 ? 365 : 366; j++; }
    r.tm_yday = (int)d; r.tm_year = j - 1900;
    for (m = 11; m > 0 && d < mt[m] + (m > 1 && !(j % 4)); m--) ;
    r.tm_mon = m; r.tm_mday = (int)(d - mt[m] - (m > 1 && !(j % 4))) + 1; r.tm_isdst = 0;
    return &r;
}

/* ---- Ersatz fuer Teile von clib3r, die Open Watcoms Mathebibliothek bei Fehlern ruft (sonst zoege sie dessen stdio und Start mit) -- */
struct _exception;
int matherr(struct _exception *e) { (void)e; return 1; }
#pragma aux rd32_gss "__get_std_stream_";
void *rd32_gss(unsigned n) { (void)n; return 0; }
