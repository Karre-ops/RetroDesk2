/* BILD.H - Bilddecoder fuer RDBILD (BMP, PCX, GIF, JPEG Baseline). Laeuft auch auf dem Entwicklungsrechner
 * (test/bild_kern.c). Ablauf: BildStart (Kopf lesen: w, h, Art), bei JPEG skal setzen (1, 2, 4, 8), BildLesen
 * liefert Zeilen als RGB (3 Bytes je Punkt) in beliebiger Reihenfolge (BMP von unten, GIF verschachtelt). */
#ifndef BILD_H
#define BILD_H
#ifdef __WATCOMC__
#include "RD2.H"
#else
#include <stdint.h>
typedef uint8_t BYTE; typedef uint16_t WORD; typedef uint32_t DWORD;
#define FAR
#endif
#define BA_BMP 1
#define BA_PCX 2
#define BA_GIF 3
#define BA_JPG 4
#define BE_FORMAT  1                              /* unbekanntes Format */
#define BE_KAPUTT  2                              /* Datei fehlerhaft oder zu kurz */
#define BE_SPEICHER 3
#define BE_NICHT   4                              /* Variante nicht unterstuetzt (z. B. progressives JPEG) */
#define BE_ABBRUCH 5
#define BE_GROSS   6                              /* zu breit */
#define BMAXW 4096                                /* breiteste Zeile (Quelle) */
typedef struct {
    WORD w, h;                                    /* Groesse nach skal (JPEG) */
    WORD qw, qh;                                  /* Groesse in der Datei */
    BYTE art, dreh;                               /* BA_*, Exif-Lage 1..8 (1 = normal) */
    BYTE skal;                                    /* JPEG: 1, 2, 4, 8 (vor BildLesen setzen) */
    BYTE farben;                                  /* Bit je Punkt in der Datei (Anzeige) */
} BILD;
/* vom Aufrufer: */
WORD BRead(BYTE FAR *b, WORD n);                  /* Datei lesen, liefert Bytes */
void BSeek(DWORD pos);                            /* ab Dateianfang */
DWORD BSize(void);
void FAR *BMem(DWORD n);                          /* Speicher (fern), 0 = keiner; frei gibt der Aufrufer nach BildLesen */
BYTE BZeile(WORD y, const BYTE FAR *rgb);         /* Zeile y (w Punkte RGB); 0 = abbrechen */
WORD BildStart(BILD *b);
WORD BildLesen(BILD *b);
WORD JpgStart(BILD *b);                           /* intern (JPEG.C) */
WORD JpgLesen(BILD *b);
/* gepuffertes Lesen, gemeinsam fuer alle Decoder */
extern BYTE FAR *bl_puf; extern WORD bl_n, bl_p; extern BYTE bl_ende;
BYTE bl_fuell(void);
#define RB() (bl_p < bl_n ? bl_puf[bl_p++] : bl_fuell())
void bl_skip(DWORD n);
WORD bl_w(void);                                  /* 16 Bit Intel */
DWORD bl_l(void);
#endif
