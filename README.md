# RetroDesk 2

Grafische Mehrprogramm-Oberfläche und DOS-kompatibles System für PCs vom 8088 mit 512 KB und Diskettenlaufwerk
bis zum Pentium. CGA, Hercules, EGA, VGA und VESA, Maus, AdLib und Sound Blaster, Drucker (9 und 24 Nadeln, PCL),
Netz über Paketreiber (FTP, IRC, Netzzeit). Ab 386 zusätzlich 32-Bit-Programme (RD2/32) und FAT32. Läuft ab DOS 5,
bringt den FreeDOS-Kern mit, keine Festplatte nötig.

## Herunterladen und installieren

Unter [Releases](../../releases), je Verteilform eine Datei:

| Datei | Für |
|---|---|
| `RD2_XT_360KB.ZIP` | 8088 bis 286: vier 360-KB-Disketten |
| `RD2_386_144MB.ZIP` | ab 386: zwei 1,44-MB-Disketten mit RD2/32 und FAT32 |
| `RD2_HDD.ZIP` | jeder PC mit DOS 5: Ordner zum Kopieren, Installation mit `RD2INST` |
| `RD2_CD.ISO`, `RD2_USB.ZIP` | Start von CD oder USB-Stick |

Von Diskette, CD oder Stick starten und im Setup „I“ wählen. Einzelheiten in `LIESMICH.TXT` beim Release.

## Aufbau

| Ordner | Inhalt |
|---|---|
| `src/` | System: `kern` (RD2.EXE), `gdi`, `user`, `ctl`, `shell`, `drv`, `snd`, `net`, `prn`, `rd32`, `disk` (FORMAT, FDISK, SYS auf FreeDOS-Grundlage), `setup`, `lng` |
| `apps/` | Programme: Editor, Rechner, ZIP, Netz, Uhr, Optionen, Taskmanager und weitere |
| `sdk/` | Header, Startcode und Bibliotheken für eigene Programme in C, Pascal und 32 Bit |
| `tools/` | Bauwerkzeuge, Diskettensatz (`mkset.sh`), Verteilformen (`mkverteil.sh`), Größengrenzen (`budget.txt`) |
| `test/` | automatische Tests in QEMU und DOSBox, Referenzbilder in `test/ref/` |
| `docs/` | Spezifikation, Funktionsliste, SDK, Lizenz; erledigte Pläne in `docs/archiv/` |

## Bauen und testen

Braucht Open Watcom 2 (`/opt/ow`), Python 3, NASM, mtools, pycdlib und QEMU.

```sh
bash build.sh             # alles bauen -> out/, mit Größenprüfung
bash tools/mkset.sh       # 360-KB-Diskettensatz -> out/set/
bash tools/mkverteil.sh   # alle Verteilformen -> out/verteilung/
bash test/schnell.sh      # Kurztest (Grafik, Desktop, Dialoge)
bash test/voll.sh         # voller Testsatz, Ergebnis in obj/voll/ERGEBNIS.txt
```

Dokumentation: [Spezifikation](docs/SPEC.md), [Funktionsliste](docs/FUNKTIONEN.md), [SDK](docs/SDK.TXT)
([English](docs/SDK_EN.TXT)), [Hilfe-Format](docs/HILFE.md), offener Plan [DOS-Fenster per V86](docs/PLAN_V86.md).

## Lizenz

Freie Software unter der **GNU General Public License Version 2 oder später** ([`COPYING`](COPYING), Übersicht
[`docs/LIZENZ.TXT`](docs/LIZENZ.TXT)). FreeDOS-Teile (Kern, SYS, FORMAT, FDISK, FreeCOM) und CWSDPMI stehen unter
ihren eigenen Lizenzen. Fehler bitte als Issue melden.

## Zusatzprogramme in eigenen Repos

[RDBILD](https://github.com/Karre-ops/RDBILD) (Bildbetrachter), [RDQRCODE](https://github.com/Karre-ops/RDQRCODE)
(Datei-Eingang per QR-Code), [RDTRON](https://github.com/Karre-ops/RDTRON) (Spiel),
[RDCALC](https://github.com/Karre-ops/RDCALC) (Rechner mit Funktionsplotter).
