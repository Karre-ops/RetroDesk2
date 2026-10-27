/* JOYTEST.C - Joystick (RD2.LIB JoyRead/JoyCalibrate): misst A und B je dreimal und meldet auf COM1
 * "JOYTEST: A fehler x y tasten". Ohne Gameport: Fehler 8 (RD_ERR_NOTSUP). test/joystick.sh */
#include "RD2.H"
static short df = -1;                             /* JOY.TXT: DOSBox 0.74 hat keine COM1-Datei */
static void aus(LPCSTR z) { WORD n = 0; DbgPrint(z); if (df < 0) return; while (z[n]) n++; FileWrite(df, z, n); FileWrite(df, "\r\n", 2); }

static void zeig(WORD nr)
{
    JOYSTATE js; WORD e; char z[48]; WORD i = 0, k; short v[3]; static const char p[] = "JOYTEST: ";
    js.x = js.y = 0; js.tasten = 0;
    e = JoyRead(nr, &js);
    for (k = 0; p[k]; k++) z[i++] = p[k];
    z[i++] = (char)('A' + nr); z[i++] = ' '; z[i++] = (char)('0' + e % 10);
    v[0] = js.x; v[1] = js.y; v[2] = (short)js.tasten;
    for (k = 0; k < 3; k++) {
        short n = v[k]; char b[6]; WORD m = 0;
        z[i++] = ' '; if (n < 0) { z[i++] = '-'; n = (short)-n; }
        do { b[m++] = (char)('0' + n % 10); n = (short)(n / 10); } while (n);
        while (m) z[i++] = b[--m];
    }
    z[i] = 0; aus(z);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WORD r, n;
    (void)inst; (void)cmd; (void)show;
    df = FileCreate("JOY.TXT", 0);
    aus("JOYTEST: Start");
    DbgHex("JOYTEST: Kalibrieren A", JoyCalibrate(0));
    for (r = 0; r < 3; r++) { zeig(0); zeig(1); for (n = 0; n < 200; n++) TaskYield(); }
    aus("JOYTEST: Ende"); if (df >= 0) FileClose(df);
    return 0;
}
