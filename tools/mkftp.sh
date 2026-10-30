#!/bin/bash
# mkftp.sh - release/2.0/RD2_HDD.ZIP (Universalpaket, 30.10.2026): Inhalt aller Disketten 1 bis 4 als ein Ordner RD2
# (SDK von Diskette 4 als Unterordner RD2\SDK) plus RD2INST.BAT. Auf die Festplatte kopieren (FTP, Netz, Diskette,
# anderer PC), dann RD2INST [Ordner]. Erwartet out/set/RD2_1..4.IMG (tools/mkset.sh).
cd "$(dirname "$0")/.."
R=${R:-release/2.0}; D=obj/hd/RD2; python3 -c "import shutil; shutil.rmtree('$D', True)"; mkdir -p $D $R; rm -f $R/RD2_FTP.ZIP
for i in 1 2 3; do mcopy -n -o -i out/set/RD2_$i.IMG "::/RD2/*" $D/ || exit 1; done
mcopy -n -o -i out/set/RD2_1.IMG ::/LIZENZ.TXT ::/QUELLEN.TXT ::/COPYING $D/ && cp dist/RD2INST.BAT $D/ || exit 1
mcopy -s -n -o -i out/set/RD2_4.IMG ::/SDK $D/ || exit 1
python3 - "$R" <<'PY'
import zipfile, os, sys
z = zipfile.ZipFile(sys.argv[1] + '/RD2_HDD.ZIP', 'w', zipfile.ZIP_DEFLATED)
for w, ds, fs in sorted(os.walk('obj/hd/RD2')):
    ds.sort()
    for f in sorted(fs): z.write(os.path.join(w, f), os.path.relpath(os.path.join(w, f), 'obj/hd'))
z.close()
PY
cp out/set/RD2_?.IMG $R/; (cd $R && sha256sum RD2_* > SHA256.TXT); echo "RD2_HDD.ZIP: $(find $D -type f | wc -l) Dateien"
