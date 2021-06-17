==============================================
omssacl: ms/ms search algorithm
==============================================

Homepage: http://www.ncbi.nlm.nih.gov/Structure/OMSSA/

For more information on command line parameters, see http://www.ncbi.nlm.nih.gov/Structure/OMSSA/run.htm

For information on setting up sequence libraries, see http://www.ncbi.nlm.nih.gov/Structure/OMSSA/blastdb.htm

For a description of the algorithm, see http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=pubmed&dopt=Abstract&list_uids=15473683
preprint at: http://arxiv.org/abs/q-bio.QM/0406002



==============================================
dta_merge_OMSSA.pl: concatenating DTA files
==============================================

This program concatenates DTA files using pseudo XML.  This allows you to track dta peak lists by filename and other properties.

To run the concatenation script use:

perl dta_merge_OMSSA.pl -i <input path> -o <output path> -s <batch size> -n <output file name root>

If the program complains that there are too many dta files, try increasing the batch size or breaking your dta file sets into smaller groups.



==============================================
OMSSA.pm: Perl xml parser for OMSSA results
==============================================


To use the sample xml parser for OMSSA results, you must install Perl and the XML-SAX module.

To run the parser, use a command line of the form:
perl readOMSSA.pl test.xml

You may get a warning about ParserDetails.ini being missing.  Normally, you can disregard this message.

Note that this is only a sample parser -- it is only intended as an example of how to parse OMSSA results using perl and XML.

The output will look like:

[...]
Start of Hitset (hits to a particular spectrum)
Hitset number: 25
 Start of a hit
 Hit E-value: 2.79663443565564
 Hit charge: 1
  Start of peptides matching a hit
  Peptide start: 1108
  Peptide stop: 1115
  Peptide found in protein id: 6322648
  Peptide found in protein with defline: involved in mannose metabolism; Mnn4p
  End of peptides
  Peptide sequence: QEEGEKMK
 End of a hit
[...]
