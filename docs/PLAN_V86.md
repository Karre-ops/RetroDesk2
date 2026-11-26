# PLAN_V86: RD2V, 32-Bit-Unterbau ab 386 (Windows-95-Weg)

Stand 26.11.2026, Entscheidungen Karsten:
- Weg wie Windows 95: Ab 386 liegt unter RD2 ein eigener 32-Bit-Kern **RD2V** mit eigenen Treibern. Dateien und
  Platten gehen dann nicht mehr durch DOS und BIOS. Ziele: mehr Leistung, lange Dateinamen, DOS-Programme
  parallel in Fenstern.
- **Eigene INT-21h-Übernahme ab 386 freigegeben** (26.11.2026). Damit gilt die Entscheidung vom 21./22.10.
  („keine eigene INT-21h-Nachbildung“) nur noch für 8088 und 286.
- Eigener Kern unter GPL. Jemm386 (teils Artistic License) und HDPMI (nur „Freeware“) werden nicht verwendet.
- 8088 und 286 bleiben unverändert beim heutigen Weg über DOS, das Multitasking dort auch. Die RD2-API bleibt für
  alle Rechner gleich; nur der Unterbau darunter wechselt.

## Rahmen
- RD2V ist eine eigene Datei (`RD2V.EXE`, 32 Bit, NASM und Open Watcom C), nur auf 386-Installationen. RD2.EXE
  bekommt nur einen kleinen Einsprung (im Kern sind 26 Bytes frei, der Rest kommt in ein Modul).
- Start beim Hochfahren von RD2, wenn `RD2.INI [System] RD2V=1` und mindestens 4 MB Speicher da sind. Ohne RD2V
  läuft alles wie heute (auch als Notweg bei Problemen). Kein Umschalten im laufenden Betrieb, weil dann zwei
  Dateisystem-Treiber gleichzeitig Puffer hätten.
- Speicher für RD2V, Cache und VMs kommt aus XMS. In den 640 KB jeder VM soll fast nichts verloren gehen.
- EMM386, QEMM und andere V86-Monitore werden erkannt. Dann startet RD2V nicht und meldet es (VCPI später).
- Dateisystem-Integrität vor Tempo: feste Schreibreihenfolge (Daten, FAT, Verzeichnis), Cache mit Durchschreiben
  als Vorgabe, Abgleich mit DOS beim Übernehmen und beim Beenden (Puffer schreiben, Laufwerke zurücksetzen).

## Folgen für bestehende Programme (Durchsicht 26.11.2026)
Durchgesehen: RDBILD, RDBURG, RDCALC, RDQRCODE, RDTRON (RD2-Fassung), RDISOENGINE, RDWerkzeugbau, RDDOOM.
- Kein RD2-Programm ruft INT 21h, INT 13h oder INT 31h selbst auf oder setzt Interruptvektoren; alle gehen über
  die RD2-API. RD2/32-Programme (RDISOENGINE, RDWerkzeugbau) nutzen nur die RD32-Laufzeit. Fester Bildspeicher
  oder BIOS-Daten über feste Adressen: keine Fundstellen.
- Direkte Ports: nur RDDOOM (geplant, Link-Adapter nach B004-Norm ab 150h, Port aus RD2.INI) und dessen
  DOS-Werkzeug LINKTEST.COM. Die DOS-Fassungen von H-TRON im RDTRON-Repo (Tastatur über INT 9 und Port 60h) sind
  DOS-Programme und laufen wie andere DOS-Programme.
- Regeln daraus für RD2V: In der System-VM sind alle Ports frei außer denen, die RD2V selbst nachbildet (PIC,
  Zeitgeber, Tastatur, ab Teil 3 Platten-Controller); das erste MB der System-VM ist eins zu eins abgebildet
  (Bildspeicher, BIOS, DMA-Puffer). Damit laufen alle Programme unverändert, ohne neu gebaut zu werden.

