"""Functional regressions; CHS_BINARY can select a sanitizer build."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

BINARY = Path(os.environ.get('CHS_BINARY', './chs')).resolve()
ROOT = Path(__file__).resolve().parents[1]


class ChsTests(unittest.TestCase):
    ## @brief Create an isolated temporary directory and a constant-pace workout fixture.
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.sample = self.csv('constant.csv', 'dist,time,heartrate\n0,0,145\n100,30,145\n200,30,145\n')

    ## @brief Write a CSV fixture in the temporary directory and return its path.
    # @param name Fixture filename relative to the temporary directory.
    # @param content CSV text to write.
    # @return Absolute fixture path as a string.
    def csv(self, name, content):
        path = self.directory / name
        path.write_text(content)
        return str(path)

    ## @brief Run the selected CLI binary and assert its exit status and clean sanitizer output.
    # @param args Command-line arguments passed to CHS.
    # @param success Whether an exit status of zero is expected; otherwise expect one.
    # @return CompletedProcess containing captured stdout and stderr.
    # @exception AssertionError If status or sanitizer output differs from expectations.
    def run_chs(self, *args, success=True):
        result = subprocess.run([str(BINARY), *args], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr)
        self.assertNotIn('AddressSanitizer', result.stderr)
        self.assertNotIn('runtime error:', result.stderr)
        return result

    ## @brief Verify exact averages and matching counts for a known constant workout.
    def test_known_statistics(self):
        result = self.run_chs('-h', '145', '-p', '5:00.0', self.sample)
        self.assertIn('145.0(2)\t5:00.00(2)', result.stdout)
        self.assertIn('2 Records', result.stdout)
        self.assertIn('145.0\t5:00.00\t1 files', result.stdout)

    ## @brief Verify long-option ordering and fractional interval durations.
    def test_option_order_and_fractional_pace(self):
        sample = self.csv('fraction.csv', 'heartrate,time,dist\n145,30.5,100')
        result = self.run_chs('--pace', '5:05.0', '--heartrate', '145', sample)
        self.assertIn('145.0(1)\t5:05.00(1)', result.stdout)

    ## @brief Verify unmatched targets produce N/A rather than nonfinite statistics.
    def test_no_matches(self):
        result = self.run_chs('-h', '180', '-p', '9:00.0', self.sample)
        self.assertIn('N/A(0)\tN/A(0)', result.stdout)
        self.assertNotIn('nan', result.stdout.lower())
        self.assertNotIn('inf', result.stdout.lower())

    ## @brief Verify each summary statistic uses only files contributing that statistic.
    def test_independent_summary_denominators(self):
        other = self.csv('other.csv', 'dist,time,heartrate\n100,60,180\n')
        result = self.run_chs('-h', '145', '-p', '10:00.0', self.sample, other)
        self.assertIn('180.0\t5:00.00\t2 files', result.stdout)

    ## @brief Verify a missing input fails the command while later valid files are processed.
    def test_missing_file_does_not_prevent_next_file(self):
        result = self.run_chs('-h', '145', '-p', '5:00.0', str(self.directory / 'missing'), self.sample, success=False)
        self.assertIn('145.0(2)\t5:00.00(2)', result.stdout)
        self.assertIn('1 files', result.stdout)

    ## @brief Verify independent processing and summary counts across repeated inputs.
    def test_multiple_files(self):
        result = self.run_chs('-h', '145', '-p', '5:00.0', self.sample, self.sample)
        self.assertEqual(result.stdout.count('145.0(2)\t5:00.00(2)'), 2)
        self.assertIn('2 files', result.stdout)

    ## @brief Verify missing targets, malformed options, and unsupported options fail.
    def test_reject_invalid_arguments(self):
        for args in [[], ['-h', '145'], ['-p', '5:00.0'],
                     ['-h', '145junk', '-p', '5:00.0'],
                     ['-h', '145', '-p', '5:60.0'],
                     ['-h', '145', '-p', '5:00.0junk'],
                     ['-h', '145', '-p', '5:00'],
                     ['-h', '145', '-p', '0:00.0'],
                     ['--heartrate-range', '140:150']]:
            with self.subTest(args=args):
                self.run_chs(*args, self.sample, success=False)

    ## @brief Verify empty, malformed, nonfinite, and inconsistent CSV inputs fail.
    def test_invalid_inputs(self):
        for content in ['', 'dist,time,heartrate\n', 'wrong,header\n1,2\n',
                        'dist,time,heartrate\n100,,145\n',
                        'dist,time,heartrate\n100,30\n',
                        'dist,time,heartrate\n100,NaN,145\n',
                        'dist,time,heartrate\n100,30,145junk\n',
                        'dist,time,heartrate\n100,30,145\n90,30,145\n',
                        'dist,time,heartrate\n100,-30,145\n',
                        'dist,time,heartrate\n100,30,145\0\n']:
            with self.subTest(content=content):
                path = self.csv('bad.csv', content)
                self.run_chs('-h', '145', '-p', '5:00.0', path, success=False)

    ## @brief Verify CRLF and empty optional fields in a one-interval workout.
    def test_crlf_empty_unrelated_columns_and_short_workouts(self):
        path = self.csv('windows.csv', 'unused,dist,time,heartrate\r\n,100,30,145\r\n')
        result = self.run_chs('-h', '145', '-p', '5:00.0', path)
        self.assertIn('145.0(1)\t5:00.00(1)', result.stdout)

    ## @brief Verify stationary intervals and missing sensor readings are excluded.
    def test_zero_intervals_and_sensor_dropouts(self):
        path = self.csv('dropouts.csv', 'dist,time,heartrate\n0,0,145\n0,30,145\n100,30,0\n200,30,145\n')
        result = self.run_chs('-h', '145', '-p', '5:00.0', path)
        self.assertIn('145.0(1)\t5:00.00(1)', result.stdout)

    ## @brief Verify the report goes to the requested file instead of standard output.
    def test_output_file(self):
        output = self.directory / 'report.txt'
        result = self.run_chs('-h', '145', '-p', '5:00.0', '-o', str(output), self.sample)
        self.assertEqual(result.stdout, '')
        self.assertIn('145.0(2)\t5:00.00(2)', output.read_text())

    ## @brief Verify all input data is read before replacing an input with its report.
    def test_output_can_replace_input_after_analysis(self):
        self.run_chs('-h', '145', '-p', '5:00.0', '-o', self.sample, self.sample)
        self.assertIn('145.0(2)\t5:00.00(2)', Path(self.sample).read_text())

    ## @brief Verify a buffered output-write failure is propagated to the exit status.
    @unittest.skipUnless(Path('/dev/full').exists(), '/dev/full is not available')
    def test_output_write_failure(self):
        self.run_chs('-h', '145', '-p', '5:00.0', '-o', '/dev/full', self.sample, success=False)

    ## @brief Verify an unavailable output directory causes a failure status.
    def test_output_failure(self):
        self.run_chs('-h', '145', '-p', '5:00.0', '-o', str(self.directory / 'absent' / 'out'), self.sample, success=False)

    ## @brief Verify the bundled workout produces both statistics successfully.
    def test_bundled_sample(self):
        result = self.run_chs('-h', '145', '-p', '5:30.0', str(ROOT / 'test.csv'))
        self.assertIn('1 files', result.stdout)
        self.assertNotIn('N/A', result.stdout)

    ## @brief Verify help prints usage and returns a success status.
    def test_help(self):
        self.assertIn('Usage:', self.run_chs('--help').stderr)


if __name__ == '__main__':
    unittest.main()
