# RD2: Checkliste Funktionsumfang (Stand 27.10.2026)

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
| Farbe (Profil Komfort, 386+): VGA16 640x480 16 Farben, EGA 640x350 16 Farben, VESA 256 Farben | da | erledigt 27.10. | Voller Testsatz 27.10. grün (44 Tests). 27.10. Teil 3: VESA.DRV (Modus 101h, 640x480, 256 Farben, Bankumschaltung INT 10h 4F05h, Palette 16 Standardfarben + Würfel 6x6x6 + 24 Grau), Geräteformat 8 Bit in RDGDI (Hintergrund retten), nur von Hand wählbar (Menü Grafik, RD2SETUP), Diskette 2. Offen: farbige Symbole (Kern macht noch alles S/W). 27.10. Teil 4: farbige Symbole (FARBSYM.DAT auf Diskette 2, 16/256 Farben mit Maske, Zeichnen in RDGDIX, S/W bleibt Rückfall); Fehler behoben: Symbole kamen auf VGA als 32x16 (CGA-Fassung). |
| Schriften 8x8, 8x14, 8x16 mit Umlauten | da | SYS8/14/16.FNT |
| Startparameter /CGA /VGA /HGC /SPK /NOSND | da 30.10. | RD.EXE kennt /CGA /HGC /VGA (Mono), /NOMAUS, /SETUP, /P, /K, /D (nur der erste Buchstabe zählt); /CGA und /VGA laufen in fast allen Tests, /HGC in bench.sh und gdi_dosbox.sh. /SPK und /NOSND entfallen (Karsten 30.10.): kein Ohne-Klang-Betrieb, Minimum ist der PC-Lautsprecher. RD2SETUP und Optionen bieten "keine" nicht mehr an, altes Sound=AUS gilt als SPEAKER. |
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
| Rechtsklickmenü (Umschalt+F10) | da 30.10. | Rechtsklick und Umschalt+F10 (Menü Datei an der Auswahl), test/kopabbr.sh Bild kopabbr_menu |
| Esc bricht Kopieren ab | da 30.10. | SHELL prüft nach jedem 4-KB-Block auf Esc (andere Eingaben verfallen während des Kopierens), halbe Kopie wird gelöscht, Quelle bleibt. test/kopabbr.sh (2,88-MB-Testdiskette, 1 MB Datei) |
| Laufwerk: Eigenschaften (Typ, Größe, frei, Cluster, Name, Seriennummer, Dateisystem) | da | |
| Laufwerk: Formatieren (180 KB bis 2,88 MB, Schnell, Name) | da | RDFORMAT; Größenauswahl gegen RD1 prüfen |
| Warnung bei Systemdateien | da | neu in RD2 |

### Programme starten

| Funktion | RD2 | Bemerkung |
|---|---|---|
| Menü Programme, Programm anheften (bis 12) | erledigt 25.10. | `.APP` und `PROGRAMM.LST`, Schreibtisch-Symbole (Variante 2) |
| Netzprogramme (Paketreiber vorher laden) | erledigt 26.10. | RDTCP.MOD (eigener Stack), RDFTP, RDIRC, RDNET, Netzzeit; Paketreiber vor RD2 laden. test/netz.sh, test/rdftp.sh, test/rdirc.sh |
| DOS-Programm mit vollem Speicher, danach zurück mit allen Fenstern | da | RunDos, XMS oder Auslagerdatei, test/rundos.sh |
| Programm auf anderer Diskette im selben Laufwerk (RDGO) | da | test/rundos_disk.sh |
| Zustand wiederherstellen (Ordner, Fenster, Auswahl) | erledigt 25.10. | `RD2.STA` |

### Zubehör und Optionen

