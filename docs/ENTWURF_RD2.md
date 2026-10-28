# RetroDesk 2: Entwurf einer grafischen Umgebung für 8088 bis Pentium

Stand: 19.10.2026, Entwurf (noch kein Code)

## 1. Ziel

Eine grafische Oberfläche im Stil von Windows 3, die

- auf einem **PC XT (8088, 4,77 MHz, 640 KB, CGA/Hercules)** läuft,
- auf einem **Pentium mit 32 MB (VGA/SVGA, Sound Blaster)** den ganzen Rechner nutzt,
- eigene **Programme für die Oberfläche** (Fenster, Menüs, Grafik, Klang) ausführt, auch mehrere gleichzeitig,
- **normale DOS-Programme** weiterhin startet,
- Programmen eine **API für Fenster, Grafik, Sound, Dateien, Drucker und Netz** bereitstellt.

DOS bleibt darunter für Dateisystem, Laufwerke und Programmstart. Das war bei Windows 1 bis 3, GEM und GEOS genauso.

## 2. Geht das? Kurzantwort

Ja, mit einer klaren Grenze:

| Programmart | 8088, 640 KB | 386/Pentium, 32 MB |
|---|---|---|
| RD2-Programme (für die API geschrieben, 16 Bit) | ja, mehrere gleichzeitig im Fenster | ja |
| RD2-Programme (32 Bit, viel Speicher) | nein | ja |
| DOS-Textprogramme | Vollbild, GUI pausiert | Vollbild, später auch im Fenster (V86) |
| DOS-Spiele/Grafikprogramme | Vollbild, GUI pausiert | Vollbild, GUI pausiert |

"Alle Programme im Fenster" geht auf dem 8088 technisch nicht: Ein DOS-Programm schreibt direkt in den Bildspeicher und auf die Hardware, der 8088 kann das nicht abfangen. Windows 3 im Real Mode konnte das auch nicht. Erst ab dem 386 (V86-Modus) lassen sich DOS-Programme einsperren und in ein Fenster umleiten. Das ist eine eigene, große Ausbaustufe (Abschnitt 11).

## 3. Betriebsarten (wie Windows 3.0 Real/Standard/Enhanced)

Ein Startprogramm `RD.EXE` prüft CPU und Speicher und wählt:

| Modus | CPU | Speicher | Kern | Programme |
|---|---|---|---|---|
| **R** (Real) | 8088/8086/V20 | 640 KB, optional EMS 4.0 | 16 Bit, Real Mode | 16 Bit |
| **P** (Protected) | 386, 486, Pentium | bis 4 GB, praktisch 32 MB+ | 32 Bit, DPMI/DOS-Extender | 16 und 32 Bit |

- Auf einem 286 läuft Modus R (ggf. mit XMS als Auslagerungsspeicher).
- Mit `RD /R` lässt sich auf dem Pentium Modus R erzwingen (Test, Kompatibilität).
- 16-Bit-Programme laufen in beiden Modi **unverändert**. Im Modus P bekommen sie über einen Umschalter (Thunk) Zugriff auf den 32-Bit-Kern, wie Win16-Programme unter Windows 3.1 Enhanced Mode.
- 32-Bit-Programme laufen nur in Modus P (wie Win32s).

## 4. Schichten

```
+---------------------------------------------------------------+
|  Programme:  Shell (Desktop, Dateimanager, Programmstarter),  |
|              RDEDIT, RDCALC, FTP, IRC, Spiele, Fremdprogramme |
+---------------------------------------------------------------+
|  RDUSER   Fenster, Menüs, Dialoge, Steuerelemente, Nachrichten|
|  RDGDI    Zeichnen: Gerätekontext, Clipping, Bitmaps, Schrift |
|  RDSND    Töne, FM-Instrumente, Samples, Musik                |
|  RDNET    TCP/IP (heutiger Stack aus mod/tcp.c)               |
+---------------------------------------------------------------+
|  RDKERN   Speicher, Module laden, Tasks, Zeitgeber, Dateien,  |
|           Zwischenablage, DOS-Programmstart, Fehler           |
+---------------------------------------------------------------+
|  Treiber (.DRV): Grafik, Maus, Tastatur, Sound, Drucker, Netz |
+---------------------------------------------------------------+
|  DOS 5+  /  BIOS  /  Hardware                                 |
+---------------------------------------------------------------+
```

Jede Schicht ist ein ladbares Modul. Fehlt ein Treiber, gibt es einen Rückfall (z. B. PC-Speaker statt AdLib, 8x8-ROM-Schrift statt RDFONT).

