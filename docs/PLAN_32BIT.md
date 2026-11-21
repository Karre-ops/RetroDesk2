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

Stand 20.11.2026: `SysInfo` (API 2.3, 0x02F, in RDKX.MOD) und die 3D-Erkennung in RD2SETUP sind fertig
(RD2.INI [System] `3D=`, `3DPCI=Hersteller:Geraet`, Test `test/sysinfo.sh` mit QEMU ati-vga). Der Speicher
der Karte wird noch nicht ermittelt. Ebenfalls fertig: `BEDARF` in der .RC (rdrc.py und RDRC.EXE), `BedarfCheck` (0x030), Meldung beim Start in der Shell und graue Eintraege im Menue "RD2-Programme". RDSETUP mit `SETUP.INF` fertig (20.11.2026, Format im Kopf von apps/rdsetup/RDSETUP.C, Test `test/rdsetup.sh`); INSTALL und RD2SETUP lassen Programme mit `BEDARF CPU` ueber der vorhandenen CPU weg. Damit ist Schritt 1 abgeschlossen.
Auf Rechnern unter 386 kopiert die Installation keine 32-Bit-Teile (Karsten, 20.11.2026).

## Schritt 2: Vollbild, unterbrechbar (alle CPUs)
- API `ScreenAcquire(modus)` / `ScreenRelease`: eigener Modus (CGA, Mode 13h, Mode X, VESA mit LFB),
  Zeiger auf den Bildspeicher, Tasten roh, Klang/Timer/Netz weiter über RD2.
- Taskwechsel (Alt+Tab bzw. feste Taste in RDKEYB, auch im Rohmodus): RD2 schickt `M_SCREENLOST`,
  merkt Modus und Palette und schaltet auf den Schreibtisch. Zurück kommt `M_SCREENBACK`: RD2 stellt
  Modus und Palette her, das Programm zeichnet sein Bild aus eigenen Daten neu.
- Kein Abbild im RAM (spart Speicher). Ein Programm darf optional einen eigenen Puffer zum Zurückkopieren halten.
- Hinter dem Schreibtisch läuft das Vollbild-Programm angehalten weiter (keine Zeitscheiben, Klang stumm),
  damit Spiele nicht ohne Bild weiterlaufen.

Stand 20.11.2026: umgesetzt. `ScreenAcquire`/`ScreenRelease` (0x0E0/0x0E1, RDUSER), `M_SCREENLOST`/`M_SCREENBACK`,
in RD2.LIB `ScreenMode` (Text, CGA 4 Farben, EGA 0Dh, VGA 12h, 13h, Mode X 320x240, VESA 101h mit Fenster), `ScreenPalette`,
`ScreenRestore`, `ScreenBank`. Alt+Tab bzw. Alt+F6 parkt das Programm unten, RD2 zeichnet den Schreibtisch; Klick oder
Alt+Tab holt es zurueck. Waehrend des Vollbilds zeichnet RDGDI nichts auf den Schirm, alle Eingaben gehen an das Programm.
Test `test/vollbild.sh` (alle sieben Modi ausser Text, Wechsel hin und zurueck). Seit der Speicherrunde haelt RD2 das
geparkte Programm selbst an (`TaskHalt`, API 0x031: keine Zeitscheiben, Klang still, bis es wieder vorn ist).

## Speicherrunde (20.11.2026, zwischen Schritt 2 und 3)
- SysInfo und BedarfCheck liegen jetzt in RDINFO.MOD (verwerfbar, Diskette 1): RDKX hat wieder 1,4 KB Luft.
- `TaskHalt` im Kern (API 0x031) fuer das geparkte Vollbild.
- Grundregel: neue Funktionen nicht mehr in RDKX, RDGDI, RDCTL, RDSND, SHELLX (fast ohne Luft), sondern in neue
  verwerfbare Module; 32-Bit-Teile nur in RD32.MOD und auf einer eigenen 1,44-MB-Diskette "RD2 32" (nur ab 386 verlangt).
