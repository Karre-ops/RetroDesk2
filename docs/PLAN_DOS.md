---
title: Plan DOS-Kompatibilität
summary: Wie RetroDesk 2 DOS-Programme so startet, dass sie wie unter reinem DOS laufen (Stand 08.11.2026, noch nicht umgesetzt)
---
# Plan: DOS-Programme 100 % kompatibel starten

Stand 08.11.2026. Analyse von `src/kern/RUNDOS.C` und `src/kern/RDSWAP.ASM`, **noch nicht umgesetzt**.

## Heute

`RunDos` lagert Kern und belegte Arena nach XMS oder in eine Datei aus. Ein Rest (`RDSWAP.BIN`,
120 Paragraphen) startet das Programm mit 4B00h und holt danach alles zurück. Gesichert und
wiederhergestellt werden Interrupt-Vektoren (1 KB) und die Maske des Interrupt-Controllers; der
Zeitgeber geht vorher auf 18,2 Hz. Ohne XMS und Festplatte beendet sich RD2 und startet danach neu.

## Lücken

Im Code bestätigt:

1. **Rest ganz oben im Speicher** (`k_stub`: Strategie "zuletzt passend"). Programme, die ihre
   DOS-Zuteilung ignorieren und bis A000h schreiben (Spiele, manche Entpacker), überschreiben ihn,
   die Rückkehr stürzt ab.
2. **Keine Hauptumgebung.** Unter `SHELL=RD.EXE` setzt niemand COMSPEC, PATH, PROMPT oder die
   SET-Zeilen aus AUTOEXEC.BAT. Programme, die zu DOS verzweigen oder PATH, BLASTER=, TEMP= lesen,
   scheitern oder verhalten sich anders.
3. **Residente Programme werden entfernt** (`st_los`). KEYB, MSCDEX, DOSKEY usw. lassen sich aus
   RD2 nicht laden.

Zu prüfen:

4. .BAT-Dateien und interne Befehle brauchen COMMAND.COM /C.
5. Hardwarezustand vor dem Programm: Textmodus mit BIOS-Schrift, Palette, Cursor, Maustreiber
   zurückgesetzt (INT 33h 0), Tastaturpuffer und Umschalt-Flags, AdLib/Sound Blaster stumm,
   UART-Register.
6. Offene RD2-Dateien belegen Plätze aus FILES=.
7. Auslagern nach XMS lässt Programmen weniger XMS (Windows 3.x, DOS-Extender).
8. DOS-Uhr nach der Rückkehr (Zeitgeber war umprogrammiert).

## Lösung

### Startart A: schnelle Rückkehr (Standard)

- **Rest nach unten**: an den Anfang des eigenen RD-Blocks (hinter den PSP), das Programm lädt
  darüber. Mit UMB kommt der Rest dorthin (0 KB konventionell).
- **Hauptumgebung** beim Start bauen: COMSPEC (gefundenes COMMAND.COM), PATH, PROMPT, SET-Zeilen
  aus AUTOEXEC.BAT, BLASTER= aus der Erkennung; änderbar in RD2.INI `[Umgebung]`. Jedes Programm erbt sie.
- **Sauberer Ausgangszustand**: Grafik aus, Textmodus 3/7 mit BIOS-Schrift, Maus zurückgesetzt,
  Tastatur geleert, Klang aus, Netz frei, RD2-Dateien geschlossen (außer Auslagerungsdatei).
- **.BAT und interne Befehle** automatisch über COMMAND.COM /C.
- Ziel: höchstens 2 KB weniger frei als reines DOS mit denselben Treibern.

### Startart B: volle Kompatibilität (Neustart)

RD2 speichert die Sitzung, beendet sich ganz, das Programm läuft über COMMAND.COM, danach startet
RD2 neu und stellt die Fenster wieder her. Im Speicher bleiben nur RDKEYB und RDMAUS. XMS bleibt
ganz frei, residente Programme dürfen bleiben. Kostet einige Sekunden.

### Auswahl

- Automatisch: will ein Programm resident bleiben oder braucht es viel XMS, empfiehlt RD2 Startart B.
- Je Programm einstellbar (Eigenschaften bzw. PROGRAMM.LST): Automatisch, Schnell, Voll.
- Liste bekannter Problemprogramme startet immer mit B.

Nur Startart B ist wirklich 100 %. A erreicht fast alle Programme, die Auswahl fängt den Rest ab.

## Prüfung

Kompatibilitäts-Testsatz in QEMU (geht in den vollen Testsatz): kleine Prüfprogramme, die bis
A000h schreiben, resident bleiben, über COMSPEC verzweigen, PATH und Variablen lesen, eine .BAT
starten, den Grafikmodus anlassen, INT 8 und den Zeitgeber umprogrammieren, 30 Dateien öffnen,
auf Strg+Untbr reagieren. Dazu echte Programme (FreeDOS-Werkzeuge, DOS-Extender, Turbo Pascal)
und Vergleich des freien Speichers mit reinem DOS.

## Reihenfolge

1. Rest nach unten, Hauptumgebung
2. Sauberer Ausgangszustand, .BAT
3. Startart B mit Auswahl
4. Testsatz
