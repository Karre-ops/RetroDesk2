# RetroDesk 2: Spezifikation (Phase 0, fortgeschrieben bis API 2.0 / RD2 2.1, Stand 01.11.2026)

Stand: 19.10.2026, **freigegeben** am 19.10.2026. Grundlage: [ENTWURF_RD2.md](archiv/ENTWURF_RD2.md), [PLAN_RD2.md](archiv/PLAN_RD2.md)

Verbindlich sind die Header im SDK, dieses Dokument erklärt sie und legt die Dateiformate fest:

| Datei | Inhalt |
|---|---|
| `sdk/RD2API.H` | alle 199 Funktionen mit fester Nummer (einzige Quelle) |
| `sdk/RD2.H` | Typen, Strukturen, Konstanten, Nachrichten, Prototypen |
| `sdk/RD2DRV.H` | Treiber-Schnittstellen Grafik, Sound, Drucker, Eingabe, Netz |
| `tools/apicheck.py` | prüft Nummern und Namen, `--list` gibt die Tabelle aus |
| `tools/budget.txt` | Größengrenzen für Dateien und Speicher |
| `test/hdrtest.c` | Kompiliertest 16 Bit (Medium, Large) und 32 Bit |

## 1. Grundregeln

1. **API-Version** `RD2_API_MAJOR.MINOR` = 1.0. Jedes Programm trägt im Kopf die Mindestversion. Ist der Kern älter, startet es nicht ("Programm braucht RetroDesk 2 Version x.y").
2. **Nummern sind für immer.** Eine Funktion behält ihre Nummer und Signatur. Änderungen gibt es nur als neue Funktion, das erhöht MINOR. MAJOR ändert sich nur bei einem Bruch, und der ist nicht geplant.
3. **Aufrufkonvention** `far pascal` (`RDAPI`), alle Zeiger FAR. Das ist auf dem 8088 kurz und schnell und aus C, Turbo Pascal und Assembler gleich nutzbar.
4. **Handles** sind 16 Bit, auch im 32-Bit-Modus. 0 heißt immer "kein" bzw. Fehler.
5. **Texte** in Codepage 437, mit 0 abgeschlossen. `len = -1` heißt "bis zur 0".
6. **Multitasking (freigegeben 19.10.2026):** Programme sind nachrichtengesteuert und warten in `GetMessage`. Zusätzlich schaltet der Kern etwa alle 55 ms um, aber nur, wenn das unterbrochene Programm gerade in **seinem eigenen Code** ist, also nicht in DOS (InDOS-Flag), im BIOS oder in einer RD2-Funktion. Ein Programm, das lange rechnet, hält die anderen deshalb nicht an. Eingaben und Fensternachrichten bekommt es aber erst, wenn es wieder `GetMessage` oder `PeekMessage` aufruft. Programme dürfen keine eigenen Interrupt-Handler setzen und DOS nicht direkt aufrufen, nur über RD2-Funktionen.
7. **Fehler:** Funktionen liefern 0/FALSE/-1 und setzen `LastError()` auf `RD_ERR_*`.

## 2. Programmformat

Ein RD2-Programm ist eine **DOS-MZ-EXE** (Open Watcom, Modell Medium oder Large), gelinkt mit `RD2START.OBJ` und `RD2.LIB`. Hinter dem Programmbild hängt der **RD2-Anhang**, den `RDRC` anfügt:

```
+-----------------------+
| MZ-Kopf, Relokationen |
| Programmbild          |  <- Code, Daten, Stack (DGROUP)
+-----------------------+
| RD2-Block             |  Kopf, Exporttabelle, Ressourcenverzeichnis
| Ressourcen            |  Menüs, Dialoge, Texte, Symbole, ...
+-----------------------+
| Abschluss (16 Bytes)  |  "RD2!", Offset RD2-Block, Länge, Prüfsumme
+-----------------------+
```

DOS ignoriert alles hinter dem Programmbild, die EXE bleibt also eine gültige DOS-Datei.

### 2.1 Abschluss (letzte 16 Bytes der Datei)

| Offset | Typ | Inhalt |
|---|---|---|
| 0 | char[4] | `RD2!` |
| 4 | DWORD | Dateioffset des RD2-Blocks |
| 8 | DWORD | Länge RD2-Block + Ressourcen |
| 12 | WORD | Summe aller Bytes des RD2-Blocks (16 Bit) |
| 14 | WORD | 0 (frei) |

### 2.2 RD2-Block

| Offset | Typ | Inhalt |
|---|---|---|
| 0 | char[4] | `RD2P` Programm, `RD2M` Modul, `RD2D` Treiber |
| 4 | WORD | Mindest-API (z. B. 0x0100) |
| 6 | WORD | Flags `PF_*` |
| 8 | WORD | Stack in Bytes (aus `RD2_STACK`, Standard 4096) |
| 10 | WORD | Speicherbedarf in KB (Hinweis für die Shell) |
| 12 | char[16] | Anzeigename |
| 28 | WORD | Programmversion hi.lo |
| 30 | WORD | Anzahl Exporte |
| 32 | WORD | Anzahl Ressourcen |
| 34 | WORD | Offset Exporttabelle (ab Blockanfang) |
| 36 | WORD | Offset Ressourcenverzeichnis |
| 38 | WORD[5] | frei, 0 (Block ist 48 Bytes lang) |

Flags `PF_*`: `0x0001` 32 Bit, `0x0002` Zubehör (WS_TOOL), `0x0004` nur einmal starten, `0x0008` braucht 8087, `0x0010` braucht Netz, `0x0020` frei (früher EMS), `0x0040` frei, `0x0080` verwerfbar (`PF_DISCARD`, nur Module).

**Exporttabelle** (nur Module und Treiber): je Eintrag `WORD nummer, WORD offset, WORD segment` (Segment relativ zum Programmbild, der Kern addiert die Ladeadresse). Nummer 1 ist bei Treibern die Treibertabelle (`RD2DRV.H`).

**Ressourcenverzeichnis**: je Eintrag 16 Bytes:

| Typ | Inhalt |
|---|---|
| WORD | Typ `RT_*` |
| WORD | Nummer |
| WORD | Sprache (`RD_LANG_*`, 0 = alle) |
| WORD | Flags: `0x0001` sofort laden, `0x0002` verwerfbar (Standard) |
| DWORD | Offset ab RD2-Block |
| DWORD | Länge |

### 2.3 Laden und Start

1. Der Kern liest den MZ-Kopf und den Abschluss, prüft Kennung, Prüfsumme und Mindest-API.
2. Er reserviert `Bildgröße + e_minalloc` am Stück (fester Block) und lädt das Bild mit **INT 21h AX=4B03h** (DOS setzt die Relokationen).
3. Er setzt SS:SP aus dem MZ-Kopf, DS = SS (DGROUP), und ruft den Einsprung CS:IP mit
   `AX = 5232h ('R2')`, `ES:BX = API-Tabelle`, `CX = HINST`, `DX = show`, `DI:SI = Befehlszeile` (Segment:Offset).
