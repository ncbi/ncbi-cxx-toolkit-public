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
    : CBlastOptionsHandle(locality),
      m_FactorySetting(eMegablast)
{
    SetDefaults();
}

void
CBlastNucleotideOptionsHandle::SetDefaults()
{
    SetTraditionalMegablastDefaults();
    m_Opts->SetProgram(eBlastn);
}

void
CBlastNucleotideOptionsHandle::SetTraditionalBlastnDefaults()
{
    m_FactorySetting = eBlastn;
    
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
}

void
CBlastNucleotideOptionsHandle::SetTraditionalMegablastDefaults()
{
    m_FactorySetting = eMegablast;
    
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
    SetHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
}

void 
CBlastNucleotideOptionsHandle::SetLookupTableDefaults()
{
    SetLookupTableType(NA_LOOKUP_TABLE);
    SetWordSize(BLAST_WORDSIZE_NUCL);
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_BLASTN);
    SetVariableWordSize(BLAST_VARWORD_NUCL);
}

void 
CBlastNucleotideOptionsHandle::SetMBLookupTableDefaults()
{
    SetLookupTableType(MB_LOOKUP_TABLE);
    SetWordSize(BLAST_WORDSIZE_MEGABLAST);
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_MEGABLAST);
    m_Opts->SetMBMaxPositions(INT4_MAX);
    SetVariableWordSize(BLAST_VARWORD_MEGABLAST);
}

void
CBlastNucleotideOptionsHandle::SetQueryOptionDefaults()
{
    SetDustFiltering(true);
    SetStrandOption(objects::eNa_strand_both);
}

void
CBlastNucleotideOptionsHandle::SetInitialWordOptionsDefaults()
{
    SetXDropoff(BLAST_UNGAPPED_X_DROPOFF_NUCL);
    SetWindowSize(BLAST_WINDOW_SIZE_NUCL);
    SetUngappedExtension();
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
    SetGapExtnAlgorithm(eDynProgExt);
    SetGapTracebackAlgorithm(eDynProgTbck);
}

void
CBlastNucleotideOptionsHandle::SetMBGappedExtensionDefaults()
{
    SetGapXDropoff(BLAST_GAP_X_DROPOFF_NUCL);
    SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_NUCL);
    SetGapTrigger(BLAST_GAP_TRIGGER_NUCL);
    SetGapExtnAlgorithm(eGreedyWithTracebackExt);
    /* Traceback will not be performed, but set traceback extension algorithm
       to greedy, for consistency of default gap penalties. */
    SetGapTracebackAlgorithm(eGreedyTbck); 
}


void
CBlastNucleotideOptionsHandle::SetScoringOptionsDefaults()
{
    SetMatrixName(NULL);
    SetMatrixPath(FindMatrixPath(GetMatrixName(), false).c_str());
    SetGapOpeningCost(BLAST_GAP_OPEN_NUCL);
    SetGapExtensionCost(BLAST_GAP_EXTN_NUCL);
    SetMatchReward(BLAST_REWARD);
    SetMismatchPenalty(BLAST_PENALTY);
    SetGappedMode();

    // set out-of-frame options to invalid? values
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
    m_Opts->SetDecline2AlignPenalty(INT2_MAX);
}

void
CBlastNucleotideOptionsHandle::SetMBScoringOptionsDefaults()
{
    SetMatrixName(NULL);
    SetMatrixPath(FindMatrixPath(GetMatrixName(), false).c_str());
    SetGapOpeningCost(BLAST_GAP_OPEN_MEGABLAST);
    SetGapExtensionCost(BLAST_GAP_EXTN_MEGABLAST);
    SetMatchReward(BLAST_REWARD);
    SetMismatchPenalty(BLAST_PENALTY);
    SetGappedMode();

    // set out-of-frame options to invalid? values
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
    m_Opts->SetDecline2AlignPenalty(INT2_MAX);
}
void
CBlastNucleotideOptionsHandle::SetHitSavingOptionsDefaults()
{
    SetHitlistSize(500);
    SetPrelimHitlistSize(550);
    SetEvalueThreshold(BLAST_EXPECT_VALUE);
    SetPercentIdentity(0);
    // set some default here, allow INT4MAX to mean infinity
    SetMaxNumHspPerSequence(0); 

    SetCutoffScore(0); // will be calculated based on evalue threshold,
    // effective lengths and Karlin-Altschul params in BLAST_Cutoffs_simple
    // and passed to the engine in the params structure

    // not applicable
    m_Opts->SetRequiredStart(0);
    m_Opts->SetRequiredEnd(0);
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2005/03/02 16:45:36  camacho
 * Remove use_real_db_size
 *
 * Revision 1.20  2005/02/24 13:46:54  madden
 * Changes to use structured filteing options instead of string
 *
 * Revision 1.19  2005/01/11 17:50:39  dondosha
 * Removed total HSP limit option
 *
 * Revision 1.18  2005/01/10 13:34:45  madden
 * Calls to SetAlphabetSize, CalculateBestStride, SetScanStep, SetSeedContainerType,
 * and SetSeedExtensionMethod removed as no longe applicable.  Calls to SetVariableWordSize
 * moved.
 *
 * Revision 1.17  2004/12/21 17:18:24  dondosha
 * eSkipTbck option has been removed
 *
 * Revision 1.16  2004/08/24 20:53:57  bealer
 * - Set default to megablast (should have no effect).
 *
 * Revision 1.15  2004/08/05 20:41:18  dondosha
 * Implemented stacks initial word container for all blastn extension methods
 *
 * Revision 1.14  2004/08/03 20:21:42  dondosha
 * Set seed extension method to eLastHitArray rather than eDiagArray
 *
 * Revision 1.13  2004/08/02 15:00:16  bealer
 * - Distinguish between blastn and megablast (for remote blast).
 *
 * Revision 1.12  2004/06/08 15:20:22  dondosha
 * Skip traceback option has been moved to the traceback extension method enum
 *
 * Revision 1.11  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.10  2004/05/17 15:32:39  madden
 * Int algorithm_type replaced with enum EBlastPrelimGapExt
 *
 * Revision 1.9  2004/04/07 03:06:15  camacho
 * Added blast_encoding.[hc], refactoring blast_stat.[hc]
 *
 * Revision 1.8  2004/03/22 20:15:26  dondosha
 * Set final x-dropoff for MB extension defaults
 *
 * Revision 1.7  2004/03/19 15:13:34  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.6  2004/03/17 19:15:28  dondosha
 * Corrected order of defaults setting, so scanning stride is set properly
 *
 * Revision 1.5  2004/03/11 17:27:12  camacho
 * Fix incorrect doxygen file directive
 *
 * Revision 1.4  2004/02/17 23:53:31  dondosha
 * Added setting of preliminary hitlist size
 *
 * Revision 1.3  2004/02/10 19:47:46  dondosha
 * Added SetMBGappedExtensionDefaults method; corrected megablast defaults setting
 *
 * Revision 1.2  2004/01/16 21:49:26  bealer
 * - Add locality flag for Blast4 API
 *
 * Revision 1.1  2003/11/26 18:23:59  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */
