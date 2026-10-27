/* RD2API.H - Liste aller RD2-Funktionen mit fester Nummer (API 1.0)
 *
 * Diese Datei ist die einzige Quelle fuer Nummern und Signaturen.
 * Sie wird mit verschiedenen Definitionen von RDF() mehrfach eingebunden:
 *   - RD2.H:     Prototypen fuer Programme
 *   - RD2.LIB:   Aufrufstuecke (call far [api + nummer*4])
 *   - Kern:      Sprungtabelle
 *   - INT 7Eh:   AX = Nummer, Parameter auf dem Stack (pascal)
 *
 * REGELN (nie brechen, sonst laufen alte Programme nicht mehr):
 *   1. Eine vergebene Nummer wird nie neu belegt und nie entfernt.
 *   2. Signaturen werden nie geaendert. Neue Faehigkeit = neue Funktion.
 *   3. Neue Funktionen bekommen die naechste freie Nummer ihrer Gruppe
 *      und erhoehen RD2_API_MINOR.
 *   4. Nicht (mehr) unterstuetzte Funktionen liefern RD_ERR_NOTSUP.
 *
 * Gruppen:  000-07F Kern   080-0BF Speicher   0C0-0FF Datei
 *           100-17F Grafik 180-1FF reserviert 200-2BF Oberflaeche
 *           2C0-2FF Dialoge 300-33F Sound     340-37F Eingabe
 *           380-3BF Netz   3C0-3DF Drucker    3E0-3FF Seriell (spaeter)
 *
 * RDF(nummer, rueckgabe, name, (parameter))
 */

/* ---- Kern: Version, System, Tasks, Module ---------------------------- */
RDF(0x000, WORD,    KernVersion,    (void))                       /* hi=major lo=minor */
RDF(0x001, WORD,    KernInfo,       (SYSINFO FAR *si))            /* 0 = ok */
RDF(0x002, HTASK,   TaskSelf,       (void))
RDF(0x003, void,    TaskYield,      (void))
RDF(0x004, HTASK,   TaskExec,       (LPCSTR app, LPCSTR cmd, WORD show))
RDF(0x005, WORD,    TaskKill,       (HTASK t, WORD code))
RDF(0x006, HTASK,   TaskFirst,      (TASKINFO FAR *ti))
RDF(0x007, HTASK,   TaskNext,       (HTASK t, TASKINFO FAR *ti))
RDF(0x008, HMOD,    ModLoad,        (LPCSTR name))                /* .MOD/.DRV nachladen */
RDF(0x009, void,    ModFree,        (HMOD m))
RDF(0x00A, FARPROC, ModProc,        (HMOD m, WORD nummer))
RDF(0x00B, WORD,    RunDos,         (LPCSTR prog, LPCSTR cmd, WORD flags)) /* RD_DOS_* */
RDF(0x00C, WORD,    LastError,      (void))                       /* RD_ERR_* */
RDF(0x00D, void,    SetLastError,   (WORD e))
RDF(0x00E, void,    ExitRD,         (WORD flags))                 /* Shell: RetroDesk beenden */
RDF(0x00F, DWORD,   InstRes,        (HINST i, LPSTR pfad, WORD len)) /* Datei und RD2-Block eines Programms (fuer Ressourcen), 0 = keine */

/* ---- Kern: Nachrichten ------------------------------------------------ */
RDF(0x010, BOOL,    GetMessage,     (MSG FAR *m))                 /* FALSE bei M_QUIT */
RDF(0x011, BOOL,    PeekMessage,    (MSG FAR *m, WORD remove))
RDF(0x012, BOOL,    PostMessage,    (HWND w, WORD msg, WORD p1, DWORD p2))
RDF(0x013, DWORD,   SendMessage,    (HWND w, WORD msg, WORD p1, DWORD p2))
RDF(0x014, DWORD,   DispatchMessage,(const MSG FAR *m))
RDF(0x015, void,    PostQuit,       (WORD code))
RDF(0x016, void,    WaitMessage,    (void))
RDF(0x017, BOOL,    PostTask,       (HTASK t, WORD msg, WORD p1, DWORD p2))
RDF(0x018, WORD,    MsgRegister,    (LPCSTR name))                /* eigene Nachricht fuer Programme untereinander */

