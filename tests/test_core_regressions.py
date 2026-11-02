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

    def test_delete_stopped_line_invalidates_continuation(self):
        output = self.run_basic(
            '10 STOP\n20 END\nRUN\n10\nCONT\nPRINT 7'
        )
        self.assertIn("CAN'T CONTINUE", output)
        self.assertIn("7\n", output)

    def test_replace_stopped_line_invalidates_continuation(self):
        output = self.run_basic(
            '10 STOP\n20 END\nRUN\n10 PRINT 99\nCONT\nPRINT 7'
        )
        self.assertIn("CAN'T CONTINUE", output)
        self.assertIn("7\n", output)

    def test_delete_gosub_return_line_during_stop(self):
        output = self.run_basic(
            '10 GOSUB 100\n20 PRINT "AFTER":END\n'
            '100 STOP\n110 RETURN\nRUN\n10\nCONT\nPRINT 7'
        )
        self.assertIn("CAN'T CONTINUE", output)
        self.assertIn("7\n", output)
        self.assertNotIn("AFTER\n", output)

    def test_stop_cont_resumes_on_next_line(self):
        output = self.run_basic(
            '10 PRINT "BEFORE"\n20 STOP\n30 PRINT "AFTER"\nRUN\nCONT'
        )
        self.assertIn("BEFORE\n", output)
        self.assertIn("BREAK IN 20", output)
        self.assertIn("AFTER\n", output)
        self.assertNotIn("?", output)

    def test_stop_cont_resumes_within_same_line(self):
        output = self.run_basic(
            '10 PRINT "BEFORE":STOP:PRINT "AFTER":END\nRUN\nCONT'
        )
        self.assertIn("BREAK IN 10", output)
        self.assertEqual(output.count("AFTER\n"), 1, output)

    def test_cont_without_stop_is_rejected(self):
        output = self.run_basic("CONT")
        self.assertIn("CAN'T CONTINUE", output)

    def test_end_invalidates_stop_continuation(self):
        output = self.run_basic("10 STOP\n20 END\nRUN\nCONT\nCONT")
        self.assertIn("CAN'T CONTINUE", output)

    def test_program_edit_invalidates_stop_continuation(self):
        output = self.run_basic("10 STOP\n20 PRINT 7\nRUN\n20 PRINT 8\nCONT")
        self.assertIn("CAN'T CONTINUE", output)
        self.assertNotIn("8\n", output)

    def test_goto_skips_remainder_of_source_line(self):
        output = self.run_basic(
            '10 GOTO 30:PRINT "WRONG"\n'
            '20 PRINT "SKIPPED"\n30 PRINT "OK"\nRUN'
        )
        self.assertIn("OK\n", output)
        self.assertNotIn("WRONG\n", output)
        self.assertNotIn("SKIPPED\n", output)

    def test_goto_same_line_does_not_advance(self):
        output = self.run_basic(
            "10 A=A+1:IF A<3 THEN 10\n20 PRINT A\nRUN"
        )
        self.assertIn("3\n", output)

    def test_gosub_return_skips_remainder_and_resumes(self):
        output = self.run_basic(
            '10 GOSUB 100:PRINT "RETURNED":END\n'
            '100 PRINT "SUB":RETURN:PRINT "WRONG"\nRUN'
        )
        self.assertIn("SUB\n", output)
        self.assertIn("RETURNED\n", output)
        self.assertNotIn("WRONG\n", output)

    def test_run_in_program_restarts_without_recursive_stack_growth(self):
        output = self.run_basic(
            '10 A=A+1\n20 IF A<3000 THEN 40\n30 PRINT A:END\n'
            '40 RUN:PRINT "SHOULD NOT PRINT"\nRUN'
        )
        self.assertIn("3000\n", output)
        self.assertNotIn("SHOULD NOT PRINT\n", output)

    def test_oversized_exponent_reports_error(self):
        output = self.run_basic("PRINT 1E999999999999\nPRINT 7")
        self.assertIn("Invalid number format", output)
        self.assertIn("7\n", output)

    def test_normal_exponent_unchanged(self):
        output = self.run_basic("PRINT 1E2")
        self.assertNotIn("?", output)
        self.assertIn("100\n", output)

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

    def test_unterminated_string_reports_error(self):
        output = self.run_basic('PRINT "unterminated\nPRINT 7')
        self.assertIn("Unterminated string", output)
        self.assertIn("7\n", output)

    def test_unterminated_string_assignment_reports_error(self):
        output = self.run_basic('A$="unterminated\nPRINT 7')
        self.assertIn("Unterminated string", output)
        self.assertIn("7\n", output)

    def test_compound_comparisons_with_spaces(self):
        output = self.run_basic(
            "PRINT 1 <= 1\nPRINT 1 >= 1\nPRINT 1 <> 2\nPRINT 2<>2"
        )
        self.assertNotIn("?", output)
        self.assertIn("-1\n", output)
        self.assertEqual(output.count("-1\n"), 3, output)
        self.assertIn("0\n", output)

    def test_string_array_clear_releases_elements(self):
        output = self.run_basic(
            'DIM A$(2)\nA$(0)="X"\nA$(1)="Y"\nCLEAR\nPRINT 7'
        )
        self.assertNotIn("?", output)
        self.assertIn("7\n", output)

    def test_string_array_new_releases_elements(self):
        output = self.run_basic(
            'DIM A$(2)\nA$(1)="HELLO"\nNEW\nPRINT 7'
        )
        self.assertNotIn("?", output)
        self.assertIn("7\n", output)

    def test_string_array_process_exit_releases_elements(self):
        output = self.run_basic('DIM A$(2)\nA$(1)="HELLO"\nPRINT 7')
        self.assertNotIn("?", output)
        self.assertIn("7\n", output)


if __name__ == "__main__":
    unittest.main()
