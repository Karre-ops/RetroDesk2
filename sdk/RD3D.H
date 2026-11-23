/* RD3D.H - 3D fuer RD2/32-Programme (PLAN_32BIT Schritt 5, ab 386), Stufe 5a: Software-Treiber.
 * SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
 * Schlanke API: Dreiecke mit Z-Puffer, Textur, Gouraud-Schattierung, Nebel und Alpha (50 % Raster), Doppelpuffer.
 * Laeuft im Vollbild (SM_13H, SM_LFB, VESA linear in jeder Groesse und Farbtiefe 8/15/16/24/32) und im Fenster
 * (Puffer-Bitmap). Ganzzahlig (16.16), braucht keinen 387.
 * Farben: 256 Eintraege = 16 Rampen (R3dRampe) zu je 16 Helligkeiten. Helligkeit 0 ist die Nebelfarbe, 15 die volle
 * Farbe. Texel = Rampe * 16 + Helligkeit. Im Fenster wird auf die feste Systempalette abgebildet, in 15/16/24/32 Bit
 * beim Zeigen ueber eine Tabelle in Echtfarben umgesetzt (die 256 Farben ohne Palette, 23.11.2026).
 * Treiber austauschbar (R3DDRV): Software jetzt; Glide (3dfx) und eigene Treiber folgen (PLAN_32BIT). */
#ifndef RD3D_H
#define RD3D_H
#include "RD2_32.H"

typedef long FIX;                                 /* 16.16 */
#define FX(n)      ((FIX)((n) * 65536L))
#define FXMUL(a,b) ((FIX)(((__int64)(a) * (b)) >> 16))
#define FXDIV(a,b) ((FIX)((((__int64)(a)) << 16) / (b)))

typedef struct { FIX x, y, z, u, v, s; } R3DVERT;  /* x,y Bildpunkte, z 0..FX(1) (nah..fern), u,v Texel, s Helligkeit 0..15 */
typedef struct { FIX x, y, z; } R3DVEC;
typedef struct { FIX m[3][4]; } R3DMAT;            /* Drehung 3x3 und Verschiebung */

#define R3D_VOLL13  1                             /* Vollbild 320x200 (13h) */
#define R3D_VOLLLFB 2                             /* Vollbild 640x480, VESA 2.0 linear */
#define R3D_FENSTER 3                             /* in ein Fenster (BufShow) */
#define R3D_VESA    4                             /* Vollbild VESA linear, Groesse und Farbtiefe frei (R3dOpenVesa) */

#define R3S_ZTEST   0x0001                        /* Z-Puffer pruefen */
#define R3S_ZWRITE  0x0002                        /* Z-Puffer schreiben */
#define R3S_TEXTUR  0x0004                        /* Textur (sonst Farbe aus R3dFarbe) */
#define R3S_GOURAUD 0x0008                        /* Helligkeit je Ecke (sonst s der ersten Ecke) */
#define R3S_NEBEL   0x0010                        /* Nebel nach Tiefe (R3dNebel) */
#define R3S_ALPHA   0x0020                        /* halb durchsichtig (Raster) */

typedef struct R3D R3D;
typedef struct {                                  /* 3D-Treiber */
    const char *name;
    int  (*open)(R3D *r);
    void (*close)(R3D *r);
    void (*clear)(R3D *r, BYTE farbe);
    void (*tri)(R3D *r, const R3DVERT *a, const R3DVERT *b, const R3DVERT *c);
    void (*flip)(R3D *r);
} R3DDRV;
struct R3D {
    WORD art, w, h; HWND fenster; short fx, fy;   /* Fenster: Ziel im Client-Bereich */
    BYTE *bild; WORD *z; BYTE *schirm; WORD zeile; /* Rueckpuffer, Z-Puffer, Vollbild-Speicher */
    WORD zustand; BYTE farbe; const BYTE *tex; BYTE tl, tm;   /* Textur 2^tl x 2^tl, tm = Maske */
    FIX nah, fern; BYTE nebel[256];               /* Nebel: Faktor je Tiefe (z >> 8) */
    BYTE pal[768], rampe[16][3], nf[3];           /* Palette (0..63), Rampen, Nebelfarbe (RGB) */
    BYTE map[256]; RDBUF buf; WORD dbpp;          /* Fenster: Abbildung auf die Systempalette */
    const R3DDRV *drv; DWORD bilder;
    WORD bpp; DWORD tc[256];                       /* Vollbild: Farbtiefe; ab 15 Bit Farbnummer -> Bildpunkt */
};

R3D  *R3dOpen(WORD art, HWND fenster, WORD w, WORD h);   /* Vollbild: vorher ScreenAcquire; w,h nur fuer Fenster */
R3D  *R3dOpenVesa(HWND fenster, WORD w, WORD h, WORD bpp);   /* VESA linear w x h x bpp (8, 15, 16, 24, 32), vorher ScreenAcquire */
void  R3dClose(R3D *r);
const char *R3dTreiber(R3D *r);
void  R3dRampe(R3D *r, WORD k, BYTE rot, BYTE gruen, BYTE blau);   /* 0..255 je Farbe */
void  R3dNebel(R3D *r, BYTE rot, BYTE gruen, BYTE blau, FIX nah, FIX fern);   /* nah/fern: z 0..FX(1) */
void  R3dPalette(R3D *r);                         /* Rampen und Nebel uebernehmen (Vollbild: DAC, auch nach M_SCREENBACK) */
void  R3dZustand(R3D *r, WORD r3s);
void  R3dFarbe(R3D *r, BYTE farbe);               /* ohne Textur: Rampe * 16 + Helligkeit */
void  R3dTextur(R3D *r, const BYTE *texel, WORD log2groesse);
void  R3dClear(R3D *r, BYTE farbe);               /* Bild und Z-Puffer */
void  R3dTri(R3D *r, const R3DVERT *a, const R3DVERT *b, const R3DVERT *c);
void  R3dFlip(R3D *r);                            /* Rueckpuffer zeigen (Vollbild: kopieren, Fenster: BufShow) */
BYTE  R3dPunkt(R3D *r, WORD x, WORD y);           /* Rueckpuffer lesen (Tests) */

/* Hilfen: Winkel 0..1023 = Vollkreis */
FIX   R3dSin(WORD w);
FIX   R3dCos(WORD w);
void  R3dIdent(R3DMAT *m);
void  R3dDreh(R3DMAT *m, WORD wx, WORD wy, WORD wz);   /* m = Drehung um x, dann y, dann z */
void  R3dVersch(R3DMAT *m, FIX x, FIX y, FIX z);
void  R3dMal(R3DMAT *e, const R3DMAT *a, const R3DMAT *b);   /* e = a * b */
void  R3dXform(const R3DMAT *m, const R3DVEC *v, R3DVEC *e);
int   R3dProj(R3D *r, const R3DVEC *v, R3DVERT *e, FIX zmax);   /* Mittelpunktprojektion, 0 = hinter der Kamera */
#endif
