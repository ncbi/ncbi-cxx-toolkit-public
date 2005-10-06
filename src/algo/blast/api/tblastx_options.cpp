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

/// @file tblastx_options.cpp
/// Implements the CTBlastxOptionsHandle class.

#include <ncbi_pch.hpp>
#include <algo/blast/api/tblastx_options.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CTBlastxOptionsHandle::CTBlastxOptionsHandle(EAPILocality locality)
    : CBlastProteinOptionsHandle(locality)
{
    SetDefaults();
    m_Opts->SetProgram(eTblastx);
}

void 
CTBlastxOptionsHandle::SetLookupTableDefaults()
{
    CBlastProteinOptionsHandle::SetLookupTableDefaults();
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_TBLASTX);
}

void
CTBlastxOptionsHandle::SetQueryOptionDefaults()
{
    CBlastProteinOptionsHandle::SetQueryOptionDefaults();
    m_Opts->SetStrandOption(objects::eNa_strand_both);
    m_Opts->SetQueryGeneticCode(BLAST_GENETIC_CODE);
}

void
CTBlastxOptionsHandle::SetScoringOptionsDefaults()
{
    CBlastProteinOptionsHandle::SetScoringOptionsDefaults();
    m_Opts->SetGappedMode(false);
}

void
CTBlastxOptionsHandle::SetHitSavingOptionsDefaults()
{
    CBlastProteinOptionsHandle::SetHitSavingOptionsDefaults();
    m_Opts->SetSumStatisticsMode();
}

void
CTBlastxOptionsHandle::SetGappedExtensionDefaults()
{
    CBlastProteinOptionsHandle::SetGappedExtensionDefaults();
    m_Opts->SetGapXDropoff(BLAST_GAP_X_DROPOFF_TBLASTX);
    m_Opts->SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_TBLASTX);
}

void
CTBlastxOptionsHandle::SetSubjectSequenceOptionsDefaults()
{
    m_Opts->SetDbGeneticCode(BLAST_GENETIC_CODE);
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/10/06 19:43:08  camacho
 * CBlastOptionsHandle subclasses must call SetDefaults unconditionally.
 * Fixes problem with uninitializes program and service name for CRemoteBlast.
 *
 * Revision 1.5  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.4  2004/03/19 15:13:34  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.3  2004/01/16 21:49:27  bealer
 * - Add locality flag for Blast4 API
 *
 * Revision 1.2  2003/12/03 16:42:33  dondosha
 * SetDbGeneticCode takes care of setting both integer and string now
 *
 * Revision 1.1  2003/11/26 18:24:01  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */
