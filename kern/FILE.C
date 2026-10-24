/* FILE.C - Dateien, Laufwerke, Diskettenwechsel, INT 24h, Profile (INI), Zwischenablage
 *
 * Alle DOS-Aufrufe gehen ueber dos21(): INT 24h ist auf "Fail" gestellt
 * (KASM.ASM), der Kern fragt selbst nach ("Bitte Diskette einlegen") und
 * wiederholt. So erscheint nie "Abort, Retry, Fail". Die Abfrage ist bis
 * Phase 4 ein Text ueber das BIOS, danach ein Dialog. */
#include "KERN.H"

static KREGS kin;
static BYTE fown[64];                             /* DOS-Handle -> Task */
static char msg[64];                              /* "Bitte Diskette <11> in Laufwerk A: einlegen" */

/* ---- Abfrage an den Benutzer -------------------------------------------- */
static void bios_out(LPCSTR s)
{
    for (; *s; s++) { kr.ax = 0x0E00 | (BYTE)*s; kr.bx = 0x0007; rd_int(0x10, &kr); }
}
/* Text zeigen, auf Enter (1) oder Esc (0) warten. Auch auf COM1 (fuer Tests). */
int k_ask(LPCSTR text)
{
    k_out(text, 0);
    bios_out("\r\n"); bios_out(text); bios_out(" [Enter/Esc]");
    for (;;) {
        kr.ax = 0x0100; rd_int(0x16, &kr);
        if (!(kr.flags & 0x40)) {                 /* ZF = 0: Taste da */
            kr.ax = 0x0000; rd_int(0x16, &kr);
            if ((kr.ax & 0xFF) == 13) { bios_out("\r\n"); k_out("RD: Abfrage: Enter", 0); return 1; }
            if ((kr.ax & 0xFF) == 27) { bios_out("\r\n"); k_out("RD: Abfrage: Esc", 0); return 0; }
        } else rd_idle();
    }
}
static WORD mn;
static void put(LPCSTR s) { while (*s && mn < sizeof msg - 1) msg[mn++] = *s++; msg[mn] = 0; }
static int ask_insert(WORD drive, LPCSTR label)  /* drive 0 = A: */
{
    static char lw[3];
    lw[0] = (char)('A' + drive); lw[1] = ':'; lw[2] = 0;
    mn = 0;
    put("Bitte Diskette "); if (label && *label) { put(label); put(" "); }
    put("in Laufwerk "); put(lw); put(" einlegen");
    return k_ask(msg);
}

/* ---- DOS-Aufruf mit Fehlerbehandlung --------------------------------------- */
static int dos21(void)
{
    kin = kr;
    for (;;) {
        kr = kin;
        k_crit = 0;
        rd_int(0x21, &kr);
        if (!CARRY) return 1;
        if (k_crit) {
            WORD code = k_crit - 1, drv = k_critdrv & 0xFF;
            if (code == 0) { k_lasterr = RD_ERR_WPROT; return 0; }
            if (code == 2 && drv < 2) {           /* Laufwerk nicht bereit */
                if (ask_insert(drv, 0)) continue;
                k_lasterr = RD_ERR_NODISK; return 0;
            }
            k_lasterr = RD_ERR_DISK; return 0;
        }
        switch (kr.ax) {
        case 2: case 3: case 18: k_lasterr = RD_ERR_NOTFOUND; break;
        case 4: k_lasterr = RD_ERR_FULL; break;
        case 5: k_lasterr = RD_ERR_WPROT; break;
        case 8: k_lasterr = RD_ERR_NOMEM; break;
        default: k_lasterr = RD_ERR_DISK;
        }
        return 0;
    }
}

int k_dos(void) { return dos21(); }
int k_askdisk(WORD drive, LPCSTR label) { return ask_insert(drive, label); }

