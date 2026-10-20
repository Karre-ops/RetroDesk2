/* GFXWAHL.C - Test fuer GfxList/GfxSelect (spaeter Menue "Grafik").
 * Aufruf: GFXWAHL           Liste der moeglichen Grafikarten auf COM1/Protokoll
 *         GFXWAHL c|h|v|f|a  zusaetzlich CGA / Hercules / VGA mono / VGA 16 Farben / automatisch waehlen */
#include "RD2.H"

static GFXINFO l[8];
static char z[60], ini[12];
static SYSINFO si;

static void zeile(const GFXINFO *e)
{
    WORD k = 0; const char *q = "GFXWAHL: ";
    while (*q) z[k++] = *q++;
    for (q = e->name; *q; ) z[k++] = *q++;
    if (e->flags & GI_AKTIV) { for (q = " [aktiv]"; *q; ) z[k++] = *q++; }
    if (e->flags & GI_GEWAEHLT) { for (q = " [eingestellt]"; *q; ) z[k++] = *q++; }
    if (e->flags & GI_STANDARD) { for (q = " [automatisch]"; *q; ) z[k++] = *q++; }
    z[k] = 0;
    DbgPrint(z);
}

short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD n, i, typ = 0xFFFF, r;
    (void)inst; (void)show;
    KernInfo(&si);
    DbgHex("GFXWAHL: CPU", si.cpu);
    n = GfxList(l, 8);
    DbgHex("GFXWAHL: Anzahl", n);
    for (i = 0; i < n; i++) zeile(&l[i]);
    if (cmd) switch (*cmd | 0x20) {
        case 'c': typ = GFX_CGA; break;
        case 'h': typ = GFX_HGC; break;
        case 'v': typ = GFX_VGAMONO; break;
        case 'f': typ = GFX_VGA16; break;
        case 'a': typ = 0; break;
    }
    if (typ != 0xFFFF) {
        r = GfxSelect(typ);
        DbgHex("GFXWAHL: Auswahl, Fehler", r);
        ProfileGetStr("RD2.INI", "System", "Grafik", "(leer)", ini, sizeof ini);
        { WORD k = 0; const char *q = "GFXWAHL: RD2.INI Grafik="; while (*q) z[k++] = *q++; for (q = ini; *q; ) z[k++] = *q++; z[k] = 0; DbgPrint(z); }
    }
    return 0;
}
