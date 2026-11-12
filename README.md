# RetroDesk 2

Grafische Mehrprogramm-Oberfläche für DOS-PCs vom 8088 mit 640 KB und Diskettenlaufwerk bis zum Pentium.
CGA, Hercules, EGA, VGA und VESA, Maus und Tastatur, AdLib/Sound Blaster, Netz über Paketreiber (FTP, IRC, Netzzeit).
Läuft ab DOS 5, ohne Festplattenzwang.

## Herunterladen und installieren

Fertige Disketten stehen unter [Releases](../../releases) und im Ordner [`release/2.0/`](release/2.0):

| Datei | Inhalt |
|---|---|
| `RD2_1.IMG` | System, bootfähig (360 KB) |
| `RD2_2.IMG` | Programme und Treiber |
| `RD2_3.IMG` | Netz, FreeCOM, H-Tron |
| `RD2_4.IMG` | SDK |
| `RD2_HDD.ZIP` | alle vier Disketten als ein Ordner für die Installation von der Festplatte |

Von Diskette: mit `RD2_1.IMG` starten, Installation im Menü. Von der Festplatte: `RD2_HDD.ZIP` z. B. nach
`C:\RD2NEU` entpacken und `C:\RD2NEU\RD2INST` aufrufen. Mehr in `release/2.0/LIESMICH.TXT`.

## Aufbau

| Ordner | Inhalt |
|---|---|
| `src/` | System: `kern` (RD2.EXE), `gdi`, `user`, `ctl`, `shell`, `drv` (Grafik, Maus, Tastatur), `snd`, `net`, `prn`, `disk` (FORMAT, FDISK, SYS auf FreeDOS-Grundlage), `setup`, `lng` |
| `apps/` | Programme: Editor, Rechner, ZIP, Netz, Uhr, Optionen, H-Tron und weitere |
| `sdk/` | Header, Startcode und `RD2.LIB` für eigene Programme in C und Pascal |
| `tools/` | Bauwerkzeuge (Ressourcen, Schriften, Disketten, Größenprüfung `budget.txt`) |
| `test/` | automatische Tests in QEMU und DOSBox, Referenzbilder `test/ref/` |
| `docs/` | Spezifikation, Funktionen, SDK, Lizenz; `docs/archiv/` alte Entwürfe und Pläne |
| `release/` | fertige Disketten |

## Bauen und testen

Braucht Open Watcom 2 (`/opt/ow`), Python 3, mtools und QEMU.

```sh
bash build.sh             # alles bauen -> out/, mit Größenprüfung
bash tools/mkset.sh       # Diskettensatz -> out/set/
bash test/schnell.sh      # Kurztest (Grafik, Desktop, Dialoge)
bash test/voll.sh         # voller Testsatz
```

Dokumentation: [Spezifikation](docs/SPEC.md), [alle Funktionen](docs/FUNKTIONEN.md), [SDK](docs/SDK.TXT).

## Lizenz

RetroDesk 2 ist freie Software unter der **GNU General Public License Version 2 oder später** (siehe [`COPYING`](COPYING),
Übersicht [`docs/LIZENZ.TXT`](docs/LIZENZ.TXT)). FreeDOS-Teile (Kern, SYS, FORMAT, FDISK, FreeCOM, CuteMouse) stehen unter
ihrer eigenen GPL. Fehler bitte als Issue melden.

## Eigene Repos für Zusatzprogramme
Nicht im Diskettensatz, getrennt entwickelt: [RDBILD](https://github.com/Karre-ops/RDBILD) (Bildbetrachter), [RDQRCODE](https://github.com/Karre-ops/RDQRCODE) (Datei-Eingang per QR-Code), [H-TRON](https://github.com/Karre-ops/H-TRON) (Spiel), [RDCALC](https://github.com/Karre-ops/RDCALC) (Rechner mit Funktionsplotter).
