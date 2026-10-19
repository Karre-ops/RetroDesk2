# RetroDesk 2 (in Arbeit)

Grafische Umgebung für 8088 (640 KB, Diskettenbetrieb) bis Pentium. Entwurf: [../docs/ENTWURF_RD2.md](../docs/ENTWURF_RD2.md), Plan: [../docs/PLAN_RD2.md](../docs/PLAN_RD2.md).

Stand: **Phase 0** (Spezifikation API 1.0), zur Freigabe.

- [docs/SPEC.md](docs/SPEC.md): Regeln, Programm-, Ressourcen-, Schrift-, INI-Formate, Disketten, Netz
- [docs/FUNKTIONEN.md](docs/FUNKTIONEN.md): alle Funktionen mit Nummer
- [sdk/RD2.H](sdk/RD2.H), [sdk/RD2API.H](sdk/RD2API.H), [sdk/RD2DRV.H](sdk/RD2DRV.H)
- [tools/budget.txt](tools/budget.txt): Größengrenzen

Prüfen:

```sh
python3 rd2/tools/apicheck.py                       # Nummern/Namen
wcc -0 -mm -zq -i=rd2/sdk rd2/test/hdrtest.c        # 16 Bit
wcc386 -zq -i=rd2/sdk rd2/test/hdrtest.c            # 32 Bit
```
