/* D32TEST.C - 32-Bit-DOS-Programm fuer PLAN_32BIT Schritt 3 (laeuft ueber RunDos, RD2 ist ausgelagert).
 * Gebaut mit wcl386 fuer DOS/4GW, DOS32A, PMODE/W und CauseWay (build.sh). Aufruf: D32TEST datei [...]
 * Schreibt in datei: Extender-Start, 2 MB Speicher ueber 1 MB (Muster pruefen), Textschirm B8000h,
 * Maus (INT 33h Funktion 0), Tastatur (INT 16h Funktion 1). Rueckgabe 42 wie FREIMEM.
 * Mit D32KEY gesetzt (QEMU-Test): bis 20 s auf eine Taste warten (RDKEYB im geschuetzten Modus: y -> z bei DE)
 * und die Mausbewegung lesen (INT 33h Funktion 0Bh, RDMAUS). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i86.h>
#include <time.h>
#include <conio.h>
int main(int argc, char **argv)
{
    FILE *f; unsigned long *p, i, n = 2UL * 1024 * 1024 / 4, bad = 0; union REGS r;
    unsigned short *bild = (unsigned short *)0xB8000; static const char t[] = "D32TEST 32 Bit";
    if (argc < 2 || !(f = fopen(argv[1], "w"))) return 1;
    fprintf(f, "D32: start, Zeigergroesse %u\n", (unsigned)sizeof p);
    p = malloc(n * 4);
    if (!p) fprintf(f, "D32: kein Speicher\n");
    else {
        for (i = 0; i < n; i++) p[i] = i * 2654435761UL;
        for (i = 0; i < n; i++) if (p[i] != i * 2654435761UL) bad++;
        fprintf(f, "D32: 2048 KB bei %08lX, Abweichungen %lu\n", (unsigned long)p, bad);
        fprintf(f, "D32: ueber 1 MB %s\n", (unsigned long)p >= 0x100000UL ? "ja" : "nein");
        free(p);
    }
    for (i = 0; t[i]; i++) bild[i] = 0x1F00 | (unsigned char)t[i];
    fprintf(f, "D32: Textschirm %s\n", (bild[0] & 0xFF) == 'D' ? "ok" : "falsch");
    memset(&r, 0, sizeof r); r.w.ax = 0; int386(0x33, &r, &r);
    fprintf(f, "D32: Maus %04X\n", r.w.ax);
    memset(&r, 0, sizeof r); r.h.ah = 1; int386(0x16, &r, &r);
    fprintf(f, "D32: Tastatur ok\n");
    if (getenv("D32KEY")) {
        clock_t e = clock() + 20 * CLOCKS_PER_SEC; int k = -1;
        fprintf(f, "D32: warte\n"); fclose(f); f = fopen(argv[1], "a");   /* schliessen: Groesse im Verzeichnis */
        while (clock() < e) {
            if (kbhit()) { k = getch(); break; }   /* DOS 0Bh/07h -> INT 16h -> RDKEYB-Puffer */
        }
        fprintf(f, "D32: Taste %02X\n", k & 0xFF);
        memset(&r, 0, sizeof r); r.w.ax = 0x0B; int386(0x33, &r, &r);
        fprintf(f, "D32: Bewegung %d %d\n", (short)r.w.cx, (short)r.w.dx);
    }
    fprintf(f, "D32: Ende\n");
    fclose(f);
    return 42;
}
