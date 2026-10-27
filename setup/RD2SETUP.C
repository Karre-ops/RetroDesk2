/* RD2SETUP.C - Einrichtung und Installation von RetroDesk 2.
 * Eigenes DOS-Programm (Textmodus, laeuft ab DOS 3 ohne RetroDesk). Erkennt die Hardware
 * EINMAL, zeigt sie als Liste, laesst jeden Wert aendern und schreibt [System] in RD2.INI.
 * RD.EXE selbst tastet nichts mehr ab, es liest nur RD2.INI (und ruft dieses Programm
 * beim allerersten Start auf, wenn RD2.INI fehlt).
 *   RD2SETUP          Liste, Aendern, Speichern, Installieren
 *   RD2SETUP /ERST    wie oben, mit Hinweis "erster Start" (ruft RD.EXE)
 *   RD2SETUP /AUTO    nur erkennen und speichern, ohne Rueckfrage (Tests, Stapeldateien)
 * Maus: heute nutzt RD2 den DOS-Treiber (INT 33h). Die Maus-Hardware wird trotzdem schon
 * erkannt und als MausHW= gespeichert, fuer den eigenen Treiber MAUS.DRV. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>
#include <conio.h>
#include <direct.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>

typedef unsigned char BYTE; typedef unsigned short WORD; typedef unsigned long DWORD;
extern WORD rd_cpudet(void);
extern WORD rd_fpudet(void);
#pragma aux rd_cpudet "_*" modify [ax bx cx dx];
#pragma aux rd_fpudet "_*" modify [ax bx cx dx];

/* ---- Bildschirm (BIOS, 80x25) ------------------------------------------------ */
static union REGS r;
static BYTE attr = 0x07;
static void gotoxy(int x, int y) { r.h.ah = 2; r.h.bh = 0; r.h.dl = (BYTE)x; r.h.dh = (BYTE)y; int86(0x10, &r, &r); }
static void putch_a(char c)
{
    r.h.ah = 9; r.h.al = (BYTE)c; r.h.bh = 0; r.h.bl = attr; r.x.cx = 1; int86(0x10, &r, &r);
    r.h.ah = 3; r.h.bh = 0; int86(0x10, &r, &r); gotoxy(r.h.dl + 1, r.h.dh);
}
static void puts_a(const char *s) { while (*s) putch_a(*s++); }
static void zeile(int y, const char *s)          /* ganze Zeile, Rest mit Leerzeichen */
{
    int n = 0; gotoxy(0, y);
    while (*s && n < 80) { putch_a(*s++); n++; }
    while (n++ < 80) putch_a(' ');
}
static void cls(void) { r.x.ax = 0x0600; r.h.bh = 0x07; r.x.cx = 0; r.x.dx = 0x184F; int86(0x10, &r, &r); gotoxy(0, 0); }
static WORD taste(void) { r.h.ah = 0; int86(0x16, &r, &r); return r.x.ax; }
#define T_HOCH 0x4800
#define T_RUNTER 0x5000
#define T_ENTER 0x1C0D
#define T_ESC   0x011B

/* ---- Zeit ---------------------------------------------------------------------- */
static volatile DWORD __far *tick = (DWORD __far *)MK_FP(0x40, 0x6C);
static void ticks(WORD n) { DWORD t = *tick; while (*tick - t < n) ; }   /* 1 Tick = 55 ms */

/* ---- Erkennung ----------------------------------------------------------------- */
static WORD cpu, fpu, dosv, konvkb, xmskb, umbkb, quelle_vga16, quelle_vesa, hw_vesa;
static BYTE hw_vga, hw_ega, hw_hgc, hw_cga, hw_adlib, hw_ps2, hw_sb;
static char sbwert[24] = "A220 I5 D1", sbanz[40];
static WORD pkt_int;                              /* Paketreiber-Interrupt, 0 = keiner */
static char mausinfo[40], mauswert[16];
static const char *cpuname[] = { "8088", "8086", "V20/V30", "80186", "80286", "80386", "80486", "Pentium" };

