#!/bin/bash
# mkdisk.sh IMAGE AUTOEXEC.BAT  - bootfaehige Systemdiskette fuer die automatischen Tests.
# FMT=360 (Vorgabe, Verteilformat seit 24.10.2026) oder 720. NUR="A.EXE B.MOD" kopiert nur diese Dateien nach RD2.
# FreeDOS-Kern und COMMAND.COM aus test/qemu/boot.img (DOS 5 darf nicht weitergegeben werden;
# am PC10-III wird dieselbe Verzeichnisstruktur auf eine DOS-5-Bootdiskette kopiert).
set -e
cd "$(dirname "$0")/.."
IMG=$1; AE=$2; SRC=test/qemu/boot.img
rm -f $IMG
mformat -C -f ${FMT:-360} -v RD2SYSTEM -i $IMG ::
python3 - $SRC $IMG <<'PY'
import sys
s = open(sys.argv[1], 'rb').read(512); f = open(sys.argv[2], 'r+b'); d = bytearray(f.read(512))
d[0:3] = s[0:3]; d[0x3E:0x200] = s[0x3E:0x200]          # FreeDOS-Bootcode, BPB der formatierten Diskette behalten
f.seek(0); f.write(d)
PY
T=obj/disk.tmp
python3 -c "import shutil; shutil.rmtree('$T', True)"; mkdir -p $T
mcopy -i $SRC ::/KERNEL.SYS $T/; mcopy -i $SRC ::/FREEDOS/BIN/COMMAND.COM $T/; mcopy -i $SRC ::/FREEDOS/BIN/FDAPM.COM $T/
mcopy -i $IMG $T/KERNEL.SYS $T/COMMAND.COM ::/
if [ -n "$SHELLZEILE" ]; then printf '%s\r\n' "$SHELLZEILE" > $T/CONFIG.SYS; else printf 'SHELL=A:\\COMMAND.COM A:\\ /E:512 /P\r\n' > $T/CONFIG.SYS; fi
sed 's/$/\r/' $AE > $T/AUTOEXEC.BAT
mcopy -i $IMG $T/CONFIG.SYS $T/AUTOEXEC.BAT $T/FDAPM.COM ::/
mmd -i $IMG ::/RD2
if [ -z "$NUR" ]; then   # Vorgabe: Systemdateien (S) + alle in AUTOEXEC/SHELL genannten Programme (360 KB reichen nicht fuer alles)
  NUR="$(awk '$4=="S"{print $1}' tools/budget.txt | tr '\n' ' ') $(printf '%s\n%s\n' "$(cat $AE)" "$SHELLZEILE" | grep -oiE '[A-Z0-9_]+\.(EXE|MOD|RDB|BIN)' | tr a-z A-Z | tr '\n' ' ')"
  case " $NUR " in *" RESTEST.EXE "*) NUR="$NUR RESTEST.RDB";; esac
  case " $NUR " in *" DOSTEST.EXE "*) NUR="$NUR FREIMEM.EXE";; esac
  case " $NUR " in *" MODTEST.EXE "*) NUR="$NUR TESTMOD.MOD TESTMOD2.MOD";; esac
  case " $NUR " in *" WINTEST.EXE "*) NUR="$NUR UEBER.EXE";; esac
  case " $NUR " in *" STRESS.EXE "*) NUR="$NUR WURM.EXE";; esac
  case " $NUR " in *" PRNTEST.EXE "*) NUR="$NUR RDPRN.MOD EPSON9.DRV";; esac
  case " $NUR " in *" SNDTEST.EXE "*) NUR="$NUR ADLIB.DRV SB.DRV COVOX.DRV RDMUS.MOD";; esac
  case " $NUR " in *" PING.EXE "*) NUR="$NUR PONG.EXE RECHNE.EXE MEMTEST.EXE";; esac
fi
for f in out/*.EXE out/*.MOD out/*.DRV out/*.FNT out/*.LNG out/*.BIN out/*.RDB out/*.HLP out/*.COM; do [ -f "$f" ] || continue; if [ -n "$NUR" ]; then case " $NUR " in *" ${f##*/} "*) ;; *) continue;; esac; fi; [ "${f##*/}" = RD2SETUP.EXE ] && [ -z "$SETUP" ] && continue; [ "${f##*/}" = INSTALL.EXE ] && [ -z "$SETUP" ] && continue; mcopy -i $IMG "$f" ::/RD2/ 2>/dev/null || { [ -z "$FMT" ] || echo "mkdisk: FEHLER ${f##*/} passt nicht auf $IMG" | tee -a obj/mkdisk_fehler.txt >&2; VOLL=1; }; done   # RD2SETUP und INSTALL nur, wenn SETUP=1 (sonst fragt der erste Start; Platz auf der 360-KB-Testdiskette)
python3 -c "import shutil; shutil.rmtree('$T', True)"
if [ -n "$VOLL" ] && [ -z "$FMT" ]; then   # FreeDOS-Testkern + COMMAND.COM (131 KB) sind groesser als DOS 5: Testdiskette dann 720 KB
    echo "mkdisk: $IMG passt nicht auf 360 KB, baue sie in 720 KB (echte Systemdiskette mit DOS 5 prueft tools/budget.py)" >&2
    FMT=720 exec bash "$0" "$@"
fi
[ -z "$VOLL" ]   # Datei fehlt: Fehler (Test sonst mit falschem Treiber, Audit G25)
