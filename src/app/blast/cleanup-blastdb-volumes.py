#!/usr/bin/env python3
"""
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
# Author:  Christiam Camacho
# 
# File Description:
#   Script to remove needless BLAST database files.
# 
# ===========================================================================
"""
import argparse, os, configparser
import unittest, tempfile
from pathlib import Path
from glob import glob

VERSION = '1.0'
DESC = r""" Remove needless BLAST database volumes.  """


class Tester(unittest.TestCase):
    """ Testing class for this script. """

    def test_blastdb_config_invalid(self):
        rv = get_blastdb_from_ncbi_config("/dev/null")
        self.assertIsNone(rv)

    def test_blastdb_config(self):
        config = configparser.ConfigParser()
        expected = "/blast/db/blast"
        config['BLAST'] = {'BLASTDB': expected}
        tf = tempfile.NamedTemporaryFile(mode="wt")
        config.write(tf)
        tf.flush()
        rv = get_blastdb_from_ncbi_config(tf.name)
        self.assertEqual(expected, rv)

    def test_blastdb_finder(self):
        tal = tempfile.NamedTemporaryFile(suffix=".pin")
        dbname = find_blastdb(tal.name[:-4], True)
        self.assertEqual(dbname, tal.name[:-4])


def find_blastdb(name: str, is_prot: bool) -> str:
    """ Returns full path to BLAST database or None. """
    alias_file = "{}.{}al".format(name, "p" if is_prot else "n")
    index_file = "{}.{}in".format(name, "p" if is_prot else "n")
    if os.path.exists(alias_file) or os.path.exists(index_file):
        return name

    if "BLASTDB" in os.environ:
        alf = os.path.join(os.environ["BLASTDB"], alias_file)
        idxf = os.path.join(os.environ["BLASTDB"], index_file)
        if os.path.exists(alf) or os.path.exists(idxf):
            return os.path.join(os.environ["BLASTDB"], name)

    paths = [ os.getcwd(), str(Path.home()) ]
    if "NCBI" in os.environ:
        paths.append(os.path.join(os.environ["NCBI"]))

    for path in paths:
        for fname in [ ".ncbirc", "ncbi.ini" ]:
            ncbirc = os.path.join(path, fname)
            if os.path.exists(ncbirc):
                blastdb = get_blastdb_from_ncbi_config(ncbirc)
                if blastdb is not None:
                    alf = os.path.join(blastdb, alias_file)
                    idxf = os.path.join(blastdb, index_file)
                    if os.path.exists(alf) or os.path.exists(idxf):
                        return os.path.join(blastdb, name)


def get_blastdb_from_ncbi_config(config_file: str) -> str:
    """ Return the BLASTDB setting from the NCBI configuration file or None. """
    config = configparser.ConfigParser()
    config.read(config_file)
    if 'BLAST' in config and 'BLASTDB' in config['BLAST']:
        return config['BLAST']['BLASTDB']


def main():
    """ Entry point into this program. """
    parser = create_arg_parser()
    args = parser.parse_args()

    ext = args.dbtype[0]
    db = find_blastdb(args.db, ext == 'p')
    if db == None:
        print("Cannot find {} {} BLAST database".
              format("protein" if ext == 'p' else "nucleotide", args.db),
              file=sys.stderr)
        return 1

    alias_file = "{}.{}al".format(db, ext)
    if not os.path.exists(alias_file):
        return 1

    with open(alias_file, "rt") as al:
        for line in al:
            if not line.startswith("DBLIST"):
                continue
            vols = list(map(lambda x: x.replace('"', ''), line.split()[1:]))
            for existing_vols in sorted(glob("{}.*.{}in".format(db, ext))):
                vol_name = os.path.basename(existing_vols)[:-4]
                if vol_name in vols:
                    continue
                if args.dry_run:
                    print("Will remove extra volume {}".format(existing_vols[:-4]))
                to_rm = glob("{}??".format(existing_vols[:-2]))
                to_rm += glob("{}.tar.gz.md5".format(existing_vols[:-4]))
                for f in to_rm:
                    if not args.dry_run:
                        os.unlink(f)
                        print("Removed {}".format(f))
                    elif args.verbose > 0:
                        print("Will remove {}".format(f))

    return 0


def create_arg_parser():
    """ Create the command line options parser object for this script. """
    parser = argparse.ArgumentParser(description=DESC)
    parser.add_argument("-db", required=True, help="BLAST database name")
    parser.add_argument("-dbtype", help="Molecule type", required=True,
                        choices=["prot", "nucl"])
    parser.add_argument("-dry-run", action='store_true',
                        help="Do not delete any files, just list them")
    parser.add_argument('-version', action='version',
                        version='%(prog)s ' + VERSION)
    parser.add_argument("-verbose", action="count", default=0,
                        help="Increase output verbosity")
    return parser


if __name__ == "__main__":
    import sys
    sys.exit(main())