static int int33_da(void)
{
    BYTE __far * __far *v = (BYTE __far * __far *)MK_FP(0, 0x33 * 4);
    if (!*v || **v == 0xCF) return 0;
    r.x.ax = 0; int86(0x33, &r, &r);
    return r.x.ax == 0xFFFF;
}
static int hgc_da(void)                           /* Bit 7 von 3BAh wechselt bei Hercules */
{
    BYTE a = (BYTE)(inp(0x3BA) & 0x80); WORD i;
    for (i = 0; i < 30000; i++) if ((inp(0x3BA) & 0x80) != a) return 1;
    return 0;
}
static int crtc_da(WORD p)                        /* CRTC antwortet: Cursorregister schreiben/lesen */
{
    BYTE alt, neu;
    outp(p, 0x0F); alt = (BYTE)inp(p + 1);
    outp(p + 1, 0x5A); neu = (BYTE)inp(p + 1);
    outp(p + 1, alt);
    return neu == 0x5A;
}
static void grafik_erkennen(void)
{
    WORD equip;
    hw_vga = hw_ega = hw_hgc = hw_cga = 0;
    r.x.ax = 0x1A00; int86(0x10, &r, &r);
    if (r.h.al == 0x1A && (r.h.bl == 7 || r.h.bl == 8)) hw_vga = 1;
    if (hw_vga) {                                 /* VBE da? (INT 10h 4F00h, Block mit "VESA") */
        static char vb[512]; struct SREGS sr; segread(&sr);
        sr.es = FP_SEG((char far *)vb); r.x.di = FP_OFF((char far *)vb); r.x.ax = 0x4F00; int86x(0x10, &r, &r, &sr);
        hw_vesa = (WORD)(r.x.ax == 0x004F && !memcmp(vb, "VESA", 4));
    }
    if (!hw_vga) { r.h.ah = 0x12; r.h.bl = 0x10; int86(0x10, &r, &r); if (r.h.bl != 0x10) hw_ega = 1; }
    int86(0x11, &r, &r); equip = r.x.ax;
    if ((equip & 0x30) == 0x30) hw_hgc = (BYTE)hgc_da();
    else if (!hw_vga && !hw_ega) hw_cga = (BYTE)crtc_da(0x3D4);
    if (hw_vga || hw_ega) hw_cga = 1;             /* Modus 6 kann jede EGA/VGA */
}
static int adlib_reg(BYTE reg, BYTE val)
{
    int i;
    outp(0x388, reg); for (i = 0; i < 6; i++) inp(0x388);
    outp(0x389, val); for (i = 0; i < 35; i++) inp(0x388);
    return 0;
}
static int dsp_da(WORD b)                         /* DSP-Reset: 1 an b+6, kurz warten, 0, dann AAh an b+0Ah */
{
    WORD i, j;
    outp(b + 6, 1); for (i = 0; i < 16; i++) inp(b + 6);
    outp(b + 6, 0);
    for (j = 0; j < 4; j++) for (i = 0; i < 0x4000; i++) if (inp(b + 0x0E) & 0x80) { if (inp(b + 0x0A) == 0xAA) return 1; break; }
    return 0;
}
static void sb_erkennen(void)                     /* BLASTER=A220 I5 D1 ... oder Ports 220h..280h abfragen */
{
    const char *e = getenv("BLASTER"); WORD a = 0, irq = 5, dma = 1, b;
    while (e && *e) {
        char c = (char)(*e & 0xDF); e++;
        if (c == 'A') a = (WORD)strtoul(e, (char **)&e, 16);
        else if (c == 'I') irq = (WORD)strtoul(e, (char **)&e, 10);
        else if (c == 'D') dma = (WORD)strtoul(e, (char **)&e, 10);
        else while (*e && *e != ' ') e++;
        while (*e == ' ') e++;
    }
    if (a && !dsp_da(a)) a = 0;
    for (b = 0x220; !a && b <= 0x280; b += 0x20) if (dsp_da(b)) a = b;
    hw_sb = (BYTE)(a != 0);
    if (!a) a = 0x220;
    sprintf(sbwert, "A%X I%u D%u", a, irq, dma);
    sprintf(sbanz, "%Xh, IRQ %u, DMA %u%s", a, irq, dma, hw_sb ? "" : " (nicht erkannt)");
}
static void klang_erkennen(void)                  /* AdLib: Zeitgeber 1 starten, Statusbits pruefen */
{
    BYTE s1, s2; int i;
    adlib_reg(4, 0x60); adlib_reg(4, 0x80);
    s1 = (BYTE)inp(0x388);
    adlib_reg(2, 0xFF); adlib_reg(4, 0x21);
    for (i = 0; i < 200; i++) inp(0x388);         /* > 80 us */
    s2 = (BYTE)inp(0x388);
    adlib_reg(4, 0x60); adlib_reg(4, 0x80);
    hw_adlib = (BYTE)((s1 & 0xE0) == 0 && (s2 & 0xE0) == 0xC0);
    sb_erkennen();
}
static int inport_da(WORD b)                      /* ID-Register: abwechselnd DEh und Version */
{
    BYTE a = (BYTE)inp(b + 2), c = (BYTE)inp(b + 2), d = (BYTE)inp(b + 2);
    return (a == 0xDE && d == 0xDE && c != 0xDE) || (c == 0xDE && a != 0xDE && d != 0xDE);
}
static int logibus_da(WORD b)                     /* 8255: Signatur in Port B schreiben und lesen */
{
    outp(b + 3, 0x91); outp(b + 1, 0xA5);
    if (inp(b + 1) != 0xA5) return 0;
    outp(b + 1, 0x5A);
    if (inp(b + 1) != 0x5A) return 0;
    outp(b + 2, 0x10);                            /* Interrupt der Karte aus */
    return 1;
}
static int seriell_da(WORD p)                     /* Microsoft-Protokoll: nach DTR/RTS meldet sich die Maus mit 'M' */
{
    BYTE lcr = (BYTE)inp(p + 3), mcr = (BYTE)inp(p + 4), c; int ok = 0; DWORD t;
    outp(p + 3, 0x80); outp(p, 0x60); outp(p + 1, 0); outp(p + 3, 0x02);   /* 1200 Baud, 7N1 */
    outp(p + 1, 0); outp(p + 4, 0); ticks(3);
    while (inp(p + 5) & 1) inp(p);
    outp(p + 4, 0x03);
    for (t = *tick; *tick - t < 6 && !ok; )
        if (inp(p + 5) & 1) { c = (BYTE)inp(p); if (c == 'M') ok = 1; }
    outp(p + 4, mcr); outp(p + 3, lcr);
    return ok;
}
static void maus_erkennen(void)
{
    WORD __far *com = (WORD __far *)MK_FP(0x40, 0); int i;
    hw_ps2 = 0;
    if (int33_da()) { strcpy(mausinfo, "DOS-Treiber (INT 33h) geladen"); strcpy(mauswert, "DOS"); return; }
    strcpy(mauswert, "KEINE");
    if (inport_da(0x23C)) { strcpy(mausinfo, "Busmaus InPort 23Ch"); strcpy(mauswert, "INPORT:23C"); return; }
    if (inport_da(0x238)) { strcpy(mausinfo, "Busmaus InPort 238h"); strcpy(mauswert, "INPORT:238"); return; }
    if (logibus_da(0x23C)) { strcpy(mausinfo, "Busmaus Logitech 23Ch"); strcpy(mauswert, "BUS:23C"); return; }
    for (i = 0; i < 4; i++)
        if (com[i] && seriell_da(com[i])) {
            sprintf(mausinfo, "serielle Maus an COM%d", i + 1); sprintf(mauswert, "SERIELL:COM%d", i + 1); return;
        }
    if (cpu >= 4) {                               /* PS/2 ueber das BIOS: initialisieren, zuruecksetzen = Maus steckt */
        r.x.ax = 0xC205; r.h.bh = 3; int86(0x15, &r, &r);
        if (!r.x.cflag && !r.h.ah) { r.x.ax = 0xC201; int86(0x15, &r, &r); }
        if (!r.x.cflag && !r.h.ah) { r.x.ax = 0xC200; r.h.bh = 0; int86(0x15, &r, &r); hw_ps2 = 1; strcpy(mausinfo, "PS/2-Maus"); strcpy(mauswert, "PS2"); return; }
    }
    strcpy(mausinfo, "keine gefunden");
}
static void netz_erkennen(void)                   /* Paketreiber: "PKT DRVR" 3 Bytes hinter dem Einsprung */
{
    WORD i;
    pkt_int = 0;
    for (i = 0x60; i <= 0x80 && !pkt_int; i++) {
        char __far * __far *v = (char __far * __far *)MK_FP(0, i * 4);
        if (*v && _fmemcmp(*v + 3, "PKT DRVR", 8) == 0) pkt_int = i;
    }
}
static void speicher_erkennen(void)
{
    WORD strat, link, seg;
    int86(0x12, &r, &r); konvkb = r.x.ax;
    r.x.ax = 0x3000; intdos(&r, &r); dosv = (WORD)(r.h.al << 8 | r.h.ah);
    xmskb = 0; umbkb = 0;
    r.x.ax = 0x4300; int86(0x2F, &r, &r);
    if (r.h.al == 0x80) {
        struct SREGS sr; void (__far *xms)(void);
        r.x.ax = 0x4310; int86x(0x2F, &r, &r, &sr);
        xms = (void (__far *)(void))MK_FP(sr.es, r.x.bx);
        { WORD kb = 0; _asm { mov ah, 8
                          call dword ptr xms
                          mov kb, dx } xmskb = kb; }
    }
    if (dosv >= 0x0500) {                         /* hoher Speicher: groessten UMB abfragen */
        r.x.ax = 0x5800; intdos(&r, &r); strat = r.x.ax;
        r.x.ax = 0x5802; intdos(&r, &r); link = r.h.al;
        r.x.ax = 0x5803; r.x.bx = 1; intdos(&r, &r);
        if (!r.x.cflag) {
            r.x.ax = 0x5801; r.x.bx = 0x40; intdos(&r, &r);
            r.h.ah = 0x48; r.x.bx = 0xFFFF; intdos(&r, &r);
            seg = r.x.bx;
            r.h.ah = 0x48; intdos(&r, &r);
            if (!r.x.cflag) { if (r.x.ax >= 0xA000) umbkb = seg / 64; { struct SREGS sr; segread(&sr); sr.es = r.x.ax; r.h.ah = 0x49; intdosx(&r, &r, &sr); } }
        }
        r.x.ax = 0x5801; r.x.bx = strat; intdos(&r, &r);
        r.x.ax = 0x5803; r.x.bx = link; intdos(&r, &r);
    }
}