- Treiber von Diskette 2 auf 3: zurueckgestellt. Im Diskettenbetrieb laedt RDGDI den Grafiktreiber vom Laufwerk mit
  RD2, das muss vorher geprueft werden.

## Schritt 3: 32-Bit-DOS-Programme (ab 386)
- DOS/4GW, DJGPP usw. laufen über RunDos (RD2 lagert sich aus). Testen, freien DPMI-Host beilegen
  (HDPMI32 oder CWSDPMI), RDKEYB/RDMAUS prüfen. Teil des Testsatzes aus PLAN_DOS Schritt 4.

Stand 20.11.2026: umgesetzt. 32-Bit-DOS-Programme laufen ueber RunDos, RD2 lagert sich aus und kommt zurueck.
Test `test/dos32.sh`: D32TEST (apps/test32, wcl386) mit DOS/4GW, DOS32A, PMODE/W und CauseWay (2 MB ueber 1 MB,
Textschirm, Maus, Tastatur, Rueckgabe 42) und ein DJGPP-Programm (DTOU.EXE aus djdev205) mit CWSDPMI, je in DOSBox
(386, 16 MB, XMS) mit RD2-Daten und Grafik danach unveraendert. Dazu QEMU mit echtem BIOS: RDKEYB DE (y ergibt z)
und RDMAUS (PS/2, Bewegung) funktionieren aus allen vier Extendern.
Beigelegt: CWSDPMI.EXE r7 (GPL, unveraendert, Quelle in QUELLEN.TXT) auf Diskette 3; INSTALL und RD2SETUP kopieren es
nur ab 386. DOS/4GW, DOS32A usw. bringen die Programme selbst mit. HDPMI32 entfaellt (CWSDPMI reicht).
Offen: DJGPP-Programme finden CWSDPMI im eigenen Ordner oder ueber PATH; ohne PATH-Zeile in der AUTOEXEC.BAT
setzt die Hauptumgebung keinen PATH auf den RD2-Ordner. Die 1,44-MB-Diskette "RD2 32" kommt mit Schritt 4 (RD32.MOD).

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

