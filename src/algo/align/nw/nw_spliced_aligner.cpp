/* $Id$
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
* Authors:  Yuri Kapustin
*
* File Description:  Base class for spliced aligners.
*                   
* ===========================================================================
*
*/


#include <ncbi_pch.hpp>
#include "messages.hpp"
#include <algo/align/nw/nw_spliced_aligner.hpp>
#include <algo/align/nw/align_exception.hpp>

BEGIN_NCBI_SCOPE


CSplicedAligner::CSplicedAligner():
    m_IntronMinSize(GetDefaultIntronMinSize()),
    m_cds_start(0), m_cds_stop(0)
{
    SetEndSpaceFree(true, true, false, false);
}
    

CSplicedAligner::CSplicedAligner(const char* seq1, size_t len1,
                                 const char* seq2, size_t len2)
    : CBandAligner(seq1, len1, seq2, len2),
      m_IntronMinSize(GetDefaultIntronMinSize())
{
    SetEndSpaceFree(true, true, false, false);
}


CSplicedAligner::CSplicedAligner(const string& seq1, const string& seq2)
    : CBandAligner(seq1, seq2),
      m_IntronMinSize(GetDefaultIntronMinSize())
{
    SetEndSpaceFree(true, true, false, false);
}


void CSplicedAligner::SetWi  (unsigned char splice_type, TScore value)
{
    if(splice_type < GetSpliceTypeCount()) {
        x_GetSpliceScores()[splice_type]  = value;
    }
    else 
    {
        NCBI_THROW(CAlgoAlignException,
                   eInvalidSpliceTypeIndex,
                   g_msg_InvalidSpliceTypeIndex);
    }
}


CSplicedAligner::TScore CSplicedAligner::GetWi  (unsigned char splice_type)
{
    if(splice_type < GetSpliceTypeCount()) {
        return x_GetSpliceScores()[splice_type];
    }
    else 
    {
        NCBI_THROW(CAlgoAlignException,
                   eInvalidSpliceTypeIndex,
                   g_msg_InvalidSpliceTypeIndex);
    }
}


bool CSplicedAligner::x_CheckMemoryLimit()
{
    return CNWAligner::x_CheckMemoryLimit();
}


END_NCBI_SCOPE
