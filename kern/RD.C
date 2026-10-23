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
char *k_hpath(const char *n)                      /* RD2-Verzeichnis + Name, in cmdline (nach dem Start frei) */
{
    char *d = cmdline; const char *h = k_home;
    while (*h) *d++ = *h++;
    while ((*d++ = *n++) != 0) ;
    return cmdline;
}

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
static void k_ch(char c, int scr) { ser(c); dput(c); if (scr) con(c); }
void k_txt(LPCSTR s, int scr) { while (*s) k_ch(*s++, scr); }
void k_out(LPCSTR s, int scr) { k_txt(s, scr); k_txt("\r\n", scr); }
void k_hex(LPCSTR s, DWORD v, int scr)
{
    WORD i;
    k_txt(s, scr); k_txt(" 0x", scr);
    for (i = 0; i < 8; i++) k_ch("0123456789ABCDEF"[(WORD)(v >> (28 - 4 * i)) & 15], scr);
    k_txt("\r\n", scr);
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
    WORD h; int a = 1, i, koop = 0; DWORD vor, nach; HMOD gdi, kxm, user = 0;
    static const WORD kx[] = { 2, RDO_TimeGet, 3, RDO_TimeSet, 4, RDO_LangGet, 5, RDO_ResLoad, 6, RDO_ResFree,
                               7, RDO_StrLoad, 8, RDO_StrGet, 9, RDO_DbgBreak,
                               10, RDO_BmpLoad, 11, RDO_BmpLoadFile, 12, RDO_IconDraw, 0 };
    static const WORD ux[] = { 0x46, RDO_SendMessage, 0x47, RDO_DispatchMessage, 0x48, RDO_MsgRegister, 0 };
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
        else if (c == 'p') koop |= 2;                    /* /P: Shell (SHELL= in CONFIG.SYS) */
        else if (c == 'w') for (s = argv[a] + 3; *s; s++) k_wseg = (k_wseg << 4) + (*s <= '9' ? *s - '0' : (*s | 0x20) - 'a' + 10);   /* /W:seg vom RunDos-Rest */
    }
    /* Shell: RD laeuft unter dem Rest (Modus 2), der es nach dem Ende wieder startet */
    h = (kr.ax = 0x6200, rd_int(0x21, &kr), kr.bx);
    if (((koop & 2) || *(WORD __far *)MK_FP(h, 0x16) == h) && !k_wseg && (h = k_stub(2)) != 0) rd_swaprun(h);
    koop &= 1;
    /* Erkennung macht nur RD2SETUP: beim allerersten Start (kein RD2.INI) oder mit /SETUP */
    kr.ax = 0x4300; kr.dx = (WORD)k_hpath("RD2.INI"); kr.ds = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
    if (CARRY || i == -1) {
        static const char tail[] = "\x06 /ERST\r";
        static struct { WORD env; const char __far *cmd; DWORD f1, f2; } pb;
        k_hpath("RD2SETUP.EXE");
        pb.cmd = tail;
        kr.ax = 0x4B00; kr.dx = (WORD)cmdline; kr.bx = (WORD)&pb; kr.ds = kr.es = FP_SEG((void __far *)&kr); rd_int(0x21, &kr);
        if (CARRY) k_out("RD: ohne RD2SETUP, Vorgaben", 1);
    }
    if (dlog) { kr.ax = 0x4800; kr.bx = DBUF / 16; rd_int(0x21, &kr); if (!(kr.flags & 1)) dbuf = (char __far *)MK_FP(kr.ax, 0); }
    k_fpu = (BYTE)rd_fpudet();
    k_cpu = (BYTE)rd_cpudet();
    rd_nsinit();
    KAPI(KernVersion, KernVersion); KAPI(KernInfo, KernInfo); KAPI(LastError, LastError); KAPI(RunDos, RunDos);
    KAPI(SetLastError, SetLastError); KAPI(DbgPrint, DbgPrint); KAPI(DbgHex, DbgHex);
    mem_api();
    task_api();
    file_api();
    mod_api();
    k_out("RD: RetroDesk 2 Kern (Phase 3), API 1.0", 1);
    if (a >= argc) { k_out("Aufruf: RD [/x] programm", 1); return 1; }
    for (i = a + 1, cmdline[0] = 0, h = 0; i < argc; i++) {
        for (s = argv[i]; *s && h < 78; ) cmdline[h++] = *s++;
        if (i + 1 < argc && h < 78) cmdline[h++] = ' ';
    }
    cmdline[h] = 0;
    mem_init(112);                                /* Reserve: DOS und der RunDos-Rest */
    task_init();
    {  /* RD2.INI erst nach mem_init lesen (Dateischicht braucht den Speicher) */ char m[6]; ProfileGetStr("RD2.INI", "System", "Maus", "DOS", m, sizeof m); if ((m[0] | 0x20) == 'k') k_maus = 0xFFFF; }
    if (!k_maus) {                                /* Maustreiber da? Vektor 33h gesetzt und kein IRET */
        BYTE __far * __far *v = (BYTE __far * __far *)MK_FP(0, 0x33 * 4);
        if (*v && **v != 0xCF) { kr.ax = 0; kr.bx = 0; rd_int(0x33, &kr); if (kr.ax == 0xFFFF) k_maus = kr.bx == 0xFFFF ? 2 : kr.bx; }
    }
    if (k_maus == 0xFFFF) k_maus = 0;
    if (k_umb_paras) k_hex("RD: UMB", (DWORD)k_umb_paras << 4, 1);
    mod_xinit();
    vor = mem_free_bytes(0);
    kxm = k_sysmod("RDKX.MOD", 0, 0, kx);                 /* Zeit, Sprache, Ressourcen, Texte (verwerfbar) */
    gdi = k_sysmod("RDGDI.MOD", 0x100, 0x3B, gdix);      /* fehlt es: Grafik liefert RD_ERR_NOTSUP */
    if (!gdi) k_hex("RD: RDGDI.MOD fehlt, Fehler", k_lasterr, 1);
    if (gdi) k_mausget = (WORD (__far __pascal *)(WORD __far *))k_modraw(gdi, 0x3C + 5);
    if (gdi) k_gdipause = (void (__far __pascal *)(WORD))k_modraw(gdi, 0x3C + 6);
    if (gdi && (user = k_sysmod("RDUSER.MOD", 0x200, 0x42, ux)) != 0) {   /* Oberflaeche: Fenster, Eingabe */
        ((void (__far __pascal *)(void __far *, void __far *, void __far *))k_modraw(user, 0x45))(k_modraw(gdi, 0x3C + 7), k_modraw(gdi, 0x3C + 8), k_modraw(gdi, 0x3C + 1));
        k_wnd = (DWORD (__far __pascal *)(WORD, HWND, WORD, WORD, DWORD))k_modraw(user, 0x44);
    }
    k_hex("RD: Speicher frei", vor, 1);
    k_hex("RD: CPU", k_cpu, 1);
    k_hex("RD: 8087", k_fpu, 1);
    k_out(koop ? "RD: nur kooperativ" : "RD: Zeitscheiben an", 1);
    rd_preempt = koop ? 0 : 1;
    rd_timer_on(16384);
    h = task_load(argv[a], cmdline, SW_SHOW);
    if (!h) { rd_timer_off(); k_hex("RD: Fehler beim Laden", k_lasterr, 1); mem_done(); return 2; }
    task_run_all();
    clip_done();                                  /* Zwischenablage gehoert dem Kern */
    rd_timer_off();
    k_mausget = 0; k_gdipause = 0; k_wnd = 0;
    k_sysend(user);
    k_sysend(gdi);                                /* zurueck in den Textmodus, Maus aus */
    k_sysend(kxm);
    nach = mem_free_bytes(0);
    k_hex("RD: alle Programme beendet, Speicher frei", nach, 1);
    k_out(vor == nach ? "RD: Speicher vollstaendig zurueck" : "RD: SPEICHERLECK", 1);
    if (dbuf) {
        WORD i; con('-'); con('\r'); con('\n'); for (i = 0; i < dn; i++) con(dbuf[i]);
        kr.ax = 0x4900; kr.es = FP_SEG(dbuf); rd_int(0x21, &kr);
    }
    mem_done();
    if (k_rseg) { if (k_wseg) return 0xD5; rd_swaprun(k_rseg); }   /* RunDos-Neustart: kehrt nicht zurueck */
    return 0;
}