## 5. Kern (RDKERN)

### 5.1 Speicherverwaltung

Das ist der entscheidende Teil, damit mehrere Programme in 640 KB passen. Vorbild ist der Global Heap von Windows 1 bis 3:

- Speicher wird über **Handles** verteilt, nicht über feste Zeiger: `h = MemAlloc(Größe, Flags)`, `p = MemLock(h)`, `MemUnlock(h)`.
- Nicht gesperrte Blöcke darf der Kern **verschieben** (Lücken schließen).
- **Verwerfbare** Blöcke (Code, Schriften, Ressourcen) darf der Kern bei Platzmangel freigeben und später von der Platte nachladen.
- **Auslagern**: Mit EMS (Modus R) oder XMS (286) werden ruhende Programme und Bitmaps dorthin verschoben. Ohne EMS geht auf Wunsch auch eine Auslagerungsdatei, auf dem XT mit Festplatte aber langsam.
- Im Modus P: ein flacher 32-Bit-Heap über DPMI, dieselben Funktionen, Handles bleiben gültig (16-Bit-Programme bekommen Selektoren).

Flags: `MEM_FIXED`, `MEM_MOVABLE`, `MEM_DISCARDABLE`, `MEM_ZEROINIT`, `MEM_SHARE` (Zwischenablage, Nachrichten zwischen Programmen).

### 5.2 Programme laden

- Ein RD2-Programm ist eine **normale DOS-MZ-EXE** (Watcom, Speichermodell Medium oder Large) mit eigenem Startcode `RD2START.OBJ` und einer Kennung im Kopf (`RD2` + Version + Speicherbedarf + Name).
- Der Kern lädt sie mit **INT 21h AX=4B03h** (Overlay laden, DOS setzt die Relokationen) in einen eigenen Speicherblock. DOS legt dafür keinen neuen Prozess an, der Kern behält die Kontrolle.
- Ressourcen (Menüs, Dialoge, Symbole, Texte) hängen als Block am Ende der EXE und werden erst bei Bedarf geladen.
- Startet man ein RD2-Programm direkt unter DOS, meldet es "Benötigt RetroDesk 2" (wie "This program requires Microsoft Windows").
- 32-Bit-Programme: LE/LX- oder PE-Format, geladen vom 32-Bit-Kern (Watcom erzeugt beides).

### 5.3 Tasks und Nachrichten

- **Multitasking** (entschieden 19.10.2026): Jedes Programm hat einen eigenen Stack und holt sich mit `GetMessage()` die nächste Nachricht. Ist keine da, wechselt der Kern zum nächsten Programm. Zusätzlich schaltet der Kern alle 55 ms um, wenn das Programm gerade in seinem eigenen Code ist (nicht in DOS, BIOS oder RD2-Kern). So hält ein rechnendes Programm die anderen nicht an.
- Auf dem 8088 die einzige sinnvolle Lösung: kein Schutz nötig, wenig Verwaltung, schnell.
- Ein Programm, das nie `GetMessage()` aufruft, hält alles an. Abhilfe: Strg+Alt+Entf-Ersatz (Tastenkombination im Tastaturtreiber), der das hängende Programm beendet.
- Im Modus P später optional **präemptiv** für 32-Bit-Programme (Zeitscheiben über den Zeitgeber), 16-Bit-Programme bleiben kooperativ.

Nachrichten (Auswahl):

| Nachricht | Bedeutung |
|---|---|
| `M_CREATE`, `M_CLOSE`, `M_DESTROY` | Fenster entsteht, soll schließen, wird zerstört |
| `M_PAINT` | Bereich neu zeichnen (ungültiges Rechteck) |
| `M_KEY`, `M_CHAR` | Taste gedrückt, Zeichen |
| `M_MOUSEMOVE`, `M_BUTTON` | Maus |
| `M_COMMAND` | Menüpunkt oder Knopf gewählt |
| `M_TIMER` | Zeitgeber abgelaufen |
| `M_SIZE`, `M_MOVE`, `M_ACTIVATE` | Fenster geändert |
| `M_DROPFILES` | Datei aus dem Dateimanager hineingezogen |
| `M_SOUNDDONE`, `M_NETDATA` | Klang fertig, Netzdaten angekommen |
| `M_QUIT` | Programm soll enden |

### 5.4 Weitere Kerndienste

