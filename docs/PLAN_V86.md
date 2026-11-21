# PLAN_V86: DOS-Programme parallel in Fenstern (ab 386)

Stand 21.11.2026, Entscheidung Karsten: als eigener Plan, angefangen wird erst nach dem Treiber-Stack.
Das Multitasking auf dem 8088 (Mittelweg, siehe SPEC) bleibt unverändert; dieser Plan betrifft nur 386 und neuer.

## Ausgangslage
- RD2 läuft im Realmodus auf DOS. DOS-Programme laufen heute einzeln: RD2 lagert sich aus (RDSWAP), das
  Programm hat den Rechner für sich, danach kommt RD2 mit allen Fenstern zurück.
- Ab 386 gibt es RD2/32 (PLAN_32BIT): 32-Bit-Programme über einen DPMI-Host (CWSDPMI -p), mit Zeitscheiben
  (RD32-Zeitgeber im PM) und Absturzschutz (Ausnahmen beenden nur das Programm). DOS-Programme profitieren davon nicht.
- Ein DPMI-Host stellt Clients keine V86-Aufgaben bereit. Für DOS-Fenster braucht RD2 einen eigenen V86-Monitor,
  also einen eigenen 32-Bit-Kern, der selbst Host ist (wie Windows 386 Enhanced Mode oder DESQview mit QEMM).

## Ziel
Mehrere DOS-Programme gleichzeitig, jedes in einer virtuellen Maschine (VM) mit eigenen 640 KB, im Fenster oder im
Vollbild, präemptiv umgeschaltet, ein Absturz beendet nur seine VM. RD2-Programme (16 und 32 Bit) laufen weiter.

## Bausteine
1. **RD2V-Kern (32 Bit, Ring 0):** GDT/IDT/TSS, Paging, eigener Speicherverwalter über XMS. Startet aus RD2
   (statt CWSDPMI) und übernimmt die DPMI-Dienste für RD2/32 selbst (0.9-Teilmenge, die RD32 heute nutzt).
2. **System-VM:** DOS, RD2 und alle 16-Bit-Programme laufen in VM 0 im V86-Modus weiter wie bisher.
3. **V86-Monitor:** privilegierte Befehle (CLI/STI, INT, IN/OUT, HLT) abfangen und nachbilden; IOPL und I/O-Bitmap
   je VM; INT-Weiterleitung in die VM.
4. **Virtuelle Geräte (VxD-artig, je Gerät ein Baustein):**
   - Bildschirm: Textmodus und CGA/EGA/VGA-Grafik je VM in einen Schattenpuffer, Anzeige im RD2-Fenster
     (Text zuerst, Grafik danach; Vollbild-VM bekommt die echte Karte).
   - Tastatur (Port 60h/64h, INT 9), Maus (INT 33h über RDMAUS), Zeitgeber (Port 40h-43h, INT 8 je VM).
   - PIC, DMA (Diskette, Sound Blaster), Sound Blaster zunächst nur für die Vollbild-VM.
5. **DOS mehrfach:** DOS ist nicht wiedereintrittsfest. Wie Windows: DOS-Daten je VM (Instanzdaten: SDA, Listen,
   aktueller PSP), Umschalten nur außerhalb von DOS (InDOS/kritischer Fehler) oder mit Sperre. Dateien aller VMs
   gehen durch dasselbe DOS.
6. **EMS/XMS je VM:** XMS über den Kern nachbilden, EMS per Paging (wie EMM386) für Programme, die es brauchen.
7. **RD2-Seite:** Shell startet ein DOS-Programm wahlweise "im Fenster" (neue VM) oder wie bisher ausgelagert.
   Fenster "DOS" mit Menü (Vollbild, Einfügen, Beenden), Einstellungen je Programm (PIF-artig in RD2.INI).

## Reihenfolge (jeweils eigener Plan-Teil, mit Tests am Ende)
1. RD2V-Kern als DPMI-Host für RD2/32 (CWSDPMI wird optional). Prüfen: alle RD2/32-Tests laufen unverändert.
2. System-VM: RD2 selbst läuft in V86. Prüfen: voller Testsatz in V86.
3. Zweite VM mit Textmodus-DOS-Programm im Fenster (COMMAND.COM, EDIT), Tastatur, Zeitgeber.
4. DOS-Instanzdaten, mehrere VMs, Absturz einer VM.
5. Grafik im Fenster (CGA/EGA/VGA-Schatten), Maus.
6. EMS/XMS je VM, Sound Blaster für die Vollbild-VM.
7. Spieletests (SimCity, Prince of Persia, Monkey Island), Speicherverbrauch.

## Aufwand und Risiken
- Sehr groß, ein Vielfaches von PLAN_32BIT. Der Grundmechanismus ist überschaubar, die Masse sind Sonderfälle
  echter DOS-Programme (direkte Hardwarezugriffe, Timing, eigene Speichermanager, DOS-Extender in einer VM).
- Konflikte mit anderen Speichermanagern (EMM386, QEMM): RD2V muss sie erkennen und ablehnen oder VCPI nutzen.
- Testen: QEMU für den Kern; Spiele und Soundkarten nur in 86Box und auf echter Hardware.
- Speicher: Kern und VM-Verwaltung liegen im XMS; in den 640 KB jeder VM soll fast nichts verloren gehen.
