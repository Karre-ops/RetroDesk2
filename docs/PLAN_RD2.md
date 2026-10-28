# RetroDesk 2: Umsetzungsplan

Stand: 19.10.2026. Grundlage: [ENTWURF_RD2.md](ENTWURF_RD2.md)

## 0. Festgelegt

| Punkt | Entscheidung |
|---|---|
| Mindest-DOS | DOS 5 |
| Zielrechner Stufe 1 | PC XT, 8088 4,77 MHz, 640 KB, **ohne Festplatte**, ein 720-KB-Laufwerk (PC10-III) |
| Zielrechner Stufe 2 | 386 bis Pentium, 32 MB, Festplatte |
| Grafik Stufe 1 | CGA 640x200, Hercules 720x348, VGA 640x480 mono |
| Sound Stufe 1 | PC-Speaker, AdLib |
| Sprache | C (Open Watcom), Assembler für schnelle Grafikroutinen |
| RetroDesk 1.x | bleibt auf `main` und wird gepflegt (Bugfixes), RD2 entsteht im Branch `rd2` im selben Repo unter `rd2/` |

Entschieden am 19.10.2026: Menüleiste global oben, Systemmenü "RD", Notbeenden Strg+Alt+Rückschritt, Symbole 32x32 (CGA 32x16). Phase 0 freigegeben.

Noch offen, Empfehlung in Klammern:
- Extender für den 386-Modus (DOS/32A)
- Name ("RetroDesk 2")

## 1. Grundregel: Diskettenbetrieb

RD2 muss vollständig von **einer bootfähigen 720-KB-Diskette** starten und benutzbar sein. Daraus folgen feste Regeln für alle Phasen:

1. **Größenbudget mit Prüfung im Test.** Jede Datei hat ein Höchstmaß, der Build bricht ab, wenn es überschritten wird (wie heute das 64-KB-Limit von RDESK).
2. **Keine Auslagerungsdatei auf Diskette.** Zu langsam und verschleißt die Diskette. Ausgelagert wird nur nach EMS (wenn vorhanden), sonst gar nicht.
3. **Code nachladen nur, wenn nötig.** Verwerfbare Code-Teile (Dialoge, selten genutzte Funktionen) werden von Diskette nachgeladen. Der Kern merkt sich die **Seriennummer der Diskette**, von der ein Programm kam, und fordert sie bei Bedarf an: "Bitte Diskette RD2 SYSTEM in Laufwerk A: einlegen". So wie GEM und Windows auf Diskettenrechnern.
4. **Schreiben auf Diskette selten und sicher.** Zustand (`RD2.STA`) und Einstellungen (`RD2.INI`) werden nur beim Beenden und beim Programmstart geschrieben, zuerst in eine Zwischendatei, dann umbenannt. Ist die Diskette schreibgeschützt, läuft RD2 trotzdem, nur ohne Speichern.
5. **Startzeit.** Ziel: von "RD" bis Desktop höchstens 15 Sekunden auf dem XT mit Diskette (Lesen ca. 15 bis 20 KB/s, also höchstens ca. 200 KB beim Start laden).
6. **Ein Laufwerk reicht.** Diskettenwechsel zwischen System- und Programmdiskette muss überall funktionieren, auch beim Kopieren im Dateimanager (DOS-Laufwerk B: als Phantomlaufwerk wird vom Kern abgefangen und als Dialog gezeigt).

### Diskettenaufteilung

**Diskette 1: RD2 SYSTEM (bootfähig, 720 KB = 713 KB nutzbar)**

| Datei | Plattenplatz (ca.) | Bemerkung |
|---|---|---|
| IO.SYS, MSDOS.SYS, COMMAND.COM | 118 KB | DOS 5 (FreeDOS für die automatischen Tests) |
| CONFIG.SYS, AUTOEXEC.BAT | 1 KB | |
| Maustreiber | 10 bis 56 KB | Microsoft MOUSE.COM oder CuteMouse (ca. 10 KB) |
| KEYB, KEYBOARD.SYS | 30 KB | optional |
| `RD.EXE` (Starter, Kern, Speicher) | 16 KB | |
| `RDUSER.MOD` | 26 KB | Fenster, Menüs, Dialoge |
| `RDGDI.MOD` | 14 KB | |
| `CGA.DRV`, `HGC.DRV`, `VGAM.DRV` | je 4 KB | |
| `RDSND.MOD`, `SPEAKER.DRV`, `ADLIB.DRV` | 8 KB | |
| Schriften `SYS8.FNT`, `SYS14.FNT`, `SYS16.FNT` | 10 KB | |
| `SHELL.EXE` (Desktop, Dateimanager) | 40 KB | |
| Zubehör: Uhr, Notizblock, kleiner Rechner | 30 KB | |
| `DE.LNG`, `EN.LNG` | 8 KB | Texte |
| `RD2.INI`, `.APP`-Dateien | 2 KB | |
| **Summe** | **ca. 330 bis 380 KB** | |
| **frei für Daten** | **ca. 330 KB** | |

