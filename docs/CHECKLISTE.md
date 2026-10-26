# RD2: Checkliste Funktionsumfang (Stand 25.10.2026)

Ziel (Karsten, 25.10.2026): alle Funktionen von RetroDesk 1.2 plus die für RD2 beschlossenen. Erst wenn alles da
ist, wird RDUSER nach Messung zerlegt. Grundlage: README von RD1 (main, v1.2), Branch `rechner` (RDCALC),
`docs/PLAN_RD2.md`, `docs/ENTWURF_RD2.md`, Entscheidungen seit dem 19.10. und eine Durchsicht des RD2-Codes.

Stand: **da** = vorhanden und getestet, **teils** = Grundlage da, Teile fehlen, **fehlt**, **prüfen** = im Code
nicht eindeutig, wird beim Abarbeiten geklärt.

## A. Funktionen aus RetroDesk 1.2

### Oberfläche und Shell

| Funktion | RD2 | Bemerkung |
|---|---|---|
| Menüleiste global oben | da | |
| Uhr in der Menüleiste | erledigt 25.10. | HH:MM rechts, Bildvergleich blendet sie aus |
| Menü Desk/RD: Info | erledigt 25.10. | API, CPU mit Takt, Koprozessor, Speicher, DOS, Grafik, Sound (IP folgt mit dem Netz). Früher: RD2 zeigt API und freien Speicher; RD1 zeigt CPU mit Takt, Speicher, DOS, Grafik, Sound, 8087, IP. CPU-Erkennung steckt schon im Kern (`KernInfo`) |
| Menü Desk/RD: Uhr, Notizblock, Rechner, Optionen, Netzwerk | da | Uhr, Notizblock, Rechner, Optionen, Netzwerk, FTP, IRC |
| Beenden | da | |
| Tastatur: F10, Alt+Buchstabe, Pfeile, Eingabe, Esc, Tab in Dialogen | da | test/menu.sh, dlg.sh |
| Bedienung nur mit Maus oder nur mit Tastatur | da | |
| Grafik CGA 640x200, VGA 640x480, Hercules 720x348 | da | Treiber in Assembler, Bildvergleiche |
| Farbe (Profil Komfort, 386+): VGA16 640x480 16 Farben, EGA 640x350 16 Farben, VESA 256 Farben | fehlt | Schnittstelle (bpp, Farben, Palette) und Setup-Eintrag VGA16 vorbereitet; Treiber fehlen. VESA: Bankumschaltung, 640x480x256 braucht 512 KB Bildspeicher; ohne VESA-BIOS Chipsatztreiber oder UniVBE. Plan Punkt 9 |
| Schriften 8x8, 8x14, 8x16 mit Umlauten | da | SYS8/14/16.FNT |
| Startparameter /CGA /VGA /HGC /SPK /NOSND | prüfen | Grafikwahl nach Hardware ist da |
| Deutsch und Englisch | da | DE.LNG, EN.LNG, Ressourcen je Sprache |

### Dateimanager

| Funktion | RD2 | Bemerkung |
|---|---|---|
| Mehrere Fenster, Laufwerksknöpfe | da | |
| Liste sortiert mit Größen | erledigt 25.10. | Listenansicht über das Menü, Ordner zuerst, nach Namen |
| Kopieren, Verschieben, Umbenennen, Neuer Ordner, Löschen, Ordner samt Inhalt | da | |
| Halb kopierte Dateien werden entfernt | da | Audit K1 |
| Mehrfachauswahl | erledigt 25.10. | Strg+Klick, Umschalt+Klick, Umschalt+Pfeil, Leertaste, Einfg |
| Ziehen und Ablegen (gleiches Laufwerk verschiebt, anderes kopiert, Strg/Umschalt) | da | prüfen: Umschalt erzwingt Verschieben |
| Rechtsklickmenü (Umschalt+F10) | teils | Rechtsklick da, Umschalt+F10 prüfen |
| Esc bricht Kopieren ab | prüfen | |
| Laufwerk: Eigenschaften (Typ, Größe, frei, Cluster, Name, Seriennummer, Dateisystem) | da | |
| Laufwerk: Formatieren (180 KB bis 2,88 MB, Schnell, Name) | da | RDFORMAT; Größenauswahl gegen RD1 prüfen |
| Warnung bei Systemdateien | da | neu in RD2 |

### Programme starten

| Funktion | RD2 | Bemerkung |
|---|---|---|
| Menü Programme, Programm anheften (bis 12) | erledigt 25.10. | `.APP` und `PROGRAMM.LST`, Schreibtisch-Symbole (Variante 2) |
| Netzprogramme (Paketreiber vorher laden) | fehlt | |
| DOS-Programm mit vollem Speicher, danach zurück mit allen Fenstern | da | RunDos, XMS oder Auslagerdatei, test/rundos.sh |
| Programm auf anderer Diskette im selben Laufwerk (RDGO) | da | test/rundos_disk.sh |
| Zustand wiederherstellen (Ordner, Fenster, Auswahl) | erledigt 25.10. | `RD2.STA` |

