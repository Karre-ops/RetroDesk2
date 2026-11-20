/* VOLL.C - ScreenMode/ScreenPalette/ScreenRestore/ScreenBank in RD2.LIB (Vollbild, API 2.3, PLAN_32BIT Schritt 2).
 * Laufen im Programm: Kern und Module wachsen nicht. Der Schirm muss mit ScreenAcquire geholt sein.
 * Kein Abbild im RAM: nach M_SCREENBACK stellt ScreenRestore Modus und Palette her, das Bild zeichnet das Programm neu.
 * Uebersetzt mit -zu: alle Zeiger far. */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include <conio.h>
#include <i86.h>
#include "RD2.H"
#pragma intrinsic(inp, outp)

static WORD akt = 0xFFFF;                         /* gemerkter Modus */
static BYTE pal[768]; static WORD palv, palb;     /* gemerkte Palette: Eintraege palv .. palb-1 */

static void bios(WORD ax, WORD bx) { union REGS r; r.x.ax = ax; r.x.bx = bx; int86(0x10, &r, &r); }
static void modex(void)                           /* 13h -> 320x240 unverkettet (Michael Abrash) */
{
    static const WORD crtc[] = { 0x0D06, 0x3E07, 0x4109, 0xEA10, 0xAC11, 0xDF12, 0x0014, 0xE715, 0x0616, 0xE317 };
    WORD i; BYTE FAR *v = (BYTE FAR *)MK_FP(0xA000, 0);
    outpw(0x3C4, 0x0604); outpw(0x3C4, 0x0100); outp(0x3C2, 0xE3); outpw(0x3C4, 0x0300);
    outp(0x3D4, 0x11); outp(0x3D5, (BYTE)(inp(0x3D5) & 0x7F));
    for (i = 0; i < sizeof crtc / 2; i++) outpw(0x3D4, crtc[i]);
    outpw(0x3C4, 0x0F02);
    for (i = 0; i < 0xFFFF; i++) v[i] = 0;
    v[0xFFFF] = 0;
}
WORD RDAPI ScreenMode(WORD modus, SCREENINFO FAR *si)
{
    static const WORD bm[] = { 3, 4, 0x0D, 0x12, 0x13, 0x13 };
    static const WORD tw[][6] = { { 80, 25, 4, 160, 0xB800, 1 }, { 320, 200, 2, 80, 0xB800, 1 }, { 320, 200, 4, 40, 0xA000, 4 },
                                  { 640, 480, 4, 80, 0xA000, 4 }, { 320, 200, 8, 320, 0xA000, 1 }, { 320, 240, 8, 80, 0xA000, 4 },
                                  { 640, 480, 8, 640, 0xA000, 1 } };
    WORD i;
    if (modus > SM_VESA) { SetLastError(RD_ERR_NOTSUP); return 0; }
    if (modus == SM_VESA) {
        union REGS r; r.x.ax = 0x4F02; r.x.bx = 0x101; int86(0x10, &r, &r);
        if (r.x.ax != 0x004F) { SetLastError(RD_ERR_NOTSUP); return 0; }
    } else bios(bm[modus], 0);
    if (modus == SM_X) modex();
    if (akt != modus) palv = palb = 0;            /* neuer Modus: alte Palette gilt nicht mehr */
    akt = modus;
    if (si) { si->modus = modus; si->w = tw[modus][0]; si->h = tw[modus][1]; si->bpp = tw[modus][2];
              si->zeilenbytes = tw[modus][3]; si->seg = tw[modus][4]; si->ebenen = tw[modus][5]; }
    for (i = palv; i < palb; i++) { outp(0x3C8, (BYTE)i); outp(0x3C9, pal[i * 3]); outp(0x3C9, pal[i * 3 + 1]); outp(0x3C9, pal[i * 3 + 2]); }
    return 1;
}
void RDAPI ScreenPalette(WORD erste, WORD n, const BYTE FAR *rgb)
{
    WORD i;
    if (erste > 255) return;
    if (erste + n > 256) n = 256 - erste;
    for (i = 0; i < n * 3; i++) pal[erste * 3 + i] = (BYTE)(rgb[i] & 63);
    if (palb == palv) { palv = erste; palb = erste + n; }
    else { if (erste < palv) palv = erste; if (erste + n > palb) palb = erste + n; }
    for (i = erste; i < erste + n; i++) { outp(0x3C8, (BYTE)i); outp(0x3C9, pal[i * 3]); outp(0x3C9, pal[i * 3 + 1]); outp(0x3C9, pal[i * 3 + 2]); }
}
void RDAPI ScreenRestore(void) { if (akt != 0xFFFF) ScreenMode(akt, 0); }
void RDAPI ScreenBank(WORD bank) { union REGS r; r.x.ax = 0x4F05; r.x.bx = 0; r.x.dx = bank; int86(0x10, &r, &r); }
