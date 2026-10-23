# RD2 API 1.0: Funktionsliste

Erzeugt mit `python3 tools/apicheck.py --list` aus `sdk/RD2API.H`. Nicht von Hand ändern.

| Nr. | Gruppe | Funktion |
|---|---|---|
| 0x000 | Kern | `WORD KernVersion(void)` |
| 0x001 | Kern | `WORD KernInfo(SYSINFO FAR *si)` |
| 0x002 | Kern | `HTASK TaskSelf(void)` |
| 0x003 | Kern | `void TaskYield(void)` |
| 0x004 | Kern | `HTASK TaskExec(LPCSTR app, LPCSTR cmd, WORD show)` |
| 0x005 | Kern | `WORD TaskKill(HTASK t, WORD code)` |
| 0x006 | Kern | `HTASK TaskFirst(TASKINFO FAR *ti)` |
| 0x007 | Kern | `HTASK TaskNext(HTASK t, TASKINFO FAR *ti)` |
| 0x008 | Kern | `HMOD ModLoad(LPCSTR name)` |
| 0x009 | Kern | `void ModFree(HMOD m)` |
| 0x00A | Kern | `FARPROC ModProc(HMOD m, WORD nummer)` |
| 0x00B | Kern | `WORD RunDos(LPCSTR prog, LPCSTR cmd, WORD flags)` |
| 0x00C | Kern | `WORD LastError(void)` |
| 0x00D | Kern | `void SetLastError(WORD e)` |
| 0x00E | Kern | `void ExitRD(WORD flags)` |
| 0x00F | Kern | `DWORD InstRes(HINST i, LPSTR pfad, WORD len)` |
| 0x010 | Kern | `BOOL GetMessage(MSG FAR *m)` |
| 0x011 | Kern | `BOOL PeekMessage(MSG FAR *m, WORD remove)` |
| 0x012 | Kern | `BOOL PostMessage(HWND w, WORD msg, WORD p1, DWORD p2)` |
| 0x013 | Kern | `DWORD SendMessage(HWND w, WORD msg, WORD p1, DWORD p2)` |
| 0x014 | Kern | `DWORD DispatchMessage(const MSG FAR *m)` |
| 0x015 | Kern | `void PostQuit(WORD code)` |
| 0x016 | Kern | `void WaitMessage(void)` |
| 0x017 | Kern | `BOOL PostTask(HTASK t, WORD msg, WORD p1, DWORD p2)` |
| 0x018 | Kern | `WORD MsgRegister(LPCSTR name)` |
| 0x020 | Kern | `WORD TimerSet(HWND w, WORD id, WORD ms)` |
| 0x021 | Kern | `void TimerKill(HWND w, WORD id)` |
| 0x022 | Kern | `DWORD TickCount(void)` |
| 0x023 | Kern | `void TimeGet(RDTIME FAR *t)` |
| 0x024 | Kern | `WORD TimeSet(const RDTIME FAR *t, WORD bios)` |
| 0x030 | Kern | `WORD ProfileGetStr(LPCSTR file, LPCSTR sec, LPCSTR key, LPCSTR def, LPSTR buf, WORD len)` |
| 0x031 | Kern | `short ProfileGetInt(LPCSTR file, LPCSTR sec, LPCSTR key, short def)` |
| 0x032 | Kern | `WORD ProfileSetStr(LPCSTR file, LPCSTR sec, LPCSTR key, LPCSTR val)` |
| 0x038 | Kern | `BOOL ClipOpen(HWND owner)` |
| 0x039 | Kern | `BOOL ClipSet(WORD format, HMEM data)` |
| 0x03A | Kern | `HMEM ClipGet(WORD format)` |
| 0x03B | Kern | `void ClipClose(void)` |
| 0x03C | Kern | `void ClipEmpty(void)` |
| 0x040 | Kern | `HMEM ResLoad(HINST i, WORD typ, WORD id)` |
| 0x041 | Kern | `void ResFree(HMEM r)` |
| 0x042 | Kern | `WORD StrLoad(HINST i, WORD id, LPSTR buf, WORD len)` |
| 0x043 | Kern | `WORD LangGet(void)` |
| 0x044 | Kern | `LPCSTR StrGet(HINST i, WORD id)` |
| 0x070 | Kern | `void DbgPrint(LPCSTR text)` |
| 0x071 | Kern | `void DbgHex(LPCSTR text, DWORD wert)` |
| 0x072 | Kern | `void DbgBreak(void)` |
| 0x080 | Speicher | `HMEM MemAlloc(DWORD size, WORD flags)` |
| 0x081 | Speicher | `HMEM MemFree(HMEM h)` |
| 0x082 | Speicher | `void FAR * MemLock(HMEM h)` |
| 0x083 | Speicher | `BOOL MemUnlock(HMEM h)` |
| 0x084 | Speicher | `HMEM MemRealloc(HMEM h, DWORD size, WORD flags)` |
| 0x085 | Speicher | `DWORD MemSize(HMEM h)` |
| 0x086 | Speicher | `WORD MemFlags(HMEM h)` |
| 0x087 | Speicher | `DWORD MemAvail(WORD art)` |
| 0x088 | Speicher | `DWORD MemCompact(DWORD wunsch)` |
| 0x089 | Speicher | `void FAR * MemFixed(WORD size)` |
| 0x08A | Speicher | `void MemFixedFree(void FAR *p)` |
| 0x0C0 | Datei | `short FileOpen(LPCSTR name, WORD mode)` |
| 0x0C1 | Datei | `short FileCreate(LPCSTR name, WORD attr)` |
| 0x0C2 | Datei | `WORD FileRead(short f, void FAR *buf, WORD n)` |
| 0x0C3 | Datei | `WORD FileWrite(short f, const void FAR *buf, WORD n)` |
| 0x0C4 | Datei | `long FileSeek(short f, long pos, WORD art)` |
| 0x0C5 | Datei | `WORD FileClose(short f)` |
| 0x0C6 | Datei | `WORD FileDelete(LPCSTR name)` |
| 0x0C7 | Datei | `WORD FileRename(LPCSTR alt, LPCSTR neu)` |
| 0x0C8 | Datei | `WORD FileFindFirst(LPCSTR muster, WORD attr, FINDDATA FAR *fd)` |
| 0x0C9 | Datei | `WORD FileFindNext(FINDDATA FAR *fd)` |
| 0x0CA | Datei | `WORD FileAttr(LPCSTR name, WORD neu, WORD setzen)` |
| 0x0CB | Datei | `WORD DirMake(LPCSTR name)` |
| 0x0CC | Datei | `WORD DirRemove(LPCSTR name)` |
| 0x0CD | Datei | `WORD DirChange(LPCSTR name)` |
| 0x0CE | Datei | `WORD DirGet(WORD drive, LPSTR buf)` |
| 0x0CF | Datei | `WORD DriveInfo(WORD drive, DRIVEINFO FAR *di)` |
| 0x0D0 | Datei | `WORD DiskRequest(WORD drive, DWORD serial, LPCSTR label)` |
| 0x0D1 | Datei | `WORD FileSaveSafe(LPCSTR name, const void FAR *buf, DWORD n)` |
| 0x100 | Grafik | `short GetCaps(HDC dc, WORD index)` |
| 0x101 | Grafik | `COLOR SetPen(HDC dc, COLOR c)` |
| 0x102 | Grafik | `COLOR SetBrush(HDC dc, COLOR c, WORD muster)` |
| 0x103 | Grafik | `WORD SetRop(HDC dc, WORD rop)` |
| 0x104 | Grafik | `HFONT SetFont(HDC dc, HFONT f)` |
| 0x105 | Grafik | `COLOR SetTextColor(HDC dc, COLOR c)` |
| 0x106 | Grafik | `COLOR SetBkColor(HDC dc, COLOR c)` |
| 0x107 | Grafik | `WORD SetBkMode(HDC dc, WORD mode)` |
| 0x108 | Grafik | `void SetOrigin(HDC dc, short x, short y)` |
| 0x109 | Grafik | `void SetClip(HDC dc, const RECT FAR *r)` |
| 0x110 | Grafik | `void Pixel(HDC dc, short x, short y, COLOR c)` |
| 0x111 | Grafik | `COLOR GetPixel(HDC dc, short x, short y)` |
| 0x112 | Grafik | `void MoveTo(HDC dc, short x, short y)` |
| 0x113 | Grafik | `void LineTo(HDC dc, short x, short y)` |
| 0x114 | Grafik | `void Line(HDC dc, short x0, short y0, short x1, short y1)` |
| 0x115 | Grafik | `void Frame(HDC dc, const RECT FAR *r)` |
| 0x116 | Grafik | `void FillRect(HDC dc, const RECT FAR *r)` |
| 0x117 | Grafik | `void InvertRect(HDC dc, const RECT FAR *r)` |
| 0x118 | Grafik | `void Ellipse(HDC dc, const RECT FAR *r, WORD fill)` |
| 0x119 | Grafik | `void Polygon(HDC dc, const POINT FAR *p, WORD n, WORD fill)` |
| 0x11A | Grafik | `void FloodFill(HDC dc, short x, short y)` |
| 0x11B | Grafik | `void RoundRect(HDC dc, const RECT FAR *r, WORD radius, WORD fill)` |
| 0x120 | Grafik | `void TextOut(HDC dc, short x, short y, LPCSTR s, short len)` |
| 0x121 | Grafik | `short TextWidth(HDC dc, LPCSTR s, short len)` |
| 0x122 | Grafik | `void TextBox(HDC dc, const RECT FAR *r, LPCSTR s, WORD flags)` |
| 0x128 | Grafik | `HFONT FontLoad(LPCSTR name, WORD hoehe, WORD stil)` |
| 0x129 | Grafik | `void FontFree(HFONT f)` |
| 0x12A | Grafik | `WORD FontInfo(HFONT f, FONTINFO FAR *fi)` |
| 0x130 | Grafik | `HBMP BmpCreate(WORD w, WORD h, WORD bpp)` |
| 0x131 | Grafik | `void BmpFree(HBMP b)` |
| 0x132 | Grafik | `HDC BmpDC(HBMP b)` |
| 0x133 | Grafik | `void DCFree(HDC dc)` |
| 0x134 | Grafik | `void BitBlt(HDC d, short x, short y, short w, short h, HDC s, short sx, short sy, WORD rop)` |
| 0x135 | Grafik | `void StretchBlt(HDC d, const RECT FAR *dr, HDC s, const RECT FAR *sr, WORD rop)` |
| 0x136 | Grafik | `HBMP BmpLoad(HINST i, WORD id)` |
| 0x137 | Grafik | `HBMP BmpLoadFile(LPCSTR name)` |
| 0x138 | Grafik | `void IconDraw(HDC dc, short x, short y, HINST i, WORD id)` |
| 0x139 | Grafik | `COLOR ColorNearest(HDC dc, COLOR c)` |
| 0x13A | Grafik | `WORD PaletteSet(HDC dc, WORD first, WORD n, const COLOR FAR *c)` |
| 0x13B | Grafik | `WORD GfxList(GFXINFO FAR *l, WORD max)` |
| 0x13C | Grafik | `WORD GfxSelect(WORD typ)` |
| 0x200 | Oberflaeche | `WORD RegisterClass(const WNDCLASS FAR *wc)` |
| 0x201 | Oberflaeche | `HWND CreateWindow(LPCSTR klasse, LPCSTR titel, DWORD stil, short x, short y, short w, short h, HWND eltern, WORD id, HINST i)` |
| 0x202 | Oberflaeche | `BOOL DestroyWindow(HWND w)` |
| 0x203 | Oberflaeche | `void ShowWindow(HWND w, WORD art)` |
| 0x204 | Oberflaeche | `void MoveWindow(HWND w, short x, short y, short cx, short cy, BOOL neu)` |
| 0x205 | Oberflaeche | `void SetWindowText(HWND w, LPCSTR s)` |
| 0x206 | Oberflaeche | `WORD GetWindowText(HWND w, LPSTR buf, WORD len)` |
| 0x207 | Oberflaeche | `void GetClientRect(HWND w, RECT FAR *r)` |
| 0x208 | Oberflaeche | `void GetWindowRect(HWND w, RECT FAR *r)` |
| 0x209 | Oberflaeche | `void InvalidateRect(HWND w, const RECT FAR *r, BOOL loeschen)` |
| 0x20A | Oberflaeche | `void ValidateRect(HWND w, const RECT FAR *r)` |
| 0x20B | Oberflaeche | `void UpdateWindow(HWND w)` |
| 0x20C | Oberflaeche | `HDC BeginPaint(HWND w, PAINT FAR *ps)` |
| 0x20D | Oberflaeche | `void EndPaint(HWND w, PAINT FAR *ps)` |
| 0x20E | Oberflaeche | `HDC GetDC(HWND w)` |
| 0x20F | Oberflaeche | `void ReleaseDC(HWND w, HDC dc)` |
| 0x210 | Oberflaeche | `HWND SetFocus(HWND w)` |
| 0x211 | Oberflaeche | `HWND GetFocus(void)` |
| 0x212 | Oberflaeche | `HWND SetActive(HWND w)` |
| 0x213 | Oberflaeche | `HWND GetActive(void)` |
| 0x214 | Oberflaeche | `DWORD DefWindowProc(HWND w, WORD msg, WORD p1, DWORD p2)` |
| 0x215 | Oberflaeche | `DWORD SetWindowData(HWND w, short index, DWORD wert)` |
| 0x216 | Oberflaeche | `DWORD GetWindowData(HWND w, short index)` |
| 0x217 | Oberflaeche | `HWND GetParent(HWND w)` |
| 0x218 | Oberflaeche | `HWND WindowFromPoint(short x, short y)` |
| 0x219 | Oberflaeche | `void ClientToScreen(HWND w, POINT FAR *p)` |
| 0x21A | Oberflaeche | `void ScreenToClient(HWND w, POINT FAR *p)` |
| 0x21B | Oberflaeche | `BOOL IsWindow(HWND w)` |
| 0x21C | Oberflaeche | `void EnableWindow(HWND w, BOOL an)` |
| 0x21D | Oberflaeche | `void ScrollWindow(HWND w, short dx, short dy, const RECT FAR *r)` |
| 0x21E | Oberflaeche | `HWND GetChild(HWND eltern, HWND nach)` |
| 0x220 | Oberflaeche | `void ScrollSet(HWND w, WORD balken, short pos, short max, short seite)` |
| 0x221 | Oberflaeche | `short ScrollGet(HWND w, WORD balken)` |
| 0x228 | Oberflaeche | `HWND CaptureMouse(HWND w)` |
| 0x229 | Oberflaeche | `void ReleaseMouse(void)` |
| 0x22A | Oberflaeche | `void CursorSet(HINST i, WORD id)` |
| 0x22B | Oberflaeche | `void CursorBusy(BOOL an)` |
| 0x22C | Oberflaeche | `void CursorPos(POINT FAR *p)` |
| 0x230 | Oberflaeche | `HMENU MenuLoad(HINST i, WORD id)` |
| 0x231 | Oberflaeche | `void MenuSet(HWND w, HMENU m)` |
| 0x232 | Oberflaeche | `void MenuCheck(HMENU m, WORD id, BOOL an)` |
| 0x233 | Oberflaeche | `void MenuEnable(HMENU m, WORD id, BOOL an)` |
| 0x234 | Oberflaeche | `void MenuFree(HMENU m)` |
| 0x235 | Oberflaeche | `WORD MenuPopup(HWND w, HMENU m, WORD sub, short x, short y)` |
| 0x236 | Oberflaeche | `WORD MenuText(HMENU m, WORD id, LPCSTR s)` |
| 0x240 | Oberflaeche | `WORD KeyState(WORD taste)` |
| 0x2C0 | Dialoge | `short DialogRun(HINST i, WORD id, HWND owner, DLGPROC proc, DWORD param)` |
| 0x2C1 | Dialoge | `void DialogEnd(HWND dlg, short ergebnis)` |
| 0x2C2 | Dialoge | `HWND DlgItem(HWND dlg, WORD id)` |
| 0x2C3 | Dialoge | `void DlgSetText(HWND dlg, WORD id, LPCSTR s)` |
| 0x2C4 | Dialoge | `WORD DlgGetText(HWND dlg, WORD id, LPSTR buf, WORD len)` |
| 0x2C5 | Dialoge | `void DlgSetInt(HWND dlg, WORD id, long v)` |
| 0x2C6 | Dialoge | `long DlgGetInt(HWND dlg, WORD id, BOOL FAR *ok)` |
| 0x2C7 | Dialoge | `void DlgCheck(HWND dlg, WORD id, WORD an)` |
| 0x2C8 | Dialoge | `WORD DlgChecked(HWND dlg, WORD id)` |
| 0x2D0 | Dialoge | `short MsgBox(HWND owner, LPCSTR text, LPCSTR titel, WORD flags)` |
| 0x2D1 | Dialoge | `BOOL FileDialog(HWND owner, FILEDLG FAR *fd)` |
| 0x2D2 | Dialoge | `BOOL InputBox(HWND owner, LPCSTR frage, LPCSTR titel, LPSTR buf, WORD len)` |
| 0x2D3 | Dialoge | `BOOL HelpShow(HWND owner, LPCSTR datei, WORD thema)` |
| 0x2DF | Dialoge | `void CtlInit(void)` |
| 0x300 | Sound | `WORD SndCaps(void)` |
| 0x301 | Sound | `void SndBeep(WORD hz, WORD ms)` |
| 0x302 | Sound | `void SndSystem(LPCSTR name)` |
| 0x303 | Sound | `WORD SndNote(WORD kanal, WORD note, WORD lautst)` |
| 0x304 | Sound | `void SndNoteOff(WORD kanal)` |
| 0x305 | Sound | `WORD SndInstr(WORD kanal, const INSTR FAR *ins)` |
| 0x306 | Sound | `WORD SndMusicPlay(HWND melden, LPCSTR datei, WORD flags)` |
| 0x307 | Sound | `void SndMusicStop(void)` |
| 0x308 | Sound | `WORD SndSamplePlay(HWND melden, HMEM daten, WORD rate, WORD flags)` |
| 0x309 | Sound | `void SndSampleStop(void)` |
| 0x30A | Sound | `WORD SndVolume(WORD art, WORD wert)` |
| 0x30B | Sound | `BOOL SndFocus(HWND w)` |
| 0x340 | Eingabe | `WORD JoyRead(WORD nr, JOYSTATE FAR *js)` |
| 0x341 | Eingabe | `WORD JoyCalibrate(WORD nr)` |
| 0x380 | Netz | `WORD NetCaps(NETINFO FAR *ni)` |
| 0x381 | Netz | `WORD NetStart(HWND melden)` |
| 0x382 | Netz | `WORD NetResolve(HWND melden, LPCSTR host)` |
| 0x383 | Netz | `HSOCK NetOpen(HWND melden, DWORD ip, WORD port, WORD flags)` |
| 0x384 | Netz | `HSOCK NetListen(HWND melden, WORD port, WORD flags)` |
| 0x385 | Netz | `WORD NetSend(HSOCK s, const void FAR *buf, WORD n)` |
| 0x386 | Netz | `WORD NetRecv(HSOCK s, void FAR *buf, WORD n)` |
| 0x387 | Netz | `WORD NetClose(HSOCK s)` |
| 0x388 | Netz | `WORD NetState(HSOCK s)` |
| 0x389 | Netz | `DWORD NetLocalIP(void)` |
| 0x3C0 | Drucker | `HDC PrnOpen(HWND owner, PRNJOB FAR *job)` |
| 0x3C1 | Drucker | `WORD PrnPage(HDC dc)` |
| 0x3C2 | Drucker | `WORD PrnClose(HDC dc)` |
| 0x3C3 | Drucker | `void PrnAbort(HDC dc)` |

204 Funktionen, 0 Fehler