| Funktion | RD2 | Bemerkung |
|---|---|---|
| Uhr groß mit Datum, Stellen, Netzzeit, BIOS-Uhr schreiben | erledigt 26.10. | UHR.EXE (RD-Menü): 7-Segment, Datum, Stellen, BIOS, Netzzeit (SNTP über RDTCP). test/uhr.sh, test/netzzeit.sh |
| Notizblock (80 x 60, Strg+S) | erledigt | NOTIZ.EXE (RD-Menü, Doppelklick auf TXT/INI/DOC/NFO/DIZ/LOG): 80 Zeilen x 60 Zeichen wie RD1, Strg+S, Strg+O, Rückfrage, zu lange Dateien nur lesend. test/notiz.sh |
| Einfacher Rechner (8087 wenn da) | erledigt | RECHNER.EXE (RD-Menü): Tasten und Anzeige wie RD1, Rechenkern mit Dezimalziffern (genau, ohne 8087 und ohne Emulator); "8087" im Titel wie RD1. test/rechner.sh, test/rechner_kern.sh (20000 Folgen gleich RD1) |
| Optionen: Systemklänge, Start nach dem Booten, Netzzeit beim Booten | erledigt 26.10. | OPTIONEN.EXE (RD-Menü): Systemklänge in RD2.INI [System] Systemklang (RDSND hält sich daran); Start nach dem Booten schaltet SHELL= in CONFIG.SYS um ("REM RD2", alte Shell wieder, doppelte INSTALL= aus AUTOEXEC weg, Aus nur mit vorhandener Shell), Netzzeit beim Booten. test/optionen.sh, test/netzzeit.sh |
| Systemklänge über AdLib oder PC-Lautsprecher, AdLib-Erkennung | erledigt 26.10. | RDSND.MOD lädt den Treiber aus RD2.INI [Hardware] Sound; RD2SETUP erkennt AdLib, Sound Blaster, Covox. test/klang.sh |

### Programme

