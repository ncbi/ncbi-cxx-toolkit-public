oligoFAR 3.101                     03-NOV-2009                                1-NCBI

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
This file may be obsolete and will be removed - see man/oligofar.* 
for documentation
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

NAME 
    oligoFAR version 3.101 - global alignment of single or paired short reads

SYNOPSIS
    usage: [-hV] [--help[=full|brief|extended]] [-U version]
      [short-read-options] [-0 qbase] [-d genomedb] [-b snpdb] [-g guidefile]
      [-v featfile] [-l gilist|-y seqID] [--hash-bitmap-file=file]
      [-o output] [-O -eumxtdhz] [-B batchsz] [-s 1|2|3] [-k skipPos]
      [--pass0 hash-options] [--pass1 hash-options]
      [-a maxamb] [-A maxamb] [-P phrap] [-F dust] [-X xdropoff] [-Y bandhw]
      [-I idscore] [-M mismscore] [-G gapcore] [-Q gapextscore]
      [-D minPair[-maxPair]] [-m margin] [-R geometry]
      [-p cutoff] [-x dropoff] [-u topcnt] [-t toppct] [-L memlimit] [-T +|-]
      [--NaHSO3=yes|no]
    where hash-options are:
      [-w win[/word]] [-N wcnt] [-f wstep] [-r wstart] [-S stride] [-H bits]
      [-n mism] [-e gaps] [-j ins] [-J del] [-E dist]
      [--add-splice=pos([min:]max)] [--longest-del=val] [--longest-ins=val]
      [--max-inserted=val] [--max-deleted=val]
    and short-read-options are:
      [-i reads.col] [-1 reads1] [-2 reads2] [-q 0|1|4] [-c yes|no]
 
EXAMPLES
    oligofar -i pairs.tbl -d contigs.fa -b snpdb.bdb -l gilist -g pairs.guide \
             -w 20/12 -B 250000 -H32 -n2 -p90 -D100-500 -m50 -Rp \
             -L16G -o output -Omx

INPUT FORMAT OPTIONS
    following combinations of input format and data flags are allowed:

    1. with column file:
       -q0 -i input.col -c no 
       -q1 -i input.col -c no 
       -q0 -i input.col -c yes
    2. with fasta or fastq files:
       -q0 -1 reads1.fa  [-2 reads2.fa]  -c yes|no
       -q1 -1 reads1.faq [-2 reads2.faq] -c no
    3. with Solexa 4-channel data
       -q4 -i input.id -1 reads1.prb [-2 reads2.prb] -c no

    See options and file formats for more info.

CHANGES
    Following parameters are new, have changed or have disappeared 
    in version 3.25: -n, -w, -N, -S, -x, -f, -R 
    in version 3.26: -n, -w, -N, -z, -Z, -D, -m, -S, -x, -f, -k
    in version 3.27: -n, -w, -e, -H, -S, -a, -A, --pass0, --pass1
    in version 3.28: -y, -R, -N
    in version 3.29: --NaHSO3 (Development)
    in version 3.91: -X -Y -r -O --NaHSO3 
    in version 3.98: -x -g -O -B
    in version 3.100: -v 
    in verison 3.101: -i -1 -2 -q -O