/* ---- Einstellungen ------------------------------------------------------------- */
typedef struct { const char *titel, *key; const char *wert[8]; const char *anzeige[8]; BYTE n, v, erk; } PUNKT;
static PUNKT p[] = {
    { "Grafik",     "Grafik", { 0 }, { 0 }, 0, 0, 0 },
    { "Profil",     "Profil", { "SCHNELL", "KOMFORT" }, { "Schnell (1 Bit)", "Komfort (Farbe, ab 386)" }, 2, 0, 0 },
    { "Maus",       "Maus",   { "DOS", "KEINE" }, { "DOS-Treiber (INT 33h)", "keine" }, 2, 0, 0 },
    { "Klang",      "Sound",  { "SPEAKER", "ADLIB", "SB", "COVOX", "AUS" }, { "PC-Lautsprecher", "AdLib (388h)", "Sound Blaster", "Covox (LPT1)", "aus" }, 5, 0, 0 },
    { "Sound Blaster", "SB",  { 0 }, { 0 }, 0, 0, 0 },
    { "XMS-Ablage", "XMS",    { "0", "64", "128", "256", "512" }, { "aus", "64 KB", "128 KB", "256 KB", "512 KB" }, 5, 0, 0 },
    { "Netz",       "Netz",   { "KEIN", "PAKET" }, { "keins", "Paketreiber" }, 2, 0, 0 },
};
#define NP (sizeof p / sizeof p[0])
enum { P_GRAFIK, P_PROFIL, P_MAUS, P_KLANG, P_SB, P_XMS, P_NETZ };
static char ini[80], quelle[80];

