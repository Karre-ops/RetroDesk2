/* RD2DRV.H - Treiber-Schnittstellen RetroDesk 2 (API 1.0)
 * SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
 *
 * Ein Treiber (.DRV) ist ein Modul wie jedes andere (siehe SPEC.md 2/3).
 * Der Kern ruft nach dem Laden Exportnummer 1 auf; sie liefert einen
 * FAR-Zeiger auf eine der Tabellen unten. Die Tabelle beginnt immer mit
 * DRVHEAD. Neue Felder kommen nur hinten dazu (groesse zeigt die Laenge).
 *
 * Treiber kennen kein Clipping, keine Fenster und keine Farbumrechnung:
 * das macht RDGDI. Ein Grafiktreiber muss deshalb nur Grundfunktionen
 * liefern; alles mit NULL wird von RDGDI aus den Grundfunktionen nachgebaut.
 */
#ifndef RD2DRV_H
#define RD2DRV_H
#include "RD2.H"

#define DRV_GFX 1
#define DRV_SND 2
#define DRV_PRN 3
#define DRV_IN  4                                 /* Maus, Tastatur, Joystick */
#define DRV_NET 5                                 /* Paketzugang (Packet-Treiber) */

#pragma pack(push, 1)
typedef struct {
    char  sig[4];                                 /* "RDRV" */
    WORD  typ;                                    /* DRV_* */
    WORD  version;                                /* Treiber-Schnittstelle, 0x0100 */
    WORD  groesse;                                /* sizeof der ganzen Tabelle */
    char  name[16];                               /* "CGA 640x200" */
    WORD  (RDAPI *init)(WORD param);              /* 0 = ok, sonst RD_ERR_* */
    void  (RDAPI *done)(void);
} DRVHEAD;

/* ---- Grafik ------------------------------------------------------------ */
typedef struct {
    WORD w, h, bpp, farben;                       /* 640, 200, 1, 2 */
    WORD aspx, aspy;                              /* Pixelseitenverhaeltnis */
    WORD zeilenbytes;                             /* fuer Bitmaps im Geraeteformat */
    WORD flags;                                   /* GD_* */
} GFXMODE;

#define GD_PALETTE 0x0001                         /* Farbtabelle aenderbar */
#define GD_LINEAR  0x0002                         /* Bildspeicher am Stueck (Modus P, VESA) */
#define GD_HWCURSOR 0x0004                        /* Zeiger zeichnet der Maustreiber */

typedef struct {
    DRVHEAD h;
    WORD  (RDAPI *modes)(GFXMODE FAR *m, WORD max);           /* moegliche Modi */
    WORD  (RDAPI *setmode)(WORD nr);                          /* Grafik an */
    void  (RDAPI *textmode)(void);                            /* zurueck in den Textmodus */
    /* Pflicht: */
    void  (RDAPI *span)(short y, short x0, short x1, WORD farbe, const BYTE FAR *muster8, WORD rop);
    void  (RDAPI *pixel)(short x, short y, WORD farbe, WORD rop);
    WORD  (RDAPI *getpixel)(short x, short y);
    void  (RDAPI *put)(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes, WORD srcbpp, WORD rop, WORD fg, WORD bg);
    void  (RDAPI *get)(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes);
    /* Optional (NULL = RDGDI baut es nach): */
    void  (RDAPI *vline)(short x, short y0, short y1, WORD farbe, WORD rop);
    void  (RDAPI *line)(short x0, short y0, short x1, short y1, WORD farbe, WORD rop);
    void  (RDAPI *copy)(short sx, short sy, short w, short h, short dx, short dy); /* Schirm -> Schirm (Rollen) */
    /* glyphs: n Zeichen je 8 Pixel breit ab x,y, Zeichen i = h Bytes am Stueck ab seg:offs[i].
       rop ROP_MASK (nur gesetzte Bits in fg), ROP_XOR (gesetzte Bits umkehren) oder
       ROP_COPY (deckend: fg auf bg, nur bei x nicht durch 8 teilbar). Liegt ganz im Schirm. */
    void  (RDAPI *glyphs)(short x, short y, const WORD FAR *offs, WORD seg, WORD h, WORD n, WORD fg, WORD bg, WORD rop);
    void  (RDAPI *palette)(WORD first, WORD n, const COLOR FAR *c);
    void  (RDAPI *cursor)(short x, short y, const BYTE FAR *maske_bild); /* NULL bei GD_HWCURSOR */
    void  (RDAPI *cursorhide)(BOOL aus);
    void  (RDAPI *vsync)(void);
    /* ab 22.10.2026 Pflicht fuer Schirmtreiber (RDGDI prueft h.groesse, cursor, box):
       box = ganzes Rechteck x0..x1, y0..y1 (inklusive) in einem Aufruf, Zeilen in Assembler.
       cursor/cursorhide: Zeiger zeichnet der Treiber selbst (mb = 32 Bytes UND, 32 Bytes XOR) */
    void  (RDAPI *box)(short x0, short y0, short x1, short y1, WORD farbe, const BYTE FAR *muster8, WORD rop);
} GFXDRV;
/* Treiber mit mehr als 1 Bit (ab 27.10.2026): GFXDRV und dahinter RGB -> naechster Index und zurueck
   (RDGDI rechnet damit Stift, Pinsel, Text, GetPixel und ColorNearest um). h.groesse = sizeof(GFXDRVF). */
