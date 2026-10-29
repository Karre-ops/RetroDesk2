# Plan: RD > Optionen fuer alle Einstellungen (2.0.1)

Beschluss Karsten, 29.10.2026: In Optionen laesst sich alles einstellen. Neuer Grafikmodus gilt sofort mit
Rueckfrage "Behalten?" (ohne Antwort nach 15 s zurueck). Menuepunkte Maus und Bildschirmschoner unter RD
entfallen und gehen in Optionen auf.

## Aufbau
OPTIONEN.EXE zeigt eine Seite mit Schaltflaechen (wie eine Systemsteuerung):

| Schaltflaeche | Inhalt | gilt |
|---|---|---|
| Grafik... | Grafikart (nur, was die Karte kann, aus GfxList) und Automatisch, Profil Schnell/Komfort/Automatisch | sofort: RD2 startet neu, Rueckfrage 15 s |
| Klang... | Geraet (Lautsprecher, AdLib, Sound Blaster, Covox, keine), SB-Einstellung, Systemklaenge | nach Neustart von RD2 (Rueckfrage "jetzt?") |
| Maus... | Tempo, Doppelklick, Tasten tauschen (bisher MAUS.EXE), Anschluss (seriell COM1-4, Mouse Systems, PS/2, Bus, InPort) | Tempo usw. sofort; Anschluss nach Neustart des PCs (RDMAUS-Zeile in CONFIG.SYS) |
| Tastatur... | Deutsch/US (RDKEYB) | sofort (RDKEYB umschalten) und dauerhaft (CONFIG.SYS, RD2.INI); fremder KEYB: nur Hinweis |
| Bildschirmschoner... | bisher SCHONER.EXE | sofort |
| Netzwerk... | startet RDNET (Menuepunkt Netzwerk bleibt) | wie bisher |
| System... | Start nach dem Booten, Netzzeit, XMS-Ablage, Hardware neu erkennen (RD2SETUP im Textmodus) | Start ab Booten, XMS nach Neustart von RD2 |

## Technik
- Neustart von RD2: neue RunDos-Option RD_DOS_NEU (Kern). Alle Programme enden, der Rest (RDSWAP.BIN) startet
  RD.EXE mit der alten Befehlszeile neu, die Shell stellt die Fenster aus RD2.STA wieder her. Ein Grafiktreiber-
  Wechsel im laufenden Betrieb (Bildgroesse 640x200/350/480, Farbtiefe, Hintergrundpuffer) waere in RDGDI deutlich
  groesser und unsicherer; der Neustart dauert wenige Sekunden. Vorher Warnung, wenn andere Programme laufen.
- Rueckfrage: Optionen schreibt [System] Grafik=neu und GrafikAlt=alt, startet RD2 neu. Die Shell sieht GrafikAlt
  und startet "OPTIONEN.EXE B": Dialog "Grafik behalten?" mit Zaehler 15 s. Ja: GrafikAlt weg. Nein/Zeit um:
  Grafik=alt, GrafikAlt weg, Neustart. Bleibt der Schirm dunkel (Monitor kann den Modus nicht), reicht Warten.
- Steht /CGA, /VGA usw. in der Befehlszeile (CONFIG.SYS), gilt diese; Optionen zeigt dann einen Hinweis.
- MAUS.EXE und SCHONER.EXE bleiben eigene Programme (Diskette 2) und werden aus Optionen gestartet; ihre
  Menuepunkte unter RD entfallen. Maus-Anschluss kommt in MAUS.EXE dazu.
- Kosten: Kern einige Bytes (RD_DOS_NEU), Shell einige Bytes (GrafikAlt pruefen, zwei Menuepunkte weniger),
  OPTIONEN.EXE waechst (wird nur bei Bedarf geladen, kein Dauerspeicher).

## Tests
test/optionen.sh erweitert (Seiten, CONFIG.SYS-Zeilen fuer RDMAUS/RDKEYB im Testordner), neu test/grafikwahl.sh
(Wechsel VGA mono -> VGA16, Neustart, Rueckfrage: Ja behaelt, Zeit um stellt zurueck), SHOT-Referenzen fuer
Menue RD und die Optionen-Seiten, danach voller Satz und DOSBox-Pruefung vor 2.0.1.
