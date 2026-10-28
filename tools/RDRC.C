/* RDRC.EXE - Ressourcen-Compiler und Linker fuer RD2-Programme (DOS, 8086, Open Watcom -ms).
   Gleiche Sprache und gleiche Ausgabe wie tools/rdrc.py + tools/rd2link.py (SPEC.md Abschnitte 2 und 6).

   RDRC QUELLE.RC PROG.EXE "Name" [/T:P|M|D] [/S:Stapel] [/K:KB] [/V:Version] [/A:API] [/F:Flags]
   RDRC - PROG.EXE "Name" ...      nur RD2-Block, keine Ressourcen

   Haengt an die MZ-EXE den 48-Byte-RD2-Block, das Verzeichnis, die Ressourcendaten und den
   16-Byte-Abschluss "RD2!". Ein vorhandener Anhang wird ersetzt. Zahlen dezimal oder 0x.. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include <malloc.h>
#include "rdrcdef.h"            /* aus sdk/RD2.H erzeugt: struct { char *n; unsigned long v; } stile[] */

#define MAXRES   128
#define MAXTOK   48
#define PUF      32000u
#define MAXTX    600

typedef unsigned short W;
typedef unsigned long  D;

static const char *quelle;
static int zeile_nr;
static FILE *rc;
static char zeile[600];
static char tokbuf[1200];
static char *tok[MAXTOK];
static char tokart[MAXTOK];     /* 'w' Wort, 's' Text */
static int ntok;

static unsigned char puf[PUF];  /* Daten der Ressource, die gerade entsteht */
static unsigned plen;

static struct { W typ, nr, sp; D lang; } verz[MAXRES];
static int nres;
static FILE *tmp;               /* Daten aller Ressourcen hintereinander */
static D tmplang;

static void fehler(const char *t, const char *x)
{
    if (rc) printf("%s(%d): ", quelle, zeile_nr);
    printf(t, x ? x : "");
    printf("\n");
    if (tmp) { fclose(tmp); remove("RDRC.$$$"); }
    exit(1);
}

/* Zeile zerlegen wie tokens() in rdrc.py */
static void zerlege(void)
{
    char *z = zeile, *o = tokbuf;
    ntok = 0;
    while (*z) {
        if (*z == ' ' || *z == '\t' || *z == '\r' || *z == '\n') { z++; continue; }
        if (*z == ';') break;
        if (ntok >= MAXTOK) fehler("zu viele Angaben in einer Zeile", 0);
        if (*z == '"') {
            tok[ntok] = o; tokart[ntok++] = 's'; z++;
            while (*z && *z != '"' && *z != '\n' && *z != '\r') {
                if (*z == '\\' && z[1] && z[1] != '\n') {
                    z++;
                    *o++ = *z == 't' ? '\t' : *z == 'n' ? '\n' : *z;
                } else *o++ = *z;
                z++;
            }
            if (*z != '"') fehler("Anfuehrungszeichen fehlt", 0);
            *o++ = 0; z++;
            continue;
        }
        tok[ntok] = o; tokart[ntok++] = 'w';
        while (*z && !strchr(" \t\r\n;\"", *z)) *o++ = *z++;
        *o++ = 0;
    }
}

/* naechste nicht leere Zeile; 0 am Dateiende */
static int lies(void)
{
    while (fgets(zeile, sizeof zeile, rc)) {
        zeile_nr++;
        zerlege();
        if (ntok) return 1;
    }
    return 0;
}

static void weiter(void) { if (!lies()) fehler("ENDE fehlt", 0); }

static int gleich(int i, const char *w)
{
    return i < ntok && tokart[i] == 'w' && !stricmp(tok[i], w);
}

static void mind(int n) { if (ntok < n) fehler("zu wenige Angaben", 0); }

/* Zahl: dezimal, 0x.., mit Vorzeichen; mit stilnamen auch WS_*, ES_*, STANDARD, AN, GRAU */
static int istzahl(const char *s, D *v)
{
    const char *p = s; int neg = 0; D x = 0;
    if (*p == '-' || *p == '+') neg = *p++ == '-';
    if (!*p) return 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2; if (!*p) return 0;
        for (; *p; p++) {
            if (!isxdigit((unsigned char)*p)) return 0;
            x = x * 16 + (isdigit((unsigned char)*p) ? *p - '0' : (toupper(*p) - 'A' + 10));
        }
    } else {
        if (p[0] == '0' && p[1]) { for (; *p == '0'; p++); if (*p) return 0; }   /* wie Python: 010 ist ungueltig */
        for (; *p; p++) { if (!isdigit((unsigned char)*p)) return 0; x = x * 10 + (*p - '0'); }
    }
    *v = neg ? (D)(-(long)x) : x;
    return 1;
}

