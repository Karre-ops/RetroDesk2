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
besonders bei 640 KB. Offen zu klaeren: Netzkarten-Paketreiber (bisher bringt sie der Nutzer mit), Druckertreiber
ueber EPSON9 hinaus, Mindestausbau 512 KB pruefen.

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

## Schritte
1. Abgleich: Jede Funktion oben in 16 und 32 Bit mit einem Testprogramm pruefen (Vollbild + Maus + Tasten + Klang
   zugleich, Alt+Tab). Ergebnis: genaue Luecken-Liste. Die Engine (RDISOENGINE/Spritzwerkzeugbauer) als Pruefstein:
   was nutzt sie heute selbst (VESA 101h, INT 33h, kbhit, eigener 140-Hz-Zeitgeber, PC-Lautsprecher, AdLib)?
2. Zeitgeber: RD2 programmiert den PIT selbst schneller (Vielfaches von 18,2 Hz, Uhr bleibt richtig) und ruft
   angemeldete Rueckrufe (16 und 32 Bit) im gewuenschten Takt; SndMusicPlay nutzt ihn mit.
3. Eingabe im Vollbild: Maus mit Koordinaten im Vollbildmodus, Bereich und Empfindlichkeit, KeyState-Tabelle fuer
   alle Tasten zugleich, auch im Rohmodus.
4. Sound Blaster vollstaendig: SB 1.x/2.0/Pro/16/AWE und Kompatible; Mischer in RD2 (mehrere Samples zugleich,
   Lautstaerke je Kanal), 16 Bit/Stereo ab SB16, FM ueber OPL2/OPL3 mit Musik und Effekten gleichzeitig.
5. MPU-401 (MIDI) als Treiber; Tandy/Game Blaster/Gravis nativ spaeter (wie in Punkt 5 festgelegt).
6. SDK und Doku: alles in RD2.H/RD2_32.H, SDK.TXT, Beispiele; danach die Engine auf die RD2-API umstellen
   (DOS-Fassung bleibt als Rueckfall).