- **Zeitgeber**: INT 8 auf 18,2 Hz bleibt für DOS, intern feiner (z. B. 72,8 Hz für Musik und Maus), `TimerSet(fenster, id, ms)`.
- **Dateien**: dünne Hülle um DOS (`FileOpen`, `FileRead`, ...), Kritische-Fehler-Behandlung (INT 24h) als Dialog statt "Abbrechen, Wiederholen, Ignorieren". Das gibt es heute schon in RDESK.
- **Zwischenablage**: Text und Bitmap, als `MEM_SHARE`-Block.
- **Profil-Dateien**: `RD2.INI`, `ProfileGet/ProfileSet` (wie GetPrivateProfileString).
- **Fehler**: ein abgestürztes Programm (ungültiger Handle, Stack-Überlauf) wird mit Meldung beendet, soweit der 8088 das erkennen kann.

## 6. Grafik (RDGDI)

### 6.1 Gerätekontext

Programme zeichnen nie direkt in den Bildspeicher, sondern über einen **Gerätekontext (DC)**:

```c
dc = BeginPaint(fenster, &ps);
SetPen(dc, FARBE_SCHWARZ);
Line(dc, 10, 10, 200, 80);
FillRect(dc, &r, MUSTER_GRAU50);
TextOut(dc, 20, 30, "Hallo XT");
EndPaint(fenster, &ps);
```

Der DC kennt Ursprung, Clipping (die sichtbaren Teile des Fensters), Stift, Füllmuster, Schrift und Zeichenmodus (Setzen, ODER, XOR). Dieselben Aufrufe gehen auf Bildschirm, Bitmap im Speicher und Drucker.

### 6.2 Funktionen

- Punkt, Linie, Rechteck, Rahmen, gefülltes Rechteck, Ellipse, Polygon, Fläche füllen
- Text mit Schriften (Bitmap-Schriften 8x8, 8x14, 8x16, proportional später), Textbreite messen
- Bitmaps: anlegen, darauf zeichnen, auf den Schirm kopieren (`BitBlt` mit Rasteroperation), skalieren (einfach)
- Symbole und Mauszeiger (Maske + Bild)
- Farben: Programme fragen `GetCaps(dc)` nach Auflösung und Farbanzahl. Farben werden logisch angegeben (`RGB(r,g,b)`), der Treiber wählt die nächste Farbe oder rastert sie (Dithering) auf Monochrom-Schirmen.

### 6.3 Neu zeichnen ohne Bildspeicher-Kopie

Auf dem 8088 gibt es keinen Platz für eine Kopie jedes Fensters (VGA mono: 38 KB pro Schirm). Deshalb wie bei Windows 3:

- Wird ein Fenster aufgedeckt, bekommt es `M_PAINT` mit dem ungültigen Bereich und zeichnet ihn selbst neu.
- Kleine Popups (Menüs, Tooltips, Meldungen) sichern den Hintergrund kurz und stellen ihn wieder her. Das macht RDESK heute schon mit `ui_save`/`ui_restore`.
- Im Modus P mit genug Speicher: optional Kopie pro Fenster, dann weniger Neuzeichnen.

### 6.4 Grafiktreiber

| Treiber | Auflösung | Farben | Für |
|---|---|---|---|
| `CGA.DRV` | 640x200 | 2 | XT (heutiger Code) |
| `HGC.DRV` | 720x348 | 2 | XT mit Hercules (heutiger Code) |
| `EGA.DRV` | 640x350 | 16 | AT, 286 |
| `VGAMONO.DRV` | 640x480 | 2 | heutiger VGA-Modus, schnell |
| `VGA.DRV` | 640x480 | 16 | 286/386 |
| `VESA.DRV` | 800x600, 1024x768 | 256, später 65536 | Pentium (VBE 1.2/2.0, Linear Framebuffer im Modus P) |

Treiber-Schnittstelle: Tabelle mit ca. 20 Grundfunktionen (Zeile füllen, Punkt, Linie waagerecht/senkrecht, Blit, Text-Zeichen, Zeiger). Alles andere baut RDGDI darauf auf. Ein neuer Treiber muss also nur die Grundfunktionen liefern.

## 7. Sound (RDSND)

Eine API für alle Karten, der Treiber entscheidet, wie es klingt:

