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
 * Authors:  Kevin Bealer
 *
 */

/// @file psiblast_options.cpp
/// Implements the CPSIBlastOptionsHandle class.

#include <ncbi_pch.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CPSIBlastOptionsHandle::CPSIBlastOptionsHandle(EAPILocality locality)
    : CBlastAdvancedProteinOptionsHandle(locality)
{
    SetDefaults();
    m_Opts->SetProgram(ePSIBlast);
    if (m_Opts->GetLocality() == CBlastOptions::eRemote) {
        return;
    }
    SetPSIBlastDefaults();
}

void
CPSIBlastOptionsHandle::SetQueryOptionDefaults()
{
    m_Opts->ClearFilterOptions();   // turn off all filtering.
}

void CPSIBlastOptionsHandle::SetPSIBlastDefaults(void)
{
    m_Opts->SetInclusionThreshold( PSI_INCLUSION_ETHRESH );
    m_Opts->SetPseudoCount( PSI_PSEUDO_COUNT_CONST );
}

void CPSIBlastOptionsHandle::SetPSITblastnDefaults(void)
{
    m_Opts->SetProgram(ePSITblastn);
    m_Opts->SetSmithWatermanMode(false);
    m_Opts->SetDbGeneticCode(BLAST_GENETIC_CODE);
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_TBLASTN);
}
 
END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
