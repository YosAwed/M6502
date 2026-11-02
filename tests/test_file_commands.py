#!/usr/bin/env python3
"""Exercise file commands through the real BASIC prompt, in isolated folders."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BASIC = Path(sys.argv.pop(1) if len(sys.argv) > 1 else "./basic").resolve()


class FileCommandsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="m6502-test-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def source(self, name, content):
        path = self.directory / name
        path.write_bytes(content.encode("utf-8") if isinstance(content, str) else content)
        return path

    def run_basic(self, commands):
        result = subprocess.run(
            [str(BASIC)],
            input=commands + "\nQUIT\n",
            cwd=self.directory,
            text=True,
            encoding="utf-8",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "", result.stderr)
        # Piped input is not echoed, so consecutive prompts can share a line.
        return result.stdout.replace("] ", "")

    def assert_success(self, output):
        self.assertFalse(any(line.startswith("?") for line in output.splitlines()), output)

    def assert_error(self, output):
        self.assertTrue(any(line.startswith("?") for line in output.splitlines()), output)

    def test_save_load_round_trip_and_run(self):
        output = self.run_basic(
            '20 END\n10 PRINT "ROUND TRIP"\nSAVE "my program.bas"\nNEW\n'
            'LOAD "my program.bas"\nLIST\nRUN'
        )
        self.assert_success(output)
        self.assertEqual(
            (self.directory / "my program.bas").read_bytes(),
            b'10 PRINT "ROUND TRIP"\n20 END\n',
        )
        self.assertIn('10 PRINT "ROUND TRIP"\n20 END\n', output)
        self.assertIn("\nROUND TRIP\n", output)

    def test_load_does_not_run_program(self):
        self.source("test.bas", '10 PRINT "EXECUTED"\n')
        output = self.run_basic('LOAD "test.bas"')
        self.assert_success(output)
        self.assertNotIn("EXECUTED", output)

    def test_bundled_example_can_be_loaded_and_run(self):
        example = Path(__file__).resolve().parent.parent / "prog.bas"
        self.source("prog.bas", example.read_bytes())
        output = self.run_basic('LOAD "prog.bas"\nRUN')
        self.assert_success(output)
        self.assertIn("\nMicrosoft BASIC M6502\n", output)
        self.assertIn("\n10 100\n", output)

    def test_load_accepts_bom_crlf_blank_lines_and_missing_final_newline(self):
        self.source("windows.bas", b'\xef\xbb\xbf\r\n  \r\n20 END\r\n10 PRINT "CRLF"')
        output = self.run_basic('LOAD "windows.bas"\nSAVE "normalized.bas"\nRUN')
        self.assert_success(output)
        self.assertEqual(
            (self.directory / "normalized.bas").read_bytes(),
            b'10 PRINT "CRLF"\n20 END\n',
        )
        self.assertIn("\nCRLF\n", output)

    def test_load_orders_replaces_and_deletes_duplicate_line_numbers(self):
        self.source("edit.bas", '20 PRINT "DELETE"\n10 PRINT "OLD"\n10 PRINT "NEW"\n20\n')
        output = self.run_basic('LOAD "edit.bas"\nSAVE "result.bas"')
        self.assert_success(output)
        self.assertEqual((self.directory / "result.bas").read_text(), '10 PRINT "NEW"\n')

    def test_empty_load_clears_program(self):
        self.source("empty.bas", "\n \t\n")
        output = self.run_basic('10 PRINT "OLD"\nLOAD "empty.bas"\nSAVE "result.bas"')
        self.assert_success(output)
        self.assertEqual((self.directory / "result.bas").read_bytes(), b"")

    def test_failed_load_preserves_program_variables_and_does_not_execute_source(self):
        invalid_sources = {
            "immediate": b'10 PRINT "PARTIAL"\nPRINT "INJECTED"\n',
            "negative": b'-1 PRINT "BAD"\n',
            "fraction": b'1.5 PRINT "BAD"\n',
            "overflow": b'65536 PRINT "BAD"\n',
            "huge": b'9999999999999999999999999 PRINT "BAD"\n',
            "separator": b'10PRINT "BAD"\n',
            "nul": b'10 PRINT "BAD"\0PRINT "INJECTED"\n',
            "long": b'10 REM ' + b'x' * 69 + b'\n',
        }
        for name, contents in invalid_sources.items():
            with self.subTest(name=name):
                self.source("bad.bas", contents)
                output = self.run_basic(
                    '10 PRINT "ORIGINAL"\nA = 314\nLOAD "bad.bas"\n'
                    'SAVE "preserved.bas"\nPRINT A'
                )
                self.assert_error(output)
                self.assertEqual(
                    (self.directory / "preserved.bas").read_text(), '10 PRINT "ORIGINAL"\n'
                )
                self.assertIn("314", output)
                self.assertNotIn("INJECTED", output)

    def test_missing_file_preserves_program(self):
        output = self.run_basic('10 END\nLOAD "missing.bas"\nSAVE "preserved.bas"')
        self.assert_error(output)
        self.assertEqual((self.directory / "preserved.bas").read_text(), "10 END\n")

    def test_failed_load_preserves_data_cursor(self):
        self.source("bad.bas", '10 END\nPRINT "INVALID"\n')
        output = self.run_basic('DATA 17, 29\nREAD A\nLOAD "bad.bas"\nREAD B\nPRINT B')
        self.assert_error(output)
        self.assertIn("\n29\n", output)
        self.assertNotIn("OUT OF DATA", output)

    def test_load_reports_physical_line_of_invalid_record(self):
        self.source("bad.bas", '\n10 END\nPRINT "INVALID"\n')
        output = self.run_basic('LOAD "bad.bas"')
        self.assert_error(output)
        self.assertRegex(output.lower(), r"line\s+3\b")

    def test_load_maximum_line_number_and_body_length(self):
        source = "0 REM ZERO\n65535 REM " + "x" * 68 + "\n"
        self.source("limits.bas", source)
        output = self.run_basic('LOAD "limits.bas"\nSAVE "limits-copy.bas"')
        self.assert_success(output)
        self.assertEqual((self.directory / "limits-copy.bas").read_text(), source)

    def test_load_line_count_limit_is_transactional(self):
        valid = "".join(f"{number} REM TEST\n" for number in range(1, 1001))
        self.source("full.bas", valid)
        self.source("too-many.bas", valid + "1001 END\n")
        output = self.run_basic(
            'LOAD "full.bas"\nSAVE "full-copy.bas"\n'
            'LOAD "too-many.bas"\nSAVE "preserved.bas"'
        )
        self.assert_error(output)
        self.assertEqual((self.directory / "full-copy.bas").read_text(), valid)
        self.assertEqual((self.directory / "preserved.bas").read_text(), valid)

    def test_successful_load_clears_variables_and_data(self):
        self.source("new.bas", "10 END\n")
        output = self.run_basic('A = 314\nDATA 99\nLOAD "new.bas"\nPRINT A\nREAD B')
        self.assertIn("\n0\n", output)
        self.assertNotIn("314", output)
        self.assertIn("OUT OF DATA", output)

    def test_successful_load_clears_control_stacks(self):
        self.source("new.bas", "10 END\n")
        cases = [
            ('10 GOSUB 100\n20 END\n100 STOP', "RETURN", "RETURN WITHOUT GOSUB"),
            ('10 FOR I = 1 TO 2\n20 STOP', "NEXT I", "NEXT WITHOUT FOR"),
        ]
        for program, command, error in cases:
            with self.subTest(command=command):
                output = self.run_basic(program + '\nRUN\nLOAD "new.bas"\n' + command)
                self.assertIn(error, output)

    def test_load_after_stop_invalidates_continue(self):
        self.source("new.bas", "10 END\n")
        output = self.run_basic('10 STOP\nRUN\nLOAD "new.bas"\nCONT')
        self.assertIn("CAN'T CONTINUE", output)

    def test_save_replaces_existing_file_and_leaves_no_temporary_files(self):
        self.source("existing.bas", "old file contents")
        output = self.run_basic('10 END\nSAVE "existing.bas"')
        self.assert_success(output)
        self.assertEqual((self.directory / "existing.bas").read_bytes(), b"10 END\n")
        self.assertEqual(sorted(p.name for p in self.directory.iterdir()), ["existing.bas"])

    def test_save_failures_preserve_destination_and_cleanup(self):
        destination = self.directory / "destination.bas"
        destination.mkdir()
        (destination / "sentinel").write_text("keep")
        output = self.run_basic('10 END\nSAVE "destination.bas"\nSAVE "missing/new.bas"')
        self.assert_error(output)
        self.assertEqual((destination / "sentinel").read_text(), "keep")
        self.assertEqual(sorted(p.name for p in self.directory.iterdir()), ["destination.bas"])

    def test_invalid_arguments_have_no_side_effects(self):
        self.source("existing.bas", "10 END\n")
        for command in [
            'SAVE', 'SAVE ""', 'SAVE 42', 'SAVE "existing.bas" JUNK',
            'SAVE "existing.bas", 1', 'SAVE "existing.bas',
            'LOAD', 'LOAD ""', 'LOAD 42', 'LOAD "existing.bas" JUNK',
            'LOAD "existing.bas", 1', 'LOAD "existing.bas',
            'FILES 42', 'FILES "existing.bas" JUNK',
        ]:
            with self.subTest(command=command):
                output = self.run_basic('10 PRINT "KEEP"\n' + command + '\nSAVE "verify.bas"')
                self.assert_error(output)
                self.assertEqual((self.directory / "existing.bas").read_text(), "10 END\n")
                self.assertEqual((self.directory / "verify.bas").read_text(), '10 PRINT "KEEP"\n')

    def test_colons_and_conditional_file_commands(self):
        output = self.run_basic(
            '10 PRINT "HELLO"\nIF 1 THEN SAVE "one.bas":PRINT "SAVED"\n'
            'NEW\nIF 1 THEN LOAD "one.bas":SAVE "two.bas"\n'
            'IF 0 THEN LOAD "absent.bas"\nFILES:PRINT "LISTED"'
        )
        self.assert_success(output)
        self.assertEqual((self.directory / "one.bas").read_bytes(), (self.directory / "two.bas").read_bytes())
        self.assertIn("\nSAVED\n", output)
        self.assertIn("\nLISTED\n", output)

    def test_load_during_program_execution_is_rejected(self):
        self.source("replacement.bas", '10 PRINT "REPLACED"\n')
        for body in ['LOAD "replacement.bas"', 'IF 1 THEN LOAD "replacement.bas"']:
            with self.subTest(body=body):
                output = self.run_basic(f'10 {body}\nRUN\nSAVE "retained.bas"')
                self.assert_error(output)
                self.assertEqual((self.directory / "retained.bas").read_text(), f"10 {body}\n")

    def test_save_and_files_work_inside_programs(self):
        output = self.run_basic('10 SAVE "self.bas"\n20 IF 1 THEN FILES\n30 END\nRUN')
        self.assert_success(output)
        self.assertIn("self.bas", output)
        self.assertEqual(
            (self.directory / "self.bas").read_text(),
            '10 SAVE "self.bas"\n20 IF 1 THEN FILES\n30 END\n',
        )

    def test_files_lists_bas_files_sorted_and_ignores_directories(self):
        for name in ["z.bas", "a.bas", "upper.BAS", "not-basic.txt"]:
            self.source(name, "10 END\n")
        (self.directory / "directory.bas").mkdir()
        output = self.run_basic("FILES")
        self.assert_success(output)
        self.assertIn(str(self.directory.resolve()), output)
        self.assertLess(output.index("a.bas"), output.index("upper.BAS"))
        self.assertLess(output.index("upper.BAS"), output.index("z.bas"))
        self.assertNotIn("not-basic.txt", output)
        self.assertNotIn("directory.bas", output)

    def test_files_accepts_directory_with_spaces_without_changing_working_directory(self):
        subdirectory = self.directory / "program folder"
        subdirectory.mkdir()
        (subdirectory / "nested.bas").write_text("10 END\n")
        output = self.run_basic('FILES "program folder"\n10 END\nSAVE "here.bas"')
        self.assert_success(output)
        self.assertIn("nested.bas", output)
        self.assertTrue((self.directory / "here.bas").exists())
        self.assertFalse((subdirectory / "here.bas").exists())

    def test_files_empty_or_missing_directory(self):
        self.assert_success(self.run_basic("FILES"))
        self.assert_error(self.run_basic('FILES "missing"'))

    def test_long_command_path_is_not_split_at_program_line_limit(self):
        name = "p" * 90 + ".bas"
        output = self.run_basic(f'10 END\nSAVE "{name}"\nNEW\nLOAD "{name}"\nLIST')
        self.assert_success(output)
        self.assertIn("10 END", output)

    def test_oversized_input_discards_the_entire_physical_line(self):
        output = self.run_basic(' ' * 4097 + 'SAVE "unexpected.bas"\nPRINT "RECOVERED"')
        self.assert_error(output)
        self.assertFalse((self.directory / "unexpected.bas").exists())
        self.assertIn("\nRECOVERED\n", output)


if __name__ == "__main__":
    unittest.main()
