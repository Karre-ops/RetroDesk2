/* ADLTEST.COM - Klangpruefung ohne RD2 (31.10.2026, PC10-III: "kein Sound mit AdLib").
 * 1 OPL2-Erkennung an 388h (Zeitgeber wie RD2), 2 Standardton aus dem AdLib-Handbuch, 3 RD2-Glockenton (gleiche
 * Schreibroutine und Wartezeiten wie ADLIB.DRV), 4 dasselbe mit 10fach langen Wartezeiten, 5 PC-Lautsprecher 1 kHz.
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
static void glocke(void)                           /* wie ADLIB.DRV: Kanal 8, 523/659/784/1047 Hz (START) */
{
    static const unsigned char m[5] = { 0x01, 0x18, 0xF4, 0x55, 0 }, c[5] = { 0x01, 0x00, 0xF4, 0x55, 0 }, rg[5] = { 0x20, 0x40, 0x60, 0x80, 0xE0 };
    static const unsigned f[4] = { 523, 659, 784, 1047 };
    unsigned j, b; unsigned long fn;
    reset(); opl(0x01, 0x20);
    for (j = 0; j < 5; j++) { opl((unsigned char)(rg[j] + 18), m[j]); opl((unsigned char)(rg[j] + 21), c[j]); }
    opl(0xC8, 0x06);
    for (j = 0; j < 4; j++) {
        b = 0; do { fn = (unsigned long)f[j] * (1UL << (20 - b)) / 49716UL; if (fn < 1024) break; } while (++b < 8);
        opl(0xB8, 0); opl(0xA8, (unsigned char)fn); opl(0xB8, (unsigned char)(0x20 | (b << 2) | ((fn >> 8) & 3)));
        warte(j == 3 ? 600 : 300);
    }
    opl(0xB8, 0); warte(300);
}
int main(int argc, char **argv)
{
    unsigned char s1, s2; unsigned i;
    if (argc > 1) port = (unsigned)strtoul(argv[1], 0, 16);
    printf("ADLTEST 1.0 (RetroDesk 2) - Port %Xh\n\n", port);
    opl(4, 0x60); opl(4, 0x80); s1 = (unsigned char)inp(port);
    opl(2, 0xFF); opl(4, 0x21); for (i = 0; i < 200; i++) inp(port);
    s2 = (unsigned char)inp(port); opl(4, 0x60); opl(4, 0x80);
    printf("1 Erkennung: Status vorher %02Xh (soll x0h), nachher %02Xh (soll Cxh) -> %s\n", s1, s2,
           (s1 & 0xE0) == 0 && (s2 & 0xE0) == 0xC0 ? "OPL2 gefunden" : "NICHT gefunden (RD2 nimmt dann den Lautsprecher)");
    taste("\n2 Standardton (AdLib-Handbuch): 1,5 s Dauerton ca. 440 Hz");
    standard();
    taste("3 RD2-Glocke wie ADLIB.DRV: vier aufsteigende Toene (START)");
    glocke();
    lang = 10;
    taste("4 RD2-Glocke mit 10fach langen Wartezeiten");
    glocke();
    reset();
    taste("5 PC-Lautsprecher: 0,5 s Ton 1 kHz");
    outp(0x43, 0xB6); outp(0x42, 0xA9); outp(0x42, 0x04); outp(0x61, inp(0x61) | 3); warte(500); outp(0x61, inp(0x61) & 0xFC);
    printf("\nWelche Nummern waren zu hoeren? (2, 3, 4, 5)\n");
    return 0;
}
