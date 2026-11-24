# PLAN_API - RD2 stellt alle Treiber und die API bereit (Karsten, 24.11.2026)

Ziel: RetroDesk 2 ist ein eigenstaendiges, DOS-kompatibles System mit 16 und 32 Bit, das alle noetigen Treiber
selbst stellt. Programme (auch Spiele wie der Spritzwerkzeugbauer) greifen nur ueber die RD2-API auf Hardware zu und
bringen keine eigenen Treiber mehr mit. 3D (Glide, ATI, nVidia) kommt spaeter (PLAN_32BIT Schritt 5).
Grundsaetze wie bisher: neue Funktionen in neue, verwerfbare Module; 8088-Multitasking unveraendert; Treiber
universell, Sonderfaelle nur ueber Erkennung/RD2.INI; 32-Bit-Teile unter dem 386 nicht installieren.

## Gesamtziel (Karsten, 24.11.2026)
Vollstaendiges, DOS-kompatibles Betriebssystem, einfach zu bedienen, mit GUI. Treiber fuer Sound, Grafik, Tastatur,
Maus, Drucker und Netzwerk. Laeuft ab 8088 mit 512 KB bis Pentium MMX mit 64 MB und schneller. Grafik HGC, CGA,
EGA, VGA, SVGA und besser. Programme laufen nativ in RD2. Multitasking auf 8088 und ab 386. Sparsam mit RAM,
besonders bei 640 KB. Netz: kompatible Paketreiber bringt der Nutzer mit (bestaetigt 24.11.2026). Drucker: universelle
Treiber statt einem je Geraet: ESCP24.DRV (Epson ESC/P 24 Nadeln, auch ESC/P2: Epson Stylus, Canon BJ in LQ-Emulation,
NEC, Star, OKI mit Epson-Emulation) und PCL.DRV (HP PCL 3/5: DeskJet, LaserJet und Kompatible); reine GDI-/Windows-
Drucker gehen unter DOS grundsaetzlich nicht. Mindestausbau 512 KB pruefen.

## Bestand (24.11.2026, aus RD2.H/RD2API.H)
| Bereich | vorhanden | Luecke |
|---|---|---|
| Grafik Fenster | GDI 1/4/8 Bit, CGA bis VESA 1024 | - |
| Grafik Vollbild | ScreenAcquire/ScreenMode/ScreenPalette, 32 Bit VESA jede Tiefe, Baenke | 16-Bit-Programme: VESA nur ueber ScreenBank |
| Tastatur | Nachrichten, KeyState | pruefen: KeyState im Vollbild/Rohmodus, mehrere Tasten zugleich |
| Maus | Nachrichten, CursorPos/CursorSet | Maus im Vollbild: Koordinaten in Spielaufloesung, Bereich, Zeiger vom Programm |
| Joystick | JoyRead, JoyCalibrate | - |
| Zeit | TickCount (ms), TimerSet (Nachricht) | feiner Zeitgeber mit Rueckruf (z. B. 70/140 Hz) fuer Spiele und Musik |
| Klang Lautsprecher | SPEAKER.DRV, SndBeep/SndNote | - |
| Klang FM | ADLIB.DRV, SB.DRV (OPL2/OPL3), SndInstr/SndNote/SndNoteOff, SndMusicPlay | pruefen: Effekte/Kanaele, OPL3-Stereo |
| Klang digital | SB.DRV 8 Bit DMA, COVOX.DRV, SndSamplePlay/SndWavLoad, Ringpuffer (SM_HALBE) | Mischen mehrerer Samples, SB16 16 Bit/Stereo, Lautstaerke je Kanal |
| MIDI | SC_MIDI in SndCaps | MPU-401-Treiber fehlt |
| Speicher | 16 Bit Bloecke, 32 Bit rd32_alloc | - |
| Dateien, INI, Netz | vorhanden | - |

## Abgleich aus dem Code (Schritt 1, 24.11.2026)
- Takt: Der Kern beschleunigt den PIT schon auf 72,8 Hz (13,7 ms, INT 8, Uhr bleibt richtig). Programme bekommen davon
  nur TimerSet-Nachrichten (Aufloesung 13,7 ms, Zustellung erst bei GetMessage) und TickCount (ms). Kein Rueckruf im
  Takt, kein hoeherer Takt (140 Hz) fuer Spiele/Musik.
