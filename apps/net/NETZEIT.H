/* NETZEIT.H - Zeitzonen, Sommerzeit und NTP-Zeit (wie RD1 net.c). Fuer RDNET.EXE und UHR.EXE.
 * Einstellungen in RD2.INI [Netz]: NTP= (leer: NTPDHCP=, sonst pool.ntp.org), ZoneName=, Zone= (Minuten zu UTC),
 * Sommer= (0 keine, 1 EU, 2 USA, 3 Australien, 4 Neuseeland), Zeit=1 (Uhr beim Start von RD2 stellen). */
typedef struct { const char *name; short min; BYTE regel; } ZONE;
static const ZONE zonen[] = {
    { "UTC", 0, 0 }, { "London, Lissabon", 0, 1 }, { "Berlin, Paris, Rom", 60, 1 },
    { "Athen, Helsinki", 120, 1 }, { "Istanbul, Moskau", 180, 0 }, { "Dubai", 240, 0 },
    { "Indien", 330, 0 }, { "Bangkok", 420, 0 }, { "China, Singapur", 480, 0 },
    { "Japan, Korea", 540, 0 }, { "Brisbane", 600, 0 }, { "Sydney, Melbourne", 600, 3 },
    { "Neuseeland", 720, 4 }, { "Hawaii", -600, 0 }, { "Alaska", -540, 2 },
    { "Los Angeles", -480, 2 }, { "Denver", -420, 2 }, { "Phoenix", -420, 0 },
    { "Chicago", -360, 2 }, { "New York", -300, 2 }, { "Brasilia", -180, 0 },
    { "*", 0, 0 } };                              /* "*" = eigene Abweichung */
#define NZONEN (sizeof zonen / sizeof zonen[0])
#define ZONE_EIGEN (NZONEN - 1)
#define NTP_1970 2208988800UL

static long nz_tage(WORD y, BYTE m, BYTE d)       /* Tage seit 1.1.1970 */
{
    long era; WORD yoe, doy;
    if (m <= 2) y--;
    era = y / 400; yoe = (WORD)(y - era * 400);
    doy = (WORD)((153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1);
    return era * 146097L + (long)yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468L;
}
static void nz_datum(long z, RDTIME *t)
{
    long era; DWORD doe; WORD yoe, doy, mp;
    z += 719468L; era = z / 146097L; doe = (DWORD)(z - era * 146097L);
    yoe = (WORD)((doe - doe / 1460 + doe / 36524 - doe / 146096) / 365);
    doy = (WORD)(doe - (365L * yoe + yoe / 4 - yoe / 100));
    mp = (5 * doy + 2) / 153;
    t->tag = (BYTE)(doy - (153 * mp + 2) / 5 + 1); t->monat = (BYTE)(mp < 10 ? mp + 3 : mp - 9);
    t->jahr = (WORD)(yoe + era * 400 + (t->monat <= 2));
    t->wtag = (BYTE)((z - 719468L + 4) % 7);       /* 1.1.1970 war Donnerstag, 0 = Sonntag */
}
static long nz_so(WORD j, BYTE m, short n)        /* n-ter Sonntag (n < 0: letzter) als Tag */
{
    long t;
    if (n < 0) { t = nz_tage(m == 12 ? j + 1 : j, (BYTE)(m == 12 ? 1 : m + 1), 1) - 1; return t - (t + 4) % 7; }
    t = nz_tage(j, m, 1); t += (7 - (t + 4) % 7) % 7;
    return t + 7L * (n - 1);
}
static BYTE nz_sommer(DWORD utc, short zmin, BYTE regel)   /* utc: Sekunden seit 1970 */
{
    RDTIME d; DWORD a, e; long std = (long)zmin * 60;
    if (!regel) return 0;
    nz_datum((long)(utc / 86400UL), &d);
    switch (regel) {
    case 1: a = nz_so(d.jahr, 3, -1) * 86400UL + 3600; e = nz_so(d.jahr, 10, -1) * 86400UL + 3600; return utc >= a && utc < e;
    case 2: a = nz_so(d.jahr, 3, 2) * 86400UL + 7200 - std; e = nz_so(d.jahr, 11, 1) * 86400UL + 3600 - std; return utc >= a && utc < e;
    case 3: a = nz_so(d.jahr, 10, 1) * 86400UL + 7200 - std; e = nz_so(d.jahr, 4, 1) * 86400UL + 7200 - std; return utc >= a || utc < e;
    case 4: a = nz_so(d.jahr, 9, -1) * 86400UL + 7200 - std; e = nz_so(d.jahr, 4, 1) * 86400UL + 7200 - std; return utc >= a || utc < e;
    }
    return 0;
}
static void nz_ortszeit(DWORD ntp, short zmin, BYTE regel, RDTIME *t)   /* NTP-Sekunden (seit 1900) -> Ortszeit */
{
    DWORD u = ntp - NTP_1970, l = u + (long)zmin * 60 + (nz_sommer(u, zmin, regel) ? 3600 : 0);
    nz_datum((long)(l / 86400UL), t);
    l %= 86400UL; t->std = (BYTE)(l / 3600); t->min = (BYTE)(l / 60 % 60); t->sek = (BYTE)(l % 60); t->hsek = 0;
}
