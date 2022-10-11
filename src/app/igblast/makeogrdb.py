#!/usr/bin/env python
"""
Script to grab "coding_sequence" in OGRDB germline set JSON file and generate
 BLAST databases for V, D, J sequences, respectively.
Requires MCBI makeblastdb in the running directory 
"""


from datetime import datetime
import os
import glob
import re
import sys
import json
import subprocess
import argparse
from pathlib import Path


parser = argparse.ArgumentParser(description = "Script to build blast dbs from OGRDB germline sets JSON files")
parser.add_argument('--germline_file', required=True,
                    help="JSON file name(s). Need to put multiple files inside quotation marks, i.e., \"file1 file2 file3\"")
parser.add_argument('--output_file', required=True,
                    help="output file name for blast database")
args = parser.parse_args()


input_file_names = re.split("\s+", args.germline_file)
outfile_name_prefix = args.output_file

sequence_type = ["V", "D", "J"]

outfile_names = {}
outfile_handles = {}
db_titles = {}
for i in sequence_type:
    outfile_names[i] = f"{outfile_name_prefix}.{i}"
    outfile_handles[i] = open(outfile_names[i], "w")
    db_titles[i] = {}
    
for each_in_file in input_file_names:
    infile = open(each_in_file)
    data = json.load(infile)
    germline_set = None
    if isinstance(data["GermlineSet"], list):
        germline_set = data["GermlineSet"][0]
    else:
        germline_set = data["GermlineSet"]
        
    title = germline_set["germline_set_name"] + " " + germline_set["germline_set_ref"] 
    for i in germline_set["allele_descriptions"]:
        seq = re.sub('\.+', '', i["coding_sequence"])
        outfile_handles[i["sequence_type"]].write(">" + i["label"] + "\n")
        outfile_handles[i["sequence_type"]].write(seq + "\n")
        if not title in db_titles[i["sequence_type"]].keys():
            db_titles[i["sequence_type"]][title] = 1
    infile.close()

for key, value in outfile_handles.items():
    outfile_handles[key].close()

for i in sequence_type:
    final_title = ""
    for key, value in db_titles[i].items():
        final_title = final_title + " " + key
    if (os.path.getsize(outfile_names[i]) > 0):
        os.system(f"./makeblastdb  -in {outfile_names[i]} -dbtype nucl -parse_seqids -title \"{final_title}\"")
    else:
        Path(outfile_names[i]).unlink(missing_ok=True)
    
exit(0)


