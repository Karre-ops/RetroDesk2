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
3. Eingabe im Vollbild (erledigt 24.11.2026): ScreenMouse(w, breite, hoehe) (API 0x0E2, RDUSER) setzt den Mausbereich
   nach dem Moduswechsel; im Vollbild bleibt die Maus an (gpause 2: Zeiger aus statt Treiber zuruecksetzen), Nachrichten
   kommen mit Vollbild-Koordinaten. KeyState(KS_SCAN + Scancode) fuer alle Tasten zugleich: Bit-Tabelle k_tasten im
   INT 9 des Kerns (rd_api[RDO_TASTEN]). Test test/vollmaus.sh. RDGDI 20.986/20.992 Byte RAM, RDUSER 27.638/27.648.
3a. Audit (erledigt 24.11.2026, main 6e281c1): Bericht docs/AUDIT_2026-11-24.md, Tests der betroffenen Teile gruen.
3b. Speicherschutz RD2/32 (erledigt 24.11.2026; Entscheidung Karsten: Mittelweg). Nachweis in Bochs: test/schutz32.sh
   (Schreiben unter das Bild -> Ausnahme 0Dh, nur das Programm endet, Speicher zurueck; QEMU prueft keine Grenzen,
   rd32.sh erwartet dort 0Dh oder 0Eh). Umgesetzt wie unten, ScreenCopy entfiel (Schattenpuffer + ScreenUpdate genuegt).
   Planung: Heute: CS/DS/SS mit Basis = Bild und Grenze
   4 GB, rd32_lin(lin) = lin - Basis erreicht jeden Speicher (Bildspeicher, BIOS, DOS, andere Programme).
   Neu, Vorgabe fuer native Programme:
   - DS/SS-Grenze = Ende des hoechsten eigenen Blocks (Bild + rd32_alloc), wird bei jedem rd32_alloc nachgezogen
     (DPMI 0008h). Alles unter dem Bild (DOS, BIOS, A0000h, 16-Bit-Programme) liegt damit ausserhalb: Zugriff
     ergibt eine Ausnahme, die nur das Programm beendet. CS-Grenze = Codeende.
   - Bloecke unter der Bildbasis gehen nicht (naher Zeiger waere negativ): rd32_alloc gibt sie zurueck und fordert
     neu an; geht es nicht, Fehler statt Schutzluecke. Speicher anderer Programme oberhalb bleibt erreichbar
     (Schutz gegen Versehen, nicht gegen Absicht).
   - Fenster als eigene Selektoren (DPMI 0000h/0007h/0008h): Bildspeicher A0000h-BFFFFh, BIOS (0-4FFh und
     F0000h-FFFFFh), LFB (Grenze = abgebildete Groesse). ScreenPtr/BiosPtr liefern dann Far-Zeiger
     (BYTE __far *, Open Watcom 48 Bit); ScreenCopy(quelle, x, y, w, h) als schnelles Kopieren ins Bild.
     RD3D und Schattenpuffer (Bankumschaltung) kopieren ueber ES, die Rechenschleifen bleiben nah.
   - Flag RD32F_VOLL im Programmkopf: alte Grenze 4 GB, rd32_lin wie bisher (Treiber, Diagnose).
     Ohne Flag liefert rd32_lin 0 und setzt den Fehler.
   - Ausnahme 0Dh/0Ch ausserhalb der Grenze: Meldung "Speicherschutz" mit Adresse, nur dieses Programm endet.
   Feinplan (24.11.2026, nach Code-Durchsicht): Unter dem Bild liegen auch Taskblock (TB), MemLock-Speicher und
   StrGet-Texte. Daher ein Selektor LOW (Basis 0, Grenze 10FFEFh, global wie g_flat) fuer alles unter 1 MB:
   TB-Makros und RD32C0 ueber GS (Open Watcom laedt GS fuer __far selbst, gs: in RD32C0, pm_preempt sichert GS),
   rd32_fp2p liefert __far; StrGet kopiert in einen nahen Ringpuffer; MemLock/MemFixed liefern __far; BiosPtr() __far.
   Bildschirm bleibt ein naher Zeiger (quelltextvertraeglich): LFB oberhalb des Bildes -> Grenze wird bis LFB-Ende
   nachgezogen (der Speicher unter dem Bild bleibt trotzdem ausserhalb, da Offsets dorthin ueber 4 GB laufen);
   13h, Textmodus und LFB unter dem Bild -> Schattenpuffer, ScreenUpdate kopiert ueber LOW/LFB-Selektor.
   Kopf +12: Byte 1 = Schutz-Format (neue RD32C0), | 2 = Vollzugriff (RD32C0V.OBJ); +14 Wort = LOW (traegt RD32 ein).
   Alte Programme (+12 = 0) laufen unveraendert mit 4 GB. Module ab Fassung 3 (+84 LOW).
   Umbau: RD32A (Selektoren, Grenze nachziehen), RD32PRG/RD32LIB (Zeiger, rd32_alloc), RD3D, VOLL32/TAKT32 und
   Testprogramme; neuer Test test/schutz32.sh (Zugriff unter das Bild, nach A0000h ohne Zeiger, mit ScreenPtr, mit Flag).