Stand 20.11.2026: Stufe 4a umgesetzt. RD32.MOD (src/rd32, ab 386, BEDARF CPU 386) meldet RD2 beim ersten
RD2/32-Programm als 32-Bit-DPMI-Client an (CWSDPMI -p muss vor RD2 geladen sein) und wechselt danach nur roh
(DPMI 0306h) zwischen Realmodus und 32-Bit-Code, ohne Verschachtelung im Host. Je Programm ein Taskblock unter 1 MB
(3,4 KB: Stapelzeiger beider Seiten, Aufrufwoerter, Rueckrufdaten, Austauschpuffer); Bild und Stapel im DPMI-Speicher
(XMS), Code-/Datenselektor mit Bildbasis und 4 GB Grenze (lineare Adressen ueber rd32_lin). API-Aufrufe gehen ueber
call far [api + nr*4] auf dem Stapel des 16-Bit-Vorspanns, Fensterprozeduren (vier Klassen je Programm) kommen ueber
Thunks zurueck in den 32-Bit-Code. Weil der Kern nur bei Code im eigenen Programmbild umschaltet, laufen 32-Bit-Tasks
kooperativ. Der Kern laesst bei vorhandenem DPMI-Host 20 KB DOS-Speicher fuer die Seitentabellen frei
(RD2.INI [RD32] DosPlatz=, Paragraphen). SDK: sdk/RD2_32.H, RD32C0.ASM, RD32LIB.C, RD32STUB.C, tools/rd32link.py
(Bild als Ressource 11 im 16-Bit-Vorspann), build.sh app32. Beispiel apps/test32/VOLL32.C, Test test/rd32.sh.
Diskette 5 "RD2 32" (1,44 MB) mit RD32.MOD, VOLL32 und SDK32. Installation (20.11.2026): INSTALL und RD2SETUP
fragen ab 386 nach Diskette 5 (auslassbar), RD2_HDD.ZIP enthaelt sie mit; ab 386 schreiben beide
INSTALL=...\CWSDPMI.EXE -p -s- in CONFIG.SYS. Damit findet auch DJGPP (Schritt 3) den Host ohne PATH-Zeile.
Stand 21.11.2026: Stufe 4b umgesetzt. RD2.H gilt auch fuer 32 Bit (RD2_32.H ergaenzt nur das 32-Bit-Eigene).
tools/mk32api.py erzeugt aus RD2API.H die Aufrufstuecke fuer 207 Funktionen (RD32API.OBJ): Texte und Strukturen
kommen in den Austauschpuffer, grosse Puffer (FileRead/FileWrite bis 64 KB) in einen kurzen festen Block, Daten unter
1 MB (MemLock) gehen direkt; Zeiger-Rueckgaben (StrGet, MemLock) werden 32-Bit-Zeiger. Von Hand (RD32LIB.C):
RegisterClass, DialogRun/DialogIndirect (vier Dialogprozeduren je Task, Thunks wp4..wp7), FileDialog, PrnOpen,
MemFixedFree, ModProc (RD_ERR_NOTSUP). Neu nur in 32 Bit: rd32_alloc/rd32_free (DPMI-Speicher, am Ende frei),
Puffer-Bitmap BufCreate/BufShow/BufFree (8-Bit-Pixel, Geraeteformat 1/4/8 Bit, Streifen bis 64 KB), ScreenPtr,
SM_LFB (VESA 2.0 linear 640x480, 256 Farben, DPMI 0800h). Mit -zm und eliminate kommt nur Benutztes ins Bild
(FEN32 9 KB). Beispiel apps/test32/FEN32.C: Zeichnen, Puffer-Bitmap, Zeitgeber, Dialog mit 32-Bit-Dialogprozedur,
100 KB Datei, Profil, zweiter 32-Bit-Task zugleich; test/rd32.sh prueft alles (VESA-Farbe, LFB).
Hinweis: Fensterklassen gehoeren einem Task, eine zweite Instanz braucht einen eigenen Klassennamen.
Stand 21.11.2026 (Abschluss Schritt 4): Linken unter DOS mit RDRC ... /32:BILD.B32 (byte-gleich mit
tools/rd32link.py, test/rdrc.sh). Den Speicher bis Ende BSS traegt jetzt der Linker in den Kopf ein (Segment
RD32ENDE hinter _BSS in RD32C0), die Map-Datei wird nicht mehr gebraucht; RD32STUB.OBJ liegt im SDK32.
Abmelden: RD32.MOD bleibt nach der Anmeldung geladen; am Ende von RD2 (Export 1) ruft es im PM INT 21h/4Ch,
der Host raeumt den Client ab, ein kurz gesetzter INT-21h-Vektor faengt das 4Ch ab, RD2 endet normal. Zweimal RD2
hintereinander mit 32-Bit-Programm geprueft (test/rd32.sh). Beim Abmelden sichert RD32 PSP-Adressen und INT 22h-24h
und stellt sie wieder her. CWSDPMI -p behaelt beim ersten Client einen DOS-Block (Seitentabellen, ca. 4,5 KB) dauerhaft;
DOS ordnet ihn RD2 zu und gab ihn am Ende von RD2 frei (zweiter Lauf: MCB-Kette zerstoert). RD32 gibt neu angelegte
Bloecke des Hosts deshalb dem System (Eigentuemer 8). Ab dem zweiten Lauf bleibt der freie Speicher gleich (rund
23 KB weniger als im ersten Lauf, weil der Dauerblock in der Reserve liegt). RD32.MOD 4,5 KB (Grenze 5 KB, Diskette 5).

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