static D zahl_s(const char *s, int stilnamen)
{
    D v; int i;
    if (istzahl(s, &v)) return v;
    if (stilnamen)
        for (i = 0; stile[i].n; i++)
            if (!stricmp(stile[i].n, s)) return stile[i].v;
    fehler("Zahl erwartet statt %s", s);
    return 0;
}

static D zahl(int i)
{
    if (i >= ntok) fehler("zu wenige Angaben", 0);
    if (tokart[i] != 'w') { static char m[80]; sprintf(m, "\"%.60s\"", tok[i]); fehler("Zahl erwartet statt %s", m); }
    return zahl_s(tok[i], 0);
}

static D stil(int ab)
{
    D v = 0; int i; char w[64]; char *p, *q;
    for (i = ab; i < ntok; i++) {
        strncpy(w, tok[i], 63); w[63] = 0;
        for (p = w; ; p = q + 1) {
            q = strchr(p, '|');
            if (q) *q = 0;
            v |= zahl_s(p, 1);
            if (!q) break;
        }
    }
    return v;
}

static void raus(const void *d, unsigned n)
{
    if (plen + n > PUF) fehler("Ressource zu gross (32 KB)", 0);
    memcpy(puf + plen, d, n); plen += n;
}
static void rausw(W w) { raus(&w, 2); }
static void rausd(D d) { raus(&d, 4); }
static void text(int i)
{
    if (i >= ntok) fehler("zu wenige Angaben", 0);
    if (tokart[i] != 's') fehler("Text in Anfuehrungszeichen erwartet statt %s", tok[i]);
    raus(tok[i], strlen(tok[i]) + 1);
}

static void fertig(W typ, W nr, W sp)
{
    int i;
    for (i = 0; i < nres; i++)
        if (verz[i].typ == typ && verz[i].nr == nr && verz[i].sp == sp) {
            static char m[40]; sprintf(m, "Typ %u Nr. %u", typ, nr);
            fehler("Ressource %s doppelt", m);
        }
    if (nres >= MAXRES) fehler("zu viele Ressourcen", 0);
    verz[nres].typ = typ; verz[nres].nr = nr; verz[nres].sp = sp; verz[nres].lang = plen;
    nres++;
    if (plen && fwrite(puf, 1, plen, tmp) != plen) fehler("Schreibfehler RDRC.$$$", 0);
    tmplang += plen;
    plen = 0;
}

static const char *klassen[] = { "BUTTON", "CHECK", "RADIO", "STATIC", "EDIT", "TEXT", "LIST", "COMBO",
                                 "SCROLL", "PROGRESS", "GROUP", 0 };
static int ohne_text(const char *k)
{
    return !stricmp(k, "EDIT") || !stricmp(k, "TEXT") || !stricmp(k, "LIST") || !stricmp(k, "COMBO") ||
           !stricmp(k, "SCROLL") || !stricmp(k, "PROGRESS");
}

static void menu(W nr, W sp)
{
    unsigned letzt[16];         /* je Ebene: Stelle des letzten Eintrags (fuer MF_END), 0xFFFF = noch leer */
    int eb = 0;
    letzt[0] = 0xFFFF;
    for (;;) {
        weiter();
        if (gleich(0, "ENDE")) {
            if (letzt[eb] == 0xFFFF) fehler(eb ? "leeres POPUP" : "leeres MENU", 0);
            *(W *)(puf + letzt[eb]) |= 4;                               /* MF_END */
            if (eb == 0) break;
            eb--;
        } else if (gleich(0, "POPUP")) {
            mind(2);
            if (eb >= 15) fehler("POPUP zu tief verschachtelt", 0);
            letzt[eb] = plen; rausw(1); rausw(0); text(1);
            letzt[++eb] = 0xFFFF;
        } else if (gleich(0, "ITEM")) {
            W f = 0; int i;
            mind(3);
            for (i = 3; i < ntok; i++) {
                if (gleich(i, "GRAU")) f |= 8;
                else if (gleich(i, "HAKEN")) f |= 16;
                else fehler("unbekannt: %s", tok[i]);
            }
            letzt[eb] = plen; rausw(f); rausw((W)zahl(2)); text(1);
        } else if (gleich(0, "TRENNER")) {
            letzt[eb] = plen; rausw(2); rausw(0); raus("", 1);
        } else fehler("POPUP, ITEM, TRENNER oder ENDE erwartet statt %s", tok[0]);
    }
    fertig(1, nr, sp);
}

