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
 * Authors:  Tom Madden
 *
 */

/// @file blast_rps_options.cpp
/// Implements the CBlastRPSOptionsHandle class.

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_rps_options.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include "blast_setup.hpp"


/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastRPSOptionsHandle::CBlastRPSOptionsHandle(EAPILocality locality)
    : CBlastOptionsHandle(locality)
{
    SetDefaults();
    m_Opts->SetProgram(eRPSBlast);
}

CBlastRPSOptionsHandle::CBlastRPSOptionsHandle(CRef<CBlastOptions> opt)
    : CBlastOptionsHandle(opt)
{
}

void 
CBlastRPSOptionsHandle::SetLookupTableDefaults()
{
    m_Opts->SetLookupTableType(eRPSLookupTable);
    // N.B.: the word threshold is not set because for RPS-BLAST it CANNOT be
    // done, i.e.: the RPS-BLAST databases already have this value encoded in
    // them and therefore this value cannot be changed on an RPS-BLAST run
}

void
CBlastRPSOptionsHandle::SetQueryOptionDefaults()
{
    SetSegFiltering(false);
    m_Opts->SetStrandOption(objects::eNa_strand_unknown);
}

void
CBlastRPSOptionsHandle::SetInitialWordOptionsDefaults()
{
    SetXDropoff(BLAST_UNGAPPED_X_DROPOFF_PROT);
    SetWindowSize(BLAST_WINDOW_SIZE_PROT);
    // FIXME: extend_word_method is missing
}

void
CBlastRPSOptionsHandle::SetGappedExtensionDefaults()
{
    SetGapXDropoff(BLAST_GAP_X_DROPOFF_PROT);
    SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_PROT);
    SetGapTrigger(BLAST_GAP_TRIGGER_PROT);
    m_Opts->SetGapExtnAlgorithm(eDynProgScoreOnly);
    m_Opts->SetGapTracebackAlgorithm(eDynProgTbck);
    SetCompositionBasedStats(false);
}


void
CBlastRPSOptionsHandle::SetScoringOptionsDefaults()
{
    SetGappedMode();
    // set invalid values for options that are not applicable
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
}

void
CBlastRPSOptionsHandle::SetHitSavingOptionsDefaults()
{
    SetHitlistSize(500);
    SetEvalueThreshold(BLAST_EXPECT_VALUE);
    SetMinDiagSeparation(0);
    SetPercentIdentity(0);
    m_Opts->SetSumStatisticsMode(false);
    // set some default here, allow INT4MAX to mean infinity
    SetMaxNumHspPerSequence(0); 

    SetCutoffScore(0); // will be calculated based on evalue threshold,
    // effective lengths and Karlin-Altschul params in BLAST_Cutoffs_simple
    // and passed to the engine in the params structure

    SetQueryCovHspPerc(0);
}

void
CBlastRPSOptionsHandle::SetEffectiveLengthsOptionsDefaults()
{
    SetDbLength(0);
    SetDbSeqNum(0);
    SetEffectiveSearchSpace(0);
}

void
CBlastRPSOptionsHandle::SetSubjectSequenceOptionsDefaults()
{}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