static void setze(PUNKT *q, const char *w) { BYTE i; for (i = 0; i < q->n; i++) if (!stricmp(q->wert[i], w)) { q->v = i; return; } }
static void grafikliste(void)                    /* nur was die Hardware kann (und dessen Treiber da ist) */
{
    PUNKT *g = &p[P_GRAFIK]; g->n = 0;
    if (hw_vga && quelle_vga16) { g->wert[g->n] = "VGA16"; g->anzeige[g->n++] = "VGA 16 Farben 640x480"; }
    if (hw_vesa && quelle_vesa) { g->wert[g->n] = "VESA"; g->anzeige[g->n++] = "VESA 256 Farben 640x480"; }
    if (hw_vga) { g->wert[g->n] = "VGA";   g->anzeige[g->n++] = "VGA mono 640x480"; }
    if (hw_hgc) { g->wert[g->n] = "HGC";   g->anzeige[g->n++] = "Hercules 720x348"; }
    if (hw_cga) { g->wert[g->n] = "CGA";   g->anzeige[g->n++] = "CGA 640x200"; }
    if (!g->n)  { g->wert[g->n] = "CGA";   g->anzeige[g->n++] = "CGA 640x200 (nichts erkannt)"; }
}
static void sbliste(void)                         /* erkannter Wert zuerst, dann uebliche Einstellungen */
{
    static const char *const w[] = { "A220 I5 D1", "A220 I7 D1", "A240 I5 D1", "A240 I7 D1", "A220 I10 D1" };
    PUNKT *q = &p[P_SB]; BYTE i;
    q->n = 0; q->wert[q->n] = sbwert; q->anzeige[q->n++] = sbanz;
    for (i = 0; i < 5; i++) if (stricmp(w[i], sbwert)) { q->wert[q->n] = w[i]; q->anzeige[q->n++] = w[i]; }
}
static void vorschlag(int i)                     /* Wert aus der Erkennung */
{
    PUNKT *q = &p[i];
    switch (i) {
    case P_GRAFIK: grafikliste(); q->v = (BYTE)((cpu >= 5 && hw_vga && quelle_vga16) ? 0 : (hw_vga && quelle_vga16) ? 1 : 0); break;
    case P_PROFIL: q->v = (BYTE)(cpu >= 5); break;
    case P_MAUS:   q->v = (BYTE)(strcmp(mauswert, "DOS") != 0); break;
    case P_KLANG:  q->v = (BYTE)(hw_sb ? 2 : hw_adlib ? 1 : 0); break;
    case P_SB:     sbliste(); q->v = 0; break;
    case P_XMS:    q->v = (BYTE)(xmskb >= 256 ? 3 : xmskb >= 128 ? 2 : xmskb >= 64 ? 1 : 0); break;
    case P_NETZ:   q->v = (BYTE)(pkt_int != 0); break;
    }
    q->erk = q->v;
}
static void erkennen(int i)                      /* -1 = alles */
{
    if (i < 0 || i == P_GRAFIK || i == P_PROFIL) { cpu = rd_cpudet(); fpu = rd_fpudet(); grafik_erkennen(); }
    if (i < 0 || i == P_MAUS) maus_erkennen();
    if (i < 0 || i == P_KLANG || i == P_SB) klang_erkennen();
    if (i < 0 || i == P_XMS) speicher_erkennen();
    if (i < 0 || i == P_NETZ) netz_erkennen();
    if (i < 0) { for (i = 0; i < (int)NP; i++) vorschlag(i); }
    else vorschlag(i);
}