**Diskette 2: RD2 PROGRAMME** (nicht bootfähig): RDCALC, RDEDIT, FTP, IRC, RDNET, Druckertreiber, weitere Programme. Jede Programmdiskette hat eine `PROGRAMM.LST`, die die Shell beim ersten Einlegen anmeldet.

**Diskette 3: RD2 SDK** (nur für Entwickler): Header, Bibliotheken, Ressourcen-Compiler, Beispiele, Handbuch.

Auf dem Pentium mit Festplatte installiert das Setup alles nach `C:\RD2`.

### Speicherbudget XT (640 KB, kein EMS)

| Teil | KB |
|---|---|
| DOS 5 (ohne HMA, Kopie im unteren Speicher) | 60 |
| Maustreiber, KEYB, ggf. Packet-Treiber | 15 bis 30 |
| Kern + Treiber + GDI + Schrift | 40 |
| RDUSER | 26 |
| RDSND | 6 |
| Shell (Teile verwerfbar) | 30 |
| **Summe** | **ca. 180** |
| **frei für Programme** | **ca. 450** |

## 2. Phasen

Jede Phase endet mit einem lauffähigen Stand, automatischen Tests (DOSBox-X mit Bildspeicher-Prüfung, wie heute) und einem Commit auf `rd2`. Aufwand grob: S = 1 bis 3 Sitzungen, M = 4 bis 8, L = mehr als 8.

### Phase 0: Spezifikation (S)

Ergebnis: verbindliche Schnittstellen, noch kein lauffähiger Code.
- `rd2/sdk/RD2.H`: alle Funktionen, Typen, Nachrichten, Fehlercodes, **API-Version 1.0**. Regel: Funktionen werden nur ergänzt, nie geändert. Jedes Programm meldet im Kopf, welche Version es mindestens braucht.
- Aufrufkonvention `far pascal`, Tabelle der Einsprungpunkte mit festen Nummern (für INT 7Eh und Pascal/Assembler).
- Formate festlegen: RD2-Programmkopf, Ressourcen (Menü, Dialog, Text, Symbol, Bitmap), Schrift (`.FNT`), Sprachdatei (`.LNG`, Codepage 437), `.APP`, `RD2.INI`, `RD2.STA`.
- Treiber-Schnittstellen Grafik, Sound, Maus/Tastatur, Drucker, Netz als Funktionstabellen.
- **Netz-API**: Sockets mit Nachrichten (`NetOpen`, `NetSend`, `M_NETDATA`, `M_NETCLOSED`, `NetResolve`), kein Blockieren.
- Größenbudgets aus Abschnitt 1 als Tabelle in `rd2/tools/budget.txt`.

Abnahme: Du liest die Spezifikation und gibst sie frei.

### Phase 1: Gerüst und Testumgebung (S)

- Ordner `rd2/kern`, `gdi`, `user`, `snd`, `drv`, `shell`, `apps`, `sdk`, `tools`, `test`
- Build-Skript (16 Bit, Open Watcom), Größenprüfung gegen `budget.txt`
- **Diskettentest**: Skript baut ein bootfähiges 720-KB-Image (FreeDOS-Kern, da DOS 5 nicht weitergegeben werden darf) und startet es in DOSBox-X ohne Laufwerk C:, `machine=cga`, XT-Takt. Gleiches Image in QEMU (gibt es schon in `test/qemu`).
- Debug-Ausgabe: `DbgPrint()` schreibt auf COM1 oder in eine Datei, im Test mitgelesen
- Beispielprogramm "Leer", das nur startet und endet

Abnahme: Image bootet im Emulator, Testskript meldet "ok", Build bricht bei überschrittenem Budget ab.

### Phase 2: Kern (M)

