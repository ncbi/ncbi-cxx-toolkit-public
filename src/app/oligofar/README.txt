oligoFAR 3.19                     7-AUG-2008                                1-NCBI

NAME 
    oligoFAR version 3.19 - global alignment of single or paired short reads

SYNOPSIS
    oligofar [-hV] [-w wsize] [-n hashmism] [-N+|-] 
            [-a max-alt-qry] [-A max-alt-sbj] [-H v|m|a]
            [-z minDist] [-Z maxDist] [-D deltaDist] [-p minPct]
            [-u topCnt] [-t topPct] [-F dust] [-s strands]
            [-i input] [-d database] [-l gilist] [-b snpdb]
            [-o output] [-O outflags] [-g guidefile]
            [-D batchSize] [-r algo] [-X dropoff] 
            [-q qualChannels] [-0 qualBase] [-1 solexa1] [-2 solexa2] 
            [-y sensitivity] [-I score] [-M score] [-G score] [-Q score] 
            [-T test] [-L memlimit] 

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
    Reads to match are hashed (one window per read, preferrably at the 5' end)
    with a window size controlled by option -w. Option -n controls how many 
    mismatches are allowed within hashed values.  Option -a controls how many 
    alternative readings (because of ambiguous bases) of same read may be hashed 
    independently to mismatches allowed. Low quality 3' ends of the reads 
    may be clipped. Low complexity (controlled by -F argument) and low quality
    reads may be ignored.

    OligoFAR may use different implementations of the hash table (see -H):
    vector (uses a lot of memory, but is faster for big batches) and
    arraymap (lower memory requirements for smaller batches). 
    For vector -L should always be used and set to large value (GygaBytes).

    Database is scanned. If database is provided as blastdb, it is 
    possible to limit scan to a number of gis with option -l. If snpdb is
    provided, all variants of alleles are used to compute hash values, as well
    as regular IUPACna ambiguities of the sequences in database. Option -A
    controls maximum number of alternative readings of the same word.

    Alignments are seeded by hash and may be extended from 5' to 3' end by 
    one of the three algorithms:
        - simple score computation, no indels allowed (use -X0 for it);
        - Smith-Watermann banded alignment (use -Xn -rs for it, n > 0);
        - fast linear-time alignment (use -Xn -rf for it, n > 0), is almost as
          fast as no-indel algorithm, and times faster then the Smith-Watermann, 
          allows indels, but sometimes may produce suboptimal results.

    Alignments are filtered (see -p option). For paired reads geometrical
    constraints are applied (reads of the same pair should be on opposite
    strands with 5'-ends pointing outside, distznce is set by -z, -Z and -D
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
    score and reports only the best hits and ties to the best hits. Certain 
    options (like -N-) may affect completeness of tie hits reported - in this 
    case only hits of same score as the best are guarranteed to appear in 
    output no matter what value of -t is set.

    Scores of hits reported are in percent to the best score theoretically
    possible for the reads. Scores of paired hits are sums of individual
    scores, so they may be as high as 200.
    
PAIRED READS
    Pairs are looked-up constrained by requirements: 
     - reads should be on opposing strands
     - plus-stranded read should start before minus-stranded one
     - distance between lowest position of plus-stranded read and highest
       position on minus-stranded one should be in range [ $z - $D ; $Z + $D ] 
       where $z, $Z and $D are values given in -z, -Z and -D switches' 
       arguments correspondently.

    If pair has no hits which comply constraints mentioned above, individual 
    hits for the pair components still will be reported.

    Paired reads have one ID per pair. Individual reads in this case do not
    have ID, although report provides info which component(s) of the pair
    produce the hit.

OPTIONS
    
Service options
    --help    
    -h          Print help to stdout, finish parsing commandline, and then 
                exit with error code 0.
                The help output contains current values assumend for options, 
                so commandline arguments values which preceed -h or --help will
                be reflected in the output, for others default values will be
                printed.

    --version
    -V          Print current version and hash implementation, finish parsing
                commandline and exit with error code 0.

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
    -i filename  Read short reads from this file. See format description in
                section FILE FORMATS.

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

    --output-file=filename
    -o filename Sets output filename.

    --gi-list=filename
    -l filename Sets file with list of gis to which scan of database should be
                limited. Works only if database is in blastdb.

    --qual-1-file=filename
    -1 filename Sets file with 4-channel scores for the first component of
                paired reads or for single reads. Should contain data in
                exactly same order as the input file.

    --qual-2-file=filename
    -2 filename Sets file with 4-channel scores for the second component of
                paired reads. Should contain data in exactly same order an the
                input file.

    --quality-channels=cnt
    -q 0|1      Affects expected input file format - indicates does it have
                1-channel scores for reads in additional (4th and 5th ) columns.

    --quality-base=value
    --quality-base=+char
    -q value
    -q +char    Sets base value for character-encoded phrap quality scores,
                i.e. integer ASCII value or `+' followed by characer which 
                corresponds to phrap score of 0.

    --output-flags=-eumxtadh
    -O-eumxtadh Controls what types of records should be produced on output.
                See OUTPUT FORMAT section below. Flags stand for:
                e - empty line
                u - unmapped reads
                m - "more" lines (there were more hits of weaker rank, dropped)
                x - "many" lines (there were more hits of this rank and below)
                t - terminator (there were no more hits except reported)
                a - alignment - output alignment details in comments 
                d - differences - output positions of misalignments
                h - print all hits above threshold before ranking
                Use '-' flag to reset flags to none. So, -Oeumx -Ot-a is
                equivalent to -O-a.

    --batch-size=count
    -B count    Sets batch size (in count of reads). Too large batch size
                takes too much memory and may lead to excessive paging, too
                small makes scan inefficient. 

Hashing and scanning options
    --window-size=size
    -w size     Sets window size. Reasonable values are 11-13 for VectorTable, 
                11-15 for ArraymapTable implementations of hash.  OligoFAR
                hashes one window per read, choosing it as close to 5' end as
                possible, but may shift it to the right if the 5' end has too
                many low quality (ambiguous) bases.

    --input-max-mism=count
    -n count    Sets maximal allowed number of mismatches within hashed word.
                Reasonable values are 0 and 1, sometimes 2.

    --hash-type=v|m|a
    -H v|m|a    Set hash type to v (vector), m (multimap) or a (arraymap).
                Vector is the most memory consuming, but is the fastest for
                batches of reads of tens of thousands or more. Multimap is not
                practical (more intended for debugging). Arraymap is the least
                memory consuming and is fast for small counts of reads.
                Arraymap is the only practical in 32-bit application.

    --max-mism-only=+|-
    -N +|-      If set to +, reads are hashed with mismatches allowed by -n
                option just after being read. If set to -, they are first
                hashed with no mismatches allowed, then after first pass only
                those reads which have mismatches or have no hits are
                rehashed, then (if -n2 is used) on third pass only those reads
                which have two mismatches or more are rehashed.

    --input-max-alt=count
    -a count    Maximal number of alternative versions of window (based on
                ambiguous bases) allowed for read to be hashed. If oligoFAR
                fails to find window containing this or lower number of
                alternatives for a read, the read is ignored. For example, 
                ATSNAGATAG has 8 alternatives (4 [for N] * 2 [for S]).

    --fasta-max-alt=count
    -A count    Maximal number of alternative versions of a window (based on
                ambiguous bases) on database side for the window to be used 
                to perform hash lookup and seed an alignment. 

    --solexa-sensitivity=bits
    -y bits     sensitivity (0-15) for basecalling of 4-channel scores. Affects
                how scores are treated to produce ambiguities for hashing. The
                larger this value, the more ambiguities are produced while
                basecalling. But if oligoFAR can't find window with low enough
                number of alternatives for a read, it automatically reduces
                sensitivity value for this read repeatedly until the read may 
                be hashed.

    --max-simplicity=dust
    -F dust     Maximal dust score of a window for read to be hashed or for
                the hash lookup to be performed. 

    --strands=1|2|3
    -s 1|2|3    Strands of database to scan. 1 - positive only, 2 - negative
                only, 3 - both.

Alignment options
    --algorithm=f|s
    -r f|s      Alignment algorithm to use if indels are allowed:
                s - Smith-Watermann (banded)
                f - Fast linear time

    --x-dropoff=value
    -X value    X-dropoff. 0 forbids indels, otherwise for banded Smith-Watermann 
                it controls band width. Does not affect fast algorithm if
                above 0.

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

    --min-pair-dist=bases
    -z bases    Set minimal `insert' length allowed for paired reads. This is
                5' to 5' distance (reads should be on opposite strands
                pointing 5' ends outside).

    --max-pair-dist=bases
    -Z bases    Set maximal `insert' length allowed for paired reads. See also
                option -z. 

    --pair-margin=bases
    -D bases    Set fuzz, or margin for `insert' length for paired reads. The
                actual range of insert lengths allowed is [z - D, Z + D],
                where z, Z and D are the valuee set by -z, -Z and -D options
                correspondently.

EXAMPLE
    ./oligofar -i pairs.tbl -d contigs.fa -b snpdb.bdb -l gilist -g pairs.guide \
                -w 13 -B 250000 -Hv -n2 -rf -p90 -z100 -Z500 -D50 -L16G -o output -O mx

FILE FORMATS

Input file
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
    minus 33 representing phrap quality score for the appropriate base.

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

Guide file
    Guide file is an output of srsearch program or similar tool which finds 
    exact or almost exact matches for reads and read pairs.

    It should be tab-separated file having records with 7 or 11 columns (11 for
    paired hits or paired reads) with columns:

    1. type of result:
       for non-paired search - always 0;
       for paired search: 
        0 - paired match; 
        1 - non-paired match for the first member of the pair; 
        2 - non-paired match for the second member of the pair;
    2. query ordinal number (0-based) or query id;
    3. subject ordinal number (OID) or subject id;
    4. subject offset 1-based (for the first member of the pair if paired match);
    5. '0' or '-' - reverse strand; '1' or '+' - forward strand (for the first 
       member of the pair if paired match);
    6. mismatch position in the query (1-based, 0 for exact match) (for the first
       member of the pair if paired match);
    7. subject base at mismatch position ('-' for exact match) (for the
       first member of the pair if paired match);
    8. if paired match - subject offset of the second member of the pair;
    9. if paired match - strand of the second member of the pair;
    10. if paired match - mismatch position of the second member of the pair;
    11. if paired match - subject base at mismatch position for the second
        member of the pair.

    It is essential that order of hits in guide is the same as order of reads in
    input file.

    When parsing input, oligoFAR reads guide file and ignores records which do not
    satisfy following constraints:

    1. Column 1 should be 0, as well as columns 6 and 10 (if exists)
    2. If record has 11 columns, column 5 should differ from column 9
    3. If column 5 = '+' or '1', then column 8 should be > then column 4,
       otherwise column 4 should be > then column 8
    4. Distance between values in columns 4 and 8 plus length of read should
       fit within allowed distance (between min - margin and max + margin)

    If read is not ignored, it is added to output list, and read or read pair will
    bypass alignment procedure.

Gi list file
    is just list of integers, one number per line.
    
Solexa-style score file
    Should contain lines of whitespace-separated integer numbers, one line per
    read, 4 numbers per base. Order of reads in the file should correspond to
    order or reads in input file. Input file is required - it provides read
    IDs. If solexa-style file is set, IUPACna and quality scores from input
    file are ignored.

OUTPUT FORMAT
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

    "Ideal" mapping (having single best) should consider using filter on first 
    column = 0, second column = 1

    Scores for individual reads have maximum of 100, for paired - 200. No
    reads with score below the one set with -p option may appear in output.
    Since filtering by -p is performed before combining individual hits to
    paires, value for -p should not ever exceed 100. 

    If the flag a for -O is set, for every individual read of hit which score 
    is below 100 three additional lines will be printed. These lines start
    with `#' and contain graphical representation of the alignment (in subject
    coordinates, so that query may appear as reverse complement of the
    original read):

    # 3'=TTTCCTTTAGA-AGAGCAGATGTTAAACACCCTTTT=5' query[1] has score 68.6
    #     |||||||||| ||||||| | | ||||||||||||    i:31, m:4, g:1
    # 5'=ATTCCTTTAGATAGAGCAGTTTTGAAACACCCTTTT=3' subject

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

EXIT VALUES
    0 for success, non-zero for failure.

END
