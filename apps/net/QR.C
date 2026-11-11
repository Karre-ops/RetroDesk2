/* QR.C - QR-Code Version 3 (29 x 29), Fehlerkorrektur L, Bytemodus, Maske 0. Bis 53 Zeichen.
 * Fuer RDQR.EXE (Adresse der Upload-Seite). Ohne RD2-Aufrufe, damit test/qr_kern.c es mit gcc pruefen kann.
 * qr_make(text, m): m[y][x] 1 = dunkel. Rueckgabe 0 = ok, 1 = Text zu lang. Ablauf nach ISO 18004. */
#include "QR.H"

#define DATW 55                                   /* Datenwoerter Version 3-L */
#define ECCW 15
static unsigned char cw[DATW + ECCW];
static unsigned char gen[ECCW];

static unsigned char gmul(unsigned char a, unsigned char b)   /* GF(256), Polynom 0x11D */
{
    unsigned char r = 0;
    while (b) { if (b & 1) r ^= a; a = (unsigned char)((a << 1) ^ ((a & 0x80) ? 0x1D : 0)); b >>= 1; }
    return r;
}
static void rs(void)
{
    int i, j; unsigned char root = 1, f;
    for (i = 0; i < ECCW; i++) gen[i] = 0;
    gen[ECCW - 1] = 1;
    for (i = 0; i < ECCW; i++) {
        for (j = 0; j < ECCW; j++) { gen[j] = gmul(gen[j], root); if (j + 1 < ECCW) gen[j] ^= gen[j + 1]; }
        root = gmul(root, 2);
    }
    for (i = 0; i < ECCW; i++) cw[DATW + i] = 0;
    for (i = 0; i < DATW; i++) {
        f = (unsigned char)(cw[i] ^ cw[DATW]);
        for (j = 0; j < ECCW - 1; j++) cw[DATW + j] = cw[DATW + j + 1];
        cw[DATW + ECCW - 1] = 0;
        for (j = 0; j < ECCW; j++) cw[DATW + j] ^= gmul(gen[j], f);
    }
}

static unsigned char (*M)[QRN];
#define FN 2                                      /* Bit 1: Funktionsmodul */
static void fm(int x, int y, int dunkel) { M[y][x] = (unsigned char)(FN | (dunkel ? 1 : 0)); }
static int ab(int v) { return v < 0 ? -v : v; }
static void finder(int cx, int cy)
{
    int dx, dy, d, x, y;
    for (dy = -4; dy <= 4; dy++) for (dx = -4; dx <= 4; dx++) {
        x = cx + dx; y = cy + dy; if (x < 0 || y < 0 || x >= QRN || y >= QRN) continue;
        d = ab(dx); if (ab(dy) > d) d = ab(dy);
        fm(x, y, d != 2 && d != 4);
    }
}
static void format(void)                          /* L = 01, Maske 000 */
{
    int i; unsigned rem = 8, bits;
    for (i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    bits = ((8u << 10) | (rem & 0x3FF)) ^ 0x5412;
#define B(i) ((bits >> (i)) & 1)
    for (i = 0; i <= 5; i++) fm(8, i, B(i));
    fm(8, 7, B(6)); fm(8, 8, B(7)); fm(7, 8, B(8));
    for (i = 9; i < 15; i++) fm(14 - i, 8, B(i));
    for (i = 0; i < 8; i++) fm(QRN - 1 - i, 8, B(i));
    for (i = 8; i < 15; i++) fm(8, QRN - 15 + i, B(i));
    fm(8, QRN - 8, 1);
#undef B
}
int qr_make(const char *s, unsigned char m[QRN][QRN])
{
    int n = 0, i, k, x, y, dx, dy, r, v, j, up; unsigned long bit = 0;
    while (s[n]) n++;
    if (n > DATW - 2) return 1;
    M = m;
    for (i = 0; i < DATW; i++) cw[i] = 0;
#define PUT(val, len) for (k = (len) - 1; k >= 0; k--, bit++) if (((val) >> k) & 1) cw[bit >> 3] |= (unsigned char)(0x80 >> (bit & 7))
    PUT(4, 4); PUT(n, 8);
    for (i = 0; i < n; i++) PUT((unsigned char)s[i], 8);
#undef PUT
    bit += 4; if (bit > DATW * 8UL) bit = DATW * 8UL;   /* Ende 0000 */
    bit = (bit + 7) & ~7UL;
    for (i = (int)(bit >> 3), k = 0; i < DATW; i++, k ^= 1) cw[i] = (unsigned char)(k ? 0x11 : 0xEC);
    rs();
    for (y = 0; y < QRN; y++) for (x = 0; x < QRN; x++) m[y][x] = 0;
    for (i = 0; i < QRN; i++) { fm(6, i, !(i & 1)); fm(i, 6, !(i & 1)); }   /* Taktlinien */
    finder(3, 3); finder(QRN - 4, 3); finder(3, QRN - 4);
    for (dy = -2; dy <= 2; dy++) for (dx = -2; dx <= 2; dx++) {   /* Ausrichtung bei 22,22 */
        v = ab(dx); if (ab(dy) > v) v = ab(dy);
        fm(22 + dx, 22 + dy, v != 1);
    }
    format();
    i = 0;
    for (r = QRN - 1; r >= 1; r -= 2) {
        if (r == 6) r = 5;
        for (v = 0; v < QRN; v++) for (j = 0; j < 2; j++) {
            x = r - j; up = ((r + 1) & 2) == 0; y = up ? QRN - 1 - v : v;
            if (m[y][x] & FN) continue;
            if (i < (DATW + ECCW) * 8) { if ((cw[i >> 3] >> (7 - (i & 7))) & 1) m[y][x] = 1; i++; }
            if (!((x + y) & 1)) m[y][x] ^= 1;     /* Maske 0 */
        }
    }
    for (y = 0; y < QRN; y++) for (x = 0; x < QRN; x++) m[y][x] &= 1;
    return 0;
}