/* ---- RD2.INI: Schluessel in [System] ersetzen oder anhaengen, Rest bleibt ------- */
static char buf[6144], neu[6144];
static int ini_schreiben(const char *datei)
{
    FILE *f; size_t n = 0, o = 0; char *z, *e; int insys = 0, sysda = 0; BYTE done[NP + 2]; WORD i;
    char zeile_[80];
    memset(done, 0, sizeof done);
    if ((f = fopen(datei, "rb")) != NULL) { n = fread(buf, 1, sizeof buf - 1, f); fclose(f); }
    if (n == sizeof buf - 1) return -1;           /* zu gross: nicht kuerzen, nicht schreiben */
    buf[n] = 0;
    #define AUS(s) do { size_t l_ = strlen(s); if (o + l_ < sizeof neu) { memcpy(neu + o, s, l_); o += l_; } } while (0)
    #define REST() do { for (i = 0; i < NP; i++) if (!done[i]) { sprintf(zeile_, "%s=%s\r\n", p[i].key, p[i].wert[p[i].v]); AUS(zeile_); done[i] = 1; } \
                        if (!done[NP]) { sprintf(zeile_, "MausHW=%s\r\n", mauswert); AUS(zeile_); done[NP] = 1; } } while (0)
    for (z = buf; *z; z = e) {
        char k[24]; int kl = 0;
        for (e = z; *e && *e != '\n'; e++) ;
        if (*e) e++;
        if (*z == '[') {
            if (insys) REST();
            insys = !strnicmp(z, "[System]", 8); if (insys) sysda = 1;
        } else if (insys) {
            while (z + kl < e && z[kl] != '=' && kl < 23) { k[kl] = z[kl]; kl++; }
            k[kl] = 0;
            for (i = 0; i < NP; i++) if (!stricmp(k, p[i].key)) break;
            if (i < NP || !stricmp(k, "MausHW")) {        /* eigene Zeile ersetzen */
                if (i == NP) { sprintf(zeile_, "MausHW=%s\r\n", mauswert); done[NP] = 1; }
                else { sprintf(zeile_, "%s=%s\r\n", p[i].key, p[i].wert[p[i].v]); done[i] = 1; }
                AUS(zeile_); continue;
            }
        }
        { char c = *e; *e = 0; AUS(z); *e = c; }
    }
    if (insys) REST();
    if (!sysda) { if (o && neu[o - 1] != '\n') AUS("\r\n"); AUS("[System]\r\n"); REST(); }
    if ((f = fopen(datei, "wb")) == NULL) return 0;
    n = fwrite(neu, 1, o, f); fclose(f);
    return n == o;
}
static void ini_lesen(const char *datei)         /* vorhandene Werte uebernehmen */
{
    FILE *f; char z[80], *g; int insys = 0; WORD i;
    if ((f = fopen(datei, "r")) == NULL) return;
    while (fgets(z, sizeof z, f)) {
        if (*z == '[') { insys = !strnicmp(z, "[System]", 8); continue; }
        if (!insys || (g = strchr(z, '=')) == NULL) continue;
        *g++ = 0; g[strcspn(g, "\r\n ")] = 0;
        for (i = 0; i < NP; i++) if (!stricmp(z, p[i].key)) setze(&p[i], g);
    }
    fclose(f);
}

/* ---- Installation -------------------------------------------------------------- */
static int kopieren(const char *von, const char *nach)
{
    FILE *a, *b; size_t n; int ok = 1;
    if ((a = fopen(von, "rb")) == NULL) return 0;
    if ((b = fopen(nach, "wb")) == NULL) { fclose(a); return 0; }
    while ((n = fread(buf, 1, sizeof buf, a)) > 0) if (fwrite(buf, 1, n, b) != n) { ok = 0; break; }
    fclose(a); if (fclose(b)) ok = 0;
    return ok;
}
static int eingabe(int x, int y, char *s, int max)
{
    WORD k; int n = (int)strlen(s);
    for (;;) {
        gotoxy(x, y); attr = 0x70; puts_a(s); attr = 0x07; puts_a("  "); gotoxy(x + n, y);
        k = taste();
        if (k == T_ESC) return 0;
        if (k == T_ENTER) return n > 0;
        if ((k & 0xFF) == 8) { if (n) s[--n] = 0; gotoxy(x + n, y); putch_a(' '); continue; }
        if ((k & 0xFF) >= ' ' && n < max) { s[n++] = (char)((k & 0xFF) >= 'a' && (k & 0xFF) <= 'z' ? (k & 0xFF) - 32 : k & 0xFF); s[n] = 0; }
    }
}
static char boot(void) { r.x.ax = 0x3305; intdos(&r, &r); return (char)(dosv >= 0x0400 && r.h.dl ? 'A' - 1 + r.h.dl : 'C'); }
static int gibts(const char *d) { unsigned a; return !_dos_getfileattr(d, &a) && !(a & _A_SUBDIR); }
static size_t lesen(const char *d)                /* ganze Datei nach buf, sizeof buf - 1 = zu gross */
{
    FILE *f; size_t n = 0;
    if ((f = fopen(d, "rb")) != NULL) { n = fread(buf, 1, sizeof buf - 1, f); fclose(f); }
    buf[n] = 0; return n;
}
static int finden(const char *w, char *aus, char lw)   /* Programm mit vollem Pfad: direkt, im Wurzelverzeichnis, im PATH */
{
    static const char *ext[] = { "", ".COM", ".EXE" }; const char *pa = getenv("PATH"); char d[64]; int i, l, pf = strchr(w, '\\') || strchr(w, ':');
    sprintf(d, "%c:", lw);
    for (;;) {
        for (i = 0; i < 3 && !(i && strchr(w, '.')); i++) {
            if (!pf) sprintf(aus, "%s\\%s%s", d, w, ext[i]);
            else if (w[1] == ':') sprintf(aus, "%s%s", w, ext[i]); else sprintf(aus, "%c:%s%s", lw, w, ext[i]);
            if (gibts(aus)) { strupr(aus); return 1; }
        }
        if (pf || !pa || !*pa) return 0;
        l = (int)strcspn(pa, ";"); if (l > 60) l = 60;
        memcpy(d, pa, l); d[l] = 0; if (l && d[l - 1] == '\\') d[l - 1] = 0;
        pa += l; if (*pa) pa++;
    }
}
static WORD janein(int y, const char *frage) { WORD t; zeile(y, frage); t = taste() & 0xFF; return t == 'j' || t == 'J' || t == 'y' || t == 'Y'; }
static const char *nicht[] = { "REM", "ECHO", "SET", "PATH", "PROMPT", "CLS", "GOTO", "IF", "CALL", "PAUSE", "FOR", "SHIFT",
                               "VER", "CD", "CHDIR", "COMMAND", "WIN", "DOSSHELL", "EXIT", 0 };
