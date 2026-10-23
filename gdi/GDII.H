/* GDII.H - interne Schnittstelle RDGDI -> RDGDIX (selten gebrauchte Zeichenfunktionen).
 * RDGDIX ist verwerfbar und hat keinen eigenen Zustand: jeder Aufruf bekommt die Tabelle
 * des Kerns der Grafik mit. Koordinaten in span/line/get sind Geraetekoordinaten. */
#ifndef GDII_H
#define GDII_H
#include "RD2.H"

/* Geraetekontexte: RDUSER setzt bei Fenster-DCs ox/oy, clip/nclip und danach *kclip = 1
   (Export 0x3C + 7 = DC-Tabelle, 0x3C + 8 = kclip). HDC n = dcs[n - 1]. */
#define MAXDC   24
#define MAXCLIP 16
typedef struct {
    BYTE  used, typ;                              /* typ 0 = Schirm, 1 = Bitmap */
    HBMP  bmp;
    HTASK owner;
    short ox, oy;
    WORD  nclip;
    const RECT FAR *clip;                         /* Geraeteclip (Fenster, gehoert RDUSER), Geraetekoordinaten */
    BYTE  uclip;
    RECT  ucl;                                    /* SetClip, logische Koordinaten */
    COLOR pen, brush, txt, bk;
    WORD  pat, rop, bkmode;
    HFONT font;
    short cx, cy;
} DCX;
typedef struct {
    short ox, oy;                                 /* Ursprung des DC */
    BYTE  brnone;                                 /* Pinsel "keiner" */
    BYTE  nclip;                                  /* 0 = nichts zu zeichnen */
    RECT  bb;                                     /* Huelle der Clipliste */
} GIS;
typedef struct {
    WORD (RDEXPORT *begin)(HDC h, GIS FAR *s);    /* Ziel waehlen, 0 = ungueltig */
    void (RDEXPORT *end)(void);
    void (RDEXPORT *span)(short y, short a, short b, WORD brush);   /* 1 Pinsel, 0 Stift */
    void (RDEXPORT *line)(short x0, short y0, short x1, short y1);
    WORD (RDEXPORT *get)(short x, short y);       /* innerhalb bb */
} GI;
typedef void (RDAPI *GX_RECT)(GI FAR *g, HDC h, const RECT FAR *r, WORD a, WORD b);
typedef void (RDAPI *GX_POLY)(GI FAR *g, HDC h, const POINT FAR *p, WORD n, WORD fl);
typedef void (RDAPI *GX_FILL)(GI FAR *g, HDC h, short x, short y);
#endif
