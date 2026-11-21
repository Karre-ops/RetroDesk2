/* RD2_32.H - RD2/32-Programme (32 Bit, flach, ab 386), PLAN_32BIT Stufe 4b.
 * SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
 * Die ganze API aus RD2.H gilt auch in 32 Bit (Zeiger sind 32-Bit-Zeiger, die Bibliothek bringt Daten fuer den Kern
 * unter 1 MB). Bauen: build.sh app32 (wcc386 -mf, RD32C0 zuerst, RD32LIB, tools/rd32link.py).
 * Dazu nur in 32 Bit: lineare Adressen, DPMI-Speicher, Puffer-Bitmap fuer schnelle Grafik im Fenster, VESA 2.0 linear. */
#ifndef RD2_32_H
#define RD2_32_H
#include "RD2.H"

void *rd32_lin(DWORD lin);                        /* lineare Adresse (z. B. 0xA0000) als Zeiger */
void *rd32_alloc(DWORD n);                        /* DPMI-Speicher (XMS), 0 = keiner; endet das Programm, wird er frei */
void  rd32_free(void *p);

/* Puffer-Bitmap: das Programm schreibt 8-Bit-Farbnummern (C_* bzw. Palette) direkt in pix, BufShow bringt sie in
   einem Aufruf ins Fenster (Geraeteformat 1, 4 oder 8 Bit, in Streifen zu hoechstens 64 KB). */
typedef struct { WORD w, h; BYTE *pix; HBMP bmp; WORD streifen; } RDBUF;
BOOL  BufCreate(RDBUF *b, WORD w, WORD h);        /* pix aus rd32_alloc, w*h Bytes */
void  BufShow(RDBUF *b, HDC dc, short x, short y);
void  BufFree(RDBUF *b);

/* Vollbild: ScreenMode wie RD2.LIB (SM_TEXT, SM_13H) und SM_LFB; ScreenPtr = Bildspeicher als Zeiger */
#define SM_LFB 7                                  /* 640x480 256 Farben, VESA 2.0 linearer Bildspeicher (nur 32 Bit) */
BYTE *ScreenPtr(void);
#endif
