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
    : CNWAligner(seq1, len1, seq2, len2),
      m_IntronMinSize(GetDefaultIntronMinSize())
{
    SetEndSpaceFree(true, true, false, false);
}


CSplicedAligner::CSplicedAligner(const string& seq1, const string& seq2)
    : CNWAligner(seq1, seq2),
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

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2006/06/27 15:16:58  kapustin
 * Extra penalty for in-cds gap extensions
 *
 * Revision 1.12  2005/07/26 16:43:29  kapustin
 * Move MakePattern() to CNWAligner
 *
 * Revision 1.11  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.10  2004/12/16 22:42:22  kapustin
 * Move to algo/align/nw
 *
 * Revision 1.9  2004/11/29 14:37:15  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction 
 * can be specified. x_ScoreByTanscript renamed to 
 * ScoreFromTranscript with two additional parameters to specify 
 * starting coordinates.
 *
 * Revision 1.8  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.7  2004/05/17 14:50:56  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.6  2004/04/23 14:39:47  kapustin
 * Add Splign library and other changes
 *
 * Revision 1.4  2003/10/27 21:00:17  kapustin
 * Set intron penalty defaults differently for 16- and 32-bit versions 
 * according to the expected quality of sequences those variants are 
 * supposed to be used with.
 *
 * Revision 1.3  2003/09/30 19:50:04  kapustin
 * Make use of standard score matrix interface
 *
 * Revision 1.2  2003/09/26 14:43:18  kapustin
 * Remove exception specifications
 *
 * Revision 1.1  2003/09/02 22:34:49  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