4. `RD2START` setzt DS und SS auf die eigene DGROUP (eigener Stack, `RD2_STACK`), speichert die Tabelle, ruft `RdMain`, stellt den Stack des Kerns wieder her und kehrt mit `AX = Ergebnis` per `RETF` zurück.
   Kernfunktionen werden deshalb mit dem Stack des Programms aufgerufen. Der Kern ist mit `-zu` (SS != DS) gebaut, seine API-Funktionen laden DS selbst (`__loadds`).
5. Startet DOS die EXE direkt, steht in AX nie `5232h`. Dann gibt `RD2START` "Dieses Programm benötigt RetroDesk 2." aus und endet mit Fehlercode 1.

### 2.4 Was verschoben, ausgelagert und verworfen werden kann

| Teil | Modus R (8088) | Modus P (386+) |
|---|---|---|
| Programmbild (Code + DGROUP) | **fest**, solange das Programm läuft | verschiebbar |
| `MemAlloc(MEM_MOVABLE)` | verschiebbar | verschiebbar |
| `MemAlloc(MEM_DISCARDABLE)` | verwerfbar (Programm lädt neu) | verwerfbar |
| Ressourcen, Schriften, Bitmaps | verwerfbar, von Diskette nachladbar | verwerfbar |
| Module/Treiber (`.MOD`, `.DRV`) | verwerfbar, wenn Sperrzähler 0 und Flag `PF_DISCARD` | verwerfbar |

Ein Programmbild ist auf dem 8088 fest, weil seine Segmentadressen nach der Relokation im Code stehen. Verschieben würde es zerstören. Platz schaffen deshalb Datenblöcke, Ressourcen und Module.

**Module werden nur über Sprungstücke im Kern aufgerufen.** `ModProc` liefert nicht die echte Adresse, sondern ein Stück im Kern. Das lädt das Modul bei Bedarf (mit `DiskRequest`), erhöht den Sperrzähler, ruft auf und senkt den Zähler wieder. Ein Modul, das gerade läuft, wird so nie verworfen.

**Speicherstrategie (21.10.2026).** EMS wird nicht genutzt (zu selten vorhanden). Ziel ist ein kleiner fester Teil in den 640 KB; der Build prüft je Datei den Speicher nach dem Laden (MZ-Bild + Mindestzuteilung) und die Summe im Profil Schnell (`tools/budget.txt`).
- Module und Treiber haben keinen eigenen Stack (laufen auf dem Stack des Aufrufers), `wlink option stack=16`.
- Selten Gebrauchtes liegt in verwerfbaren Zusatzmodulen, z. B. `RDGDIX.MOD` (Ellipse, runde Rechtecke, Polygone, Füllen), das RDGDI erst beim ersten Aufruf lädt.
- **Hoher Speicher (UMB):** Gibt DOS welchen her (`DOS=UMB`), nimmt ihn der Kern dazu. Module und Treiber kommen bevorzugt dorthin, Programme bleiben unten.
- **XMS:** Mit HIMEM legt der Kern beim ersten Laden eines verwerfbaren Moduls das unberührte Bild samt Verschiebetabelle in einen XMS-Block (`RD2.INI [System] XMS=` KB, Vorgabe 256, 0 = aus). Nachladen kommt dann von dort, an jede Adresse umgerechnet, ohne Diskette. Code läuft nie im XMS: im Real Mode erreicht die CPU nur das erste MB.
- **Auslagerung (freigegeben 23.10.2026, gebaut 25.10.2026):** Reicht der Speicher nicht, geht der Kern in dieser Reihenfolge vor: 1. verschiebbare Blöcke zusammenschieben (verdichten), 2. den ältesten verwerfbaren Block oder ein verwerfbares Modul verwerfen, 3. den ältesten verschiebbaren, nicht gesperrten Datenblock auslagern, nach jedem Schritt erneut verdichten. Erst wenn nichts mehr geht, kommt `RD_ERR_NOMEM`; die Shell meldet dann „Zu wenig Speicher für … Frei: … KB“. Ziel ist **XMS, wenn vorhanden** (ein XMS-Block je ausgelagertem Block), sonst **`RD2.SWP`** im RD2-Verzeichnis, aber nur auf einer Festplatte (C: und höher); die Datei wird beim Beenden gelöscht. Von Diskette wird nicht ausgelagert. `RD2.INI [System] Auslagern=0` schaltet es ab. Der nächste `MemLock` (oder `MemRealloc`) holt einen Block zurück; `MemFlags` meldet ausgelagerte Blöcke nicht als verworfen. Nie ausgelagert werden gesperrte, verwerfbare (die werden verworfen), Programmbilder, Module, Kernblöcke und Blöcke mit `MEM_NOEMS`. Die Logik liegt in `RDAUSL.MOD` (2 KB Datei, 2,4 KB Speicher), das der Kern nur behält, wenn es ein Ziel gibt; es bleibt geladen, weil es gerade dann gebraucht wird, wenn kein Speicher mehr frei ist. Im Kern sind es die Einstiege in `mem_get`, `MemLock`, `MemRealloc`, `MemFree`. Bis 32 Blöcke gleichzeitig. Später, nur bei Bedarf: ganze wartende Hintergrundprogramme auslagern.

### 2.5 C-Bibliothek

Erlaubt sind aus der Watcom-Bibliothek alles ohne DOS- und Bildschirmzugriff: `string.h`, `ctype.h`, `math.h` (mit 8087 oder Emulator), `stdlib.h` ohne `malloc`/`system`/`exit`, `sprintf`.
`RD2.LIB` ersetzt `malloc`, `free`, `realloc` und `calloc` durch `MemFixed`/`MemFixedFree`.
Nicht erlaubt sind `printf`, `getch`, `fopen` und Grafik- oder Interrupt-Funktionen. Stattdessen gibt es `TextOut`, `M_KEY` und `FileOpen`.

## 3. Module und Treiber

Module (`RDUSER.MOD`, `RDGDI.MOD`, `RDSND.MOD`, `RDNET.MOD`) und Treiber (`*.DRV`) haben dasselbe Format wie Programme, nur mit `RD2M` bzw. `RD2D` und ohne eigenen Task. Ihre API-Funktionen trägt der Kern beim Laden in die Sprungtabelle ein. Fehlt ein Modul, liefern seine Funktionen 0 (AX = DX = 0) und setzen `LastError()` auf `RD_ERR_NOTSUP` (z. B. Netzfunktionen ohne `RDNET.MOD`). Bei Funktionen, deren 0 Erfolg bedeutet (`NetCaps`, `KernInfo`), muss das Programm deshalb `LastError()` prüfen.

