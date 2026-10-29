#!/bin/bash
# mkftp.sh - release/2.0/RD2_FTP.ZIP: Inhalt der Disketten 1 bis 3 als ein Ordner RD2 (Installation per FTP, HDD auf HDD)
# plus RD2INST.BAT. Erwartet out/set/RD2_1..3.IMG (tools/mkset.sh).
cd "$(dirname "$0")/.."
R=${R:-release/2.0}; D=obj/hd/RD2; mkdir -p $D $R; rm -f $D/*
for i in 1 2 3; do mcopy -n -o -i out/set/RD2_$i.IMG "::/RD2/*" $D/ || exit 1; done
mcopy -n -o -i out/set/RD2_1.IMG ::/LIZENZ.TXT ::/QUELLEN.TXT ::/COPYING $D/ && cp dist/RD2INST.BAT $D/ || exit 1
python3 - "$R" <<'PY'
import zipfile, os, sys
z = zipfile.ZipFile(sys.argv[1] + '/RD2_FTP.ZIP', 'w', zipfile.ZIP_DEFLATED)
for f in sorted(os.listdir('obj/hd/RD2')): z.write('obj/hd/RD2/' + f, 'RD2/' + f)
z.close()
PY
cp out/set/RD2_?.IMG $R/; (cd $R && sha256sum RD2_* > SHA256.TXT); echo "RD2_FTP.ZIP: $(ls $D | wc -l) Dateien"