- Starter `RD.EXE`: CPU, Speicher, EMS, Grafikkarte, Maus, Sound erkennen; `RD2.INI` lesen; Notstart `RD /SAFE` (CGA, Speaker, keine Zusatzprogramme)
- Speicher: Global Heap mit Handles (fest, verschiebbar, verwerfbar), Verdichten, EMS-Auslagerung wenn vorhanden, Anzeige des freien Speichers
- Module laden: MZ-EXE über INT 21h 4B03h, Ressourcen bei Bedarf, Code verwerfen und **von Diskette mit Seriennummer-Prüfung** nachladen (Dialog "Diskette einlegen")
- Tasks: eigener Stack je Programm, nachrichtengesteuert plus Zeitscheiben (55 ms) außerhalb von DOS und Kern, 8087-Zustand je Task, `GetMessage`/`PostMessage`/`SendMessage`, Warteschlange je Task
- Zeitgeber (feiner als 18,2 Hz, alter INT 8 wird weiter bedient), `M_TIMER`
- Dateien: Hülle um DOS, INT 24h als Dialog, Phantomlaufwerk B: abfangen
- Profil (`ProfileGet/Set`), Zwischenablage
- Absturzschutz: Stack-Prüfung, ungültige Handles, Notbeenden per Tastenkombination

Abnahme (Textmodus, noch ohne Grafik): drei Testprogramme schicken sich Nachrichten, eines wird verworfen und von Diskette nachgeladen (mit Diskettenwechsel im Emulator), Speicher nach Beenden wieder vollständig frei.

### Phase 3: Grafik, Eingabe, Schriften (M)

- Treiber `CGA.DRV`, `HGC.DRV`, `VGAM.DRV` aus heutigem `gfx.c`/`gfxfast.c` (Grundfunktionen: Zeile füllen, Punkt, Linien, Blit, Zeichen, Zeiger)
- RDGDI: Gerätekontext, Clipping mit Rechtecklisten, Stifte, Muster, Linie, Rechteck, Ellipse, Polygon, Bitmaps im Speicher, `BitBlt` mit Rasteroperationen, Dithering für Farben auf Monochrom
- Schriftformat mit festen und proportionalen Schriften, Fett/Kursiv durch Berechnung; `SYS8`, `SYS14`, `SYS16` aus den heutigen Schriften
- Maus über INT 33h (eigener Zeiger wie heute), Tastatur mit Umlauten (Codepage 437)

Abnahme: Testbild mit allen Grundfunktionen auf CGA, HGC und VGA mono, Pixelvergleich gegen Referenzbilder (wie heute `shotdiff.py`), Zeitmessung mit RDPERF-Methode auf XT-Takt.

### Phase 4: Fenster und Bedienelemente (L)

- Fensterklassen, `CreateWindow`, Rangfolge, Aktivieren, Verschieben, Größe ändern, Minimieren zum Symbol
- Neuzeichnen über ungültige Bereiche (`M_PAINT`), Popups mit Hintergrundsicherung
- Globale Menüleiste (oder pro Fenster, je nach Entscheidung), Tastaturbedienung (Alt, Tab, Alt+Tab)
- Steuerelemente: Knopf, Häkchen, Optionsfeld, Eingabefeld, mehrzeiliges Eingabefeld, Liste, Klappliste, Rollbalken, Fortschrittsbalken
- Dialoge aus Ressourcen, Standarddialoge: Meldung, Ja/Nein, Datei öffnen/speichern (mit Diskettenwechsel), Eingabe
- Ressourcen-Compiler `RDRC` (läuft auf dem Entwicklungs-PC und unter DOS)
- Sprachdateien: Texte über Nummern, eine EXE für DE und EN
- Ziehen und Ablegen zwischen Fenstern (`M_DROPFILES`)

Abnahme: Demo mit drei Fenstern aus zwei Programmen, Bedienung nur mit Tastatur und nur mit Maus, Tests für jedes Steuerelement.

### Phase 5: Shell (M)

- Desktop mit Laufwerks- und Programmsymbolen, globale Menüleiste mit Uhr
- Dateimanager aus heutigem `fm.c` als RD2-Programm: mehrere Fenster, Kopieren/Verschieben/Löschen/Umbenennen, Formatieren (heutiges RDFMT), Laufwerksinfo
- Programmanmeldung über `.APP` und `PROGRAMM.LST` von Programmdisketten
- Doppelklick auf Dateitypen öffnet das passende Programm
- Diskettenwechsel-Erkennung (Liste aktualisiert sich nach Wechsel)
- Zustand in `RD2.STA` (offene Fenster, Ordner) wie heute in RDESK.STA

Abnahme: alle heutigen Dateimanager-Tests aus `test/all.sh` laufen sinngemäß gegen die neue Shell, Start von Diskette ohne C: unter 15 s (gemessen im Emulator mit XT-Takt und Diskettenverzögerung).

### Phase 6: Sound (S)

- RDSND mit `SPEAKER.DRV` und `ADLIB.DRV` aus heutigem `snd.c`
- Systemklänge per Name, Noten/Instrumente, Musik-Player für IMF/DRO im Zeitgeber-Interrupt, Sound-Fokus
- Joystick-Abfrage (Spiele) als kleine Ergänzung im Eingabetreiber

