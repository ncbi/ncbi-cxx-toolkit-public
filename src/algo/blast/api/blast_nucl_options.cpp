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
 * Authors:  Christiam Camacho
 *
 */

/// @file blast_nucl_options.cpp
/// Implements the CBlastNucleotideOptionsHandle class.

#include <ncbi_pch.hpp>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastNucleotideOptionsHandle::CBlastNucleotideOptionsHandle(EAPILocality locality)
    : CBlastOptionsHandle(locality)
{
    SetDefaults();
}

void
CBlastNucleotideOptionsHandle::SetDefaults()
{
    m_Opts->SetDefaultsMode(true);
    SetTraditionalMegablastDefaults();
    m_Opts->SetDefaultsMode(false);
}

void
CBlastNucleotideOptionsHandle::SetTraditionalBlastnDefaults()
{
    m_Opts->SetDefaultsMode(true);
    m_Opts->SetRemoteProgramAndService_Blast3("blastn", "plain");
    m_Opts->SetProgram(eBlastn);
    
    if (m_Opts->GetLocality() == CBlastOptions::eRemote) {
        return;
    }
    
    SetQueryOptionDefaults();
    SetLookupTableDefaults();
    // NB: Initial word defaults must be set after lookup table defaults, 
    // because default scanning stride depends on the lookup table type.
    SetInitialWordOptionsDefaults();
    SetGappedExtensionDefaults();
    SetScoringOptionsDefaults();
    SetHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
    m_Opts->SetDefaultsMode(false);
}

void
CBlastNucleotideOptionsHandle::SetTraditionalMegablastDefaults()
{
    m_Opts->SetDefaultsMode(true);
    m_Opts->SetRemoteProgramAndService_Blast3("blastn", "megablast");
    m_Opts->SetProgram(eMegablast);
    
    if (m_Opts->GetLocality() == CBlastOptions::eRemote) {
        return;
    }
    SetQueryOptionDefaults();
    SetMBLookupTableDefaults();
    // NB: Initial word defaults must be set after lookup table defaults, 
    // because default scanning stride depends on the lookup table type.
    SetMBInitialWordOptionsDefaults();
    SetMBGappedExtensionDefaults();
    SetMBScoringOptionsDefaults();
    SetMBHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
    m_Opts->SetDefaultsMode(false);
}

void 
CBlastNucleotideOptionsHandle::SetLookupTableDefaults()
{
    SetLookupTableType(eNaLookupTable);
    SetWordSize(BLAST_WORDSIZE_NUCL);
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_BLASTN);
}

void 
CBlastNucleotideOptionsHandle::SetMBLookupTableDefaults()
{
    SetLookupTableType(eMBLookupTable);
    SetWordSize(BLAST_WORDSIZE_MEGABLAST);
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_MEGABLAST);
}

void
CBlastNucleotideOptionsHandle::SetQueryOptionDefaults()
{
    SetDustFiltering(true);
    SetMaskAtHash(true);
    SetStrandOption(objects::eNa_strand_both);
}

void
CBlastNucleotideOptionsHandle::SetInitialWordOptionsDefaults()
{
    SetXDropoff(BLAST_UNGAPPED_X_DROPOFF_NUCL);
    SetWindowSize(BLAST_WINDOW_SIZE_NUCL);
    SetOffDiagonalRange(BLAST_SCAN_RANGE_NUCL);
}

void
CBlastNucleotideOptionsHandle::SetMBInitialWordOptionsDefaults()
{
    SetWindowSize(BLAST_WINDOW_SIZE_NUCL);
}

void
CBlastNucleotideOptionsHandle::SetGappedExtensionDefaults()
{
    SetGapXDropoff(BLAST_GAP_X_DROPOFF_NUCL);
    SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_NUCL);
    SetGapTrigger(BLAST_GAP_TRIGGER_NUCL);
    SetGapExtnAlgorithm(eDynProgScoreOnly);
    SetGapTracebackAlgorithm(eDynProgTbck);
}

void
CBlastNucleotideOptionsHandle::SetMBGappedExtensionDefaults()
{
    SetGapXDropoff(BLAST_GAP_X_DROPOFF_NUCL);
    SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_NUCL);
    SetGapTrigger(BLAST_GAP_TRIGGER_NUCL);
    SetGapExtnAlgorithm(eGreedyScoreOnly);
    SetGapTracebackAlgorithm(eGreedyTbck); 
}


void
CBlastNucleotideOptionsHandle::SetScoringOptionsDefaults()
{
    SetMatrixName(NULL);
    SetGapOpeningCost(BLAST_GAP_OPEN_NUCL);
    SetGapExtensionCost(BLAST_GAP_EXTN_NUCL);
    SetMatchReward(2);
    SetMismatchPenalty(-3);
    SetGappedMode();

    // set out-of-frame options to invalid? values
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
}

void
CBlastNucleotideOptionsHandle::SetMBScoringOptionsDefaults()
{
    SetMatrixName(NULL);
    SetGapOpeningCost(BLAST_GAP_OPEN_MEGABLAST);
    SetGapExtensionCost(BLAST_GAP_EXTN_MEGABLAST);
    SetMatchReward(1);
    SetMismatchPenalty(-2);
    SetGappedMode();

    // set out-of-frame options to invalid? values
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
}
void
CBlastNucleotideOptionsHandle::SetHitSavingOptionsDefaults()
{
    SetHitlistSize(500);
    SetEvalueThreshold(BLAST_EXPECT_VALUE);
    SetPercentIdentity(0);
    // set some default here, allow INT4MAX to mean infinity
    SetMaxNumHspPerSequence(0); 
    SetMinDiagSeparation(50);

    SetCutoffScore(0); // will be calculated based on evalue threshold,
    // effective lengths and Karlin-Altschul params in BLAST_Cutoffs_simple
    // and passed to the engine in the params structure
}
void
CBlastNucleotideOptionsHandle::SetMBHitSavingOptionsDefaults()
{
    SetHitlistSize(500);
    SetEvalueThreshold(BLAST_EXPECT_VALUE);
    SetPercentIdentity(0);
    // set some default here, allow INT4MAX to mean infinity
    SetMaxNumHspPerSequence(0); 
    SetMinDiagSeparation(6);

    SetCutoffScore(0); // will be calculated based on evalue threshold,
    // effective lengths and Karlin-Altschul params in BLAST_Cutoffs_simple
    // and passed to the engine in the params structure
}

void
CBlastNucleotideOptionsHandle::SetEffectiveLengthsOptionsDefaults()
{
    SetDbLength(0);
    SetDbSeqNum(0);
    SetEffectiveSearchSpace(0);
}

void
CBlastNucleotideOptionsHandle::SetSubjectSequenceOptionsDefaults()
{}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
