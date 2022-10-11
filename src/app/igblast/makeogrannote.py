#!/usr/bin/env python
"""

Script to generate V gene FWR/CDR annotation file given one or more OGRDB JSON file
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


def main():
    
    parser = argparse.ArgumentParser(description = "Script to build V gene FWR/CDR annotation file from OGRDB germline sets JSON files")
    parser.add_argument('--germline_file', required=True,
                        help="JSON file name(s). Need to put multiple files inside quotation marks, i.e., \"file1 file2 file3\"")
    parser.add_argument('--output_file', required=True,
                        help="output annotation file name")
    args = parser.parse_args()
    
    annotation_type = ["imgt"]
    input_file_names = re.split("\s+", args.germline_file)
    outfile_name = args.output_file
    outfile_handle = open(outfile_name, "w")
    
    for each_in_file in input_file_names:
        infile = open(each_in_file)
        print(f"file processed: {each_in_file}");
        data = json.load(infile)
        allele_descriptions = None
        germline_set = None
        if isinstance(data["GermlineSet"], list):
            germline_set = data["GermlineSet"][0]
        else:
            germline_set = data["GermlineSet"]

        for each_allele in germline_set["allele_descriptions"]:
            if (each_allele["sequence_type"] == "V"):
                seq = each_allele["coding_sequence"]
                outfile_handle.write(each_allele["label"] + "\t")
                for j in each_allele["v_gene_delineations"]:
                    if (j["delineation_scheme"].lower() == annotation_type[0]):
                        outfile_handle.write(str(j["fwr1_start"]) + "\t")
                
                        fwr1_end = j["fwr1_end"] - GetDotCounts(seq, j["fwr1_end"])
                        outfile_handle.write(str(fwr1_end) + "\t")
                        
                        cdr1_start = j["cdr1_start"] - GetDotCounts(seq, j["cdr1_start"])
                        outfile_handle.write(str(cdr1_start) + "\t")
                        
                        cdr1_end = j["cdr1_end"] - GetDotCounts(seq, j["cdr1_end"])
                        outfile_handle.write(str(cdr1_end) + "\t")
                        
                        fwr2_start = j["fwr2_start"] - GetDotCounts(seq, j["fwr2_start"])
                        outfile_handle.write(str(fwr2_start) + "\t")
                        
                        fwr2_end = j["fwr2_end"] - GetDotCounts(seq, j["fwr2_end"])
                        outfile_handle.write(str(fwr2_end) + "\t")
                        
                        cdr2_start = j["cdr2_start"] - GetDotCounts(seq, j["cdr2_start"])
                        outfile_handle.write(str(cdr2_start) + "\t")
                        
                        cdr2_end = j["cdr2_end"] - GetDotCounts(seq, j["cdr2_end"])
                        outfile_handle.write(str(cdr2_end) + "\t")
                        
                        fwr3_start = j["fwr3_start"] - GetDotCounts(seq, j["fwr3_start"])
                        outfile_handle.write(str(fwr3_start) + "\t")
                        
                        fwr3_end = j["fwr3_end"] - GetDotCounts(seq, min(len(seq), j["fwr3_end"]))
                        outfile_handle.write(str(fwr3_end) + "\t")

                        chain_type = each_allele["sequence_type"] + each_allele["locus"][len(each_allele["locus"]) - 1]
                        outfile_handle.write(chain_type + "\t")
                        outfile_handle.write("0"+ "\n")

            
        infile.close()
                
    outfile_handle.close()    
    exit(0)

#functions

def GetDotCounts(seq, end_pos):
    count = 0
    for i in range(0, end_pos):
        if (seq[i] == '.'):
            count += 1
    return count

if __name__ == "__main__":
    main()


