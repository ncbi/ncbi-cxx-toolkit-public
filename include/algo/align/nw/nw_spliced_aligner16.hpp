#ifndef ALGO_ALIGN_SPLICEDALIGNER16__HPP
#define ALGO_ALIGN_SPLICEDALIGNER16__HPP

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
* Author:  Yuri Kapustin, Alexander Souvorov
*
*/

#include "nw_spliced_aligner.hpp"


/** @addtogroup AlgoAlignSpliced
 *
 * @{
 */


BEGIN_NCBI_SCOPE

const size_t splice_type_count_16 (4);

class NCBI_XALGOALIGN_EXPORT CSplicedAligner16: public CSplicedAligner
{
public:

    CSplicedAligner16(void);

    CSplicedAligner16(const char* seq1, size_t len1,
                      const char* seq2, size_t len2);

    CSplicedAligner16(const string& seq1, const string& seq2);

    // Getters
    static TScore  GetDefaultWi  (unsigned char splice_type);

    // returns the size of a single backtrace matrix element
    virtual size_t GetElemSize(void) const {
#ifdef ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY
        return 2;
#else
        return 3;
#endif
    }

    virtual size_t GetSpliceTypeCount(void) {
        return splice_type_count_16;
    }

    virtual TScore ScoreFromTranscript(const TTranscript& transcript,
                                       size_t start1 = kMax_UInt,
                                       size_t start2 = kMax_UInt ) const;

protected:

    TScore m_Wi [splice_type_count_16];

    virtual TScore* x_GetSpliceScores(void) {
        return m_Wi;
    }
    virtual TScore  x_Align (CNWAligner::SAlignInOut* data);

#ifdef ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY
    void x_DoBackTrace(const Uint2* backtrace_matrix,
                       CNWAligner::SAlignInOut* data,
                       int i_global_max,
                       int j_global_max);
#else
    void x_DoBackTrace(const Uint2* backtrace_matrix,
                       const Uint1* backtrace_matrix_ext,
                       CNWAligner::SAlignInOut* data);
#endif
};


END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_ALIGN_SPLICEDALIGNER16__HPP */
