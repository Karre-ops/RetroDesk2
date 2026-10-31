/* bild_kern.c - RDBILD-Decoder auf dem Entwicklungsrechner: bild_kern DATEI SKAL AUS.PPM (test/bild_kern.sh) */
#include <stdio.h>
#include <stdlib.h>
#include "../apps/bild/BILD.H"
static FILE *f; static BILD b; static BYTE *img; static void *mem[64]; static int nm;
WORD BRead(BYTE *p, WORD n) { return (WORD)fread(p, 1, n, f); }
void BSeek(DWORD pos) { fseek(f, (long)pos, SEEK_SET); }
DWORD BSize(void) { long a = ftell(f), g; fseek(f, 0, SEEK_END); g = ftell(f); fseek(f, a, SEEK_SET); return (DWORD)g; }
void *BMem(DWORD n) { return nm < 64 ? (mem[nm++] = calloc(1, n)) : 0; }
BYTE BZeile(WORD y, const BYTE *rgb) { WORD i; if (y < b.h) for (i = 0; i < 3 * b.w; i++) img[(size_t)y * b.w * 3 + i] = rgb[i]; return 1; }
int main(int c, char **v)
{
    WORD r; FILE *o;
    if (c < 4 || !(f = fopen(v[1], "rb"))) return 2;
    r = BildStart(&b);
    printf("start %u art %u %ux%u dreh %u farben %u\n", r, b.art, b.qw, b.qh, b.dreh, b.farben);
    if (r) return 1;
    b.skal = (BYTE)atoi(v[2]);
    img = calloc(3, (size_t)(b.qw + 8) * (b.qh + 8));
    r = BildLesen(&b);
    printf("lesen %u %ux%u\n", r, b.w, b.h);
    o = fopen(v[3], "wb"); fprintf(o, "P6\n%u %u\n255\n", b.w, b.h); fwrite(img, 3, (size_t)b.w * b.h, o); fclose(o);
    return r ? 1 : 0;
}
