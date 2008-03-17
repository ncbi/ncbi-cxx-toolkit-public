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
#include <algo/blast/core/blast_export.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Query sequence(s)
NCBI_XBLAST_EXPORT extern const string kArgQuery;
/// Default value for query sequence input
NCBI_XBLAST_EXPORT extern const string kDfltArgQuery;

/// Output file name
NCBI_XBLAST_EXPORT extern const string kArgOutput;

/// BLAST database name
NCBI_XBLAST_EXPORT extern const string kArgDb;

/// Effective length of BLAST database
NCBI_XBLAST_EXPORT extern const string kArgDbSize;

/// Subject input file to search
NCBI_XBLAST_EXPORT extern const string kArgSubject;

/// BLAST database molecule type
NCBI_XBLAST_EXPORT extern const string kArgDbType;

/// gi list file name to restrict BLAST database
NCBI_XBLAST_EXPORT extern const string kArgGiList;
/// argument for gi list to exclude from a BLAST database search
NCBI_XBLAST_EXPORT extern const string kArgNegativeGiList;

/// Task to perform
NCBI_XBLAST_EXPORT extern const string kTask;

/// Query genetic code
NCBI_XBLAST_EXPORT extern const string kArgQueryGeneticCode;
/// Database genetic code
NCBI_XBLAST_EXPORT extern const string kArgDbGeneticCode;

/// Argument to determine whether searches should be run locally or remotely
NCBI_XBLAST_EXPORT extern const string kArgRemote;

/// Argument to determine the number of threads to use when running BLAST
NCBI_XBLAST_EXPORT extern const string kArgNumThreads;

/// Argument for scoring matrix
NCBI_XBLAST_EXPORT extern const string kArgMatrixName;

/// Argument for expectation value cutoff
NCBI_XBLAST_EXPORT extern const string kArgEvalue;
/// Argument for minimum raw gapped score for preliminary gapped and traceback
/// stages
NCBI_XBLAST_EXPORT extern const string kArgMinRawGappedScore;

/// Argument for minimum raw score to keep an alignment in the preliminary
/// gapped and traceback stages
NCBI_XBLAST_EXPORT extern const string kMinRawGappedScore;

/* Formatting options */

/// Argument to select formatted output type
NCBI_XBLAST_EXPORT extern const string kArgOutputFormat;
/// Default value for formatted output type
NCBI_XBLAST_EXPORT extern const int kDfltArgOutputFormat;
/// Argument to specify whether the GIs should be shown in the deflines in the
/// traditional BLAST report
NCBI_XBLAST_EXPORT extern const string kArgShowGIs;
/// Argument to specify the number of one-line descriptions to show in the
/// traditional BLAST report
NCBI_XBLAST_EXPORT extern const string kArgNumDescriptions;
/// Default number of one-line descriptions to display in the traditional
/// BLAST report
NCBI_XBLAST_EXPORT extern const size_t kDfltArgNumDescriptions;
/// Argument to specify the number of alignments to show in the traditional 
/// BLAST report
NCBI_XBLAST_EXPORT extern const string kArgNumAlignments;
/// Default number of alignments to display in the traditional BLAST report
NCBI_XBLAST_EXPORT extern const size_t kDfltArgNumAlignments;
/// Argument to specify whether to create output as HTML or not
NCBI_XBLAST_EXPORT extern const string kArgProduceHtml;


/// Argument to select the gap opening penalty
NCBI_XBLAST_EXPORT extern const string kArgGapOpen;
/// Argument to select the gap extending penalty
NCBI_XBLAST_EXPORT extern const string kArgGapExtend;

/// Argument to select the nucleotide mismatch penalty
NCBI_XBLAST_EXPORT extern const string kArgMismatch;
/// Argument to select the nucleotide match reward
NCBI_XBLAST_EXPORT extern const string kArgMatch;

/// Argument to select the ungapped X dropoff value
NCBI_XBLAST_EXPORT extern const string kArgUngappedXDropoff;
/// Argument to select the gapped X dropoff value
NCBI_XBLAST_EXPORT extern const string kArgGappedXDropoff;
/// Argument to select the final gapped X dropoff value
NCBI_XBLAST_EXPORT extern const string kArgFinalGappedXDropoff;

/// Argument to select the window size in the 2-hit wordfinder algorithm
NCBI_XBLAST_EXPORT extern const string kArgWindowSize;

/// Argument to select the wordfinder's word size
NCBI_XBLAST_EXPORT extern const string kArgWordSize;

/// Argument to specify the minimum word score such that the word is added to
/// the lookup table
NCBI_XBLAST_EXPORT extern const string kArgWordScoreThreshold;

/// Argument to specify the effective length of the search space
NCBI_XBLAST_EXPORT extern const string kArgEffSearchSpace;

/// Argument to specify that Smith-Waterman algorithm should be used to compute
/// locally optimal alignments
NCBI_XBLAST_EXPORT extern const string kArgUseSWTraceback;

/// Argument to specify whether lowercase masking in the query sequence(s)
/// should be interpreted as masking
NCBI_XBLAST_EXPORT extern const string kArgUseLCaseMasking;
/// Argument to select the query strand(s) to search
NCBI_XBLAST_EXPORT extern const string kArgStrand;
/// Default value for strand selection
NCBI_XBLAST_EXPORT extern const string kDfltArgStrand;
/// Argument to specify a location to restrict the query sequence(s)
NCBI_XBLAST_EXPORT extern const string kArgQueryLocation;
/// Argument to specify a location to restrict the subject sequence(s)
NCBI_XBLAST_EXPORT extern const string kArgSubjectLocation;
/// Argument to specify if the query sequence(s) defline should be parsed
NCBI_XBLAST_EXPORT extern const string kArgParseQueryDefline;