- **Töne**: `SndBeep(frequenz, dauer)`, Systemklänge per Name (`SndSystem("START")`), wie heute
- **Instrumente/Noten**: `SndNote(kanal, instrument, note, lautstärke)`: AdLib/OPL2 (9 Kanäle), OPL3 (18), auf dem PC-Speaker einstimmig
- **Musik**: Player für IMF/DRO, später MIDI und RAD, läuft im Zeitgeber-Interrupt im Hintergrund, Nachricht `M_SOUNDDONE` am Ende
- **Samples (WAV)**: Sound Blaster per DMA. Auf dem PC-Speaker nur ab 286/386, auf dem 8088 zu langsam (bzw. nur Vollbild, wie RDAUDIO-Plan)
- **Mischen**: mehrere Programme dürfen Systemklänge spielen, Musik und Samples bekommt immer nur ein Programm (Sound-Fokus).

| Treiber | Hardware |
|---|---|
| `SPEAKER.DRV` | PC-Speaker (Töne, einfache Musik) |
| `ADLIB.DRV` | OPL2 auf 388h (heutiger Code) |
| `SB.DRV` | Sound Blaster 1.x bis 16: OPL2/OPL3 und Samples |
| `GUS.DRV`, `MPU401.DRV` | später |

## 8. Benutzeroberfläche (RDUSER)

- **Fenster**: Titelleiste, Schließfeld, Größe ändern, verschieben, Rollbalken, minimieren zum Symbol auf dem Desktop (wie GEM/Win 3.0)
- **Fensterklassen**: Programme registrieren eine Klasse mit Fensterfunktion (`RegisterClass`, `CreateWindow`), alles Weitere läuft über Nachrichten
- **Steuerelemente**: Knopf, Häkchen, Optionsfeld, Eingabefeld (einzeilig und mehrzeilig), Liste, Klappliste, Rollbalken, Fortschrittsbalken
- **Menüs**: Menüleiste pro Programm oder globale Leiste oben (GEM-Stil wie heute; Entscheidung siehe Abschnitt 14)
- **Dialoge**: aus Ressourcen beschrieben, Standarddialoge für Datei öffnen/speichern, Meldung, Ja/Nein, Farbwahl, Drucken
- **Tastatur**: Alt+Buchstabe für Menüs, Tab durch Steuerelemente, Alt+Tab zwischen Programmen
- **Aussehen**: wie RetroDesk heute, 1-Bit-tauglich; auf Farbschirmen dieselbe Form mit Farbschema

## 9. Shell

Die Shell ist ein ganz normales RD2-Programm und damit austauschbar:

- **Desktop** mit Laufwerkssymbolen, Programmsymbolen, Papierkorb (optional)
- **Dateimanager** (heutiger, mit mehreren Fenstern, Ziehen und Ablegen)
- **Programmstarter** aus `RD2.INI` und den angemeldeten Programmen
- **Programmanmeldung**: Jedes Programm liegt mit einer `.APP`-Datei vor:

```ini
[App]
Name=Rechner
Datei=RDCALC.EXE
Symbol=RDCALC.EXE,1
Menue=Zubehoer
Typen=CAL
Modus=RD2          ; RD2, RD2-32 oder DOS
Speicher=48        ; KB, Hinweis für den Kern
```

  `Modus=DOS` bedeutet ein klassisches DOS-Programm, `Speicher=` dann den Mindest-RAM. Eine Zeile `Grafik=1` sagt, dass das Programm selbst den Grafikmodus umschaltet.
- **Zubehör** (heute Uhr, Notizblock, Rechner, Shell, FTP, IRC) wird zu kleinen RD2-Programmen und kann gleichzeitig offen sein.

## 10. DOS-Programme starten

### Modus R (8088)

1. Shell ruft `RunDos("C:\\SPIELE\\PRINCE.EXE")`.
2. Der Kern speichert Desktop-Zustand und alle Programme nach EMS oder in eine Datei (`RD2SWAP.TMP`), gibt den Speicher frei, behält nur einen **Rest-Stub von ca. 4 bis 6 KB** (heute macht `RDGO.COM` Ähnliches).
3. Textmodus, DOS-Programm läuft mit fast dem ganzen Speicher.
4. Danach lädt der Stub den Kern zurück, Programme machen dort weiter, wo sie waren.

Alternative ohne Zustandssicherung bleibt die heutige BAT-Schleife (schnell, alle RD2-Programme werden dabei beendet).

### Modus P (386+)

- Zuerst wie oben (Vollbild).
- Ausbaustufe: DOS-Textprogramme in einer **V86-Box im Fenster**. Der Kern fängt Zugriffe auf B800h/B000h ab und zeichnet sie ins Fenster, Tastatur und Zeitgeber werden virtualisiert. Grafikprogramme bleiben Vollbild (mit Alt+Enter zurück). Das ist sehr aufwendig, deshalb als spätere Stufe.