Treiber-Tabellen siehe `RD2DRV.H`. Wichtig:
- Ein Grafiktreiber muss nur `span`, `pixel`, `getpixel`, `put`, `get` liefern, alles Weitere ist optional und wird sonst von RDGDI nachgebaut.
- Treiber kennen kein Clipping und keine Fenster.
- Die Treibertabelle wächst nur hinten. `groesse` zeigt, welche Felder es gibt.
- Erweiterungen der Grafiktabelle (alle optional, erkannt an `h.groesse`):
  - `GFXDRVF` (`farbe`, `rgb`): Farbtreiber, Index zu einer Farbe und zurück.
  - `GFXDRVR` (30.10.2026): `rohgr`/`rohget`/`rohput` retten einen Bildschirmausschnitt unverändert (Ebenen bzw. Schirmbytes) und schreiben ihn an dieselbe Stelle zurück, auch in Teilen; `symput` setzt farbige Symbole mit Maske.
  - `GFXDRVV` (01.11.2026): Ablage im unsichtbaren Bildspeicher. `vrneu(w, h)` reserviert Platz für einen rohen Block und liefert 1..255 oder 0 (kein Platz), `vrfrei(n)` gibt ihn frei. RDGDI trägt dann `bpr = 0x8000 | n` in die Bitmap ein, `rohget`/`rohput` erkennen das und kopieren im Bildspeicher (EGA/VGA: über die Latches, Schreibmodus 1; Ränder mit Bitmaske). Der Treiber ermittelt den Platz selbst: VGA 64 KB je Ebene (Modus 12h und 11h: 26 KB hinter Zeile 480), EGA laut BIOS (INT 10h 12h, 16 KB je 64 KB Karte; EGA mono nur mit 256 KB), Hercules zweite Bildseite B800h (32 KB, nur ohne Farbkarte auf 3D4h und nach Speichertest). Ein Moduswechsel leert die Ablage. Umgesetzt in VGA16, VGA4, EGA, EGA4, EGA200, EGA320, VGAM, EGAM und HGC; CGA, MCGA, SVGA16 und VESA haben keine Ablage.

**Grafikwahl (beschlossen 20.10.2026).** Der Kern erkennt die CPU (`SYSINFO.cpu`). Daraus folgt das Profil:
- **Schnell** (8088, V20, 286): nur 1-Bit-Treiber, Reihenfolge Hercules, VGA mono, CGA (wie RetroDesk 1.x).
- **Komfort** (386 und schneller): zuerst Farbe (`VGA16.DRV`, später 256 Farben/VESA), sonst wie Schnell. Für 386+ gibt es später eigene Treiberfassungen mit 386-Befehlen.

Von Hand lässt sich jede Grafikart wählen, die die Hardware wirklich kann. Jeder Treiber prüft das in `init` (CGA: VGA/MCGA da, oder Farbkarte laut BIOS und ein 6845 auf 3D4h; Hercules: Strahlrücklauf auf 3BAh; VGA: INT 10h 1A00h). Mit einer VGA-Karte gehen also VGA und CGA, mit einer CGA-Karte nur CGA, mit Hercules nur Hercules.
- `GfxList` liefert genau diese Liste mit `GI_AKTIV`, `GI_GEWAEHLT` und `GI_STANDARD` (die automatische Wahl). Das wird das Menü „Grafik“.
- `GfxSelect(typ)` prüft die Hardware und schreibt `Grafik=` in `RD2.INI`. `0` heißt automatisch. Die Wahl gilt ab dem nächsten Start der Oberfläche; Umschalten im laufenden Betrieb kommt mit Phase 4 (alles neu zeichnen).
- `RD /CGA`, `/HGC`, `/VGA` gilt nur für diesen Start. Geht die eingestellte Art auf der Hardware nicht (z. B. Diskette in einen anderen Rechner gesteckt), nimmt RDGDI die automatische Wahl.
- `Profil=AUTO|SCHNELL|KOMFORT` in `RD2.INI` überschreibt das Profil.

## 4. Nachrichten und Tasks

- Jeder Task hat eine Warteschlange (32 Einträge, im Kern). Maus- und Zeitgebernachrichten werden zusammengefasst und erzeugt, wenn sie abgeholt werden (wie Win16). So läuft die Schlange nicht über.
- `SendMessage` an ein Fenster eines anderen Tasks wechselt zum Ziel-Task, ruft dessen Fensterfunktion auf und kehrt zurück. Das Programm wartet so lange.
- `PostMessage` stellt nur ein.
- Der Kern ruft Fensterfunktionen immer mit Stack und DS des Programms auf, dem das Fenster gehört. Exportieren oder `MakeProcInstance` ist nicht nötig.
- **Umschalten**: Kooperativ in `GetMessage`, `WaitMessage`, `TaskYield`, `PeekMessage` (wenn leer). Präemptiv aus dem Zeitgeber (PIT 72,8 Hz, Zeitscheibe 4 Takte = 55 ms), wenn CS:IP des unterbrochenen Programms in dessen Programmbild liegt, InDOS und das Kritischer-Fehler-Flag 0 sind und ein anderer Task bereit ist. Beim Umschalten werden Register, Flags und (falls vorhanden) der 8087-Zustand (FNSAVE, 94 Bytes) gesichert. `RD /KOOP` schaltet das präemptive Umschalten ab.
- **Notbeenden**: Strg+Alt+Rückschritt beendet das aktive Programm, wenn es länger als 5 Sekunden keine Nachricht abgeholt hat.
- Zeitgeber: Der Kern programmiert den PIT auf 72,8 Hz und ruft den alten INT 8 weiter mit 18,2 Hz auf. `TickCount` liefert ms.

Reihenfolge beim Start eines Fensters: `M_CREATE`, `M_SIZE`, `M_SHOW`, `M_ACTIVATE`, `M_PAINT`. Beim Schließen: `M_CLOSE` (Programm darf ablehnen, z. B. bei ungespeicherten Daten), dann `M_DESTROY`.

## 5. Oberfläche

- **Menüleiste: global oben** (wie RetroDesk 1.x und GEM). Sie zeigt das Menü des aktiven Fensters, die Uhr steht rechts. Freigegeben am 19.10.2026.
- Ganz links steht immer das Systemmenü "RD" (Info, Zubehör, Programmliste, Beenden), es gehört der Shell.
- Zubehör (`PF_TOOL`/`WS_TOOL`) hat kein eigenes Menü. Ist es aktiv, bleibt das Menü des zuletzt aktiven Programms stehen.
- Neu zeichnen über `M_PAINT`. Menüs retten ihren Hintergrund roh (`BMP_ROH`), bei Ablage im Bildspeicher ohne RAM. Hauptfenster mit `WS_POPUP` (Dialoge, `MsgBox`, `InputBox`) retten ihn beim Erscheinen nur in die Bildspeicher-Ablage (`BMP_NURVR`, höchstens zwei verschachtelt) und holen ihn beim Schließen zurück, statt die Fenster darunter neu zu zeichnen (01.11.2026). Malt währenddessen ein Fenster darunter (Inhalt) oder der Schreibtisch in den verdeckten Bereich, gilt die Rettung als veraltet und es wird normal neu gezeichnet. `EnableWindow` zeichnet Hauptfenster nicht neu (nur Kindfenster, die gesperrt anders aussehen). `CS_SAVEBITS` ist reserviert, aber nicht ausgewertet.
- Tastatur: Alt bzw. F10 öffnet die Menüleiste, Alt+Buchstabe ein Menü, Tab wechselt das Steuerelement, Alt+Tab das Programm, Alt+F4 schließt, F1 öffnet die Hilfe.

