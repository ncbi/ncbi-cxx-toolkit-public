#!/opt/python-all/bin/python 


import argparse, os, subprocess, sys

# This script can be run as follows:
# python protein_match.py -u update -id current_entry_id -o outstub -bindir binary_dir -log log_file
# update is the update Seq-entry file
# current_entry_id is the id for the old GenBank entry
# outstub.tab is the resulting protein match table
# binary_dir is an optional parameter specifying the location of the binaries 
# executed by the script. If unspecified, binary_dir defaults to the current 
# working directory.
# log_file is also optional and defaults to stdout.

# Required binaries:
#   preprocess_entry 
#   getfasta
#   gp_fetch_sequences
#   assm_assm_blastn
#   compare_annots
#   generate_match_table

# Description of binaries:
#
#  ### preprocess_entry 
#      Reads the update provided by the submitter. 
#      Constructs a local id from CDS feature locations. 
#      Strips any preexisting IDs on the nucleotide sequence and replace with the local id.
#      Returns the processed seq-entry and the processed nucleotide seq-entry.
#
# ### getfasta 
#     Given genbank id, fetch sequence in FASTA format
#
# ### gp_fetch_sequences
#     Given genbank id, fetch ASN.1 Seq-entry
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

    def check_binaries(self):
        self.binary_list = []
        self.binary_list.append(self.binary_dir + "/preprocess_entry")
        self.binary_list.append(self.binary_dir + "/getfasta")
        self.binary_list.append(self.binary_dir + "/gp_fetch_sequences")
        self.binary_list.append(self.binary_dir + "/assm_assm_blastn")
        self.binary_list.append(self.binary_dir + "/compare_annots")
        self.binary_list.append(self.binary_dir + "/generate_match_table")
        for binary in self.binary_list:
            isfile = os.path.isfile(binary)
            if not isfile:
                raise BinaryNotFoundError(binary)

    def initialize_arg_parser(self):
        self.parser = argparse.ArgumentParser()
        self.parser.add_argument("-id", required=True, help="GenBank ID") 
        self.parser.add_argument("-update", required=True, help="Path to update Seq-entry file")
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
        for tempfile in tempfiles:
            if tempfile not in self.tempfile_list:
                self.tempfile_list.append(tempfile)

    def remove_tempfiles(self):
        for tempfile in self.tempfile_list:
            os.remove(tempfile)
 
    def preprocess_seqentry(self, infile, outfile, gen_outfile):

        """Preprocess a Seq-entry containing a nuc-prot set.

        Replace Seq-ids on nucleotide sequence and CDS feature locations by local ids. 
        This is done to ensure the sequence and annotations have consistent ids, 
        so that compare_annots can match annotations and alignments.
        See the discussion on MSS-566 on 1/28/2016 for further details.
        Returns the nucleotide-sequence sub-entry needed to generate an alignment.
        
        """
        cmd = "{}/preprocess_entry -i {} -o {} -og {} ".format(self.binary_dir, infile, outfile, gen_outfile)
        p = subprocess.Popen(cmd.split(), stdout=self.logfile)
        p.wait()
        self.log_tempfile(outfile)
        self.log_tempfile(gen_outfile)

    def make_id_manifest(self, genbank_id):
        """Write the genbank id to a manifest file."""
        id_manifest = genbank_id + ".id.mft"
        with open(id_manifest, 'w') as file:
            file.write(genbank_id)
            file.write("\n")
        self.log_tempfile(id_manifest)
        return id_manifest

    def fetch_entry(self, id_manifest, outfile):
        """ Calls gp_fetch_sequences on the id(s) contained in the id_manifest file.

        Fetches the ASN.1 Seq-entry for each id.

        """
        cmd = "{}/gp_fetch_sequences -i {} -o {} ".format(self.binary_dir, id_manifest, outfile)
        p = subprocess.Popen(cmd.split(), stdout=self.logfile)
        p.wait()

    def fetch_fasta(self, id_manifest, outfile):
        """ Calls getfasta on the id(s) contained in the id_manifest file.

        Fetches the nucleotide sequence in FASTA format.
        
        """
        cmd = "{}/getfasta -i {} -o {} ".format(self.binary_dir, id_manifest, outfile)
        p = subprocess.Popen(cmd.split(), stdout=self.logfile)
        p.wait()
        self.log_tempfile(outfile)

    def assm_assm_blastn(self, query_seq, target_seq, output_stub):

        """Generate an alignment between two nucleotide sequences.
        
        The nucleotides sequences are in ASN.1 Seq-entry or FASTA format.
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
        infile = output_stub + ".compare.asn"
        outfile = output_stub + ".tab"
        cmd = "{}/generate_match_table".format(self.binary_dir)
        cmd += " -i " + infile
        cmd += " -o " + outfile
        p = subprocess.Popen(cmd.split(), stdout=self.logfile)
        p.wait()

    def match(self): 
        # Process the update file. 
        # Replace Seq-ids on nucleotide sequence and annotations by local ids and save the result 
        # to processed_update. 
        # Save the processed nucleotide sub-entry to processed_update_genome.
        processed_update = self.args.update + ".proc"
        processed_update_genome = processed_update + ".gen" 
        self.preprocess_seqentry(self.args.update, processed_update, processed_update_genome)

        # Fetch the current GenBank entry in ASN.1 and the corresponding sequence FASTA file.
        genbank_entry = self.args.id + ".asn"
        id_manifest = self.make_id_manifest(self.args.id)
        self.fetch_entry(id_manifest, genbank_entry) 
        self.log_tempfile(genbank_entry) 
        # Fetch the genbank sequence in FASTA format. 
        # Could equally well pass a Seq-entry, but preprocessing would be required.
        fasta_file = self.args.id + ".fa" 
        self.fetch_fasta(id_manifest, fasta_file)

        # Align the nucleotide sequences of the update and current entry.
        self.assm_assm_blastn(processed_update_genome, fasta_file, self.args.outstub)
        # Run compare_annots.
        self.compare_annots(processed_update, genbank_entry, self.args.outstub)
        # Convert the Seq-annot file generated by compare_annots to a match table.
        self.make_table(self.args.outstub)

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

