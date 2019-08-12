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
#include <objtools/align_format/format_flags.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Query sequence(s)
NCBI_BLASTINPUT_EXPORT extern const string kArgQuery;
/// Default value for query sequence input
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgQuery;

/// Output file name
NCBI_BLASTINPUT_EXPORT extern const string kArgOutput;

/// BLAST database name
NCBI_BLASTINPUT_EXPORT extern const string kArgDb;

/// Effective length of BLAST database
NCBI_BLASTINPUT_EXPORT extern const string kArgDbSize;

/// Subject input file to search
NCBI_BLASTINPUT_EXPORT extern const string kArgSubject;

/// BLAST database molecule type
NCBI_BLASTINPUT_EXPORT extern const string kArgDbType;

/// gi list file name to restrict BLAST database
NCBI_BLASTINPUT_EXPORT extern const string kArgGiList;

/// Title for the BLAST database
NCBI_BLASTINPUT_EXPORT extern const string kArgDbTitle;

/// seqid list file name to restrict BLAST database
NCBI_BLASTINPUT_EXPORT extern const string kArgSeqIdList;

/// argument for seqid list to exclude from a BLAST database search
NCBI_BLASTINPUT_EXPORT extern const string kArgNegativeGiList;

/// argument for gi list to exclude from a BLAST database search
NCBI_BLASTINPUT_EXPORT extern const string kArgNegativeSeqidList;

/// IPG list file name to restrict BLAST database
NCBI_BLASTINPUT_EXPORT extern const string kArgIpgList;

/// argument for IPG list to exclude from a BLAST database search
NCBI_BLASTINPUT_EXPORT extern const string kArgNegativeIpgList;

/// List of filtering algorithms to apply to subjects as soft masking
extern const string kArgDbSoftMask;
// List of filtering algorithms to apply to subjects as hard masking
extern const string kArgDbHardMask;

/// Task to perform
NCBI_BLASTINPUT_EXPORT extern const string kTask;

/// Query genetic code
NCBI_BLASTINPUT_EXPORT extern const string kArgQueryGeneticCode;
/// Database genetic code
NCBI_BLASTINPUT_EXPORT extern const string kArgDbGeneticCode;

/// Argument to determine whether searches should be run locally or remotely
NCBI_BLASTINPUT_EXPORT extern const string kArgRemote;

/// Argument to determine the number of threads to use when running BLAST
NCBI_BLASTINPUT_EXPORT extern const string kArgNumThreads;
/// Default number of threads for IgBLAST command line tools
NCBI_BLASTINPUT_EXPORT extern const size_t kDfltIgBlastNumThreads;

/// Argument for scoring matrix
NCBI_BLASTINPUT_EXPORT extern const string kArgMatrixName;

// Argurment for mask_level for RMBlastN -RMH-
NCBI_BLASTINPUT_EXPORT extern const string kArgMaskLevel;
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgMaskLevel;

// Argurment for complexity adjustment mode of RMBlastN -RMH-
NCBI_BLASTINPUT_EXPORT extern const string kArgComplexityAdj;

/// Argument for expectation value cutoff
NCBI_BLASTINPUT_EXPORT extern const string kArgEvalue;
/// Argument for minimum raw gapped score for preliminary gapped and traceback
/// stages
NCBI_BLASTINPUT_EXPORT extern const string kArgMinRawGappedScore;

/// Argument to specify the maximum number of target sequences to keep (a.k.a.:
/// hitlist size)
/// If not set in the command line, this value is the maximum of the number of
/// alignments/descriptions to show in the traditional BLAST report
NCBI_BLASTINPUT_EXPORT extern const string kArgMaxTargetSequences;
/// Default maximum number of target sequences, to be used only on the web
NCBI_BLASTINPUT_EXPORT extern const TSeqPos kDfltArgMaxTargetSequences;


/// Argument to select the gap opening penalty
NCBI_BLASTINPUT_EXPORT extern const string kArgGapOpen;
/// Argument to select the gap extending penalty
NCBI_BLASTINPUT_EXPORT extern const string kArgGapExtend;

/// Argument to select the nucleotide mismatch penalty
NCBI_BLASTINPUT_EXPORT extern const string kArgMismatch;
/// Argument to select the nucleotide match reward
NCBI_BLASTINPUT_EXPORT extern const string kArgMatch;

