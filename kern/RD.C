/* RD.C - RetroDesk 2 Kern: Start, Ausgabe fuer die Fehlersuche, Systeminfo.
 * Stand Phase 3: Systemmodul RDGDI (Grafik). Phase 2: Speicher (MEM.C), Tasks/Nachrichten/Zeitgeber (TASK.C).
 * Aufruf: RD [/KOOP] programm.exe [parameter] */
#include "KERN.H"

KREGS kr;
WORD  k_lasterr;
char  k_home[66];
static WORD k_maus;                               /* Maustasten, 0 = keine Maus */
WORD  k_gfx;
BYTE  k_fpu, k_cpu;
static char cmdline[80];

/* ---- Ausgabe: COM1 (immer) und Bildschirm (nur im Kern-Kontext) ---------- */
static void ser(char c)
{
    WORD n = 0;
    while (!(inp(0x3FD) & 0x20) && ++n) ;
    outp(0x3F8, c);
}
static void con(char c) { kr.ax = 0x0200; kr.dx = (BYTE)c; rd_int(0x21, &kr); }
/* /D: Protokoll zusaetzlich im Speicher sammeln und nach dem Ende ausgeben
   (DOSBox hat keine serielle Ausgabe in eine Datei; im Grafikmodus kein con()) */
#define DBUF 1536                                 /* nur mit /D, eigener DOS-Block */
static WORD dlog; static char __far *dbuf; static WORD dn;
static void dput(char c) { if (dbuf && dn < DBUF) dbuf[dn++] = c; }
void k_out(LPCSTR s, int scr)
{
    for (; *s; s++) { ser(*s); dput(*s); if (scr) con(*s); }
    dput('\r'); dput('\n');
    ser('\r'); ser('\n'); if (scr) { con('\r'); con('\n'); }
}
void k_hex(LPCSTR s, DWORD v, int scr)
{
    static const char h[] = "0123456789ABCDEF";
    static char b[100];
    WORD n = 0, i;
    while (*s && n < 80) b[n++] = *s++;
    b[n++] = ' '; b[n++] = '0'; b[n++] = 'x';
    for (i = 0; i < 8; i++) b[n++] = h[(WORD)(v >> (28 - 4 * i)) & 15];
    b[n] = 0;
    k_out(b, scr);
}

/* ---- API: Kern ------------------------------------------------------------ */
WORD RDK KernVersion(void)            { return RD2_API_VERSION; }
WORD RDK LastError(void)              { return k_lasterr; }
void RDK SetLastError(WORD e)         { k_lasterr = e; }
void RDK DbgPrint(LPCSTR s)           { k_out(s, 0); }
void RDK DbgHex(LPCSTR s, DWORD v)    { k_hex(s, v, 0); }
WORD RDK KernInfo(SYSINFO FAR *si)
{
    WORD i;
    BYTE FAR *p = (BYTE FAR *)si;
    for (i = 0; i < sizeof(SYSINFO); i++) p[i] = 0;
    si->api = RD2_API_VERSION; si->modus = RD_MODE_R; si->cpu = k_cpu;
    si->fpu = k_fpu ? 87 : 0;
    kr.ax = 0x3000; rd_int(0x21, &kr); si->dos = (WORD)((kr.ax & 0xFF) << 8) | (kr.ax >> 8);
    rd_int(0x12, &kr); si->kb_konv = kr.ax;
    si->grafik = k_gfx; si->maus = k_maus; si->bootlw = 1; si->sprache = RD_LANG_DE;
    for (i = 0; i < 65 && k_home[i]; i++) si->home[i] = k_home[i];
    return 0;
}

