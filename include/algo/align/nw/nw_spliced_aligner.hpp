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

#include "nw_aligner.hpp"


/** @addtogroup AlgoAlignSpliced
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XALGOALIGN_EXPORT CSplicedAligner: public CNWAligner
{
public:

    // Setters and getters
    void   SetWi(unsigned char splice_type, TScore value);
    TScore GetWi(unsigned char splice_type);

    void SetIntronMinSize(size_t s) {
        m_IntronMinSize  = s;
    }

    size_t GetIntronMinSize(void) const {
        return m_IntronMinSize;
    }

    static size_t GetDefaultIntronMinSize (void) {
        return 30;
    }

    virtual size_t GetSpliceTypeCount(void)  = 0;

    // A naive pattern generator-use cautiously.
    // Do not use on sequences with repeats or error.

    size_t MakePattern(const size_t hit_size = 30);

protected:

    CSplicedAligner();
    CSplicedAligner( const char* seq1, size_t len1,
                     const char* seq2, size_t len2);
    CSplicedAligner(const string& seq1, const string& seq2);

    size_t  m_IntronMinSize;

    virtual TScore* x_GetSpliceScores() = 0;
  
    // Guides
    unsigned char   x_CalcFingerPrint64( const char* beg,
                                         const char* end,
                                         size_t& err_index );
    const char*     x_FindFingerPrint64( const char* beg, 
                                         const char* end,
                                         unsigned char fingerprint,
                                         size_t size,
                                         size_t& err_index );
};


END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.10  2004/04/23 14:39:22  kapustin
 * Add Splign librry and other changes
 *
 * Revision 1.6  2003/12/12 19:41:46  kapustin
 * Valuable comments added
 *
 * Revision 1.5  2003/10/27 20:56:50  kapustin
 * Move static GetDefaultWi to descendants
 *
 * Revision 1.4  2003/09/30 19:49:32  kapustin
 * Make use of standard score matrix interface
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

#endif  /* ALGO_ALIGN___SPLICED_ALIGNER__HPP */