/// Argument to select the ungapped X dropoff value
NCBI_BLASTINPUT_EXPORT extern const string kArgUngappedXDropoff;
/// Argument to select the gapped X dropoff value
NCBI_BLASTINPUT_EXPORT extern const string kArgGappedXDropoff;
/// Argument to select the final gapped X dropoff value
NCBI_BLASTINPUT_EXPORT extern const string kArgFinalGappedXDropoff;

/// Argument to select the window size in the 2-hit wordfinder algorithm
NCBI_BLASTINPUT_EXPORT extern const string kArgWindowSize;

/// Argument to select the off-diagonal scan range in the 2-hit wordfinder algorithm
NCBI_BLASTINPUT_EXPORT extern const string kArgOffDiagonalRange;
NCBI_BLASTINPUT_EXPORT extern const int kDfltOffDiagonalRange;

/// Argument to select the wordfinder's word size
NCBI_BLASTINPUT_EXPORT extern const string kArgWordSize;

/// Argument to specify the minimum word score such that the word is added to
/// the lookup table
NCBI_BLASTINPUT_EXPORT extern const string kArgWordScoreThreshold;

/// Argument to specify the effective length of the search space
NCBI_BLASTINPUT_EXPORT extern const string kArgEffSearchSpace;

/// Argument to specify that Smith-Waterman algorithm should be used to compute
/// locally optimal alignments
NCBI_BLASTINPUT_EXPORT extern const string kArgUseSWTraceback;

/// Argument to specify whether lowercase masking in the query sequence(s)
/// should be interpreted as masking
NCBI_BLASTINPUT_EXPORT extern const string kArgUseLCaseMasking;
/// Default argument to specify whether lowercase masking should be used
NCBI_BLASTINPUT_EXPORT extern const bool kDfltArgUseLCaseMasking;
/// Argument to select the query strand(s) to search
NCBI_BLASTINPUT_EXPORT extern const string kArgStrand;
/// Default value for strand selection
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgStrand;
/// Argument to specify a location to restrict the query sequence(s)
NCBI_BLASTINPUT_EXPORT extern const string kArgQueryLocation;
/// Argument to specify a location to restrict the subject sequence(s)
NCBI_BLASTINPUT_EXPORT extern const string kArgSubjectLocation;
/// Argument to specify if the query and subject sequences defline should be
/// parsed
NCBI_BLASTINPUT_EXPORT extern const string kArgParseDeflines;
/// Default argument to specify whether sequences deflines should be parsed
NCBI_BLASTINPUT_EXPORT extern const bool kDfltArgParseDeflines;

/// Argument to specify the maximum length of an intron when linking multiple
/// distinct alignments (applicable to translated queries only)
NCBI_BLASTINPUT_EXPORT extern const string kArgMaxIntronLength;
/// Default value for maximum intron length
NCBI_BLASTINPUT_EXPORT extern const int kDfltArgMaxIntronLength;

/// Argument to specify the culling limit
NCBI_BLASTINPUT_EXPORT extern const string kArgCullingLimit;
/// Default argument to specify the culling limit
NCBI_BLASTINPUT_EXPORT extern const int kDfltArgCullingLimit;

/// Argument to specify the culling limit
NCBI_BLASTINPUT_EXPORT extern const string kArgSubjectBestHit;

/// Argument to specify the overhang parameter to the best hit algorithm
NCBI_BLASTINPUT_EXPORT extern const string kArgBestHitOverhang;
/// Default argument for the overhang parameter to the best hit algorithm
NCBI_BLASTINPUT_EXPORT extern const double kDfltArgBestHitOverhang;
/// Argument to specify the score edge parameter to the best hit algorithm
NCBI_BLASTINPUT_EXPORT extern const string kArgBestHitScoreEdge;
/// Default argument for the score edge parameter to the best hit algorithm
NCBI_BLASTINPUT_EXPORT extern const double kDfltArgBestHitScoreEdge;

/// Argument to specify the frame shift penality
NCBI_BLASTINPUT_EXPORT extern const string kArgFrameShiftPenalty;

/// Argument to specify number of bits to initiate gapping
NCBI_BLASTINPUT_EXPORT extern const string kArgGapTrigger;

/// Argument to specify whether the search should be ungapped only
NCBI_BLASTINPUT_EXPORT extern const string kArgUngapped;

/// Argument to specify the composition based statistics mode to sue
NCBI_BLASTINPUT_EXPORT extern const string kArgCompBasedStats;
/// Default argument for composition based statistics
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgCompBasedStats;
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgCompBasedStatsDelta;
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgCompBasedStatsRPS;