int main(int argc, char **argv)
{
    WORD h; int a = 1, i, koop = 0; DWORD vor, nach; HMOD gdi;
    static const WORD gdix[] = { 0x3C + 1, RDO_GetDC, 0x3C + 2, RDO_ReleaseDC, 0x3C + 3, RDO_GfxList, 0x3C + 4, RDO_GfxSelect, 0 };
    char *s;
    kr.ax = 0x00E3; kr.dx = 0; rd_int(0x14, &kr);           /* COM1 9600 8N1 */
    for (i = 0, s = argv[0]; *s && i < 64; s++) k_home[i++] = *s;
    while (i > 0 && k_home[i - 1] != '\\' && k_home[i - 1] != ':') i--;
    k_home[i] = 0;
    for (; a < argc && (argv[a][0] == '/' || argv[a][0] == '-'); a++) {
        char c = (char)(argv[a][1] | 0x20);
        if (c == 'k') koop = 1;
        else if (c == 'd') dlog = 1;
        else if (c == 'c') k_gfx = GFX_CGA;
        else if (c == 'h') k_gfx = GFX_HGC;
        else if (c == 'v') k_gfx = GFX_VGAMONO;
        else if (c == 'n') k_maus = 0xFFFF;              /* /NOMAUS: Maus nicht benutzen (Testbilder) */
        else if (c == 's') i = -1;                       /* /SETUP: Einrichtung vorher */
    }
    /* Erkennung macht nur RD2SETUP: beim allerersten Start (kein RD2.INI) oder mit /SETUP */
    for (h = 0, s = k_home; *s; ) cmdline[h++] = *s++;
    for (s = "RD2.INI"; (cmdline[h++] = *s++) != 0; ) ;
    kr.ax = 0x4300; kr.dx = (WORD)cmdline; kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    if (CARRY || i == -1) {
        static const char tail[] = "\x06 /ERST\r";
        static struct { WORD env; const char __far *cmd; DWORD f1, f2; } pb;
        for (h -= 8, s = "RD2SETUP.EXE"; (cmdline[h++] = *s++) != 0; ) ;
        pb.cmd = tail;
        kr.ax = 0x4B00; kr.dx = (WORD)cmdline; kr.bx = (WORD)&pb; kr.ds = kr.es = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
        if (CARRY) k_out("RD: RD2SETUP.EXE fehlt, es gelten Vorgaben", 1);
    }
    if (dlog) { kr.ax = 0x4800; kr.bx = DBUF / 16; rd_int(0x21, &kr); if (!(kr.flags & 1)) dbuf = (char __far *)MK_FP(kr.ax, 0); }
    k_fpu = (BYTE)rd_fpudet();
    k_cpu = (BYTE)rd_cpudet();
    rd_nsinit();
    KAPI(KernVersion, KernVersion); KAPI(KernInfo, KernInfo); KAPI(LastError, LastError);
    KAPI(SetLastError, SetLastError); KAPI(DbgPrint, DbgPrint); KAPI(DbgHex, DbgHex);
    mem_api();
    task_api();
    file_api();
    mod_api();
    k_out("RD: RetroDesk 2 Kern (Phase 3), API 1.0", 1);
    if (a >= argc) { k_out("Aufruf: RD [/KOOP] [/CGA|/HGC|/VGA] [/NOMAUS] programm.exe [parameter]", 1); return 1; }
    for (i = a + 1, cmdline[0] = 0, h = 0; i < argc; i++) {
        for (s = argv[i]; *s && h < 78; ) cmdline[h++] = *s++;
        if (i + 1 < argc && h < 78) cmdline[h++] = ' ';
    }
    cmdline[h] = 0;
    mem_init(64);
    task_init();
    {  /* RD2.INI erst nach mem_init lesen (Dateischicht braucht den Speicher) */ char m[6]; ProfileGetStr("RD2.INI", "System", "Maus", "DOS", m, sizeof m); if ((m[0] | 0x20) == 'k') k_maus = 0xFFFF; }
    if (!k_maus) {                                /* Maustreiber da? Vektor 33h gesetzt und kein IRET */
        BYTE __far * __far *v = (BYTE __far * __far *)MK_FP(0, 0x33 * 4);
        if (*v && **v != 0xCF) { kr.ax = 0; kr.bx = 0; rd_int(0x33, &kr); if (kr.ax == 0xFFFF) k_maus = kr.bx == 0xFFFF ? 2 : kr.bx; }
    }
    if (k_maus == 0xFFFF) k_maus = 0;
    if (k_umb_paras) k_hex("RD: hoher Speicher (UMB) Bytes", (DWORD)k_umb_paras << 4, 1);
    mod_xinit();
    vor = mem_free_bytes(0);
    gdi = k_sysmod("RDGDI.MOD", 0x100, 0x3B, gdix);      /* fehlt es: Grafik liefert RD_ERR_NOTSUP */
    if (!gdi) k_hex("RD: RDGDI.MOD fehlt oder ist fehlerhaft, Fehler", k_lasterr, 1);
    if (gdi) k_mausget = (WORD (__far __pascal *)(WORD __far *))k_modraw(gdi, 0x3C + 5);
    k_hex("RD: Speicher frei", vor, 1);
    k_hex("RD: CPU (0 8088, 2 V20, 4 286, 5 386, 6 486, 7 586)", k_cpu, 1);
    k_hex("RD: 8087", k_fpu, 1);
    k_out(koop ? "RD: Umschalten nur kooperativ (/KOOP)" : "RD: Zeitscheiben an", 1);
    rd_preempt = koop ? 0 : 1;
    rd_timer_on(16384);
    h = task_load(argv[a], cmdline, SW_SHOW);
    if (!h) { rd_timer_off(); k_hex("RD: Fehler beim Laden", k_lasterr, 1); mem_done(); return 2; }
    task_run_all();
    clip_done();                                  /* Zwischenablage gehoert dem Kern */
    rd_timer_off();
    k_mausget = 0;
    k_sysend(gdi);                                /* zurueck in den Textmodus, Maus aus */
    nach = mem_free_bytes(0);
    k_hex("RD: alle Programme beendet, Speicher frei", nach, 1);
    k_out(vor == nach ? "RD: Speicher vollstaendig zurueck" : "RD: SPEICHERLECK", 1);
    if (dbuf) {
        WORD i; con('-'); con('\r'); con('\n'); for (i = 0; i < dn; i++) con(dbuf[i]);
        kr.ax = 0x4900; kr.es = FP_SEG(dbuf); rd_int(0x21, &kr);
    }
    mem_done();
    return 0;
}