DESCRIPTION
    Performs global alignments of multiple single or paired short reads 
    with noticeable error rate to a genome or to a set of transcripts 
    provided in a blast-db or a fasta file.

    Reads may be provided as UIPACna base calls, possibly accompanied 
    with phrap scores (referred below as 1-channel quality scores), 
    or as 4-channel Solexa scores. Input file format is described 
    below in section FILE FORMATS.

    Output of srsearch (referred below as guide-file) or of a similar 
    program which performs exact or nealy exact short read alignment
    may be used as input for oligoFAR to ignore processing of perfectly 
    matched reads, but format the matches to output in uniform with
    oligoFAR matches way.

    Input is processed by batches of size controlled by option -B. 
    Reads to match are hashed (one window (unless option -N is used) per read, 
    preferrably at the 5' end) with a window size controlled by option -w. 
    Option -n controls how many mismatches are allowed within hashed values.  
    Option -a controls how many ambiguous bases withing a window of a read 
    may be hashed independently to mismatches allowed. Low quality 3' ends 
    of the reads may be clipped.  Low complexity (controlled by -F argument) 
    and low quality reads may be ignored.

    OligoFAR may use different implementations of the hash table (see -H):
    vector (uses a lot of memory, but is faster for big batches) and
    arraymap (lower memory requirements for smaller batches). 
    For vector -L should always be used and set to large value (GygaBytes).

    Database is scanned. If database is provided as blastdb, it is 
    possible to limit scan to a number of gis with option -l. If snpdb is
    provided, all variants of alleles are used to compute hash values, as well
    as regular IUPACna ambiguities of the sequences in database. Option -A
    controls maximum number of ambiguities in the same window.

    Alignments are seeded by hash and may be extended by Smith-Watermann
    algorithm (unless -X0 or -Y0 is used).

    Alignments are filtered (see -p option). For paired reads geometrical
    constraints are applied (reads of the same pair should be mutually 
    oriented according to -R option, distance is set by -D and -m
    options). Then hits ranked by score (hits of the same score have same
    rank, best hits have rank 0). Week hits or too repetitive hits are 
    thrown away (see -t and -u options).

    At the end of each batch both alignments produced by oligoFAR and
    alignments imported from guide-file which have passed filtering and
    ranking get printed to output file (if set) or stdout (see FILE FORMATS
    for output format).

NOTE
    Since it is global alignment tool, independent runs against, say,
    individual chromosomes and run against full genome will produce different 
    results.

    To save disk space and computational resources, oligoFAR ranks hits by
    score and reports only the best hits and ties to the best hits. 
    In the two-pass mode tie hits may be incompletely reported - in this 
    case only hits of same score as the best are guarranteed to appear in 
    output no matter what value of -t is set.

    Scores of hits reported are in percent to the best score theoretically
    possible for the reads. Scores of paired hits are sums of individual
    scores, so they may be as high as 200.
    
PAIRED READS
    Pairs are looked-up constrained by following requirements: 
     - relative orientation (geometry) which may be set by --geometry or -R 
       (see section OPTIONS subsection ``Filtering and ranking options'')
     - distance between lowest position of the two reads and highest
       position of the two reads one should be in range [ $a - $m ; $b + $m ] 
       where $a, $b and $m are arguments of parameters -D $a-$b -m $m.

    If pair has no hits which comply constraints mentioned above, individual 
    hits for the pair components still will be reported. Also for each
    component unpaired hits better then the best paired hit will be reported.

    Paired reads have one ID per pair. Individual reads in this case do not
    have individual ID, although report provides info which component(s) of 
    the pair produce the hit.

SODIUM BISULFITE TREATMENT
    To discover methylation state of DNA sodium bisulfite curation may be
    used before producing reads.  In order to simulate this procedure
    oligoFAR has special mode, which may be turned on by:
    
        --NaHSO3=true

    It is advised to use longer words and windows in this mode for better
    performance. 
    
    This mode is not compatible with colorspace computations.

MULTIPASS MODE
    By default oligoFAR aligns all reads just once, but if option --pass1 is 
    used, oligoFAR switches to the two-pass mode. Parameters -w, -n, -e, -H, 
    and some other, preceeding --pass1 or following --pass0 affect first run, same 
    parameters when follow --pass1 are for the second run.  For the second run 
    only reads (or pairs) having more mismatches or indels then allowed in 
    parameters for the first pass will be hashed and aligned. So using something 
    like:

    oligofar --pass0 -w22/22 -n0 -e0 --pass1 -w22/13 -n2 -e1 

    will pick up exact matches first, and then run search with less strict 
    parameters only for those reads which did not have exact hits.

WINDOW, STRIDE AND WORD
    When hashing, oligoFAR first chooses some region on the read (called window).
    If the stride is greater then 1, it extracts the "stride" windows with offset 
    of 1 to each other. Then each window gets variated (with mismatches and/or 
    indels). If word size is equal to window size, window is hashed as is. If 
    word size is smaller then window, two possibly overlapping words are created
    and added to hash: at the beginning of the window and at the end of the 
    window.

    Example for case -N2 -w13/9 -S3 -e0:
    
    +-------------+               hashed region 1 (should not exceed 32 bases)
                 +-------------+  hashed region 2 (should not exceed 32 bases)
    ACGTGTTGATGACTACTGATGATCTGATccat
    +-----------+                 window group 1
    ACGTGTTGATGAC                 window 1 \
     CGTGTTGATGACT                window 2  } stride = 3
      GTGTTGATGACTA               window 3 /
                 +-----------+    window group 2
                 TACTGATGATCTG    window 4 \
                  ACTGATGATCTGA   window 5  } stride = 3
                   CTGATGATCTGAT  window 6 /
    +-------+                     word size = 9
    ACGTGTTGA                     word 1 of window 1
        GTTGATGAC                 word 2 of window 1
     CGTGTTGAT                    word 1 of window 2
         TTGATGACT                word 2 of window 2
      GTGTTGATG                   word 1 of window 3
          TGATGACTA               word 2 of window 3
                 +-------+        word size = 9
                 TACTGATGA        word 1 of window 4
                     GATGATCTG    word 2 of window 4
                  ACTGATGAT       word 1 of window 5
                      ATGATCTGA   word 2 of window 5
                   CTGATGATC      word 1 of window 6
                       TGATCTGAT  word 2 of window 6

    If you allow indels to be hashed, hashed region is extended by 1 base. So 
    for window of 24 and stride 4 with indels allowed reads should be at least
    28 bases long (27 with no indels).

    Words when hashing are splet into the two parts: index and supplement. 
    Supplement can't have more then 16 bits. Index can't be more then 31bits.
    Therefore maximal word size should fit in 47 bits which is 23 bases.

