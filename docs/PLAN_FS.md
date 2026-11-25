# PLAN_FS: Dateisystem (FAT32 ab 386, Formatieroptionen)

Stand 25.11.2026, Vorgaben Karsten:
- Formatieroptionen im Setup (INSTALL) und in allen Formatier-Menues (RDFORMAT, Laufwerksmenue der Shell).
- FAT32 ist nur bei erkanntem 386 waehlbar, sonst FAT12/FAT16 wie bisher. Der 8088 und der XT-360-KB-Satz
  bleiben unveraendert (FreeDOS-Kern 8086/FAT16).
- FAT16 oder FAT32 haengt am Partitionstyp. Wechsel automatisch: Partitionstyp aendern, neu starten,
  danach automatisch weiter formatieren (und bei INSTALL weiter installieren).
- Kerne: Die 386-Medien (RD2_386A, CD, USB, RD2_HDD.ZIP) starten mit FreeDOS 2046 8086/FAT32 (laeuft auch
  auf 8088/286, sieht FAT32-Partitionen). Auf der Festplatte bleibt der FAT16-Kern Vorgabe; der FAT32-Kern
  kommt nur bei FAT32 dorthin.
- RDFDISK nutzt ab 386 LBA (INT-13h-Erweiterungen, Typ 0Ch/0Eh/0Fh) fuer Platten ueber 8 GB, ohne LBA-BIOS
  automatisch CHS (nur bis 8 GB).

## Schritte
1. Kerne und SYS: ke2046_86f32 als Startkern der 386-Medien, ke2046_86f16 dort zusaetzlich als
   \RD2\K16\KERNEL.SYS fuer FAT16-Installationen. RDSYS mit FAT32-Bootsektoren (boot32.asm CHS,
   boot32lb.asm LBA, WITHFAT32). XT-Satz unveraendert.
2. RDFDISK: Anlegen-Dialog ab 386 mit Dateisystem FAT16/FAT32 (FDISK-Kern: flags.fat32, Version W95B:
   LBA-Typen nur wenn noetig). Ohne 386 unveraendert (DOS-6-Typen).
3. RDFORMAT: Festplatte ab 386 mit Wahl FAT16/FAT32 (Vorgabe: jetziger Typ). Weicht die Wahl ab, aendert
   RDFORMAT den Typ im Partitionseintrag (MBR/EMBR, vorher gesichert), schreibt einen Merker "RD2FMT" in
   Sektor 2 der Platte (Zylinder 0, Kopf 0; frei vor der ersten Partition, geht auch von CD) und startet neu.
   Grenzen: FAT16 hoechstens 2 GB, FAT32 mindestens 33 MB; sonst Hinweis auf RDFDISK.
4. Fortsetzen nach dem Neustart: RD2SETUP (Startmedium) bzw. SHELL (installiertes RD2) findet den Merker,
   loescht ihn und startet RDFORMAT bzw. INSTALL mit den gemerkten Angaben ohne neue Rueckfrage.
5. INSTALL: Dateisystem-Wahl neben "C: vorher formatieren" (ab 386), gibt sie an RDFORMAT weiter; RDSYS mit
   dem passenden Kern (FAT32: Startkern, FAT16: \RD2\K16).
6. Shell-Laufwerksmenue "Formatieren" startet RDFORMAT wie bisher (Wahl im Dialog von RDFORMAT).
7. Tests (gesammelt am Ende): QEMU 386 mit 20-MB- und 4-GB-Platte: FAT16->FAT32 mit Neustart, FAT32->FAT16,
   Installation auf FAT32 bis zum Start von Festplatte (fsck.fat), RDFDISK FAT32/LBA auf 10-GB-Platte,
   XT-Satz unveraendert (kette.sh, satz.sh).

## Stand 25.11.2026: fertig, alle Schritte getestet
- Schritt 1: src/disk/fdkern/F32/KERNEL.SYS (ke2046_86f32), mkverteil.sh: Startkern FAT32 auf RD2_386A, CD (2,88-MB-Abzug)
  und USB, FAT16-Kern als \RD2\K16\KERNEL.SYS. RDSYS mit -DWITHFAT32 (boot32.asm, boot32lb.asm aus ke2046s.zip).
- Schritt 2: fdglue.c fg_386/fg_fat32; RDFDISK fragt ab 386 beim Anlegen "mit FAT32?" (Nein vorgewaehlt, Text 25).
  INT-13h-Erweiterungen erst ab 386 (vorher auf jedem Rechner abgefragt).
- Schritt 3: RDFORMAT Dialog 12 mit FAT16/FAT32 (ab 386, wenn die Partition gefunden wird; FAT32 nur mit FAT32-Kern
  und ab 129 MB, FAT16 bis 2 GB). Partition ueber Vergleich der Startsektoren (INT 25h bzw. 7305h gegen INT 13h, MBR und
  EMBR-Kette). Wechsel: Text 36 (Nein vorgewaehlt), Tabellensektor nach RD2\PTABn.BAK, Typ 0Bh/0Ch bzw. 04h/06h/0Eh,
  Merker RD2FMT1 in Sektor 2 (nur wenn leer oder schon RD2-Merker), Neustart.
- Schritt 4: RDDISK Export 3 (Merker pruefen, CHS 0/0/2), SHELL ab 386: RDFORMAT /W; RDFORMAT loescht den Merker zuerst,
  findet das Laufwerk wieder, formatiert ohne Rueckfrage, startet danach INSTALL "W<ziel>".
- Schritt 5: INSTALL mit FAT16/FAT32 unter "C: vorher formatieren" (nur ab 386 vom 386-Medium), uebergibt /FS: und /I:;
  "W": Ziel uebernehmen, gleich installieren. RDSYS-Kern: FAT32-Ziel -> Startkern, sonst \RD2\K16. RD2SETUP-Notweg
  ebenso (7303h, ab 65525 Cluster).
- Kern: DriveInfo zaehlt Festplatten ueber 7303h (FAT32-Kern), ueber 4 GB Hoechstwert 0xFFFFFFFF.
- Schritt 7 (Tests, alle gruen): fat32.sh (FAT16<->FAT32 mit Neustart, fsck.fat), fdisk32.sh (10 GB: FAT32 Typ 0Ch
  10.236 MB, FAT16 Typ 06h 2.047 MB), install32.sh (1,44-MB-Satz auf FAT32 bis Start von C:), frei.sh (3 GB richtig,
  6 GB Hoechstwert), kette, satz, fdisk, hdd, install_fmt, schnell.sh.
- Behoben beim Testen: alter Startsektor wird beim Typwechsel geloescht (sonst nimmt DOS das alte FAT), RDFORMAT findet
  das Laufwerk nach dem Neustart ueber GENIOCTL (Groesse, Lage), BPB-Stellen 24/28, RDFDISK ruft
  Check_For_INT13_Extensions auf (vorher bei 8 GB Schluss), SHELL startet INSTALL nach Fortsetzung nicht doppelt.
- Testskripte beenden nur noch ihre eigenen Emulatoren (Socket in der Befehlszeile).
