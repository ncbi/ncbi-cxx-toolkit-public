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
 * Authors:  Greg Boratyn
 *
 */

/// @file blast_mapper_options.cpp
/// Implements the CMagicBlastOptionsHandle class.

#include <ncbi_pch.hpp>
//#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/api/magicblast_options.hpp>
//#include <objects/seqloc/Na_strand.hpp>
//#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CMagicBlastOptionsHandle::CMagicBlastOptionsHandle(EAPILocality locality)
    : CBlastOptionsHandle(locality)
{
    SetDefaults();
}


CMagicBlastOptionsHandle::CMagicBlastOptionsHandle(CRef<CBlastOptions> opt)
    : CBlastOptionsHandle(opt)
{
}


void
CMagicBlastOptionsHandle::SetDefaults()
{
    m_Opts->SetDefaultsMode(true);
    SetRNAToGenomeDefaults();
    m_Opts->SetDefaultsMode(false);
}

void
CMagicBlastOptionsHandle::SetRNAToGenomeDefaults()
{
    m_Opts->SetDefaultsMode(true);
    m_Opts->SetProgram(eMapper);
    SetLookupTableDefaults();
    SetQueryOptionDefaults();
    SetInitialWordOptionsDefaults();
    SetGappedExtensionDefaults();
    SetScoringOptionsDefaults();
    SetHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
    SetSubjectSequenceOptionsDefaults();
    m_Opts->SetDefaultsMode(false);
}

void
CMagicBlastOptionsHandle::SetRNAToRNADefaults()
{
    m_Opts->SetDefaultsMode(true);
    m_Opts->SetProgram(eMapper);
    SetLookupTableDefaults();
    SetQueryOptionDefaults();
    SetInitialWordOptionsDefaults();
    SetGappedExtensionDefaults();
    SetScoringOptionsDefaults();
    SetHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
    SetSubjectSequenceOptionsDefaults();

    SetMismatchPenalty(-4);
    SetGapExtensionCost(4);
    SetLookupDbFilter(false);
    SetSpliceAlignments(false);
    SetWordSize(30);

    m_Opts->SetDefaultsMode(false);
}


void
CMagicBlastOptionsHandle::SetGenomeToGenomeDefaults()
{
    m_Opts->SetDefaultsMode(true);
    m_Opts->SetProgram(eMapper);
    SetLookupTableDefaults();
    SetQueryOptionDefaults();
    SetInitialWordOptionsDefaults();
    SetGappedExtensionDefaults();
    SetScoringOptionsDefaults();
    SetHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
    SetSubjectSequenceOptionsDefaults();

    SetMismatchPenalty(-4);
    SetGapExtensionCost(4);
    SetLookupDbFilter(true);
    SetSpliceAlignments(false);
    SetWordSize(28);

    m_Opts->SetDefaultsMode(false);
}

void 
CMagicBlastOptionsHandle::SetLookupTableDefaults()
{
    if (getenv("MAPPER_MB_LOOKUP")) {
        m_Opts->SetLookupTableType(eMBLookupTable);
    }
    else {
        m_Opts->SetLookupTableType(eNaHashLookupTable);
    }
    SetWordSize(BLAST_WORDSIZE_MAPPER);
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_BLASTN);
    SetMaxDbWordCount(MAX_DB_WORD_COUNT_MAPPER);
    SetLookupTableStride(0);
}


void
CMagicBlastOptionsHandle::SetQueryOptionDefaults()
{
    SetReadQualityFiltering(true);
    m_Opts->SetDustFiltering(false);
    // Masking is used for filtering out poor quality reads which are masked
    // for the full length. Setting mask at hash to true prevents additional
    // memory allocation for unmasked queries.
    m_Opts->SetMaskAtHash(true);
    m_Opts->SetStrandOption(objects::eNa_strand_both);
    SetLookupDbFilter(true);
    SetPaired(false);
}

void
CMagicBlastOptionsHandle::SetInitialWordOptionsDefaults()
{
}

void
CMagicBlastOptionsHandle::SetGappedExtensionDefaults()
{
    m_Opts->SetGapExtnAlgorithm(eJumperWithTraceback);
    m_Opts->SetMaxMismatches(5);
    m_Opts->SetMismatchWindow(10);
    SetSpliceAlignments(true);
    // 0 means that the value will be set to max(-penalty, gap open +
    // gap extend)
    SetGapXDropoff(0);
}


void
CMagicBlastOptionsHandle::SetScoringOptionsDefaults()
{
    m_Opts->SetMatrixName(NULL);
    SetGapOpeningCost(BLAST_GAP_OPEN_MAPPER);
    SetGapExtensionCost(BLAST_GAP_EXTN_MAPPER);
    m_Opts->SetMatchReward(BLAST_REWARD_MAPPER);
    SetMismatchPenalty(BLAST_PENALTY_MAPPER);
    m_Opts->SetGappedMode();
    m_Opts->SetComplexityAdjMode(false);

    // set out-of-frame options to invalid? values
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
}

void
CMagicBlastOptionsHandle::SetHitSavingOptionsDefaults()
{
    m_Opts->SetHitlistSize(500);
    m_Opts->SetEvalueThreshold(BLAST_EXPECT_VALUE);
    m_Opts->SetPercentIdentity(0);
    // set some default here, allow INT4MAX to mean infinity
    m_Opts->SetMaxNumHspPerSequence(0); 
    m_Opts->SetMaxHspsPerSubject(0);
    // cutoff zero means use adaptive score threshold that depends on query
    // length
    SetCutoffScore(0);
    vector<double> coeffs = {0.0, 0.0};
    SetCutoffScoreCoeffs(coeffs);
    SetMaxEditDistance(INT4_MAX);
    SetLongestIntronLength(500000);

    // do not compute each query's ungapped alignment score threshold to
    // trigger gapped alignment
    m_Opts->SetLowScorePerc(0.0);
    m_Opts->SetQueryCovHspPerc(0);
}

void
CMagicBlastOptionsHandle::SetEffectiveLengthsOptionsDefaults()
{
    m_Opts->SetDbLength(0);
    m_Opts->SetDbSeqNum(0);
    m_Opts->SetEffectiveSearchSpace(0);
}

void
CMagicBlastOptionsHandle::SetSubjectSequenceOptionsDefaults()
{}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
