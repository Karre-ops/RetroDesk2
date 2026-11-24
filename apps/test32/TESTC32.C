/* TESTC32.C - Pruefung der C-Laufzeit fuer RD2/32 (RD32C.LIB): string, ctype, stdlib, malloc, printf/sprintf mit
 * Gleitkomma, sscanf, Dateien (Text und binaer), Mathe, setjmp, time, exit. Befehlszeile S = Gleitkomma in Software
 * erzwingen (wie auf einem 386 ohne 387). Ergebnis im RD2-Protokoll, Ende mit exit(7). */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <math.h>
#include "RD2_32.H"

extern char _8087, _real87;              /* Open Watcom: 0 = Gleitkomma in Software */
static int n, fehl; static char b[256]; static jmp_buf jb;
static void ist(int ok, const char *was) { n++; if (!ok) { fehl++; printf("TESTC32 FEHLER: %s\n", was); } }
static void gleich(const char *soll, const char *was) { n++; if (strcmp(b, soll)) { fehl++; printf("TESTC32 FEHLER: %s: \"%s\" statt \"%s\"\n", was, b, soll); } }
static int nahe(double a, double s) { double d = a - s; if (d < 0) d = -d; return d < 1e-9 * (s < 0 ? -s : s) + 1e-12; }
static int cmp(const void *a, const void *c) { return *(const int *)a - *(const int *)c; }
static void tief(int k) { if (k == 0) longjmp(jb, 42); tief(k - 1); }

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    int i, a[200], x; long l; double d; char s[40], *p, *q; FILE *f; void *m[300];
    (void)inst; (void)show;
    if (cmd[0] == 'S') { _8087 = 0; _real87 = 0; }
    printf("TESTC32: Start, Gleitkomma %s\n", _real87 ? "387" : "Software");
    /* string, ctype */
    strcpy(b, "Hallo"); strcat(b, ", Welt"); gleich("Hallo, Welt", "strcpy/strcat");
    ist(strlen(b) == 11 && !strncmp(b, "Hallo", 5) && strcmp("a", "b") < 0 && !stricmp("ABC", "abc"), "strlen/strcmp");
    ist(strchr(b, 'W') == b + 7 && strrchr(b, 'l') == b + 9 && strstr(b, "Welt") == b + 7 && !strstr(b, "xyz"), "strchr/strstr");
    memset(s, 'x', 10); s[10] = 0; memmove(s + 2, s, 5); ist(!memcmp(s, "xxxxxxxxxx", 10), "memset/memmove");
    strcpy(b, "eins,zwei,,drei"); p = strtok(b, ","); q = strtok(0, ","); ist(p && q && !strcmp(p, "eins") && !strcmp(q, "zwei") && !strcmp(strtok(0, ","), "drei") && !strtok(0, ","), "strtok");
    ist(isdigit('7') && !isdigit('a') && isalpha('Q') && isspace('\t') && toupper('q') == 'Q' && tolower('Z') == 'z' && isxdigit('F'), "ctype");
    DbgPrint("TESTC32: Zahlen");
    /* Zahlen */
    ist(atoi("-123") == -123 && strtol("ff", 0, 16) == 255 && strtol("0x1F", 0, 0) == 31 && strtoul("777", &p, 8) == 511 && !*p, "strtol");
    ist(nahe(atof("3.25e2"), 325.0) && nahe(strtod("-0.5", 0), -0.5), "strtod");
    for (i = 0; i < 200; i++) a[i] = (i * 7919) % 200;
    qsort(a, 200, sizeof a[0], cmp);
    for (i = 0, x = 1; i < 200; i++) if (a[i] != i) x = 0;
    ist(x, "qsort"); x = 50; ist(bsearch(&x, a, 200, sizeof a[0], cmp) == &a[50], "bsearch");
    srand(1); x = rand(); srand(1); ist(rand() == x && x >= 0 && x <= RAND_MAX, "rand");
    DbgPrint("TESTC32: sprintf");
    /* sprintf */
    sprintf(b, "%d|%5d|%-5d|%05d|%+d|%x|%X|%#x|%o|%u", 42, 42, 42, 42, 42, 255, 255, 255, 8, 3000000000U); gleich("42|   42|42   |00042|+42|ff|FF|0xff|10|3000000000", "sprintf ganz");
    sprintf(b, "%s|%10s|%-6s|%.3s|%c|%%|%ld", "abc", "rechts", "li", "abcdef", 'Z', -7L); gleich("abc|    rechts|li    |abc|Z|%|-7", "sprintf text");
    sprintf(b, "%.2f|%8.3f|%e|%g|%g|%g|%.0f", 3.14159, -2.5, 12345.678, 0.0001, 1234567.0, 100.0, 2.5); gleich("3.14|  -2.500|1.234568e+04|0.0001|1.23457e+06|100|3", "sprintf gleit");
    ist(snprintf(s, 6, "%s", "abcdefgh") == 8 && !strcmp(s, "abcde"), "snprintf");
    DbgPrint("TESTC32: sscanf");
    /* sscanf */
    ist(sscanf("12 ab 0x10 2.5", "%d %s %i %lf", &x, s, &i, &d) == 4 && x == 12 && !strcmp(s, "ab") && i == 16 && nahe(d, 2.5), "sscanf");
    DbgPrint("TESTC32: Mathe");
    /* Mathe */
    d = sqrt(2.0); ist(nahe(d * d, 2.0), "sqrt");
    ist(nahe(sin(3.14159265358979323846 / 6), 0.5) && nahe(cos(0.0), 1.0) && nahe(atan2(1.0, 1.0) * 4, 3.14159265358979323846), "sin/cos/atan2");
    ist(nahe(exp(log(10.0)), 10.0) && nahe(pow(2.0, 10.0), 1024.0) && floor(-2.5) == -3.0 && ceil(2.1) == 3.0 && nahe(fmod(7.5, 2.0), 1.5), "exp/log/pow/floor");
    sprintf(b, "%.6f", sqrt(1e6) + sin(0.0)); gleich("1000.000000", "Gleitkomma ueber printf");
    DbgPrint("TESTC32: Speicher");
    /* Speicher */
    for (i = 0; i < 300; i++) { m[i] = malloc((size_t)(i * 37 % 1000) + 1); if (m[i]) memset(m[i], i & 255, (size_t)(i * 37 % 1000) + 1); }
    for (i = 0, x = 1; i < 300; i++) if (!m[i] || ((BYTE *)m[i])[0] != (BYTE)(i & 255)) x = 0;
    for (i = 0; i < 300; i += 2) free(m[i]);
    for (i = 1; i < 300; i += 2) { m[i] = realloc(m[i], 3000); if (!m[i] || ((BYTE *)m[i])[0] != (BYTE)(i & 255)) x = 0; }
    p = malloc(2000000UL); if (p) { p[0] = 1; p[1999999UL] = 2; } ist(x && p && p[1999999UL] == 2, "malloc/realloc/free, 2 MB");
    for (i = 1; i < 300; i += 2) free(m[i]);
    free(p); p = calloc(100, 4); ist(p && !p[399], "calloc"); free(p);
    ist(!malloc(0xFFFFFFF0UL) && !calloc(0x10000UL, 0x10001UL) && !calloc(0x40000000UL, 4), "malloc/calloc Ueberlauf: 0");   /* Audit 24.11. */
    { short h[2] = { 0, 0x1234 }; long v = 0; char t[400];
      ist(sscanf("-5 77", "%hd %ld", &h[0], &v) == 2 && h[0] == -5 && h[1] == 0x1234 && v == 77, "sscanf %hd schreibt 2 Bytes");
      ist(snprintf(t, sizeof t, "%.1000d", 7) == 300 && t[0] == '0' && strlen(t) < sizeof t, "printf %.1000d ohne Ueberlauf");
      ist(!strcmp(ultoa(255UL, t, 99), "255"), "ultoa falsche Basis"); ist(strtod("1e999999999", 0) > 1e300, "strtod grosser Exponent"); }
    DbgPrint("TESTC32: Dateien");
    /* Dateien */
    f = fopen("TESTC32.TXT", "w"); ist(f != 0, "fopen w");
    if (f) { fprintf(f, "Zeile %d\n", 1); fputs("Zweite Zeile\n", f); for (i = 0; i < 2000; i++) fputc('0' + i % 10, f); fputc('\n', f); fclose(f); }
    f = fopen("TESTC32.TXT", "rb"); if (f) { fseek(f, 0, SEEK_END); l = ftell(f); fclose(f); } else l = 0;
    ist(l == 9 + 14 + 2002, "Textmodus CR/LF, ftell");
    f = fopen("TESTC32.TXT", "r");
    if (f) { fgets(b, sizeof b, f); gleich("Zeile 1\n", "fgets"); fgets(b, sizeof b, f); x = fgetc(f); ungetc(x, f); ist(fgetc(f) == '0', "ungetc");
             fseek(f, -3, SEEK_END); x = fgetc(f); ist(x == '9', "fseek"); fclose(f); }
    f = fopen("TESTC32.BIN", "wb"); p = malloc(100000UL);
    if (f && p) { for (l = 0; l < 100000L; l++) p[l] = (char)(l * 13); ist(fwrite(p, 1, 100000UL, f) == 100000UL, "fwrite 100 KB"); fclose(f); }
    f = fopen("TESTC32.BIN", "rb");
    if (f && p) { memset(p, 0, 100000UL); ist(fread(p, 1, 100000UL, f) == 100000UL, "fread 100 KB"); for (l = 0, x = 1; l < 100000L; l++) if (p[l] != (char)(l * 13)) x = 0; ist(x && fgetc(f) == EOF && feof(f), "Inhalt, feof"); fclose(f); }
    free(p);
    f = fopen("TESTC32.TXT", "a"); if (f) { fputs("Anhang\n", f); fclose(f); }
    f = fopen("TESTC32.TXT", "r"); x = 0; if (f) { while ((i = fgetc(f)) != EOF) if (i == '\n') x++; fclose(f); } ist(x == 4, "fopen a");
    ist(!remove("TESTC32.TXT") && !rename("TESTC32.BIN", "TESTC32.ALT") && !remove("TESTC32.ALT") && !fopen("TESTC32.ALT", "r"), "remove/rename");
    DbgPrint("TESTC32: setjmp, time");
    /* setjmp, time */
    if ((x = setjmp(jb)) == 0) tief(20);
    ist(x == 42, "setjmp/longjmp");
    { time_t t = time(0); struct tm *z = localtime(&t); ist(t > 1700000000L && z->tm_year >= 124 && mktime(z) == t, "time/localtime/mktime"); }
    ist(clock() > 0, "clock");
    printf("TESTC32: %d Pruefungen, Fehler %d\n", n, fehl);
    printf("TESTC32: Teil 1"); printf(" ohne Zeilenende");   /* stdout ohne \\n: wird beim Ende geleert */
    exit(7);
    return 0;
}