/* G21: RD.EXE als Shell (SHELL= in CONFIG.SYS). Ohne CONFIG.SYS und AUTOEXEC.BAT (neues System) ohne Rueckfrage,
   sonst Sicherung CONFIG.RDA und Rueckfrage je Programmzeile der AUTOEXEC.BAT (wird als INSTALL= uebernommen).
   Rueckgabe: 1 geaendert (Sicherung CONFIG.RDA), 3 neu angelegt, 2 schon eingetragen, 0 Fehler, -1 Datei zu gross, -2 abgelehnt */
static int shell_eintragen(const char *ziel, int y, int *verloren)
{
    char cs[16], z[96], w[64], pf[80]; size_t n, o = 0, m; char *a, *e, *q; int alt, i, mausda = 0; FILE *f;
    sprintf(cs, "%c:\\CONFIG.SYS", ziel[0]); sprintf(z, "%c:\\AUTOEXEC.BAT", ziel[0]);
    alt = gibts(cs) || gibts(z); *verloren = 0;
    n = lesen(cs); if (n == sizeof buf - 1) return -1;
    strcpy(neu, buf); strupr(neu); sprintf(z, "%sRD.EXE", ziel);
    if (strstr(neu, z)) return 2;
    if (alt && !janein(y, "RD2 als Shell in CONFIG.SYS eintragen (J/N)?")) return -2;
    if (n) { sprintf(z, "%c:\\CONFIG.RDA", ziel[0]); if (!kopieren(cs, z)) return 0; lesen(cs); }
    #define NEU(t) do { m = strlen(t); if (o + m >= sizeof neu) return -1; memcpy(neu + o, t, m); o += m; } while (0)
    for (a = buf; *a; a = e) {                    /* alte SHELL=-Zeile bleibt als REM stehen */
        char c;
        for (e = a; *e && *e != '\n'; e++) ;
        if (*e) e++;
        for (q = a; *q == ' ' || *q == '\t'; q++) ;
        if (!strnicmp(q, "SHELL", 5) && (q[5] == '=' || q[5] == ' ')) NEU("REM ");
        c = *e; *e = 0; NEU(a); *e = c;
    }
    if (o && neu[o - 1] != '\n') NEU("\r\n");
    sprintf(z, "SHELL=%sRD.EXE /P %sSHELL.EXE\r\n", ziel, ziel); NEU(z);   /* Startprogramm: ohne es meldet RD nur den Aufruf */
    if (alt) {                                    /* Programme aus AUTOEXEC.BAT uebernehmen? */
        sprintf(z, "%c:\\AUTOEXEC.BAT", ziel[0]); n = lesen(z);
        for (a = buf; *a; a = e) {
            for (e = a; *e && *e != '\n'; e++) ;
            q = a; if (*e) *e++ = 0;
            q[strcspn(q, "\r")] = 0;
            while (*q == ' ' || *q == '\t' || *q == '@') q++;
            if (!*q || *q == ':' || *q == ';') continue;
            for (i = 0; i < 2; i++) {               /* LH / LOADHIGH samt /L:-Schaltern ueberspringen */
                if (!strnicmp(q, "LH ", 3)) q += 3; else if (!strnicmp(q, "LOADHIGH ", 9)) q += 9; else break;
                while (*q == ' ' || *q == '/') { if (*q == '/') q += strcspn(q, " "); else q++; }
            }
            m = strcspn(q, " \t="); if (!m || m >= sizeof w) continue;
            memcpy(w, q, m); w[m] = 0; strupr(w);
            for (i = 0; nicht[i] && stricmp(w, nicht[i]); i++) ;
            if (nicht[i]) { if (i == 3 || i == 2) *verloren = 1; continue; }   /* SET/PATH gelten ohne COMMAND.COM nicht */
            if (strstr(w, "RD.EXE") || strstr(w, "RD2SETUP")) continue;
            if (!finden(w, pf, ziel[0])) continue;
            sprintf(z, "Uebernehmen (J/N)? %.58s", q);
            if (!janein(y, z)) continue;
            if (strstr(pf, "MOUSE")) mausda = 1;
            sprintf(z, "INSTALL=%s%.60s\r\n", pf, q + m); NEU(z);
        }
    }
    if (!mausda) {                                /* Maustreiber: CTMOUSE fuer seriell/PS2, sonst Herstellertreiber */
        sprintf(pf, "%sCTMOUSE.EXE", ziel);
        if ((!strncmp(mauswert, "SERIELL", 7) || !strcmp(mauswert, "PS2") || (!alt && !strcmp(mauswert, "DOS"))) && gibts(pf)) {
            sprintf(z, "INSTALL=%s\r\n", pf); NEU(z);
        } else if (!alt && (!strncmp(mauswert, "BUS", 3) || !strncmp(mauswert, "INPORT", 6))) {
            w[0] = 0; zeile(y, "Treiber fuer die Busmaus (z.B. A:\\MOUSE.COM, leer = keiner):");
            if (eingabe(62, y, w, 17) && gibts(w)) {
                q = strrchr(w, '\\'); q = q ? q + 1 : (w[1] == ':' ? w + 2 : w);
                sprintf(pf, "%s%s", ziel, q);
                if (kopieren(w, pf)) { sprintf(z, "INSTALL=%s\r\n", pf); NEU(z); }
            }
        }
    }
    if ((f = fopen(cs, "wb")) == NULL) return 0;
    m = fwrite(neu, 1, o, f);
    return fclose(f) == 0 && m == o ? (alt ? 1 : 3) : 0;
}
static int ordner(const char *ziel)               /* G22: Ebene fuer Ebene anlegen; ziel endet mit '\' */
{
    char d[64]; int i; unsigned a;
    for (i = 3; ziel[i]; i++) if (ziel[i] == '\\') { memcpy(d, ziel, i); d[i] = 0; mkdir(d); }
    return !_dos_getfileattr(d, &a) && (a & _A_SUBDIR);
}
static int startfaehig(char lw, int y)            /* Variante A: leere Platte mit FreeDOS SYS startfaehig machen */
{
    char p[80], k[8], d[4]; int h, alt, e;
    sprintf(p, "%c:\\KERNEL.SYS", lw); if (gibts(p)) return 0;          /* hat schon ein System */
    sprintf(p, "%c:\\IO.SYS", lw); if (gibts(p)) return 0;
    sprintf(p, "%c:\\IBMBIO.COM", lw); if (gibts(p)) return 0;
    sprintf(k, "%c:\\", quelle[0]); sprintf(p, "%sKERNEL.SYS", k);
    if (!gibts(p)) return 0;                      /* Diskette mit eigenem DOS (SYS A:): kein FreeDOS-Kern da */
    sprintf(p, "%sRDSYS.EXE", quelle); if (!gibts(p)) return 0;
    if (!janein(y, "Festplatte startfaehig machen (FreeDOS-Kern) (J/N)?")) { zeile(y, "Festplatte nicht startfaehig gemacht."); return 0; }
    zeile(y, "Uebertrage FreeDOS-Kern ...");
    d[0] = lw; d[1] = ':'; d[2] = 0;
    fflush(stdout); alt = dup(1); h = open("NUL", O_WRONLY); if (h >= 0) { dup2(h, 1); close(h); }   /* SYS-Ausgaben nicht auf den Schirm */
    e = spawnl(P_WAIT, p, "RDSYS", k, d, "/UPDATE", NULL);
    if (alt >= 0) { dup2(alt, 1); close(alt); }
    zeile(y, e == 0 ? "Festplatte ist startfaehig (FreeDOS-Kern)." : "FEHLER: Festplatte nicht startfaehig (SYS).");
    return e == 0 ? 1 : -1;
}
static void installieren(void)
{
    static const char *muster[] = { "*.EXE", "*.MOD", "*.DRV", "*.FNT", "*.LNG", "*.BIN", 0 };   /* BIN: RDSWAP.BIN fuer RunDos */
    char ziel[64], von[80], nach[80], z[80]; struct find_t ft; int i, k = 0, fehl = 0, y = 17;
    sprintf(ziel, "%c:\\RD2", boot() == 'A' ? 'C' : boot());
    zeile(y, "Installieren nach:"); 
    if (!eingabe(19, y, ziel, 40)) { zeile(y, "Installation abgebrochen."); return; }
    if (ziel[strlen(ziel) - 1] != '\\') strcat(ziel, "\\");
    if (!stricmp(ziel, quelle)) { zeile(y + 1, "Ziel ist das Quellverzeichnis."); return; }
    if (strlen(ziel) < 4 || !ordner(ziel)) { zeile(y + 1, "Zielordner kann nicht angelegt werden."); return; }
    for (i = 0; muster[i]; i++) {
        sprintf(von, "%s%s", quelle, muster[i]);
        if (_dos_findfirst(von, _A_NORMAL, &ft)) continue;
        do {
            sprintf(von, "%s%s", quelle, ft.name); sprintf(nach, "%s%s", ziel, ft.name);
            sprintf(z, "kopiere %s", ft.name); zeile(y + 1, z);
            if (kopieren(von, nach)) k++; else fehl++;
        } while (!_dos_findnext(&ft));
    }
    sprintf(nach, "%sRD2.INI", ziel);
    if ((i = ini_schreiben(nach)) != 1) fehl++;
    sprintf(z, "%d Dateien kopiert%s.", k, i < 0 ? ", RD2.INI ueber 6 KB: nicht geaendert" : fehl ? ", FEHLER" : "");
    zeile(y + 1, z);
    i = shell_eintragen(ziel, y + 2, &k);
    zeile(y + 2, i == 2 ? "CONFIG.SYS: RD2 ist schon als Shell eingetragen." : i == -2 ? "CONFIG.SYS bleibt unveraendert."
               : i == -1 ? "CONFIG.SYS ueber 6 KB: nicht geaendert." : !i ? "CONFIG.SYS: FEHLER"
               : i == 1 ? "CONFIG.SYS: RD2 ist die Shell, alte Fassung: CONFIG.RDA." : "CONFIG.SYS angelegt, RD2 ist die Shell.");
    if (i == 1 && k) zeile(y + 3, "Hinweis: SET- und PATH-Zeilen der AUTOEXEC.BAT gelten mit RD2 als Shell nicht.");
    if ((ziel[0] & 0xDF) == 'C' && i != -2) startfaehig('C', y + 4);   /* G24: nur C: startet (aktive Partition der 1. Platte) */
    strcpy(ini, nach);
}

