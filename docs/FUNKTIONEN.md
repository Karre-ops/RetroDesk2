# RD2 API 2.4: Funktionsliste (Stand 26.11.2026)

Erzeugt mit `python3 tools/apicheck.py --list` aus `sdk/RD2API.H`. Nicht von Hand ändern.

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
| 0x031 | Kern | `void TaskHalt(HTASK t, BOOL an)` |
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
| 0x019 | Kern | `WORD TimerSet(HWND w, WORD id, WORD ms)` |
| 0x01A | Kern | `void TimerKill(HWND w, WORD id)` |
| 0x01B | Kern | `DWORD TickCount(void)` |
| 0x032 | Kern | `WORD TaktSet(TAKTPROC f, WORD teiler)` |
| 0x033 | Kern | `void TaktKill(WORD nr)` |
| 0x034 | Kern | `DWORD TaktCount(void)` |
| 0x01C | Kern | `void TimeGet(RDTIME FAR *t)` |
| 0x01D | Kern | `WORD TimeSet(const RDTIME FAR *t, WORD bios)` |
| 0x01E | Kern | `DWORD InputIdle(void)` |
| 0x01F | Kern | `WORD ProfileGetStr(LPCSTR file, LPCSTR sec, LPCSTR key, LPCSTR def, LPSTR buf, WORD len)` |
| 0x020 | Kern | `short ProfileGetInt(LPCSTR file, LPCSTR sec, LPCSTR key, short def)` |
| 0x021 | Kern | `WORD ProfileSetStr(LPCSTR file, LPCSTR sec, LPCSTR key, LPCSTR val)` |
| 0x022 | Kern | `BOOL ClipOpen(HWND owner)` |
| 0x023 | Kern | `BOOL ClipSet(WORD format, HMEM data)` |
| 0x024 | Kern | `HMEM ClipGet(WORD format)` |
| 0x025 | Kern | `void ClipClose(void)` |
| 0x026 | Kern | `void ClipEmpty(void)` |
| 0x027 | Kern | `HMEM ResLoad(HINST i, WORD typ, WORD id)` |
| 0x028 | Kern | `void ResFree(HMEM r)` |
| 0x029 | Kern | `WORD StrLoad(HINST i, WORD id, LPSTR buf, WORD len)` |
| 0x02A | Kern | `WORD LangGet(void)` |
| 0x02B | Kern | `LPCSTR StrGet(HINST i, WORD id)` |
| 0x02C | Kern | `void DbgPrint(LPCSTR text)` |
| 0x02D | Kern | `void DbgHex(LPCSTR text, DWORD wert)` |
| 0x02E | Kern | `void DbgBreak(void)` |
| 0x02F | Kern | `WORD SysInfo(RDSYSDATA FAR *sd)` |
| 0x030 | Kern | `WORD BedarfCheck(LPCSTR datei)` |
| 0x037 | Speicher | `HMEM MemAlloc(DWORD size, WORD flags)` |
| 0x038 | Speicher | `HMEM MemFree(HMEM h)` |
| 0x039 | Speicher | `LPLOW MemLock(HMEM h)` |
| 0x03A | Speicher | `BOOL MemUnlock(HMEM h)` |
| 0x03B | Speicher | `HMEM MemRealloc(HMEM h, DWORD size, WORD flags)` |
| 0x03C | Speicher | `DWORD MemSize(HMEM h)` |
| 0x03D | Speicher | `WORD MemFlags(HMEM h)` |
| 0x03E | Speicher | `DWORD MemAvail(WORD art)` |
| 0x03F | Speicher | `DWORD MemCompact(DWORD wunsch)` |
| 0x040 | Speicher | `LPLOW MemFixed(WORD size)` |
| 0x041 | Speicher | `void MemFixedFree(LPLOW p)` |
| 0x042 | Speicher | `WORD MemError(void)` |
| 0x043 | Speicher | `WORD MemStats(MEMSTATS FAR *info)` |
| 0x04A | Datei | `short FileOpen(LPCSTR name, WORD mode)` |
| 0x04B | Datei | `short FileCreate(LPCSTR name, WORD attr)` |
| 0x04C | Datei | `WORD FileRead(short f, void FAR *buf, WORD n)` |
| 0x04D | Datei | `WORD FileWrite(short f, const void FAR *buf, WORD n)` |
| 0x04E | Datei | `long FileSeek(short f, long pos, WORD art)` |
| 0x04F | Datei | `WORD FileClose(short f)` |
| 0x050 | Datei | `WORD FileDelete(LPCSTR name)` |
| 0x051 | Datei | `WORD FileRename(LPCSTR alt, LPCSTR neu)` |
| 0x052 | Datei | `WORD FileFindFirst(LPCSTR muster, WORD attr, FINDDATA FAR *fd)` |
| 0x053 | Datei | `WORD FileFindNext(FINDDATA FAR *fd)` |
| 0x054 | Datei | `WORD FileAttr(LPCSTR name, WORD neu, WORD setzen)` |
| 0x055 | Datei | `WORD DirMake(LPCSTR name)` |
| 0x056 | Datei | `WORD DirRemove(LPCSTR name)` |
| 0x057 | Datei | `WORD DirChange(LPCSTR name)` |
| 0x058 | Datei | `WORD DirGet(WORD drive, LPSTR buf)` |
| 0x059 | Datei | `WORD DriveInfo(WORD drive, DRIVEINFO FAR *di)` |
| 0x05A | Datei | `WORD DiskRequest(WORD drive, DWORD serial, LPCSTR label)` |
| 0x05B | Datei | `WORD FileSaveSafe(LPCSTR name, const void FAR *buf, DWORD n)` |
| 0x064 | Grafik | `short GetCaps(HDC dc, WORD index)` |
| 0x065 | Grafik | `COLOR SetPen(HDC dc, COLOR c)` |
| 0x066 | Grafik | `COLOR SetBrush(HDC dc, COLOR c, WORD muster)` |
| 0x067 | Grafik | `WORD SetRop(HDC dc, WORD rop)` |
| 0x068 | Grafik | `HFONT SetFont(HDC dc, HFONT f)` |
| 0x069 | Grafik | `COLOR SetTextColor(HDC dc, COLOR c)` |
| 0x06A | Grafik | `COLOR SetBkColor(HDC dc, COLOR c)` |
| 0x06B | Grafik | `WORD SetBkMode(HDC dc, WORD mode)` |
| 0x06C | Grafik | `void SetOrigin(HDC dc, short x, short y)` |
| 0x06D | Grafik | `void SetClip(HDC dc, const RECT FAR *r)` |
| 0x074 | Grafik | `void Pixel(HDC dc, short x, short y, COLOR c)` |
| 0x075 | Grafik | `COLOR GetPixel(HDC dc, short x, short y)` |
| 0x076 | Grafik | `void MoveTo(HDC dc, short x, short y)` |
| 0x077 | Grafik | `void LineTo(HDC dc, short x, short y)` |
| 0x078 | Grafik | `void Line(HDC dc, short x0, short y0, short x1, short y1)` |
| 0x079 | Grafik | `void Frame(HDC dc, const RECT FAR *r)` |
| 0x07A | Grafik | `void FillRect(HDC dc, const RECT FAR *r)` |
| 0x07B | Grafik | `void InvertRect(HDC dc, const RECT FAR *r)` |
| 0x07C | Grafik | `void Ellipse(HDC dc, const RECT FAR *r, WORD fill)` |
| 0x07D | Grafik | `void Polygon(HDC dc, const POINT FAR *p, WORD n, WORD fill)` |
| 0x07E | Grafik | `void FloodFill(HDC dc, short x, short y)` |
| 0x07F | Grafik | `void RoundRect(HDC dc, const RECT FAR *r, WORD radius, WORD fill)` |
| 0x084 | Grafik | `void TextOut(HDC dc, short x, short y, LPCSTR s, short len)` |
| 0x085 | Grafik | `short TextWidth(HDC dc, LPCSTR s, short len)` |
| 0x086 | Grafik | `void TextBox(HDC dc, const RECT FAR *r, LPCSTR s, WORD flags)` |
| 0x08C | Grafik | `HFONT FontLoad(LPCSTR name, WORD hoehe, WORD stil)` |
| 0x08D | Grafik | `void FontFree(HFONT f)` |
| 0x08E | Grafik | `WORD FontInfo(HFONT f, FONTINFO FAR *fi)` |
| 0x094 | Grafik | `HBMP BmpCreate(WORD w, WORD h, WORD bpp)` |
| 0x095 | Grafik | `void BmpFree(HBMP b)` |
| 0x096 | Grafik | `HDC BmpDC(HBMP b)` |
| 0x097 | Grafik | `void DCFree(HDC dc)` |
| 0x098 | Grafik | `void BitBlt(HDC d, short x, short y, short w, short h, HDC s, short sx, short sy, WORD rop)` |
| 0x099 | Grafik | `void StretchBlt(HDC d, const RECT FAR *dr, HDC s, const RECT FAR *sr, WORD rop)` |
| 0x09A | Grafik | `HBMP BmpLoad(HINST i, WORD id)` |
| 0x09B | Grafik | `HBMP BmpLoadFile(LPCSTR name)` |
| 0x09C | Grafik | `void IconDraw(HDC dc, short x, short y, HINST i, WORD id)` |
| 0x09D | Grafik | `COLOR ColorNearest(HDC dc, COLOR c)` |
| 0x09E | Grafik | `WORD PaletteSet(HDC dc, WORD first, WORD n, const COLOR FAR *c)` |
| 0x09F | Grafik | `WORD GfxList(GFXINFO FAR *l, WORD max)` |
| 0x0A0 | Grafik | `WORD GfxSelect(WORD typ)` |
| 0x0A1 | Grafik | `WORD IconDrawFile(HDC dc, short x, short y, LPCSTR datei, WORD id)` |
| 0x0A9 | Oberflaeche | `WORD RegisterClass(const WNDCLASS FAR *wc)` |
| 0x0AA | Oberflaeche | `HWND CreateWindow(LPCSTR klasse, LPCSTR titel, DWORD stil, short x, short y, short w, short h, HWND eltern, WORD id, HINST i)` |
| 0x0AB | Oberflaeche | `BOOL DestroyWindow(HWND w)` |
| 0x0AC | Oberflaeche | `void ShowWindow(HWND w, WORD art)` |
| 0x0AD | Oberflaeche | `void MoveWindow(HWND w, short x, short y, short cx, short cy, BOOL neu)` |
| 0x0AE | Oberflaeche | `void SetWindowText(HWND w, LPCSTR s)` |
| 0x0AF | Oberflaeche | `WORD GetWindowText(HWND w, LPSTR buf, WORD len)` |
| 0x0B0 | Oberflaeche | `void GetClientRect(HWND w, RECT FAR *r)` |
| 0x0B1 | Oberflaeche | `void GetWindowRect(HWND w, RECT FAR *r)` |
| 0x0B2 | Oberflaeche | `void InvalidateRect(HWND w, const RECT FAR *r, BOOL loeschen)` |
| 0x0B3 | Oberflaeche | `void ValidateRect(HWND w, const RECT FAR *r)` |
| 0x0B4 | Oberflaeche | `void UpdateWindow(HWND w)` |
| 0x0B5 | Oberflaeche | `HDC BeginPaint(HWND w, PAINT FAR *ps)` |
| 0x0B6 | Oberflaeche | `void EndPaint(HWND w, PAINT FAR *ps)` |
| 0x0B7 | Oberflaeche | `HDC GetDC(HWND w)` |
| 0x0B8 | Oberflaeche | `void ReleaseDC(HWND w, HDC dc)` |
| 0x0B9 | Oberflaeche | `HWND SetFocus(HWND w)` |
| 0x0BA | Oberflaeche | `HWND GetFocus(void)` |
| 0x0BB | Oberflaeche | `HWND SetActive(HWND w)` |
| 0x0BC | Oberflaeche | `HWND GetActive(void)` |
| 0x0BD | Oberflaeche | `DWORD DefWindowProc(HWND w, WORD msg, WORD p1, DWORD p2)` |
| 0x0BE | Oberflaeche | `DWORD SetWindowData(HWND w, short index, DWORD wert)` |
| 0x0BF | Oberflaeche | `DWORD GetWindowData(HWND w, short index)` |
| 0x0C0 | Oberflaeche | `HWND GetParent(HWND w)` |
| 0x0C1 | Oberflaeche | `HWND WindowFromPoint(short x, short y)` |
| 0x0C2 | Oberflaeche | `void ClientToScreen(HWND w, POINT FAR *p)` |
| 0x0C3 | Oberflaeche | `void ScreenToClient(HWND w, POINT FAR *p)` |
| 0x0C4 | Oberflaeche | `BOOL IsWindow(HWND w)` |
| 0x0C5 | Oberflaeche | `void EnableWindow(HWND w, BOOL an)` |
| 0x0C6 | Oberflaeche | `void ScrollWindow(HWND w, short dx, short dy, const RECT FAR *r)` |
| 0x0C7 | Oberflaeche | `HWND GetChild(HWND eltern, HWND nach)` |
| 0x0C9 | Oberflaeche | `void ScrollSet(HWND w, WORD balken, short pos, short max, short seite)` |
| 0x0CA | Oberflaeche | `short ScrollGet(HWND w, WORD balken)` |
| 0x0D1 | Oberflaeche | `HWND CaptureMouse(HWND w)` |
| 0x0D2 | Oberflaeche | `void ReleaseMouse(void)` |
| 0x0D3 | Oberflaeche | `void CursorSet(HINST i, WORD id)` |
| 0x0D4 | Oberflaeche | `void CursorBusy(BOOL an)` |
| 0x0D5 | Oberflaeche | `void CursorPos(POINT FAR *p)` |
| 0x0D9 | Oberflaeche | `HMENU MenuLoad(HINST i, WORD id)` |
| 0x0DA | Oberflaeche | `void MenuSet(HWND w, HMENU m)` |
| 0x0DB | Oberflaeche | `void MenuCheck(HMENU m, WORD id, BOOL an)` |
| 0x0DC | Oberflaeche | `void MenuEnable(HMENU m, WORD id, BOOL an)` |
| 0x0DD | Oberflaeche | `void MenuFree(HMENU m)` |
| 0x0DE | Oberflaeche | `WORD MenuPopup(HWND w, HMENU m, WORD sub, short x, short y)` |
| 0x0DF | Oberflaeche | `WORD MenuText(HMENU m, WORD id, LPCSTR s)` |
| 0x0E0 | Oberflaeche | `WORD ScreenAcquire(HWND w)` |
| 0x0E2 | Oberflaeche | `void ScreenMouse(HWND w, WORD breite, WORD hoehe)` |
| 0x0E1 | Oberflaeche | `void ScreenRelease(HWND w)` |
| 0x0E9 | Oberflaeche | `WORD KeyState(WORD taste)` |
| 0x0F3 | Dialoge | `short DialogRun(HINST i, WORD id, HWND owner, DLGPROC proc, DWORD param)` |
| 0x0F4 | Dialoge | `void DialogEnd(HWND dlg, short ergebnis)` |
| 0x0F5 | Dialoge | `HWND DlgItem(HWND dlg, WORD id)` |
| 0x0F6 | Dialoge | `void DlgSetText(HWND dlg, WORD id, LPCSTR s)` |
| 0x0F7 | Dialoge | `WORD DlgGetText(HWND dlg, WORD id, LPSTR buf, WORD len)` |
| 0x0F8 | Dialoge | `void DlgSetInt(HWND dlg, WORD id, long v)` |
| 0x0F9 | Dialoge | `long DlgGetInt(HWND dlg, WORD id, BOOL FAR *ok)` |
| 0x0FA | Dialoge | `void DlgCheck(HWND dlg, WORD id, WORD an)` |
| 0x0FB | Dialoge | `WORD DlgChecked(HWND dlg, WORD id)` |
| 0x0FC | Dialoge | `short DialogIndirect(HINST i, const void FAR *vorlage, HWND owner, DLGPROC proc, DWORD param)` |
| 0x103 | Dialoge | `short MsgBox(HWND owner, LPCSTR text, LPCSTR titel, WORD flags)` |
| 0x104 | Dialoge | `BOOL FileDialog(HWND owner, FILEDLG FAR *fd)` |
| 0x105 | Dialoge | `BOOL InputBox(HWND owner, LPCSTR frage, LPCSTR titel, LPSTR buf, WORD len)` |
| 0x106 | Dialoge | `BOOL HelpShow(HWND owner, LPCSTR datei, WORD thema)` |
| 0x112 | Dialoge | `void CtlInit(LPCSTR klasse)` |
| 0x11B | Sound | `WORD SndCaps(void)` |
| 0x11C | Sound | `void SndBeep(WORD hz, WORD ms)` |
| 0x11D | Sound | `void SndSystem(LPCSTR name)` |
| 0x11E | Sound | `WORD SndNote(WORD kanal, WORD note, WORD lautst)` |
| 0x11F | Sound | `void SndNoteOff(WORD kanal)` |
| 0x120 | Sound | `WORD SndInstr(WORD kanal, const INSTR FAR *ins)` |
| 0x121 | Sound | `WORD SndMusicPlay(HWND melden, LPCSTR datei, WORD flags)` |
| 0x122 | Sound | `void SndMusicStop(void)` |
| 0x123 | Sound | `WORD SndSamplePlay(HWND melden, HMEM daten, WORD rate, WORD flags)` |
| 0x124 | Sound | `void SndSampleStop(void)` |
| 0x125 | Sound | `WORD SndVolume(WORD art, WORD wert)` |
| 0x126 | Sound | `BOOL SndFocus(HWND w)` |
| 0x127 | Sound | `WORD SndMixPlay(HWND melden, HMEM daten, WORD rate, WORD flags)` |
| 0x128 | Sound | `WORD SndMixSet(WORD kanal, WORD laut, WORD lage)` |
| 0x129 | Sound | `void SndMixStop(WORD kanal)` |
| 0x12A | Sound | `WORD SndMixOut(WORD rate, WORD fmt)` |
| 0x12B | Sound | `WORD SndMixState(WORD kanal)` |
| 0x12C | Sound | `WORD SndMusicMem(HWND melden, HMEM daten, WORD flags)` |
| 0x12D | Sound | `WORD SndTonePlay(HMEM daten, WORD flags)` |
| 0x12F | Eingabe | `WORD JoyRead(WORD nr, JOYSTATE FAR *js)` |
| 0x130 | Eingabe | `WORD JoyCalibrate(WORD nr)` |
| 0x139 | Netz | `WORD NetCaps(NETINFO FAR *ni)` |
| 0x13A | Netz | `WORD NetStart(HWND melden)` |
| 0x13B | Netz | `WORD NetResolve(HWND melden, LPCSTR host)` |
| 0x13C | Netz | `HSOCK NetOpen(HWND melden, DWORD ip, WORD port, WORD flags)` |
| 0x13D | Netz | `HSOCK NetListen(HWND melden, WORD port, WORD flags)` |
| 0x13E | Netz | `WORD NetSend(HSOCK s, const void FAR *buf, WORD n)` |
| 0x13F | Netz | `WORD NetRecv(HSOCK s, void FAR *buf, WORD n)` |
| 0x140 | Netz | `WORD NetClose(HSOCK s)` |
| 0x141 | Netz | `WORD NetState(HSOCK s)` |
| 0x142 | Netz | `DWORD NetLocalIP(void)` |
| 0x14C | Drucker | `HDC PrnOpen(HWND owner, PRNJOB FAR *job)` |
| 0x14D | Drucker | `WORD PrnPage(HDC dc)` |
| 0x14E | Drucker | `WORD PrnClose(HDC dc)` |
| 0x14F | Drucker | `void PrnAbort(HDC dc)` |
225 Funktionen, 0 Fehler