/* Ein Laufwerk fuer A: und B:? Dann selbst fragen, bevor DOS es tut. */
static void prep(LPCSTR path)
{
    WORD d;
    if (path && path[0] && path[1] == ':') d = (WORD)((path[0] | 0x20) - 'a');
    else { kr.ax = 0x1900; rd_int(0x21, &kr); d = kr.ax & 0xFF; }
    if (d > 1) return;
    kr.ax = 0x440E; kr.bx = d + 1; rd_int(0x21, &kr);
    if (CARRY || !(kr.ax & 0xFF) || (kr.ax & 0xFF) == d + 1) return;
    ask_insert(d, 0);
    kr.ax = 0x440F; kr.bx = d + 1; rd_int(0x21, &kr);
}
#define SETNAME(p) (kr.dx = FP_OFF(p), kr.ds = FP_SEG(p))

/* ---- Dateien ---------------------------------------------------------------- */
short RDK FileOpen(LPCSTR name, WORD mode)
{
    prep(name);
    kr.ax = 0x3D00 | (mode & 0x73); SETNAME(name);
    if (!dos21()) return -1;
    if (kr.ax < 64) fown[kr.ax] = (BYTE)k_cur;
    return (short)kr.ax;
}
short RDK FileCreate(LPCSTR name, WORD attr)
{
    prep(name);
    kr.ax = 0x3C00; kr.cx = attr; SETNAME(name);
    if (!dos21()) return -1;
    if (kr.ax < 64) fown[kr.ax] = (BYTE)k_cur;
    return (short)kr.ax;
}
WORD RDK FileRead(short f, void FAR *buf, WORD n)
{
    kr.ax = 0x3F00; kr.bx = f; kr.cx = n; SETNAME(buf);
    return dos21() ? kr.ax : 0;
}
WORD RDK FileWrite(short f, const void FAR *buf, WORD n)
{
    kr.ax = 0x4000; kr.bx = f; kr.cx = n; SETNAME(buf);
    if (!dos21()) return 0;
    if (kr.ax < n) k_lasterr = RD_ERR_FULL;
    return kr.ax;
}
long RDK FileSeek(short f, long pos, WORD art)
{
    kr.ax = 0x4200 | (art & 3); kr.bx = f; kr.cx = (WORD)((DWORD)pos >> 16); kr.dx = (WORD)pos;
    return dos21() ? (long)(((DWORD)kr.dx << 16) | kr.ax) : -1L;
}
WORD RDK FileClose(short f)
{
    kr.ax = 0x3E00; kr.bx = f;
    if (f >= 0 && f < 64) fown[f] = 0;
    return dos21() ? 0 : k_lasterr;
}
WORD RDK FileDelete(LPCSTR name)  { prep(name); kr.ax = 0x4100; SETNAME(name); return dos21() ? 0 : k_lasterr; }
WORD RDK FileRename(LPCSTR a, LPCSTR b)
{
    prep(a); kr.ax = 0x5600; SETNAME(a); kr.di = FP_OFF(b); kr.es = FP_SEG(b);
    return dos21() ? 0 : k_lasterr;
}
static void setdta(FINDDATA FAR *fd) { kr.ax = 0x1A00; SETNAME(fd); rd_int(0x21, &kr); }
WORD RDK FileFindFirst(LPCSTR muster, WORD attr, FINDDATA FAR *fd)
{
    prep(muster); setdta(fd);
    kr.ax = 0x4E00; kr.cx = attr; SETNAME(muster);
    return dos21() ? 0 : k_lasterr;
}
WORD RDK FileFindNext(FINDDATA FAR *fd) { setdta(fd); kr.ax = 0x4F00; return dos21() ? 0 : k_lasterr; }
WORD RDK FileAttr(LPCSTR name, WORD neu, WORD setzen)
{
    prep(name); kr.ax = setzen ? 0x4301 : 0x4300; kr.cx = neu; SETNAME(name);
    return dos21() ? kr.cx : 0xFFFF;
}
WORD RDK DirMake(LPCSTR n)   { prep(n); kr.ax = 0x3900; SETNAME(n); return dos21() ? 0 : k_lasterr; }
WORD RDK DirRemove(LPCSTR n) { prep(n); kr.ax = 0x3A00; SETNAME(n); return dos21() ? 0 : k_lasterr; }
WORD RDK DirChange(LPCSTR n)
{
    prep(n);
    if (n[0] && n[1] == ':') { kr.ax = 0x0E00; kr.dx = (WORD)((n[0] | 0x20) - 'a'); rd_int(0x21, &kr); }
    kr.ax = 0x3B00; SETNAME(n); return dos21() ? 0 : k_lasterr;
}
WORD RDK DirGet(WORD drive, LPSTR buf)
{
    WORD d = drive;
    if (!d) { kr.ax = 0x1900; rd_int(0x21, &kr); d = (kr.ax & 0xFF) + 1; }
    buf[0] = (char)('A' + d - 1); buf[1] = ':'; buf[2] = '\\';
    kr.ax = 0x4700; kr.dx = d; kr.si = FP_OFF(buf) + 3; kr.ds = FP_SEG(buf);
    return dos21() ? 0 : k_lasterr;
}