## 11. Zwei Kerne, eine API

| | Modus R | Modus P |
|---|---|---|
| Compiler | Open Watcom 16 Bit, `-mm`/`-ml` | Open Watcom 32 Bit, `-mf` |
| Speicher | Global Heap, EMS | DPMI, flach |
| Extender | keiner | DOS/32 Advanced (frei, quelloffen) oder eigener kleiner Extender |
| Grafik | CGA, HGC, VGA mono, EGA | dazu VGA 16, VESA 256 mit Linear Framebuffer |
| Tasks | nachrichtengesteuert + präemptiv außerhalb von DOS/Kern | ebenso, 32 Bit voll präemptiv möglich |
| DOS-Programme | Vollbild mit Auslagern | Vollbild, später V86 im Fenster |

- Die Quellen von RDUSER, RDGDI (ohne Treiber) und RDSND sind **dieselben C-Dateien**, einmal 16 Bit, einmal 32 Bit gebaut. Nur Kern, Speicher und Treiber unterscheiden sich.
- **16-Bit-Programme** rufen die API über eine Tabelle von Far-Zeigern auf, die der Kern beim Start übergibt. Im Modus P zeigt die Tabelle auf Übergänge (Thunks) in den 32-Bit-Kern.
- **API-Aufrufkonvention**: `far pascal` (Parameter fest, Aufrufer räumt nicht auf; kurz und schnell auf dem 8088, aus Turbo Pascal, Assembler und C gleich gut nutzbar).
- **Andere Sprachen**: Zusätzlich gibt es einen Software-Interrupt `INT 7Eh` mit Funktionsnummer in AX, für Assembler, BASIC oder Compiler, die keine Tabelle laden können.

## 12. Speicherbudget auf dem XT (Schätzung)

| Teil | KB |
|---|---|
| DOS 5, Maustreiber, Packet-Treiber | ca. 80 |
| RDKERN | 12 |
| RDUSER | 24 |
| RDGDI + Grafiktreiber + Schrift | 18 |
| RDSND + AdLib-Treiber | 5 |
| Shell (Desktop, Dateimanager) | 35 (verwerfbar zum Teil) |
| **Summe Oberfläche** | **ca. 95** |
| **Frei für Programme** | **ca. 460** |

Die Zahlen sind geschätzt nach heutigem RetroDesk (RDESK.EXE 64 KB mit Dateimanager, Menüs, Uhr, Notizblock). RDNET (TCP/IP, ca. 16 KB) wird nur geladen, wenn ein Programm es braucht. Mit EMS passen deutlich mehr ruhende Programme.

Geschwindigkeit: Neuzeichnen eines ganzen Fensters auf CGA ca. 50 bis 150 ms auf 4,77 MHz (heutige Messung mit RDPERF als Grundlage). Das reicht für Büroprogramme, nicht für Animation in mehreren Fenstern.

## 13. SDK

Das SDK enthält:

- `RD2.H` (alle Funktionen, Nachrichten, Konstanten), `RD2.LIB` (Importtabelle), `RD2START.OBJ`
- `RD2.PAS` für Turbo Pascal 5.5/7, `RD2.INC` für Assembler (später)
- Ressourcen-Compiler `RDRC` (Textdatei mit Menüs/Dialogen -> Binärblock an der EXE)
- Beispielprogramme: Hallo, Uhr, Malprogramm, Klangtest
- Handbuch (deutsch und englisch)

Beispiel:

```c
#include "rd2.h"

static long far pascal HalloProc(HWND w, UINT msg, UINT p1, long p2)
{
    PAINT ps; HDC dc;
    switch (msg) {
    case M_PAINT:
        dc = BeginPaint(w, &ps);
        TextOut(dc, 16, 16, "Hallo von RetroDesk 2");
        EndPaint(w, &ps);
        return 0;
    case M_COMMAND:
        if (p1 == ID_KLANG) SndSystem("DING");
        return 0;
    case M_DESTROY:
        PostQuit(0);
        return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}

int far pascal RdMain(HINST inst, char far *cmd)
{
    MSG m;
    RegisterClass("Hallo", HalloProc);
    CreateWindow("Hallo", "Hallo", 40, 40, 320, 120, MenuLoad(inst, 1));
    while (GetMessage(&m)) DispatchMessage(&m);
    return m.p1;
}
```

