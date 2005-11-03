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
    SetSegFiltering(false);
}

void CPSIBlastOptionsHandle::SetPSIBlastDefaults(void)
{
    m_Opts->SetInclusionThreshold( PSI_INCLUSION_ETHRESH );
    m_Opts->SetPseudoCount( PSI_PSEUDO_COUNT_CONST );
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/11/03 22:15:01  camacho
 * Add method to override query options defaults to enforce no filtering of the query
 *
 * Revision 1.7  2005/10/06 19:43:08  camacho
 * CBlastOptionsHandle subclasses must call SetDefaults unconditionally.
 * Fixes problem with uninitializes program and service name for CRemoteBlast.
 *
 * Revision 1.6  2005/06/02 16:19:01  camacho
 * Remove LookupTableOptions::use_pssm
 *
 * Revision 1.5  2005/05/24 14:07:44  madden
 * CPSIBlastOptionsHandle now derived from CBlastAdvancedProteinOptionsHandle
 *
 * Revision 1.4  2004/12/20 20:12:01  camacho
 * + option to set use of pssm in lookup table
 *
 * Revision 1.3  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2004/05/17 19:33:23  bealer
 * - Update sources to new macro spelling.
 *
 * Revision 1.1  2004/05/17 18:55:46  bealer
 * - Add PSI-Blast support.
 *
 *
 * ===========================================================================
 */