/// Argument to specify the maximum length of an intron when linking multiple
/// distinct alignments (applicable to translated queries only)
NCBI_XBLAST_EXPORT extern const string kArgMaxIntronLength;
/// Default value for maximum intron length
NCBI_XBLAST_EXPORT extern const int kDfltArgMaxIntronLength;

/// Argument to specify the culling limit
NCBI_XBLAST_EXPORT extern const string kArgCullingLimit;
/// Default argument to specify the culling limit
NCBI_XBLAST_EXPORT extern const int kDfltArgCullingLimit;

/// Argument to specify the frame shift penality
NCBI_XBLAST_EXPORT extern const string kArgFrameShiftPenalty;

/// Argument to specify number of bits to initiate gapping
NCBI_XBLAST_EXPORT extern const string kArgGapTrigger;

/// Argument to specify whether the search should be ungapped only
NCBI_XBLAST_EXPORT extern const string kArgUngapped;

/// Argument to specify the composition based statistics mode to sue
NCBI_XBLAST_EXPORT extern const string kArgCompBasedStats;

/// Default argument to specify no filtering
NCBI_XBLAST_EXPORT extern const string kDfltArgNoFiltering;
/// Default argument to specify filtering
NCBI_XBLAST_EXPORT extern const string kDfltArgApplyFiltering;

/// Argument to specify SEG filtering on query sequence(s)
NCBI_XBLAST_EXPORT extern const string kArgSegFiltering;
/// Default arguments to apply SEG filtering on query sequence(s)
NCBI_XBLAST_EXPORT extern const string kDfltArgSegFiltering;

/// Argument to specify DUST filtering on query sequence(s)
NCBI_XBLAST_EXPORT extern const string kArgDustFiltering;
/// Default arguments to apply DUST filtering on query sequence(s)
NCBI_XBLAST_EXPORT extern const string kDfltArgDustFiltering;

/// Argument to specify a filtering database (i.e.: one containing repetitive
/// elements)
NCBI_XBLAST_EXPORT extern const string kArgFilteringDb;

/// Argument to specify to mask query during lookup table creation
NCBI_XBLAST_EXPORT extern const string kArgLookupTableMaskingOnly;

/* PSI-BLAST options */

/// Argument to select the number of iterations to perform in PSI-BLAST
NCBI_XBLAST_EXPORT extern const string kArgPSINumIterations;

/// Argument to specify a 'checkpoint' file to recover the PSSM from
NCBI_XBLAST_EXPORT extern const string kArgPSIInputChkPntFile;
/// Argument to specify a multiple sequence alignment file to create a PSSM from
NCBI_XBLAST_EXPORT extern const string kArgMSAInputFile;
/// Argument to specify a 'checkpoint' file to write the PSSM
NCBI_XBLAST_EXPORT extern const string kArgPSIOutputChkPntFile;
/// Argument to specify the file name for saving the ASCII representation of
/// the PSSM
NCBI_XBLAST_EXPORT extern const string kArgAsciiPssmOutputFile;
/// Argument to specify a PHI-BLAST pattern file
NCBI_XBLAST_EXPORT extern const string kArgPHIPatternFile;

/// Argument to specify the pseudo-count value used when constructing PSSM
NCBI_XBLAST_EXPORT extern const string kArgPSIPseudocount;
/// Argument to specify the evalue inclusion threshold for considering
/// aligned sequences for PSSM constructions
NCBI_XBLAST_EXPORT extern const string kArgPSIInclusionEThreshold;

/// Argument to specify non-greedy dynamic programming extension
NCBI_XBLAST_EXPORT extern const string kArgNoGreedyExtension;
/// Argument to specify the discontinuous megablast template type
NCBI_XBLAST_EXPORT extern const string kArgDMBTemplateType;
/// Argument to specify the discontinuous megablast template length
NCBI_XBLAST_EXPORT extern const string kArgDMBTemplateLength;

#if 0
/// Argument to specify the maximum number of HPSs to save per subject
NCBI_XBLAST_EXPORT extern const string kArgMaxHSPsPerSubject;
/// Default value for specifying the maximum number of HPSs to save per subject
NCBI_XBLAST_EXPORT extern const int kDfltArgMaxHSPsPerSubject;
#endif

/// Argument to specify the target percent identity
NCBI_XBLAST_EXPORT extern const string kArgPercentIdentity;
/// Argument to specify the search strategy file to read and use for a BLAST
/// search
NCBI_XBLAST_EXPORT extern const string kArgInputSearchStrategy;
/// Argument to specify the file name to save the search strategy used for a 
/// BLAST search
NCBI_XBLAST_EXPORT extern const string kArgOutputSearchStrategy;
/// Flag to force using or not using megablast database index.
NCBI_XBLAST_EXPORT extern const string kArgUseIndex;
/// Megablast database index name.
NCBI_XBLAST_EXPORT extern const string kArgIndexName;

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_BLASTINPUT__CMDLINE_FLAGS__HPP */