| Funktion | RD2 | Bemerkung |
|---|---|---|
| RDEDIT (8000 Zeilen, Auswahl, Zwischenablage, Suchen/Ersetzen, Gehe zu, sicheres Speichern, .BAK für Startdateien) | erledigt | RDEDIT.EXE: 8000 Zeilen, Auswahl, Zwischenablage, Suchen/Ersetzen, Gehe zu, FileSaveSafe, .BAK. test/edit.sh |
| RDCALC wissenschaftlich mit Kurven, Verlauf, Druck (ESC/P), Zustand | zurückgestellt 27.10. | Karsten: Rechner vorerst lassen, wie er ist |
| DOS-Shell-Fenster (RDSHELL) | erledigt 26.10. | RD > DOS-Fenster: DIR [/W], CD, X:, MD, RD, DEL, REN, COPY, TYPE, VOL, MEM, VER, DATE, TIME, ECHO, CLS eingebaut (ohne COMMAND.COM); andere Programme über RunDos, Suche in RD2.INI [DOS] Pfad=, .BAT über COMMAND.COM ([DOS] Comspec=); Diskettenbetrieb ohne XMS: Hinweis, Start über den Schreibtisch; `test/rdshell.sh` |
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
| Maximieren, Parkreihe, drei Titelfelder | erledigt 26.10. | rechts im Titel _ □ X; Viereck, Doppelklick auf den Titel oder Alt+F10 maximiert und stellt zurück; Minimierte unten links geparkt (6 je Reihe), Klick holt zurück; RDUSER 29 KB (Variante B) |
| Steuerelemente: Knopf, Häkchen, Option, Text, Gruppe, Eingabe, Liste, Fortschritt, Rollbalken | da | |
| Klappliste (COMBO), mehrzeiliges Textfeld (TEXT), eigener Rollbalken (SCROLL) | da | RDCTLX.MOD; Korrektur 25.10.: stand zuerst als fehlend |
| Standarddialoge: Meldung, Eingabe, Datei öffnen/speichern | da | |
| Zwischenablage | da | |
| Programmanmeldung `.APP`, Dateitypen öffnen das passende Programm | erledigt 25.10. | |
| Diskettenwechsel erkennen, Liste aktualisieren | erledigt 25.10. | |
| Hilfesystem (F1) | erledigt 27.10. | HILFE.EXE + RD2.HLP (Inhalt, 13 Themen, Verweise, zurück), HelpShow über RDKX -> RDPRN.MOD Nr. 6; F1 in Schreibtisch, Ordner, Notiz, Rechner, Uhr; docs/HILFE.md, test/hilfe.sh |
| RDSND: Speaker- und AdLib-Treiber, IMF/DRO, WAV über Lautsprecher | erledigt 26.10. | Treiber SPEAKER.DRV (Töne und Samples, Uhr läuft weiter), ADLIB.DRV, SB.DRV, COVOX.DRV (LPT1). Musik IMF/DRO in RDMUS.MOD, WAV (8 Bit mono) per `SndWavLoad` aus RD2.LIB. MPU-401 später. test/klang.sh |
| Tastaturtreiber DE/US (RDKEYB.COM), auch in DOS-Programmen | erledigt 28.10. | eigener residenter Treiber (Assembler, 8086, INT 9, 1 KB Datei): Y/Z, Umlaute, ß, AltGr bzw. Strg+Alt, Tottasten ^ ´ `, <>|; Strg+Alt+F1/F2 schaltet US/DE; zweiter Aufruf schaltet nur um, fremder KEYB hat Vorrang. Diskette 1 und RD2SETUP tragen ihn per INSTALL= ein (Einstellung Tastatur in RD2SETUP), vorhandener Tastaturtreiber aus der AUTOEXEC.BAT bleibt. test/tastatur.sh |
| Datei-Eigenschaften (Größe, Datum/Zeit, Attribute) | erledigt 28.10. | SHELL: Datei > Eigenschaften oder Alt+Eingabe im Ordner: Name, Größe (Ordner: „Ordner“), geändert TT.MM.JJJJ HH:MM, Häkchen Schreibgeschützt/Versteckt/System/Archiv, ändern mit OK (Systemdateien: zweite Rückfrage). Dabei behoben: Alt+Eingabe vom erweiterten BIOS (11Ch) kam nie an, auch nicht bei Laufwerk-Eigenschaften. test/eigensch.sh |
| Dateien suchen | erledigt 28.10. | SHELL: Datei > Suchen oder Strg+F im Ordner (ab diesem Ordner), Laufwerk > Suchen oder Strg+F auf dem Schreibtisch (ganzes Laufwerk). Muster mit * und ? (8.3), alle Unterordner bis 8 Ebenen, bis 100 Treffer, Liste mit vollem Pfad, „Ordner öffnen“ oder Doppelklick öffnet den Ordner der Datei. test/suchen.sh |
| Versteckte Dateien anzeigen | erledigt 28.10. | SHELL: Ansicht > Versteckte Dateien (Haken): versteckte und System-Dateien erscheinen grau, sonst unsichtbar; gilt für alle Ordnerfenster, bleibt in RD2.STA. test/versteckt.sh |
| Papierkorb (Festplatte) | erledigt 28.10. | Schreibtisch-Symbol (eigenes S/W-Symbol PAPIERK.ICO, SYMBOL 6): RD2SETUP legt bei neuer Installation PROGRAMM.LST mit [Papierk] Schreibtisch=1 an, wenn PAPIERK.EXE kopiert wurde; die Shell zeigt für den Abschnitt Papierk das Papierkorb-Symbol. Farbsatz hat noch kein eigenes Papierkorb-Symbol. | SHELL: Entf/F8 legt Dateien auf Festplatten nach X:\PAPIERK\ (Umbenennen, PAPIERK.LST Name|Originalpfad, Namensgleiche bekommen eine Ziffer), Ordner und Disketten löschen sofort, Umschalt+Entf endgültig. PAPIERK.EXE (RD > Papierkorb, Diskette 2): Wiederherstellen, Löschen, Leeren. test/papierk.sh |
| Mauseinstellungen | erledigt 28.10., seit 29.10. in Optionen | (29.10.: MAUS.EXE entfällt, Karte Maus in RD > Optionen, dazu Anschluss = RDMAUS-Schalter.) Bisher MAUS.EXE (Diskette 2): Geschwindigkeit 1..5, Doppelklick kurz/mittel/lang (300/500/700 ms) mit Testknopf, linke/rechte Taste tauschen. RD2.INI [Maus] Tempo=, Doppelklick=, Tasten=; gilt beim nächsten Start von RD2. Kern liest die Werte (Doppelklick-Zeit, Hochbyte von SYSINFO.maus: Tempo + Bit 15 Tausch), RDGDI setzt das Tempo (INT 33h/0Fh) nach jedem Treiber-Reset neu und tauscht die Tasten. Test: test/maus.sh. Kern 28.800/28.800, RDGDI 16.412/16.448 Bytes. |
| Bildschirmschoner | erledigt 28.10., Einstellungen seit 29.10. in Optionen | SCHONER.EXE (Diskette 2) zeigt nur noch den Schoner; Einstellungen auf der Karte Schoner in RD > Optionen (Menüpunkte RD > Maus und RD > Bildschirmschoner entfallen). Sternenfeld (Standard), Uhr, Schwarz; Wartezeit aus/1/2/5/10/15 Min. (Standard 5), Vorschau. RD2.INI [Schoner] Zeit= Sekunden, Art= 0/1/2. Neue Kern-API InputIdle (0x025): ms seit letzter Eingabe. Shell startet SCHONER.EXE S einmal je Ruhephase, Menüleisten-Uhr ruht solange. Taste/Klick/Mausbewegung beendet ihn, ohne weitergereicht zu werden. Test: test/schoner.sh. Kern 28.864/28.864 Bytes. |
| Eigener Maustreiber RDMAUS.COM (seriell, Bus, InPort, PS/2) | fertig 28.10. | Residenter 8086-Treiber (INT 33h 00-10, 13-17, 1A-1E, 21, 24, Kennung 4Dh "RDMS"), PS/2 über BIOS INT 15h/C2h, seriell Microsoft/Logitech und Mouse Systems, Logitech/Microsoft-Busmaus und InPort (23Ch/238h, IRQ-Suche, /Q2../Q7), Zeiger im Textmodus und in den DOS-Grafikmodi, Funktion 10h (Zeiger im Bereich aus, bis Funktion 1). RD2SETUP trägt RDMAUS ein, CTMOUSE entfällt. 28.10.: InPort-Tasten korrigiert (Bit 2 links, 1 Mitte, 0 rechts). Tests: test/rdmaus.sh (PS/2, seriell, Grafik, Funktion 10h, QEMU), test/busmaus.sh (Bus, InPort, PS/2 in Bochs über RFB). Echte Hardware (PC10-III) steht aus. Für 2.1: Beschleunigung bei schnellen Bewegungen (Funktion 13h). Netzkartentreiber bringt der Nutzer selbst mit (Karsten, 28.10.). |
| Einrichtung RD2SETUP: Bedienung, Maus- und Klangwahl | Textteil erledigt 29.10. | Maus wählbar (seriell COM1-4, Mouse Systems COM1/2, PS/2, Bus, InPort 23Ch/238h, eigener Treiber, keine), Vorschlag aus der Erkennung; ist RDMAUS schon geladen, fragt RD2SETUP Art und IRQ ab (INT 33h 4Dh/24h). Gewählte Maus gilt für MausHW= und den RDMAUS-Schalter (/S, /M, /P, /B, /I1, /I2), bei keine/eigener Treiber kein RDMAUS. Klang „keine“, Zeile Sound Blaster nur bei Sound Blaster. „geändert, erkannt: …“ statt „(von Hand)“, links/rechts ändern, klare Texte (I nimmt die angezeigten Werte, S speichert auf der Diskette), Liste überdeckt die Hilfezeile nicht mehr. Grafische Installation erledigt 29.10.: INSTALL.EXE (Diskette 1, 10,9 KB), RD2SETUP I schreibt Installieren=1, die Shell startet INSTALL.EXE (auch Menü RD > Installation auf Festplatte, nur beim Start von Diskette). Dialog Ziel, startfähig, CONFIG.SYS; Fortschrittsbalken über Diskette 1-3, Wechsel per Meldung (auslassen möglich), RD2.INI, CONFIG.SYS wie RD2SETUP (Fragen als Ja/Nein-Dialog, PATH aus AUTOEXEC.BAT), Papierkorb, zuletzt RDSYS per RunDos (ohne XMS mit Neustart von RD2). T in RD2SETUP: Text-Installation (Notweg). Test test/install.sh (54 s; FALL=2 bestehendes DOS mit Fragen als Dialog, 40 s), satz.sh und install_text.sh prüfen den Textweg. XMS-Ablage bietet nur an, was an XMS da ist (29.10.). |
| Fehler: graue Tasten doppelt (Entf löschte zweimal) | behoben 28.10. | Kern-INT 9 und RDKEYB lasen Port 60h vor dem BIOS; emulierte 8042 (QEMU) geben dabei schon das nächste Byte heraus, das BIOS sah Entf/Pos1/Pfeile doppelt und ohne E0. Kern liest Port 60h nur noch bei gedrücktem Strg+Alt, RDKEYB holt den Scancode auf AT-BIOS über INT 15h/4Fh (XT: Port 60h wie bisher). test/tastatur.sh (Entf, Pos1), test/papierk.sh mit Entf |
| Taskmanager | erledigt 28.10. | TASKMAN.EXE (RD > Taskmanager, Diskette 2): laufende Programme mit belegtem Speicher, freier Speicher, Beenden mit Rückfrage („Nein“ vorgewählt), SHELL nur nach zweiter Warnung, Aktualisieren. test/taskman.sh |
| Datenträger prüfen (wie CHKDSK) | erledigt 28.10. | DPRUEF.EXE (Diskette 2), Aufruf über Laufwerk > Prüfen: FAT12/FAT16, FAT-Kopien vergleichen, alle Ordner und Ketten (ungültige Cluster, Querverweise, unterbrochene Ketten, Größe passt nicht), verlorene Cluster. Reparieren (mit Rückfrage): verlorene Cluster freigeben, FAT 1 auf alle Kopien. Querverweise/Größe nur gemeldet. Disketten über INT 13h (INT 25h scheiterte unter FreeDOS bei B:), Festplatten über INT 25h/26h (große Partitionen mit Paket, ungetestet). test/dpruef.sh (Diskette mit Schäden); 10-MB-Platte FAT12 ohne Fehlalarm |
| Diskette kopieren (wie DISKCOPY, auch mit einem Laufwerk) | erledigt 28.10. | DKOPIE.EXE (Diskette 2), Aufruf über Laufwerk > Diskette kopieren (nur Diskettenlaufwerke): A:→A:, B:→B:, A:→B:, B:→A:; Format aus dem Bootsektor (180 KB bis 2,88 MB), Spuren über INT 13h (RDDISK.MOD), so viele Zylinder je Durchgang wie Speicher frei, bei einem Laufwerk Wechsel mit Rückfrage, Fortschrittsbalken. Ziel muss im gleichen Format formatiert sein (kein Formatieren beim Kopieren). test/dkopie.sh: 360 KB B:→B:, 2 Durchgänge, Ziel byte-gleich |
| Joystick | erledigt 27.10. | `JoyRead`/`JoyCalibrate` in RD2.LIB (sdk/JOY.C, läuft im Programm, Kern unverändert): Gameport 201h, Zeit über PIT (gleiche Werte auf XT und Pentium), Joystick A/B, x/y -100..100, Mitte und Grenzen lernt es selbst, ohne Gameport RD_ERR_NOTSUP. test/joystick.sh (QEMU ohne Gameport, DOSBox 2axis). Bewegung nur am echten Gerät prüfbar |
| Drucken: 9-Nadel-Treiber (ESC/P), Druckdialog | erledigt 27.10. | RDPRN.MOD (RDKX lädt es bei PrnOpen): Druckdialog (LPT1/LPT2/Datei DRUCK.PRN, Kopien, Entwurf), Seite in Bändern zu 48 Zeilen (5,6 KB statt 95 KB), LPT direkt am Port mit Rückfrage „nicht bereit“. EPSON9.DRV: ESC L 120 x 72 dpi, Entwurf ESC K 60 dpi, leere Bereiche nur Vorschub. RD2.INI [Drucker]. test/drucken.sh (QEMU-Druckerport, test/escp.py als Bild) |
| Serielle Schnittstelle (Nullmodem, Modem) | später | nach 2.0 (Karsten, 27.10.): Netz über Karte und FTP decken die Datenverbindung ab. Idee für später: RDCOM.MOD + SLIP in RDTCP, kleines TERMINAL.EXE |
| Festplatte einrichten (RDFDISK), formatieren, startfähig (FreeDOS) | da | test/kette.sh |
| Installation auf Festplatte (RD2SETUP) | da | |
| Bootfähige Systemdiskette mit FreeDOS-Kern, Satz aus 360-KB-Disketten | teils | Diskette 1 startet; Aufteilung auf Diskette 2 bis 4 prüfen |
| SDK: RD2.H, RD2.LIB, Startcode | da | |
| Ressourcen-Compiler RDRC | erledigt 28.10. | RDRC.EXE (DOS, 8086, tools/RDRC.C) übersetzt .RC und hängt RD2-Block + Ressourcen an die EXE (ersetzt rdrc.py + rd2link.py, byte-gleich bei allen 24 .RC im Baum, test/rdrc.sh in DOSBox). Auf Diskette 4 in \\SDK. |
| SDK: Pascal (RD2.PAS), Beispiele, Handbuch DE/EN | teils 29.10. | Pascal erledigt: Unit RD2 (tools/mkpas.py aus RD2.H/RD2API.H, 206 Funktionen, 17 Strukturen, 329 Konstanten) für Turbo Pascal 5.5-7.0 (inline, kein Aufrufaufwand) und Free Pascal i8086-msdos (Large). Vorspann sdk/RD2PAS.ASM (MCB+PSP, Speicher löschen, INT 21h nur während des Starts, Ende über ExitProc), RDRC /PAS bzw. rd2link.py --pas, Kern unverändert. Rückrufe über CbProc (DS-Brücke). Beispiele apps/pascal (PHALLO, PTEST), Anleitung docs/PASCAL.TXT, Diskette 4 \\SDK\\PASCAL. Test test/pascal.sh (TP 5.5 in DOSBox + FPC 3.2.2, beide in QEMU auf RD2). SDK-Handbuch erledigt 29.10.: docs/SDK.TXT (DE) und SDK_EN.TXT (EN, mit Pascal-Teil) auf Diskette 4, C-Beispiel apps/hallo (HALLO.C/.RC, Fenster, Menü, Klicks) unter \\SDK\\BEISPIEL, läuft in test/pascal.sh mit. Hilfe RD2.HLP: Einrichtung neu beschrieben, neues Thema 14 Installation auf Festplatte. |
| Später: 386 (Modus P, DOS im Fenster), RD2 bootet ohne DOS | später | nach 2.0 |

## C. Vorschlag Reihenfolge

1. Speichermangel: Meldungen, Auslagern (XMS, RD2.SWP), Regeln für 640 KB. Das legt fest, wie alle Programme mit Speicher umgehen.
2. Fenster fertig: Größe ändern, Minimieren, Alt+Tab. (erledigt 25.10.)
3. Shell fertig: Listenansicht, Uhr in der Leiste, Info wie RD1, `.APP`/Programme-Menü, `RD2.STA`, Diskettenwechsel, fehlende Tasten. (erledigt 25.10., acb820d; dabei G31: MsgBox-Text war auf 31 Zeichen gekürzt)
4. Zubehör: Uhr, Notizblock, einfacher Rechner, Optionen.
5. Sound: RDSND mit Speaker und AdLib, Systemklänge, IMF/DRO. (erledigt 26.10.; dazu Sound Blaster, Covox, WAV)
6. RDEDIT.
7. Drucken, danach RDCALC mit Kurven und Druck.
8. Netz: RDTCP, RDNET, Netzzeit, FTP, IRC, DOS-Shell-Fenster. (erledigt 26.10.)
9. Farbe (Profil Komfort): VGA16.DRV (Modus 12h, Assembler), EGA 640x350 (16 Farben), danach VESA 256 Farben mit Bankumschaltung; Farben in RDGDI/RDUSER (Palette, Systemfarben). (aufgenommen 26.10., erledigt 27.10.: dazu farbige Symbole FARBSYM.DAT)
10. Hilfe (erledigt), Joystick (erledigt), SDK-Rest, Diskettensatz, Test am PC10-III, Release 2.0.
| Optionen mit Karteikarten | umgesetzt 29.10., in QEMU getestet 30.10. | OPTIONEN.EXE: Karten Grafik, Klang, Maus, Tastatur, Schoner, System (Alt+1..6, wie RDNET). Grafik gilt sofort: RD2 startet neu (RunDos RD_DOS_NEU), danach "Behalten?" mit 15 s (Shell startet OPTIONEN B, solange [System] GrafikAlt= gesetzt ist). Plan: docs/PLAN_OPTIONEN.md. 30.10.: test/optionen.sh auf Karten umgestellt, SHOT-Referenzen erneuert, voller Satz (68 Tests) grün. Offen: test/grafikwahl.sh, DOSBox, Test am PC10-III. |

## Universalpaket (30.10.2026)

| Funktion | RD2 | Bemerkung |
|---|---|---|
| Installation aus einem Festplattenordner per RD2INST.BAT | da 30.10. | release/2.0/RD2_HDD.ZIP (ersetzt RD2_FTP.ZIP): alle vier Disketten in einem Ordner RD2, SDK als RD2\SDK. INSTALL.EXE kopiert bei Quelle Festplatte auch *.TXT, COPYING und den Ordner SDK samt Unterordnern. test/install_hd.sh prüft SDK-Dateien und LIZENZ.TXT auf C:\RD2. |