static void dialog(W nr, W sp)
{
    unsigned anz_pos; W anz = 0; int i, k;
    mind(7);
    rausd(stil(7) | WS_DIALOG_V | WS_TITLE_V);
    for (i = 3; i < 7; i++) rausw((W)zahl(i));
    anz_pos = plen; rausw(0);
    text(2);
    for (;;) {
        weiter();
        if (gleich(0, "ENDE")) break;
        for (k = 0; klassen[k] && stricmp(klassen[k], tok[0]); k++);
        if (!klassen[k] || tokart[0] != 'w') fehler("unbekannte Klasse %s", tok[0]);
        raus(klassen[k], strlen(klassen[k]) + 1);
        i = 1;
        if (ntok > 1 && tokart[1] == 's') { text(1); i = 2; }
        else if (ohne_text(klassen[k])) raus("", 1);
        else fehler("%s braucht einen Text", klassen[k]);
        if (ntok < i + 5) fehler("Element: id x y w h", 0);
        rausw((W)zahl(i));
        rausw((W)zahl(i + 1)); rausw((W)zahl(i + 2)); rausw((W)zahl(i + 3)); rausw((W)zahl(i + 4));
        rausd(stil(i + 5) | WS_CHILD_V | WS_VISIBLE_V);
        anz++;
    }
    *(W *)(puf + anz_pos) = anz;
    fertig(2, nr, sp);
}

static struct { W id; unsigned pos, len; } tx[MAXTX];
static unsigned char __far *txpuf;          /* _fmalloc ausserhalb DGROUP */

static void texte(W sp)
{
    int n = 0, i, j; unsigned tl = 0; D kopf;
    if (!txpuf && (txpuf = (unsigned char __far *)_fmalloc(PUF)) == 0) fehler("zu wenig Speicher", 0);
    for (;;) {
        weiter();
        if (gleich(0, "ENDE")) break;
        mind(2);
        tx[n].id = (W)zahl(0);
        for (i = 0; i < n; i++) if (tx[i].id == tx[n].id) fehler("Text %s doppelt", tok[0]);
        if (tokart[1] != 's') fehler("Text in Anfuehrungszeichen erwartet statt %s", tok[1]);
        tx[n].len = strlen(tok[1]) + 1;
        if (tl + tx[n].len > PUF) fehler("TEXTE zu gross (32 KB)", 0);
        tx[n].pos = tl; _fmemcpy(txpuf + tl, tok[1], tx[n].len); tl += tx[n].len;
        if (++n >= MAXTX) fehler("zu viele Texte", 0);
    }
    for (i = 1; i < n; i++)                                 /* nach Nummer sortieren */
        for (j = i; j > 0 && tx[j - 1].id > tx[j].id; j--) {
            W a = tx[j].id; unsigned b = tx[j].pos, c = tx[j].len;
            tx[j].id = tx[j - 1].id; tx[j].pos = tx[j - 1].pos; tx[j].len = tx[j - 1].len;
            tx[j - 1].id = a; tx[j - 1].pos = b; tx[j - 1].len = c;
        }
    kopf = 2 + 4L * n;
    if (kopf + tl > 0xFFF0UL || kopf + tl > PUF) fehler("TEXTE zu gross", 0);
    rausw((W)n);
    for (i = 0, j = 0; i < n; i++) { rausw(tx[i].id); rausw((W)(kopf + j)); j += tx[i].len; }
    for (i = 0; i < n; i++) { unsigned char __far *q = txpuf + tx[i].pos; unsigned m = tx[i].len; while (m--) { unsigned char c = *q++; raus(&c, 1); } }
    fertig(3, 1, sp);
}

