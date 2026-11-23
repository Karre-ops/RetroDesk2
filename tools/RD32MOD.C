/* RD32MOD.C - 32-Bit-Modul (.M32) unter DOS fertigstellen (DOS-Fassung von tools/rd32mod.py, byte-gleich, test/rd32mod.sh).
 * RD32MOD AUS.M32 BILD.0 BILD.1 KARTE.MAP EXPORTE.TXT
 * BILD.0 und BILD.1: dasselbe Modul zweimal mit wlink "format raw bin" gelinkt, Offset 0 und 1048576 (MKM32.BAT tut das);
 * KARTE.MAP: Map von BILD.0; EXPORTE.TXT: C-Namen der Exporte, je Zeile einer. DWORDs, die sich um genau 1 MB
 * unterscheiden, sind Verschiebungen. Format wie rd32mod.py: Bild | Anzahl Exporte, je (Offset, Laenge, Name) |
 * Anzahl Verschiebungen, je Offset. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
typedef unsigned long D;
#define DIST 0x100000UL
#define BLK 4096
static unsigned char p0[BLK + 4], p1[BLK + 4];
static FILE *f0, *f1, *aus;
static void fehler(const char *t, const char *a) { printf("RD32MOD: "); printf(t, a); printf("\n"); if (aus) fclose(aus); exit(1); }
static D le(const unsigned char *p) { return p[0] | (D)p[1] << 8 | (D)p[2] << 16 | (D)p[3] << 24; }
static void wd(D v) { unsigned char b[4]; b[0] = (unsigned char)v; b[1] = (unsigned char)(v >> 8); b[2] = (unsigned char)(v >> 16); b[3] = (unsigned char)(v >> 24); fwrite(b, 1, 4, aus); }
static long laenge(FILE *f) { long l; fseek(f, 0, SEEK_END); l = ftell(f); fseek(f, 0, SEEK_SET); return l; }
static D sym(const char *kart, const char *e)     /* Adresse von e (Watcom: e_, E, _e, e) aus der Map, sonst 0xFFFFFFFF */
{
    static char z[200]; char k[4][70]; FILE *m = fopen(kart, "r"); int i; D r = 0xFFFFFFFFUL; int best = 9;
    if (!m) fehler("%s fehlt", kart);
    if (strlen(e) > 60) fehler("Name zu lang: %s", e);
    sprintf(k[0], "%s_", e); strcpy(k[1], e); for (i = 0; k[1][i]; i++) k[1][i] = (char)toupper(k[1][i]);
    sprintf(k[2], "_%s", e); strcpy(k[3], e);
    while (fgets(z, sizeof z, m)) {
        char a[20], n[100]; int j, h = 1;
        if (sscanf(z, "%19s %99s", a, n) != 2) continue;
        for (j = 0; j < 8; j++) if (!isxdigit((unsigned char)a[j])) h = 0;
        if (!h || (a[8] && (a[8] != '*' || a[9]))) continue;
        for (i = 0; i < 4 && i < best; i++) if (!strcmp(n, k[i])) { best = i; r = strtoul(a, 0, 16); }
    }
    fclose(m);
    return r;
}
int main(int argc, char **argv)
{
    long l0, l1, i, base = -1, nrp; D nrel = 0, msize, nexp = 0, v0, v1; char z[100]; FILE *ex; size_t n;
    if (argc != 6) { printf("RD32MOD AUS.M32 BILD.0 BILD.1 KARTE.MAP EXPORTE.TXT\n32-Bit-Modul fuer RD2/32 fertigstellen (siehe MKM32.BAT).\n"); return 1; }
    if (!(f0 = fopen(argv[2], "rb"))) fehler("%s fehlt", argv[2]);
    if (!(f1 = fopen(argv[3], "rb"))) fehler("%s fehlt", argv[3]);
    l0 = laenge(f0); l1 = laenge(f1);
    if (fread(p0, 1, 16, f0) != 16 || memcmp(p0, "R32M", 4) || l0 != l1) fehler("Bilder ungleich lang oder Kopf R32M fehlt (RD32M0 zuerst)%s", "");
    msize = le(p0 + 12);
    if (!(aus = fopen(argv[1], "wb"))) fehler("%s laesst sich nicht anlegen", argv[1]);
    fseek(f0, 0, SEEK_SET);                       /* Bild kopieren, Groesse bei +8 eintragen */
    while ((n = fread(p0, 1, BLK, f0)) != 0) fwrite(p0, 1, n, aus);
    fseek(aus, 8, SEEK_SET); wd((D)l0); fseek(aus, 0, SEEK_END);
    if (!(ex = fopen(argv[5], "r"))) fehler("%s fehlt", argv[5]);   /* Exporte */
    wd(0);
    while (fgets(z, sizeof z, ex)) {
        char *e = z, *q; D a; unsigned char lb;
        while (*e == ' ' || *e == '\t') e++;
        for (q = e; *q && *q != '\r' && *q != '\n' && *q != ' ' && *q != '\t'; q++) ;
        *q = 0; if (!*e) continue;
        if ((a = sym(argv[4], e)) == 0xFFFFFFFFUL) fehler("Export %s nicht gefunden", e);
        wd(a); lb = (unsigned char)strlen(e); fwrite(&lb, 1, 1, aus); fwrite(e, 1, lb, aus); nexp++;
    }
    fclose(ex);
    nrp = ftell(aus); wd(0);                      /* Verschiebungen, Anzahl danach eintragen */
    for (i = 0; i + 4 <= l0; ) {
        if (base < 0 || i + 4 > base + BLK + 4 || i < base) {
            base = i; fseek(f0, i, SEEK_SET); fseek(f1, i, SEEK_SET);
            memset(p0, 0, sizeof p0); memset(p1, 0, sizeof p1);
            fread(p0, 1, BLK + 4, f0); fread(p1, 1, BLK + 4, f1);
        }
        v0 = le(p0 + (i - base)); v1 = le(p1 + (i - base));
        if (v1 - v0 == DIST) { wd((D)i); nrel++; i += 4; continue; }
        if (v0 != v1 && p0[i - base] != p1[i - base]) { sprintf(z, "%lx", i); fehler("unerwarteter Unterschied bei 0x%s", z); }
        i++;
    }
    fseek(aus, 16L + l0 - 16L, SEEK_SET);         /* l0: Anfang der Exporttabelle */
    wd(nexp); fseek(aus, nrp, SEEK_SET); wd(nrel);
    fclose(aus); fclose(f0); fclose(f1);
    printf("RD32MOD: %s Bild %ld Bytes, Speicher %lu, %lu Exporte, %lu Verschiebungen\n", argv[1], l0, msize, nexp, nrel);
    return 0;
}
