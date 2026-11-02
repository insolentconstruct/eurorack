#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

"""Check the WAV with the vendor decoder, collect notices, package the release."""
import os
import json
import re
import shlex
import subprocess
import sys
import tempfile
import wave
from release_package import package, sha
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

# The updater writes 64 KiB blocks from 0x08020000 up to the frozen-buffer
# sector at 0x08100000.
BLOCK_SIZE = 65536
APP_MAX_SIZE = 0x08100000 - 0x08020000
SAMPLE_RATE = 48000

ROOT = Path('/src')
HERE = ROOT / 'beads/build'
NAME = os.environ.get('RELEASE_NAME', 'drizzlet-dev')
if not re.fullmatch(r'drizzlet-[A-Za-z0-9._-]+', NAME):
    raise SystemExit('RELEASE_NAME must match drizzlet-[A-Za-z0-9._-]+')
WAV = NAME + '.wav'
ZIP = NAME + '-wav.zip'
COMBO = NAME + '-combo.bin'
COMBO_HEX = NAME + '-combo.hex'
COMBO_ZIP = NAME + '-combo.zip'


def wav_read(path):
    with wave.open(str(path), 'rb') as w:
        if (w.getnchannels(), w.getsampwidth(), w.getframerate()) != (1, 2, SAMPLE_RATE):
            raise ValueError('expected mono PCM16 48000 Hz')
        return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(np.float64) / 32768


def wav_write(path, pcm):
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(np.rint(pcm * 32768).astype('<i2').tobytes())


def decoded_ok(stats, payload, expected):
    blocks = len(expected) // BLOCK_SIZE
    return (stats['packets'] == 256 * blocks and stats['blocks'] == blocks and stats['errors'] == 0
                    and stats['overflow'] == 0 and stats['complete'] == 1 and payload == expected
                    and len(payload) == len(expected))


