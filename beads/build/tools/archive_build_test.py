#!/usr/bin/env python3
"""Transport retains exact build bytes and failures, including partial logs."""
import hashlib
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest


SCRIPT = Path(__file__).with_name('archive_build.sh')


class TransportTest(unittest.TestCase):
  def run_build(self, status, archive_failure=False):
    with tempfile.TemporaryDirectory() as name:
      root = Path(name)
      out = root / 'output with spaces'
      out.mkdir()
      payload = bytes(range(256)) * 1024
      (out / 'app.bin').write_bytes(payload)
      (out / 'build.log').write_text('compiler output\n')
      archive = root / ('missing/output.tar.gz' if archive_failure else 'output.tar.gz')
      run = subprocess.run(['bash', str(SCRIPT), str(out), str(archive),
                            'bash', '-c', 'exit "$1"', 'build', str(status)],
                           capture_output=True)
      if archive_failure:
        self.assertNotEqual(run.returncode, 0)
        if status:
          self.assertEqual(run.returncode, status)
        return
      self.assertEqual(run.returncode, status)
      restored = root / 'restored'
      restored.mkdir()
      with tarfile.open(archive, 'r:gz') as packed:
        packed.extractall(restored)
      for source in out.iterdir():
        self.assertEqual(hashlib.sha256(source.read_bytes()).digest(),
                         hashlib.sha256((restored / source.name).read_bytes()).digest())
      self.assertLess(archive.stat().st_size, len(payload) // 10)

  def test_success(self):
    self.run_build(0)

  def test_build_failure_keeps_logs_and_status(self):
    self.run_build(23)

  def test_archive_failure(self):
    self.run_build(0, archive_failure=True)

  def test_build_failure_takes_priority(self):
    self.run_build(23, archive_failure=True)


if __name__ == '__main__':
  unittest.main()
