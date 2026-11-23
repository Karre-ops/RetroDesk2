@echo off
rem MKM32 NAME - 32-Bit-Modul NAME.M32 fuer RD2/32 bauen (unter DOS, Open Watcom).
rem Braucht NAME.LNK (wlink: "file RD32M0.OBJ,NAME.OBJ,RD32LIB.OBJ,RD32API.OBJ" und "library RD32C.LIB,...")
rem und NAME.EXP (Namen der Exporte, je Zeile einer). Linkt zweimal (Offset 0 und 1 MB), RD32MOD rechnet die
rem Verschiebungen aus. RD32M0.OBJ muss in NAME.LNK zuerst stehen. Die Optionen gehen ueber eine Steuerdatei
rem (MKM32.$$$), weil die DOS-Befehlszeile hoechstens 127 Zeichen lang ist. "format" muss vor "offset" stehen.
if "%1"=="" goto hilfe
echo option quiet> MKM32.$$$
echo option nodefaultlibs,eliminate,start=rd32_mkopf>> MKM32.$$$
echo @%1.LNK>> MKM32.$$$
wlink format raw bin name %1.0 option offset=0,map=%1.MAP @MKM32.$$$
if errorlevel 1 goto ende
wlink format raw bin name %1.1 option offset=1048576 @MKM32.$$$
if errorlevel 1 goto ende
RD32MOD %1.M32 %1.0 %1.1 %1.MAP %1.EXP
del %1.0
del %1.1
:ende
if exist MKM32.$$$ del MKM32.$$$
goto aus
:hilfe
echo MKM32 NAME   (NAME.LNK und NAME.EXP siehe Kopf dieser Datei)
:aus