/* Seriennummer und Name des Datentraegers (INT 21h 6900h). 0 = ok */
static int strncmp_k(const char *a, const char *b) { while (*b && *a == *b) a++, b++; return *b || *a; }
/* Seriennummer/Name der eingelegten Diskette aus dem Bootsektor (INT 25h: der Treiber
   prueft dabei den Wechsel; 6900h lieferte unter FreeDOS manchmal die alte Diskette). */
static BYTE bootsek[512];
static DWORD ms; static char mlab[12];           /* DriveInfo, DiskRequest, ProfileGetInt (nie gleichzeitig) */
static WORD media(WORD drive, DWORD *serial, char *label)
{
    WORD i, e;
    kr.ax = 0x440E; kr.bx = drive + 1; rd_int(0x21, &kr);          /* Phantomlaufwerk ohne DOS-Text umschalten */
    if (!CARRY && (kr.ax & 0xFF) && (kr.ax & 0xFF) != drive + 1) { kr.ax = 0x440F; kr.bx = drive + 1; rd_int(0x21, &kr); }
    for (i = 0; i < 3; i++) if (!(e = rd_absread(drive, bootsek))) break;   /* Motor anlaufen lassen */
    if (e) return k_lasterr = ((e & 0xFF) == 2) ? RD_ERR_NODISK : RD_ERR_DISK;
    *serial = bootsek[0x26] == 0x29 ? *(DWORD *)(bootsek + 0x27) : 0;
    if (label) {
        for (i = 0; i < 11; i++) label[i] = bootsek[0x26] == 0x29 ? bootsek[0x2B + i] : ' ';
        label[11] = 0;
        for (i = 11; i > 0 && label[i - 1] == ' '; i--) label[i - 1] = 0;
        if (!strncmp_k(label, "NO NAME")) label[0] = 0;
    }
    return 0;
}
WORD k_media(WORD drive, DWORD *serial, char *label) { return media(drive, serial, label); }

WORD RDK DriveInfo(WORD drive, DRIVEINFO FAR *di)       /* drive 1 = A: */
{
    WORD i, d = drive & 0xFF;
    BYTE FAR *p = (BYTE FAR *)di;
    for (i = 0; i < sizeof(DRIVEINFO); i++) p[i] = 0;
    if (!d) { kr.ax = 0x1900; rd_int(0x21, &kr); d = (kr.ax & 0xFF) + 1; }
    kr.ax = 0x4408; kr.bx = d; rd_int(0x21, &kr);
    di->typ = CARRY ? DT_NONE : (kr.ax == 0 ? DT_FLOPPY : DT_HARD);
    kr.ax = 0x4409; kr.bx = d; rd_int(0x21, &kr);
    if (!CARRY && (kr.dx & 0x1000)) di->typ = DT_NET;
    kr.ax = 0x440E; kr.bx = d; rd_int(0x21, &kr);
    if (!CARRY && (kr.ax & 0xFF) && (kr.ax & 0xFF) != d) di->flags |= DI_PHANTOM;
    if ((drive & DI_NURTYP) || (di->typ == DT_FLOPPY && (di->flags & DI_PHANTOM))) return 0;   /* nicht nachfragen */
    kr.ax = 0x3600; kr.dx = d;
    if (!dos21() || kr.ax == 0xFFFF) return k_lasterr ? k_lasterr : RD_ERR_NOTFOUND;
    di->frei = (DWORD)kr.ax * kr.cx * kr.bx;
    di->gesamt = (DWORD)kr.ax * kr.cx * kr.dx;
    if (di->typ == DT_FLOPPY) {
        DWORD kb = di->gesamt >> 10;
        di->format_kb = kb > 2000 ? 2880 : kb > 1300 ? 1440 : kb > 1000 ? 1200 : kb > 600 ? 720 : kb > 300 ? 360 : 180;
    }
    if (!media(d - 1, &ms, mlab)) { di->serial = ms; for (i = 0; i < 12; i++) di->label[i] = mlab[i]; }
    return 0;
}

