#ifndef ALGO_ALIGN_SPLICEDALIGNER32__HPP
#define ALGO_ALIGN_SPLICEDALIGNER32__HPP

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
* Author:  Yuri Kapustin
*
*/

#include "nw_spliced_aligner.hpp"

/** @addtogroup AlgoAlignSpliced
 *
 * @{
 */


BEGIN_NCBI_SCOPE

const size_t splice_type_count_32 = 3;

class NCBI_XALGOALIGN_EXPORT CSplicedAligner32: public CSplicedAligner
{
public:

    CSplicedAligner32(void);

    CSplicedAligner32(const char* seq1, size_t len1,
                      const char* seq2, size_t len2);

    CSplicedAligner32(const string& seq1, const string& seq2);

    // Getters
    static TScore GetDefaultWi  (unsigned char splice_type);
    static TScore GetDefaultWd1 (void) { return  -3; }
    static TScore GetDefaultWd2 (void) { return  -5; }

    // returns the size of a single backtrace matrix element
    virtual size_t GetElemSize(void) const {
        return 4;
    }

    virtual size_t GetSpliceTypeCount(void) {
        return splice_type_count_32;
    }

    virtual TScore ScoreFromTranscript(const TTranscript& transcript,
                                       size_t start1 = kMax_UInt,
                                       size_t start2 = kMax_UInt ) const;

protected:

    TScore m_Wi [splice_type_count_32];

    TScore m_Wd1; // applies if only one donor/acceptor char damaged
    TScore m_Wd2; // applies if both chars damaged

    virtual TScore* x_GetSpliceScores(void) {
        return m_Wi;
    }
    virtual TScore  x_Align (CNWAligner::SAlignInOut* data);

    void x_DoBackTrace(const Uint4* backtrace_matrix,
                       CNWAligner::SAlignInOut* data);
};


END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2005/04/04 16:32:23  kapustin
 * Distinguish matches from mismatches in raw transcripts
 *
 * Revision 1.11  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.10  2004/11/29 14:36:45  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 * Revision 1.9  2004/06/29 20:46:04  kapustin
 * Support simultaneous segment computing
 *
 * Revision 1.8  2004/04/23 14:39:22  kapustin
 * Add Splign librry and other changes
 *
 * Revision 1.4  2003/12/29 13:02:03  kapustin
 * Make x_GetElemSize() public and rename.
 *
 * Revision 1.3  2003/09/26 14:43:01  kapustin
 * Remove exception specifications
 *
 * Revision 1.2  2003/09/10 20:12:47  kapustin
 * Update Doxygen tags
 *
 * Revision 1.1  2003/09/02 22:27:44  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO_ALIGN_SPLICEDALIGNER32__HPP */
