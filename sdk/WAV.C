/* WAV.C - SndWavLoad (RD2.LIB): WAV-Datei laden, nur PCM 8 Bit mono. Liefert einen beweglichen Block mit den reinen
 * Samples (data-Teil) fuer SndSamplePlay und die Rate aus dem Kopf. Fehler: 0, LastError RD_ERR_NOTFOUND (Datei),
 * RD_ERR_FORMAT (kein RIFF/WAVE, 16 Bit, Stereo, komprimiert), RD_ERR_NOMEM. Laeuft im Programm (RDSND bleibt klein).
 * Uebersetzt mit -zu: auch aus Modulen (SS != DS) aufrufbar, alle Zeiger far. */
#include <i86.h>
#include "RD2.H"

static DWORD dw(const BYTE FAR *p) { return p[0] | ((WORD)p[1] << 8) | ((DWORD)(p[2] | ((WORD)p[3] << 8)) << 16); }
static BYTE id(const BYTE FAR *p, const char FAR *t) { return p[0] == t[0] && p[1] == t[1] && p[2] == t[2] && p[3] == t[3]; }
HMEM RDAPI SndWavLoad(LPCSTR name, WORD FAR *rate)
{
    BYTE k[16]; DWORD n, pos = 12; short f; WORD e = RD_ERR_FORMAT, r = 0; HMEM h = 0; BYTE FAR *p;
    BYTE FAR *kp = (BYTE FAR *)k;
    if ((f = FileOpen(name, 0)) < 0) { SetLastError(RD_ERR_NOTFOUND); return 0; }
    if (FileRead(f, kp, 12) != 12 || !id(kp, "RIFF") || !id(kp + 8, "WAVE")) goto aus;
    for (;;) {
        if (FileSeek(f, pos, 0) != (long)pos || FileRead(f, kp, 8) != 8) goto aus;
        n = dw(kp + 4); pos += 8;
        if (id(kp, "fmt ")) {
            if (n < 16 || FileRead(f, kp, 16) != 16) goto aus;
            if (kp[0] != 1 || kp[1] || kp[2] != 1 || kp[3] || kp[14] != 8) goto aus;   /* PCM, mono, 8 Bit */
            r = (WORD)dw(kp + 4);
        } else if (id(kp, "data")) {
            if (!r || !n) goto aus;
            if (!(h = MemAlloc(n, MEM_MOVABLE))) { e = RD_ERR_NOMEM; goto aus; }
            p = (BYTE FAR *)MemLock(h);
            while (n) {                           /* in Stuecken bis 32 KB, Zeiger normalisiert */
                WORD s = n > 0x8000UL ? 0x8000 : (WORD)n, g = FileRead(f, p, s);
                if (g != s) { MemUnlock(h); MemFree(h); h = 0; goto aus; }
                n -= s; { DWORD o = (DWORD)FP_OFF(p) + s; p = (BYTE FAR *)MK_FP(FP_SEG(p) + (WORD)(o >> 4), (WORD)o & 15); }
            }
            MemUnlock(h);
            if (rate) *rate = r;
            e = 0; goto aus;
        }
        pos += n + (n & 1);
    }
aus:
    FileClose(f);
    if (e) SetLastError(e);
    return e ? 0 : h;
}