### Zubehör und Optionen

| Funktion | RD2 | Bemerkung |
|---|---|---|
| Uhr groß mit Datum, Stellen, Netzzeit, BIOS-Uhr schreiben | teilweise | UHR.EXE (RD-Menü): 7-Segment, Datum, Stellen, BIOS; Netzzeit folgt mit RDTCP (Punkt 8). test/uhr.sh |
| Notizblock (80 x 60, Strg+S) | erledigt | NOTIZ.EXE (RD-Menü, Doppelklick auf TXT/INI/DOC/NFO/DIZ/LOG): 80 Zeilen x 60 Zeichen wie RD1, Strg+S, Strg+O, Rückfrage, zu lange Dateien nur lesend. test/notiz.sh |
| Einfacher Rechner (8087 wenn da) | erledigt | RECHNER.EXE (RD-Menü): Tasten und Anzeige wie RD1, Rechenkern mit Dezimalziffern (genau, ohne 8087 und ohne Emulator); "8087" im Titel wie RD1. test/rechner.sh, test/rechner_kern.sh (20000 Folgen gleich RD1) |
| Optionen: Systemklänge, Start nach dem Booten, Netzzeit beim Booten | teilweise | OPTIONEN.EXE (RD-Menü): Systemklänge in RD2.INI [System] Systemklang (RDSND hält sich daran); Start nach dem Booten schaltet SHELL= in CONFIG.SYS um ("REM RD2", alte Shell wieder, doppelte INSTALL= aus AUTOEXEC weg, Aus nur mit vorhandener Shell). Netzzeit grau bis Punkt 8. test/optionen.sh |
| Systemklänge über AdLib oder PC-Lautsprecher, AdLib-Erkennung | erledigt 26.10. | RDSND.MOD lädt den Treiber aus RD2.INI [Hardware] Sound; RD2SETUP erkennt AdLib, Sound Blaster, Covox. test/klang.sh |

### Programme

| Funktion | RD2 | Bemerkung |
|---|---|---|
| RDEDIT (8000 Zeilen, Auswahl, Zwischenablage, Suchen/Ersetzen, Gehe zu, sicheres Speichern, .BAK für Startdateien) | fehlt | Zwischenablage und `FileSaveSafe` sind im Kern da |
| RDCALC wissenschaftlich mit Kurven, Verlauf, Druck (ESC/P), Zustand | fehlt | |
| DOS-Shell-Fenster (RDSHELL) | fehlt | heute nur RunDos im Vollbild |
| RDNET: Netzkarte, DHCP oder feste IP, FTP/IRC-Einstellungen, Test | erledigt 26.10. | RDNET.EXE (RD-Menü Netzwerk), Seiten Karte/Adresse/Zeit/Dienste, Netztest, RD2.INI. test/rdnet.sh |
| Eigener TCP/IP-Stapel (ARP, IP, ICMP, TCP, UDP, DHCP, DNS) | erledigt 26.10. | RDTCP.MOD, ohne mTCP, wird beim ersten NetStart geladen. test/netz.sh (QEMU NE2000) |
| Netzzeit (SNTP, Zeitzonen, Sommerzeit) | erledigt 26.10. | UHR Netz-Knopf, beim Booten einmal am Tag (Optionen). test/netzzeit.sh |
| FTP als zweites Dateifenster | erledigt 26.10. | RDFTP.EXE (RD-Menü FTP): links Platte, rechts Server, Tab wechselt, Alt+K kopiert in beide Richtungen, Alt+L löscht, Alt+N neuer Ordner, Ordner per Eingabe/Doppelklick. Passiv, Unix- und DOS-Listen, lange Namen als 8.3. test/rdftp.sh (Host-FTP-Server test/ftpserver.py). Befund: QEMU verdoppelt graue E0-Tasten (Kern-INT-9 liest Port 60h, in QEMU zerstörend), Test nimmt Ziffernblock |
| IRC (Befehle, UTF-8 nach CP437) | erledigt 26.10. | RDIRC.EXE (RD-Menü IRC): verbindet beim Start, PASS für ZNC/BitlBee, Nick belegt -> Nick_, PING/PONG, Kanal aus RD2.INI, /join /part /nick /me /msg /raw /quit, CTCP VERSION, Verlauf 120 Zeilen mit Umbruch, Bild auf/ab. Umlaute, ß, é/è/à/ç, ° in beide Richtungen. test/rdirc.sh (Host-IRC-Server test/ircserver.py) |

## B. Neu in RD2 (beschlossen)

