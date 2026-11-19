# Platzplan RetroDesk 2

Stand 19.11.2026 (main 83e5a38), Zahlen aus build.sh und dem Diskettensatz.

## Lage
- **Diskette 1** (RD2SYSTEM): Die S-Dateien belegen 271 KB (auf 1-KB-Cluster gerundet). Mit DOS 5
  (IO.SYS + MSDOS.SYS, 72 KB) sind das genau die erlaubten 343 KB (`@SYSTEM_DISK`). Die restlichen
  11 KB bleiben für RD2.INI, RD2.STA und Daten frei. Mit dem FreeDOS-Kern sind physisch auch 11 KB frei.
- **RD2.EXE**: Datei 29.666 von 29.696 Bytes (30 Bytes Luft), Speicher 44.066 von 45.504 Bytes (1,4 KB Luft).
- **Weitere enge Dateien**: RDGDI 52 Bytes Speicher, RDCTL 8 Bytes Speicher, RDUSER 194 Bytes Speicher,
  RDAUSL 3 Bytes Datei, RDSND 7 Bytes Datei, OPTIONEN 596 Bytes Datei.
- Verschnitt durch die 1-KB-Cluster auf Diskette 1: 14 KB.

## Wofür der Platz gebraucht wird
- Fehlerkorrekturen und kleine Erweiterungen am Kern (jede neue API-Funktion kostet Bytes in RD2.EXE).
- PLAN_DOS Schritt 2 und 3 (Kern und RDSWAP.BIN).
- PLAN_32BIT Schritt 1 und 2 (Bedarf, SysInfo, Vollbild-API). RD32.MOD und RD3D.MOD liegen nicht auf Diskette 1.

## Maßnahmen Diskette 1
| Nr. | Maßnahme | Gewinn | Risiko |
|---|---|---|---|
| D1 | RDSYS.EXE (FreeDOS SYS, nur für "Festplatte startfähig") auf Diskette 2. INSTALL kopiert es mit den anderen Dateien nach C:\RD2, KERNEL.SYS kommt von Diskette 1, dann `C:\RD2\RDSYS C:\RD2 C: /UPDATE`. Textweg in RD2SETUP genauso. | 20 KB | mittel (Installationskette testen: kette.sh, install_hd.sh) |
| D2 | DE/US/PC10.KBD in eine Datei RDKEYB.KBD mit Abschnitten (RDKEYB und Optionen lesen den Abschnitt), DE.LNG und EN.LNG in eine Datei RD2.LNG | 3 KB | klein bis mittel |
| D3 | Schriften SYS8/14/16.FNT in eine Datei | 1 KB | mittel (RDGDI-Laden) |
| D4 | COPYING bleibt auf Diskette 1 (GPL: Lizenztext gehört zum Programm) | – | – |

## Maßnahmen Kern (RD2.EXE)
Größte Teile laut obj/rd.map: main 2,8 KB, Tabellen der Speicherverwaltung 4,4 KB (Daten, in der Map fälschlich hinter `_k_fpuseg` gezählt), mod_xdone 1,4 KB,
mem_done 1,3 KB, input_poll 1,1 KB, task_load 1,0 KB, Funktionstabelle 1,4 KB, Stummel für fehlende
Funktionen 1,0 KB.

| Nr. | Maßnahme | Gewinn | Risiko |
|---|---|---|---|
| K1 | Selten gebrauchte Funktionen nach RDKX.MOD (verwerfbar, wird bei Bedarf geladen): MemStats, DriveInfo, KernInfo, ProfileSetStr. Weiterleitung über die vorhandene kx-Tabelle (4 Bytes je Funktion). RDKX-Grenze dafür auf 9 KB. | 1,5 bis 2 KB Datei und Speicher | mittel |
| K2 | Startcode (main, die *_api-Anmeldungen, k_imgload) als RD2INIT.BIN wie RDSWAP.BIN: einmal laden, ausführen, freigeben | 4 bis 5 KB Speicher, RD2.EXE entsprechend kleiner | hoch |
| K3 | ~~8087-Ablage nur mit 8087~~: schon so umgesetzt (TASK.C holt die 12 x 94 Bytes nur mit 8087 per mem_get). Die 4,3 KB hinter `_k_fpuseg` in der Map sind die Tabellen der Speicherverwaltung (siehe K5). | 0 | – |
| K5 | Tabellen der Speicherverwaltung verkleinern: hnd[201] je 14 Bytes = 2,8 KB, blk[160] je 10 Bytes = 1,6 KB (MEM.C, MAXH/MAXB). Erst den Höchststand im Betrieb messen (Zähler in MemStats), dann z. B. MAXH 200 -> 128 (1 KB) | bis 1,5 KB Speicher | mittel (zu wenig Handles = MemAlloc scheitert) |
| K4 | Dateigrenze RD2.EXE von 29 auf 30 KB, sobald D1 Platz schafft | +1 KB Spielraum | klein |

## Umgesetzt
- D1 (19.11.2026): RDSYS.EXE auf Diskette 3, Diskette 1 hat 20 KB mehr frei.
- K4 (19.11.2026): Dateigrenze RD2.EXE 30 KB.
- K1 (19.11.2026), anders als geplant: Die Tabelle der Stummel für fehlende Funktionen (NOTSUP.ASM, erzeugt von
  tools/mkapi.py) hat jetzt ein Halbbyte je API-Platz statt 4 Bytes je Funktion. RD2.EXE 640 Bytes kleiner,
  im Speicher ebenso (29.026 Bytes Datei, 43.426 Bytes Speicher). MemStats, KernInfo und ProfileSetStr
  hängen an inneren Daten des Kerns und bleiben. DriveInfo bleibt ebenfalls: die Shell fragt damit laufend
  die Wechselleitung ab, aus dem verwerfbaren RDKX würde das bei fremder Diskette nach RD2SYSTEM fragen.
- D2 (19.11.2026) zurückgestellt: Nachgerechnet bringt nur das Bündeln der .KBD-Dateien etwas (3 KB -> 1 KB,
  also 2 KB). DE.LNG und EN.LNG belegen auch zusammen 2 Cluster, Gewinn 0. Dafür müssten RDKEYB (Assembler),
  Optionen (Liste aller *.KBD, eigene Belegungen als Datei), RD2SETUP und INSTALL umgebaut werden, und alte
  CONFIG.SYS-Zeilen wie `RDKEYB.COM C:\RD2\PC10.KBD` müssten weiter gehen. Nach D1 hat Diskette 1 31 KB frei,
  2 KB rechtfertigen das Risiko nicht. Wieder aufgreifen, wenn Diskette 1 erneut knapp wird.
- K3 (19.11.2026) entfällt: Der 8087-Puffer wird schon nur mit 8087 angelegt. Die Zahl im Plan war ein Lesefehler der Map.
- D3 ebenfalls zurückgestellt: Schriften gebündelt 11 statt 12 KB, nur 1 KB Gewinn.

## Vorschlag Reihenfolge
1. D1 (schafft 20 KB auf Diskette 1, dann ist K4 sofort möglich)
2. K1 (kleinster Kerneingriff mit spürbarem Gewinn)
3. D2
4. K3, danach K2 erst, wenn PLAN_32BIT mehr Kernplatz braucht

Entscheidungen von Karsten offen: Reihenfolge, ob D1 sofort, ob K2 überhaupt.
