@ECHO OFF
REM RD2INST.BAT - RetroDesk 2 von einem Festplattenordner auf C:\RD2 installieren
REM (z. B. nach dem Laden per FTP mit RetroDesk 1). Aufruf: RD2INST [Quellordner]
REM Ohne Angabe ist der Quellordner C:\RD2NEU. Ordner ohne \ am Ende angeben.
SET Q=%1
IF "%Q%"=="" SET Q=C:\RD2NEU
IF NOT EXIST %Q%\RD.EXE GOTO FEHLT
IF NOT EXIST %Q%\INSTALL.EXE GOTO FEHLT
IF NOT EXIST %Q%\RD2SETUP.EXE GOTO FEHLT
IF EXIST %Q%\RD2.INI GOTO SETUP
ECHO RetroDesk 2 startet. In der Einrichtung die Werte pruefen und I druecken.
ECHO Danach Ziel C:\RD2 bestaetigen. RD2 laeuft nach dem Neustart von C:\RD2.
GOTO START
:SETUP
ECHO Einrichtung: Werte pruefen und I druecken.
%Q%\RD2SETUP.EXE
:START
%Q%\RD.EXE %Q%\SHELL.EXE
GOTO ENDE
:FEHLT
ECHO RetroDesk 2 nicht vollstaendig in %Q% gefunden.
ECHO Aufruf: RD2INST Quellordner   (z. B. RD2INST C:\RD2NEU)
:ENDE
SET Q=