### 5.0 Schreibtisch und Shell (24.10.2026)

- `SHELL.EXE` legt ein Fenster mit `WS_DESKTOP` über den ganzen Schirm unter der Menüleiste. RDUSER hält es immer ganz unten (`SetActive` holt es nicht nach oben), es bekommt aber Klicks und kann aktiv sein.
- Das Menü **"RD"** gehört dem Schreibtisch: Klick auf "RD" oder **Alt+Leertaste** schickt `M_SYSMENU` an das `WS_DESKTOP`-Fenster, egal welches Programm aktiv ist. Die Shell öffnet es mit `MenuPopup`.
- Teil 1: Laufwerke als Symbole links (Abfrage mit `DI_NURTYP`, keine Diskette wird angefasst), Ordnerfenster mit Symbolraster (Ordner zuerst, bis 240 Einträge), Rollbalken, Ordner wechseln, `..`, Programme starten (RD2-EXE mit `TaskExec`, sonst `RunDos`), RD: Info, Beenden. Tastatur: Pfeile, Eingabe, Rückschritt, Alt+F4.
- Teil 2: Kopieren (F5), Verschieben (F6, auf demselben Laufwerk per Umbenennen), Umbenennen (F2), Neuer Ordner (F7), Löschen (Entf/F8), jeweils auch ganze Ordner. Markieren mit Einfg oder Strg+Klick, rechte Maustaste öffnet das Menü "Datei". Kopierziel vorgeschlagen: zuletzt aktives anderes Ordnerfenster. Rückfrage beim Ersetzen; Kopie in denselben Ordner oder Ordner in sich selbst wird abgelehnt. Test `test/desk2.sh`.
- Systemdateien (Karsten, 24.10.2026): Löschen, Ersetzen beim Kopieren, Verschieben (auch per Ziehen) und Umbenennen fragen bei Systemdateien ein zweites Mal, „Nein“ ist vorgewählt (`MB_DEFNO`). Als Systemdatei gilt: CONFIG.SYS, AUTOEXEC.BAT, KERNEL.SYS, IO.SYS, MSDOS.SYS, IBMBIO.COM, IBMDOS.COM, COMMAND.COM, RD.EXE und RD2.INI in der Wurzel eines Laufwerks, jede Datei mit Attribut System, das RD2-Verzeichnis samt Inhalt und die Ordner darüber. Text: „Achtung: … gehört zum System. Ohne sie startet der Rechner oder RD2 vielleicht nicht mehr. Trotzdem …?“ (DE/EN). Bei Nein bricht der ganze Vorgang ab, beim Ersetzen nur diese Datei. Test `test/sysdatei.sh`.
- Teil 3: **Ziehen mit der Maus** auf ein Ordnerfenster, einen Ordner darin, `..` oder ein Laufwerk. Gleiches Laufwerk = Verschieben, anderes = Kopieren; Strg erzwingt Kopieren, Umschalt Verschieben. Der Zeiger wird beim Ziehen zur Hand (ab 5 Pixel Weg). Ein Klick ohne Strg auf einen nicht markierten Eintrag hebt die Markierungen auf.
- **Laufwerksmenü** (Rechtsklick auf ein Laufwerk): Öffnen, Eigenschaften (auch Alt+Eingabe: Name, Gesamt, Frei), Formatieren (grau, bis `RDDISK.MOD` da ist). `MenuPopup` ignoriert das Loslassen der Taste, die das Menü geöffnet hat, solange die Maus nicht bewegt wurde (sonst würde sofort der erste Eintrag gewählt). Test `test/desk3.sh` (PS/2-Maus mit CTMOUSE).
- Laufwerksmenü: „Formatieren...“ startet `RDFORMAT.EXE A:` (aktiv, wenn RDFORMAT.EXE im RD2-Verzeichnis liegt und das Laufwerk eine Diskette oder Festplatte ist). „Bootfähig machen“ entfällt (Karsten, 24.10.2026: keine DOS-Bootdisketten; wenn, dann neue RD2-Disketten als vollständige Kopie).
- Folgt: Listenansicht, `.APP`-Programmliste, Zustand in `RD2.STA`.

### Formatieren (Beschluss 24.10.2026)

`RDFORMAT.EXE` (Diskette 2, GPL v2) = FreeDOS FORMAT 0.92a als Kern, mit RD2-Oberfläche. Quellen unverändert in `src/disk/fdformat` (Herkunft in `HERKUNFT.txt`), Anpassungen in eigenen Commits.
- Übernommen: Formatieren (createfs, floppy, hdisk, uformat, savefs, recordbc, bcread, driveio, init), Disketten und Festplatte (FAT12/FAT16), Prüfen, defekte Cluster, Schnellformat mit Wiederherstellungsdaten, SYS mit FreeDOS-Bootsektor (aus `boot.asm`, mit NASM gebaut).
- Ersetzt: Textausgabe und Rückfragen (`userint.c`, `main.c`, `prf.c`, kitten) durch RD2-Dialoge: Einstellungen, Rückfrage, Fortschrittsbalken (`Display_Percentage_Formatted`), Fehlermeldung (`Critical_Error_Handler`, `Exit` springt zurück in die Oberfläche).
- DOS- und BIOS-Zugriffe (INT 13h, INT 21h IOCTL 44xxh, INT 25h/26h) laufen über `RDDISK.MOD` (RD2-Programme rufen DOS nie direkt auf).
- **Festplatte (24.10.2026):** `RDFORMAT C:` zeigt Laufwerk und Größe, Bezeichnung und „Schnell“ (Vorgabe an: sicheres Schnellformat mit UNFORMAT-Daten, aus = `/U` mit Prüfung aller Sektoren). FreeDOS fragt vor dem Löschen nach (Text 22). Das Laufwerk, auf dem RD2 liegt (`SYSINFO.home`), lässt sich nicht formatieren. FAT12/FAT16 bis 2 GB, FAT32 nur unter DOS 7. Dazu erlaubt `RDDISK.MOD` INT 21h 60h (vollständiger Pfad, FORMAT prüft SUBST/Netz). Test `test/hdd.sh` (20-MB-Platte, FAT16): schnell, voll mit Bezeichnung, Nein bei der Rückfrage, RD2-Laufwerk.

