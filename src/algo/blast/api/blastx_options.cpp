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

/// @file blastx_options.cpp
/// Implements the CBlastxOptionsHandle class.

#include <ncbi_pch.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <objects/seqloc/Na_strand.hpp>


/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastxOptionsHandle::CBlastxOptionsHandle(EAPILocality locality)
    : CBlastProteinOptionsHandle(locality)
{
    SetDefaults();
    m_Opts->SetProgram(eBlastx);
}

void 
CBlastxOptionsHandle::SetLookupTableDefaults()
{
    CBlastProteinOptionsHandle::SetLookupTableDefaults();
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_BLASTX);
}

void
CBlastxOptionsHandle::SetQueryOptionDefaults()
{
    CBlastProteinOptionsHandle::SetQueryOptionDefaults();
    m_Opts->SetStrandOption(objects::eNa_strand_both);
    m_Opts->SetQueryGeneticCode(BLAST_GENETIC_CODE);
    SetSegFiltering(false); // disable SEG filtering because of eCompositionMatrixAdjust mode
}

void
CBlastxOptionsHandle::SetGappedExtensionDefaults()
{
    CBlastProteinOptionsHandle::SetGappedExtensionDefaults();
    m_Opts->SetCompositionBasedStats(eCompositionMatrixAdjust);
    m_Opts->SetSmithWatermanMode(false);
    _ASSERT(!m_Opts->GetSegFiltering());
}

void
CBlastxOptionsHandle::SetScoringOptionsDefaults()
{
    CBlastProteinOptionsHandle::SetScoringOptionsDefaults();
}

void
CBlastxOptionsHandle::SetHitSavingOptionsDefaults()
{
    CBlastProteinOptionsHandle::SetHitSavingOptionsDefaults();
    m_Opts->SetSumStatisticsMode();
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