def proof(out):
    app = (out / 'app.bin').read_bytes()
    if not 0 < len(app) <= APP_MAX_SIZE:
        raise ValueError('unexpected application size')
    payload_size = -(-len(app) // BLOCK_SIZE) * BLOCK_SIZE
    expected = app + b'\xff' * (payload_size - len(app))
    wav_sha = sha(out / WAV)
    rows = []
    with tempfile.TemporaryDirectory(prefix='beads-decode-', dir=str(out)) as temp:
        work = Path(temp)
        sim = work / 'flashsim'
        subprocess.run(['g++', '-O2', '-I.', 'beads/build/tools/flashsim.cc',
                                        'stm_audio_bootloader/qpsk/demodulator.cc',
                                        'stm_audio_bootloader/qpsk/packet_decoder.cc', '-o', str(sim)], check=True, cwd=ROOT)
        x = wav_read(out / WAV)
        corrupt = x.copy()
        corrupt[30*SAMPLE_RATE:31*SAMPLE_RATE] = 0
        corrupted = work / 'corrupted.wav'
        wav_write(corrupted, corrupt)
        # Keep only 0.1 s of the final pause/outro; decoding must fail.
        last_block = 0x08020000 + payload_size - BLOCK_SIZE
        tail = 5.0 + (3.0 if last_block % 0x20000 == 0 else 0.2)
        truncated = work / 'missing-eot.wav'
        wav_write(truncated, x[:-int((tail - 0.1)*SAMPLE_RATE)])
        for label, path, rate, want in [('nominal-48000', out/WAV, SAMPLE_RATE, True),
                                                                      ('resampled-44100', out/WAV, 44100, True),
                                                                      ('corrupted-file', corrupted, SAMPLE_RATE, False),
                                                                      ('missing-eot', truncated, SAMPLE_RATE, False)]:
            stream = wav_read(path)
            if rate == 44100:
                stream = resample_poly(stream, 147, 160, window=('kaiser',14.0))
                stream /= max(1.0, np.max(np.abs(stream)) / .999)
            stream = stream.astype(np.float32).astype(np.float64)
            if rate == 44100:
                stream = resample_poly(stream, 160, 147, window=('kaiser',14.0))
            rng = np.random.default_rng(0)
            stream = stream * .5 + rng.standard_normal(len(stream)) * 10**(-70/20)
            s16 = np.clip(np.rint(stream * 32767), -32768, 32767).astype(np.int32)
            adc = work / 'samples.u16'
            adc.write_bytes(((s16 >> 4) + 2048).astype('<u2').tobytes())
            payload = work / 'decoded.bin'
            run = subprocess.run([str(sim), str(adc), str(payload)], capture_output=True, text=True, check=True)
            stats = {k:int(v) for k,v in (x.split('=') for x in run.stdout.split())}
            data = payload.read_bytes()
            ok = decoded_ok(stats, data, expected)
            if ok != want:
                raise ValueError('unexpected decode result: '+label+' '+run.stdout)
            if label == 'missing-eot' and not decoded_ok(
                            dict(stats, complete=1), data, expected):
                raise ValueError('missing-EOT control must retain exact payload with only EOT absent')
            rows.append(dict(case=label, verdict='PASS' if want else 'CONTROL-OK',
                                              input_sha256=sha(path), published_wav_sha256=wav_sha,
                                              decoded_sha256=sha(payload), decoded_bytes=len(data), **stats))
            print(label, rows[-1]['verdict'], run.stdout.strip(), flush=True)
            if label == 'nominal-48000':
                wrong = bytearray(expected)
                wrong[1000] ^= 1
                if decoded_ok(stats, data, bytes(wrong)):
                    raise ValueError('wrong payload was accepted')
                rows.append(dict(case='wrong-payload', verdict='CONTROL-OK', published_wav_sha256=wav_sha))
    (out / 'decode-proof.json').write_text(json.dumps(rows, indent=2)+'\n')


def source_text(path):
    data = path.read_bytes()
    try:
        return data.decode('utf-8')
    except UnicodeDecodeError:
        # Some CMSIS headers are Windows-1252.
        return data.decode('cp1252')


def comments(text):
    return [m.group() for m in re.finditer(r'/\*.*?\*/|(?m:^\s*//[^\n]*(?:\n\s*//[^\n]*)*)', text, re.S)
                    if re.search(r'copyright|redistribution|permission is hereby|licensed?\b|SPDX', m.group(), re.I)]


def notices(out):
    files = set()
    for dep in sorted((out/'objects').rglob('*.nd')):
        text = dep.read_text().replace('\\\n',' ')
        files.update(shlex.split(text.split(':',1)[1]))
    if not files or not any('stm32h7xx_hal.c' in f for f in files):
        raise ValueError('incomplete compiled source inventory')
    chunks = ['Drizzlet, alternative firmware for Mutable Instruments Beads. Notices by compiled file.\n'
                        'Original notices below are preserved verbatim. Files without a standalone\n'
                        'notice are listed too; dependency/runtime licenses follow.\n']
    for name in sorted(files):
        path = Path(name) if name.startswith('/') else ROOT/name
        if not path.is_file():
            raise ValueError('missing dependency: '+name)
        blocks = comments(source_text(path))
        chunks.append('\n===== '+name+' =====\n'+ ('\n\n'.join(blocks) if blocks else '(No standalone notice in this file.)')+'\n')
    for name in ('beads','beads_bootloader'):
        text = (out/'objects'/name/(name+'.map')).read_text()
        libs = sorted(set(re.findall(r'(/opt/[^\s()]+\.a)',text)))
        if not libs:
            raise ValueError('missing runtime archive inventory')
        chunks.append('\n===== Linked runtime archives: '+name+' =====\n'+'\n'.join(libs)+'\n')
    for tc in (Path('/opt/gcc-arm-none-eabi-4_9-2015q2'),
                          Path('/opt/gcc-arm-none-eabi-9-2020-q2-update')):
        licenses = sorted(p for p in tc.rglob('*') if p.is_file() and
                                            re.search(r'(?i)(license|copying)',p.name) and p.suffix.lower() not in ('.pdf','.html','.info'))
        if not licenses:
            raise ValueError('missing compiler runtime licenses: '+str(tc))
        for path in licenses:
            chunks.append('\n===== Toolchain: '+str(tc.name/path.relative_to(tc))+' =====\n'+source_text(path)+'\n')
    name = 'stm_audio_bootloader/LICENSE'
    chunks.append('\n===== '+name+' =====\n'+(ROOT/name).read_text()+'\n')
    chunks.append('\n===== ST HAL BSD-3-Clause license (headers reference this license) =====\n'+(HERE/'tools/ST-BSD-3-Clause.txt').read_text())
    data=''.join(chunks)
    for required in ("Fred's Lab", 'Oliver Rockstedt', 'STMicroelectronics', 'ARM', 'Emilie Gillet'):
        if required not in data:
            raise ValueError('required notice missing: '+required)
    (out/'NOTICE.txt').write_text(data)
    (out/'notice-inventory.json').write_text(json.dumps(sorted(files),indent=2)+'\n')


if __name__ == '__main__':
    out=Path(sys.argv[1])
    proof(out)
    notices(out)
    package(out, HERE, WAV, ZIP, COMBO, COMBO_HEX, COMBO_ZIP)
