This file summarizes the usage of the block-multiple alignment refinement
program (called 'bma_refiner' below) described in the paper "Refining multiple
sequence alignments with conserved core regions", Saikat Chakrabarti,
Christopher J. Lanczycki, Anna R. Panchenko, Teresa M. Przytycka, Paul
A. Thiessen and Stephen H. Bryant (2006) Nucl. Acids Res., 34, 2598-2606.  This
software is in the public domain, as per the notice at the end of this file.  

Currently the program supports files the in format of Cn3D and the Conserved
Domain Database (CD format).  A preliminary version of an MFASTA -> CD converter
('fa2cd') is also provided.

The Linux binary names end in '_LX' and PC binary names end in '.exe'.  

A Mac binary is available for the Intel architecture (name ends in '_MAC').  

Contact Chris Lanczycki (lanczyck@ncbi.nlm.nih.gov) or Saikat Chakrabarti
(chakraba@ncbi.nlm.nih.gov) for comments or reports of problems with this
program. 

Last modified:  $Date$

**  09/08/2009:
    Make -selection_order mandatory and emphasize the importance of using >1
    trial with a random selection order.
**  03/21/2007:
    Added a blurb about using cddalignview to dump refined CDs as FASTA.
**  12/12/2006:  
    v1.0.0 -> v1.1.0  Added -selection_order and -lno options in 'bma_refiner'
    to allow speedup of runs, particularly for larger models. 
    Changes to 'fa2cd' command-line options.
    
**


============================================================================
Contents:
1)  Overview
2)  Input & Output formats
3)  Command options
4)  fa2cd:  A FASTA -> CD file conversion utility
5)  Examples
============================================================================

=============================
1)  Overview:

The 'bma_refiner' program aims to refine an existing multiple alignment by using
the predetermined 'block model' of that domain family as a biologically relevant
constraint on the search space. A block model represents conserved
sequence/structure regions which are highly unlikely to contain gaps and are
common to all family members. The algorithm employed (sketched below) does well
in finding and fixing misalignments, even in families that contain
evolutionarily distant members, while not disrupting known regions of structural
and sequence conservation.   

Nevertheless, we do expect to start from a passably good initial multiple
alignment.  One that is contaminated by sequences unrelated to the domain family
being described, consists of multiple families with sufficiently distinctive
features or has other serious errors may be improved to some degree.  But the
initial flaws will confound the algorithm to various extents as it assumes all
sequences in the alignment do in fact belong to one family, so some global
misalignments may remain in such a case.   

The refinement protocol works by random selection and realignment of a sequence
with the family block model until no improvement in the alignment score has been
observed, or until the iteration cycle has expired. In the first phase of a
cycle of realignment, a dynamic programming algorithm is used on individual
sequences to realign the individual blocks.  (A block is defined as a region of
contiguous aligned gapless columns.)  This essentially shifts each block
attempting to correct misalignments between a given sequence and the rest of the
profile and at the same time preserves the family's overall block model (i.e.,
the conserved core regions).  The latter constraint prohibits the insertion of
gap characters in the middle of conserved blocks.  

In the second phase of a refinement cycle, the blocks in the alignment are
allowed to be extended or shortened in size depending on the overall score
improvement. (This phase does not use a dynamic programming approach.)   
 
The algorithm has been benchmarked against many Conserved Domain Database (CDD)
alignments and shows an overall score improvement, reliability in retrieval of
functional important sites and enhanced sensitivity compared to the original CDD
alignment. The method is also reasonably fast and can realign several hundreds
of highly diverse sequences within minutes.  The refinement method also provides
a means to detect the outlier sequences within an alignment and may thereby
point a way towards new subfamily identification schemes. 

However, we should note that a change in the details of PSSM calculation since
our original study seems to cause a small but noticeable drop in average
performance.  This was detected when refining automatically generated MSAs
containing sequences that appear in the various Balibase benchmark MSAs.  The
PSSM calculation routines we employ are linked to the PSI-BLAST codebase in the
NCBI C++ Toolkit, and underwent a series of changes to how psuedo-counts,
scaling, and composition bias adjustments were dealt with (based on examining
code-revision logs).  Other internal projects related to CDD curation had noted
such changes in PSSM construction, as well.  While we do not have direct evidence to
explain why bma_refiner performs less well than in 2006 on the BALiBASE test set, it
does seem the most likely hypothesis due to the central role the PSSM plays in
our scoring scheme.  

