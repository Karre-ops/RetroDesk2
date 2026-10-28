/* TASKMAN.EXE - Taskmanager (Zubehoer, RD-Menue): laufende Programme mit belegtem Speicher, freier Speicher,
 * Beenden (Rueckfrage; SHELL nur mit zweiter Warnung), Aktualisieren. */
#include "RD2.H"

#define NMAX 16
#define ID_LISTE 40
#define ID_FREI  41
#define ID_ENDE  21
#define ID_NEU   22
static HINST hi; static HTASK tt[NMAX]; static char tn[NMAX][16]; static WORD n;

static WORD slen(LPCSTR s) { WORD k = 0; while (s[k]) k++; return k; }
static char *zahl(char *p, DWORD v) { char t[11]; short i = 0; do t[i++] = (char)('0' + v % 10); while (v /= 10); while (i) *p++ = t[--i]; *p = 0; return p; }
static BYTE gleich(LPCSTR a, LPCSTR b) { while (*a && (*a | 0x20) == (*b | 0x20)) a++, b++; return (BYTE)(*a == *b); }
static void fuellen(HWND d)
{
    TASKINFO ti; HTASK t; HWND l = DlgItem(d, ID_LISTE); char z[40], *p; WORD k; HTASK ich = TaskSelf();
    SendMessage(l, L_CLEAR, 0, 0); n = 0;
    for (t = TaskFirst(&ti); t && n < NMAX; t = TaskNext(t, &ti)) {
        if (t == ich) continue;
        for (k = 0; k < 15 && ti.name[k]; k++) tn[n][k] = ti.name[k]; tn[n][k] = 0;
        p = z; for (k = 0; tn[n][k]; k++) *p++ = tn[n][k];
        while (p < z + 14) *p++ = ' ';
        p = zahl(p, ti.kb); *p++ = ' '; *p++ = 'K'; *p++ = 'B'; *p = 0;
        tt[n++] = t; SendMessage(l, L_ADD, 0, (DWORD)(LPCSTR)z);
    }
    if (n) SendMessage(l, L_SETSEL, 0, 0);
    p = z; for (k = 0; StrGet(hi, 4)[k] && k < 20; k++) *p++ = StrGet(hi, 4)[k];
    p = zahl(p, MemAvail(MEMA_KONV) >> 10); *p++ = ' '; *p++ = 'K'; *p++ = 'B'; *p = 0;
    DlgSetText(d, ID_FREI, z);
    DbgHex("TASKMAN: Programme", n);
}
static BOOL RDCALLBACK dproc(HWND d, WORD msg, WORD p1, DWORD p2)
{
    WORD i; char z[80], *p; LPCSTR s;
    (void)p2;
    if (msg == M_INITDIALOG) { fuellen(d); SetFocus(DlgItem(d, ID_LISTE)); return TRUE; }
    if (msg != M_COMMAND) return FALSE;
    if (p1 == ID_NEU) { fuellen(d); return TRUE; }
    if (p1 == ID_ENDE) {
        i = (WORD)SendMessage(DlgItem(d, ID_LISTE), L_GETSEL, 0, 0);
        if (i >= n) return TRUE;
        p = z; for (s = StrGet(hi, 2); *s && p < z + 60; ) *p++ = *s++;
        for (s = tn[i]; *s; ) *p++ = *s++; *p++ = '?'; *p = 0;
        if (MsgBox(d, z, StrGet(hi, 1), MB_YESNO | MB_WARN | MB_DEFNO) != ID_YES) return TRUE;
        if ((gleich(tn[i], "SHELL") || gleich(tn[i], "SHELL.EXE")) && MsgBox(d, StrGet(hi, 3), StrGet(hi, 1), MB_YESNO | MB_WARN | MB_DEFNO) != ID_YES) return TRUE;
        TaskKill(tt[i], 0xFFFD);
        DbgPrint("TASKMAN: beendet"); DbgPrint(tn[i]);
        TaskYield(); fuellen(d); return TRUE;
    }
    (void)slen;
    return FALSE;
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    (void)show; hi = inst;
    if (cmd && cmd[0] == 'T') {                   /* Testmodus (test/taskman.sh): erst UHR.EXE starten */
        SYSINFO si; char p[80]; WORD k = 0; LPCSTR s;
        KernInfo(&si); for (s = si.home; *s && k < 66; ) p[k++] = *s++;
        if (k && p[k - 1] != '\\') p[k++] = '\\';
        for (s = "UHR.EXE"; *s; ) p[k++] = *s++; p[k] = 0;
        TaskExec(p, "", SW_SHOW); for (k = 0; k < 300; k++) TaskYield();   /* UHR soll ihr Fenster vor dem Dialog haben */
    }
    DialogRun(inst, 10, 0, dproc, 0);
    return 0;
}