Stand 21.11.2026: Stufe 5a umgesetzt (Software). sdk/RD3D.H/RD3D.C (RD3D.LIB, nur in Programmen, die es benutzen):
Dreiecke mit Z-Puffer (16 Bit), affiner Textur (2^n), Gouraud, Nebel nach Tiefe, Alpha als 50-%-Raster, Doppelpuffer,
ganzzahlig 16.16 (kein 387 noetig, 64-Bit-Zwischenwerte ueber clib3r). Farben: 16 Rampen zu 16 Helligkeiten, Helligkeit 0
= Nebelfarbe. Ziele: Vollbild 13h, VESA linear 640x480, Fenster (Puffer-Bitmap, Abbildung auf die feste Systempalette
VESA/VGA16/mono). Treiber austauschbar (R3DDRV), jetzt nur "Software". Hilfen: Drehung, Verschiebung, Projektion.
Beispiel apps/test32/WUERFEL.C mit Selbsttest (Z, Alpha, Nebel), test/rd3d.sh (4 Darstellungen).
Entscheidung Karsten 21.11.2026: Glide (Stufe 5b) wartet auf den Treiber-Stack.

## Zeitscheiben und Absturzschutz (Stand 21.11.2026)
- Zeitscheiben: RD32 setzt beim Anmelden einen eigenen PM-Zeitgeber (DPMI 0205h, INT 8). Steht ein 32-Bit-Task in seinem
  eigenen Code (hinter RD32C0, Kopf +36), biegt er alle 4 Takte (ca. 55 ms) die Rueckkehr auf pm_preempt im Bild um
  (Kopf +32): der Task ruft dort TaskYield und macht danach an der alten Stelle weiter (TB_PREEIP, laufender Aufruf
  gesichert). CWSDPMI legt vor den Rahmen des unterbrochenen Codes einen eigenen; RD32 erkennt beide Lagen.
  RD2 /KOOP: SYSINFO.modus traegt RD_MODE_KOOP, dann auch 32 Bit nur kooperativ.
- Absturzschutz: Ausnahmen 0, 6, 0Ch, 0Dh, 0Eh (DPMI 0203h). Im Code eines 32-Bit-Tasks: Rahmen auf ex_kill umbiegen,
  im Realmodus auf dem Stapel des Tasks DPMI-Speicher und Taskblock frei, Meldung, TaskKill. Sonst an den alten Handler.
  Kein voller Speicherschutz: wegen des flachen Modells mit Umlauf kann ein Programm fremden Speicher beschreiben.
- Vor dem Abmelden gibt RD32 Zeitgeber und Ausnahmen an den Host zurueck (sonst scheitert der naechste RD2-Lauf).
- Beispiel apps/test32/SCHUTZ32.C (Endlosschleife ohne API, Division durch 0, Seitenfehler), test/rd32.sh.
- Stufe 3 (DOS-Programme parallel per V86): eigener Plan docs/PLAN_V86.md, nach dem Treiber-Stack.

## C-Laufzeit RD32C.LIB (Stand 21.11.2026)
- Header in sdk/rd32c (SDK32\RD32C): stdio, stdlib, string, ctype, setjmp, time; math.h, stdarg.h usw. von Open Watcom.
  Uebersetzen mit -fpc -i=sdk/rd32c -i=sdk, linken mit RD32C.LIB, math3r.lib, clib3r.lib (nur Gleitkomma- und Mathe-Module,
  ohne Watcoms Start und stdio: RD32C0 stellt __8087/__real87/__chipbug, RD32C ersetzt matherr und __get_std_stream).
