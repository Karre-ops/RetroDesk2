#!/usr/bin/env python3
"""Real FreeDOS boundary tests on disposable 10 MiB disks. Never uses the live VM disk."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
import zipfile
import socket

def command(name, arguments=None):
    with socket.socket(socket.AF_UNIX) as sock:
        sock.settimeout(8); sock.connect(os.environ['RETRODESK_QMP'])
        stream = sock.makefile('rwb', buffering=0); stream.readline()
        for request in [{'execute': 'qmp_capabilities'}, {'execute': name, 'arguments': arguments or {}}]:
            stream.write(json.dumps(request).encode() + b'\n')
            while True:
                line = stream.readline()
                if not line and name == 'quit': return
                reply = json.loads(line)
                if 'error' in reply: raise RuntimeError(reply['error'])
                if 'return' in reply: break
        return reply['return']


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT
OUT = ROOT / 'obj/swap-feature'
CASES = [('normal', 0, 64, True, '', None), ('limit', 64, 64, True, '', 2),
         ('reserve', 0, 32767, True, '', 3), ('disabled', 0, 64, False, '', 1),
         ('readonly', 0, 0, True, 'readonly', 4), ('diskfull', 0, 0, True, 'full', 3),
         ('shortwrite', 0, 0, True, 'shortwrite', 4), ('readerror', 0, 0, True, 'readerror', 4),
         ('slots', 0, 64, True, '', 5)]
subprocess.run(['nasm', '-f', 'bin', '-o', str(SRC/'out/SWFAULT.COM'), str(SRC/'test/dos/SWFAULT.ASM')], check=True)
results = []
for name, maximum, reserve, enabled, fault, expected in CASES:
    case = OUT / name
    case.mkdir(parents=True, exist_ok=True)
    disk = case / 'disk.raw'
    subprocess.run(['bash', str(SRC / 'tools/mkhd.sh'), str(disk)], check=True, stdout=subprocess.DEVNULL)
    fat = str(disk) + '@@32256'
    with zipfile.ZipFile(SRC / 'release/2.0/RD2_HDD.ZIP') as archive:
        archive.extractall(case / 'payload')
    rd = case / 'payload/RD2'
    for file in ['RD.EXE', 'RDAUSL.MOD', 'RDCTLX.MOD', 'SWAPTEST.EXE', 'SWFAULT.COM']:
        shutil.copy2(SRC / 'out' / file, rd / file)
    (rd / 'RD2.INI').write_bytes(f'[System]\r\nGrafik=VGA\r\nXMS=0\r\nAuslagern={int(enabled)}\r\nSwapMaxKB={maximum}\r\nSwapReserveKB={reserve}\r\n'.encode())
    subprocess.run(['mcopy', '-s', '-i', fat, str(rd), '::/'], check=True)
    if fault == 'readonly':
        (case / 'readonly.txt').write_text('pre-existing read-only swap file')
        subprocess.run(['mcopy', '-i', fat, str(case / 'readonly.txt'), '::/RD2/RD2.SWP'], check=True)
        subprocess.run(['mattrib', '-i', fat, '+r', '::/RD2/RD2.SWP'], check=True)
    if fault == 'full':
        listing = subprocess.check_output(['mdir', '-i', fat, '::/'], text=True)
        free = int(re.search(r'([\d ]+) bytes free', listing)[1].replace(' ', ''))
        with (case / 'fill.dat').open('wb') as f:
            f.truncate(free)
        subprocess.run(['mcopy', '-i', fat, str(case / 'fill.dat'), '::/FILL.DAT'], check=True)
    boot = case / 'boot.img'
    shutil.copy2(SRC / 'release/2.0/RD2_1.IMG', boot)
    config = ''
    if fault in ('shortwrite', 'readerror'):
        config += 'INSTALL=C:\\RD2\\SWFAULT.COM' + (' R' if fault == 'readerror' else '') + '\r\n'
    config += 'SHELL=C:\\RD2\\RD.EXE /D C:\\RD2\\SWAPTEST.EXE ' + ('S' if name == 'slots' else 'T') + '\r\n'
    (case / 'CONFIG.SYS').write_bytes(config.encode())
    subprocess.run(['mcopy', '-o', '-i', str(boot), str(case / 'CONFIG.SYS'), '::/CONFIG.SYS'], check=True)
    serial = case / 'serial.log'
    serial.unlink(missing_ok=True)
    sock = case / 'qmp.sock'
    os.environ['RETRODESK_QMP'] = str(sock)
    args = ['/usr/bin/qemu-system-i386', '-name', 'retrodesk-feature-test', '-machine', 'pc,accel=tcg', '-m', '1', '-display', 'none',
            '-monitor', 'none', '-net', 'none', '-drive', f'file={disk},format=raw,if=ide,index=0',
            '-drive', f'file={boot},format=raw,if=floppy,index=0', '-boot', 'a', '-qmp', f'unix:{sock},server=on,wait=off', '-serial', f'file:{serial}']
    with (case / 'qemu.log').open('wb') as log:
        proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline:
                text = serial.read_bytes().decode('cp437') if serial.exists() else ''
                if 'Speicher vollstaendig zurueck' in text:
                    break
                if proc.poll() is not None:
                    raise RuntimeError('QEMU exited')
                time.sleep(.05)
            else:
                raise TimeoutError(name)
        finally:
            if proc.poll() is None:
                try:
                    command('quit')
                except OSError:
                    proc.terminate()
            proc.wait(timeout=10)
    values = {k: int(v, 16) for k, v in re.findall(r'SWAPTEST: (angelegt|LastError|MemError|Datenfehler) 0x([0-9A-F]+)', text)}
    ok = values.get('Datenfehler') == 0 and 'App laeuft weiter' in text and 'Speicher vollstaendig zurueck' in text
    ok = ok and (values.get('angelegt') == 40 and values.get('MemError') == 0 if expected is None else values.get('LastError') == 1 and values.get('MemError') == expected)
    stats = {k: int(v, 16) for k, v in re.findall(r'SWAPTEST: (Swap belegt|Swapdatei|Limit|Reserve|Swap aktiv|Swap nach Freigabe|Dateilaenge danach|Puffer|Swap frei|Puffer danach|Frei danach) 0x([0-9A-F]+)', text)}
    ok = ok and stats.get('Swap nach Freigabe') == 0 and stats.get('Dateilaenge danach', -1) >= stats.get('Swapdatei', 0)
    ok = ok and stats.get('Swap aktiv') == int(enabled)
    if enabled: ok = ok and stats.get('Limit') == maximum and stats.get('Reserve') == reserve
    else: ok = ok and not stats.get('Swap belegt') and not stats.get('Swapdatei')
    if name == 'normal': ok = ok and stats.get('Swap belegt', 0) > 0 and stats.get('Swapdatei', 0) > 0
    ok = ok and stats.get('Puffer', 0) <= stats.get('Swapdatei', 0)
    if name in ('disabled', 'reserve', 'diskfull'): ok = ok and stats.get('Swap frei') == 0
    if name == 'slots': ok = ok and stats.get('Swap frei') == 0
    if name == 'limit': ok = ok and stats.get('Swap frei') == 0 and stats.get('Frei danach') == maximum * 1024
    if name == 'normal': ok = ok and stats.get('Puffer danach') == stats.get('Dateilaenge danach') and stats.get('Frei danach', 0) > stats.get('Puffer danach', 0)
    result = {'case': name, **values, 'stats': stats, 'passed': ok}
    results.append(result)
    print(json.dumps(result), flush=True)
    (OUT / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