`RDFDISK.EXE` (Diskette 2, GPL v2) = Free FDISK 1.3 (`pcompute.c`, `pdiskio.c`, MBR-Startcode `bootnorm.asm`) als Kern, Quellen in `src/disk/fdfdisk` (Herkunft in `HERKUNFT.txt`). `src/disk/fdglue.c` ersetzt Oberfläche, Befehlszeile und FDISK.INI, `src/disk/RDFDISK.C` ist die RD2-Oberfläche. Aufruf über das Laufwerksmenü „Festplatte einrichten...“ (aktiv, wenn RDFDISK.EXE im RD2-Verzeichnis liegt; eine leere Platte hat keinen Buchstaben, deshalb unabhängig vom gewählten Laufwerk).
- Dialog: Platte 1/2 (nur bei zwei Platten), Liste nach Lage (Laufwerk, Art, Dateisystem, aktiv, Größe; logische Laufwerke eingerückt unter der erweiterten Partition, freier Platz als „Frei“), Knöpfe Anlegen, Löschen, Aktiv, Schreiben, Schließen.
- Anlegen nach DOS-Regel: das erste DOS-Laufwerk wird primär und aktiv, weitere werden logische Laufwerke in einer erweiterten Partition über den restlichen Platz. Größe per Eingabe (Vorgabe: höchstens möglich); danach ist der restliche freie Platz gewählt.
- Löschen fragt nach; eine erweiterte Partition samt ihren logischen Laufwerken. Das Laufwerk, auf dem RD2 liegt, lässt sich nicht löschen.
- Erst „Schreiben“ ändert die Platte (Rückfrage). Danach fragt RDFDISK „Jetzt neu starten?“ (Audit M14, Variante A, 25.10.2026): bei Ja DOS-Puffer schreiben (INT 21h 0Dh), Warmstart über den Tastaturcontroller, auf dem XT Sprung nach FFFF:0. Bei Nein Hinweis, vor dem Formatieren neu zu starten. Test: kette.sh (Ja), fdisk.sh (Nein). Schließen mit Änderungen fragt nach dem Verwerfen.
- Immer wie DOS 6: nur FAT12/FAT16 (bis 2 GB je Laufwerk), keine LBA-Typen, keine INT-13h-Erweiterungen, damit auch MS-DOS 5 die Platte liest. Platten über 8 GB: nur der per CHS erreichbare Teil. Lage der logischen Laufwerke aus der EMBR-Kette (Audit M8).
- Test `test/fdisk.sh`: leere 20-MB-Platte (C: primär, aktiv), 100 MB (C: 40 MB + D: logisch), Löschen, Verwerfen (Platte unverändert); Python prüft MBR und EMBR.
- RD2SETUP benutzt denselben Kern für die Festplatte.
- Der eigene Formatier-Code aus RD1 (RDFMT) entfällt, sobald der FreeDOS-Kern läuft.

### 5.1 Steuerelemente und Dialoge (RDCTL, 23.10.2026)

`RDCTL.MOD` ist verwerfbar und wird erst geladen, wenn ein Programm einen Dialog oder eine Standardklasse braucht (RDUSER ruft bei unbekannter Klasse `CtlInit`). Fertig: `BUTTON`, `CHECK`, `RADIO`, `STATIC`, `GROUP`, `EDIT` (einzeilig, höchstens 80 Zeichen; der Vorgabetext aus der Vorlage ist wie jeder Fenstertext höchstens 31 Zeichen lang, längere Texte per `DlgSetText`), `LIST` (mit `WS_VSCROLL`, Einträge höchstens 32 Zeichen), `PROGRESS`, dazu `DialogRun`, `DialogEnd`, `DlgItem`, `DlgSet/GetText`, `DlgSet/GetInt`, `DlgCheck/Checked`, `MsgBox` (Knöpfe `MB_OK`, `MB_OKCANCEL`, `MB_YESNO`, `MB_YESNOCANCEL`, seit 01.11.2026 `MB_YESALLNO` und `MB_YESALLNOCANCEL` mit Knopf „Alle“, Rückgabe `ID_ALLE`; `MB_DEFNO` wählt Nein vor), `InputBox` und `DialogIndirect` (Vorlage im Speicher). `FileDialog` liegt in `RDCTLX.MOD` (3,3 KB, verwerfbar), das RDCTL nur für die Dauer des Dialogs lädt: Eingabefeld, Dateiliste nach dem ersten Filtermuster, rechte Liste mit `[..]`, Ordnern und Laufwerken (`[-A-]`, Phantom-B: ausgeblendet), Muster wie `*.TXT` im Eingabefeld ändern die Liste, `FD_MUSTEXIST` und `FD_ASKOVER` fragen per `MsgBox`. Ebenfalls in RDCTLX (RDCTL meldet die Klassen erst an, wenn ein Programm eine davon anlegt; RDUSER gibt das Modul am Ende frei): `TEXT` mehrzeilig bis 4 KB mit Rollbalken, `COMBO` Klappliste (F4 oder Alt+Pfeil runter öffnet, wächst nach unten, daher im Dialog als letztes Element ohne etwas darunter), `SCROLL` eigener Rollbalken (`C_SETPOS`, `C_SETRANGE`, `C_GETPOS`, Meldung `N_CHANGE`). Der Dateidialog hat eine Klappliste „Dateityp“ mit allen Paaren aus `filter`; die Wahl ändert das Muster sofort. Offen: `HelpShow`.

- Zustand steht nur in den Fensterdaten (`data` 0..3) und in Speicher des Programms, damit das Modul zwischen zwei Aufrufen verworfen werden darf. `data[3]` = `'RC'` + Typ.
- Nachrichten an Steuerelemente: `C_SETTEXT`, `C_GETTEXT`, `C_SETCHECK`, `C_GETCHECK`, `C_SETLIMIT`, `C_CLICK`, `C_MNEMO`, `C_SETPOS`, `C_SETRANGE`; Listen `L_ADD`, `L_CLEAR`, `L_COUNT`, `L_GETSEL`, `L_SETSEL`, `L_GETTEXT`, `L_DELETE`. Meldungen an die Eltern als `M_COMMAND` mit `N_CLICKED`, `N_CHANGE`, `N_SELECT`, `N_DBLCLK`.
- Tastatur im Dialog: Tab/Shift+Tab und Pfeile wechseln das Feld, Eingabe löst den fokussierten Knopf bzw. den Standardknopf aus, Esc = `ID_CANCEL`, Leertaste schaltet, Alt+Buchstabe (bzw. Buchstabe, wenn kein Eingabefeld den Fokus hat) wählt das Feld mit `&`. In Listen springt ein Buchstabe zum nächsten Eintrag damit.
- Modal: Der Besitzer ist gesperrt, solange der Dialog läuft; gesperrte Eltern sperren auch alle Kinder.

## 6. Ressourcen

Quelltext für `RDRC` (Textdatei `.RC`, CP437):

```
SPRACHE DE
MENU 1
  POPUP "&Datei"
    ITEM "&Neu\tStrg+N"      101
    ITEM "&Öffnen..."         102
    TRENNER
    ITEM "&Beenden"           109
  ENDE
ENDE
DIALOG 10 "Einstellungen" 0 0 220 90
  STATIC   "Name:"   -1   8  10  50 12
  EDIT               20  60   8 150 14
  CHECK    "&Ton"    21   8  30 100 12
  BUTTON   "OK"       1  60  65  50 16 STANDARD
  BUTTON   "Abbruch"  2 120  65  50 16
ENDE
TEXTE
  1 "Hallo von RetroDesk 2"
  2 "Datei nicht gefunden: %s"
ENDE
SYMBOL 1 "HALLO.RDB"
SPRACHE EN
MENU 1 ...
```