typedef struct {
    GFXDRV g;
    WORD  (RDAPI *farbe)(COLOR c);
    COLOR (RDAPI *rgb)(WORD index);
} GFXDRVF;
/* Hintergrund roh retten (ab 30.10.2026, optional, h.groesse = sizeof(GFXDRVR)): Bildspeicher unveraendert
   kopieren (Ebenen bzw. Schirmbytes), nur fuer Menue-Hintergruende (BmpCreate mit BMP_ROH), nie bemalt.
   rohgr: Bytes je Zeile fuer w Punkte ab x. rohget: Rechteck ganz (liegt im Schirm). rohput: Teil qx..qx+qw-1
   eines bei x0 mit Breite w0 geretteten Blocks an dieselbe Stelle zurueck; z = Anfang der Zeile qy. */
typedef struct {
    GFXDRVF f;
    WORD  (RDAPI *rohgr)(short x, short w);
    void  (RDAPI *rohget)(short x, short y, short w, short h, BYTE FAR *z, WORD bpr);
    void  (RDAPI *rohput)(short qx, short qy, short qw, short qh, const BYTE FAR *z, WORD bpr, short x0, short w0);
    /* Farbiges Symbol mit Maske (30.10.2026): nur Punkte mit gesetztem Maskenbit schreiben. s = Zeile 0 ab dem
       ersten Punkt (unten: erster Punkt im unteren Halbbyte, nur 4 Bit), m = Maskenzeile 0, erstes Bit mbit (0 = Bit 7).
       Darf 0 sein (dann setzt RDGDIX die Punkte ueber BitBlt ein). */
    void  (RDAPI *symput)(short x, short y, short w, short h, const BYTE FAR *s, WORD sbpr, WORD unten, const BYTE FAR *m, WORD mbpr, WORD mbit);
} GFXDRVR;
/* Ablage im unsichtbaren Bildspeicher (01.11.2026, optional, h.groesse = sizeof(GFXDRVV)): vrneu reserviert Platz
   fuer einen rohen Block w x h und liefert eine Nummer 1..255 (0 = kein Platz, dann RAM wie bisher). BmpCreate
   setzt dann bpr = 0x8000 | Nummer; rohget/rohput erkennen das und kopieren ueber die Latches. vrfrei gibt frei. */
typedef struct {
    GFXDRVR r;
    WORD  (RDAPI *vrneu)(short w, short h);
    void  (RDAPI *vrfrei)(WORD n);
} GFXDRVV;

/* ---- Sound --------------------------------------------------------------- */
typedef struct {
    DRVHEAD h;
    WORD  (RDAPI *caps)(void);                                /* SC_* */
    void  (RDAPI *beep)(WORD hz, WORD ms);                    /* ms 0 = aus */
    void  (RDAPI *reg)(WORD reg, BYTE wert);                  /* OPL-Register direkt (NULL ohne FM) */
    WORD  (RDAPI *note)(WORD kanal, WORD note, WORD lautst);
    void  (RDAPI *noteoff)(WORD kanal);
    WORD  (RDAPI *instr)(WORD kanal, const INSTR FAR *ins);
    WORD  (RDAPI *pcm_start)(const BYTE FAR *buf, WORD n, WORD rate, void (RDAPI *nachfuellen)(void)); /* NULL ohne PCM */
    void  (RDAPI *pcm_stop)(void);
    void  (RDAPI *volume)(WORD art, WORD wert);
    void  (RDAPI *tick)(void);                                /* vom Zeitgeber, z. B. fuer Speaker-Huellkurven */
} SNDDRV;

/* ---- Drucker ------------------------------------------------------------- */
/* RDPRN gibt dem Treiber eine Ausgaberoutine (LPT-Port oder Datei); 0 = ok, sonst RD_ERR_* (z. B. RD_ERR_ABORT). */
typedef WORD (RDAPI *PRNOUT)(const BYTE FAR *p, WORD n);
typedef struct {
    DRVHEAD h;
    WORD  (RDAPI *open)(PRNJOB FAR *job, PRNOUT aus);         /* setzt dpi_x, dpi_y, breite, hoehe; PJ_ENTWURF beachten */
    WORD  bandhoehe;                                          /* Zeilen je Durchgang (9-Nadel: 8); RDPRN gibt Vielfache */
    WORD  (RDAPI *band)(const BYTE FAR *bits, WORD zeilenbytes, WORD zeilen); /* 1 Bit, 1 = Punkt, links oben */
    WORD  (RDAPI *page)(void);                                /* Seitenvorschub */
    WORD  (RDAPI *close)(BOOL abbruch);
} PRNDRV;

/* ---- Eingabe ------------------------------------------------------------- */
typedef struct {
    DRVHEAD h;
    WORD  (RDAPI *mouse)(short FAR *x, short FAR *y);         /* Tasten, Position */
    void  (RDAPI *mouserange)(short w, short h);
    WORD  (RDAPI *key)(void);                                 /* 0 = keine Taste */
    WORD  (RDAPI *shift)(void);                               /* KS_* */
    WORD  (RDAPI *joy)(WORD nr, JOYSTATE FAR *js);            /* NULL ohne Joystick */
} INDRV;

/* ---- Netz (Paketzugang fuer RDNET) --------------------------------------- */
typedef struct {
    DRVHEAD h;
    WORD  (RDAPI *mac)(BYTE FAR *mac6);
    WORD  (RDAPI *send)(const BYTE FAR *frame, WORD n);
    WORD  (RDAPI *recv)(BYTE FAR *frame, WORD max);           /* 0 = nichts da */
} NETDRV;
#pragma pack(pop)

#endif /* RD2DRV_H */
