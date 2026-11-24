/* VESA16.C - VESA fuer 16-Bit-Programme in RD2.LIB (PLAN_API Luecke 3, 24.11.2026): ScreenModeVesa waehlt einen Modus nach
 * Groesse und Farbtiefe (8, 15, 16, 24, 32 Bit), ScreenVesaList listet die Modi der Karte, ScreenPut kopiert ein Rechteck
 * aus dem Programm auf den Schirm und schaltet die 64-KB-Baenke selbst. Im Realmodus gibt es keinen linearen Bildspeicher:
 * immer Fenster A (si.seg), ScreenBank zaehlt in 64 KB. Eigene Datei: nur Programme, die es nutzen, binden es ein. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include <i86.h>
#include "RD2.H"

extern WORD rd2_vnr, rd2_vgran, rd2_vseg, rd2_vzb, rd2_vh, rd2_vbpp;
WORD rd2_vinfo(WORD nr, BYTE FAR * FAR *mi);
WORD rd2_vesaset(WORD nr);
static BYTE vbe[512];
static WORD liste(WORD FAR *l, WORD max)          /* VBE 4F00: Modusnummern */
{
    static union REGS r; static struct SREGS sr; WORD FAR *q; WORD n = 0;
    segread(&sr); vbe[0] = 'V'; vbe[1] = 'B'; vbe[2] = 'E'; vbe[3] = '2';
    r.x.ax = 0x4F00; sr.es = FP_SEG(vbe); r.x.di = FP_OFF(vbe); int86x(0x10, &r, &r, &sr);
    if (r.x.ax != 0x004F || vbe[0] != 'V' || vbe[1] != 'E' || vbe[5] < 1) return 0;
    q = (WORD FAR *)MK_FP(*(WORD FAR *)(vbe + 16), *(WORD FAR *)(vbe + 14));
    while (n < max && *q != 0xFFFF) l[n++] = *q++;
    return n;
}
static WORD tiefe(BYTE FAR *mi)                    /* 8, 15, 16, 24, 32; 0 = passt nicht (Grafik, Farbe, Fenster A beschreibbar) */
{
    if ((mi[0] & 0x1B) != 0x1B || (mi[2] & 5) != 5 || !*(WORD FAR *)(mi + 4) || *(WORD FAR *)(mi + 6) < 64) return 0;
    if (mi[0x19] == 8 && mi[0x1B] == 4) return 8;
    if (mi[0x1B] != 6) return 0;
    if (mi[0x19] == 16 && mi[0x21] == 5) return 15;
    return mi[0x19] == 15 || mi[0x19] == 16 || mi[0x19] == 24 || mi[0x19] == 32 ? mi[0x19] : 0;
}
WORD RDAPI ScreenVesaList(VESAMODE FAR *l, WORD max)
{
    static WORD nr[128]; WORD n = liste(nr, 128), i, k = 0, b; BYTE FAR *mi;
    for (i = 0; i < n && k < max; i++)
        if (rd2_vinfo(nr[i], &mi) && (b = tiefe(mi)) != 0) { l[k].nr = nr[i]; l[k].w = *(WORD FAR *)(mi + 0x12); l[k].h = *(WORD FAR *)(mi + 0x14); l[k].bpp = b; k++; }
    return k;
}
WORD RDAPI ScreenModeVesa(WORD w, WORD h, WORD bpp, SCREENINFO FAR *si)
{
    static WORD nr[128]; WORD n = liste(nr, 128), i; BYTE FAR *mi;
    for (i = 0; i < n; i++)
        if (rd2_vinfo(nr[i], &mi) && *(WORD FAR *)(mi + 0x12) == w && *(WORD FAR *)(mi + 0x14) == h && tiefe(mi) == bpp) break;
    if (i == n || !rd2_vesaset(nr[i])) { SetLastError(RD_ERR_NOTSUP); return 0; }
    rd2_vbpp = bpp;
    if (si) { si->modus = SM_VESA; si->w = w; si->h = h; si->bpp = bpp; si->zeilenbytes = rd2_vzb; si->seg = rd2_vseg; si->ebenen = 1; }
    return 1;
}
/* Rechteck b x h Bildpunkte aus q (qzeile Bytes je Zeile) nach x, y; Bytes je Punkt nach der Farbtiefe des Modus */
void RDAPI ScreenPut(WORD x, WORD y, WORD b, WORD h, const BYTE FAR *q, WORD qzeile)
{
    WORD bp = (rd2_vbpp + 7) >> 3, j, n, bank = 0xFFFF; DWORD off; BYTE FAR *z;
    if (rd2_vbpp == 15) bp = 2;
    if (!rd2_vzb) return;
    for (j = 0; j < h; j++, q += qzeile) {
        DWORD o = (DWORD)(y + j) * rd2_vzb + (DWORD)x * bp; WORD rest = b * bp; const BYTE FAR *s = q;
        while (rest) {
            off = o & 0xFFFF; n = (WORD)(0x10000UL - off < rest ? 0x10000UL - off : rest);
            if ((WORD)(o >> 16) != bank) { bank = (WORD)(o >> 16); ScreenBank(bank); }
            z = (BYTE FAR *)MK_FP(rd2_vseg, (WORD)off);
            { WORD k; for (k = 0; k < n; k++) z[k] = s[k]; }
            s += n; o += n; rest -= n;
        }
    }
}