Dialogmaße sind in **Dialogeinheiten**: 1/4 Breite und 1/8 Höhe der Systemschrift. So passt derselbe Dialog auf CGA (8x8) und VGA (8x16).

Binärformate:
- **MENU**: Folge von Einträgen `WORD flags (MF_POPUP 1, MF_SEP 2, MF_END 4, MF_GRAY 8), WORD id, Text\0`.
- **DIALOG**: Kopf `DWORD stil, short x,y,w,h, WORD anzahl, Titel\0`, dann je Element `Klasse\0, Text\0, WORD id, short x,y,w,h, DWORD stil`.
- **STRING**: `WORD anzahl`, dann `WORD id, WORD offset`, dann die Texte.
- **ICON/BITMAP/CURSOR**: Format `.RDB` (Abschnitt 7).
- **ACCEL**: `WORD taste, WORD ks, WORD id`.

Sprache: Der Kern sucht zuerst die Ressource in `LangGet()`, dann Englisch, dann Sprache 0. Systemtexte (Kern, Shell) liegen in `DE.LNG`/`EN.LNG`. Das Format ist dasselbe wie STRING, mit Kopf `RDL1` + `WORD sprache`.

## 7. Bilder und Schriften

**Bitmaps (`BmpCreate(w, h, bpp)`)**: `bpp` 0 = Format des Schirms, 1, 4 oder 8 Bit. `BMP_MASKE` (zu 4/8 Bit): Maske hinter den Punkten, BitBlt setzt nur deckende Punkte (Symbole), fällt weg, wenn der Treiber kein `symput` hat. `BMP_ROH`: Hintergrund unverändert retten, nur BitBlt Schirm -> Bitmap -> dieselbe Stelle, nie hineinzeichnen; nutzt die Bildspeicher-Ablage des Treibers, sonst RAM. `BMP_NURVR` (mit `BMP_ROH`): nur Bildspeicher-Ablage, sonst liefert `BmpCreate` 0 (`RD_ERR_FULL`).

**`.RDB` Bitmap**: `RDB1`, `WORD w, h, bpp (1/4/8), flags (1 = mit Maske)`, bei bpp > 1 eine Palette (`n x 3 Bytes RGB`), dann Zeilen von oben, je Zeile auf ganze Bytes aufgefüllt. Mit Maske folgt danach die Maske als 1-Bit-Bild. Symbole sind 32x32 (VGA) und 32x16 (CGA, wegen des Seitenverhältnisses), beide in derselben Ressource-Nummer mit Flag-Bit 2 für die CGA-Fassung.

**`.FNT` Schrift**: `RDF1`, `WORD hoehe, oberlaenge, stil`, `BYTE fest, erstes, letztes, 0`, `char name[13]`, dann `BYTE breite[letztes-erstes+1]`, `WORD offset[...]` und die Zeichen zeilenweise, je Zeile `(breite+7)/8` Bytes. Fett und kursiv berechnet RDGDI, eigene Schnitte sind möglich (eigene Datei mit `stil`).

Mitgeliefert werden `SYS8.FNT` (8x8), `SYS14.FNT` (aus RDFONT.F14) und `SYS16.FNT` (8x16), später proportionale Schriften. 8x8 und 8x16 stammen aus dem VGA-BIOS von SeaBIOS (Schriften aus fntcol16.zip von Joseph Gil, gemeinfrei; geprüft 01.11.2026), 8x14 aus den gemeinfreien VGA14-Konsolenschriften von console-setup, `tools/mkfont.py` erzeugt alle drei beim Bauen. Zeichen 32 bis 255, Codepage 437 (ä = 0x84, ö = 0x94, ü = 0x81, Ä = 0x8E, Ö = 0x99, Ü = 0x9A, ß = 0xE1).

Umgesetzt (20.10.2026):
- `FontLoad("SYS", 14, stil)` lädt `SYS14.FNT` aus dem RD2-Verzeichnis. Ein Name mit Punkt ist ein Dateiname. Höhe 0 = Systemschrift.
- Jede Datei liegt nur einmal im Speicher, auch wenn mehrere Programme oder Stile sie nutzen.
- Systemschrift nach Bildschirm: CGA SYS8, Hercules SYS14, VGA SYS16 (`GetCaps(dc, CAPS_TEXTH)`). Ein DC ohne `SetFont` und `SetFont(dc, 0)` nehmen sie.
- `TextOut` (y = Oberkante), `TextWidth`, `TextBox` mit `TB_LEFT/CENTER/RIGHT`, `TB_WRAP` (an Leerzeichen, sonst mitten im Wort), `TB_VCENTER`, `\n` und `\r\n`; abgeschnitten am Rechteck.
- Deckend (`BK_OPAQUE`, Textfarbe auf Hintergrundfarbe) oder durchsichtig, auch mit `ROP_XOR`.
- Fett (+1 Pixel je Zeichen), kursiv (bis (Höhe-1)/4 Pixel geneigt), unterstrichen (Zeile unter der Grundlinie).
- Feste 8-Pixel-Schrift ohne Stil hat einen schnellen Weg: ein Byte je Zeichen und Zeile, Zeilen gesammelt in einem Treiberaufruf, innere Schleifen in Assembler.

## 8. Konfigurationsdateien

**`RD2.INI`** (Text, wie Windows-INI):

```ini
[System]            ; schreibt RD2SETUP (Erkennung), der Kern liest nur
Grafik=VGA          ; CGA, HGC, VGA, VGA16 (nur was die Karte kann; AUTO = automatisch)
Profil=SCHNELL      ; SCHNELL (1 Bit), KOMFORT (Farbe, ab 386)
Maus=DOS            ; DOS (INT 33h), KEINE; spaeter eigener Treiber MAUS.DRV
MausHW=BUS:23C      ; erkannte Maus: BUS:23C, INPORT:23C, SERIELL:COM1, PS2, DOS, KEINE
Sound=ADLIB         ; SPEAKER, ADLIB, AUS
XMS=256             ; KB XMS-Ablage fuer Module, 0 = aus
Netz=PAKET          ; PAKET (Paketreiber), KEIN
Sprache=DE
Systemklang=1
Drucker=EPSON9.DRV
Port=LPT1
[Shell]
Ordner1=A:\
Programme=PROGRAMM.LST
[Netz]
Start=0
DHCP=1
```

Die **`.APP`**-Datei meldet ein Programm an (eine je Programm, oder alle in `PROGRAMM.LST` als Abschnitte):

```ini
[Rechner]
Datei=RDCALC.EXE
Symbol=1            ; Ressource in der EXE
Gruppe=Zubehoer
Typen=CAL
Modus=RD2           ; RD2, RD2-32, DOS
Speicher=48         ; KB
Diskette=RD2 PROG   ; Datentraegername, falls nicht auf der Systemdiskette
```

Für DOS-Programme kommen dazu: `Parameter=`, `Ordner=`, `Grafik=1` (Programm schaltet selbst um), `Pause=1`.

**`RD2.STA`** (binär) enthält den Zustand von Shell und Fenstern. Kopf `RDS1` + `WORD version`, unbekannte Versionen werden ignoriert. Es wird nur beim Beenden und vor `RunDos` geschrieben, mit `FileSaveSafe`.

