/* RD2_32.H - RD2/32-Programme (32 Bit, flach, ab 386), PLAN_32BIT Stufe 4b.
 * SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
 * Die ganze API aus RD2.H gilt auch in 32 Bit (Zeiger sind 32-Bit-Zeiger, die Bibliothek bringt Daten fuer den Kern
 * unter 1 MB). Bauen: build.sh app32 (wcc386 -mf, RD32C0 zuerst, RD32LIB, tools/rd32link.py).
 * Dazu nur in 32 Bit: lineare Adressen, DPMI-Speicher, Puffer-Bitmap fuer schnelle Grafik im Fenster, VESA 2.0 linear. */
#ifndef RD2_32_H
#define RD2_32_H
#include "RD2.H"

/* Speicherschutz (PLAN_API 3b, ab 24.11.2026): Programme mit RD32C0.OBJ sehen mit nahen Zeigern nur ihren eigenen
   Speicher (Bild, Stapel, rd32_alloc, im LFB-Modus bis zum Bildspeicher). Zugriff auf DOS, BIOS, A0000h, Taskblock
   oder 16-Bit-Programme ergibt eine Ausnahme, die nur dieses Programm beendet. Speicher unter 1 MB gibt es nur ueber
   __far-Zeiger: BiosPtr, LowPtr, MemLock/MemFixed (LPLOW). Vollzugriff wie frueher (4 GB): RD32C0V.OBJ statt RD32C0.OBJ.
   13h und Textmodus: ScreenPtr ist ein Schattenpuffer, ScreenUpdate bringt ihn auf den Schirm. */
void *rd32_lin(DWORD lin);                        /* lineare Adresse als naher Zeiger: nur eigener Speicher (Vollzugriff: alles) */
BYTE __far *BiosPtr(void);                        /* BIOS-Datenbereich: BiosPtr()[0x46C] usw. (Linear 0, bis 10FFFFh) */
BYTE __far *LowPtr(DWORD lin);                    /* beliebige Adresse unter 1 MB + HMA, 0 = darueber */
int   rd32_grenze(DWORD lin, DWORD n);            /* intern: [lin, lin+n) nah erreichbar machen (rd32_alloc, LFB) */
void *rd32_alloc(DWORD n);                        /* DPMI-Speicher (XMS), 0 = keiner; endet das Programm, wird er frei */
void  rd32_free(void *p);

/* Puffer-Bitmap: das Programm schreibt 8-Bit-Farbnummern (C_* bzw. Palette) direkt in pix, BufShow bringt sie in
   einem Aufruf ins Fenster (Geraeteformat 1, 4 oder 8 Bit, in Streifen zu hoechstens 64 KB). */
typedef struct { WORD w, h; BYTE *pix; HBMP bmp; WORD streifen; } RDBUF;
BOOL  BufCreate(RDBUF *b, WORD w, WORD h);        /* pix aus rd32_alloc, w*h Bytes */
void  BufShow(RDBUF *b, HDC dc, short x, short y);
void  BufFree(RDBUF *b);

/* 32-Bit-Module (.M32, tools/rd32mod.py mit RD32M0.OBJ vorn): laufen im Adressraum des Programms, Exporte nach Namen.
   Ohne Pfad sucht rd32_modload im Verzeichnis des Programms. Jedes Laden ist eine eigene Kopie mit eigenen Daten.
   Das Modul liefert int RdMod32Init(void) (0 = ok); rd32_modfree ruft sein atexit und schliesst seine Dateien.
   Am Programmende werden noch geladene Module freigegeben. exit() im Modul beendet das Programm (Rueckgabe 0). */
void *rd32_modload(const char *datei);            /* 0 = Fehler, LastError */
void *rd32_modproc(void *mod, const char *name);  /* Adresse des Exports, 0 = fehlt */
void  rd32_modfree(void *mod);
int   RdMod32Init(void);

/* Vollbild: ScreenMode wie RD2.LIB (SM_TEXT, SM_13H) und SM_LFB; ScreenPtr = Bildspeicher als Zeiger */
#define SM_LFB 7                                  /* 640x480 256 Farben, VESA 2.0 linearer Bildspeicher (nur 32 Bit) */
BYTE *ScreenPtr(void);
/* VESA linear in jeder Groesse und Farbtiefe der Karte (VBE 2.0): bpp 8 (Palette), 15/16 (RGB 555/565), 24, 32
   (je ein Byte Blau, Gruen, Rot). si->zeilenbytes beachten. ScreenRestore setzt denselben Modus wieder. */
typedef struct { WORD nr, w, h, bpp; } VESAMODE;
WORD  ScreenModeVesa(WORD w, WORD h, WORD bpp, SCREENINFO *si);   /* 1 = ok, sonst LastError RD_ERR_NOTSUP */
WORD  ScreenVesaList(VESAMODE *l, WORD max);      /* lineare Modi der Karte, Anzahl */
#define VESA_BANK 0x8000                          /* bpp | VESA_BANK: Bankumschaltung erzwingen (Test, Karten mit LFB-Fehlern) */
void  ScreenUpdate(void);                          /* Karten ohne linearen Bildspeicher (VBE 1.2): ScreenPtr ist ein Schattenpuffer
                                                      mit zeilenbytes je Zeile, ScreenUpdate bringt ihn ueber 64-KB-Baenke auf den
                                                      Schirm; ebenso 13h/Text mit Speicherschutz. Mit LFB tut es nichts; nach dem
                                                      Zeichnen also immer aufrufen. */
#endif
