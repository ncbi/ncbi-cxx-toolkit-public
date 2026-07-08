#!/usr/bin/env python3
# $Id$
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================
#
# Author: Christiam Camacho
#
"""Functional test suite for the clusterdbcmd application.

The clusterdbcmd binary itself must be reachable via $PATH.

Usage: clusterdbcmd_test.py
   or: python3 -m unittest clusterdbcmd_test
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import clusterdbcmd_test_data as test_data  # noqa: E402


class TestClusterDBCmd(unittest.TestCase):
    """Exercises clusterdbcmd's retrieval modes, output formatting options
    and error paths against small, deterministic SQLite test databases."""

    tmpdir: tempfile.TemporaryDirectory
    workdir: str
    cluster_db: str
    env: dict[str, str]

    @classmethod
    def setUpClass(cls) -> None:
        cls.tmpdir = tempfile.TemporaryDirectory(prefix="clusterdbcmd_test.")
        cls.workdir = cls.tmpdir.name
        cls.cluster_db = os.path.join(cls.workdir, "cluster_test.sqlite3")
        tax_db = os.path.join(cls.workdir, "taxonomy4blast.sqlite3")
        test_data.create_cluster_db(cls.cluster_db)
        test_data.create_taxonomy_db(tax_db)

        # Make the auxiliary taxonomy database discoverable by clusterdbcmd
        # via SeqDB_ResolveDbPath (which honors BLASTDB and the current
        # working directory).
        cls.env = dict(os.environ)
        cls.env["BLASTDB"] = cls.workdir

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmpdir.cleanup()

    def _run(self, *args: str) -> subprocess.CompletedProcess[str]:
        """Invokes clusterdbcmd with the given arguments (honoring
        $CHECK_EXEC, the NCBI test suite's time-guard wrapper, when set)
        and returns the resulting subprocess.CompletedProcess."""
        cmd = list(os.environ["CHECK_EXEC"].split()) \
            if os.environ.get("CHECK_EXEC") else []
        cmd.append("clusterdbcmd")
        cmd.extend(args)
        timeout = float(os.environ.get("CHECK_TIMEOUT", "60"))
        return subprocess.run(cmd, cwd=self.workdir, env=self.env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, timeout=timeout)

    def _check(self, expected_rc: int, expected_out: str, *args: str) -> None:
        """Runs clusterdbcmd with ARGS and asserts both its exit code and
        stdout (trailing newlines stripped, like shell command
        substitution) match the expected values."""
        proc = self._run(*args)
        details = "\nstderr was: %r" % proc.stderr
        self.assertEqual(proc.returncode, expected_rc,
                          msg="unexpected exit code" + details)
        self.assertEqual(proc.stdout.rstrip("\n"), expected_out,
                          msg="unexpected output" + details)

    # --- -representative: retrieve cluster members (default outfmt '%m %t') ---
    def test_representative_default_outfmt(self) -> None:
        self._check(0,
                    "REPR_A Protein A representative\n"
                    "MEMBER_A2 Protein A member 2\n"
                    "MEMBER_A1 Protein A member 1",
                    "-db", self.cluster_db, "-representative", "REPR_A")

    # --- -representative with a custom outfmt ---
    def test_representative_custom_outfmt(self) -> None:
        self._check(0,
                    "REPR_A|100|Protein A representative\n"
                    "MEMBER_A2|201|Protein A member 2\n"
                    "MEMBER_A1|200|Protein A member 1",
                    "-db", self.cluster_db, "-representative", "REPR_A",
                    "-outfmt", "%m|%T|%t")

    # --- -representative with a comma-delimited outfmt: titles get quoted ---
    def test_representative_csv_outfmt(self) -> None:
        self._check(0,
                    'REPR_A,100,"Protein A representative"\n'
                    'MEMBER_A2,201,"Protein A member 2"\n'
                    'MEMBER_A1,200,"Protein A member 1"',
                    "-db", self.cluster_db, "-representative", "REPR_A",
                    "-outfmt", "%m,%T,%t")

    # --- -representative -get-common-ancestor (default outfmt '%T') ---
    def test_get_common_ancestor(self) -> None:
        self._check(0, "50",
                    "-db", self.cluster_db, "-representative", "REPR_A",
                    "-get-common-ancestor")

    # --- -accession: retrieve the cluster representative (default outfmt '%r %t') ---
    def test_accession_default_outfmt(self) -> None:
        self._check(0, "REPR_A Protein A member 1",
                    "-db", self.cluster_db, "-accession", "MEMBER_A1")

    # --- -taxid -exact-match: only the representative's own taxid matches ---
    def test_taxid_exact_match(self) -> None:
        self._check(0, "REPR_A 100",
                    "-db", self.cluster_db, "-taxid", "100", "-exact-match",
                    "-outfmt", "%r %T")

    # --- -taxid (default): expands to descendant taxids via taxonomy4blast ---
    def test_taxid_with_expansion(self) -> None:
        self._check(0, "REPR_A 100",
                    "-db", self.cluster_db, "-taxid", "90",
                    "-outfmt", "%r %T")

    # --- -taxid -exact-match with an ancestor (not exact) taxid: no match ---
    def test_taxid_exact_match_no_match(self) -> None:
        self._check(1, "",
                    "-db", self.cluster_db, "-taxid", "90", "-exact-match",
                    "-outfmt", "%r %T")

    # --- -taxid -exact-match for a cluster with no taxonomy descendants ---
    def test_taxid_exact_match_no_descendants(self) -> None:
        self._check(0, "REPR_B 300",
                    "-db", self.cluster_db, "-taxid", "300", "-exact-match",
                    "-outfmt", "%r %T")

    # --- error: unknown accession ---
    def test_unknown_accession(self) -> None:
        self._check(1, "",
                    "-db", self.cluster_db, "-accession",
                    "NOT_A_REAL_ACCESSION")

    # --- error: unknown representative ---
    def test_unknown_representative(self) -> None:
        self._check(1, "",
                    "-db", self.cluster_db, "-representative",
                    "NOT_A_REAL_REPRESENTATIVE")

    # --- error: '%m' is not applicable for -taxid ---
    def test_invalid_outfmt_for_taxid(self) -> None:
        self._check(1, "",
                    "-db", self.cluster_db, "-taxid", "100", "-exact-match",
                    "-outfmt", "%m")

    # --- error: only '%T' is applicable for -get-common-ancestor ---
    def test_invalid_outfmt_for_common_ancestor(self) -> None:
        self._check(1, "",
                    "-db", self.cluster_db, "-representative", "REPR_A",
                    "-get-common-ancestor", "-outfmt", "%r")

    # --- error: missing database file ---
    def test_missing_database(self) -> None:
        self._check(1, "",
                    "-db", os.path.join(self.workdir, "does_not_exist.sqlite3"),
                    "-representative", "REPR_A")


if __name__ == "__main__":
    unittest.main()
