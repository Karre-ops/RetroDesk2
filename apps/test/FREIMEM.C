/* FREIMEM.C - normales DOS-Programm fuer den RunDos-Test: schreibt den groessten freien
 * DOS-Block und die Befehlszeile in die Datei argv[1] und endet mit Code 42. */
#include <stdio.h>
#include <dos.h>
int main(int argc, char **argv)
{
    union REGS r; FILE *f;
    r.h.ah = 0x48; r.x.bx = 0xFFFF; intdos(&r, &r);
    if (argc < 2 || (f = fopen(argv[1], "w")) == NULL) return 1;
    fprintf(f, "FREI %u KB\nARG %s\n", r.x.bx / 64, argc > 2 ? argv[2] : "-");
    fclose(f);
    return 42;
}
