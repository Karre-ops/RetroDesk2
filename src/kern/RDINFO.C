/* RDINFO.MOD - SysInfo und BedarfCheck (API 2.3), verwerfbar: belegt nur Speicher, waehrend es gebraucht wird.
 * 20.11.2026 aus RDKX.MOD herausgezogen (Speicherrunde nach PLAN_32BIT Schritt 2). */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD2.H"
#include "RD2ORD.H"

typedef struct { WORD typ, nr, lang, flags; DWORD off, len; } RESDIR;
static short bfind(LPCSTR p, DWORD FAR *pos, WORD FAR *len)   /* FAR: Modul laeuft mit dem Stack des Aufrufers (-zu) */   /* BEDARF-Ressource im RD2-Block am Dateiende */
{
    static struct { char k[4]; DWORD off; } t;
    static struct { char sig[4]; WORD api, flags, stack, kb; char name[16]; WORD ver, nexp, nres, oexp, ores; } rb;
    static RESDIR d; WORD k; short f;
    if ((f = FileOpen(p, FO_READ | FO_DENYNONE)) < 0) return -1;
    if (FileSeek(f, -16L, 2) < 0 || FileRead(f, &t, 8) != 8 || t.k[0] != 'R' || t.k[1] != 'D' || t.k[2] != '2' || t.k[3] != '!') goto weg;
    if (FileSeek(f, (long)t.off, 0) < 0 || FileRead(f, &rb, sizeof rb) != sizeof rb) goto weg;
    if (FileSeek(f, (long)(t.off + rb.ores), 0) < 0) goto weg;
    for (k = 0; k < rb.nres; k++) {
        if (FileRead(f, &d, sizeof d) != sizeof d) break;
        if (d.typ == RT_BEDARF && d.nr == 1 && d.len < 0x100) { *pos = t.off + d.off; *len = (WORD)d.len; return f; }
    }
weg:
    FileClose(f);
    return -1;
}
/* ---- SysInfo (API 2.3): Rechner fuer Programme und die Bedarfspruefung -------------- */
static WORD xmsfrei(void)                         /* XMS-Funktion 08h: freier Speicher in KB, 0 = kein XMS */
{
    WORD kb = 0;
    _asm {
        mov  ax, 4300h
        int  2Fh
        cmp  al, 80h
        jne  ende
        push es
        mov  ax, 4310h
        int  2Fh
        push es
        push bx
        mov  bx, sp
        mov  ah, 08h
        call dword ptr ss:[bx]
        add  sp, 4
        pop  es
        mov  kb, dx
    ende:
    }
    return kb;
}
static WORD emsfrei(void)                         /* EMS: Geraet EMMXXXX0 hinter INT 67h, Funktion 42h (freie Seiten) */
{
    const char FAR *n = (const char FAR *)(((DWORD)*(WORD FAR *)0x0000019EUL << 16) | 10); WORD i, s = 0;   /* Segment von INT 67h, Name ab Offset 10 */
    static const char emm[] = "EMMXXXX0";
    for (i = 0; i < 8; i++) if (n[i] != emm[i]) return 0;
    _asm {
        mov  ah, 42h
        int  67h
        or   ah, ah
        jnz  ende
        mov  s, bx
    ende:
    }
    return s << 4;                                /* Seiten zu 16 KB */
}
static WORD RDEXPORT k_SysInfo(RDSYSDATA FAR *sd)
{
    static RDSYSDATA d; SYSINFO si; char w[12]; WORD i, n;
    static const char ini[] = "RD2.INI", sy[] = "System";
    static const char *const d3n[] = { "VOODOO1", "VOODOO2", "BANSHEE", "VOODOO3", "VOODOO4", "RAGE", "RAGE128", "ATI", "RIVA128", "TNT", "NVIDIA" };
    if (!sd || sd->groesse < 4) { SetLastError(RD_ERR_HANDLE); return RD_ERR_HANDLE; }
    KernInfo(&si);
    for (i = 0; i < sizeof d / 2; i++) ((WORD *)&d)[i] = 0;
    d.cpu = si.cpu; d.fpu = si.fpu; d.dos = si.dos; d.kb_konv = si.kb_konv; d.grafik = si.grafik; d.maus = si.maus;
    d.kb_frei = (WORD)(MemAvail(MEMA_GROESSTER) >> 10);
    d.kb_xms = xmsfrei(); d.kb_ems = emsfrei();
    d.festplatte = (si.home[0] | 0x20) >= 'c';
    ProfileGetStr(ini, sy, "Sound", "SPEAKER", w, sizeof w);
    d.sound = SC_SPEAKER | ((w[0] | 0x20) == 'a' ? SC_OPL2 : (w[0] | 0x20) == 's' && (w[1] | 0x20) == 'b' ? SC_OPL2 | SC_PCM : (w[0] | 0x20) == 'c' ? SC_PCM : 0);
    ProfileGetStr(ini, sy, "Netz", "KEIN", w, sizeof w); d.netz = (w[0] | 0x20) == 'p';
    ProfileGetStr(ini, sy, "3D", "KEINE", w, sizeof w);
    for (i = 0; i < 11; i++) { const char *a = d3n[i]; for (n = 0; a[n] && (a[n] | 0x20) == (w[n] | 0x20); n++) ; if (!a[n] && !w[n]) d.d3 = i + 1; }
    ProfileGetStr(ini, sy, "3DPCI", "", w, sizeof w);   /* "121A:0002" (Hersteller:Geraet, hex) */
    for (i = 0, n = 0; w[i] && i < 9; i++) {
        BYTE c = (BYTE)(w[i] | 0x20);
        if (c == ':') { d.d3_hersteller = n; n = 0; continue; }
        n = (n << 4) | (c <= '9' ? c - '0' : c - 'a' + 10);
    }
    if (d.d3_hersteller) d.d3_geraet = n;
    n = sd->groesse > sizeof d ? sizeof d : sd->groesse;
    for (i = 2; i < n; i++) ((BYTE FAR *)sd)[i] = ((BYTE *)&d)[i];
    return 0;
}
/* BedarfCheck (API 2.3): BEDARF-Ressource einer Programmdatei gegen SysInfo; BD_* was fehlt */
static WORD farbstufe(WORD g)
{
    if (g == GFX_VESA || g == GFX_MCGA || g == GFX_VESA800 || g == GFX_VESA1024) return 3;
    if (g == GFX_EGA || g == GFX_VGA16 || g == GFX_EGA320 || g == GFX_EGA200 || g == GFX_SVGA16) return 2;
    if (g == GFX_CGA4 || g == GFX_EGA4 || g == GFX_VGA4) return 1;
    return 0;
}
static WORD RDEXPORT k_BedarfCheck(LPCSTR datei)
{
    static RDBEDARF b; static RDSYSDATA s; DWORD pos; WORD len, i, m = 0; short f;
    if (!datei) return 0;
    if ((f = bfind(datei, &pos, &len)) < 0) return 0;     /* kein BEDARF: laeuft ueberall */
    for (i = 0; i < sizeof b; i++) ((BYTE *)&b)[i] = 0;
    if (len > sizeof b) len = sizeof b;
    if (FileSeek(f, (long)pos, 0) < 0 || FileRead(f, &b, len) != len) len = 0;
    FileClose(f);
    if (!len || b.version != 1) return 0;
    s.groesse = sizeof s; k_SysInfo(&s);
    if (s.cpu < b.cpu) m |= BD_CPU;
    if (b.fpu && !s.fpu) m |= BD_FPU;
    if (farbstufe(s.grafik) < b.farben) m |= BD_FARBEN;
    if (s.kb_frei < b.kb_ram) m |= BD_RAM;
    if (s.kb_xms < b.kb_xms) m |= BD_XMS;
    if ((s.sound & b.klang) != b.klang) m |= BD_KLANG;
    if (b.netz && !s.netz) m |= BD_NETZ;
    if ((b.d3 == 2 && !s.d3) || (b.d3 == 1 && s.cpu < CPU_386)) m |= BD_3D;   /* Software-3D ab 386 */
    return m;
}

static void RDEXPORT ende(void) { }
static struct { WORD n; FARPROC f[3]; } ext = { 3, { (FARPROC)ende, (FARPROC)k_SysInfo, (FARPROC)k_BedarfCheck } };
void FAR * RDAPI RdModInit(void) { return &ext; }
