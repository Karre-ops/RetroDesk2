# Lizenzen in RetroDesk 2 (geprüft am 24.10.2026, umgestellt am 01.11.2026)

**Beschluss 01.11.2026 (Karsten): Ganz RetroDesk 2 steht unter der GPL Version 2 oder später**, auch Kern, Module, Treiber, Shell, Programme und SDK. Damit ersetzt: Freeware ohne Quelltext für die eigenen Teile und MIT für das SDK (27.10.2026). Ziel: möglichst viele nutzen RD2, passen es an und melden Fehler. Teile aus Free FDISK und FreeCOM bleiben GPL v2 ohne „oder später“; das verträgt sich, weil eigener Code unter „v2 oder später“ zusammen mit v2-Code als v2 weitergegeben werden darf.

Folgen:
- Programme, die mit dem SDK gebaut werden, binden `RD2START.OBJ` und `RD2.LIB` ein und sind damit abgeleitete Werke: Wer sie weitergibt, muss sie GPL-verträglich lizenzieren (z. B. GPL, LGPL, MIT, BSD). Geschlossene Programme für RD2 gehen nur ohne Weitergabe. Wenn das später stört, wäre eine Ausnahme für Startcode und Bibliothek (wie die GCC Runtime Library Exception) der Weg.
- Quelltext: Solange das Repo privat ist, gilt das schriftliche Angebot in `QUELLEN.TXT` für ganz RD2. Bei Weitergabe als Download genügt der Quelltext am selben Ort.
- Offen: SYS8.FNT/SYS16.FNT stammen aus einem VGA-BIOS (Urheberrecht prüfen, ggf. freie Schriften), RDTRON (H-Tron) bleibt draußen, bis seine Lizenz geklärt ist.

Der Rest dieser Datei ist die Prüfung vom 24.10.2026 und gilt für die fremden Teile weiter.

Geprüft an den Lizenztexten selbst: GNU GPL Version 2 (gnu.org) und den Kopfzeilen der Quellen in
`github.com/FDOS/fdisk`, `github.com/FDOS/format`, `github.com/FDOS/kernel`.

## Fremde Teile

| Teil | Lizenz laut Quelle | Wo in RD2 |
|---|---|---|
| FreeDOS-Kern `KERNEL.SYS` (8086-Fassung) | GPL v2 „oder später“ | Diskette 1, unverändert |
| FreeDOS `SYS` (kernel/sys) | GPL v2 „oder später“ | Grundlage für `RDSYS.EXE` |
| FreeDOS `FORMAT` (Eric Auer u. a.) | GPL v2 „oder später“ | Grundlage für `RDFORMAT.EXE` |
| Free FDISK (Brian Reifsnyder, Bernd Boeckmann) | GPL **Version 2** (ohne „oder später“); einzelne Hilfsdateien (printf, ansicon, svarlang) MIT | Grundlage für `RDFDISK.EXE` |
| MS-DOS 5 | Microsoft, nicht weitergebbar | wird **nicht** verteilt; Nutzer kann mit `SYS A:` tauschen |

Weil FDISK nur GPL v2 erlaubt, stehen alle drei Werkzeuge einheitlich unter **GPL v2**.

## Was daraus folgt (Abschnitte der GPL v2)

- **§2 b**: Ein Programm, das FreeDOS-Code enthält, muss als Ganzes unter der GPL weitergegeben werden. Das betrifft `RDFDISK`, `RDFORMAT`, `RDSYS`.
- **§2 a**: Geänderte Dateien bekommen einen deutlichen Vermerk „geändert für RetroDesk 2, Datum“.
- **§2 c**: Interaktive Programme zeigen beim Start (bei uns: im Info-Fenster) Copyright, „keine Gewährleistung“ und den Hinweis auf die GPL.
- **§2 letzter Absatz** („mere aggregation“): Kern, Module, Shell und alle anderen eigenständigen RD2-Programme liegen nur auf derselben Diskette. Sie werden dadurch **nicht** GPL.
- **§3, ganzer Quelltext**: Zum Quelltext gehören alle Teile, die im Programm stecken, plus Build-Skripte. Die Ausnahme für „Bestandteile des Betriebssystems“ gilt nicht für Teile, die mitgeliefert werden. Deshalb muss **RD2.LIB** (wird in die Werkzeuge gebunden) und der Startcode `RD2START.OBJ` GPL-verträglich lizenziert sein: **MIT**. Der RD2-Kern selbst wird nur zur Laufzeit über die API aufgerufen, nicht eingebunden.
- **§3 a/b, Quelltext bei Disketten**: Ein bloßer Link reicht bei Disketten **nicht** (das erlaubt §3 nur, wenn die Programme selbst zum Herunterladen angeboten werden). Also entweder
  - a) Quelltext der GPL-Teile auf einer mitgelieferten Diskette (geplant: Diskette 4, gepackt), oder
  - b) ein schriftliches Angebot, den Quelltext mindestens 3 Jahre lang zum Selbstkostenpreis zu liefern (`QUELLEN.TXT`).
  Für die Weitergabe über GitHub/Download genügt es, den Quelltext am selben Ort anzubieten.
- **§1**: Jeder Kopie liegt der GPL-Text bei (`COPYING` auf Diskette 1 und 2).

## Umsetzung

- Ordner `rd2/gpl/` mit `COPYING`, darin `fdisk/`, `format/`, `sys/` (nur die übernommenen Teile, Herkunft und Stand im Kopf jeder Datei).
- `sdk/RD2START.ASM` und die Quellen von `RD2.LIB` bekommen einen MIT-Kopf.
- Diskette 1: `COPYING`, `LIZENZ.TXT` (diese Übersicht kurz, für Nutzer). Diskette 4: Quelltext der GPL-Teile.

Das ist eine sorgfältige Lesart des Lizenztexts, keine Rechtsberatung.
