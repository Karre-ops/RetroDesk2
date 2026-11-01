# Geschwindigkeit aller Grafikarten

Messung: test/bench_modi.sh, BENCH2 unter RD.EXE in DOSBox, 8086 mit festen 1000 Takten/ms (XT-nah), VESA-Treiber (386-Code) mit cputype 386 und gleichen Takten. Mikrosekunden je Aufruf (tools/benchtab.py). Sehr langsame Werte sind grob (1.977.300 = nur ein Durchlauf in 2 s).

Stand 30.10.2026 nach den Beschleunigungen (Blöcke über Tabellen, 256 Farben zeilenweise, schmale Schrift in Assembler, CGA4 Muster/Invertieren). Neu gemessen: CGA4, VGA4, VGA16, MCGA, EGA4, EGA, EGA200, EGA320, VESA. Die übrigen Zeilen sind die erste Messung vom selben Tag (SVGA16, VESA800/1024 seither geändert, nicht neu gemessen). 'Block Geraet' = Block im Geräteformat (wie Menü-Hintergrund), neu in BENCH2.

Nachtrag 30.10.2026 (Hintergrund roh): 'Block roh' = Menü-Hintergrund im rohen Bildspeicherformat (BMP_ROH, neu). Neu gemessen: CGA4, VGA4, VGA16, EGA4, EGA, EGA200, EGA320. VESA/MCGA nutzen weiter das Geräteformat (dort schon Speicherkopie).

| Grafikart | Text durchs. | Text deckend | Schirm fuellen | Muster | Invert 200x100 | Rahmen | Block 1 Bit | Block Geraet | Block roh | Füllen je 1000 Punkte |
|---|---|---|---|---|---|---|---|---|---|---|
| CGA 640x200 mono | 3.099 | 3.295 | 19.577 | 19.577 | 8.170 | 2.605 | 16.207 | – | – | 152 |
| CGA 320x200 4 | 14.328 | 14.866 | 26.720 | 30.895 | 26.720 | 8.307 | 247.162 | 395.460 | 35.308 | 417 |
| Hercules 720x348 | 4.127 | 4.609 | 31.385 | 31.385 | 8.070 | 2.528 | 15.945 | – | – | 125 |
| VGA 640x480 mono | 3.907 | 4.453 | 39.546 | 39.546 | 8.273 | 2.515 | 16.075 | – | – | 128 |
| VGA 640x480 4 | 10.630 | 20.384 | 70.617 | 219.700 | 15.093 | 11.769 | 197.730 | 395.460 | 61.790 | 229 |
| VGA 640x480 16 | 10.630 | 20.384 | 76.050 | 219.700 | 15.093 | 11.769 | 197.730 | 659.100 | 104.068 | 247 |
| MCGA 320x200 256 | 22.727 | 20.596 | 94.157 | 197.730 | 76.050 | 31.385 | 659.100 | 123.581 | – | 1471 |
| EGA 640x350 mono | 3.493 | 3.954 | 29.511 | 29.511 | 8.037 | 2.444 | 15.569 | – | – | 131 |
| EGA 640x350 4 | 9.371 | 17.813 | 53.440 | 164.775 | 14.646 | 11.429 | 197.730 | 395.460 | 59.918 | 238 |
| EGA 640x350 16 | 9.371 | 17.654 | 59.918 | 164.775 | 14.646 | 11.429 | 197.730 | 659.100 | 104.068 | 267 |
| EGA 640x200 16 | 7.036 | 12.205 | 42.070 | 104.068 | 14.646 | 11.429 | 197.730 | 659.100 | 104.068 | 328 |
| EGA 320x200 16 | 8.005 | 10.245 | 38.025 | 76.050 | 32.414 | 11.429 | 219.700 | 659.100 | 116.311 | 594 |
| VESA 640x480 256 (386) | 48.226 | 94.157 | 247.162 | 494.325 | 44.938 | 29.077 | 494.325 | 89.877 | – | 804 |
| VESA 800x600 256 (386) | 47.078 | 109.850 | 329.550 | 1.977.300 | 494.325 | 27.849 | 988.650 | – | – | 686 |
| VESA 1024x768 256 (386) | 47.078 | 109.850 | 494.325 | 1.977.300 | 494.325 | 27.849 | 988.650 | – | – | 628 |
| VESA 800x600 16 | 10.192 | 19.577 | 219.700 | 395.460 | 14.328 | 12.056 | 197.730 | – | – | 457 |
