#!/usr/bin/env python
"""
* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================

Script to grab "coding_sequence" in OGRDB germline set JSON file to
make a BLAST sequence database
Usage: ./makeogrdb.py germline_set_file_name blast_db_output_file_name
"""


from datetime import datetime
import os
import glob
import re
import sys
import json
from functools import cmp_to_key
import subprocess


if (len(sys.argv) <3): 
    print("Please supply the germline set JSON file name and the output file name for blast database\n")
    print("Usage: ./makeogrdb.py germline_set_file_name blast_db_output_file_name\n")

infile_name = str(sys.argv[1])
outfile_name = str(sys.argv[2])


infile = open(infile_name)
data = json.load(infile)

outfile = open(outfile_name, "w")

for i in data['GermlineSet']["allele_descriptions"]:
    seq = re.sub('\.+', '', i["coding_sequence"])
    outfile.write(">" + i["label"] + "\n")
    outfile.write(seq + "\n")
infile.close()  
outfile.close()


os.system("./makeblastdb  -in " + outfile_name + " -dbtype prot -parse_seqids " + " -title " + "\"" + data['GermlineSet']["germline_set_name"] + " " + data["GermlineSet"]["germline_set_ref"] + "\"")
  
exit(0)