OPTIONS

Service options
    --help=[brief|full|extended]
    -h          Print help to stdout, finish parsing commandline, and then 
                exit with error code 0. Long version may accept otional 
                argument which specifies should be printed brief help version
                (synopsis), full version (without extended options) or extended
                options help.

                The output (except brief) contains current values taken for options, 
                so commandline arguments values which preceed -h or --help will
                be reflected in the output, for others default values will be
                printed.

    --version
    -V          Print current version and hash implementation, finish parsing
                commandline and exit with error code 0.

    --assert-version=version
    -U          If oligofar version is not that is specified in argument,
                forces oligofar to exit with error.  Every time this option 
                appears in commandline or in config file, the comparison is
                performed, so each config file may contain this check
                independently.  

    --test-suite=+|-
    -T +|-      Run internal tests for basic operations before doing anything
                else.

    --memory-limit=value
    -L value    Set upper memory limit to given value (in bytes). Suffixes k,
                M, G are allowed. Default is 'all free RAM + cache + buffers'.
                Recommended value for VectorTable hash implementation is 14G or
                above.

File options
    --input-file=filename
    -i filename For -q0 and -q1 read file with 2,3, or 5 space-separated 
                columns (see FILE FORMATS); for -q4 read only first column 
                with read IDs.

    --fasta-file=filename
    -d filename Sets database file name. If there exists file with given name
                extended with one of suffixes .nin, .nal - the suffixed version 
                is opened as blastdb, otherwise the file itself is expected to
                contain sequences in FASTA format. 
    
    --guide-file=filename
    -g filename Sets guide file name. The file should have exactly the same order 
                of reads as input file does. There may be multiple hits per each
                read, or some reads may be skipped.

    --snpdb-file=filename
    -b filename Sets SNP database filename. Snpdb is a file prepared by an
                oligofar.snpdb program.

    --feat-file=filename
    -v filename Sets filename for feature file which is three column file
                containing subject sequence id, begin and end positions for
                regions within which scanning should be performed
                
    --output-file=filename
    -o filename Sets output filename.

    --gi-list=filename
    -l filename Sets file with list of gis to which scan of database should be
                limited. Works only if database is in blastdb.

    --only-seqid=seqid
    -y seqid    Limits database lookup to this seqid. May appear multiple
                times - then list of seqids is used. Does not work with -l.
                Comparison is pretty strict, so lcl| or .2 are required in
                'lcl|chr12' and 'NM_012345.2'. 

    --read-1-file=filename
    -1 filename For -q0 read this file as fasta file of the reads sequences;
                for -q1 read this file as fastq file of the reads sequences and
                quality; for -q4 read file with 4-channel (Solexa) quality scores, 
                then this requires also -i for read IDs (which should go in
                the same order). All 1 sequence per read - for paired reads,
                pair mates will be in the file specified by option -2.

    --read-2-file=filename
    -2 filename In case of paired reads contains pair mate data in the same
                order and the same format as in file specified by option -1.

    --quality-channels=cnt
    -q 0|1|4    What data are expected on input: with -q0 it should be either 
                2 or 3 column file in -i or fasta file(s) in -1, -2; with -q1 
                it should be either 5-column file in -i or fastq file(s) in
                -1, -2; with -q4 read IDs are read from -i and Solexa
                4-channel scores from -1 and -2.

    --quality-base=value
    --quality-base=+char
    -0 value
    -0 +char    Sets base value for character-encoded phrap quality scores,
                i.e. integer ASCII value or `+' followed by character which 
                corresponds to the phrap score of 0.

    --colorspace=+|-
    -c +|-      Input is in di-base colorspace encoding. Hashing and alignment 
                will be performed in the colorspace encoding.  Requires -q0.

    --NaHSO3=+|- (Development)
                Turns sodium bisulfite treatment simulation mode on or off.
                Make sure to use reasonable value for -A.

    --output-flags=-eumxtadrh
    -O-eumxtadrh 
                Controls what types of records should be produced on output.
                See OUTPUT FORMAT section below. Flags stand for:
                e - empty line
                u - unmapped reads
                m - "more" lines (there were more hits of weaker rank, dropped)
                x - "many" lines (there were more hits of this rank and below)
                t - terminator (there were no more hits except reported)
                a - alignment - output alignment details in comments 
                d - differences - output positions of misalignments
                r - print raw scores, rather then relative to the best
                h - print all hits above threshold before ranking
                Use '-' flag to reset flags to none. So, -Oeumx -Ot-a is
                equivalent to -O-a.

    --batch-size=count
    -B count    Sets batch size (in count of reads). Too large batch size
                takes too much memory and may lead to excessive paging, too
                small makes scan inefficient. 

    --batch-range=min[-max]
                Process only batches in given ordinal range. First batch is 0,
                therefore if one uses -B100000 --batch-range=2-4 reads from 
                200000 to 499999 will be processed in three batches 100000 in
                each. Convenient for parallel processing.

Hashing and scanning options
    --pass0     Hashing and pairing (-D, -m) options which follow this flag will 
                be applied to the first pass.

    --pass1     Turns on two-pass mode; hashing and pairing (-D, -m) options  
                that follow this flag will be applied to the second pass.

    --window-size=window[/word]
    -w window[/word]
                Sets window and word size.

    --max-windows=count
    -N count    Set maximal number of consecutive windows to hash.
                Should be 1 (default) if used with -k. Actual number of words
                will be multiplied by stride size. Also alternatives, indels 
                and mismatches will extend this number independently.

    --window-start=position
    -r position Start first hashed window at this position (default is 1)

    --window-step=dist
    -f dist     Set step between hashed windows (default is window size plus 
                number of indels allowed in hash)

    --input-mism=mism
    -n mism     Sets maximal allowed number of mismatches within hashed word.
                Reasonable values are 0 and 1, sometimes 2.

    --input-gaps=[0,]0|1|2
    -e [0,]0|1  Sets maximal number of indels per window. Allowed values are 
                0, 1, and 2.  Value of 2 allows only single indel of length 
                up to 2 bases.

    --max-hash-dist=value
    -E value    Hash only at most with this amount of errors within window 
                (default is max( -n, -e, -E ))

    --index-bits=bits
    -H bits     Set number of bits in hash value to be used as an direct index.
                The larger this value the more memory is used.

    --stride-size=size
    -S stride   Stride size. Minimum value is 1. Maximum is one base smaller 
                then word size. The larger this value is, the more hash 
                entries are created, but the less hash lookups are performed
                (every "stride's" base of subject). 

    --window-skip=pos,...
    -k pos,...  Always skip certain positions of reads when hashing.
                Useful in combination with -n0 when it is known that certain
                bases have poor quality for all reads.
                If used with --ambiguify-positions=true (extended option, may
                be changed in next release), just replaces appropriate bases
                with `N's, then this option affects scoring as well.

    --indel-pos=pos
    -K pos      Allow indels only at this position of read when hashing

    --input-max-amb=count
    -a count    Maximal number of ambiguous bases in a window allowed for read 
                to be hashed. If oligoFAR fails to find window containing 
                this or lower number of ambiguities for a read, the read is 
                ignored. 

    --fasta-max-amb=count
    -A count    Maximal number of ambiguous bases in a window on a subject
                side for the window to be used to perform hash lookup and 
                seed an alignment. 

    --phrap-cutoff=score
    -P score    Phrap score for which and below bases are considered as
                ambiguous (used for hashing). The larger it is, the more
                chances to seed lower quality read to where it belongs, but
                also the larger hash table is which decreases performance. 
                Affects 1-channel and 4-channel input. Replaces obsolete 
                --solexa-sencitivity (-y) option.

    --max-simplicity=dust
    -F dust     Maximal dust score of a window for read to be hashed or for
                the hash lookup to be performed. 

    --strands=1|2|3
    -s 1|2|3    Strands of database to scan. 1 - positive only, 2 - negative
                only, 3 - both.

