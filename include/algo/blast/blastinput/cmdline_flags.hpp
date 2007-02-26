/*  $Id$
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
 *
 * Author: Christiam Camacho
 *
 */

/** @file cmdline_flags.hpp
 *  Constant declarations for command line arguments for BLAST programs
 */

#ifndef ALGO_BLAST_BLASTINPUT__CMDLINE_FLAGS__HPP
#define ALGO_BLAST_BLASTINPUT__CMDLINE_FLAGS__HPP

#include <corelib/ncbistd.hpp>
#include <string>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Query sequence(s)
extern const string kArgQuery;

/// Output file name
extern const string kArgOutput;

/// BLAST database name
extern const string kArgDb;

/// Effective length of BLAST database
extern const string kArgDbSize;

/// Subject input file to search
extern const string kArgSubject;

/// BLAST database molecule type
extern const string kArgDbType;

/// gi list file name to restrict BLAST database
extern const string kArgGiList;

/// Query genetic code
extern const string kArgQueryGeneticCode;
/// Default value for query genetic code
extern const int kDfltArgQueryGeneticCode;

/// Database genetic code
extern const string kArgDbGeneticCode;
/// Default value for database genetic code
extern const int kDfltArgDbGeneticCode;

/// Argument to determine whether searches should be run locally or remotely
extern const string kArgRemote;

/// Argument to determine the number of threads to use when running BLAST
extern const string kArgNumThreads;

/// Argument for scoring matrix
extern const string kArgMatrixName;
/// Default value for scoring matrix
extern const string kDfltArgMatrixName;

/// Argument for expectation value cutoff
extern const string kArgEvalue;
/// Default value for expectation value cutoff
extern const double kDfltArgEvalue;

/// Argument to select formatted output type
extern const string kArgOutputFormat;
/// Default value for formatted output type
extern const int kDfltArgOutputFormat;
/// Argument to specify whether the GIs should be shown in the deflines in the
/// traditional BLAST report
extern const string kArgShowGIs;
/// Argument to specify the number of one-line descriptions to show in the
/// traditional BLAST report
extern const string kArgNumDescriptions;
/// Argument to specify the number of alignments to show in the traditional 
/// BLAST report
extern const string kArgNumAlignments;

/// Argument to select the gap opening penalty
extern const string kArgGapOpen;
/// Argument to select the gap extending penalty
extern const string kArgGapExtend;

/// Argument to select the nucleotide mismatch penalty
extern const string kArgMismatch;
/// Default value for nucleotide mismatch penalty
extern const int kDfltArgMismatch;
/// Argument to select the nucleotide match reward
extern const string kArgMatch;
/// Default value for nucleotide match reward
extern const int kDfltArgMatch;

/// Argument to select the ungapped X dropoff value
extern const string kArgUngappedXDropoff;
/// Argument to select the gapped X dropoff value
extern const string kArgGappedXDropoff;
/// Argument to select the final gapped X dropoff value
extern const string kArgFinalGappedXDropoff;

/// Argument to select the window size in the 2-hit wordfinder algorithm
extern const string kArgWindowSize;

/// Argument to select the wordfinder's word size
extern const string kArgWordSize;

/// Argument to specify the minimum word score such that the word is added to
/// the lookup table
extern const string kArgWordScoreThreshold;

/// Argument to specify the effective length of the search space
extern const string kArgEffSearchSpace;

/// Argument to specify that Smith-Waterman algorithm should be used to compute
/// locally optimal alignments
extern const string kArgUseSWTraceback;

/// Argument to specify whether lowercase masking in the query sequence(s)
/// should be interpreted as masking
extern const string kArgUseLCaseMasking;
/// Argument to select the query strand(s) to search
extern const string kArgStrand;
/// Default value for strand selection
extern const string kDfltArgStrand;
/// Argument to specify a location to restrict the query sequence(s)
extern const string kArgQueryLocation;
/// Argument to specify if the query sequence(s) defline should be parsed
extern const string kArgParseQueryDefline;

/// Argument to specify the maximum length of an intron when linking multiple
/// distinct alignments (applicable to translated queries only)
extern const string kArgMaxIntronLength;
/// Default value for maximum intron length
extern const int kDfltArgMaxIntronLength;

