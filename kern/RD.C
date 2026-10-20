/* RD.C - RetroDesk 2 Kern: Start, Ausgabe fuer die Fehlersuche, Systeminfo.
 * Stand Phase 2: Speicher (MEM.C), Tasks/Nachrichten/Zeitgeber (TASK.C).
 * Aufruf: RD [/KOOP] programm.exe [parameter] */
#include "KERN.H"

KREGS kr;
WORD  k_lasterr;
char  k_home[66];
BYTE  k_fpu;
static char cmdline[80];

/* ---- Ausgabe: COM1 (immer) und Bildschirm (nur im Kern-Kontext) ---------- */
static void ser(char c)
{
    WORD n = 0;
    while (!(inp(0x3FD) & 0x20) && ++n) ;
    outp(0x3F8, c);
}
static void con(char c) { kr.ax = 0x0200; kr.dx = (BYTE)c; rd_int(0x21, &kr); }
void k_out(LPCSTR s, int scr)
{
    for (; *s; s++) { ser(*s); if (scr) con(*s); }
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
    si->api = RD2_API_VERSION; si->modus = RD_MODE_R; si->cpu = CPU_8088;
    si->fpu = k_fpu ? 87 : 0;
    kr.ax = 0x3000; rd_int(0x21, &kr); si->dos = (WORD)((kr.ax & 0xFF) << 8) | (kr.ax >> 8);
    rd_int(0x12, &kr); si->kb_konv = kr.ax;
    si->bootlw = 1; si->sprache = RD_LANG_DE;
    for (i = 0; i < 65 && k_home[i]; i++) si->home[i] = k_home[i];
    return 0;
}

int main(int argc, char **argv)
{
    WORD h; int a = 1, i, koop = 0; DWORD vor, nach;
    char *s;
    kr.ax = 0x00E3; kr.dx = 0; rd_int(0x14, &kr);           /* COM1 9600 8N1 */
    for (i = 0, s = argv[0]; *s && i < 64; s++) k_home[i++] = *s;
    while (i > 0 && k_home[i - 1] != '\\' && k_home[i - 1] != ':') i--;
    k_home[i] = 0;
    if (a < argc && (argv[a][0] == '/' || argv[a][0] == '-') && (argv[a][1] == 'K' || argv[a][1] == 'k')) { koop = 1; a++; }
    k_fpu = (BYTE)rd_fpudet();
    rd_nsinit();
    KAPI(KernVersion, KernVersion); KAPI(KernInfo, KernInfo); KAPI(LastError, LastError);
    KAPI(SetLastError, SetLastError); KAPI(DbgPrint, DbgPrint); KAPI(DbgHex, DbgHex);
    mem_api();
    task_api();
    file_api();
    mod_api();
    k_out("RD: RetroDesk 2 Kern (Phase 2), API 1.0", 1);
    if (a >= argc) { k_out("Aufruf: RD [/KOOP] programm.exe [parameter]", 1); return 1; }
    for (i = a + 1, cmdline[0] = 0, h = 0; i < argc; i++) {
        for (s = argv[i]; *s && h < 78; ) cmdline[h++] = *s++;
        if (i + 1 < argc && h < 78) cmdline[h++] = ' ';
    }
    cmdline[h] = 0;
    mem_init(64);
    task_init();
    vor = mem_free_bytes(0);
    k_hex("RD: Speicher frei", vor, 1);
    k_hex("RD: 8087", k_fpu, 1);
    k_out(koop ? "RD: Umschalten nur kooperativ (/KOOP)" : "RD: Zeitscheiben an", 1);
    rd_preempt = koop ? 0 : 1;
    rd_timer_on(16384);
    h = task_load(argv[a], cmdline, SW_SHOW);
    if (!h) { rd_timer_off(); k_hex("RD: Fehler beim Laden", k_lasterr, 1); mem_done(); return 2; }
    task_run_all();
    clip_done();                                  /* Zwischenablage gehoert dem Kern */
    rd_timer_off();
    nach = mem_free_bytes(0);
    k_hex("RD: alle Programme beendet, Speicher frei", nach, 1);
    k_out(vor == nach ? "RD: Speicher vollstaendig zurueck" : "RD: SPEICHERLECK", 1);
    mem_done();
    return 0;
}