/* ---- Kern: Zeit ---------------------------------------------------------- */
RDF(0x020, WORD,    TimerSet,       (HWND w, WORD id, WORD ms))
RDF(0x021, void,    TimerKill,      (HWND w, WORD id))
RDF(0x022, DWORD,   TickCount,      (void))                       /* ms seit Start */
RDF(0x023, void,    TimeGet,        (RDTIME FAR *t))
RDF(0x024, WORD,    TimeSet,        (const RDTIME FAR *t, WORD bios))

/* ---- Kern: Profil, Zwischenablage, Ressourcen, Sprache ----------------- */
RDF(0x030, WORD,    ProfileGetStr,  (LPCSTR file, LPCSTR sec, LPCSTR key, LPCSTR def, LPSTR buf, WORD len))
RDF(0x031, short,   ProfileGetInt,  (LPCSTR file, LPCSTR sec, LPCSTR key, short def))
RDF(0x032, WORD,    ProfileSetStr,  (LPCSTR file, LPCSTR sec, LPCSTR key, LPCSTR val))
RDF(0x038, BOOL,    ClipOpen,       (HWND owner))
RDF(0x039, BOOL,    ClipSet,        (WORD format, HMEM data))     /* Block gehoert dann der Ablage */
RDF(0x03A, HMEM,    ClipGet,        (WORD format))
RDF(0x03B, void,    ClipClose,      (void))
RDF(0x03C, void,    ClipEmpty,      (void))
RDF(0x040, HMEM,    ResLoad,        (HINST i, WORD typ, WORD id))
RDF(0x041, void,    ResFree,        (HMEM r))
RDF(0x042, WORD,    StrLoad,        (HINST i, WORD id, LPSTR buf, WORD len)) /* aus .LNG bzw. Ressource */
RDF(0x043, WORD,    LangGet,        (void))                       /* RD_LANG_* */
RDF(0x044, LPCSTR,  StrGet,         (HINST i, WORD id))           /* Zeiger gilt bis zum naechsten GetMessage */

/* ---- Kern: Fehlersuche ------------------------------------------------- */
RDF(0x070, void,    DbgPrint,       (LPCSTR text))                /* COM1 oder RD2DBG.TXT */
RDF(0x071, void,    DbgHex,         (LPCSTR text, DWORD wert))
RDF(0x072, void,    DbgBreak,       (void))                       /* INT 3 wenn Debugger da */

/* ---- Speicher ------------------------------------------------------------ */
RDF(0x080, HMEM,    MemAlloc,       (DWORD size, WORD flags))     /* MEM_* */
RDF(0x081, HMEM,    MemFree,        (HMEM h))                     /* 0 = ok */
RDF(0x082, void FAR *, MemLock,     (HMEM h))                     /* 0 = verworfen/Fehler */
RDF(0x083, BOOL,    MemUnlock,      (HMEM h))
RDF(0x084, HMEM,    MemRealloc,     (HMEM h, DWORD size, WORD flags))
RDF(0x085, DWORD,   MemSize,        (HMEM h))
RDF(0x086, WORD,    MemFlags,       (HMEM h))
RDF(0x087, DWORD,   MemAvail,       (WORD art))                   /* MEMA_* in Bytes */
RDF(0x088, DWORD,   MemCompact,     (DWORD wunsch))               /* liefert groessten freien Block */
RDF(0x089, void FAR *, MemFixed,    (WORD size))                  /* kleiner fester Block, wie malloc */
RDF(0x08A, void,    MemFixedFree,   (void FAR *p))