static void accel(W nr, W sp)
{
    for (;;) {
        weiter();
        if (gleich(0, "ENDE")) break;
        mind(3);
        rausw((W)zahl(0)); rausw((W)zahl(1)); rausw((W)zahl(2));
    }
    fertig(9, nr, sp);
}

static void datei(W typ, W nr, W sp, const char *rcpfad)
{
    char p[128]; const char *e; FILE *f; unsigned n;
    mind(3);
    e = strrchr(rcpfad, '\\'); if (!e) e = strrchr(rcpfad, '/'); if (!e) e = strrchr(rcpfad, ':');
    n = e ? (unsigned)(e - rcpfad + 1) : 0;
    if (n + strlen(tok[2]) >= sizeof p) fehler("Pfad zu lang", 0);
    memcpy(p, rcpfad, n); strcpy(p + n, tok[2]);
    f = fopen(p, "rb");
    if (!f) fehler("Datei fehlt: %s", tok[2]);
    plen = fread(puf, 1, PUF, f);
    if (fgetc(f) != EOF) { fclose(f); fehler("Datei zu gross (32 KB): %s", tok[2]); }
    fclose(f);
    fertig(typ, nr, sp);
}

static void uebersetze(const char *pfad)
{
    W sp = 0;
    quelle = pfad;
    rc = fopen(pfad, "r");
    if (!rc) { printf("RDRC: %s nicht gefunden\n", pfad); exit(1); }
    while (lies()) {
        W nr;
        if (gleich(0, "SPRACHE")) {
            if (gleich(1, "DE")) sp = 0x0407;
            else if (gleich(1, "EN")) sp = 0x0409;
            else if (gleich(1, "ALLE")) sp = 0;
            else fehler("SPRACHE DE, EN oder ALLE", 0);
        } else if (gleich(0, "TEXTE")) texte(sp);
        else {
            static const struct { char *w; W t; } dt[] = { {"SYMBOL",4}, {"ICON",4}, {"BITMAP",5}, {"CURSOR",6},
                {"SCHRIFT",7}, {"FONT",7}, {"DATEN",8}, {"DATA",8}, {"STRING",3}, {0,0} };
            int k;
            if (tokart[0] != 'w') fehler("unbekannt: %s", tok[0]);
            nr = (W)zahl(1);
            if (gleich(0, "MENU")) menu(nr, sp);
            else if (gleich(0, "DIALOG")) dialog(nr, sp);
            else if (gleich(0, "ACCEL")) accel(nr, sp);
            else {
                for (k = 0; dt[k].w && !gleich(0, dt[k].w); k++);
                if (!dt[k].w) fehler("unbekannt: %s", tok[0]);
                datei(dt[k].t, nr, sp, pfad);
            }
        }
    }
    fclose(rc); rc = 0;
}

static D opt(const char *s)
{
    D v;
    if (!istzahl(s, &v)) { printf("RDRC: Zahl erwartet statt %s\n", s); exit(1); }
    return v;
}

static void hilfe(void)
{
    printf("RDRC - Ressourcen-Compiler fuer RetroDesk 2\n\n"
           "RDRC QUELLE.RC PROG.EXE \"Name\" [/T:P|M|D] [/S:Stapel] [/K:KB] [/V:Version] [/A:API] [/F:Flags]\n"
           "RDRC - PROG.EXE \"Name\" ...   (ohne Ressourcen)\n\n"
           "Uebersetzt QUELLE.RC und haengt RD2-Block und Ressourcen an PROG.EXE an.\n"
           "Vorgaben: /T:P /S:4096 /K:0 /V:0x0100 /A:0x0100 /F:0. Name hoechstens 15 Zeichen.\n");
    exit(1);
}

