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

/** @file cmdline_flags.cpp
 *  Constant definitions for command line arguments for BLAST programs
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/hspfilter_besthit.h>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(align_format);

const string kArgQuery("query");
const string kDfltArgQuery("-");
const string kArgOutput("out");

const string kArgDb("db");
const string kArgSubject("subject");

const string kArgDbSize("dbsize");

const string kArgDbType("dbtype");

const string kArgGiList("gilist");
const string kArgDbTitle("title");
const string kArgSeqIdList("seqidlist");
const string kArgNegativeGiList("negative_gilist");
const string kArgDbSoftMask("db_soft_mask");
const string kArgDbHardMask("db_hard_mask");

const string kTask("task");

const string kArgQueryGeneticCode("query_gencode");
const string kArgDbGeneticCode("db_gencode");

const string kArgRemote("remote");
const string kArgNumThreads("num_threads");

const string kArgMatrixName("matrix");

/* Turn on complexity adjustment for RMBlastN -RMH- */
const string kArgComplexityAdj("complexity_adjust");

/* Turn on masklevel parameter for RMBlastN -RMH- */
const string kArgMaskLevel("mask_level");
const string kDfltArgMaskLevel("-1");

const string kArgEvalue("evalue");
const string kArgMinRawGappedScore("min_raw_gapped_score");

const string kArgMaxTargetSequences("max_target_seqs");
const TSeqPos kDfltArgMaxTargetSequences = 100;

const string kArgGapOpen("gapopen");
const string kArgGapExtend("gapextend");

const string kArgMismatch("penalty");
const string kArgMatch("reward");

const string kArgUngappedXDropoff("xdrop_ungap");
const string kArgGappedXDropoff("xdrop_gap");
const string kArgFinalGappedXDropoff("xdrop_gap_final");

const string kArgWindowSize("window_size");
const string kArgOffDiagonalRange("off_diagonal_range");
const int kDfltOffDiagonalRange = 0;

const string kArgWordSize("word_size");

const string kArgWordScoreThreshold("threshold");

const string kArgEffSearchSpace("searchsp");

const string kArgUseSWTraceback("use_sw_tback");

const string kArgUseLCaseMasking("lcase_masking");
const bool kDfltArgUseLCaseMasking = false;
const string kArgStrand("strand");
const string kDfltArgStrand("both");
const string kArgQueryLocation("query_loc");
const string kArgSubjectLocation("subject_loc");
const string kArgParseDeflines("parse_deflines");
const bool kDfltArgParseDeflines = false;

const string kArgMaxIntronLength("max_intron_length");
const int kDfltArgMaxIntronLength = 0;

const string kArgCullingLimit("culling_limit");
const int kDfltArgCullingLimit = 0;

const string kArgBestHitOverhang("best_hit_overhang");
const double kDfltArgBestHitOverhang = kBestHit_OverhangDflt;
const string kArgBestHitScoreEdge("best_hit_score_edge");
const double kDfltArgBestHitScoreEdge = kBestHit_ScoreEdgeDflt;

const string kArgFrameShiftPenalty("frame_shift_penalty");

const string kArgGapTrigger("gap_trigger");

const string kArgUngapped("ungapped");

const string kArgCompBasedStats("comp_based_stats");
const string kDfltArgCompBasedStats("2");
const string kDfltArgCompBasedStatsDelta("1");
const string kDfltArgCompBasedStatsRPS("0");

const string kDfltArgNoFiltering("no");
const string kDfltArgApplyFiltering("yes");

const string kArgSegFiltering("seg");
const string kDfltArgSegFiltering =
    NStr::IntToString(kSegWindow) + string(" ") +
    NStr::DoubleToString(kSegLocut) + string(" ") +
    NStr::DoubleToString(kSegHicut);

const string kArgDustFiltering("dust");
const string kDfltArgDustFiltering =
    NStr::IntToString(kDustLevel) + string(" ") +
    NStr::DoubleToString(kDustWindow) + string(" ") +
    NStr::DoubleToString(kDustLinker);

const string kArgFilteringDb("filtering_db");
const string kArgWindowMaskerTaxId("window_masker_taxid");
const string kArgWindowMaskerDatabase("window_masker_db");
const string kArgLookupTableMaskingOnly("soft_masking");
const string kDfltArgLookupTableMaskingOnlyProt("false");
const string kDfltArgLookupTableMaskingOnlyNucl("true");

const string kArgPSINumIterations("num_iterations");
const string kArgPSIInputChkPntFile("in_pssm");
const string kArgPSIOutputChkPntFile("out_pssm");
const string kArgAsciiPssmOutputFile("out_ascii_pssm");
const string kArgMSAInputFile("in_msa");
const string kArgIgnoreMsaMaster("ignore_msa_master");
const string kArgMSAMasterIndex("msa_master_idx");
const string kArgPSIPseudocount("pseudocount");
const string kArgPSIInclusionEThreshold("inclusion_ethresh");
const string kArgPHIPatternFile("phi_pattern");

const string kArgGLSubject("germline_sequence_");
const string kArgGLDatabase("germline_db_");
const string kArgGLNumAlign("num_alignments_");
const string kArgGLChainType("auxiliary_data");
const string kArgGLOrigin("organism");
const string kArgGLDomainSystem("domain_system");
const string kArgGLFocusV("focus_on_V_segment");
const string kArgTranslate("show_translation");
const string kArgMinDMatch("min_D_match");
const string kArgDPenalty("D_penalty");
const string kArgIgSeqType("ig_seqtype");
const string kArgMaxHSPsPerSubject("max_hsps");
const int    kDfltArgMaxHSPsPerSubject = 0;
const string kArgSumStats("sum_statistics");
const string kArgPercentIdentity("perc_identity");
const string kArgNoGreedyExtension("no_greedy");
const string kArgDMBTemplateType("template_type");
const string kArgDMBTemplateLength("template_length");

const string kArgInputSearchStrategy("import_search_strategy");
const string kArgOutputSearchStrategy("export_search_strategy");

const string kArgUseIndex("use_index");
const string kArgIndexName("index_name");
const string kArgOldStyleIndex("old_style_index");
const string kDfltArgOldStyleIndex("true");
const bool kDfltArgUseIndex = false;

const string kArgEntrezQuery("entrez_query");

const string kArgRpsDb("rpsdb");
const string kDfltArgRpsDb("cdd_delta");
const string kArgDomainInclusionEThreshold("domain_inclusion_ethresh");
const string kArgShowDomainHits("show_domain_hits");

const string kArgRid("rid");
const string kArgArchive("archive");

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* SKIP_DOXYGEN_PROCESSING */