/* ---- Dateien und Laufwerke ---------------------------------------------- */
RDF(0x0C0, short,   FileOpen,       (LPCSTR name, WORD mode))     /* FO_*, -1 = Fehler */
RDF(0x0C1, short,   FileCreate,     (LPCSTR name, WORD attr))
RDF(0x0C2, WORD,    FileRead,       (short f, void FAR *buf, WORD n))
RDF(0x0C3, WORD,    FileWrite,      (short f, const void FAR *buf, WORD n))
RDF(0x0C4, long,    FileSeek,       (short f, long pos, WORD art))
RDF(0x0C5, WORD,    FileClose,      (short f))
RDF(0x0C6, WORD,    FileDelete,     (LPCSTR name))
RDF(0x0C7, WORD,    FileRename,     (LPCSTR alt, LPCSTR neu))
RDF(0x0C8, WORD,    FileFindFirst,  (LPCSTR muster, WORD attr, FINDDATA FAR *fd))
RDF(0x0C9, WORD,    FileFindNext,   (FINDDATA FAR *fd))
RDF(0x0CA, WORD,    FileAttr,       (LPCSTR name, WORD neu, WORD setzen))
RDF(0x0CB, WORD,    DirMake,        (LPCSTR name))
RDF(0x0CC, WORD,    DirRemove,      (LPCSTR name))
RDF(0x0CD, WORD,    DirChange,      (LPCSTR name))
RDF(0x0CE, WORD,    DirGet,         (WORD drive, LPSTR buf))      /* drive 0 = aktuell, 1 = A: */
RDF(0x0CF, WORD,    DriveInfo,      (WORD drive, DRIVEINFO FAR *di))
RDF(0x0D0, WORD,    DiskRequest,    (WORD drive, DWORD serial, LPCSTR label)) /* "Bitte Diskette ... einlegen" */
RDF(0x0D1, WORD,    FileSaveSafe,   (LPCSTR name, const void FAR *buf, DWORD n)) /* erst .$$$, dann umbenennen */

/* ---- Grafik (RDGDI) --------------------------------------------------- */
RDF(0x100, short,   GetCaps,        (HDC dc, WORD index))         /* CAPS_* */
RDF(0x101, COLOR,   SetPen,         (HDC dc, COLOR c))
RDF(0x102, COLOR,   SetBrush,       (HDC dc, COLOR c, WORD muster)) /* PAT_* */
RDF(0x103, WORD,    SetRop,         (HDC dc, WORD rop))           /* ROP_* */
RDF(0x104, HFONT,   SetFont,        (HDC dc, HFONT f))
RDF(0x105, COLOR,   SetTextColor,   (HDC dc, COLOR c))
RDF(0x106, COLOR,   SetBkColor,     (HDC dc, COLOR c))
RDF(0x107, WORD,    SetBkMode,      (HDC dc, WORD mode))          /* BK_* */
RDF(0x108, void,    SetOrigin,      (HDC dc, short x, short y))
RDF(0x109, void,    SetClip,        (HDC dc, const RECT FAR *r))  /* zusaetzlich zum Fenster-Clipping */
RDF(0x110, void,    Pixel,          (HDC dc, short x, short y, COLOR c))
RDF(0x111, COLOR,   GetPixel,       (HDC dc, short x, short y))
RDF(0x112, void,    MoveTo,         (HDC dc, short x, short y))
RDF(0x113, void,    LineTo,         (HDC dc, short x, short y))
RDF(0x114, void,    Line,           (HDC dc, short x0, short y0, short x1, short y1))
RDF(0x115, void,    Frame,          (HDC dc, const RECT FAR *r))
RDF(0x116, void,    FillRect,       (HDC dc, const RECT FAR *r))  /* mit aktuellem Pinsel */
RDF(0x117, void,    InvertRect,     (HDC dc, const RECT FAR *r))
RDF(0x118, void,    Ellipse,        (HDC dc, const RECT FAR *r, WORD fill))
RDF(0x119, void,    Polygon,        (HDC dc, const POINT FAR *p, WORD n, WORD fill))
RDF(0x11A, void,    FloodFill,      (HDC dc, short x, short y))
RDF(0x11B, void,    RoundRect,      (HDC dc, const RECT FAR *r, WORD radius, WORD fill))
RDF(0x120, void,    TextOut,        (HDC dc, short x, short y, LPCSTR s, short len)) /* len -1 = bis 0 */
RDF(0x121, short,   TextWidth,      (HDC dc, LPCSTR s, short len))
RDF(0x122, void,    TextBox,        (HDC dc, const RECT FAR *r, LPCSTR s, WORD flags)) /* TB_* umbrechen/zentrieren */
RDF(0x128, HFONT,   FontLoad,       (LPCSTR name, WORD hoehe, WORD stil)) /* FS_* */
RDF(0x129, void,    FontFree,       (HFONT f))
RDF(0x12A, WORD,    FontInfo,       (HFONT f, FONTINFO FAR *fi))
RDF(0x130, HBMP,    BmpCreate,      (WORD w, WORD h, WORD bpp))
RDF(0x131, void,    BmpFree,        (HBMP b))
RDF(0x132, HDC,     BmpDC,          (HBMP b))                     /* Zeichnen in die Bitmap */
RDF(0x133, void,    DCFree,         (HDC dc))
RDF(0x134, void,    BitBlt,         (HDC d, short x, short y, short w, short h, HDC s, short sx, short sy, WORD rop))
RDF(0x135, void,    StretchBlt,     (HDC d, const RECT FAR *dr, HDC s, const RECT FAR *sr, WORD rop))
RDF(0x136, HBMP,    BmpLoad,        (HINST i, WORD id))           /* Ressource oder 0 + Dateiname per BmpLoadFile */
RDF(0x137, HBMP,    BmpLoadFile,    (LPCSTR name))                /* .RDB, spaeter .BMP */
RDF(0x138, void,    IconDraw,       (HDC dc, short x, short y, HINST i, WORD id))
RDF(0x139, COLOR,   ColorNearest,   (HDC dc, COLOR c))
RDF(0x13A, WORD,    PaletteSet,     (HDC dc, WORD first, WORD n, const COLOR FAR *c)) /* nur 16/256 Farben */
RDF(0x13B, WORD,    GfxList,        (GFXINFO FAR *l, WORD max))   /* Grafikarten, die diese Hardware kann */
RDF(0x13C, WORD,    GfxSelect,      (WORD typ))                   /* GFX_*, 0 = automatisch; gilt ab naechstem Start */

