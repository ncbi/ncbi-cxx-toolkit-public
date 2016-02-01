#! /usr/bin/env python3

import unittest
import re
import os
import subprocess

GAP_STATS_EXE = "./gap_stats"
if not os.path.exists(GAP_STATS_EXE):
    raise Exception("Not found: " + GAP_STATS_EXE)

TEST_DATA_DIR = "./test_data"
if not os.path.exists(TEST_DATA_DIR):
    raise Exception("Not found: " + TEST_DATA_DIR)


class TestGapStats(unittest.TestCase):

    def _run_gap_stats(self, gap_stats_args):
        """
        Cannot assume that subprocess.run is available, so we simulate
        something like that

        :returns: this tuple: (returncode, stdout str, stderr str)
        """
        assert isinstance(gap_stats_args, list)

        proc = subprocess.Popen(
            ['./gap_stats'] + gap_stats_args,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            )
        # never give stdin input
        (stdout_str, stderr_str) = proc.communicate('')

        # remove irrelevant applog-style messages from stderr, if any
        stderr_irrelevant_lines_re = re.compile(
            br'UNK_SESSION\s*gap_stats\s*(start|stop|extra)')
        stderr_str = b'\n'.join(
            filter((lambda line: not stderr_irrelevant_lines_re.search(line)),
                   stderr_str.split(b'\n'))
        )

        return (proc.returncode, stdout_str, stderr_str)

    def _file_contents(self, file_path):
        with open(file_path, 'rb') as fd:
            return fd.read()

    def test_basic(self):
        (returncode, stdout_str, stderr_str) = self._run_gap_stats([
            '-show-seqs-for-gap-lengths', '-show-hist', '-hist-algo',
            'even_bins', './test_data/basic.fsa'])
        self.assertEqual(returncode, 0)
        self.assertFalse(stderr_str)
        self.assertEqual(
            stdout_str,
            self._file_contents(
                './test_data/basic.expected_stdout.txt'))

    def test_with_accession(self):
        (returncode, stdout_str, stderr_str) = self._run_gap_stats(['U54469'])
        self.assertEqual(returncode, 0)
        self.assertFalse(stderr_str)
        self.assertEqual(stdout_str, b'SUMMARY:\n\n')

    def test_with_accession_no_gbload(self):
        (returncode, _, stderr_str) = self._run_gap_stats(
            ['-no-gbload', 'U54469'])
        self.assertNotEqual(returncode, 0)
        self.assertRegex(
            stderr_str, br'.*U54469: Accession could not be found.*')

    def test_with_accession_that_has_gaps(self):
        (returncode, stdout_str, stderr_str) = self._run_gap_stats(
            ['AH008761.2'])
        self.assertEqual(returncode, 0)
        self.assertFalse(stderr_str)
        self.assertEqual(
            stdout_str,
            self._file_contents(
                './test_data/'
                'test_with_accession_that_has_gaps.expected_stdout.txt'))

    def test_with_specified_gap_types(self):
        (returncode, stdout_str, stderr_str) = self._run_gap_stats(
            ['-show-hist', '-show-seqs-for-gap-lengths', '-gap-types',
                'seq-gaps,unknown-bases', 'AH008761.2'])
        self.assertEqual(returncode, 0)
        self.assertFalse(stderr_str)
        self.assertEqual(
            stdout_str,
            self._file_contents(
                './test_data/'
                'test_with_specified_gap_types.expected_stdout.txt'))

    def test_with_non_file_argument(self):
        (returncode, _, stderr_str) = self._run_gap_stats(['/dev/null'])
        self.assertNotEqual(returncode, 0)
        self.assertRegex(stderr_str, br'.*not a plain file: /dev/null.*')

    def test_with_bad_asn(self):
        (returncode, _, stderr_str) = self._run_gap_stats([
            './test_data/invalid.asn'])
        self.assertNotEqual(returncode, 0)
        self.assertRegex(stderr_str, br'.*format is not.*supported.*')

    def test_delta_to_other_seqs(self):
        (returncode, stdout_str, stderr_str) = self._run_gap_stats(
            ['-show-hist', '-show-seqs-for-gap-lengths',
                './test_data/delta_to_other_seqs.asn'])
        self.assertEqual(returncode, 0)
        self.assertFalse(stderr_str)
        self.assertEqual(
            stdout_str,
            self._file_contents(
                './test_data/delta_to_other_seqs.expected_stdout.txt'))

    def test_insufficient_max_resolve_count(self):
        (returncode, stdout_str, stderr_str) = self._run_gap_stats(
            ['-max-resolve-count', '0', '-show-hist',
                '-show-seqs-for-gap-lengths',
                './test_data/delta_to_other_seqs.asn'])
        self.assertEqual(returncode, 0)
        self.assertRegex(stderr_str, br'.*Not all segments.*')
        self.assertEqual(
            stdout_str,
            self._file_contents(
                './test_data/'
                'delta_to_other_seqs.no_resolve.expected_stdout.txt'))


if __name__ == '__main__':
    # make sure we get warnings even in Release mode
    os.environ['DIAG_POST_LEVEL'] = 'Warning'

    unittest.main()
