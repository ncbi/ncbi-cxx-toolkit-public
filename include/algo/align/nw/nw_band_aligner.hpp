#ifndef  ALGO_ALIGN___NW_BAND_ALIGNER__HPP
#define ALGO_ALIGN___NW_BAND_ALIGNER__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
*   CBandAligner class definition
*
*   CBandAligner implements a generic global (Needleman-Wunsch)
*   alignment algorithm assuming at most K differences between 
*   input sequences.
*/

#include <algo/align/nw/nw_aligner.hpp>

/** @addtogroup AlgoAlignRoot
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XALGOALIGN_EXPORT CBandAligner: public CNWAligner
{
public:

    // ctors
    CBandAligner(size_t band = 0): 
        m_band(band),
        m_Shift(0)
    {}

    // Null scoremat pointer indicates IUPACna coding
    CBandAligner(const char* seq1, size_t len1,
                 const char* seq2, size_t len2,
                 const SNCBIPackedScoreMatrix* scoremat = 0,
                 size_t band = 0);

    CBandAligner(const string& seq1,
                 const string& seq2,
                 const SNCBIPackedScoreMatrix* scoremat = 0,
                 size_t band = 0);

    virtual ~CBandAligner(void) {}

    // Setters    
    void    SetBand(size_t band) { m_band = band; }
    void    SetShift(Uint1 where, size_t offset);

    // Getters
    size_t  GetBand  (void) const { return  m_band; }
    pair<Uint1,size_t> GetShift(void) const;

protected:

    // band width
    size_t   m_band;

    // band shift along the first sequence (can be negative)
    long     m_Shift;

    // backtrace helpers
    size_t   m_TermK;
    size_t   m_LastCoordSeq1;
    size_t   m_LastCoordSeq2;

    // core dynamic programming
    virtual TScore x_Align (CNWAligner::SAlignInOut* data);

    // backtrace
    void           x_DoBackTrace(const CBacktraceMatrix4 & backtrace,
                                 CNWAligner::SAlignInOut* data);

    // other
    void x_CheckParameters(const SAlignInOut* data) const;
    virtual bool x_CheckMemoryLimit(void);
};


END_NCBI_SCOPE


/* @} */

#endif  /* ALGO_ALIGN___NW_BAND_ALIGNER__HPP */
