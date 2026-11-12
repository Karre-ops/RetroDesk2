# Plan: 32-Bit-Programme, Vollbild, 3D und Programm-Setup

Stand 12.11.2026, Entscheidungen von Karsten:
- 3D-Karten: 3dfx Voodoo 1 bis 4, ATI Rage und neuer, nVidia Riva 128 und neuer.
- Vollbild-Programme sind per Taskwechsel unterbrechbar (nicht exklusiv).
- RD2/32-Programme laufen im Fenster und im Vollbild.

## Grundsatz
- Kleinster gemeinsamer Nenner bleibt der 8088. Der Kern bleibt 16 Bit und unter 44 KB.
- Alles Neue kommt in Module, die nur auf passender Hardware geladen werden
  (RDVOLL.MOD, RD32.MOD, RD3D.MOD und 3D-Treiber *.3DT).
- Jedes Programm erklärt seinen Bedarf. RD2 prüft ihn vor dem Start, mit oder ohne Installation.

## Schritt 1: Bedarf und Setup (alle CPUs)
- Ressource `BEDARF` in der EXE: CPU (8088/286/386/486/Pentium), FPU, Grafik (Modus, Farben),
  RAM, XMS, Klang, Netz, 3D (keins / Software / Beschleuniger).
- Die Shell prüft vor dem Start und meldet klar, was fehlt. Im Menü "RD2-Programme" steht das Programm grau.
- `SysInfo` fürs SDK: CPU, FPU, Grafik, Speicher, Klang und neu die 3D-Erkennung per PCI-Suche
  (Hersteller 121Ah 3dfx, 1002h ATI, 10DEh nVidia) mit Karte und Speicher. Ergebnis in RD2.INI,
  wie die übrige Hardware-Erkennung nur einmal und nachträglich über RD2SETUP erneut.
- RDSETUP mit `SETUP.INF`: Dateien, Zielordner, Menüeintrag, Symbol, Bedarf, Varianten
  (16/32 Bit, CGA/VGA, 3D an/aus), Deinstallationsliste. Eigenes Setup-Programm eines Pakets möglich.

## Schritt 2: Vollbild, unterbrechbar (alle CPUs)
- API `ScreenAcquire(modus)` / `ScreenRelease`: eigener Modus (CGA, Mode 13h, Mode X, VESA mit LFB),
  Zeiger auf den Bildspeicher, Tasten roh, Klang/Timer/Netz weiter über RD2.
- Taskwechsel (Alt+Tab bzw. feste Taste in RDKEYB, auch im Rohmodus): RD2 schickt `M_SCREENLOST`,
  merkt Modus und Palette und schaltet auf den Schreibtisch. Zurück kommt `M_SCREENBACK`: RD2 stellt
  Modus und Palette her, das Programm zeichnet sein Bild aus eigenen Daten neu.
- Kein Abbild im RAM (spart Speicher). Ein Programm darf optional einen eigenen Puffer zum Zurückkopieren halten.
- Hinter dem Schreibtisch läuft das Vollbild-Programm angehalten weiter (keine Zeitscheiben, Klang stumm),
  damit Spiele nicht ohne Bild weiterlaufen.

## Schritt 3: 32-Bit-DOS-Programme (ab 386)
- DOS/4GW, DJGPP usw. laufen über RunDos (RD2 lagert sich aus). Testen, freien DPMI-Host beilegen
  (HDPMI32 oder CWSDPMI), RDKEYB/RDMAUS prüfen. Teil des Testsatzes aus PLAN_DOS Schritt 4.

## Schritt 4: RD2/32-Programme (ab 386), Fenster und Vollbild
- Neuer Programmtyp RD2/32 (Kennung im RD2-Block), gebaut mit Open Watcom `wcc386`, flaches Modell.
- RD32.MOD (nur ab 386): DPMI-Host einbinden, 32-Bit-Speicher aus XMS, die 640 KB bleiben frei.
- Brücken: 32-Bit-Aufrufe in die RD2-API (Wechsel in den 16-Bit-Kern) und Rückrufe des Kerns in den
  32-Bit-Code (Fensterprozeduren, Dialoge, Timer). Daten über einen gemeinsamen Puffer unter 1 MB.
- Fenster: alle Fenster-, Zeichen- und Dialogfunktionen über die Brücken. Für schnelle Grafik im Fenster
  ein Puffer-Bitmap, das das Programm direkt beschreibt und mit einem Aufruf ins Fenster kopiert.
- Vollbild: wie Schritt 2, zusätzlich direkter Zugriff auf den LFB von VESA 2.0.
- Multitasking: 32-Bit-Tasks werden zunächst nur an API-Aufrufen gewechselt (kooperativ), präemptiv
  erst, wenn die Umschaltung im Protected Mode sicher läuft.
- Fat-EXE: 16-Bit-Vorspann meldet auf 8088/286 sauber "braucht 386".
- Ausbau in zwei Stufen: 4a Vollbild und Grundfunktionen, 4b volle Fenster-API. Beide sind eingeplant.

## Schritt 5: RD3D (ab 386, braucht 2 und 4)
- Schlanke API: Dreiecke, Texturen, Z-Puffer, Doppelpuffer, Nebel, Alpha. Läuft im Vollbild und im Fenster.
- Austauschbare 3D-Treiber, Auswahl über die Erkennung aus Schritt 1:
  1. Software (überall, im Fenster und Vollbild).
  2. 3dfx Voodoo 1 bis 4 über Glide (Glide 2 für Voodoo 1/2, Glide 3 für Voodoo 3/4; freigegebene
     Quellen, Watcom-Tauglichkeit und Lizenz noch prüfen).
     Voodoo 1/2 sind Zusatzkarten mit Durchschleifung: nur Vollbild. Im Fenster dann Software.
     Voodoo 3/4 können ins Fenster rendern (Bild zurückkopieren), ggf. langsamer.
  3. ATI Rage (Mach64-Familie) und neuer, nVidia Riva 128 und neuer: unter DOS gibt es dafür keine
     3D-Schnittstelle. Erst VESA-LFB mit Software-3D. Echte Beschleunigung nur durch eigene Treiber nach
     den Linux-Quellen (mach64/r128, nv/Nouveau). Das ist Forschung mit offenem Ausgang, Reihenfolge:
     ATI Rage Pro/128 zuerst (besser dokumentiert), nVidia danach.
- Taskwechsel: bei `M_SCREENLOST` gibt RD3D den Glide-Kontext frei, bei `M_SCREENBACK` lädt das Programm
  seine Texturen neu.
- Testen: QEMU kann keine 3D-Karten. 86Box (Voodoo 1/2/3, teils ATI/nVidia, prüfen) und echte Hardware.

## Reihenfolge und Aufwand
1. Bedarf, SysInfo, RDSETUP (klein bis mittel)
2. Vollbild unterbrechbar (mittel)
3. 32-Bit-DOS-Programme testen (klein)
4. RD2/32: 4a Vollbild (groß), 4b Fenster (groß)
5. RD3D: Software, dann Glide Voodoo 1 bis 4 (groß), danach ATI und nVidia (sehr groß, ungewiss)