/// Argument to specify the culling limit
extern const string kArgCullingLimit;
/// Default argument to specify the culling limit
extern const int kDfltArgCullingLimit;

/// Argument to specify the frame shift penality
extern const string kArgFrameShiftPenalty;

/// Argument to specify number of bits to initiate gapping
extern const string kArgGapTrigger;
/// Default value for number of bits to initiate gapping
extern const double kDfltArgGapTrigger;

/// Argument to specify whether the search should be ungapped only
extern const string kArgUngapped;

/// Argument to specify the composition based statistics mode to sue
extern const string kArgCompBasedStats;


/// Argument to specify SEG filtering on query sequence(s)
extern const string kArgSegFiltering;
/// Default arguments to apply SEG filtering on query sequence(s)
extern const string kDfltArgSegFiltering;

/// Argument to specify DUST filtering on query sequence(s)
extern const string kArgDustFiltering;
/// Default arguments to apply DUST filtering on query sequence(s)
extern const string kDfltArgDustFiltering;

/// Argument to specify a filtering database (i.e.: one containing repetitive
/// elements)
extern const string kArgFilteringDb;

/// Argument to specify to mask query during lookup table creation
extern const string kArgLookupTableMaskingOnly;

/* PSI-BLAST options */

/// Argument to select the number of iterations to perform in PSI-BLAST
extern const string kArgPSINumIterations;

/// Argument to specify a 'checkpoint' file to recover the PSSM from
extern const string kArgPSIInputChkPntFile;
/// Argument to specify a 'checkpoint' file to write the PSSM
extern const string kArgPSIOutputChkPntFile;

/* OLD C TOOLKIT ARGUMENTS */

//#define ARG_QUERY "i"
//#define ARG_EVALUE "e"
//#define ARG_FORMAT "m"
//#define ARG_OUT "o"
//#define ARG_FILTER "F"
//#define ARG_GAPOPEN "G"
//#define ARG_GAPEXT "E"
//#define ARG_XDROP "X"
//#define ARG_SHOWGI "I"
//#define ARG_DESCRIPTIONS "v"
//#define ARG_ALIGNMENTS "b"
//#define ARG_THREADS "a"
//#define ARG_ASNOUT "O"
//#define ARG_BELIEVEQUERY "J"
//#define ARG_WORDSIZE "W"
//#define ARG_CULLING_LIMIT "K"
//#define ARG_SEARCHSP "Y"
//#define ARG_HTML "T"
//#define ARG_LCASE "U"
//#define ARG_XDROP_UNGAPPED "y"
//#define ARG_XDROP_FINAL "Z"
//#define ARG_QUERYLOC "L"
//#define ARG_WINDOW "A"

/* PSI-BLAST arguments */
#define ARG_PSEUDOCOUNT "c"
#define ARG_INCLUSION_THRESHOLD "h"
//#define ARG_NUM_ITERATIONS "j"

//#define ARG_CHECKPOINT "C"
#define ARG_ASCII_MATRIX "Q"
#define ARG_MSA_RESTART "B"
//#define ARG_GAP_TRIGGER "N"

/* OLD blastall arguments */

//#define ARG_PROGRAM "p"
//#define ARG_DB "d"
//#define ARG_MISMATCH "q"
//#define ARG_MATCH "r"
//#define ARG_THRESHOLD "f"
//#define ARG_QGENETIC_CODE "query_gencode"
//#define ARG_DBGENETIC_CODE "db_gencode"
//#define ARG_QGENETIC_CODE "Q"
//#define ARG_DBGENETIC_CODE "D"
//#define ARG_MATRIX "M"
//#define ARG_DBSIZE "z"
//#define ARG_STRAND "S"
//#define ARG_PSI_CHKPNT "R"
//#define ARG_MEGABLAST "n"
//#define ARG_FRAMESHIFT "w"
//#define ARG_INTRON "t"
//#define ARG_COMP_BASED_STATS "comp_based_stats"
//#define ARG_COMP_BASED_STATS "C"
//#define ARG_SMITH_WATERMAN "s"
//#define ARG_GAPPED "g"

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_BLASTINPUT__CMDLINE_FLAGS__HPP */

