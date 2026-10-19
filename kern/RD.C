/* RD.C - RetroDesk 2 Kern, Stand Phase 1: laedt ein RD2-Programm und startet es.
 * Noch ohne Speicherverwaltung, Tasks und Grafik. Prueft Format und Aufrufweg.
 * Bau: Modell Small, -zu (API-Aufrufe kommen mit dem Stack des Programms). */
#define RD2_KERNEL
#include "RD2.H"
#include "RD2ORD.H"
#include <dos.h>
#include <i86.h>
#include <string.h>
#include <conio.h>
#pragma intrinsic(inp, outp)

#define RDK __far __pascal __loadds
extern FARPROC rd_api[RDO_MAX];
extern short __cdecl rd_enter(WORD cs, WORD ip, void __far *api, WORD hinst, WORD show, char __far *cmd);

static WORD lasterr;
static union REGS r;                              /* statisch: SS != DS bei API-Aufrufen */
static struct SREGS sr;
static char numbuf[12];
static char cmdline[128];

/* ---- Ausgabe: COM1 (Fehlersuche) und Bildschirm ------------------------
 * Wird auch aus API-Aufrufen benutzt (SS = Stack des Programms): darum keine
 * Funktionen der C-Bibliothek, nur Ports und eigene Schleifen. */
static void ser(char c)
{
    unsigned n = 0;
    while (!(inp(0x3FD) & 0x20) && ++n) ;            /* Sender frei (mit Zeitgrenze, falls kein COM1) */
    outp(0x3F8, c);
}
static void con(char c) { r.h.ah = 2; r.h.dl = c; intdos(&r, &r); }   /* nur im Kern-Kontext */
static void out(const char __far *s, int scr)
{
    for (; *s; s++) { ser(*s); if (scr) con(*s); }     /* scr nur im Kern-Kontext */
    ser('\r'); ser('\n'); if (scr) { con('\r'); con('\n'); }
}
static char *hex(DWORD v)
{
    static const char h[] = "0123456789ABCDEF";
    int i;
    numbuf[0] = ' '; numbuf[1] = '0'; numbuf[2] = 'x';
    for (i = 0; i < 8; i++) numbuf[3 + i] = h[(v >> (28 - 4 * i)) & 15];
    numbuf[11] = 0;
    return numbuf;
}
static void outhex(const char __far *s, DWORD v, int scr)
{
    static char b[100];
    int n = 0; const char *h = hex(v);
    while (*s && n < 80) b[n++] = *s++;
    while (*h) b[n++] = *h++;
    b[n] = 0;
    out(b, scr);
}

/* ---- API (Phase 1) ---------------------------------------------------- */
WORD RDK KernVersion(void)            { return RD2_API_VERSION; }
WORD RDK LastError(void)              { return lasterr; }
void RDK SetLastError(WORD e)         { lasterr = e; }
HTASK RDK TaskSelf(void)              { return 1; }
void RDK DbgPrint(LPCSTR s)           { out(s, 0); }
void RDK DbgHex(LPCSTR s, DWORD v)    { outhex(s, v, 0); }
DWORD RDK TickCount(void)             { return *(DWORD __far *)MK_FP(0x40, 0x6C) * 55UL; }

/* ---- Programm laden --------------------------------------------------- */
#pragma pack(push, 1)
typedef struct { WORD sig, cblp, cp, crlc, cparhdr, minalloc, maxalloc, ss, sp, csum, ip, cs, lfarlc, ovno; } MZ;
typedef struct { char sig[4]; DWORD blk, len; WORD sum, res; } TRAILER;
typedef struct { char sig[4]; WORD api, flags, stack, kb; char name[16]; WORD ver, nexp, nres, oexp, ores, res[5]; } RD2BLK;
#pragma pack(pop)

/* Laden laeuft im Kern-Kontext, wegen -zu sind Adressen lokaler Variablen aber FAR:
   deshalb alles hier statisch. */