## 9. Disketten

- **Seriennummer**: Der Kern merkt sich für jedes geladene Programm, Modul und jede Ressource die Seriennummer des Datenträgers (INT 21h 6900h, DOS 4+). Muss er nachladen und die Diskette ist eine andere, ruft er `DiskRequest`: "Bitte Diskette RD2 SYSTEM in Laufwerk A: einlegen" mit OK/Abbruch. Bei Abbruch liefert die ladende Funktion `RD_ERR_NODISK`.
- **Phantomlaufwerk B:** auf Rechnern mit einem Laufwerk: Der Kern setzt das logische Laufwerk selbst (IOCTL 440Fh) und zeigt "Diskette für Laufwerk B: einlegen" als Dialog statt der DOS-Meldung.
- **INT 24h** wird immer als Dialog gezeigt (Wiederholen, Abbrechen), nie als DOS-Text.
- **Schreibschutz**: Ist die Systemdiskette geschützt, läuft RD2 ohne `RD2.STA`/`RD2.INI` speichern weiter. Beim ersten Versuch kommt einmal ein Hinweis.
- **Wechsel**: Die Shell fragt `DriveInfo` beim Aktivieren und alle 2 Sekunden ab, wenn ein Diskettenfenster offen ist (ohne Laufwerkszugriff, über die Wechselleitung bzw. INT 13h 16h). Programme erhalten `M_DISKCHANGE`.

## 10. Netz

- `NetStart` lädt `RDNET.MOD` (TCP/IP aus RetroDesk 1.x `mod/tcp.c`), sucht den Packet-Treiber und holt per DHCP eine Adresse. Das Ergebnis kommt als `M_NETSTATE`.
- Alle Netzfunktionen kehren sofort zurück. Ergebnisse kommen als Nachricht an das Fenster, das bei `NetOpen`/`NetResolve` angegeben wurde: `M_NETCONNECT`, `M_NETDATA` (Bytes bereit, abholen mit `NetRecv`), `M_NETCLOSED`, `M_NETACCEPT`.
- Der Stack arbeitet im Hintergrund, während die Programme `GetMessage` aufrufen. Ein Programm, das nicht abholt, bremst nur seine eigenen Verbindungen (TCP-Fenster).
- Höchstens 8 Verbindungen gleichzeitig in Modus R, Puffer je Verbindung 2 KB Empfang und 1 KB Senden .
- Stand 2.1: je Verbindung 4 KB Empfang und 2 KB Senden. `NetOpen(..., flags)`: `NO_TCP` (0), `NO_UDP` (1), `NO_GROSS` (2, 01.11.2026): TCP mit 8 KB Empfangsfenster für Datenkanäle (FTP-Laden), fällt bei Speichermangel auf 4 KB zurück. Das Fenster wird schon gemeldet, wenn ein halbes Segment frei ist.
- Fehlt der Paketreiber (`NetStart` liefert `RD_ERR_NOTFOUND`), lädt `RDNET.EXE L` ihn im laufenden Betrieb (fragt vorher; `LS` ohne Frage, z. B. Netzzeit beim Start) in den Platz, den der Kern dafür freihält (`[Netz] Platz=` in Paragraphen, schreibt RDNET beim Speichern). FTP und Uhr nutzen das und versuchen `NetStart` danach erneut.

## 11. 16 und 32 Bit

- Strukturen ohne Zeiger (`MSG`, `RECT`, `POINT`, `RDTIME`, `FINDDATA`, `DRIVEINFO`, `INSTR`, `NETINFO`) sind in beiden Modi gleich groß. Das prüft `test/hdrtest.c`.
- Strukturen mit Zeigern (`WNDCLASS`, `FILEDLG`, `PRNJOB`) werden beim Übergang 16 -> 32 Bit vom Kern umgesetzt.
- `int` kommt in der API nicht vor, nur `short`, `WORD`, `long` und `DWORD`.

## 12. Größengrenzen

Siehe `tools/budget.txt`. Ab Phase 1 prüft der Build jede Datei und rechnet jede Diskette mit 1-KB-Clustern aus.

