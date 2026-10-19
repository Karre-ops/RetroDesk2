# RetroDesk 2 (in Arbeit)

Grafische Umgebung für 8088 (640 KB, Diskettenbetrieb) bis Pentium. Entwurf: [../docs/ENTWURF_RD2.md](../docs/ENTWURF_RD2.md), Plan: [../docs/PLAN_RD2.md](../docs/PLAN_RD2.md).

Stand: Phase 0 (Spezifikation API 1.0) freigegeben am 19.10.2026. **Phase 1** (Gerüst, Diskettentest) fertig.

## Bauen und testen

```sh
bash rd2/build.sh          # SDK (RD2START.OBJ, RD2.LIB), Kern RD.EXE, LEER.EXE -> rd2/out, Budgetprüfung
bash rd2/test/floppy.sh    # bootet eine 720-KB-Diskette in QEMU (nur A:, keine Platte), prüft COM1-Ausgaben
```

Aufbau:

| Ordner | Inhalt |
|---|---|
| `sdk/` | `RD2.H`, `RD2API.H`, `RD2ORD.H`, `RD2DRV.H`, `RD2START.ASM` (Startcode) |
| `kern/` | `RD.C` (Kern, Stand Phase 1: Laden und Starten), `ENTER.ASM` |
| `apps/` | Programme, zurzeit `leer/` |
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
