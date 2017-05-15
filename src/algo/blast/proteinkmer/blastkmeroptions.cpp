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
 * Author: Tom Madden
 *
 * File Description:
 *   Options for BLAST-kmer searches
 *
 */

#include <ncbi_pch.hpp>
#include <algo/blast/proteinkmer/blastkmeroptions.hpp>

BEGIN_NCBI_SCOPE

CBlastKmerOptions::CBlastKmerOptions()	
    : m_Thresh(0.10),
      m_MinHits(1),
      m_NumTargetSeqs(500)
{
}	

bool
CBlastKmerOptions::Validate() const
{
	if (m_Thresh <= 0.0 || m_Thresh > 1.0)
		return false;

	if (m_MinHits < 0)
		return false;

	if (m_NumTargetSeqs < 0) 
		return false;

	return true;
}

END_NCBI_SCOPE