- Vollbild: ScreenAcquire schaltet Zeiger und GDI ab (gpause), das Programm setzt den Modus selbst. Mausnachrichten
  kommen weiter mit Koordinaten der Schreibtisch-Aufloesung; Bereich und Zeiger im Vollbildmodus fehlen.
- Tasten: Nachrichten und KeyState vorhanden; im Rohmodus mehrere Tasten zugleich noch pruefen (Testprogramm).
- Klang: SND.C deckt PC-Lautsprecher, AdLib/OPL2, SB 1.x bis AWE (FM an 388h, OPL3 zweite Bank 38Ah) und Covox ab.
  PCM nur 8 Bit mono, ein Sample zur Zeit (Einzelblock-DMA, Befehl 14h); kein Mischen, kein Auto-Init, kein SB16
  16 Bit/Stereo, keine Lautstaerke je Kanal. Musik IMF/DRO ueber RDMUS.MOD. MIDI (MPU-401) fehlt.
- Drucker: EPSON9.DRV; ESCP24 und PCL fehlen.
- Zusammen mit dem Spiel (eigene Treiber heute: VESA 101h, INT 33h, kbhit, 140-Hz-Zeitgeber, Lautsprecher, AdLib)
  ergibt das die Schritte 2 bis 6, dazu die Drucker (Schritt 7).

## Schritte
1. Abgleich: Jede Funktion oben in 16 und 32 Bit mit einem Testprogramm pruefen (Vollbild + Maus + Tasten + Klang
   zugleich, Alt+Tab). Ergebnis: genaue Luecken-Liste. Die Engine (RDISOENGINE/Spritzwerkzeugbauer) als Pruefstein:
   was nutzt sie heute selbst (VESA 101h, INT 33h, kbhit, eigener 140-Hz-Zeitgeber, PC-Lautsprecher, AdLib)?
2. Zeitgeber (erledigt 24.11.2026, Entscheidung Karsten: fester Takt): PIT fest auf 145,6 Hz (Teiler 8192, 8 x 18,2 Hz),
   TaktSet(proc, teiler)/TaktKill/TaktCount (API 0x032-0x034), bis zu 8 Rueckrufe im Interrupt. Kern-Takt (Klang,
   Zeitscheiben, TickCount) intern weiter jeden 2. Takt mit 72,8 Hz, BIOS-Uhr jeden 8.; Sample-Takt im Treiber und
   32-Bit-Zeitscheibe (8 Takte = 55 ms) angepasst. Test test/takt.sh (8088 und Pentium).
   2b erledigt (24.11.2026): RD2/32-Rueckrufe aus dem PM-Zeitgeber (RD32A i8pm, TB+68 = rd32_takttramp in RD32C0,
   eigener Stapel 2 KB, Eintraege in RD32LIB), auch in Schleifen ohne API; Module ohne TaktSet. TAKT32.EXE in takt.sh.
   Absicherung: TB+72 = ~TB+68 und TB_CS32 muss passen (verwaiste RD32-Eintraege nach TaskKill). Offen: solche
   verwaisten Eintraege belegen ihren der 8 RD32-Plaetze weiter (schon vor 2b so), beim Aufraeumen beheben.
3. Eingabe im Vollbild: Maus mit Koordinaten im Vollbildmodus, Bereich und Empfindlichkeit, KeyState-Tabelle fuer
   alle Tasten zugleich, auch im Rohmodus.
4. Sound Blaster vollstaendig: SB 1.x/2.0/Pro/16/AWE und Kompatible; Mischer in RD2 (mehrere Samples zugleich,
   Lautstaerke je Kanal), 16 Bit/Stereo ab SB16, FM ueber OPL2/OPL3 mit Musik und Effekten gleichzeitig.
5. MPU-401 (MIDI) als Treiber; Tandy/Game Blaster/Gravis nativ spaeter (wie in Punkt 5 festgelegt).
6. SDK und Doku: alles in RD2.H/RD2_32.H, SDK.TXT, Beispiele; danach die Engine auf die RD2-API umstellen
   (DOS-Fassung bleibt als Rueckfall).
7. Drucker: ESCP24.DRV (ESC/P 24 Nadeln, ESC/P2) und PCL.DRV (HP PCL 3/5), Auswahl in Optionen.
Danach voller Testsatz und Release 2.4 (Karsten, 24.11.2026: Release erst nach PLAN_API).
