/* DKOPIE.EXE - Diskette kopieren (wie DISKCOPY): Quelle und Ziel A: oder B:, auch dasselbe Laufwerk (Wechsel
 * mit Rueckfrage). Format aus dem Bootsektor (BPB), Spuren ueber INT 13h (RDDISK.MOD, Positivliste), so viele
 * Zylinder je Durchgang wie Speicher frei ist. Die Zieldiskette muss im gleichen Format formatiert sein.
 * Befehlszeile "T": Testmodus (Quelle = Ziel = B:, test/dkopie.sh). */
#include <dos.h>
#include "RD2.H"
#include "../../disk/RDDISK.H"

#define ID_VA 31
#define ID_VB 32
#define ID_NA 33                                  /* A: nach B: */
#define ID_NB 34                                  /* B: nach A: */
#define ID_BALKEN 41
#define ID_TEXT 42
#define MAXB 40                                   /* Zylinderpuffer */
static HINST hi; static RDD_INT_F dInt; static HMEM hb[MAXB]; static WORD nb, spt, kopf, zyl, zgr;
static BYTE von, nach, test; static char FAR *bounce; static HMEM hbo;

static WORD i13(WORD ax, WORD cx, WORD dx, void FAR *p)   /* 0 = ok, sonst BIOS-Fehler (AH) */
{
    union REGS r; struct SREGS s; WORD v;
    r.x.ax = ax; r.x.bx = FP_OFF(p); r.x.cx = cx; r.x.dx = dx; s.es = FP_SEG(p); s.ds = 0;
    for (v = 0; v < 3; v++) {
        dInt(0x13, &r, &r, &s);
        if (!r.x.cflag) return 0;
        if ((r.x.ax >> 8) == 9) return 9;             /* DMA-Grenze: Aufrufer liest einzeln */
        r.x.ax = 0; r.x.dx = dx & 0xFF; dInt(0x13, &r, &r, &s);   /* Laufwerk zuruecksetzen, nochmal */
        r.x.ax = ax; r.x.bx = FP_OFF(p); r.x.cx = cx; r.x.dx = dx; s.es = FP_SEG(p);
    }
    return (WORD)(r.x.ax >> 8 ? r.x.ax >> 8 : 0xFF);
}
static WORD spur(BYTE schreib, BYTE lw, WORD c, WORD h, char FAR *p)   /* eine Spur, bei DMA-Grenze sektorweise */
{
    WORD e, s, k;
    e = i13((WORD)((schreib ? 0x0300 : 0x0200) | spt), (WORD)((c << 8) | ((c >> 2) & 0xC0) | 1), (WORD)((h << 8) | lw), p);
    if (e != 9) return e;
    for (s = 1; s <= spt; s++) {
        char FAR *q = p + (s - 1) * 512;
        if (schreib) for (k = 0; k < 512; k++) bounce[k] = q[k];
        if ((e = i13((WORD)(schreib ? 0x0301 : 0x0201), (WORD)((c << 8) | ((c >> 2) & 0xC0) | s), (WORD)((h << 8) | lw), bounce)) != 0) return e;
        if (!schreib) for (k = 0; k < 512; k++) q[k] = bounce[k];
    }
    return 0;
}
static void text(HWND d, WORD n) { DlgSetText(d, ID_TEXT, StrGet(hi, n)); }
static BYTE wechsel(HWND d, WORD n)               /* Diskette einlegen lassen; 0 = Abbruch */
{
    DbgPrint(n == 3 ? "DKOPIE: Quelle einlegen" : "DKOPIE: Ziel einlegen");
    return MsgBox(d, StrGet(hi, n), StrGet(hi, 1), MB_OKCANCEL | MB_INFO) == ID_OK;
}
static void kopieren(HWND d)
{
    char FAR *p; WORD c, c0, n, h, e = 0, i; BYTE erst = 1;
    for (i = 0; i < nb; i++) if (hb[i]) { MemFree(hb[i]); hb[i] = 0; } nb = 0;
    if (!wechsel(d, 3)) return;
    text(d, 5);
    if ((e = i13(0x0201, 1, von, bounce)) != 0) { text(d, 6); DbgHex("DKOPIE: Bootsektor Fehler", e); return; }
    spt = *(WORD FAR *)(bounce + 24); kopf = *(WORD FAR *)(bounce + 26); n = *(WORD FAR *)(bounce + 19);
    if (spt < 8 || spt > 36 || kopf < 1 || kopf > 2 || !n) { text(d, 6); DbgPrint("DKOPIE: kein gueltiges Format"); return; }
    zyl = (WORD)(n / (spt * kopf)); zgr = (WORD)(spt * kopf * 512);
    for (nb = 0; nb < MAXB && nb < zyl && (hb[nb] = MemAlloc(zgr, MEM_MOVABLE)) != 0; nb++) ;
    if (!nb) { text(d, 7); return; }
    DbgHex("DKOPIE: Zylinder", zyl); DbgHex("DKOPIE: je Durchgang", nb);
    SendMessage(DlgItem(d, ID_BALKEN), C_SETRANGE, 0, zyl * 2);
    for (c0 = 0; c0 < zyl && !e; c0 += nb) {
        n = zyl - c0 < nb ? zyl - c0 : nb;
        if (!erst && von == nach && !wechsel(d, 3)) return;
        erst = 0; CursorBusy(TRUE); text(d, 5);
        for (c = 0; c < n && !e; c++) {
            if (!(p = (char FAR *)MemLock(hb[c]))) { e = 0xFE; break; }
            for (h = 0; h < kopf && !e; h++) e = spur(0, von, c0 + c, h, p + h * spt * 512);
            MemUnlock(hb[c]); SendMessage(DlgItem(d, ID_BALKEN), C_SETPOS, 0, (c0 + c) * 2 + 1);
        }
        CursorBusy(FALSE);
        if (e) { text(d, 6); break; }
        if (von == nach && !wechsel(d, 4)) return;
        CursorBusy(TRUE); text(d, 8);
        for (c = 0; c < n && !e; c++) {
            if (!(p = (char FAR *)MemLock(hb[c]))) { e = 0xFE; break; }
            for (h = 0; h < kopf && !e; h++) e = spur(1, nach, c0 + c, h, p + h * spt * 512);
            MemUnlock(hb[c]); SendMessage(DlgItem(d, ID_BALKEN), C_SETPOS, 0, (c0 + c) * 2 + 2);
        }
        CursorBusy(FALSE);
        if (e) { text(d, e == 3 ? 10 : 9); break; }
    }
    for (i = 0; i < nb; i++) if (hb[i]) { MemFree(hb[i]); hb[i] = 0; } nb = 0;
    if (e) { DbgHex("DKOPIE: Fehler", e); return; }
    text(d, 11); DbgPrint("DKOPIE: fertig");
}
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    (void)p2;
    if (msg == M_INITDIALOG) {
        DlgCheck(d, test ? ID_VB : ID_VA, 1); text(d, 2);
        if (test) PostMessage(d, M_COMMAND, ID_OK, 0);
        return FALSE;
    }
    if (msg == M_COMMAND && p1 == ID_OK) {
        von = (BYTE)(DlgChecked(d, ID_VB) || DlgChecked(d, ID_NB)); nach = (BYTE)(DlgChecked(d, ID_VB) || DlgChecked(d, ID_NA));   /* 31 A-A, 32 B-B, 33 A-B, 34 B-A */
        kopieren(d);
        if (test) DialogEnd(d, ID_CANCEL);
        return TRUE;
    }
    return FALSE;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    HMOD hm; char FAR *b;
    (void)show; hi = inst; test = (BYTE)(cmd && cmd[0] == 'T');
    if (!(hm = ModLoad("RDDISK.MOD"))) { MsgBox(0, StrGet(inst, 12), StrGet(inst, 1), MB_OK | MB_ERROR); return 1; }
    dInt = (RDD_INT_F)ModProc(hm, RDD_INT);
    if (!(hbo = MemAlloc(1024, 0)) || !(b = (char FAR *)MemLock(hbo))) return 1;
    bounce = ((FP_OFF(b) + (DWORD)FP_SEG(b) * 16) & 0xFFFF) > 0xFE00 ? b + 512 : b;   /* 512 Bytes ohne 64-KB-Grenze */
    DialogRun(inst, 10, 0, dproc, 0);
    MemUnlock(hbo); MemFree(hbo); ModFree(hm);
    return 0;
}
