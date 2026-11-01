/* BENCH1.C - Leistungsvergleich, Seite RetroDesk 1: dieselben Aufgaben wie BENCH2 (RD2),
 * direkt mit dem RD1-Renderer (gfx.c, mouse.c aus main). Messung: jede Aufgabe so oft wie
 * moeglich in 36 BIOS-Ticks (ca. 2 s), Ergebnis in Mikrosekunden je Aufruf.
 * Aufruf: BENCH1 [/CGA|/HGC|/VGA] ausgabe.txt */
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include "gfx.h"
#include "mouse.h"

static volatile unsigned long far *bt = (volatile unsigned long far *)MK_FP(0x40, 0x6C);
static FILE *out;
static unsigned char far *blk;
static int mx, my, mb;

static void miss(const char *name, void (*fn)(void))
{
    unsigned long t, n = 0;
    t = *bt; while (*bt == t) ;
    t = *bt;
    do { fn(); n++; } while (*bt - t < 36);
    fprintf(out, "%-32s %8lu us\n", name, (36UL * 54925UL) / n);
}
static void a_text13(void) { gfx_text(13, 40, "RDEDIT.EXE     51110", 0); }
static void a_text16(void) { gfx_text(16, 40, "RDEDIT.EXE     51110", 0); }
static void a_fill(void)   { gfx_fill(0, 0, gfx_w, gfx_h, 1); }
static void a_pat(void)    { gfx_pattern(0, 0, gfx_w, gfx_h); }
static void a_inv(void)    { gfx_xorfill(40, 40, 200, 100); }
static void a_rect(void)   { gfx_rect(40, 40, 200, 100, 0); }
static void a_blk(void)    { gfx_getblock(8, 20, 208, 150, blk); gfx_putblock(8, 20, 208, 150, blk); }
static volatile unsigned rz;
static void a_calc(void)   { unsigned i; for (i = 0; i < 1000; i++) rz += i; }
static void a_inp(void)    { kbhit(); mouse_read(&mx, &my, &mb); }
/* deckend wie TextOut mit BK_OPAQUE: erst Hintergrund, dann Schrift */
static void a_dtext13(void) { gfx_fill(13, 40, 160, gfx_fh, 1); gfx_text(13, 40, "RDEDIT.EXE     51110", 0); }
static void a_dtext16(void) { gfx_fill(16, 40, 160, gfx_fh, 1); gfx_text(16, 40, "RDEDIT.EXE     51110", 0); }

int main(int argc, char **argv)
{
    int i, typ = -1; unsigned sg;
    const char *fn = "BENCH1.TXT";
    for (i = 1; i < argc; i++) { if (gfx_arg(argv[i]) >= 0) typ = gfx_arg(argv[i]); else fn = argv[i]; }
    out = fopen(fn, "w");
    if (!out) return 1;
    if (_dos_allocmem((gfx_blocksize(8, 208, 150) + 15) >> 4, &sg)) return 2;
    blk = (unsigned char far *)MK_FP(sg, 0);
    if (gfx_init(typ) < 0) return 3;
    mouse_init();
    miss("Text 20 Zeichen, x=13", a_text13);
    miss("Text 20 Zeichen, x=16", a_text16);
    miss("Text deckend, x=13", a_dtext13);
    miss("Text deckend, x=16", a_dtext16);
    miss("Schirm fuellen", a_fill);
    miss("Muster 50% voll", a_pat);
    miss("Rechteck 200x100 invertieren", a_inv);
    miss("Rahmen 200x100", a_rect);
    miss("Block 208x150 sichern+zurueck", a_blk);
    miss("Rechenschleife 1000",  a_calc);
    miss("Eingabe abfragen", a_inp);
    gfx_close();
    fclose(out);
    return 0;
}