static unsigned got;
static int rdfile(int f, long pos, int how, void *buf, unsigned n)
{
    r.x.ax = 0x4200 | how; r.x.bx = f; r.x.cx = (WORD)(pos >> 16); r.x.dx = (WORD)pos;
    intdos(&r, &r);
    if (r.x.cflag) return 0;
    return _dos_read(f, buf, n, &got) == 0 && got == n;
}

static short res;
static WORD load(const char *path, const char *cmd)
{
    static MZ mz; static TRAILER t; static RD2BLK b; static int f; static unsigned seg;
    static struct { WORD seg, reloc; } pb;
    unsigned i; DWORD img; WORD sum = 0, paras;
    if (_dos_open(path, 0, &f)) return RD_ERR_NOTFOUND;
    if (!rdfile(f, 0, 0, &mz, sizeof mz) || mz.sig != 0x5A4D) { _dos_close(f); return RD_ERR_FORMAT; }
    if (!rdfile(f, -16L, 2, &t, sizeof t) || memcmp(t.sig, "RD2!", 4)) { _dos_close(f); return RD_ERR_FORMAT; }
    if (!rdfile(f, t.blk, 0, &b, sizeof b) || memcmp(b.sig, "RD2P", 4)) { _dos_close(f); return RD_ERR_FORMAT; }
    _dos_close(f);
    for (i = 0; i < sizeof b; i++) sum += ((BYTE *)&b)[i];
    if (sum != t.sum) return RD_ERR_FORMAT;
    if (b.api > RD2_API_VERSION) return RD_ERR_VERSION;
    img = (DWORD)mz.cp * 512 - (mz.cblp ? 512 - mz.cblp : 0) - (DWORD)mz.cparhdr * 16;
    paras = (WORD)((img + 15) >> 4) + mz.minalloc;
    if (_dos_allocmem(paras, &seg)) return RD_ERR_NOMEM;
    pb.seg = seg; pb.reloc = seg;
    r.x.ax = 0x4B03; r.x.dx = FP_OFF(path); sr.ds = FP_SEG((void __far *)path);
    r.x.bx = FP_OFF((void __far *)&pb); sr.es = FP_SEG((void __far *)&pb);
    intdosx(&r, &r, &sr);
    if (r.x.cflag) { _dos_freemem(seg); return RD_ERR_DISK; }
    b.name[15] = 0;
    outhex("RD: geladen, Segment", seg, 1);
    res = rd_enter(seg + mz.cs, mz.ip, (void __far *)rd_api, 1, SW_SHOW, (char __far *)cmd);
    _dos_freemem(seg);
    return RD_OK;
}

int main(int argc, char **argv)
{
    WORD e; int i;
    r.h.ah = 0; r.h.al = 0xE3; r.x.dx = 0; int86(0x14, &r, &r);   /* COM1 9600 8N1 */
    rd_api[RDO_KernVersion]  = (FARPROC)KernVersion;
    rd_api[RDO_LastError]    = (FARPROC)LastError;
    rd_api[RDO_SetLastError] = (FARPROC)SetLastError;
    rd_api[RDO_TaskSelf]     = (FARPROC)TaskSelf;
    rd_api[RDO_DbgPrint]     = (FARPROC)DbgPrint;
    rd_api[RDO_DbgHex]       = (FARPROC)DbgHex;
    rd_api[RDO_TickCount]    = (FARPROC)TickCount;
    out("RD: RetroDesk 2 Kern (Phase 1), API 1.0", 1);
    if (argc < 2) { out("Aufruf: RD programm.exe [parameter]", 1); return 1; }
    cmdline[0] = 0;
    for (i = 2; i < argc && strlen(cmdline) + strlen(argv[i]) < 120; i++) {
        if (i > 2) strcat(cmdline, " ");
        strcat(cmdline, argv[i]);
    }
    e = load(argv[1], cmdline);
    if (e) { outhex("RD: Fehler beim Laden", e, 1); return 2; }
    outhex("RD: Programm beendet, Ergebnis", (DWORD)(WORD)res, 1);
    return 0;
}