/// Default argument to specify no filtering
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgNoFiltering;
/// Default argument to specify filtering
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgApplyFiltering;

/// Argument to specify SEG filtering on query sequence(s)
NCBI_BLASTINPUT_EXPORT extern const string kArgSegFiltering;
/// Default arguments to apply SEG filtering on query sequence(s)
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgSegFiltering;

/// Argument to specify DUST filtering on query sequence(s)
NCBI_BLASTINPUT_EXPORT extern const string kArgDustFiltering;
/// Default arguments to apply DUST filtering on query sequence(s)
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgDustFiltering;

/// Argument to specify a filtering database (i.e.: one containing repetitive
/// elements)
NCBI_BLASTINPUT_EXPORT extern const string kArgFilteringDb;

/// Argument to specify a taxid for Window Masker.
NCBI_BLASTINPUT_EXPORT extern const string kArgWindowMaskerTaxId;

/// Argument to specify a path to a Window Masker database.
NCBI_BLASTINPUT_EXPORT extern const string kArgWindowMaskerDatabase;

/// Argument to specify to mask query during lookup table creation
NCBI_BLASTINPUT_EXPORT extern const string kArgLookupTableMaskingOnly;
/// Default argument mask a protein query during lookup table construction
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgLookupTableMaskingOnlyProt;
/// Default argument mask a nucleotide query during lookup table construction
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgLookupTableMaskingOnlyNucl;

/* PSI-BLAST options */

/// Argument to select the number of iterations to perform in PSI-BLAST
NCBI_BLASTINPUT_EXPORT extern const string kArgPSINumIterations;
NCBI_BLASTINPUT_EXPORT extern const unsigned int kDfltArgPSINumIterations;

/// Argument to specify a 'checkpoint' file to recover the PSSM from
NCBI_BLASTINPUT_EXPORT extern const string kArgPSIInputChkPntFile;
/// Argument to specify a multiple sequence alignment file to create a PSSM from
NCBI_BLASTINPUT_EXPORT extern const string kArgMSAInputFile;
/// Argument to specify the index (1-based) of the sequence in the multiple
/// sequence alignment to use as a master sequence
NCBI_BLASTINPUT_EXPORT extern const string kArgMSAMasterIndex;
/// Argument to specify whether the template sequence (usually the query)
/// should be ignored for the purposes of PSSM computation
NCBI_BLASTINPUT_EXPORT extern const string kArgIgnoreMsaMaster;
/// Argument to specify a 'checkpoint' file to write the PSSM
NCBI_BLASTINPUT_EXPORT extern const string kArgPSIOutputChkPntFile;
/// Argument to specify the file name for saving the ASCII representation of
/// the PSSM
NCBI_BLASTINPUT_EXPORT extern const string kArgAsciiPssmOutputFile;
/// Argument to specify a PHI-BLAST pattern file
NCBI_BLASTINPUT_EXPORT extern const string kArgPHIPatternFile;

/// Argument to specify the pseudo-count value used when constructing PSSM
NCBI_BLASTINPUT_EXPORT extern const string kArgPSIPseudocount;
/// Argument to specify the evalue inclusion threshold for considering
/// aligned sequences for PSSM constructions
NCBI_BLASTINPUT_EXPORT extern const string kArgPSIInclusionEThreshold;
/// Argument to specify whether the PSSM after the last psiblast database
/// search should be saved
NCBI_BLASTINPUT_EXPORT extern const string kArgSaveLastPssm;
/// Argument to specify whether to save PSSM after each psiblast iteration
NCBI_BLASTINPUT_EXPORT extern const string kArgSaveAllPssms;