/* ---- Oberflaeche (RDUSER) ---------------------------------------------- */
RDF(0x200, WORD,    RegisterClass,  (const WNDCLASS FAR *wc))
RDF(0x201, HWND,    CreateWindow,   (LPCSTR klasse, LPCSTR titel, DWORD stil, short x, short y, short w, short h, HWND eltern, WORD id, HINST i))
RDF(0x202, BOOL,    DestroyWindow,  (HWND w))
RDF(0x203, void,    ShowWindow,     (HWND w, WORD art))           /* SW_* */
RDF(0x204, void,    MoveWindow,     (HWND w, short x, short y, short cx, short cy, BOOL neu))
RDF(0x205, void,    SetWindowText,  (HWND w, LPCSTR s))
RDF(0x206, WORD,    GetWindowText,  (HWND w, LPSTR buf, WORD len))
RDF(0x207, void,    GetClientRect,  (HWND w, RECT FAR *r))
RDF(0x208, void,    GetWindowRect,  (HWND w, RECT FAR *r))        /* Bildschirmkoordinaten */
RDF(0x209, void,    InvalidateRect, (HWND w, const RECT FAR *r, BOOL loeschen)) /* r 0 = alles */
RDF(0x20A, void,    ValidateRect,   (HWND w, const RECT FAR *r))
RDF(0x20B, void,    UpdateWindow,   (HWND w))
RDF(0x20C, HDC,     BeginPaint,     (HWND w, PAINT FAR *ps))
RDF(0x20D, void,    EndPaint,       (HWND w, PAINT FAR *ps))
RDF(0x20E, HDC,     GetDC,          (HWND w))                     /* 0 = ganzer Schirm */
RDF(0x20F, void,    ReleaseDC,      (HWND w, HDC dc))
RDF(0x210, HWND,    SetFocus,       (HWND w))
RDF(0x211, HWND,    GetFocus,       (void))
RDF(0x212, HWND,    SetActive,      (HWND w))
RDF(0x213, HWND,    GetActive,      (void))
RDF(0x214, DWORD,   DefWindowProc,  (HWND w, WORD msg, WORD p1, DWORD p2))
RDF(0x215, DWORD,   SetWindowData,  (HWND w, short index, DWORD wert)) /* WD_* oder 0..n eigene */
RDF(0x216, DWORD,   GetWindowData,  (HWND w, short index))
RDF(0x217, HWND,    GetParent,      (HWND w))
RDF(0x218, HWND,    WindowFromPoint,(short x, short y))
RDF(0x219, void,    ClientToScreen, (HWND w, POINT FAR *p))
RDF(0x21A, void,    ScreenToClient, (HWND w, POINT FAR *p))
RDF(0x21B, BOOL,    IsWindow,       (HWND w))
RDF(0x21C, void,    EnableWindow,   (HWND w, BOOL an))
RDF(0x21D, void,    ScrollWindow,   (HWND w, short dx, short dy, const RECT FAR *r))
RDF(0x21E, HWND,    GetChild,       (HWND eltern, HWND nach))     /* naechstes Kind (oberstes zuerst), nach 0 = erstes */
RDF(0x220, void,    ScrollSet,      (HWND w, WORD balken, short pos, short max, short seite)) /* SB_* */
RDF(0x221, short,   ScrollGet,      (HWND w, WORD balken))
RDF(0x228, HWND,    CaptureMouse,   (HWND w))
RDF(0x229, void,    ReleaseMouse,   (void))
RDF(0x22A, void,    CursorSet,      (HINST i, WORD id))           /* 0,CUR_* = Standardzeiger */
RDF(0x22B, void,    CursorBusy,     (BOOL an))                    /* Sanduhr */
RDF(0x22C, void,    CursorPos,      (POINT FAR *p))
RDF(0x230, HMENU,   MenuLoad,       (HINST i, WORD id))
RDF(0x231, void,    MenuSet,        (HWND w, HMENU m))            /* global: Leiste beim Aktivieren */
RDF(0x232, void,    MenuCheck,      (HMENU m, WORD id, BOOL an))
RDF(0x233, void,    MenuEnable,     (HMENU m, WORD id, BOOL an))
RDF(0x234, void,    MenuFree,       (HMENU m))
RDF(0x235, WORD,    MenuPopup,      (HWND w, HMENU m, WORD sub, short x, short y)) /* Rechtsklickmenue */
RDF(0x236, WORD,    MenuText,       (HMENU m, WORD id, LPCSTR s))
RDF(0x240, WORD,    KeyState,       (WORD taste))                 /* Shift/Strg/Alt, KS_* */

