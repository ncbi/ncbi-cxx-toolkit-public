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
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <algo/blast/core/blast_def.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

const string kArgQuery("query");
const string kArgOutput("out");

// FIXME: Do some kind of auto-detection here?
const string kArgDb("db");
const string kArgSubject("subject");

const string kArgDbSize("dbsize");

const string kArgDbType("dbtype");

const string kArgGiList("gilist");

const string kArgQueryGeneticCode("query_gencode");
const int kDfltArgQueryGeneticCode = 1;

const string kArgDbGeneticCode("db_gencode");
const int kDfltArgDbGeneticCode = 1;

const string kArgRemote("remote");
const string kArgNumThreads("num_threads");

const string kArgMatrixName("matrix");
const string kDfltArgMatrixName("BLOSUM62");

const string kArgEvalue("evalue");
const double kDfltArgEvalue = 10.0;

const string kArgOutputFormat("outfmt");
const int kDfltArgOutputFormat = 0;
const string kArgShowGIs("show_gis");
const string kArgNumDescriptions("num_descriptions");
const string kArgNumAlignments("num_alignments");

const string kArgGapOpen("gapopen");
const string kArgGapExtend("gapextend");

const string kArgMismatch("mismatch_penalty");
const int kDfltArgMismatch = -3;
const string kArgMatch("match_reward");
const int kDfltArgMatch = 1;

const string kArgUngappedXDropoff("xdrop_ungap");
const string kArgGappedXDropoff("xdrop_gap");
const string kArgFinalGappedXDropoff("xdrop_gap_final");

const string kArgWindowSize("window_size");

const string kArgWordSize("word_size");

const string kArgWordScoreThreshold("min_word_score");

const string kArgEffSearchSpace("searchsp");

const string kArgUseSWTraceback("use_sw_tback");

const string kArgUseLCaseMasking("lcase_masking");
const string kArgStrand("strand");
const string kDfltArgStrand("both");
const string kArgQueryLocation("query_loc");
const string kArgParseQueryDefline("parse_query_defline");

const string kArgMaxIntronLength("max_intron_length");
const int kDfltArgMaxIntronLength = 0;

const string kArgCullingLimit("culling_limit");
const int kDfltArgCullingLimit = 1;

const string kArgFrameShiftPenalty("frame_shift_penalty");

const string kArgGapTrigger("gap_trigger");
const double kDfltArgGapTrigger = 22.0;

const string kArgUngapped("ungapped");

const string kArgCompBasedStats("comp_based_stats");

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
const string kArgLookupTableMaskingOnly("soft_masking");

const string kArgPSINumIterations("num_iterations");
const string kArgPSIInputChkPntFile("in_pssm");
const string kArgPSIOutputChkPntFile("out_pssm");

END_SCOPE(blast)
END_NCBI_SCOPE
