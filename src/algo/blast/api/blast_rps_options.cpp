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

void 
CBlastRPSOptionsHandle::SetLookupTableDefaults()
{
    m_Opts->SetLookupTableType(RPS_LOOKUP_TABLE);
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
    m_Opts->SetUngappedExtension();
}

void
CBlastRPSOptionsHandle::SetGappedExtensionDefaults()
{
    SetGapXDropoff(BLAST_GAP_X_DROPOFF_PROT);
    SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_PROT);
    SetGapTrigger(BLAST_GAP_TRIGGER_PROT);
    m_Opts->SetGapExtnAlgorithm(eDynProgExt);
    m_Opts->SetGapTracebackAlgorithm(eDynProgTbck);
}


void
CBlastRPSOptionsHandle::SetScoringOptionsDefaults()
{
    SetGappedMode();
    // set invalid values for options that are not applicable
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
    m_Opts->SetDecline2AlignPenalty(INT2_MAX);
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.16  2006/01/23 16:37:11  papadopo
 * use {Set|Get}MinDiagSeparation to specify the number of diagonals to be used in HSP containment tests
 *
 * Revision 1.15  2005/11/29 17:28:02  camacho
 * Remove BlastHitSavingOptions::required_{start,end}
 *
 * Revision 1.14  2005/10/06 19:43:07  camacho
 * CBlastOptionsHandle subclasses must call SetDefaults unconditionally.
 * Fixes problem with uninitializes program and service name for CRemoteBlast.
 *
 * Revision 1.13  2005/05/16 12:24:37  madden
 * Remove references to [GS]etPrelimHitlistSize
 *
 * Revision 1.12  2005/03/02 16:45:36  camacho
 * Remove use_real_db_size
 *
 * Revision 1.11  2005/02/24 13:46:54  madden
 * Changes to use structured filteing options instead of string
 *
 * Revision 1.10  2005/01/11 17:50:39  dondosha
 * Removed total HSP limit option
 *
 * Revision 1.9  2004/09/29 20:47:23  papadopo
 * use the actual underlying score matrix name from the RPS DB, rather than just hardwiring BLOSUM62
 *
 * Revision 1.8  2004/08/03 21:10:42  dondosha
 * Set DbSeqNum option to 0 by default - the real value is set in parameter structure
 *
 * Revision 1.7  2004/06/08 15:20:23  dondosha
 * Skip traceback option has been moved to the traceback extension method enum
 *
 * Revision 1.6  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.5  2004/05/17 15:32:39  madden
 * Int algorithm_type replaced with enum EBlastPrelimGapExt
 *
 * Revision 1.4  2004/04/23 13:50:16  papadopo
 * derived BlastRPSOptionsHandle from BlastOptions (again)
 *
 * Revision 1.3  2004/04/16 14:27:47  papadopo
 * make this class a derived class of CBlastProteinOptionsHandle, that corresponds to the eRPSBlast program
 *
 * Revision 1.2  2004/03/19 15:13:34  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.1  2004/03/10 14:52:13  madden
 * Options handle for RPSBlast searches
 *
 *
 * ===========================================================================
 */