/* ---- Dialoge ------------------------------------------------------------ */
RDF(0x2C0, short,   DialogRun,      (HINST i, WORD id, HWND owner, DLGPROC proc, DWORD param))
RDF(0x2C1, void,    DialogEnd,      (HWND dlg, short ergebnis))
RDF(0x2C2, HWND,    DlgItem,        (HWND dlg, WORD id))
RDF(0x2C3, void,    DlgSetText,     (HWND dlg, WORD id, LPCSTR s))
RDF(0x2C4, WORD,    DlgGetText,     (HWND dlg, WORD id, LPSTR buf, WORD len))
RDF(0x2C5, void,    DlgSetInt,      (HWND dlg, WORD id, long v))
RDF(0x2C6, long,    DlgGetInt,      (HWND dlg, WORD id, BOOL FAR *ok))
RDF(0x2C7, void,    DlgCheck,       (HWND dlg, WORD id, WORD an))
RDF(0x2C8, WORD,    DlgChecked,     (HWND dlg, WORD id))
RDF(0x2C9, short,   DialogIndirect, (HINST i, const void FAR *vorlage, HWND owner, DLGPROC proc, DWORD param)) /* Vorlage wie RT_DIALOG im Speicher */
RDF(0x2D0, short,   MsgBox,         (HWND owner, LPCSTR text, LPCSTR titel, WORD flags)) /* MB_* */
RDF(0x2D1, BOOL,    FileDialog,     (HWND owner, FILEDLG FAR *fd))
RDF(0x2D2, BOOL,    InputBox,       (HWND owner, LPCSTR frage, LPCSTR titel, LPSTR buf, WORD len))
RDF(0x2D3, BOOL,    HelpShow,       (HWND owner, LPCSTR datei, WORD thema)) /* F1-Hilfe */
RDF(0x2DF, void,    CtlInit,        (LPCSTR klasse))                       /* intern: Standardklassen anmelden (RDUSER) */

