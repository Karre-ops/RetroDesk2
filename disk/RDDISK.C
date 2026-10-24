/* RDDISK.C - Laufwerks-API (verwerfbar, Diskette 2): Diskettenlaufwerke ueber das BIOS (INT 13h).
 * Benutzt von RDFORMAT.EXE (Formatieren, Bootfaehig machen). Laufwerk 0 = A:, 1 = B:.
 * Alle Sektorzugriffe laufen ueber einen eigenen 512-Byte-Puffer, der keine 64-KB-Grenze
 * kreuzt (DMA). Die Spurformatierung stellt auf dem AT das Medium ein (INT 13h 18h: 360K im
 * 1,2M-Laufwerk, 720K im 1,44M-Laufwerk) und setzt die Diskettentabelle (INT 1Eh) danach zurueck.
 * Exporte (ModProc-Nummer):
 *   1 Ende (Tabelle zurueck)
 *   2 WORD DiskTyp(WORD lw)                     BIOS-Laufwerkstyp 1=360K 2=1,2M 3=720K 4=1,44M 5/6=2,88M, 0 = unbekannt (XT)
 *   3 WORD DiskStart(WORD lw, const DGEO FAR *g) Medium einstellen, zuruecksetzen; 0 = ok
 *   4 WORD DiskSpur(WORD lw, WORD zyl, WORD kopf, WORD pruefen)  Spur formatieren (+ pruefen), BIOS-Status
 *   5 WORD DiskSektor(WORD lw, WORD schreiben, WORD lba, void FAR *puf)  ein Sektor, BIOS-Status
 *   6 void DiskEnde(WORD lw, WORD dos)           Tabelle zurueck, DOS-Puffer schreiben, DOS-Laufwerk dos (1 = A:)
 *                                               neu einlesen lassen (DPB-Zugriffsflag FFh, DOS 4+ und FreeDOS),
 *                                               damit DOS das neue Format (BPB) und die neue FAT sieht
 * BIOS-Status: 0 ok, 3 schreibgeschuetzt, 0x80 nicht bereit, sonst Fehler. */
#include <i86.h>
#include "RD2.H"
#include "RDDISK.H"

static BYTE dbuf[1024], *io;                      /* io: 512 Byte ohne 64-KB-Grenze */
static DGEO geo;
static void __far *alt1e; static WORD gesetzt;
static union REGS r; static struct SREGS sr;    /* statisch: Module laufen mit SS != DS (-zu), int86 will DS-Zeiger */

static void __far * __far *v1e(void) { return (void __far * __far *)MK_FP(0, 0x78); }
static void tabelle_zurueck(void) { if (gesetzt) { _disable(); *v1e() = alt1e; _enable(); gesetzt = 0; } }
static void puffer(void)
{
    DWORD ph;
    segread(&sr); ph = ((DWORD)sr.ds << 4) + (WORD)dbuf;
    io = ((ph & 0xFFFFUL) + 512 > 0x10000UL) ? dbuf + 512 : dbuf;
}
static void reset(WORD lw) { r.h.ah = 0; r.h.dl = (BYTE)lw; int86(0x13, &r, &r); }
static WORD bios(WORD lw, BYTE ah, BYTE al, WORD c, WORD h, WORD s)   /* 3 Versuche (Motor anlaufen) */
{
    WORD t, st = 1;
    for (t = 0; t < 3; t++) {
        segread(&sr); sr.es = sr.ds;
        r.h.ah = ah; r.h.al = al; r.h.ch = (BYTE)c; r.h.cl = (BYTE)s; r.h.dh = (BYTE)h; r.h.dl = (BYTE)lw;
        r.x.bx = (WORD)io;
        int86x(0x13, &r, &r, &sr);
        if (!r.x.cflag) return 0;
        if ((st = r.h.ah) == 3) return 3;
        reset(lw);
    }
    return st ? st : 1;
}

static void RDEXPORT d_schluss(void) { tabelle_zurueck(); }
static WORD RDEXPORT d_Typ(WORD lw)
{
    segread(&sr); r.h.ah = 8; r.h.dl = (BYTE)lw; r.x.bx = 0; r.x.cflag = 1;
    int86x(0x13, &r, &r, &sr);
    return (r.x.cflag || r.h.bl > 6) ? 0 : r.h.bl;
}
static WORD RDEXPORT d_Start(WORD lw, const DGEO FAR *g)
{
    geo = *g; puffer();
    tabelle_zurueck(); alt1e = *v1e();
    segread(&sr); r.h.ah = 0x18; r.h.ch = (BYTE)(g->zyl - 1); r.h.cl = (BYTE)g->spt; r.h.dl = (BYTE)lw; r.x.cflag = 1;
    int86x(0x13, &r, &r, &sr);                     /* AT: Medium einstellen, liefert passende Tabelle */
    if (!r.x.cflag && r.h.ah == 0) { _disable(); *v1e() = MK_FP(sr.es, r.x.di); _enable(); gesetzt = 1; }
    reset(lw);
    return 0;
}
static WORD RDEXPORT d_Spur(WORD lw, WORD c, WORD h, WORD pruefen)
{
    WORD i, st;
    if (!io) return 1;
    for (i = 0; i < geo.spt; i++) { io[i * 4] = (BYTE)c; io[i * 4 + 1] = (BYTE)h; io[i * 4 + 2] = (BYTE)(i + 1); io[i * 4 + 3] = 2; }
    st = bios(lw, 5, (BYTE)geo.spt, c, h, 1);
    if (!st && pruefen) st = bios(lw, 4, (BYTE)geo.spt, c, h, 1);
    return st;
}
static WORD RDEXPORT d_Sektor(WORD lw, WORD schreiben, WORD lba, void FAR *puf)
{
    WORD t, i, st; BYTE FAR *p = (BYTE FAR *)puf;
    if (!io || !geo.spt) return 1;
    t = lba / geo.spt;
    if (schreiben) for (i = 0; i < 512; i++) io[i] = p[i];
    st = bios(lw, (BYTE)(schreiben ? 3 : 2), 1, t / geo.koepfe, t % geo.koepfe, lba % geo.spt + 1);
    if (!st && !schreiben) for (i = 0; i < 512; i++) p[i] = io[i];
    return st;
}
static void RDEXPORT d_Ende(WORD lw, WORD dos)
{
    tabelle_zurueck(); reset(lw);
    r.h.ah = 0x0D; int86(0x21, &r, &r);           /* DOS: Puffer schreiben (Disk Reset) */
    segread(&sr); r.h.ah = 0x32; r.h.dl = (BYTE)dos; int86x(0x21, &r, &r, &sr);
    if (r.h.al == 0) ((BYTE FAR *)MK_FP(sr.ds, r.x.bx))[0x18] = 0xFF;   /* DPB: neu aufbauen */
}

static struct { WORD n; FARPROC f[6]; } ext = { 6, {
    (FARPROC)d_schluss, (FARPROC)d_Typ, (FARPROC)d_Start, (FARPROC)d_Spur, (FARPROC)d_Sektor, (FARPROC)d_Ende } };
void FAR * RDAPI RdModInit(void) { return &ext; }
