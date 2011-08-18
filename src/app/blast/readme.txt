Igblastn is the command line igblast (for nucleotide sequence only).

1.  Required files 
1).       Blast database files for searching germline V, D, and J genes.  You can specify any germline databases you like.   The default databases are the following NCBI  human germline databases (i.e., the program will automatically look for these if not specified):
Organism_name_gl_V
Organism_name_gl_D
Organism_name_gl_J
For example, if you specify human as organism, then the database is human_gl_V.  All NCBI database files are supplied with igblastn program (see http://www.ncbi.nlm.nih.gov/igblast/ about database details).  You can make your own blast database using NCBI's makeblastdb such as:
makeblastdb -parse_seqids -dbtype nucl -in my_database


2).     Files for annotating domains (all files are supplied with igblastn).  These files are considered internal to igblastn program and we suggest you do not modify them. 
(a).  Blast database files for germline V gene domain annotation purpose.  
Domain annotation will be based on closest-matched germline V gene that has the pre-annotated domain information contained in domain annotation file (see below).  The file names MUST have this format:
Organism_name_gl_V.* (i.e., human_gl_V if your search is for human sequences).

 (b).  Ig domain annotation file for germline V genes (sequence position is 0-based).  
This file contains domain boundary information for germline genes and its name MUST have this format:
Organism_name_gl.n.dm.kabat (i.e., human_gl.n.dm.kabat if your search is for human sequences) or Organism_name_gl.n.dm.imgt.

2.  Optional files (also supplied with igblastn program if you search NCBI germline gene database):
File to indicate germline V and J gene coding frame start position (position is 0-based) and chain type for each sequence in your germline sequence database.  Note that the supplied file contains only information for NCBI germline database.   If you search your own database and if it contains different sequences or sequence identifiers, then you need to edit that file (or supply your own file) to reflect that or you won't get proper frame information or amino acid translation or chain type information (other results will still be shown).  The default file name is:
Organism_name_gl.aux (i.e., human_gl.aux if you don’t specify that file and your search is for human sequences).  
The entry format has tab-delimited fields for sequence id, chain type and coding frame start position.  See human_gl.aux or mouse_gl.aux for examples.  Enter -1 if the frame information is unknown.  You need to use -auxilary_data option to specify your file.
 
3.  Some examples.
1).  Searching germline gene database
a).  To query one or more human Ig sequences (contained in myseq) against NCBI human germline gene database with standard text alignment result format (you can type "./igblastn -help" to see details on all input parameters and the default setting, particularly those under IgBLAST options):
./igblastn -germline_db_V human_gl_V -germline_db_J human_gl_J -germline_db_D human_gl_D -organism human -domain_system kabat -query myseq -show_translation -outfmt 3

b).  To query one or more human Ig sequences (contained in myseq) against NCBI human germline gene database with tabular result:
./igblastn -germline_db_V human_gl_V -germline_db_J human_gl_J -germline_db_D human_gl_D -organism human -domain_system kabat -query myseq -show_translation -outfmt 7

c).  To query one or more human Ig sequences (contained in myseq) against custom database such as Andew Collins IGH repertoire database:
./igblastn -germline_db_V UNSWIgVRepertoire_fasta.txt -germline_db_J UNSWIgJRepertoire_fasta.txt -germline_db_D UNSWIgDRepertoire_fasta.txt -organism human -domain_system kabat -query myseq  -show_translation
Please be aware that you need to specify your own coding frame start and chain_type information (see details on optional files above) if your custom database has different sequence entries from NCBI database.  

2).  Searching other databases in addition to germline database.
Igblast allows you to search any other databases (such as NCBI IG V gene sequence database or NCBI nr database) as well as the germline database at the same time.  You’ll get top hits from germline sequences followed by hits from non-germline database.
You MUST use –db option to specify the non-germline database which may not contain any sequences identifiers that exist in germline databases.  Note this option is ONLY for non-germline database (germline databases MUST be used with -germline_db_V, -germline_db_D or -germline_db_J option).  

a).  To query one or more human Ig sequences (contained in myseq) against NCBI  IG V gene sequence database:
./igblastn -germline_db_V human_gl_V -germline_db_J human_gl_J -germline_db_D human_gl_D -query myseq -show_translation -db igSeqNt -num_alignments 10 -outfmt 3

b).  To query one or more human Ig sequences (contained in myseq) against NCBI  nr database:
./igblastn -germline_db_V human_gl_V -germline_db_J human_gl_J -germline_db_D human_gl_D -query myseq -show_translation -db nr -num_alignments 10 -remote -outfmt 3
Note the –remote option used with this search…-remote option directs igblast to send nr database searching to NCBI server which typically is much faster.
 

