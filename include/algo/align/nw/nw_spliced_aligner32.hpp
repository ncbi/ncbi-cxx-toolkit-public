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


BEGIN_NCBI_SCOPE

const size_t splice_type_count_32 = 3;

class NCBI_XALGOALIGN_EXPORT CSplicedAligner32: public CSplicedAligner
{
public:

    CSplicedAligner32();

    CSplicedAligner32( const char* seq1, size_t len1,
                       const char* seq2, size_t len2)
        throw(CAlgoAlignException);

    // Getters
    static TScore GetDefaultWi  (unsigned char splice_type)
        throw(CAlgoAlignException);
    static TScore GetDefaultWd1 () { return  -5; }
    static TScore GetDefaultWd2 () { return  -8; }

protected:

    TScore m_Wi [splice_type_count_32];

    TScore m_Wd1; // applies if only one donor/acceptor char damaged
    TScore m_Wd2; // applies if both chars damaged

    virtual size_t  x_GetSpliceTypeCount() {
        return splice_type_count_32;
    }
    virtual TScore* x_GetSpliceScores() {
        return m_Wi;
    }
    virtual TScore  x_Align ( const char* seg1, size_t len1,
                              const char* seg2, size_t len2,
                              vector<ETranscriptSymbol>* transcript );

    // backtrace
    void x_DoBackTrace( const Uint4* backtrace_matrix,
                        size_t N1, size_t N2,
                        vector<ETranscriptSymbol>* transcript );
    // included primarily for test purpose
    virtual TScore  x_ScoreByTranscript() const throw(CAlgoAlignException);

    // returns the size of a single backtrace matrix element
    virtual size_t x_GetElemSize() const {
        return 4;
    }
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/09/02 22:27:44  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO_ALIGN_SPLICEDALIGNER32__HPP */
