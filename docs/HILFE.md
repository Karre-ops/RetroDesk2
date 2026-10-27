# Hilfe in RetroDesk 2 (F1)

Ablauf: Programm ruft `HelpShow(fenster, "RD2.HLP", thema)` (API 0x2D3). RDKX lädt RDPRN.MOD (Export 6),
das startet `HILFE.EXE datei thema` aus dem RD2-Ordner und kehrt sofort zurück. Die Hilfe ist ein normales
Fenster (nicht modal). Datei ohne Pfad liegt im RD2-Ordner. HILFE.EXE, RD2.HLP und RDPRN.MOD liegen auf Diskette 2.

## Format der .HLP-Datei (Text, CP437)

    # Kommentar
    @0 Inhalt              Thema 0 ist der Inhalt
    Text ...               jede Zeile ist ein Absatz und wird umbrochen, Leerzeile = Abstand
    {Fenster|3}            Verweis auf Thema 3
    @3 Fenster
    ...

Höchstens 4 KB Text und 40 Verweise je Thema. Fehlt ein Thema, zeigt HILFE.EXE den Inhalt.
Quelle der RD2-Hilfe: docs/RD2_HLP.txt (UTF-8), build.sh wandelt nach out/RD2.HLP (CP437).

## Bedienung

Pfeile, Bild auf/ab, Pos1, Ende blättern. Tab / Umschalt+Tab wählen einen Verweis, Eingabe oder Mausklick folgt.
Rückschritt oder rechte Maustaste: zurück (8 Schritte). I: Inhalt. Esc schließt.

## F1 in Programmen

Programme fangen `K_F1` in `M_KEY` ab. Textfelder (RDCTLX) geben F1 an ihr Elternfenster weiter.
Eingebaut: Schreibtisch (Thema 2), Ordnerfenster (5), Notizblock, Rechner, Uhr (7).
Test: test/hilfe.sh.