- Gleitkomma: RD32.MOD setzt TB_FPU (Kopf +124) aus KernInfo, RD32C0 macht dann fninit und rechnet in Hardware, sonst Software.
- malloc: Bereiche aus rd32_alloc (64 KB, wachsend bis 1 MB, max. 12), Bloecke mit geradem Kopf, Freies beim Suchen zusammenlegen.
- stdio: Dateien ueber die RD2-API, Puffer 512 Bytes, Textmodus ohne b (CR weg, LF -> CR LF), stdout/stderr zeilenweise ins
  RD2-Protokoll, stdin leer. exit/atexit ueber setjmp in rd32_main; am Ende werden Dateien geschlossen und stdout geleert.
- Pruefprogramm apps/test32/TESTC32.C (35 Pruefungen, mit 387 und mit S fuer Software), test/rd32.sh.
- Fehlt noch: getchar/Konsole, Gebietsschema, wide chars, scanf auf Dateien, long long in printf.

## 32-Bit-Module (Stand 21.11.2026)
- Format .M32 (tools/rd32mod.py): Bild ab Offset 0 mit Kopf aus RD32M0 ("R32M"), dahinter Exporte (Offset, Name) und
  Verschiebungen. Das Werkzeug linkt zweimal (Offset 0 und 1 MB); DWORDs mit genau 1 MB Unterschied sind Verschiebungen.
- rd32_modload (RD32PRG.C) laedt in rd32_alloc-Speicher des Programms, verschiebt um die Ladeadresse und fuellt die Tabelle
  im Kopf: Bruecke (rd32_call), rd32_alloc/free, ScreenMode/Ptr/Palette/Restore, int31, dazu Taskblock, Bildbasis,
  jmp_buf und 387-Merker. Damit teilen sich Programm und Module Speicherliste und Vollbild-Zustand.
- Austauschpuffer-Zeiger (rd32_xp, TB +126) und Rueckruf-Prozeduren (TB_XBUF + 2976) liegen im Taskblock, damit Fenster-
  und Dialogprozeduren im Modul ueber den Verteiler des Programms laufen. RD32LIB ist geteilt: RD32LIB (Programm und Modul),
  RD32PRG (nur Programm). Zeitscheiben und Absturzschutz gelten auch im Modulcode (CS des Programms, oberhalb RD32C0).
- Pruefung TESTMOD/TESTM32 (12 Pruefungen) in test/rd32.sh.
- Offen: Module unter DOS bauen (RDRC), Module, die Module laden, gemeinsamer Code mehrerer Programme.

## VESA-Modi (Stand 21.11.2026)
- ScreenModeVesa(w, h, bpp, si) sucht in der VBE-2.0-Modusliste (4F00/4F01) einen linearen Modus mit passender Groesse und
  Farbtiefe (8, 15, 16, 24, 32; 15 auch als 16 Bit mit 5 Bit Gruen) und setzt ihn mit 4F02 | 4000h. ScreenVesaList
  liefert alle linearen Modi. SM_LFB in ScreenMode ist jetzt 640x480x8 ueber denselben Weg.
- Bildspeicher: DPMI 0800h, mindestens 3 MB auf einmal (DPMI 0.9 kann Abbildungen nicht freigeben), neu nur bei anderer
  Adresse oder groesserem Bedarf. VBE 3: Zeilenbytes linear (+32h). ScreenRestore setzt denselben Modus, Palette nur bei 8 Bit.
- Module erreichen beides ueber den Kopf (+72, +76). Pruefung VESA32 in test/rd32.sh (QEMU: 64 lineare Modi).
- Offen: RD3D nur 8 Bit (13h, 640x480); RD3D in 800x600/1024x768 und in 16/32 Bit, Bankumschaltung ohne LFB (VBE 1.2).

## Reihenfolge und Aufwand
1. Bedarf, SysInfo, RDSETUP (klein bis mittel)
2. Vollbild unterbrechbar (mittel)
3. 32-Bit-DOS-Programme testen (klein)
4. RD2/32: 4a Vollbild (groß), 4b Fenster (groß)
5. RD3D: Software, dann Glide Voodoo 1 bis 4 (groß), danach ATI und nVidia (sehr groß, ungewiss)