**Verteilformat seit 24.10.2026: 360-KB-Disketten** (5,25" DD, 354 KB nutzbar; vorher 720 KB). Ein 720-KB- oder 1,44-MB-Laufwerk liest dieselben Dateien, wenn man sie auf eine größere Diskette kopiert.

| Diskette | Name | Inhalt | Grenze |
|---|---|---|---|
| 1 | `RD2SYSTEM` | bootfähig: FreeDOS-Kern (8086-Fassung, GPL) oder eigenes DOS per `SYS A:`, RDMAUS (eigener Maustreiber), RDKEYB, Kern, RDGDI/X, RDKX, RDUSER, RDCTL/X, RDSND, Grafiktreiber CGA/HGC/VGA, Lautsprecher, Schriften, Sprachen, RD2SETUP, SHELL (Desktop, Dateimanager), Start der Installation, `COPYING`, `LIZENZ.TXT` | höchstens 300 KB, damit ≥ 50 KB für `RD2.INI`, `RD2.STA` und Daten frei bleiben |
| 2 | `RD2 PROG` | Werkzeuge (RDFDISK, RDFORMAT, RDDISK.MOD), Zubehör (Uhr, Notizblock, Rechner), RDCALC, RDEDIT, weitere Treiber (AdLib, Drucker), `COPYING` | 354 KB |
| 3 | `RD2 NETZ` | RDNET, FTP, IRC, SNTP, weiteres | 354 KB |
| 4 | `RD2 EXTRA` | SDK (Kopfdateien, RD2.LIB, Werkzeuge), Quelltext der GPL-Teile, weiteres | 354 KB |

Treiber, die zum Starten nötig sind (Grafik, Lautsprecher), bleiben auf Diskette 1. Lizenzen: [LIZENZ.md](LIZENZ.md).

Für den Alltag reichen Diskette 1 und 2. Der Kern fragt beim Nachladen per Dialog nach der richtigen Diskette (Abschnitt 9). Die Tests booten von einem 360-KB-Abbild mit FreeDOS.

## 13. Freigegeben (19.10.2026)

1. Menüleiste global oben (Abschnitt 5)
2. Notbeenden mit Strg+Alt+Rückschritt (Strg+Alt+Entf bleibt der Neustart)
3. Systemmenü "RD" ganz links
4. Symbolgröße 32x32, auf CGA 32x16

## 14. Weg zum Betriebssystem (freigegeben 21.10.2026)

Ziel: RetroDesk 2 ist für den Benutzer ein eigenes Betriebssystem, das DOS-Programme ausführen kann. Es gibt keine eigene Nachbildung von INT 21h. DOS bleibt Unterbau, ist aber unsichtbar.

**Start**
1. `CONFIG.SYS` enthält `SHELL=A:\RD2\RD.EXE /P` (oder `C:\RD2\RD.EXE /P`), danach optional Schalter und das Startprogramm. Eine DOS-Eingabe und COMMAND.COM gibt es nicht. `/P` heißt wie bei COMMAND.COM "bleibt": RD lädt sofort den Rest (`RDSWAP.BIN`, Modus 2) und läuft als dessen Kind mit `/W:seg`. Endet RD, zeigt der Rest "RetroDesk ist beendet. Taste: neu starten" und startet RD wieder, die Shell endet also nie. `RunDos` im Diskettenbetrieb trägt das Programm direkt in diesen Rest ein. Ohne `/P` erkennt RD die Shell nur, wenn DOS den PSP als eigene Eltern einträgt (MS-DOS; der FreeDOS-Kern tut das nicht). Gemessen (QEMU, FreeDOS-Diskette, ohne COMMAND.COM): das DOS-Programm bekommt 550 KB, mit COMMAND.COM 481 KB. Unterbau ist MS-DOS ab 5 oder der FreeDOS-Kern (GPL), mit dem die Bootdiskette frei weitergegeben werden darf.
2. Fehlt `RD2.INI` (erster Start) oder steht `RD /SETUP` im Aufruf, ruft der Kern zuerst `RD2SETUP.EXE /ERST` auf. Nur RD2SETUP erkennt Hardware (Grafik, Maus, Klang, XMS, hoher Speicher, Netz), der Kern tastet beim Start nichts ab.
3. Installation auf Festplatte (Variante B, 29.10.2026): RD2SETUP erkennt nur und speichert; „I“ schreibt `Installieren=1` in RD2.INI, danach startet die Shell `INSTALL.EXE` (grafisch: Ziel, Fortschrittsbalken über alle Disketten, Diskettenwechsel per Meldung, CONFIG.SYS-Fragen als Ja/Nein-Dialog, zuletzt RDSYS per RunDos). „T“ ist die Text-Installation als Notweg mit denselben Regeln. RD2SETUP (Text) installiert auch auf Festplatte: Dateien kopieren (Zielordner Ebene für Ebene), `RD2.INI` anlegen, `SHELL=…\RD.EXE /P` in `CONFIG.SYS` (24.10.2026). Neues System ohne CONFIG.SYS und AUTOEXEC.BAT: keine Rückfrage, nur `SHELL=` und Maustreiber. Bestehendes DOS: Rückfrage, Sicherung `CONFIG.RDA`, alte `SHELL=` als `REM`, jede Programmzeile der AUTOEXEC.BAT wird einzeln als `INSTALL=` angeboten (SET und PATH gelten ohne COMMAND.COM nicht). Maus: eigener Treiber `RDMAUS.COM` (Diskette 1) für Bus-, InPort-, PS/2- und serielle Mäuse, mit dem erkannten Schalter (`/B`, `/I1`, `/I2`, `/P`, `/S1`–`/S4`). Ein eigener `MAUS.DRV` folgt später.
4. Festplatte startfähig (Variante A, 24.10.2026): Hat die Zielplatte kein System (kein KERNEL.SYS, IO.SYS, IBMBIO.COM) und liegt KERNEL.SYS auf der Startdiskette, fragt RD2SETUP „Festplatte startfähig machen (FreeDOS-Kern)?“. Bei J ruft es `RDSYS.EXE A:\ C: /UPDATE` auf (FreeDOS SYS, GPL v2, unverändert): KERNEL.SYS und FAT16-Bootsektor, keine COMMAND.COM. Die Shell-Zeile lautet `SHELL=C:\RD2\RD.EXE /P C:\RD2\SHELL.EXE`. Test: `test/kette.sh` (leere Platte bis Desktop von Festplatte).

**DOS-Programme (`RunDos`)**
- RD2 schreibt `RD2.STA` und lagert sich aus: mit XMS in den XMS-Speicher, sonst in eine Datei auf der Festplatte. Im Speicher bleibt nur ein Rest von 2 bis 3 KB, der das Programm startet und RD2 danach zurückholt. Alle Fenster sind danach wie vorher.
- Ohne XMS und ohne Festplatte (reiner Diskettenbetrieb) beendet sich RD2 ganz und startet nach dem Programm mit `RD2.STA` neu.
- Umsetzung (22.10.2026): Der Rest liegt als `RDSWAP.BIN` (1,2 KB) neben `RD.EXE` und kostet so keinen Kernspeicher. Er läuft in einem Block von 1,5 KB ganz oben, mit eigenem Stack. Ausgelagert wird der Kern ab dem PSP und die Arena bis zum Ende ihres belegten Teils. Mit XMS in einen XMS-Block, sonst in `RD2SWAP.$$$` im RD2-Verzeichnis (nur auf Festplatte, die Datei wird danach gelöscht). Danach muss der Speicher wieder an derselben Adresse frei sein. Hat das DOS-Programm sich resident gemacht, meldet der Rest das und startet den Rechner neu.
- `RunDos` liefert den Rückgabecode des Programms (0 bis 255) oder `0xFFFF` mit `LastError`: `RD_ERR_NOTFOUND` (Programm oder `RDSWAP.BIN` fehlt), `RD_ERR_NOMEM`, `RD_ERR_NOTSUP` (mit `RD_DOS_KEEP`, aber kein XMS und keine Festplatte). `RD_DOS_PAUSE` wartet nach dem Programm auf eine Taste. Den Bildschirm stellt RDGDI zurück, den Inhalt zeichnen die Programme neu (später schickt RDUSER die Neuzeichnen-Nachricht).
- **Neustart (Diskettenbetrieb, ohne `RD_DOS_KEEP`):** Alle RD2-Programme enden, RD gibt seinen Speicher frei und beendet sich bis auf PSP und Rest. Der Rest startet das DOS-Programm und danach `RD.EXE /W:seg` mit der alten Befehlszeile. Ruft dieses RD wieder `RunDos`, trägt es Programm und Befehlszeile im Rest ein und endet mit Code `0D5h`; der Rest startet dann das nächste Programm. So wächst der Speicherbedarf auch bei vielen Wechseln nicht. Den Zustand stellen die Programme selbst wieder her (später über `RD2.STA`). Endet RD normal, endet auch der Rest mit dessen Code.
- Gemessen: DOSBox (Hercules, 8086, ohne XMS, Auslagerdatei) 609 KB für das DOS-Programm. QEMU-Diskette mit FreeDOS (Neustart): 481 KB, ohne RD2 an der DOS-Eingabe 483 KB.
- Das DOS-Programm bekommt fast die vollen 640 KB und alle Hardware. Grafik, Maus und Zeitgeber stellt RD2 vorher auf den DOS-Zustand zurück.

**Später**
- Modus P (386): DOS-Programme parallel in eigenen Fenstern (V86-Modus).
- Optional eine reine RD2-Bootdiskette ohne DOS (eigener Bootsektor, eigenes FAT12/16 über INT 13h) für den größten freien Speicher. Darauf laufen nur RD2-Programme.

**Regel dafür:** RD2-Programme rufen DOS nie direkt auf (Abschnitt 1, Punkt 6, und 2.5). Dateien, Speicher und Programmstart gehen nur über die RD2-Funktionen. Deshalb laufen sie unverändert auch ohne DOS.