Alignment options
    --indel-dropoff=value
    -X value    Longest indel reliably detectable. 0 forbids indels, otherwise 
                for banded Smith-Watermann it controls band width

    --extention-dropoff=value
    -x value    maximal score penalty which may be accumulated when extending
                seeded alignment

    --band-half-width=value
    -Y value    Band half width for matrix to compute; it makes no sense for X
                to exceed Y. 0 forbids indels.

    --identity-score=score
    -I score    Set identity score

    --mismatch-score=score
    -M score    Set mismatch score

    --gap-opening-score=score
    -G score    Gap opening score

    --gap-extention-score=score
    -Q score    Gap extention score

Filtering and ranking options
    --min-pctid=pct
    -p pct      Set minimal score for hit to appear in output.  Scores reported are 
                in percent to the best score theoretically possible for the read, so 
                perfect match is always 100.

    --top-percent=pct
    -t pct      Set `tie-hit' cutoff. Taking the best hit for the read or read
                pair as 100%, this is the weakest hit for the read to be
                reported. So, if one has -t 90, and the best hit for some read
                has score 90, report will contain hits with scores between
                90%*90 = 81 and 90 (provided that -p is 81 or below). This
                option does not affect lines generated by output option -Oh.

    --top-count=cnt
    -u cnt      Set maximal number of hits per read to be reported. The hits
                reported by ouptut option -Oh do not count - they will be
                reported all.

    --pair-distance=min[-max]
    -D m[-M]    Set allowed range for distance between component hits for 
                paired read hits. If max is omited it is considered to be equal 
                to min.Length of hits should be included to this length. 

    --pair-margin=bases
    -m bases    Set fuzz, or margin for `insert' length for paired reads. The
                actual range of insert lengths allowed is [a - m, b + m],
                where a, b, and m are set with -D a-b -m m command line parameters.

    --geometry=type
    -R geometry Sets allowed mutual orientation of the hits in paired read hits. 
                Values allowed (synonyms are separated by `|') are:

        p|centripetal|inside|pcr|solexa     reads are oriented so that vectors 
                                            5'->3' are pointing to each other
                                            ex: >>>>>>>   <<<<<<<

        f|centrifugal|outside               reads are oriented so that vectors 
                                            5'->3' are pointing outside
                                            ex: <<<<<<<   >>>>>>>

        i|incr|incremental|solid            reads are on same strand, first 
                                            preceeds second on this strand
                                            ex: >>>1>>>   >>>2>>>
                                            or: <<<2<<<   <<<1<<<

        d|decr|decremental                  reads are on same strand, first 
                                            succeeds second on this strand
                                            ex: >>>2>>>   >>>1>>>
                                            or: <<<1<<<   <<<2<<<

                In examples above the pattern >>>1>>> means first component of 
                the paired read on plus strand, <<<2<<< means second component 
                on reverse complement strand; if the digit is not set then the
                component number does not matter for the example.

                Combinations of the values are not allowed.

Extended options
    These options are supposed more for development cycle - they may choose 
    development versus production algorithm implementations, or set parameters 
    that may not supposed to be exposed in the future. These options should 
    not be used on production and are not guarranteed to be preserved.

    --min-block-length=bases
                Set minimal length of the subject sequence block to be 
                processed as a whole with same algorithm (depending on 
                presence of ambiguity characters).  Added to tune 
                performance when high density of SNPs is provided. Makes
                no difference with -A1.

FILE FORMATS

Column-based input file
    Input file is two to five column whitespace-separated text file. Empty
    lines and lines starting with `#' are ignored (note: they are ignored as
    if not present, so if you use # to comment out a read but provide solexa
    file with -1, or if you use guide file, the corespondense of read ids to 
    the scores or to gided hits will be broken).

    Columns:
    #1 - considered as read-id or read-pair id
    #2 - UIPACna sequence of read #1
    #3 - UIPACna sequence of read #2 (optional), or '-' if the read does not have pair
    #4 - quality scores for read #1 or '-' (optional)
    #5 - quality scores for read #2 or '-' (optional)

    Quality scores should be represented as ASCII-strings of length equal to 
    appropriate read length, one char per base, with ASCII-value of each char 
    minus 33 representing phrap quality score for the appropriate base.  The 
    number 33 here may be changed with -0 (--quality-base) parameter.

    If -q1 is used, column 4 is required and should not be '-'; same as column 5 
    if column 3 is not '-' and not empty.

    If -q0 is used, columns 3-5 are optional and columns 4, 5 are never used.
    Column 3 is used if present and non-empty and not '-'.

    Mixed (paired and non-paired) input is allowed - rows with columns 3,5 
    containing '-' may interleave rows with all four columns set.

    Example:

    rd1    ACAGTAGCGATGATGATGATGATGATWNG    -    ????>????=<?????>>>(876555+!.  -

    Here in column 4 each char represents a base score for appropriate base in
    column 2, e.g. ? indicates phrap score of 30, > stands for phrap score of 
    29, etc.

Input file with di-base colorspace reads (SOLiD technology)
    Reads may be specified in di-base colorspace encoding.  Option --colorspace=+ 
    should be used, quality scores will be ignored.  SEquence representation should 
    be following: first base is IUPACna, all the rest are digits 0-3 representing 
    dibases:

    0 - AA, CC, GG, or TT
    1 - AC, CA, GT, or TG
    2 - AG, GA, CT, or TC
    3 - AT, TA, CG, or GC

    Example: 

    rd1    C02033003022113110030030211    -
    
    The read above represents sequence CCTTATTTAAGACATGTTTAAATTCAC.

Guide file
    Starting from version 3.99, guide file should be in SAM 1.2 format. 
    
    Also this file should either have OligoFAR version of CIGAR alignments
    (with R for replace, C for changed base, B for overlap), or have field MD:
    set for all reads, because otherwise there is no way to compute compatible 
    alignment score and estimate should the guide alignment be taken or the
    query should be realigned.

    It is essential that order of hits in guide is the same as order of reads in
    input file.

Gi list file
    is just list of integers, one number per line.
    
Solexa-style score file
    Should contain lines of whitespace-separated integer numbers, one line per
    read, 4 numbers per base. Order of reads in the file should correspond to
    order or reads in input file. Input file is required - it provides read
    IDs. If solexa-style file is set, IUPACna and quality scores from input
    file are ignored.

FASTA reads file 
    should be an argument of -1 and -2 when -q0 is used. No spaces are allowed
    after '>' sign. As an extension, colorspace sequence data may be used
    instead of IUPAC.  See note below regarding read identifiers.

FASTQ reads file
    should be an argument of -1 and -2 when -q1 is used.  No spaces are
    allowed after '@' or '+' signs specifying ID lines.  Since now (as of
    version 3.101) oligofar does not support colorspace with quality scores,
    only IUPAC seequence format may be used. See note below regarding read
    identifiers.

Read identifiers
    It is essential that each of the files given by options -i, -1, -2, -g
    contains read information in the same order, exactly one record per read
    (except -g which may have multiple or no records).  Paired reads should
    have the same ID and be in different files (for -1, -2) or columns 
    (for -i); exception is Illumina-style naming: if the reads in -1 have ID
    ending by "/1" AND ids in -2 are ending by "/2", these last slash and 
    digit are being ignored. 
    
    NB: it is expected that both output and guide SAM do not have "/1"
        and "/2" components in readID.

Feature file
    Is a three-column whitespace-separated file containing 1-based closed
    regions on the reference gequences to scan. 
    Columns are: sequence-id, from, and to.

OUTPUT FORMAT
    There are two formats avaialble for output: SAM 1.2 and oligoFAR
    proprietary. Default is SAM, although for some purposes proprietary 
    may be more convenient.

    SAM format does not output quality information, sequence data may differ
    from input if quality scoreas are used. Tags AS, XN and XR are generated,
    where XR is rank (see explanation in proprietary output section) and XN is
    number of sequences in output for this rank.

Proprietary output format
    Output file is a 15-column tab-separated file representing different types
    of records (see -O options) in uniform way. 

    Columns:
    1.  Rank of the hit by score (0 - highest), 
        or `*' for unranked hits (see -Oh).
    2.  Number of hits of this rank, or "many" if there are more then it
        can be printed by -u option value, or "none" if there are no hits for 
        the query line, or "more" if followed after line "many", or "no_more"
        for terminator lines (see -Ot), or "hit" for unranked hits (see -Oh),
        or "diff" for lines that present differences of reads to subject 
        (see -Od). See details below.
    3.  Query-ID - read or read pair ID.
    4.  Subject-ID - one or more IDs of the subject sequence. Which one(s) is 
        not defined here and is implementation dependent.
    5.  Mate bitmask: 0 for no hits, 1 if first read matches, 2 if second
        read matches, 3 for paired match.
    6.  Total score for the hit (sum or individual read scores)
    7.  Position `from' on subject for the read or for the first read of the 
        pair for paired read, or `-' if it is hit for the second read of the 
        pair only.
    8.  Position `to' for the read or for the first read of the paired read,
        or `-' if it is hit for the second read of the pair. This position is
        included in the range, and indicates strand (to < from for negatsve 
        strand). 
    9.  Score for first read hit, or `-'.
    10. Position `from' for the second read, or `-'.
    11. Position `to' for the second read (to < from for negative strand), 
        or `-'.
    12. Score for second read hit, or `-'.
    13. Pair "orientation": '+' if first read is on positive strand, '-'
        if second read is on positive strand.
    14. Instantiated as IUPACna image of positive strand of the subject 
        sequence at positions where the first read maps, or '-'.
    15. Instantiated as IUPACna image of positive strand of the subject 
        sequence at positions where the second read maps, or '-'.
    16. CIGAR alignment of the first read in subject strand, or '-'. 
    17. CIGAR alignment of the second read in subject strand, or '-'.

    "Ideal" mapping (having single best) should consider using filter on first 
    column = 0, second column = 1

    Scores for individual reads have maximum of 100, for paired - 200. No
    reads with score below the one set with -p option may appear in output.
    Since filtering by -p is performed before combining individual hits to
    paires, value for -p should not ever exceed 100. 

    CIGAR here uses following letters:
        M - match (*)
        R - replacement (**)
        I - insertion
        D - deletion
        S - soft masking (dovetail)
        C - changed (scored as match although is not the same) (**)
        N - splice (not penalized deletion) 
        B - overlap (subject basea match twice) (**)
    Here (*) means that code is changed compared to the extended CIGAR
    (as described in SAM 1.2 standard), and (**) is addition to the CIGAR.

    Following extended CIGAR codes are not produced by oligoFAR:
        H - hard masking
        P - padding

Following output flag (a) is currently ignored, 
but description is preserved for future
    If the flag a for -O is set, for every individual read of hit which score 
    is below 100 three additional lines will be printed. These lines start
    with `#' and contain graphical representation of the alignment (in subject
    coordinates, so that query may appear as reverse complement of the
    original read):

    # 3'=TTTCCTTTAGA-AGAGCAGATGTTAAACACCCTTTT=5' query[1] has score 68.6
    #     |||||||||| ||||||| | | ||||||||||||    i:31, m:4, g:1
    # 5'=ATTCCTTTAGATAGAGCAGTTTTGAAACACCCTTTT=3' subject

    or for di-base colorspace reads:

    # 3'=31200000222133320222333030T=5' query[1]
    #    ||| |||||||||||||||||||||||    i:26, m:0, g:1
    # 5'=TAC-TTTTTCTCATATCCTCTATAATT=3' subject

    This format is intended for human review and may be changed in future
    versions.

