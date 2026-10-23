"""Functional regressions; CHS_BINARY can select a sanitizer build."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

BINARY = Path(os.environ.get('CHS_BINARY', './chs')).resolve()
ROOT = Path(__file__).resolve().parents[1]


class ChsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.sample = self.csv('constant.csv', 'dist,time,heartrate\n0,0,145\n100,30,145\n200,30,145\n')

    def csv(self, name, content):
        path = self.directory / name
        path.write_text(content)
        return str(path)

    def run_chs(self, *args, success=True):
        result = subprocess.run([str(BINARY), *args], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr)
        self.assertNotIn('AddressSanitizer', result.stderr)
        self.assertNotIn('runtime error:', result.stderr)
        return result

    def test_known_statistics(self):
        result = self.run_chs('-h', '145', '-p', '5:00.0', self.sample)
        self.assertIn('145.0(2)\t5:00.00(2)', result.stdout)
        self.assertIn('2 Records', result.stdout)
        self.assertIn('145.0\t5:00.00\t1 files', result.stdout)

    def test_option_order_and_fractional_pace(self):
        sample = self.csv('fraction.csv', 'heartrate,time,dist\n145,30.5,100')
        result = self.run_chs('--pace', '5:05.0', '--heartrate', '145', sample)
        self.assertIn('145.0(1)\t5:05.00(1)', result.stdout)

    def test_no_matches(self):
        result = self.run_chs('-h', '180', '-p', '9:00.0', self.sample)
        self.assertIn('N/A(0)\tN/A(0)', result.stdout)
        self.assertNotIn('nan', result.stdout.lower())
        self.assertNotIn('inf', result.stdout.lower())

    def test_independent_summary_denominators(self):
        other = self.csv('other.csv', 'dist,time,heartrate\n100,60,180\n')
        result = self.run_chs('-h', '145', '-p', '10:00.0', self.sample, other)
        self.assertIn('180.0\t5:00.00\t2 files', result.stdout)

    def test_missing_file_does_not_prevent_next_file(self):
        result = self.run_chs('-h', '145', '-p', '5:00.0', str(self.directory / 'missing'), self.sample, success=False)
        self.assertIn('145.0(2)\t5:00.00(2)', result.stdout)
        self.assertIn('1 files', result.stdout)

    def test_multiple_files(self):
        result = self.run_chs('-h', '145', '-p', '5:00.0', self.sample, self.sample)
        self.assertEqual(result.stdout.count('145.0(2)\t5:00.00(2)'), 2)
        self.assertIn('2 files', result.stdout)

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

    def test_crlf_empty_unrelated_columns_and_short_workouts(self):
        path = self.csv('windows.csv', 'unused,dist,time,heartrate\r\n,100,30,145\r\n')
        result = self.run_chs('-h', '145', '-p', '5:00.0', path)
        self.assertIn('145.0(1)\t5:00.00(1)', result.stdout)

    def test_zero_intervals_and_sensor_dropouts(self):
        path = self.csv('dropouts.csv', 'dist,time,heartrate\n0,0,145\n0,30,145\n100,30,0\n200,30,145\n')
        result = self.run_chs('-h', '145', '-p', '5:00.0', path)
        self.assertIn('145.0(1)\t5:00.00(1)', result.stdout)

    def test_output_file(self):
        output = self.directory / 'report.txt'
        result = self.run_chs('-h', '145', '-p', '5:00.0', '-o', str(output), self.sample)
        self.assertEqual(result.stdout, '')
        self.assertIn('145.0(2)\t5:00.00(2)', output.read_text())

    def test_output_failure(self):
        self.run_chs('-h', '145', '-p', '5:00.0', '-o', str(self.directory / 'absent' / 'out'), self.sample, success=False)

    def test_bundled_sample(self):
        result = self.run_chs('-h', '145', '-p', '5:30.0', str(ROOT / 'test.csv'))
        self.assertIn('1 files', result.stdout)
        self.assertNotIn('N/A', result.stdout)

    def test_help(self):
        self.assertIn('Usage:', self.run_chs('--help').stderr)


if __name__ == '__main__':
    unittest.main()
