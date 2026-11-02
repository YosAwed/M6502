#!/usr/bin/env python3
"""BASIC本体のメモリ安全性と基本動作の回帰テスト。"""

from pathlib import Path
import subprocess
import sys
import unittest

BASIC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else "./basic").resolve()


class CoreRegressionsTest(unittest.TestCase):
    def run_basic(self, commands):
        result = subprocess.run(
            [str(BASIC)],
            input=commands.rstrip("\n") + "\nQUIT\n",
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=5,
        )
        self.assertEqual(
            result.returncode, 0,
            f"exit={result.returncode}\n{result.stdout}\n{result.stderr}",
        )
        return result.stdout.replace("] ", "")

    def test_array_size_overflow_is_rejected(self):
        output = self.run_basic("DIM A(300,300)\nPRINT 7")
        self.assertIn("Array too large", output)
        self.assertIn("7\n", output)

    def test_array_valid_multidimensional_index(self):
        output = self.run_basic("DIM A(2,3)\nA(2,3)=99\nPRINT A(2,3)")
        self.assertNotIn("?", output)
        self.assertIn("99\n", output)

    def test_array_out_of_range_index(self):
        output = self.run_basic("DIM A(2)\nA(-65536)=1\nPRINT A(0)")
        self.assertIn("SUBSCRIPT OUT OF RANGE", output)
        self.assertIn("0\n", output)

    def test_array_rejects_excessive_dimension(self):
        output = self.run_basic("DIM A(65535)\nPRINT 7")
        self.assertIn("Invalid array dimension", output)
        self.assertIn("7\n", output)


if __name__ == "__main__":
    unittest.main()