## Plan-Teile (je Teil Tests gesammelt am Ende)
1. **RD2V als DPMI-Host für RD2/32.** Lader aus RD2, GDT/IDT/TSS, Paging, Speicherverwalter über XMS,
   Ausnahmen. Die DPMI-Teilmenge, die RD32 nutzt: Einsprung 1687h, Selektoren (0000h, 0001h, 0003h,
   0007h bis 000Ah), Interrupts und Ausnahmen (0202h bis 0205h), Speicher (0501h, 0502h), rohe Umschaltung
   (0305h, 0306h), Speicherschutz zwischen 32-Bit-Programmen. CWSDPMI bleibt als Notweg.
   Prüfen: rd32, rd32mod, schutz32, rd3d und dos32 laufen unverändert mit RD2V.
2. **System-VM.** DOS, RD2 und 16-Bit-Programme laufen in VM 0 im V86-Modus. INT-Reflektion in die VM, IRQs an
   die VM, I/O-Bitmap gibt die Grafik- und Soundports frei (Treiber bleiben schnell). `RunDos` mit „Allein“
   beendet RD2V vorher.
   Prüfen: voller Testsatz mit `RD2V=1`.
3. **Dateien und Platten in RD2V.** Blockschicht zuerst über INT 13h im V86-Rückweg (läuft mit jeder Hardware),
   dann eigener ATA-Treiber (PIO, CHS und LBA) für die Festplatte; Diskette und CD vorerst über BIOS und MSCDEX.
   Eigener FAT12/16/32-Treiber mit Cache. Die RD2-Datei-API geht direkt an RD2V. INT-21h-Dateifunktionen aller
   VMs landen bei RD2V (DOS ist für Dateien nur noch Hülle); Netzlaufwerke und Unbekanntes gehen weiter an DOS.
   Prüfen: Dateitests, Installation, fsck.fat nach Schreiblast, Stromausfall-Test (QEMU hart beenden).
4. **Lange Dateinamen (VFAT).** In RD2V, neue RD2-API-Funktionen (API 2.5) für lange Namen, Shell und
   Dateidialog zeigen sie. DOS-Programme bekommen INT 21h 71xxh (wie Windows 95) und sonst die kurzen Namen.
   8088/286: später optional als Modul, Vorgabe 8.3.
5. **Eingabe und Zeit in RD2V.** Tastatur, Maus (PS/2 und seriell), Zeitgeber je VM virtuell; RDMAUS und der
   feste Takt (145,6 Hz) bleiben die Schnittstelle nach oben.
6. **DOS-Fenster.** Zweite VM mit Textmodus-Programm im Fenster (COMMAND.COM, EDIT), DOS-Instanzdaten je VM,
   mehrere VMs, Absturz beendet nur seine VM. Shell: „im Fenster“ oder wie bisher; Einstellungen je Programm in
   RD2.INI.
7. **Grafik im Fenster, EMS/XMS je VM, Sound Blaster** für die Vollbild-VM.
8. **Spieletests** (SimCity, Prince of Persia, Monkey Island) auf echter Hardware, Speicherverbrauch.

## Aufwand und Risiken
- Sehr groß, damit wird RD2 ab 386 ein eigenes 32-Bit-System mit DOS als Hilfsschicht. Teil 1 und 2 sind der
  Grundmechanismus; die Masse sind Treiber (Teil 3) und Sonderfälle echter DOS-Programme (Teil 6 und 7).
- Kosten im V86-Modus: Jeder INT-Aufruf von RD2 geht durch RD2V (geschätzt 5 bis 20 Prozent langsamer, bis Teil 3
  die häufigsten Aufrufe selbst erledigt). Gemessen wird nach Teil 2.
- Treiber für echte Hardware: eigenwillige Controller und Laufwerke lassen sich in QEMU nur teilweise prüfen;
  deshalb bleibt der BIOS-Rückweg für alles Unbekannte.
- Lange Dateinamen und alte DOS-Werkzeuge (CHKDSK, alte Defragmentierer) vertragen sich nicht. Hinweis in der
  Doku, RDFORMAT und RDFDISK sind sicher.