WORD RDK DiskRequest(WORD drive, DWORD serial, LPCSTR label)   /* drive 1 = A: */
{
    WORD i;
    for (;;) {
        /* Seriennummer UND Name vergleichen: zwei im selben Moment formatierte Disketten
           koennen dieselbe Seriennummer haben */
        if (!media(drive - 1, &ms, mlab) && ms == serial) {
            if (!label || !*label) return 0;
            for (i = 0; i < 11 && label[i] == mlab[i] && mlab[i]; i++) ;
            if (label[i] == mlab[i] || (i == 11)) return 0;
        }
        if (!ask_insert(drive - 1, label)) return k_lasterr = RD_ERR_NODISK;
    }
}

WORD RDK FileSaveSafe(LPCSTR name, const void FAR *buf, DWORD n)
{
    static char tmp[80]; WORD i, j; short f; const BYTE FAR *p = (const BYTE FAR *)buf;
    for (i = 0; i < 76 && name[i]; i++) tmp[i] = name[i];
    for (j = i; j > 0 && tmp[j - 1] != '.' && tmp[j - 1] != '\\' && tmp[j - 1] != ':'; j--) ;
    if (j > 0 && tmp[j - 1] == '.') i = j - 1;
    tmp[i] = '.'; tmp[i + 1] = '$'; tmp[i + 2] = '$'; tmp[i + 3] = '$'; tmp[i + 4] = 0;
    if ((f = FileCreate(tmp, 0)) < 0) return k_lasterr;
    while (n) {
        WORD k = n > 32768UL ? 32768U : (WORD)n;
        if (FileWrite(f, p, k) != k) { FileClose(f); FileDelete(tmp); return k_lasterr ? k_lasterr : RD_ERR_FULL; }
        p = (const BYTE FAR *)MK_FP(FP_SEG(p) + (k >> 4), FP_OFF(p));
        n -= k;
    }
    FileClose(f);
    FileDelete(name);                             /* darf fehlen */
    return FileRename(tmp, name);
}

void file_owner_close(WORD task)
{
    WORD i;
    for (i = 5; i < 64; i++) if (fown[i] == task && task) { kr.ax = 0x3E00; kr.bx = i; rd_int(0x21, &kr); fown[i] = 0; }
}