Fortunately, as used in the context of CDD curation, bma_refiner continues to
perform well in repairing small misalignments that would be very time consuming
to manually discover and fix despite the drop in average performance on the
BALiBASE test set.  



=============================
2)  Input & Output formats:

The native format for alignments is the same as that used by CDD
(https://www.ncbi.nlm.nih.gov/Structure/cdd/cdd.shtml) and the 3D structure
viewer/alignment editor Cn3D
(https://www.ncbi.nlm.nih.gov/Structure/CN3D/cn3d.shtml).  Namely, the
bma_refiner takes so-called CD files with a default file extension
'.cn3'. [Formerly called 'acd' files, only the default file extension has been
changed and the data in the files remains unchanged.  The bma_refiner program
does not require any particular file extension for the input alignment file.].   

Such CD files can be saved from Cn3D directly or acquired via the CDD service.
These files contain the alignment formatted according the NCBI data
specification in ASN.1.  See the NCBI & CDD web sites for details.    

To support user-created alignments a utility, 'fa2cd' [formerly named
'fasta2cd'], to convert FASTA-formatted multiple alignments to a CD file is
available.  See section (4) below for more details, or the web site
https://www.ncbi.nlm.nih.gov/Structure/cdtree/fa2cd.shtml.   

The .cn3 file created by the bma_refiner program can be loaded into Cn3D and
saved as FASTA, if desired.  Also, a command-line utility program called 'cddalignview'
exists in the NCBI C++ Toolkit (under src/objtools/cddalignview, relative to the
toolkit's root directory) which will read a .cn3 file and generate FASTA output.

NOTE:  A 'data' subdirectory contains some basic information used in
constructing the family profiles.  This directory should be placed in the same
directory in which you install the binary. 

=============================
3)  Command options:

A full list of command options is available by using the flag -h (for a brief
summary) or -help (list with descriptions):  e.g,  ./bma_refiner -help 


There are two mandatory arguments:

-i               (the initial alignment in CD file format).  
-selection_order (how to choose the order sequences are refined; see details below) 


Other simple options include:

-o               (specifies the base file name for refined alignments)
-n               (number of separate trials when random selection_order is used where
                  each trial uses a different order of rows; use the -nout option
                  to chose control how many refined alignments are output)
                
-lno             (recompute PSSM after this many sequences refined; default = 1)
-nc              (number of cycles per trial; 3-5 is a typical value)

-be_noShrink     (do not allow blocks to shrink in size)
-be_fix          (do not modify block boundaries; i.e., skip the second phase of refinement)
-no_LOO          (only modify block boundaries; i.e., skip the first phase of refinement)
-ex              (extension of input alignment fragment; see below)

-q               (minimal information printed during refinement)
-nout            (of the 'n' trials, save the best 'nout' of them; default = 1)
-details         (redirect program log to the specified file)



-selection_order [you may use the alias '-so' for this argument]:  
This parameter is mandatory whenever the LOO phase is enabled because it is
important that the user recognize there is an implicit performance trade-off
between using a deterministic selection order (value = 1 or 2) and a set of
random selection orders (value = 0).  A determininstic selection order will run
faster than using -selection_order 0 when the number of trials (-n) is > 1.
That speed comes at the expense of performance, as we've shown [2009,
unpublished] that -so 0 with -n > 1 tends to result in a better refinement due
to the ablility to explore more of alignment space.  The degree to which this
parameter is important depends, of course, on the nature of the MSA being
refined.  But in general, we recommend using -so 0 (random selection order) with
-n set to 3 or higher when possible.  By using the -nout parameter, you can
output each of the N refined alignments to compare, if desired.  

-lno > 1:  
the majority of time in bma_refiner is spent in re-computing the PSSM after each
individual sequence is realigned.  To make the program finish more quickly,
provide the option "-lno X", where X is an integer.  This requests an
approximate refinement that recomputes the PSSM after every X-th sequence.
However, if X is too large the benefits of refinement are rapidly lost.
Therefore, in practice make X < 20% of the total number of rows in your
alignment. 

-ex:  
this option directs the refiner to expand its search outside of the original
start/stop range, considering up to the specified number of residues beyond the
first and last aligned residues.  This assumes that the sequences to be refined
are not aligned over their full length; when this assumption holds, -ex 20 has
typically worked well, particularly in conjunction with -p 1.0.

-p:
We have found that to obtain a good refinement, loop lengths between
blocks cannot be allowed to grow without bound.  To that end, using the
parameters -p and -ex together provides a good compromise between rendering enough
of 'alignment space' accessible our algorithm while avoiding unrealistically
long loops being generated.  E.g., -p 1.25 constrains the unaligned regions
between two blocks during refinement to be no longer than 1.25 times the size of
the longest unaligned loop (L) between those two blocks in the input alignment.
Related parameters are -x and -c, and the maximum loop length between two blocks
during refinement is  min{ p*L + x, c}, where the default value of c = 0 means
an absolute loop length cutoff is not applied.  [Pre-existing loops exceeding the
limit, e.g., when p < 1, are permitted; they are *not* forced to be less than
the limit by the refinement.]



=============================
4)  fa2cd:  A FASTA -> CD file conversion utility:

This program will produce a blocked alignment that by default has all aligned
columns in the FASTA alignment.  The refinement algorithm's extension phase
alone will not have any effect in this case as every block is optimally
extended, unless the first block-shifting phase of refinement has made changes
to the alignment.  Run the converter as: 

fa2cd -i <input fasta file> 

The '-parseIds' flag will try and interpret the description line for each
sequence in terms of recognized sequence accession formats.  The '-ibm' flag
creates a CD whose alignment is the subset of columns from the input multiple
alignment that contain no gap characters. 

An optional -t <value> argument instructs the converter to trim 'bad' columns
from the output alignment.  Here, <value> is the minumum median PSSM score of a
column in the output alignment (3 is often a good choice).

(Use the -h or -help flags to see all available options.)

A few notes on FASTA file format:

a)  In general, input FASTA files are expected to follow the standard protein
FASTA file format (for example, https://www.ncbi.nlm.nih.gov/blast/fasta.shtml).
Further, it is important that each sequence input has a unique identifier to
avoid downstream errors.  By default, fa2cd assigns each sequence an integer as
its local accession.  With the '-parseIds' option you instruct fa2cd to use
information in your file to build sequence identifiers. 

b)  When using '-parseIds', the description line (i.e., those with a leading '>'
character; we may also use the term 'defline') is parsed by fa2cd, generating a
sequence identifier based on the characters in the defline up to the first
whitespace.  Therefore, to ensure unique identifiers make sure multiple
definition lines do not start with the identical same word.   

For example, if two sequences have deflines ">ABC 1" and ">ABC 2", they will be
assigned the same identifier when -parseIds is used.    

c)  Try and avoid using small integers as your defline (e.g., ">1"), unless
you've done this for all deflines without repeating a number.  When using
'-parseIds', unparseable identifiers may inadvertently duplicate a number you
have already used.  Future versions of fa2cd will fix such conflicts.  



[Information about fa2cd is also available at
https://www.ncbi.nlm.nih.gov/Structure/cdtree/cdtree.shtml.  fa2cd is also
distributed as part of the CDTree software, a GUI-based application to aid in
the classification of protein sequences and investigate their evolutionary
relationships.  See https://www.ncbi.nlm.nih.gov/Structure/cdtree/cdtree.shtml to
learn more about CDTree.] 


=============================
5)  Examples:

Sample data is available in the 'examples' subdirectory.  Example command line
parameters are [Note:  '-so' is an alias for the longer '-selection_order']:

Recommendation:
When runtime is not a concern, use a random selection order with
multiple trials.  By default, bma_refiner selects the highest-scoring final
alignment as the refined alignment, although the -nout option allows you to see
the final alignment produced in other trials.


This performs one refinement trial (-so 1 is a deterministic selection order,
implying -n 1) with a maximum of 5 cycles using a particular method for scoring
potential block extension/contraction events (-be_score 3.3.3).  
Each cycle performs both refinement phases:  

./bma_refiner -i <input filename> -o <output basename> -so 1 -p 1.0 -ex 20 -nc 5 -be_score 3.3.3  

This performs three refinement trials (-so 0 -n 3), each with a maximum of 5 cycles, but
skips the second refinement phase in every cycle (-be_fix).  The best two alignments from
among the three trials are saved to a file: 

./bma_refiner -i <input filename> -o <output basename> -so 0 -n 3 -p 1.0 -ex 20 -nc 5 -nout 2 -be_fix




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
