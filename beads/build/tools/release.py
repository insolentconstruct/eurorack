#!/usr/bin/env python3
"""Check the WAV with the vendor decoder, collect notices, package the release."""
import hashlib
import os
import json
import re
import shlex
import subprocess
import sys
import tempfile
import wave
import zipfile
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

ROOT = Path('/src')
HERE = ROOT / 'beads/build'
NAME = os.environ.get('RELEASE_NAME', 'drizzlet-dev')
if not re.fullmatch(r'drizzlet-[A-Za-z0-9._-]+', NAME):
  raise SystemExit('RELEASE_NAME must match drizzlet-[A-Za-z0-9._-]+')
WAV = NAME + '.wav'
ZIP = NAME + '-wav.zip'
COMBO = NAME + '-combo.bin'
COMBO_ZIP = NAME + '-combo.zip'
ASSETS = [ZIP, COMBO_ZIP, 'NOTICE.txt']


def sha(path):
  return hashlib.sha256(path.read_bytes()).hexdigest()


def wav_read(path):
  with wave.open(str(path), 'rb') as w:
    if (w.getnchannels(), w.getsampwidth(), w.getframerate()) != (1, 2, 48000):
      raise ValueError('expected mono PCM16 48000 Hz')
    return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(np.float64) / 32768


def wav_write(path, pcm):
  with wave.open(str(path), 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000)
    w.writeframes(np.rint(pcm * 32768).astype('<i2').tobytes())


def decoded_ok(stats, payload, expected):
  return (stats['packets'] == 1280 and stats['blocks'] == 5 and stats['errors'] == 0
          and stats['overflow'] == 0 and stats['complete'] == 1 and payload == expected
          and len(payload) == 327680)


def proof(out):
  app = (out / 'app.bin').read_bytes()
  if len(app) != 268384:
    raise ValueError('unexpected application size')
  expected = app + b'\xff' * (327680 - len(app))
  wav_sha = sha(out / WAV)
  rows = []
  with tempfile.TemporaryDirectory(prefix='beads-decode-', dir=str(out)) as temp:
    work = Path(temp)
    sim = work / 'flashsim'
    subprocess.run(['g++', '-std=c++11', '-O2', '-I.', 'beads/build/tools/flashsim.cc',
                    'stm_audio_bootloader/qpsk/demodulator.cc',
                    'stm_audio_bootloader/qpsk/packet_decoder.cc', '-o', str(sim)], check=True, cwd=ROOT)
    x = wav_read(out / WAV)
    corrupt = x.copy(); corrupt[30*48000:31*48000] = 0
    corrupted = work / 'corrupted.wav'; wav_write(corrupted, corrupt)
    # Truncated ending: must not decode.
    truncated = work / 'missing-eot.wav'; wav_write(truncated, x[:-int(7.9*48000)])
    for label, path, rate, want in [('nominal-48000', out/WAV, 48000, True),
                                   ('resampled-44100', out/WAV, 44100, True),
                                   ('corrupted-file', corrupted, 48000, False),
                                   ('missing-eot', truncated, 48000, False)]:
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
      adc = work / 'samples.u16'; adc.write_bytes(((s16 >> 4) + 2048).astype('<u2').tobytes())
      payload = work / 'decoded.bin'
      run = subprocess.run([str(sim), str(adc), str(payload)], capture_output=True, text=True, check=True)
      stats = {k:int(v) for k,v in (x.split('=') for x in run.stdout.split())}
      data = payload.read_bytes()
      ok = decoded_ok(stats, data, expected)
      if ok != want:
        raise ValueError('unexpected decode result: '+label+' '+run.stdout)
      if label == 'missing-eot' and (stats['complete'] != 0 or data != expected
              or stats['packets'] != 1280 or stats['blocks'] != 5
              or stats['errors'] != 0 or stats['overflow'] != 0):
        raise ValueError('missing-EOT control must retain exact payload with only EOT absent')
      rows.append(dict(case=label, verdict='PASS' if want else 'CONTROL-OK',
                       input_sha256=sha(path), published_wav_sha256=wav_sha,
                       decoded_sha256=sha(payload), decoded_bytes=len(data), **stats))
      print(label, rows[-1]['verdict'], run.stdout.strip(), flush=True)
      if label == 'nominal-48000':
        wrong = bytearray(expected); wrong[1000] ^= 1
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
  tc = Path('/opt/gcc-arm-none-eabi-4_9-2015q2')
  licenses = sorted(p for p in tc.rglob('*') if p.is_file() and
                    re.search(r'(?i)(license|copying)',p.name) and p.suffix.lower() not in ('.pdf','.html','.info'))
  if not licenses:
    raise ValueError('missing compiler runtime licenses')
  for path in licenses:
    chunks.append('\n===== Toolchain: '+str(path.relative_to(tc))+' =====\n'+source_text(path)+'\n')
  for name in ('stm_audio_bootloader/LICENSE',):
    chunks.append('\n===== '+name+' =====\n'+(ROOT/name).read_text()+'\n')
  chunks.append('\n===== ST HAL BSD-3-Clause license (headers reference this license) =====\n'+(HERE/'tools/ST-BSD-3-Clause.txt').read_text())
  data=''.join(chunks)
  for required in ("Fred's Lab", 'Oliver Rockstedt', 'STMicroelectronics', 'ARM', 'Emilie Gillet'):
    if required not in data:
      raise ValueError('required notice missing: '+required)
  (out/'NOTICE.txt').write_text(data)
  (out/'notice-inventory.json').write_text(json.dumps(sorted(files),indent=2)+'\n')


def write_zip(path, names):
  # Reproducible zip.
  with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_STORED) as z:
    for name in sorted(names):
      info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0))
      info.create_system=0; info.external_attr=0; info.internal_attr=0; info.extra=b''
      z.writestr(info,(path.parent/name).read_bytes())
      info.external_attr=0  # writestr defaults Unix permissions; the central directory is written on close


def package(out):
  (out/'README.txt').write_bytes((HERE/'README.txt').read_bytes())
  write_zip(out/ZIP, [WAV, 'README.txt', 'NOTICE.txt'])
  # Combo image, named after the release.
  (out/COMBO).write_bytes((out/'combo.bin').read_bytes())
  write_zip(out/COMBO_ZIP, [COMBO, 'README.txt', 'NOTICE.txt'])
  (out/'SHA256SUMS').write_text(''.join(sha(out/name)+'  '+name+'\n' for name in ASSETS))
  subprocess.run(['sha256sum','-c','SHA256SUMS'],cwd=out,check=True)


if __name__ == '__main__':
  out=Path(sys.argv[1]); proof(out); notices(out); package(out)