/// Argument to specify the germline subject file for igblast
NCBI_BLASTINPUT_EXPORT extern const string kArgGLSubject;
/// Argument to specify the germline database name for igblast
NCBI_BLASTINPUT_EXPORT extern const string kArgGLDatabase;
/// Argument to specify the germline database chaintype name for igblast
NCBI_BLASTINPUT_EXPORT extern const string kArgGLChainType;
/// Argument to specify the number of alignments for germline database
NCBI_BLASTINPUT_EXPORT extern const string kArgGLNumAlign;
/// Argument to specify the germline origin for igblast
NCBI_BLASTINPUT_EXPORT extern const string kArgGLOrigin;
/// Argument to specify the Ig domain system
NCBI_BLASTINPUT_EXPORT extern const string kArgGLDomainSystem;
/// Arugment to specify if Igblast alignment should restrict to V seg
NCBI_BLASTINPUT_EXPORT extern const string kArgGLFocusV;
/// Arugment to specify if Igblast alignment should be extends at 5' end
NCBI_BLASTINPUT_EXPORT extern const string kArgExtendAlign;
/// Arugment to to detect overlap at vdj junction
NCBI_BLASTINPUT_EXPORT extern const string kArgDetectOverlap;
///Argument to specify minimal required V length
NCBI_BLASTINPUT_EXPORT extern const string kArgMinVLength;
///Argument to specify minimal required J gene length
NCBI_BLASTINPUT_EXPORT extern const string kArgMinJLength;
///Argument to specify number of clonotype to show
NCBI_BLASTINPUT_EXPORT extern const string kArgNumClonotype;
///Argument to specify number of clonotype file
NCBI_BLASTINPUT_EXPORT extern const string kArgClonotypeFile;
/// Arugment to specify if Igblast alignment should be translated to protein
NCBI_BLASTINPUT_EXPORT extern const string kArgTranslate;
///Arugment to specify if Igblast min D gene match
NCBI_BLASTINPUT_EXPORT extern const string kArgMinDMatch;
/// Argument to specify mismatch penalty for D gene search
NCBI_BLASTINPUT_EXPORT extern const string kArgDPenalty;
/// Argument to specify mismatch penalty for J gene search
NCBI_BLASTINPUT_EXPORT extern const string kArgJPenalty;
/// Argument to specify IgBlast sequence type
NCBI_BLASTINPUT_EXPORT extern const string kArgIgSeqType;
/// Argument to specify non-greedy dynamic programming extension
NCBI_BLASTINPUT_EXPORT extern const string kArgNoGreedyExtension;
/// Argument to specify the discontinuous megablast template type
NCBI_BLASTINPUT_EXPORT extern const string kArgDMBTemplateType;
/// Argument to specify the discontinuous megablast template length
NCBI_BLASTINPUT_EXPORT extern const string kArgDMBTemplateLength;
/// Argument to specify if non-concordant pairs should be displayed
NCBI_BLASTINPUT_EXPORT extern const string kArgNoDiscordant;
/// Argument to specify forward/reverse strand specificity
NCBI_BLASTINPUT_EXPORT extern const string kArgFwdRev;
/// Argument to specify reverse/forward strand specificity
NCBI_BLASTINPUT_EXPORT extern const string kArgRevFwd;
/// Argument to specify forward-only strand specificity
NCBI_BLASTINPUT_EXPORT extern const string kArgFwdOnly;
/// Argument to specify reverse-only strand specificity
NCBI_BLASTINPUT_EXPORT extern const string kArgRevOnly;
/// Argument to specify only strand specific results
NCBI_BLASTINPUT_EXPORT extern const string kArgOnlyStrandSpecific;

/// Argument to specify the maximum number of HPSs to save per subject for each query
NCBI_BLASTINPUT_EXPORT extern const string kArgMaxHSPsPerSubject;

/// Argument to turn on sum statistics
NCBI_BLASTINPUT_EXPORT extern const string kArgSumStats;
/// Argument to specify the target percent identity
NCBI_BLASTINPUT_EXPORT extern const string kArgPercentIdentity;
/// Argument to specify the search strategy file to read and use for a BLAST
/// search
NCBI_BLASTINPUT_EXPORT extern const string kArgInputSearchStrategy;
/// Argument to specify the file name to save the search strategy used for a
/// BLAST search
NCBI_BLASTINPUT_EXPORT extern const string kArgOutputSearchStrategy;
/// Flag to force using or not using megablast database index.
NCBI_BLASTINPUT_EXPORT extern const string kArgUseIndex;
/// Default value for megablast database index flag.
NCBI_BLASTINPUT_EXPORT extern const bool kDfltArgUseIndex;
/// Megablast database index name.
NCBI_BLASTINPUT_EXPORT extern const string kArgIndexName;
/// Use old style megablast index.
NCBI_BLASTINPUT_EXPORT extern const string kArgOldStyleIndex;
/// Default value for use old style megablast index.
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgOldStyleIndex;

/// Entrez query.
NCBI_BLASTINPUT_EXPORT extern const string kArgEntrezQuery;

// DELTA-BLAST argumnets
/// Argument to specify domain database name for DELTA-BLAST
NCBI_BLASTINPUT_EXPORT extern const string kArgRpsDb;
/// Default value for domain database name
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgRpsDb;

