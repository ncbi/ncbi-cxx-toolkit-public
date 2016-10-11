#!/opt/python-all/bin/python 


import argparse, os, subprocess, sys, glob, re

# This script can be run as follows:
# python protein_match.py -u update -id current_entry_id -o outstub -bindir binary_dir -log log_file
# update is the update Seq-entry file
# outstub.tab is the resulting protein match table
# binary_dir is an optional parameter specifying the location of the binaries 
# executed by the script. If unspecified, binary_dir defaults to the current 
# working directory.
# log_file is also optional and defaults to stdout.

# Required binaries:
#   setup_match 
#   assm_assm_blastn
#   compare_annots
#   generate_match_table

# Description of binaries:
#
#  ### setup_match
#      Reads the update provided by the submitter. 
#      Reads the GenBank accession from the update.
#      Fetches the Genbank Seq-entry.
#      Constructs a local id from CDS feature locations. 
#      Strips any preexisting IDs on the nucleotide sequence and replaces them with the local id.
#      Returns the GenBank Seq-entry, the processed update Seq-entry and the nucleotide sequences 
#      for these entries.

#
# ### assm_assm_blastn
#     Generates an alignment between the update sequence and the genbank sequence
#
# ### compare_annots 
#     Matches the proteins.
#     Reads the update and genbank Seq-entrys and the alignment file.
#     Returns a sequence of Seq-annots
#
# ### generate_match_table
#     Generates a table from the output from compare_annots 
#

class BinaryNotFoundError(Exception):
    def __init__(self, binary):
        error_string = binary + " not found"
        super(BinaryNotFoundError, self).__init__(error_string)