4. Sound Blaster vollstaendig: SB 1.x/2.0/Pro/16/AWE und Kompatible; Mischer in RD2 (mehrere Samples zugleich,
   Lautstaerke je Kanal), 16 Bit/Stereo ab SB16, FM ueber OPL2/OPL3 mit Musik und Effekten gleichzeitig.
   Erledigt 24.11.2026: SB.DRV erkennt die DSP-Fassung (E1h): 1.x Einzelbloecke, ab 2.00 Ring (auto-init), Pro Stereo
   (Mischer 0Eh, High-Speed), SB16/AWE Bxh/Cxh mit 41h und 16 Bit ueber H (RD2.INI SB=... H5); Mischerregister fuer
   SndVolume (art 1 PCM, 2 gesamt); OPL3-Modus mit 18 FM-Kanaelen (9..17 fuer Effekte neben der Musik). Neu RDMIX.MOD
   (beim ersten Aufruf geladen): 8 Kanaele, 8/16-Bit-Samples mit eigener Rate, Lautstaerke, Lage, Schleife; Ausgabe
   je Karte, MixRate=. API 127h-12Bh SndMixPlay/Set/Stop/Out/State, M_SOUNDDONE p1 = 4. SDK.TXT Abschnitt 5c.
   Test test/mischer.sh: QEMU-SB16 mit Tonaufnahme (440 und 1000 Hz zugleich, Fourier), DOSBox sb1/sb2/sbpro2.
   RDSND 5 KB (war 4), SB.DRV 5 KB (war 3).
4b. Luecken (erledigt 24.11.2026): VESA fuer 16-Bit-Programme in RD2.LIB (VESA16.C: ScreenModeVesa, ScreenVesaList,
   ScreenPut; ScreenBank in 64 KB bei jeder Granularitaet, ScreenRestore fuer VESA-Modi), test/vesa16.sh. Gesamttest
   test/gesamt.sh (GESAMT.EXE/GESAMT32.EXE aus einer Quelle: Vollbild, Maus, zwei Tasten, Takt, Mischer, Alt+Tab; 32 Bit
   neunmal Kind mit TaktSet per TaskKill: RD32-Plaetze werden frei, behoben schon im Audit ueber RDO_TASKENDE).
   Mindestausbau test/min512.sh (KB512.COM: DOS mit 512 KB; PING, STRESS, Mischer, Schreibtisch). Audit-Rest siehe
   AUDIT Abschnitt 6 (FPU mit EM, MemFixedFree, DirGet). API 2.4.
5. MPU-401 (MIDI) als Treiber (auf Halt, Karsten 24.11.2026); Tandy/Game Blaster/Gravis nativ spaeter (wie in Punkt 5 festgelegt).
6. SDK und Doku: alles in RD2.H/RD2_32.H, SDK.TXT, Beispiele; danach die Engine auf die RD2-API umstellen
   (DOS-Fassung bleibt als Rueckfall).
   Erledigt 24.11.2026: SDK.TXT (Takt, Vollbild-Maus, Mischer, Drucken, Joystick, FM, Hilfe) und SDK_EN.TXT vollstaendig;
   Diskette 4 mit Beispielen zu Gesamttest, Vollbild, VESA, Maus, Takt, Mischer, Drucken (Pascal-Teil als PASCAL.ZIP),
   Diskette 5 mit RD32C0V.OBJ und GESAMT.C. Neue API fuer Spiele (Abschnitt 5h): SndMusicMem 12Ch (IMF/DRO aus einem
   Block) und SndTonePlay 12Dh (Tonfolge hz/ms) - Geraeusche vorab berechnet, RD2 spielt sie; im Takt-Rueckruf bleibt
   die API verboten (32-Bit-Rueckruf laeuft im geschuetzten Modus). Alt+Tab holt ein allein geparktes Vollbild-Programm
   zurueck (RDUSER). RDISOENGINE src/rd2/rdrd2.c: Grafik, Maus, Tasten, Takt und Klang nur ueber die RD2-API (SndCaps:
   Sound Blaster -> Mischer, FM -> SndMusicMem, sonst SndTonePlay); Spritzwerkzeugbauer als RD2/32-Programm,
   Test RDWerkzeugbau test/rd2_spiel.sh (QEMU 486, SB16 + AdLib, alle drei Klangwege, Alt+Tab).
7. Drucker: ESCP24.DRV (ESC/P 24 Nadeln, ESC/P2) und PCL.DRV (HP PCL 3/5), Auswahl in Optionen.
   Erledigt 24.11.2026: ESCP24.DRV (ESC * 39, 180 x 180 dpi; Entwurf ESC * 33, 120 x 180; ESC J n/180) und PCL.DRV
   (300 dpi, Entwurf 150; A4, Raster mit PackBits ESC *b2M, leere Zeilen ESC *b0W, ESC *rB, ESC E). RDPRN zeichnet Seiten
   ueber 1024 Punkte Breite in Kacheln (RDGDI-Grenze) und setzt das Band zusammen. OPTIONEN Karte 7 "Drucker" (Dialog
   18): Treiber, Anschluss LPT1/LPT2/Datei, Entwurf -> RD2.INI [Drucker]. Tests: drucken.sh (Decoder test/escp24.py,
   test/pcl.py, Seite liegt in Zoll wie bei EPSON9), drucker_opt.sh.
Danach voller Testsatz und Release 2.4 (Karsten, 24.11.2026: Release erst nach PLAN_API).