/* ---- Sound (RDSND) -------------------------------------------------------- */
RDF(0x300, WORD,    SndCaps,        (void))                       /* SC_* */
RDF(0x301, void,    SndBeep,        (WORD hz, WORD ms))
RDF(0x302, void,    SndSystem,      (LPCSTR name))                /* "START", "DING", "FEHLER", ... */
RDF(0x303, WORD,    SndNote,        (WORD kanal, WORD note, WORD lautst))  /* note: MIDI 0..127 */
RDF(0x304, void,    SndNoteOff,     (WORD kanal))
RDF(0x305, WORD,    SndInstr,       (WORD kanal, const INSTR FAR *ins))
RDF(0x306, WORD,    SndMusicPlay,   (HWND melden, LPCSTR datei, WORD flags)) /* IMF/DRO, SM_* */
RDF(0x307, void,    SndMusicStop,   (void))
RDF(0x308, WORD,    SndSamplePlay,  (HWND melden, HMEM daten, WORD rate, WORD flags)) /* 8-Bit PCM */
RDF(0x309, void,    SndSampleStop,  (void))
RDF(0x30A, WORD,    SndVolume,      (WORD art, WORD wert))
RDF(0x30B, BOOL,    SndFocus,       (HWND w))                     /* Musik/Samples fuer dieses Programm */

/* ---- Eingabe ------------------------------------------------------------- */
RDF(0x340, WORD,    JoyRead,        (WORD nr, JOYSTATE FAR *js))   /* RD2.LIB (JOY.C): x/y -100..100, Tasten Bit 0/1 */
RDF(0x341, WORD,    JoyCalibrate,   (WORD nr))                     /* RD2.LIB: jetzige Lage = Mitte */

/* ---- Netz (RDNET) ---------------------------------------------------- */
RDF(0x380, WORD,    NetCaps,        (NETINFO FAR *ni))            /* 0 = Netz bereit */
RDF(0x381, WORD,    NetStart,       (HWND melden))                /* Packet-Treiber, DHCP; M_NETSTATE */
RDF(0x382, WORD,    NetResolve,     (HWND melden, LPCSTR host))   /* -> M_NETRESOLVED p2 = IP */
RDF(0x383, HSOCK,   NetOpen,        (HWND melden, DWORD ip, WORD port, WORD flags)) /* NO_TCP/NO_UDP */
RDF(0x384, HSOCK,   NetListen,      (HWND melden, WORD port, WORD flags))
RDF(0x385, WORD,    NetSend,        (HSOCK s, const void FAR *buf, WORD n)) /* gesendete Bytes */
RDF(0x386, WORD,    NetRecv,        (HSOCK s, void FAR *buf, WORD n))
RDF(0x387, WORD,    NetClose,       (HSOCK s))
RDF(0x388, WORD,    NetState,       (HSOCK s))                    /* NS_* */
RDF(0x389, DWORD,   NetLocalIP,     (void))

/* ---- Drucker ------------------------------------------------------------- */
RDF(0x3C0, HDC,     PrnOpen,        (HWND owner, PRNJOB FAR *job)) /* zeigt Druckdialog wenn PJ_DIALOG */
RDF(0x3C1, WORD,    PrnPage,        (HDC dc))                     /* Band fertig: PRN_NOCHMAL = Seite noch einmal zeichnen, 0 = Seite gedruckt */
RDF(0x3C2, WORD,    PrnClose,       (HDC dc))
RDF(0x3C3, void,    PrnAbort,       (HDC dc))