| Funktion | RD2 | Bemerkung |
|---|---|---|
| Mehrere Programme gleichzeitig, Zeitscheiben, Nachrichten | da | |
| Speicher verschiebbar und verwerfbar, Verdichten | da | |
| Module verwerfen und nachladen (Diskettenabfrage, XMS als Nachladespeicher) | da | |
| Bei 640 KB oder weniger stärker verwerfen | da | Reihenfolge verdichten, verwerfen, auslagern (SPEC 2.4) |
| Auslagern bei Speichermangel (XMS, sonst RD2.SWP auf der Platte) | da | RDAUSL.MOD, test/auslagern.sh (25.10.) |
| Klare Meldungen bei Speichermangel | da | Shell: „Zu wenig Speicher für … Frei: … KB“ (25.10.) |
| Fenster: Größe ändern, Minimieren | erledigt 25.10. | Ecke rechts unten ziehen, Feld rechts im Titel; `test/fenster.sh` |
| Alt+Tab zwischen Programmen | erledigt 25.10. | auch Alt+F6 (XT-BIOS kennt kein Alt+Tab), holt minimierte Fenster zurück |
| Steuerelemente: Knopf, Häkchen, Option, Text, Gruppe, Eingabe, Liste, Fortschritt, Rollbalken | da | |
| Klappliste (COMBO), mehrzeiliges Textfeld (TEXT), eigener Rollbalken (SCROLL) | da | RDCTLX.MOD; Korrektur 25.10.: stand zuerst als fehlend |
| Standarddialoge: Meldung, Eingabe, Datei öffnen/speichern | da | |
| Zwischenablage | da | |
| Programmanmeldung `.APP`, Dateitypen öffnen das passende Programm | erledigt 25.10. | |
| Diskettenwechsel erkennen, Liste aktualisieren | erledigt 25.10. | |
| Hilfesystem (F1) | fehlt | `HelpShow` nur festgelegt |
| RDSND: Speaker- und AdLib-Treiber, IMF/DRO, WAV über Lautsprecher | erledigt 26.10. | Treiber SPEAKER.DRV (Töne und Samples, Uhr läuft weiter), ADLIB.DRV, SB.DRV, COVOX.DRV (LPT1). Musik IMF/DRO in RDMUS.MOD, WAV (8 Bit mono) per `SndWavLoad` aus RD2.LIB. MPU-401 später. test/klang.sh |
| Joystick | fehlt | `JoyRead` nur festgelegt |
| Drucken: 9-Nadel-Treiber (ESC/P), Druckdialog | fehlt | |
| Serielle Schnittstelle (Nullmodem, Modem) | fehlt | |
| Festplatte einrichten (RDFDISK), formatieren, startfähig (FreeDOS) | da | test/kette.sh |
| Installation auf Festplatte (RD2SETUP) | da | |
| Bootfähige Systemdiskette mit FreeDOS-Kern, Satz aus 360-KB-Disketten | teils | Diskette 1 startet; Aufteilung auf Diskette 2 bis 4 prüfen |
| SDK: RD2.H, RD2.LIB, Startcode | da | |
| Ressourcen-Compiler RDRC | teils | nur als Python-Werkzeug, DOS-Fassung fehlt |
| SDK: Turbo Pascal (RD2.PAS), Beispiele, Handbuch DE/EN | fehlt | |
| Später: 386 (Modus P, DOS im Fenster), RD2 bootet ohne DOS | später | nach 2.0 |

## C. Vorschlag Reihenfolge

1. Speichermangel: Meldungen, Auslagern (XMS, RD2.SWP), Regeln für 640 KB. Das legt fest, wie alle Programme mit Speicher umgehen.
2. Fenster fertig: Größe ändern, Minimieren, Alt+Tab. (erledigt 25.10.)
3. Shell fertig: Listenansicht, Uhr in der Leiste, Info wie RD1, `.APP`/Programme-Menü, `RD2.STA`, Diskettenwechsel, fehlende Tasten. (erledigt 25.10., acb820d; dabei G31: MsgBox-Text war auf 31 Zeichen gekürzt)
4. Zubehör: Uhr, Notizblock, einfacher Rechner, Optionen.
5. Sound: RDSND mit Speaker und AdLib, Systemklänge, IMF/DRO. (erledigt 26.10.; dazu Sound Blaster, Covox, WAV)
6. RDEDIT.
7. Drucken, danach RDCALC mit Kurven und Druck.
8. Netz: RDTCP, RDNET, Netzzeit, FTP, IRC, DOS-Shell-Fenster.
9. Farbe (Profil Komfort): VGA16.DRV (Modus 12h, Assembler), EGA 640x350 (16 Farben), danach VESA 256 Farben mit Bankumschaltung; Farben in RDGDI/RDUSER (Palette, Systemfarben). (aufgenommen 26.10.)
10. Hilfe, Joystick, serielle Schnittstelle, SDK-Rest, Diskettensatz, Test am PC10-III, Release 2.0.