Gebaut wird mit `wcl -0 -mm hallo.c rd2.lib rd2start.obj` (XT) bzw. `wcl386 hallo.c rd2_32.lib` (32 Bit).

## 14. Offene Entscheidungen

1. **Menüleiste**: entschieden (19.10.2026): **global oben**, Systemmenü "RD" ganz links.
2. **Mindest-DOS**: entschieden (19.10.2026): **DOS 5**, wie RetroDesk 1.x.
3. **Farben im Modus R**: nur Monochrom (CGA, HGC, VGA mono) oder auch EGA/VGA 16 Farben auf dem 286?
4. **Extender für Modus P**: DOS/32A übernehmen oder eigenen schreiben (passt zur Linie "alles eigen", ist aber mehr Arbeit)?
5. **Name**: "RetroDesk 2" oder eigener Name für die Umgebung?
6. **Diskettenbetrieb**: entschieden (19.10.2026): RD2 läuft ohne Festplatte von 720-KB-Disketten, Details und Umsetzung in [PLAN_RD2.md](PLAN_RD2.md).
7. **Verhältnis zu RetroDesk 1.x**: 1.x bleibt die schlanke Shell für Disketten-XT (ohne Multitasking), RD2 ist der neue Weg.

## 15. Fahrplan

| Stufe | Inhalt | Läuft auf | Aufwand |
|---|---|---|---|
| 0 | Dieser Entwurf, API-Liste (`RD2.H`) festlegen | | klein |
| 1 | RDKERN Modus R: Heap mit Handles, Laden über 4B03h, kooperative Tasks, Nachrichten | 8088 | mittel |
| 2 | RDGDI + Treiber CGA/HGC/VGA mono aus dem heutigen `gfx.c`, Clipping mit Rechtecklisten | 8088 | mittel |
| 3 | RDUSER: Fenster, Menüs, Dialoge aus heutigem `rdui.c`/`rdmenu.c` | 8088 | groß |
| 4 | Shell: heutiger Dateimanager und Desktop als RD2-Programm, `.APP`-Anmeldung | 8088 | mittel |
| 5 | Portierung RDCALC, RDEDIT, Uhr, Notizblock, FTP, IRC | 8088 | mittel |
| 6 | RDSND mit Speaker/AdLib/SB, Musik im Hintergrund | 8088 | mittel |
| 7 | DOS-Programme mit Auslagern (EMS/Datei), Rest-Stub | 8088 | mittel |
| 8 | SDK, Handbuch, Beispielprogramme, Version 2.0 | 8088 | mittel |
| 9 | Modus P: 32-Bit-Kern mit DPMI, Thunks für 16-Bit-Programme | 386+ | groß |
| 10 | Treiber VGA 16 Farben, VESA 256, Sound Blaster 16 | 386+ | mittel |
| 11 | 32-Bit-Programme, präemptive Tasks | 386+ | groß |
| 12 | DOS-Textprogramme im Fenster (V86) | 386+ | sehr groß |

Nach Stufe 8 ist die Umgebung auf dem XT vollständig nutzbar und läuft unverändert auf dem Pentium (als Modus R). Die Stufen 9 bis 12 holen danach die 32 MB heraus.

Getestet wird wie heute: DOSBox-X-Automatisierung mit Bildspeicher-Prüfung, zusätzlich 86Box/PCem für XT und Pentium und echte Hardware (PC10-III).

## 16. Risiken

- **Geschwindigkeit auf 4,77 MHz**: Mehrere Fenster mit viel Neuzeichnen werden träge. Gegenmittel: sparsames Neuzeichnen, schnelle Zeilenroutinen in Assembler (wie `gfxfast.c`), kleine Fenster.
- **Speicher**: Ohne EMS sind auf dem XT 2 bis 4 gleichzeitige Programme realistisch, nicht 10.
- **Multitasking ohne Speicherschutz**: Ein fehlerhaftes Programm kann andere überschreiben (wie bei jedem 8088-System). Rechenschleifen blockieren dank Zeitscheiben nicht mehr.
- **Aufwand**: Windows 3.0 war jahrelange Teamarbeit. Hier hilft, dass Grafik, Maus, Fenster, Menüs, Sound, Netz und Tests aus RetroDesk 1.x schon da sind. Trotzdem ist RD2 ein großes Projekt; der Fahrplan ist so geschnitten, dass jede Stufe für sich läuft.
- **DOS-Programme im Fenster (Stufe 12)**: technisch heikel (Hardware-Virtualisierung). Kann auch entfallen, ohne den Rest zu entwerten.
