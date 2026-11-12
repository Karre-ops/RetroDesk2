---
title: Plan DOS-Kompatibilität
summary: Wie RetroDesk 2 DOS-Programme so startet, dass sie wie unter reinem DOS laufen (Stand 10.11.2026, Schritt 1 umgesetzt)
---
# Plan: DOS-Programme 100 % kompatibel starten

Stand 10.11.2026. Analyse von `src/kern/RUNDOS.C` und `src/kern/RDSWAP.ASM`.

**Umgesetzt (09.11.2026):** Hauptumgebung. Hat RD selbst kein COMSPEC (SHELL=RD2.EXE), baut der Rest vor jedem DOS-Programm eine Umgebung aus den SET- und PATH-Zeilen der AUTOEXEC.BAT (Startlaufwerk) und setzt COMSPEC auf COMMAND.COM im RD2-Ordner; sonst erbt das Programm die Umgebung von RD. Platz: 512 Bytes im Rest (RunDos-Rest 152 statt 120 Paragraphen). Test: `obj/dbxenv` in DOSBox, `test/rundos.sh` 32 ok.
**Umgesetzt (10.11.2026):** Rest nach unten. Nach dem Auslagern zieht der Rest in einen UMB (DOS=UMB) oder, ohne UMB, in den ersten freien Block gleich hinter dem PSP von RD; das Programm lädt darüber und darf den Speicher bis A000h beschreiben. Danach zieht der Rest (außer im UMB) wieder nach oben, damit Kern und Arena an ihre alte Adresse können. Bei Neustart und Shell (ohne XMS und Festplatte) bleibt er unten. Dabei behoben: Seit dem 09.11. lagen Stapel und Hauptumgebung im Code des Rests (Code 2046 Bytes, Stapel und ENVO bei 1920); jetzt Daten, Code (höchstens 2240), Stapel bis 2368, Umgebung 384 Bytes, zusammen 172 Paragraphen. RDSWAP.BIN enthält den leeren Datenbereich nicht mehr (1801 statt 2046 Bytes). Neuer Test: `test/dos/OBEN.COM` beschreibt den Speicher bis A000h, in `test/rundos.sh` mit XMS, Auslagerdatei und UMB (alter Stand: RD2 kommt nicht zurück, neuer Stand: ok).
**Offen:** Schritte 2 bis 4.

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
2. **Keine Hauptumgebung.** Unter `SHELL=RD2.EXE` setzt niemand COMSPEC, PATH, PROMPT oder die
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

1. Rest nach unten (erledigt 10.11.2026), Hauptumgebung (erledigt 09.11.2026)
2. Sauberer Ausgangszustand, .BAT
3. Startart B mit Auswahl
4. Testsatz
