#ifndef ALGO___MM_ALIGNER__HPP
#define ALGO___MM_ALIGNER__HPP

/* $Id $
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
*   CMMAligner class definition
*
*   CMMAligner encapsulates the Myers and Miller
*   global alignment algorithm featuring affine
*   gap penalty model and requiring linear
*   space to run.
*
*   E.W. Myers and W. Miller
*   Optimal alignment in linear space
*   Comp. Appl. Biosciences, 4:11-17, 1988 
*
*/

#include "nw_aligner.hpp"
#include <list>

BEGIN_NCBI_SCOPE

// MM algorithm encapsulation
//

class CMMAligner: public CNWAligner
{
public:

    CMMAligner(const char* seq1, size_t len1,
               const char* seq2, size_t len2,
               EScoringMatrixType matrix_type)
        throw(CNWAlignerException);

    virtual ~CMMAligner() {}

    // Run this algorithm, return the alignment's score
    virtual TScore Run();

protected:

    TScore                   m_score;
    list<ETranscriptSymbol>  m_TransList;
    
    struct SCoordRect {
        size_t i1, j1, i2, j2;
        SCoordRect(size_t l, size_t t, size_t r, size_t b):
            i1(l), j1(t), i2(r), j2(b) {}
    };
    
    void x_DoSubmatrix(const SCoordRect& submatr,
                   list<ETranscriptSymbol>::iterator translist_pos,
                   bool left_top, bool right_bottom);

    void x_RunTop(const SCoordRect& rect,
             vector<TScore>& vE, vector<TScore>& vF, vector<TScore>& vG,
             vector<unsigned char>& trace, bool lt) const;

    void x_RunBtm(const SCoordRect& rect,
             vector<TScore>& vE, vector<TScore>& vF, vector<TScore>& vG,
             vector<unsigned char>& trace, bool rb) const;

    TScore x_RunTerm(const SCoordRect& rect,
                     bool left_top, bool right_bottom,
                     list<ETranscriptSymbol>& subpath);
    
    enum ETransitionType {
        eII = 0,  eDI,   eGI,
        eID,      eDD,   eGD,
        eIG,      eDG,   eGG
    };
    TScore x_FindBestJ( const vector<TScore>& vEtop,
                        const vector<TScore>& vFtop,
                        const vector<TScore>& vGtop,
                        const vector<TScore>& vEbtm,
                        const vector<TScore>& vFbtm,
                        const vector<TScore>& vGbtm,
                        size_t& pos,
                        ETransitionType& trans_type ) const;
    
    size_t x_ExtendSubpath(vector<unsigned char>::const_iterator trace_it,
                           bool direction,
                           list<ETranscriptSymbol>& subpath) const;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/01/21 12:33:10  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO___MM_ALIGNER__HPP */