int main(int argc, char **argv)
{
    char typ = 'P'; D api = 0x100, flags = 0, stack = 4096, kb = 0, ver = 0x100;
    unsigned char kopf[28], blk[48], ab[16], kp[4096];
    char *name = 0; int i, fd; long dl, img, ende;
    W summe = 0; D pos, off;

    if (argc < 4) hilfe();
    name = argv[3];
    for (i = 4; i < argc; i++) {
        char *a = argv[i];
        if ((a[0] != '/' && a[0] != '-') || !a[1] || a[2] != ':') hilfe();
        switch (toupper(a[1])) {
        case 'T': typ = (char)toupper(a[3]); if (!strchr("PMD", typ) || !typ || a[4]) hilfe(); break;
        case 'S': stack = opt(a + 3); break;
        case 'K': kb = opt(a + 3); break;
        case 'V': ver = opt(a + 3); break;
        case 'A': api = opt(a + 3); break;
        case 'F': flags = opt(a + 3); break;
        default: hilfe();
        }
    }
    tmp = fopen("RDRC.$$$", "w+b");
    if (!tmp) { printf("RDRC: RDRC.$$$ laesst sich nicht anlegen\n"); return 1; }
    if (strcmp(argv[1], "-")) uebersetze(argv[1]);

    fd = open(argv[2], O_RDWR | O_BINARY);
    if (fd < 0) fehler("%s nicht gefunden", argv[2]);
    dl = filelength(fd);
    if (read(fd, kopf, 28) != 28 || kopf[0] != 'M' || kopf[1] != 'Z') { close(fd); fehler("%s: keine MZ-EXE", argv[2]); }
    ende = dl;
    if (dl >= 16) {                                         /* alten Anhang entfernen */
        lseek(fd, dl - 16, SEEK_SET); read(fd, ab, 16);
        if (!memcmp(ab, "RD2!", 4)) ende = *(D *)(ab + 4);
    }
    img = (long)*(W *)(kopf + 4) * 512 - (*(W *)(kopf + 2) ? 512 - *(W *)(kopf + 2) : 0);
    if (ende < img) { close(fd); fehler("%s: Datei kuerzer als im MZ-Kopf angegeben", argv[2]); }
    if (chsize(fd, img)) { close(fd); fehler("%s: laesst sich nicht kuerzen", argv[2]); }

    memset(blk, 0, 48);
    blk[0] = 'R'; blk[1] = 'D'; blk[2] = '2'; blk[3] = typ;
    *(W *)(blk + 4) = (W)api; *(W *)(blk + 6) = (W)flags; *(W *)(blk + 8) = (W)stack; *(W *)(blk + 10) = (W)kb;
    for (i = 0; i < 15 && name[i]; i++) blk[12 + i] = name[i];
    *(W *)(blk + 28) = (W)ver; *(W *)(blk + 30) = 0; *(W *)(blk + 32) = (W)nres;
    *(W *)(blk + 34) = 48; *(W *)(blk + 36) = 48;
    for (i = 0; i < 48; i++) summe += blk[i];

    lseek(fd, img, SEEK_SET);
    write(fd, blk, 48);
    pos = 48 + 16L * nres; off = 0;
    for (i = 0; i < nres; i++) {                            /* Verzeichnis, Offsets ab Blockanfang */
        unsigned char e[16];
        *(W *)(e + 0) = verz[i].typ; *(W *)(e + 2) = verz[i].nr; *(W *)(e + 4) = verz[i].sp; *(W *)(e + 6) = 2;
        *(D *)(e + 8) = pos + off; *(D *)(e + 12) = verz[i].lang;
        write(fd, e, 16);
        off += (verz[i].lang + 1) & ~1UL;
    }
    rewind(tmp);
    for (i = 0; i < nres; i++) {                            /* Daten, je auf gerade Laenge aufgefuellt */
        D rest = verz[i].lang;
        while (rest) {
            unsigned n = rest > sizeof kp ? sizeof kp : (unsigned)rest;
            if (fread(kp, 1, n, tmp) != n) fehler("Lesefehler RDRC.$$$", 0);
            write(fd, kp, n); rest -= n;
        }
        if (verz[i].lang & 1) write(fd, "", 1);
    }
    memcpy(ab, "RD2!", 4);
    *(D *)(ab + 4) = img; *(D *)(ab + 8) = pos + off; *(W *)(ab + 12) = summe; *(W *)(ab + 14) = 0;
    if (write(fd, ab, 16) != 16) { close(fd); fehler("%s: Schreibfehler", argv[2]); }
    dl = filelength(fd);
    close(fd);
    fclose(tmp); tmp = 0; remove("RDRC.$$$");
    printf("%s: %d Ressourcen, RD2-Anhang bei %ld, Datei %ld Bytes\n", argv[2], nres, img, dl);
    return 0;
}
