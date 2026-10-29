{ PTEST.PAS - Test der Unit RD2 (Turbo Pascal 5.5-7.0 und Free Pascal): Start, API, Befehlszeile, Heap, Ende }
program PTest;
{$M 8192,8192,16384}
uses RD2;
type PRec = ^TRec; TRec = record a, b: Longint; end;
var p: PRec; si: TSysInfo; s: String;
begin
  DbgPrint(Z('PASCAL: Start'));
  DbgHex(Z('PASCAL: Kern-API'), KernVersion);
  DbgPrint(RD2Cmd);
  s := ParamStr(1);
  DbgPrint(Z('PASCAL: Param1=' + s));
  New(p); p^.a := 12345; p^.b := 0;
  DbgHex(Z('PASCAL: Heap'), p^.a);
  Dispose(p);
  DbgHex(Z('PASCAL: MaxAvail'), MaxAvail);
  if KernInfo(@si) = 0 then DbgHex(Z('PASCAL: konv KB'), si.kb_konv);
  DbgPrint(Z('PASCAL: Ende'));
  Halt(42);
end.