/* ---- Profile (INI) --------------------------------------------------------- */
#define PMAX 16384
static BYTE __far *pb; static WORD pn, pbig;      /* geladene Datei; pbig = groesser als PMAX */
static BYTE lc(BYTE c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static void pfree(void);
static int pload(LPCSTR file)
{
    short f; void FAR *m;
    pn = 0; pb = 0;
    if (!(m = MemFixed(PMAX + 512))) return 0;
    pb = (BYTE __far *)m;
    pbig = 0;
    if ((f = FileOpen(file, FO_READ)) >= 0) { k_lasterr = 0; pn = FileRead(f, pb, PMAX); if (pn == PMAX && FileRead(f, pb + PMAX, 1)) pbig = 1; FileClose(f); }
    else if (k_lasterr == RD_ERR_NOTFOUND) k_lasterr = 0;   /* nur "gibt es nicht" heisst leer */
    if (k_lasterr) { pfree(); return 0; }         /* sonst nie mit halbem Inhalt neu schreiben */
    return 1;
}
static void pfree(void) { if (pb) MemFixedFree(pb); pb = 0; }
/* Zeile ab Position i: Ende (ohne CR/LF) und Anfang der naechsten */
static WORD eol(WORD i) { while (i < pn && pb[i] != '\r' && pb[i] != '\n') i++; return i; }
static WORD nextl(WORD i) { i = eol(i); if (i < pn && pb[i] == '\r') i++; if (i < pn && pb[i] == '\n') i++; return i; }
static WORD skipsp(WORD i, WORD e) { while (i < e && (pb[i] == ' ' || pb[i] == '\t')) i++; return i; }
static int match(WORD i, WORD e, LPCSTR s, BYTE term)  /* pb[i..] == s (ohne Gross/Klein), dann term */
{
    while (*s) { if (i >= e || lc(pb[i]) != lc((BYTE)*s)) return 0; i++; s++; }
    i = skipsp(i, e);
    return i < e && pb[i] == term;
}
/* Sucht Abschnitt und Schluessel. *sec_end = hinter der letzten nicht leeren Zeile des
   Abschnitts (Einfuegestelle) oder 0xFFFF; Rueckgabe Zeilenanfang des Schluessels oder 0xFFFF */
static WORD pfind(LPCSTR sec, LPCSTR key, WORD *sec_end, WORD *val)
{
    WORD i = 0, e, a, nl, insec = 0, found = 0xFFFF;
    *sec_end = 0xFFFF;
    while (i < pn) {
        e = eol(i); a = skipsp(i, e); nl = nextl(i);
        if (a < e && pb[a] == '[') {
            insec = match(a + 1, e, sec, ']');
            if (insec) *sec_end = nl;
        } else if (insec && a < e) {
            *sec_end = nl;
            if (pb[a] != ';' && found == 0xFFFF && match(a, e, key, '=')) {
                found = i;
                while (pb[a] != '=') a++;
                *val = skipsp(a + 1, e);
            }
        }
        i = nl;
    }
    return found;
}
WORD RDK ProfileGetStr(LPCSTR file, LPCSTR sec, LPCSTR key, LPCSTR def, LPSTR buf, WORD len)
{
    static WORD se, v; WORD n = 0, e, l;
    if (!len) return 0;
    if (pload(file) && pfind(sec, key, &se, &v) != 0xFFFF) {
        e = eol(v);
        while (e > v && (pb[e - 1] == ' ' || pb[e - 1] == '\t')) e--;
        while (v < e && n + 1 < len) buf[n++] = pb[v++];
    } else if (def) while (def[n] && n + 1 < len) { buf[n] = def[n]; n++; }
    buf[n] = 0; l = n;
    pfree();
    return l;
}
short RDK ProfileGetInt(LPCSTR file, LPCSTR sec, LPCSTR key, short def)
{
    char *b = mlab; WORD i = 0, neg = 0; short v = 0;
    if (!ProfileGetStr(file, sec, key, "", b, sizeof mlab)) return def;
    if (b[0] == '-') { neg = 1; i = 1; }
    if (b[i] < '0' || b[i] > '9') return def;
    for (; b[i] >= '0' && b[i] <= '9'; i++) v = v * 10 + (b[i] - '0');
    return neg ? -v : v;
}
/* Bytes in pb ab Stelle at durch s ersetzen (del Bytes entfernen); passt es nicht: pvoll = 1 */
static WORD pvoll;
static int pput(WORD at, WORD del, LPCSTR s1, LPCSTR s2, LPCSTR s3)
{
    WORD n = 0, i; LPCSTR p;
    for (p = s1; p && *p; p++) n++;
    for (p = s2; p && *p; p++) n++;
    for (p = s3; p && *p; p++) n++;
    if (pn - del + n > PMAX + 500) { pvoll = 1; return 0; }
    if (n > del) for (i = pn; i > at + del; i--) pb[i - 1 + n - del] = pb[i - 1];
    else if (n < del) for (i = at + del; i < pn; i++) pb[i - del + n] = pb[i];
    pn = pn - del + n;
    for (p = s1; p && *p; p++) pb[at++] = *p;
    for (p = s2; p && *p; p++) pb[at++] = *p;
    for (p = s3; p && *p; p++) pb[at++] = *p;
    return 1;
}
WORD RDK ProfileSetStr(LPCSTR file, LPCSTR sec, LPCSTR key, LPCSTR val)
{
    static WORD se, v; WORD line, r;
    if (!pload(file)) return k_lasterr ? k_lasterr : (k_lasterr = RD_ERR_NOMEM);
    if (pbig) { pfree(); return k_lasterr = RD_ERR_FULL; }   /* nie abgeschnitten zurueckschreiben */
    pvoll = 0;
    line = pfind(sec, key, &se, &v);
    if (line != 0xFFFF) {
        WORD nl = nextl(line);
        if (val) { WORD e = eol(v); pput(v, e - v, val, 0, 0); }
        else pput(line, nl - line, 0, 0, 0);    /* Schluessel loeschen */
    } else if (val) {
        if (se != 0xFFFF) {
            if (se > 0 && pb[se - 1] != '\n') { pput(se, 0, "\r\n", 0, 0); se += 2; }
            pput(se, 0, "\r\n", 0, 0);
            pput(se, 0, key, "=", val);           /* vor das CR/LF: "key=val\r\n" */
        } else {
            if (pn && pb[pn - 1] != '\n') pput(pn, 0, "\r\n", 0, 0);
            pput(pn, 0, "[", sec, "]\r\n");
            pput(pn, 0, key, "=", val);
            pput(pn, 0, "\r\n", 0, 0);
        }
    } else pvoll = 2;                             /* nichts zu loeschen: Datei nicht anfassen */
    if (pvoll) { pfree(); return pvoll == 1 ? (k_lasterr = RD_ERR_FULL) : 0; }   /* passt nicht: Datei unveraendert */
    r = FileSaveSafe(file, pb, pn);
    pfree();
    return r;
}

/* ---- Zwischenablage -------------------------------------------------------- */
static HMEM clip[4]; static WORD clipown;
BOOL RDK ClipOpen(HWND owner) { if (clipown && clipown != k_cur) return FALSE; clipown = k_cur; (void)owner; return TRUE; }
void RDK ClipClose(void)      { if (clipown == k_cur) clipown = 0; }
void RDK ClipEmpty(void)      { WORD i; for (i = 1; i < 4; i++) if (clip[i]) { MemFree(clip[i]); clip[i] = 0; } }
BOOL RDK ClipSet(WORD fmt, HMEM h)
{
    if (fmt < 1 || fmt > 3 || clipown != k_cur) return FALSE;
    if (clip[fmt] && clip[fmt] != h) MemFree(clip[fmt]);
    clip[fmt] = h;
    mem_share(h);                                 /* gehoert jetzt der Ablage */
    return TRUE;
}
void clip_done(void) { WORD c = clipown; clipown = 0; ClipEmpty(); clipown = c; }
HMEM RDK ClipGet(WORD fmt) { return (fmt >= 1 && fmt <= 3) ? clip[fmt] : 0; }

void file_api(void)
{
    kr.ax = 0x6700; kr.bx = 40; rd_int(0x21, &kr);      /* bis zu 40 Dateien gleichzeitig */
    KAPI(FileOpen, FileOpen); KAPI(FileCreate, FileCreate); KAPI(FileRead, FileRead); KAPI(FileWrite, FileWrite);
    KAPI(FileSeek, FileSeek); KAPI(FileClose, FileClose); KAPI(FileDelete, FileDelete); KAPI(FileRename, FileRename);
    KAPI(FileFindFirst, FileFindFirst); KAPI(FileFindNext, FileFindNext); KAPI(FileAttr, FileAttr);
    KAPI(DirMake, DirMake); KAPI(DirRemove, DirRemove); KAPI(DirChange, DirChange); KAPI(DirGet, DirGet);
    KAPI(DriveInfo, DriveInfo); KAPI(DiskRequest, DiskRequest); KAPI(FileSaveSafe, FileSaveSafe);
    KAPI(ProfileGetStr, ProfileGetStr); KAPI(ProfileGetInt, ProfileGetInt); KAPI(ProfileSetStr, ProfileSetStr);
    KAPI(ClipOpen, ClipOpen); KAPI(ClipClose, ClipClose); KAPI(ClipEmpty, ClipEmpty); KAPI(ClipSet, ClipSet); KAPI(ClipGet, ClipGet);
}
