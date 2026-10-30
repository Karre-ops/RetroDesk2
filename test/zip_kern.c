/* zip_kern.c - RDZIP-Kern (CRC, Inflate, Deflate) auf dem Entwicklungsrechner pruefen (test/zip_kern.sh).
 * zip_kern d EIN AUS : packen;  zip_kern i EIN AUS : entpacken;  zip_kern c EIN : CRC-32 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../apps/zip/ZK.H"
static FILE *fi, *fo;
static WORD rd(void *u, BYTE *b, WORD n) { (void)u; return (WORD)fread(b, 1, n, fi); }
static WORD wr(void *u, const BYTE *b, WORD n) { (void)u; return (WORD)fwrite(b, 1, n, fo); }
int main(int c, char **v)
{
    static BYTE fe[32768], ein[2048], da[DEF_PUF0], sy[DEF_PUF3]; static WORD pv[DEF_PUF1 / 2], hd[DEF_PUF2 / 2];
    WORD r; DWORD n = 0;
    if (c < 3) return 2;
    fi = fopen(v[2], "rb"); if (!fi) return 2;
    if (v[1][0] == 'c') { BYTE b[4096]; size_t k; DWORD crc = 0xFFFFFFFFUL; CrcInit(); while ((k = fread(b, 1, sizeof b, fi)) > 0) crc = CrcRechne(crc, b, (WORD)k); printf("%08x\n", (unsigned)(crc ^ 0xFFFFFFFFUL)); return 0; }
    fo = fopen(v[3], "wb"); if (!fo) return 2;
    if (v[1][0] == 'i') r = Inflate(rd, wr, 0, fe, ein, sizeof ein);
    else r = Deflate(rd, wr, 0, da, pv, hd, sy, &n);
    fclose(fo);
    if (r) { printf("Fehler %u\n", r); return 1; }
    return 0;
}
