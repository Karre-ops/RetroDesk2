/* KIFPU.C - Zielhilfe des Computergegners mit Koprozessor (8087/287/387).
 * Wird mit -fpi87 gebaut (8087-Befehle im Code) und nur aufgerufen, wenn KernInfo einen
 * Koprozessor meldet. Ohne 8087 sucht BURGEN.C die Ladung mit ganzzahligen Probeschuessen.
 * Die Flugbahn im Spiel selbst bleibt immer ganzzahlig (gleiches Ergebnis auf allen Rechnern im Netz).
 *
 * Geschlossene Loesung der Wurfparabel mit Wind (gleichmaessige Beschleunigung a in x):
 *   x(t) = v c t + a t^2 / 2 = dx,   y(t) = v s t - g t^2 / 2 = dy   (y nach oben)
 * Ohne Wind: v^2 = g dx^2 / (2 c^2 (dx s / c - dy)); mit Wind wird dx um a t^2 / 2 berichtigt (Iteration). */

static double wurzel(double a)                    /* Newton, ohne Bibliothek (kein Emulator, kein DOS) */
{
    double x = a > 1.0 ? a : 1.0, n; int i;
    if (a <= 0.0) return 0.0;
    for (i = 0; i < 40; i++) { n = 0.5 * (x + a / x); if (n == x) break; x = n; }
    return x;
}

/* dx > 0 Weite, dy Hoehe des Ziels ueber dem Abschuss (Welteinheiten), s/c = sin/cos * 1024,
 * wind = Beschleunigung in Schussrichtung in 1/1024 je Schritt^2. g = 52/1024, Ladung 1 = 80/1024.
 * Liefert die Ladung * 16 (Bruchteile fuer die Feinsuche) oder 0 (Ziel so nicht erreichbar). */
unsigned ki_fpu_ladung(short dx, short dy, short s, short c, short wind)
{
    double g = 52.0 / 1024.0, a = wind / 1024.0, sn = s / 1024.0, cs = c / 1024.0, x = dx, v = 0.0, t, n, l;
    int i;
    if (cs <= 0.0 || dx <= 0) return 0;
    for (i = 0; i < 8; i++) {
        n = 2.0 * cs * cs * (x * sn / cs - dy);
        if (n <= 0.0) return 0;
        v = wurzel(g * x * x / n);
        if (v <= 0.0) return 0;
        t = x / (v * cs);
        x = dx - 0.5 * a * t * t;                 /* Wind traegt die Kugel mit: Rest selbst schaffen */
        if (x <= 1.0) x = 1.0;
    }
    l = v * 1024.0 / 80.0 * 16.0 + 0.5;
    if (l < 16.0) return 16;
    if (l > 1600.0) return 0;
    { unsigned r = 0, k; double d = 1024.0;       /* ganzzahlig machen ohne __CHP-Rundungshilfe der clib */
      for (k = 1024; k; k >>= 1, d *= 0.5) if (l >= d) { l -= d; r |= k; }
      return r; }
}
