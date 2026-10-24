# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)

import struct
import unittest
import dwt


class DecoderTest(unittest.TestCase):
    def sample(self):
        b = bytearray(dwt.SIZE)
        struct.pack_into("<III", b, 0, dwt.MAGIC, dwt.VERSION, 2)
        struct.pack_into("<I", b, 16, 7)
        struct.pack_into("<IIQ", b, 24, 3999, 1000, 3000000)
        struct.pack_into("<I", b, 44, 999)
        struct.pack_into("<I", b, 40 + 63 * 4, 1)
        return b

    def test_layout(self):
        b = self.sample()
        b[13] = 1
        struct.pack_into("<I", b, 20, 16000)
        row = dwt.decode(b)
        self.assertTrue(row["fixture"])
        self.assertEqual(row["blocks_elapsed"], 16000)

    def test_quantiles(self):
        row = dwt.decode(self.sample())["brackets"][0]
        self.assertEqual(row["median_interval"], [2000, 3999])
        self.assertEqual(row["p999_interval"], [2000, 3999])
        self.assertEqual(row["mean"], 3000)

    def test_overflow(self):
        b = self.sample()
        struct.pack_into("<I", b, 44, 998)
        struct.pack_into("<I", b, 292, 2)
        self.assertEqual(
            dwt.decode(b)["brackets"][0]["p999_interval"], [126000, None]
        )

    def test_reject(self):
        for offset, value in [(0, 0), (4, 1), (8, 3), (28, 5), (1112, 1)]:
            b = self.sample()
            struct.pack_into("<I", b, offset, value)
            with self.assertRaises(ValueError):
                dwt.decode(b)
        with self.assertRaises(ValueError):
            dwt.decode(b"")

    def frame(self, b, channel, values):
        struct.pack_into("<10IQQ", b, 1120 + 56 * channel, *values)

    def test_frames(self):
        b = self.sample()
        self.frame(b, 0, (7, 2813, 2810, 0, 1, 900000, 300000, 5, 2, 0,
                          2810 * 800000, 2810 * 250000))
        self.frame(b, 1, (6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
        left, right = dwt.decode(b)["frames"]["channels"]
        self.assertTrue(left["current"])
        self.assertEqual(left["produced"], 2813)
        self.assertEqual(left["skipped"], 0)
        self.assertEqual(left["wall_maximum"], 900000)
        self.assertEqual(left["wall_mean"], 800000)
        self.assertEqual(left["exclusive_maximum"], 300000)
        self.assertEqual(left["exclusive_mean"], 250000)
        self.assertEqual(left["discontinuities"], 2)
        self.assertFalse(right["current"])

    def test_frame_reject(self):
        for values in [(7, 3, 3, 1, 2, 9, 9, 0, 0, 0, 27, 27),
                       (7, 3, 3, 0, 1, 9, 10, 0, 0, 0, 27, 27),
                       (7, 3, 3, 0, 0, 9, 9, 0, 0, 0, 27, 27),
                       (7, 4, 3, 0, 2, 9, 9, 0, 0, 0, 27, 27),
                       (7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)]:
            b = self.sample()
            self.frame(b, 0, values)
            with self.assertRaises(ValueError):
                dwt.decode(b)

    def test_commands(self):
        read = dwt.openocd("read")
        self.assertIn("mdw 0x30041000 308", read)
        self.assertIn("dump_image capture.bin 0x30041000 1232", read)
        self.assertIn('$first != $last', read)
        self.assertIn("[mrw 0x30041458]", read)
        self.assertIn('$first_frames != $last_frames', read)
        self.assertNotIn("mwb", read)
        self.assertEqual(dwt.openocd("reset").count("mwb "), 1)
        self.assertIn("mwb 0x3004100c 1", dwt.openocd("reset"))
        self.assertEqual(dwt.openocd("sai"), "mdw 0x4001581c 1\nmdw 0x4001583c 1\n")


if __name__ == "__main__":
    unittest.main()
