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
 * Authors:  Ilya Dondoshansky
 *
 */

/// @file phiblast_prot_options.cpp
/// Implements the CPHIBlastProtOptionsHandle class.

#include <ncbi_pch.hpp>
#include <algo/blast/api/phiblast_prot_options.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CPHIBlastProtOptionsHandle::CPHIBlastProtOptionsHandle(EAPILocality locality)
    : CBlastAdvancedProteinOptionsHandle(locality)
{
    if (m_Opts->GetLocality() == CBlastOptions::eRemote) {
        return;
    }
    SetDefaults();
    m_Opts->SetProgram(ePHIBlastp);
}

void CPHIBlastProtOptionsHandle::SetGappedExtensionDefaults()
{
    CBlastAdvancedProteinOptionsHandle::SetGappedExtensionDefaults();
    m_Opts->SetCompositionBasedStatsMode(false);
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/26 14:35:04  dondosha
 * Implementation of PHI BLAST options handle classes
 *
 *
 * ===========================================================================
 */
