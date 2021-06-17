#ifndef ALGO_ALIGN___NW_SPLICED_ALIGNER__HPP
#define ALGO_ALIGN___NW_SPLICED_ALIGNER__HPP

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
* File Description:
*   Base class for spliced aligners.
*
*/

#include "nw_band_aligner.hpp"


/** @addtogroup AlgoAlignSpliced
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XALGOALIGN_EXPORT CSplicedAligner: public CBandAligner
{
public:

    // Setters and getters
    void   SetWi(unsigned char splice_type, TScore value);
    TScore GetWi(unsigned char splice_type);

    void SetCDS(size_t cds_start, size_t cds_stop) {
        m_cds_start = cds_start;
        m_cds_stop = cds_stop;
    }

    void SetIntronMinSize(size_t s) {
        m_IntronMinSize  = s;
    }

    size_t GetIntronMinSize(void) const {
        return m_IntronMinSize;
    }

    static size_t GetDefaultIntronMinSize (void) {
        return 25;
    }

    void CheckPreferences(void);

    virtual size_t GetSpliceTypeCount(void)  = 0;

protected:

    CSplicedAligner();

    CSplicedAligner( const char* seq1, size_t len1,
                     const char* seq2, size_t len2);
    CSplicedAligner(const string& seq1, const string& seq2);

    size_t  m_IntronMinSize;
    size_t  m_cds_start, m_cds_stop;

    virtual bool    x_CheckMemoryLimit(void);

    virtual TScore* x_GetSpliceScores() = 0;

    // a trivial but helpful memory allocator for core dynprog
    // that promptly throws std::bad_alloc on failure
    template<class T>
    struct SAllocator {

        SAllocator (size_t N) {
            m_Buf = new T [N];
            memset((void*)m_Buf, 0, N * sizeof(T));
        }

        ~SAllocator() { delete[] m_Buf; }

        T * GetPointer(void) {
            return m_Buf;
        }

        T * m_Buf;
    };
};


END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_ALIGN___SPLICED_ALIGNER__HPP */
