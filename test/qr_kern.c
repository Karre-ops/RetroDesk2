/* qr_kern.c - prueft apps/net/QR.C mit gcc: Matrix als PGM, test/qr_kern.sh liest sie mit OpenCV zurueck. */
#include <stdio.h>
#include "../apps/net/QR.H"
int main(int argc, char **argv)
{
    static unsigned char m[QRN][QRN]; int x, y, s = 8, q = 4, n = (QRN + 2 * q) * 8, px, py; FILE *f;
    if (argc < 3 || qr_make(argv[1], m)) return 1;
    f = fopen(argv[2], "wb"); fprintf(f, "P5 %d %d 255\n", n, n);
    for (py = 0; py < n; py++) for (px = 0; px < n; px++) {
        x = px / s - q; y = py / s - q;
        fputc(x >= 0 && y >= 0 && x < QRN && y < QRN && m[y][x] ? 0 : 255, f);
    }
    fclose(f); return 0;
}
