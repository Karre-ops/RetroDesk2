# RetroDesk 2 (in Arbeit)

Grafische Umgebung für 8088 (640 KB, Diskettenbetrieb) bis Pentium. Entwurf: [../docs/ENTWURF_RD2.md](../docs/ENTWURF_RD2.md), Plan: [../docs/PLAN_RD2.md](../docs/PLAN_RD2.md).

Stand: Phase 0 (Spezifikation API 1.0) freigegeben am 19.10.2026, Phase 1 (Gerüst, Diskettentest) fertig.
**Phase 2, Teil 1** fertig: Speicherverwaltung (Handles, verschiebbar, verwerfbar, Verdichten), Tasks mit Zeitscheiben, Nachrichten, Zeitgeber, 8087-Zustand je Task.
**Phase 3, Teil 1** fertig (20.10.2026): Grafiktreiber `CGA.DRV`, `HGC.DRV`, `VGAM.DRV`, `RDGDI.MOD` (Gerätekontexte, Clipping mit Rechtecklisten, Linien, Rechtecke, Ellipsen, abgerundete Rechtecke, Polygone, Füllen, Dithering/Muster, Bitmaps, BitBlt/StretchBlt mit Rasteroperationen). Testbild `GDITEST.EXE`, Vergleich gegen `test/ref/` mit `test/gdi_dosbox.sh` (CGA, Hercules, VGA) und `test/gdi.sh` (QEMU). Dazu CPU-Erkennung, Profile Schnell/Komfort und Grafikwahl nach Hardware (`GfxList`/`GfxSelect`, Test `test/gfxwahl_dosbox.sh`). Schriften und Text: `SYS8/14/16.FNT`, `TextOut`, `TextBox`, fett/kursiv/unterstrichen, Testbild `TEXTTEST.EXE` (`PROG=TEXTTEST BILD=text test/gdi_dosbox.sh`). Offen in Phase 3: Maus, Tastatur, Treiber in Assembler.
Phase 2 erledigt; früher offen in Phase 2: Module laden/verwerfen mit Diskettenwechsel, Datei-API, Profile, INT-24h-Dialog, Zwischenablage, Notbeenden.

## Bauen und testen

```sh
bash rd2/build.sh          # SDK (RD2START.OBJ, RD2.LIB), Kern RD.EXE, LEER.EXE -> rd2/out, Budgetprüfung
bash rd2/test/floppy.sh    # bootet eine 720-KB-Diskette in QEMU (nur A:, keine Platte), prüft COM1-Ausgaben
```

Aufbau:

| Ordner | Inhalt |
|---|---|
| `sdk/` | `RD2.H`, `RD2API.H`, `RD2ORD.H`, `RD2DRV.H`, `RD2START.ASM` (Startcode) |
| `kern/` | `RD.C` (Start, Ausgabe, Systeminfo), `MEM.C` (Speicher), `TASK.C` (Laden, Tasks, Nachrichten, Zeitgeber), `KASM.ASM` (Umschalten, INT 8, Kopieren) |
| `apps/` | `leer/`, `test/` (PING, PONG, RECHNE, MEMTEST für Phase 2) |
| `tools/` | `apicheck.py`, `mkapi.py` (erzeugt Aufrufstücke und Kerntabelle), `rd2link.py` (RD2-Anhang), `budget.py`/`budget.txt`, `mkdisk.sh` |
| `test/` | `hdrtest.c`, `floppy.sh` |
| `gdi/ user/ snd/ drv/ shell/` | folgen ab Phase 2 |

`RD2.LIB` enthält je Funktion ein eigenes Stück (5 bis 15 Bytes), eingebunden wird nur, was ein Programm benutzt. LEER.EXE ist so 508 Bytes groß.

- [docs/SPEC.md](docs/SPEC.md): Regeln, Programm-, Ressourcen-, Schrift-, INI-Formate, Disketten, Netz
- [docs/FUNKTIONEN.md](docs/FUNKTIONEN.md): alle Funktionen mit Nummer
- [sdk/RD2.H](sdk/RD2.H), [sdk/RD2API.H](sdk/RD2API.H), [sdk/RD2DRV.H](sdk/RD2DRV.H)
- [tools/budget.txt](tools/budget.txt): Größengrenzen

Header prüfen:

```sh
python3 rd2/tools/apicheck.py                       # Nummern/Namen
wcc -0 -mm -zq -i=rd2/sdk rd2/test/hdrtest.c        # 16 Bit
wcc386 -zq -i=rd2/sdk rd2/test/hdrtest.c            # 32 Bit
```