/* ---- Liste --------------------------------------------------------------------- */
static void zeigen(int sel, int erst)
{
    char z[100]; WORD i;
    attr = 0x70; zeile(0, " RetroDesk 2 - Einrichtung"); attr = 0x07;
    if (erst) zeile(1, " Erster Start: bitte pruefen und mit S speichern."); else zeile(1, "");
    sprintf(z, " Rechner: %s, %s, DOS %d.%02d, %u KB, XMS %u KB, hoher Speicher %u KB",
            cpuname[cpu & 7], fpu ? "8087" : "kein 8087", dosv >> 8, dosv & 0xFF, konvkb, xmskb, umbkb);
    zeile(3, z);
    sprintf(z, " Maus: %s.   Netz: %s", mausinfo, pkt_int ? "Paketreiber" : "kein Paketreiber");
    if (pkt_int) sprintf(z + strlen(z), " an INT %Xh", pkt_int);
    zeile(4, z);
    for (i = 0; i < NP; i++) {
        sprintf(z, " %c %-12s %-26s %s", (int)i == sel ? 16 : ' ', p[i].titel, p[i].anzeige[p[i].v],
                p[i].v == p[i].erk ? "" : "(von Hand)");
        attr = (BYTE)((int)i == sel ? 0x70 : 0x07); zeile(6 + i, z);
    }
    attr = 0x07;
    zeile(13, " Pfeile: waehlen   Enter/Leertaste: aendern   E: neu erkennen   A: alles neu");
    zeile(14, " S: speichern   I: auf Festplatte installieren   Esc: beenden");
    sprintf(z, " Einstellungen: %s", ini); zeile(15, z);
}