class ProteinMatchClass :
    def __init__(self):
        self.initialize_arg_parser()
        self.current_dir = os.getcwd()
        self.binary_dir = self.current_dir # default value
        self.tempfile_list = [] # empty list used to keep track of temporary files
        self.log = []
        self.logfile = None
        self.genbank_id = None

    def check_binaries(self):
        self.binary_list = []
        self.binary_list.append(self.binary_dir + "/setup_match")
        self.binary_list.append(self.binary_dir + "/assm_assm_blastn")
        self.binary_list.append(self.binary_dir + "/compare_annots")
        self.binary_list.append(self.binary_dir + "/generate_match_table")
        for binary in self.binary_list:
            isfile = os.path.isfile(binary)
            if not isfile:
                raise BinaryNotFoundError(binary)

    def initialize_arg_parser(self):
        self.parser = argparse.ArgumentParser()
        self.parser.add_argument("-input", required=True, help="Path to update Seq-entry file")
        self.parser.add_argument("-outstub", required=True, help="Output file stub")
        self.parser.add_argument("-bindir", help="Binaries directory")
        self.parser.add_argument("-log", help="Log file")
        self.parser.add_argument("-keep-temps", dest='keep_temps', action='store_true', help="Keep temporary files")
        self.parser.set_defaults(keep_temps=False)

    def parse_args(self):
        self.args = self.parser.parse_args()
        if self.args.bindir:
            self.binary_dir = self.args.bindir
        if self.args.log:
            self.logfile = open(self.args.log, 'w')

    def log_tempfile(self, *tempfiles):
        for entry in tempfiles:
            if isinstance(entry, list):
                for tempfile in entry:
                    self.log_tempfile(tempfile)
            elif entry not in self.tempfile_list:
                self.tempfile_list.append(entry)

    def remove_tempfiles(self):
        for tempfile in self.tempfile_list:
            os.remove(tempfile)
 
    def setup_match(self, infile, outstub):

        """Preprocess the update Seq-entry so that the nucleotide sequence 
        and annotations on this sequence have consistent ids 
        Fetch the current GenBank entry to which this update will be applied.
        Fetch nucleotide sequences from the update entry and GenBank entries, 
        which are needed by assm_assm_blastn.
        """
        cmd = "{}/setup_match -i {} -o {}".format(self.binary_dir, infile, outstub)

        print(cmd)
        p = subprocess.Popen(cmd.split(), stdout=self.logfile)
        p.wait()

    def assm_assm_blastn(self, query_seq, target_seq, output_stub):

        """Generate an alignment between two nucleotide sequences.
        
        The nucleotides sequences are in ASN.1 Seq-entry format.
        Generates an alignment file and a manifest pointing towards this file.

        """

        aln_file = output_stub + ".merged.asn"
        cmd = "{}/assm_assm_blastn ".format(self.binary_dir)
        cmd += "-query {} ".format(query_seq)
        cmd += "-target {} ".format(target_seq)
        cmd += "-task megablast "
        cmd += "-word_size 16 -evalue 0.01 "
        cmd += "-gapopen 2 -gapextend 1 "
        cmd += "-best_hit_overhang 0.1 -best_hit_score_edge 0.1 "
        cmd += "-align-output {} ".format(aln_file)
        cmd += "-ofmt asn-binary "
        cmd += "-nogenbank "
        p = subprocess.Popen(cmd.split(), stdout=self.logfile)
        p.wait()
        self.aln_manifest = output_stub + ".aln.mft"
        with open(self.aln_manifest,'w') as file:
            file.write(aln_file)
            file.write("\n")
        self.log_tempfile(self.aln_manifest, aln_file)

    def compare_annots(self, first_entry, second_entry, output_stub):

        """Compare proteins for two Seq-entrys and return a list of Seq-annots"
        
        first_entry and second_entry are ASN1 files. 
        Each file contains a single Seq-entry encompassing a nuc-prot set.
        compare_annots also takes as an argument a manifest file pointing 
        toward a nucleotide-sequence alignment.
        
        """
        self.aln_manifest = output_stub + ".aln.mft"
        cmd = "{}/compare_annots ".format(self.binary_dir)
        cmd += "-q_scope_type annots -q_scope_args {} ".format(first_entry)
        cmd += "-s_scope_type annots -s_scope_args {} ".format(second_entry)
        cmd += "-nogenbank "
        cmd += "-alns {} ".format(self.aln_manifest) 
        cmd += "-o_asn {}.compare.asn ".format(output_stub)
        p = subprocess.Popen(cmd.split(), stdout=self.logfile)
        p.wait()
        self.log_tempfile(output_stub + ".compare.asn")

    def make_table(self, output_stub):
        """Construct a protein match table from the output generated by compare_annots.

           Executes: generate_match_table -i output_stub.compare.asn -o output_stub.tab
           output_stub.compare.asn --- contains comparison Seq-annots generated by compare_annots.
           output_stub.tab --- protein match table.
        
        """
        annot_file = output_stub + ".compare.asn"
        align_file = output_stub + ".merged.asn"
        outfile = output_stub + ".tab"
        cmd = "{}/generate_match_table".format(self.binary_dir)
        cmd += " -i " + annot_file
        cmd += " -aln-input " + align_file
        cmd += " -o " + outfile
        p = subprocess.Popen(cmd.split(), stdout=self.logfile)
        p.wait()

    def match(self): 
        # Process the update file. 
        # Replace Seq-ids on nucleotide sequence and annotations by local ids and save the result 
        # to processed_update. 
        # Save the processed nucleotide sub-entry to processed_update_genome.
        #local_entry = self.args.outstub  + ".local.asn"
        #local_nucseq = self.args.outstub + ".local_nuc.asn" 
        #genbank_entry = self.args.outstub + ".genbank.asn"
        #genbank_nucseq = self.args.outstub + ".genbank_nuc.asn" 

        self.setup_match(self.args.input, self.args.outstub)

        all_entries = glob.glob(self.args.outstub + '*.asn')
        pattern = re.compile(self.args.outstub + ".local[0-9]*.asn")
        local_entries = [s for s in all_entries if pattern.match(s)]

        pattern = re.compile(self.args.outstub + ".local_nuc[0-9]*.asn")
        local_nucseqs = [s for s in all_entries if pattern.match(s)]

        pattern = re.compile(self.args.outstub + ".genbank[0-9]*.asn")
        genbank_entries = [s for s in all_entries if pattern.match(s)]

        pattern = re.compile(self.args.outstub + ".genbank_nuc[0-9]*.asn")
        genbank_nucseqs = [s for s in all_entries if pattern.match(s)]

        self.log_tempfile(local_entries)
        self.log_tempfile(local_nucseqs)
        self.log_tempfile(genbank_entries)
        self.log_tempfile(genbank_nucseqs)

        table_filenames = []
        count = 0
        # Should report an error if these lists don't all have the same length
        for local_nucseq, genbank_nucseq, local_entry, genbank_entry in zip(local_nucseqs, genbank_nucseqs, local_entries, genbank_entries):
            # Generate an alignment between the nucleotide sequences in the update and current genbank entry.
            self.assm_assm_blastn(local_nucseq, genbank_nucseq, self.args.outstub)
            # Run compare_annots
            self.compare_annots(local_entry, genbank_entry, self.args.outstub)
            # Convert the Seq-annot file generate by compare_annots to a match table.
            self.make_table(self.args.outstub)
            # Relabel the table
            temporary_tablefile = self.args.outstub + ".tab" + str(count)
            os.rename(self.args.outstub + ".tab", temporary_tablefile)
            comparefile = self.args.outstub + ".compare.asn"
            self.log_tempfile(temporary_tablefile)
            table_filenames.append(temporary_tablefile)
            count += 1

        merged_table_filename = self.args.outstub + ".tab"
        with open(merged_table_filename, 'w') as outfile:
            fname = table_filenames.pop(0)
            with open(fname) as infile:
                outfile.write(infile.read())
            for fname in table_filenames:
                with open(fname) as infile:
                    infile.readline() # skip to the next line
                    outfile.write(infile.read())


    def cleanup(self):
        """Delete temporary files and close logfile."""
        if not self.args.keep_temps:
            self.remove_tempfiles()
        if self.logfile != None:
            self.logfile.close()


if __name__ == "__main__":
    prot_match = ProteinMatchClass()
    prot_match.parse_args()
    prot_match.check_binaries()
    prot_match.match()
    prot_match.cleanup()

