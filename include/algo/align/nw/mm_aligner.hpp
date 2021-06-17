#ifndef ALGO___MM_ALIGNER__HPP
#define ALGO___MM_ALIGNER__HPP

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
*   CMMAligner class definition
*
*   CMMAligner encapsulates the Hirschberg's divide-and-conquer
*   algorithm (also credited to Myers and Miller) featuring
*   affine gap penalty model and running in linear space
*
*   E.W. Myers and W. Miller
*   Optimal alignment in linear space
*   Comp. Appl. Biosciences, 4:11-17, 1988 
*
*/

#include "nw_aligner.hpp"
#include <list>
#include <corelib/ncbithr.hpp>


/** @addtogroup AlgoAlignMM
 *
 * @{
 */


BEGIN_NCBI_SCOPE

struct SCoordRect; // auxiliary structure, see below


class NCBI_XALGOALIGN_EXPORT CMMAligner: public CNWAligner
{
public:

    CMMAligner();

    CMMAligner(const char* seq1, size_t len1,
               const char* seq2, size_t len2,
               const SNCBIPackedScoreMatrix* scoremat = 0);

    CMMAligner(const string& seq1,
               const string& seq2,
               const SNCBIPackedScoreMatrix* scoremat = 0);

    virtual ~CMMAligner() {}

protected:

    list<ETranscriptSymbol>  m_TransList;

    virtual TScore   x_Run();
    
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

    virtual bool x_CheckMemoryLimit();

    friend class CThreadRunOnTop;
    friend class CThreadDoSM;

};

// auxiliary structure
    
struct NCBI_XALGOALIGN_EXPORT SCoordRect {
    size_t i1, j1, i2, j2;
    SCoordRect() {};
    SCoordRect(size_t l, size_t t, size_t r, size_t b):
        i1(l), j1(t), i2(r), j2(b) {}
    unsigned int GetArea() {
        return (i2 - i1 + 1)*(j2 - j1 + 1);
    }
};


END_NCBI_SCOPE


/* @} */

#endif  /* ALGO___MM_ALIGNER__HPP */