/// KBLASTP arguments
/// Specifies Jaccard distance (threshold)
NCBI_BLASTINPUT_EXPORT extern const string kArgJDistance;
/// Jaccard default value
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgJDistance;
/// Specifies minimal number of LSH matches
NCBI_BLASTINPUT_EXPORT extern const string kArgMinHits;
/// LSH matches default value.
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgMinHits;
/// Number of sequences to attempt BLAST on.
NCBI_BLASTINPUT_EXPORT extern const string kArgCandidateSeqs;
NCBI_BLASTINPUT_EXPORT extern const string kDfltArgCandidateSeqs;


/// Argument to specify inclusion e-value threshold for conserved domains
NCBI_BLASTINPUT_EXPORT extern const string kArgDomainInclusionEThreshold;
/// Argument to specify whether show domain hits in DELTA-BLAST
NCBI_BLASTINPUT_EXPORT extern const string kArgShowDomainHits;

/// Argument to blast_formatter to request RID
NCBI_BLASTINPUT_EXPORT extern const string kArgRid;
/// Argument to blast_formatter to request BLAST archive file name
NCBI_BLASTINPUT_EXPORT extern const string kArgArchive;

/// Argument to specify min query coverage percentage for each hsp
NCBI_BLASTINPUT_EXPORT extern const string kArgQueryCovHspPerc;

/// Argument to specify line length for displaying alignments
NCBI_BLASTINPUT_EXPORT extern const string kArgLineLength;

/// Argument to specify taxonomy ids for filtering
NCBI_BLASTINPUT_EXPORT extern const string kArgTaxIdList;
/// Argument to specify negative taxonomy ids filtering
NCBI_BLASTINPUT_EXPORT extern const string kArgNegativeTaxIdList;

/// Argument to specify file with taxonomy ids for filtering
NCBI_BLASTINPUT_EXPORT extern const string kArgTaxIdListFile;
/// Argument to specify file with taxonomy ids for Negative filtering
NCBI_BLASTINPUT_EXPORT extern const string kArgNegativeTaxIdListFile;


// Magic-BLAST arguments
/// Argument to specify whether mapped reads are paired
NCBI_BLASTINPUT_EXPORT extern const string kArgPaired;
/// Argument to specify cutoff score for accepting a spliced alignment
NCBI_BLASTINPUT_EXPORT extern const string kArgScore;
/// Argument to specify filtering lookup tables words by frequency in the
/// searched database
NCBI_BLASTINPUT_EXPORT extern const string kArgLimitLookup;
/// Argument to specify whether to search for spliced alignments
NCBI_BLASTINPUT_EXPORT extern const string kArgSplice;
/// Argument to sepcify the stride when creating a lookup table
NCBI_BLASTINPUT_EXPORT extern const string kArgLookupStride;
/// Argument to specify input format
NCBI_BLASTINPUT_EXPORT extern const string kArgInputFormat;
/// Argyment to specify whether quality filtering is to be done
NCBI_BLASTINPUT_EXPORT extern const string kArgQualityFilter;
/// Mates for the query sequences if given in a separate file
NCBI_BLASTINPUT_EXPORT extern const string kArgQueryMate;
/// Reference type: genome or transcriptome
NCBI_BLASTINPUT_EXPORT extern const string kArgRefType;
/// Argument to specify that the output will be compressed with gzip
NCBI_BLASTINPUT_EXPORT extern const string kArgOutputGzip;
/// Argument to specify SRA accessions
NCBI_BLASTINPUT_EXPORT extern const string kArgSraAccession;
/// Argument to specify not trimming of '.1' and '.2' at the end of read ids
/// in SAM format for paired reads
NCBI_BLASTINPUT_EXPORT extern const string kArgNoReadIdTrim;
/// Argument to trun off printing of unaligned reads
NCBI_BLASTINPUT_EXPORT extern const string kArgNoUnaligned;
/// Argument to enable SRA caching in local files
NCBI_BLASTINPUT_EXPORT extern const string kArgEnableSraCache;
/// Argument to specify a cutoff edit distance fot an alignment
NCBI_BLASTINPUT_EXPORT extern const string kArgMaxEditDist;
/// Argument to specify a maximum number of times a word can be repeated in a
/// database
NCBI_BLASTINPUT_EXPORT extern const string kArgMaxDbWordCount;
/// Argument to specify printing SAN MD tag
NCBI_BLASTINPUT_EXPORT extern const string kArgPrintMdTag;

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_BLASTINPUT__CMDLINE_FLAGS__HPP */

