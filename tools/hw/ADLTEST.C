/* ADLTEST.COM - Klangpruefung ohne RD2 (31.10.2026, PC10-III: "kein Sound mit AdLib").
 * 1 OPL2-Erkennung an 388h (Zeitgeber wie RD2), 2 Standardton aus dem AdLib-Handbuch, 3 RD2-Glockenton auf Kanal 8 (gleiche
 * Schreibroutine und Wartezeiten wie ADLIB.DRV), 4 derselbe Klang auf Kanal 0 wie RD1, 5 Kanal 8 mit 10fach Wartezeit, 6 PC-Lautsprecher.
 * Aufruf: ADLTEST [Port], z. B. ADLTEST 388. Ausgabe auch per > ADLTEST.TXT. */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <dos.h>
static unsigned port = 0x388, lang = 1;
static void opl(unsigned char r, unsigned char v)
{
    unsigned i;
    outp(port, r); for (i = 0; i < 6 * lang; i++) inp(port);
    outp(port + 1, v); for (i = 0; i < 35 * lang; i++) inp(port);
}
static void warte(unsigned ms) { delay(ms); }
static void reset(void) { unsigned r; for (r = 1; r <= 0xF5; r++) opl((unsigned char)r, 0); }
static int taste(const char *s) { int c; printf("%s\n  Taste druecken ... ", s); c = getch(); printf("\n"); return c; }
static void standard(void)                         /* AdLib Programming Guide, Kanal 0 */
{
    reset();
    opl(0x20, 0x01); opl(0x40, 0x10); opl(0x60, 0xF0); opl(0x80, 0x77);
    opl(0x23, 0x01); opl(0x43, 0x00); opl(0x63, 0xF0); opl(0x83, 0x77);
    opl(0xA0, 0x98); opl(0xB0, 0x31);               /* Key-On, ca. 440 Hz */
    warte(1500); opl(0xB0, 0x11); warte(500);
}
static void glocke(unsigned k)                     /* Kanal 8 wie ADLIB.DRV, Kanal 0 wie RD1; 523/659/784/1047 Hz (START) */
{
    static const unsigned char m[5] = { 0x01, 0x18, 0xF4, 0x55, 0 }, c[5] = { 0x01, 0x00, 0xF4, 0x55, 0 }, rg[5] = { 0x20, 0x40, 0x60, 0x80, 0xE0 };
    static const unsigned f[4] = { 523, 659, 784, 1047 };
    static const unsigned char opo[9] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };
    unsigned j, b, o = opo[k]; unsigned long fn;
    reset(); opl(0x01, 0x20);
    for (j = 0; j < 5; j++) { opl((unsigned char)(rg[j] + o), m[j]); opl((unsigned char)(rg[j] + o + 3), c[j]); }
    opl((unsigned char)(0xC0 + k), 0x06);
    for (j = 0; j < 4; j++) {
        b = 0; do { fn = (unsigned long)f[j] * (1UL << (20 - b)) / 49716UL; if (fn < 1024) break; } while (++b < 8);
        opl((unsigned char)(0xB0 + k), 0); opl((unsigned char)(0xA0 + k), (unsigned char)fn); opl((unsigned char)(0xB0 + k), (unsigned char)(0x20 | (b << 2) | ((fn >> 8) & 3)));
        warte(j == 3 ? 600 : 300);
    }
    opl((unsigned char)(0xB0 + k), 0); warte(300);
}
int main(int argc, char **argv)
{
    unsigned char s1, s2; unsigned i;
    if (argc > 1) port = (unsigned)strtoul(argv[1], 0, 16);
    printf("ADLTEST 1.1 (RetroDesk 2) - Port %Xh\n\n", port);
    opl(4, 0x60); opl(4, 0x80); s1 = (unsigned char)inp(port);
    opl(2, 0xFF); opl(4, 0x21); for (i = 0; i < 200; i++) inp(port);
    s2 = (unsigned char)inp(port); opl(4, 0x60); opl(4, 0x80);
    printf("1 Erkennung: Status vorher %02Xh (soll x0h), nachher %02Xh (soll Cxh) -> %s\n", s1, s2,
           (s1 & 0xE0) == 0 && (s2 & 0xE0) == 0xC0 ? "OPL2 gefunden" : "NICHT gefunden (RD2 nimmt dann den Lautsprecher)");
    taste("\n2 Standardton (AdLib-Handbuch): 1,5 s Dauerton ca. 440 Hz");
    standard();
    taste("3 RD2-Glocke wie ADLIB.DRV (Kanal 8): vier aufsteigende Toene (START)");
    glocke(8);
    taste("4 dieselbe Glocke wie RD1 (Kanal 0)");
    glocke(0);
    lang = 10;
    taste("5 RD2-Glocke (Kanal 8) mit 10fach langen Wartezeiten");
    glocke(8);
    reset();
    taste("6 PC-Lautsprecher: 0,5 s Ton 1 kHz");
    outp(0x43, 0xB6); outp(0x42, 0xA9); outp(0x42, 0x04); outp(0x61, inp(0x61) | 3); warte(500); outp(0x61, inp(0x61) & 0xFC);
    printf("\nWelche Nummern waren zu hoeren? (2, 3, 4, 5, 6)\n");
    return 0;
}
