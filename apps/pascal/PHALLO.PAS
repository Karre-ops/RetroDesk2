{ PHALLO.PAS - Beispiel fuer RetroDesk 2 in Pascal (Turbo Pascal 5.5-7.0, Free Pascal i8086):
  Fenster mit Text, zaehlt Mausklicks, Esc schliesst. Exitcode = Zahl der Klicks.
  Bauen:  TPC PHALLO.PAS              (oder: ppcross8086 -Tmsdos -WmLarge PHALLO.PAS)
          RDRC - PHALLO.EXE "Hallo Pascal" /PAS
  Befehlszeile "T": meldet ueber DbgPrint (fuer test/pascal.sh). }
program PHallo;
{$M 8192,0,0}
{$F+}
uses RD2;

var
  fe: HWND;
  klicks: Word;
  test: Boolean;

{ Funktionsergebnisse, die nicht gebraucht werden, gehen nach Egal (TP 5.5 kann sie nicht verwerfen).
  Fensterfunktion: far und pascal (TP: $F+; FPC: pascal). RD2 ruft sie ueber CbProc auf. }
function Proc(w: HWND; msg, p1: Word; p2: Longint): Longint; {$IFDEF FPC} pascal; {$ENDIF}
var ps: TPaint; s: String;
begin
  Proc := 0;
  case msg of
    M_PAINT:
      begin
        Egal := BeginPaint(w, @ps);
        Egal := SetBkMode(ps.dc, BK_TRANSPARENT);
        Egal := SetTextColor(ps.dc, C_BLACK);
        TextOut(ps.dc, 10, 10, Z('Hallo aus Pascal!'), -1);
        Str(klicks, s);
        TextOut(ps.dc, 10, 30, Z('Klicks: ' + s), -1);
        EndPaint(w, @ps);
        if test then DbgPrint(Z('PHALLO: Paint ' + s));
      end;
    M_BUTTONDOWN:
      begin
        Inc(klicks);
        InvalidateRect(w, nil, 1);
      end;
    M_KEY:
      if p1 = K_ESC then Egal := DestroyWindow(w)
      else Proc := DefWindowProc(w, msg, p1, p2);
    M_DESTROY:
      PostQuit(klicks);
  else
    Proc := DefWindowProc(w, msg, p1, p2);
  end;
end;

var
  wc: TWndClass;
  m: TMsg;
begin
  test := (RD2Cmd <> nil) and (Mem[Seg(RD2Cmd^):Ofs(RD2Cmd^)] = Ord('T'));
  wc.name := Z('PHallo');
  wc.proc := CbProc(@Proc);
  wc.stil := 0; wc.daten := 0; wc.inst := RD2Inst; wc.symbol := 0;
  wc.hinter := RD_DEFAULT;
  if RegisterClass(@wc) = 0 then Halt(1);
  fe := CreateWindow(Z('PHallo'), Z('Hallo Pascal'), WS_TITLE or WS_CLOSE or WS_BORDER or WS_VISIBLE,
                     100, 100, 440, 200, 0, 0, RD2Inst);
  if fe = 0 then Halt(2);
  if test then DbgPrint(Z('PHALLO: Fenster da'));
  while GetMessage(@m) <> 0 do
    Egal := DispatchMessage(@m);
  Halt(klicks);
end.