int main(int argc, char **argv)
{
    int i, sel = 0, erst = 0, aut = 0, geaendert = 0; WORD k; char *s;
    for (i = 1; i < argc; i++) { if (!stricmp(argv[i], "/ERST")) erst = 1; if (!stricmp(argv[i], "/AUTO")) aut = 1; }
    strcpy(quelle, argv[0]); s = strrchr(quelle, '\\'); if (s) s[1] = 0; else quelle[0] = 0;
    sprintf(ini, "%sRD2.INI", quelle);
    { char v[80]; sprintf(v, "%sVGA16.DRV", quelle); quelle_vga16 = (WORD)(_dos_findfirst(v, _A_NORMAL, (struct find_t *)buf) == 0); }
    { char v[80]; sprintf(v, "%sVESA.DRV", quelle); quelle_vesa = (WORD)(_dos_findfirst(v, _A_NORMAL, (struct find_t *)buf) == 0); }
    erkennen(-1);
    ini_lesen(ini);
    if (aut) {
        i = ini_schreiben(ini);
        printf("RD2SETUP: %s, Grafik=%s Profil=%s Maus=%s (%s) Sound=%s SB=%s XMS=%s Netz=%s -> %s %s\n", cpuname[cpu & 7],
               p[0].wert[p[0].v], p[1].wert[p[1].v], p[2].wert[p[2].v], mauswert, p[P_KLANG].wert[p[P_KLANG].v], p[P_SB].wert[p[P_SB].v], p[P_XMS].wert[p[P_XMS].v], p[P_NETZ].wert[p[P_NETZ].v],
               ini, i ? "gespeichert" : "FEHLER");
        return i ? 0 : 1;
    }
    cls();
    for (;;) {
        zeigen(sel, erst);
        k = taste();
        if (k == T_HOCH && sel > 0) sel--;
        else if (k == T_RUNTER && sel < (int)NP - 1) sel++;
        else if (k == T_ENTER || (k & 0xFF) == ' ') { p[sel].v = (BYTE)((p[sel].v + 1) % p[sel].n); geaendert = 1; }
        else if ((k & 0xFF) == 'e' || (k & 0xFF) == 'E') { zeile(16, " erkenne ..."); erkennen(sel); zeile(16, ""); geaendert = 1; }
        else if ((k & 0xFF) == 'a' || (k & 0xFF) == 'A') { zeile(16, " erkenne alles ..."); erkennen(-1); zeile(16, ""); geaendert = 1; }
        else if ((k & 0xFF) == 's' || (k & 0xFF) == 'S') { zeile(16, ini_schreiben(ini) ? " Gespeichert." : " FEHLER beim Speichern."); geaendert = 0; }
        else if ((k & 0xFF) == 'i' || (k & 0xFF) == 'I') { for (i = 16; i < 20; i++) zeile(i, ""); installieren(); }
        else if (k == T_ESC) {
            if (geaendert || erst) { zeile(16, " Aenderungen speichern (J/N)?"); k = taste() & 0xFF; if (k == 'j' || k == 'J') ini_schreiben(ini); }
            break;
        }
    }
    cls();
    return 0;
}