Abnahme: Klangtest-Programm, OPL-Register-Mitschnitt im Emulator gegen Erwartung (Ton selbst nur am echten Rechner prüfbar).

### Phase 7: DOS-Programme starten (M)

- `RunDos()`: Zustand sichern, mit EMS den ganzen RD2-Speicher dorthin, ohne EMS nur Zustand in `RD2.STA` und Programme beenden (mit Rückfrage bei ungespeicherten Daten)
- Rest-Stub (ca. 4 KB, aus heutigem `RDGO.COM` weiterentwickelt) startet das DOS-Programm und danach RD2 neu, Diskettenwechsel inklusive ("Bitte RD2-Diskette wieder einlegen")
- `.APP`-Einträge für DOS-Programme mit Speicherbedarf und Parametern

Abnahme: SimCity und Prince of Persia aus der Shell starten und zurückkehren, im Emulator mit und ohne EMS.

### Phase 8: Programme portieren (M bis L)

Reihenfolge nach Größe und Nutzen:
1. Uhr, Notizblock, kleiner Rechner (Zubehör auf der Systemdiskette)
2. RDEDIT
3. RDCALC mit Kurven (Grafik über RDGDI, 8087 wie heute)
4. **Drucken**: Druckertreiber 9-Nadel (ESC/P) über denselben Gerätekontext, Druckdialog. RDCALC und RDEDIT drucken darüber
5. RDNET (heutiger TCP-Stack als Modul hinter der Netz-API), DHCP, FTP, IRC
6. Serielle Schnittstelle als Treiber (Nullmodem, Modem)

Abnahme: jedes Programm mit eigenen Tests, alle laufen gleichzeitig (so viele, wie in 450 KB passen, Zielwert: Shell + 3 Programme).

### Phase 9: Setup, Hilfe, SDK, Version 2.0 (M)

- Setup: Disketten-Images bauen, Installation auf Festplatte (optional), Treiberwahl, Notstart
- Hilfesystem (Text mit Verweisen, F1 in jedem Programm)
- SDK: `RD2.H`, `RD2.LIB`, `RD2START.OBJ`, `RD2.PAS` (Turbo Pascal), `RDRC`, Beispiele (Hallo, Malprogramm, Klangtest), Handbuch DE/EN
- Test am PC10-III von Diskette, dann Release **RD2 2.0** (drei Disketten-Images im GitHub-Release)

### Phase 10 und später: 386 bis Pentium

Eigener Plan nach 2.0. Kurz: 32-Bit-Kern mit DOS/32A, Thunks für 16-Bit-Programme, VGA 16 Farben, VESA 256 Farben, Sound Blaster mit Samples, 32-Bit-Programme, später DOS-Textprogramme im Fenster (V86). Phase 0 legt dafür die API schon 16/32-Bit-tauglich an.

## 3. Reihenfolge und Abhängigkeiten

```
P0 Spezifikation
 └─ P1 Gerüst, Diskettentest
     └─ P2 Kern
         ├─ P3 Grafik ── P4 Fenster ── P5 Shell ── P7 DOS-Programme
         │                    └─────── P8 Programme
         └─ P6 Sound (parallel zu P4/P5 möglich)
                                        └─ P9 Setup, SDK, Version 2.0
```

## 4. Risiken und Gegenmaßnahmen

| Risiko | Gegenmaßnahme |
|---|---|
| Kern und Shell passen nicht ins Budget | Budget wird ab Phase 1 bei jedem Build geprüft; notfalls Zubehör auf Diskette 2 |
| Zu langsam auf 4,77 MHz | Zeitmessung ab Phase 3 in jedem Test, Grafik-Grundfunktionen in Assembler |
| Nachladen von Diskette nervt | wenig verwerfbarer Code, mit EMS gar kein Nachladen; Messung in Phase 2 |
| Zu wenig Speicher für mehrere Programme | Speicheranzeige, Programme melden Bedarf in `.APP`, Shell warnt vor dem Start |
| DOS 5 im Testimage nicht verteilbar | automatische Tests mit FreeDOS, Abnahme jeder Phase zusätzlich am PC10-III mit DOS 5 |
| Projekt wird zu groß | jede Phase ist für sich lauffähig; RetroDesk 1.x bleibt parallel nutzbar |

## 5. Was ich von dir brauche

- Phase 0: Freigabe der Spezifikation, Entscheidung Menüleiste
- Ab Phase 2: Test am PC10-III nach jeder Phase (Diskette per Greaseweazle schreiben)
- Phase 6: Hörprobe AdLib/Speaker am echten Rechner
- Phase 8: Test am echten Nadeldrucker
