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
"""Creates small, deterministic SQLite databases used to test clusterdbcmd,
mirroring the schemas of the production nr_cluster_seq.sqlite3 and
taxonomy4blast.sqlite3 databases.

Usage: clusterdbcmd_test_data.py <cluster_db_path> <taxonomy_db_path>
"""
import os
import sqlite3
import sys


def create_cluster_db(path: str) -> None:
    if os.path.exists(path):
        os.remove(path)
    conn = sqlite3.connect(path)
    cur = conn.cursor()
    cur.executescript("""
        CREATE TABLE Representative (
            id                      INTEGER PRIMARY KEY,
            accession               TEXT NOT NULL UNIQUE
        );
        CREATE TABLE Title (
            id                      INTEGER PRIMARY KEY,
            title                   TEXT NOT NULL
        );
        CREATE TABLE ClusterInfo (
            representative_id       INTEGER REFERENCES Representative(id),
            member_accession        TEXT NOT NULL,
            member_taxid            INTEGER CHECK(member_taxid >= 0),
            title_id                INTEGER REFERENCES Title(id),
            PRIMARY KEY(representative_id, member_accession)
        );
        CREATE TABLE ClusterCommonAncestor (
            representative_id       INTEGER PRIMARY KEY REFERENCES Representative(id),
            taxid                   INTEGER CHECK(taxid >= 0)
        );
        CREATE VIEW ClusterInfoView AS
        SELECT
            R.accession as representative,
            C.member_accession,
            C.member_taxid,
            T.title as member_title
        FROM ClusterInfo C JOIN Representative R ON C.representative_id = R.id
        LEFT JOIN Title T ON C.title_id = T.id;
        CREATE VIEW ClusterCommonAncestorView AS
        SELECT
            R.accession as representative,
            taxid
        FROM ClusterCommonAncestor C JOIN Representative R ON C.representative_id = R.id;
        CREATE INDEX ClusterInfoIdx_MembTaxid ON ClusterInfo(member_taxid);
        CREATE INDEX ClusterInfoIdx_MembAcc ON ClusterInfo(member_accession);
    """)

    # Cluster 'REPR_A' has 3 members (itself and two others), taxid 100 is
    # the representative's own taxid, which is a descendant of taxid 90 in
    # the taxonomy test database created below.
    # Cluster 'REPR_B' only contains itself, with taxid 300 (no ancestors in
    # the taxonomy test database).
    cur.executemany(
        "INSERT INTO Representative (id, accession) VALUES (?,?)",
        [(1, "REPR_A"), (2, "REPR_B")])
    cur.executemany(
        "INSERT INTO Title (id, title) VALUES (?,?)",
        [(1, "Protein A representative"),
         (2, "Protein A member 1"),
         (3, "Protein A member 2"),
         (4, "Protein B representative")])
    cur.executemany(
        "INSERT INTO ClusterInfo "
        "(representative_id, member_accession, member_taxid, title_id) "
        "VALUES (?,?,?,?)",
        [(1, "REPR_A", 100, 1),
         (1, "MEMBER_A1", 200, 2),
         (1, "MEMBER_A2", 201, 3),
         (2, "REPR_B", 300, 4)])
    cur.executemany(
        "INSERT INTO ClusterCommonAncestor (representative_id, taxid) "
        "VALUES (?,?)",
        [(1, 50), (2, 300)])
    conn.commit()
    conn.close()


def create_taxonomy_db(path: str) -> None:
    if os.path.exists(path):
        os.remove(path)
    conn = sqlite3.connect(path)
    cur = conn.cursor()
    cur.executescript("""
        CREATE TABLE TaxidInfo (
            taxid           INTEGER PRIMARY KEY,
            parent          INTEGER
        );
        CREATE INDEX TaxidInfoCompositeIdx_parent ON TaxidInfo(parent,taxid);
    """)
    # taxid 100 (REPR_A's own taxid) is a descendant of taxid 90.
    # taxid 300 (REPR_B's own taxid) has no descendants.
    cur.executemany(
        "INSERT INTO TaxidInfo (taxid, parent) VALUES (?,?)",
        [(90, None), (100, 90), (300, None)])
    conn.commit()
    conn.close()


def main() -> int:
    if len(sys.argv) != 3:
        sys.stderr.write(
            "Usage: %s <cluster_db_path> <taxonomy_db_path>\n" % sys.argv[0])
        return 1
    create_cluster_db(sys.argv[1])
    create_taxonomy_db(sys.argv[2])
    return 0


if __name__ == "__main__":
    sys.exit(main())