Output record types
    If a flag `h' was used for -O option, every individual hit with score above
    that was set by -p option will be immediately reported in a line looking
    like

    *    hit    ?    ?    ?    ?    ?    ?    ?    ?    ?    ?    ?    ?    ?

    where ? are set to appropriate values. If the hit bypasses ranking it will
    be also printed as ranked hit after batch is processed. Regular ranked
    hits output may look like:

    0  1     rd1  gi|192  1  100  349799  349825  100  - - - +  ACT...  -
    1  3     rd1  gi|195  1  95   799070  799096  95   - - - +  AGT...  -
    1  3     rd1  gi|198  1  95   99070   99096   95   - - - +  ACT...  -
    1  3     rd1  gi|298  1  95   299050  299024  95   - - - -  TTA...  -
    2  many  rd1  gi|978  1  92   267050  267076  92   - - - -  TTA...  -
    2  many  rd1  gi|576  1  92   167050  167076  92   - - - +  ACT...  -
    2  more  rd1  *       *  *    *       *       *    * * * *  *       *

    Line 1 here means that there is 1 (col 2) hit with best score (col 1) of 
    100 (col 6).

    Lines 2-4 mean that there are also three (col 2) hits with next to best 
    (col 1) score (col 6) of 95.

    Lines 5,6 mean that there are many (col 2) reads of rank 2 in score (if 
    flag x in -O is not used, there will be number 2 in col 2 of these lines).

    Line 7 means that there are more hits of rank 2 and below, which are not
    reported. If flag m in -O is not set, this line will not appear).

    This example suggests that -u 6 option was used (that's why no more hits
    are reported).

    If all hits with score above that given by -p are reported, output would
    look like:

    0  1       rd1 gi|192  1  100  349799  349825  100  - - - +  ACT...  -
    1  3       rd1 gi|195  1  95   799070  799096  95   - - - +  AGT...  -
    1  3       rd1 gi|198  1  95   99070   99096   95   - - - +  ACT...  -
    1  3       rd1 gi|298  1  95   299050  299024  95   - - - -  TTA...  -
    2  3       rd1 gi|978  1  92   267050  267076  92   - - - -  TTA...  -
    2  3       rd1 gi|576  1  92   167050  167076  92   - - - +  ACT...  -
    2  3       rd1 gi|585  1  92   465010  465036  92   - - - +  ACT...  -
    2  no_more rd1 -       0  0    -       -       0    - - - *  -       -

    Last line appears only if the flag t of option -O is used.

    If the flag d of option -O is set, every record having score below 100 (or
    below 200 for paired reads) will be followed by one or more records of
    type `diff':

    1  diff  rd1  gi|195  1  95  799071  799071  95  -  -  -  +  C=>G  -

    which means that base C at position 799071 of gi|195 is replaced with G in
    read. Diff lines are always converted to be on positive strand of the
    subject sequence.  Differences longer then one base may be reported,
    IUPACna with ambiguity characters extended with `-' for deletion may be
    used. For paired reads differences for each read are reported in separate
    records, but in appropriate columns (for second read columns 10, 11, 12, 15
    are used instead of 7, 8, 9, 14). Column 13 is always `+'.

    If the flag u of option -O is set, and read has no matches, the line like
    following will appear on output:

    0  none  rd2  -  0  0  -  -  0  -  -  0  *  ACG... -

    where columns 14 and 15 will contain basecalls of the read or reads (if it
    is pair) instead of subject sequence interval.

    Normally all records except `hit' record are clustered by read id (but not
    sorted). If flag e of option -O is set, an empty line is inserted in
    output between blocks of records for different reads to visually separate
    them.

BUGS, UNTESTED AND DEVELOPMENT CODE, SPECIAL CASES
    There should be some ;-)

EXIT VALUES
    0 for success, non-zero for failure.

END

vim:expandtab:tabstop=4
