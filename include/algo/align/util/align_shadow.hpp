#ifndef ALGO_ALIGN_UTIL_ALIGN_SHADOW__HPP
#define ALGO_ALIGN_UTIL_ALIGN_SHADOW__HPP

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
*   CAlignShadow class
*
* CAlignShadow is a simplified yet and compact representation of
* a pairwise sequence alignment which keeps information about
* the alignment's borders
*
*/


#include <corelib/ncbiobj.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/align/util/algo_align_util_exceptions.hpp>

#include <math.h>


BEGIN_SCOPE(objects)
    class CSeq_id;
END_SCOPE(objects)


BEGIN_NCBI_SCOPE


class NCBI_XALGOALIGN_EXPORT CAlignShadow: public CObject
{
public:

    typedef CConstRef<objects::CSeq_id> TId;
    typedef TSeqPos TCoord;

    // c'tors
    CAlignShadow(void);

    /// Create the object from a seq-align structure
    ///
    /// @param seq_align
    ///    Input seq-align structure to create from
    /// @param save_xcript
    ///    If true, the alignment transcript string will be run-length encoded
    ///    and saved in m_Transcript. All diagonals are recorded
    ///    as matches.
    CAlignShadow(const objects::CSeq_align& seq_align, bool save_xcript = false);

    /// Create the object from a transcript
    ///
    /// @param idquery
    ///    Query sequence ID
    /// @param qstart
    ///    Starting coordinate on the query
    /// @param qstrand
    ///    Query strand (direction)
    /// @param idsubj
    ///    Subject sequence ID
    /// @param sstart
    ///    Starting coordinate on the subject
    /// @param sstrand
    ///    Subject strand (direction)
    /// @param xcript
    ///    Plain alignment (edit) transcript.
    ///    Allowed characters are 'M', 'R', 'I', 'D'.
    ///
    CAlignShadow(const TId& idquery, TCoord qstart, bool qstrand,
                 const TId& idsubj, TCoord sstart, bool sstrand,
                 const string& xcript);

    virtual ~CAlignShadow() {}

    // getters / setters
    const TId& GetId(Uint1 where) const;
    const TId& GetQueryId(void) const;
    const TId& GetSubjId(void) const;

    void  SetId(Uint1 where, const TId& id);
    void  SetQueryId(const TId& id);
    void  SetSubjId(const TId& id);

    bool  GetStrand(Uint1 where) const;
    bool  GetQueryStrand(void) const;
    bool  GetSubjStrand(void) const;

    void  SetStrand(Uint1 where, bool strand);
    void  SetQueryStrand(bool strand);
    void  SetSubjStrand(bool strand);
    void  FlipStrands(void);

    void  SwapQS(void);
    
    const TCoord* GetBox(void) const;
    void  SetBox(const TCoord box [4]);

    TCoord GetMin(Uint1 where) const;
    TCoord GetMax(Uint1 where) const;
    TCoord GetQueryMin(void) const;
    TCoord GetQueryMax(void) const;
    TCoord GetSubjMin(void) const;
    TCoord GetSubjMax(void) const;
    void   SetMax(Uint1 where, TCoord pos);
    void   SetMin(Uint1 where, TCoord pos);
    void   SetQueryMin(TCoord pos);
    void   SetQueryMax(TCoord pos);
    void   SetSubjMin(TCoord pos);
    void   SetSubjMax(TCoord pos);

    TCoord GetQuerySpan(void) const;
    TCoord GetSubjSpan(void) const;

    TCoord GetStart(Uint1 where) const;
    TCoord GetStop(Uint1 where) const;
    TCoord GetQueryStart(void) const;
    TCoord GetQueryStop(void) const;
    TCoord GetSubjStart(void) const;
    TCoord GetSubjStop(void) const;
    void   SetStop(Uint1 where, TCoord pos);
    void   SetStart(Uint1 where, TCoord pos);
    void   SetQueryStart(TCoord pos);
    void   SetQueryStop(TCoord pos);
    void   SetSubjStart(TCoord pos);
    void   SetSubjStop(TCoord pos);

    void         Shift(Int4 shift_query, Int4 shift_subj);

    // 0 = query min, 1 = query max, 2 = subj min, 3 = subj max
    virtual void Modify(Uint1 point, TCoord new_pos);

    // tabular serialization
    friend NCBI_XALGOALIGN_EXPORT CNcbiOstream& operator << (CNcbiOstream& os, 
                                      const CAlignShadow& align_shadow);

    // optional alignment transcript
    typedef string TTranscript;
    const TTranscript& GetTranscript(void) const;

    static string s_RunLengthEncode(const string& in);
    static string s_RunLengthDecode(const string& in);


protected:
    
    std::pair<TId,TId>   m_Id;     // Query and subj IDs

    TCoord  m_Box [4];    // [0] = query_start, [1] = query_stop
                          // [2] = subj_start,[3] = subj_stop, all zero-based;
                          // Order in which query and subj coordinates go
                          // reflects strand.

    // tabular serialization without IDs
    virtual void   x_PartialSerialize(CNcbiOstream& os) const;

    // alignment (edit) transcript is a sequence of elementary 
    // string editing commands followed by their counts (if > 1), e.g.:M23RI5M40D3M9

    TTranscript m_Transcript;
};


template <typename T>
T round (const T& v)
{
    const T fl (floor(v));
    return v < fl + 0.5? fl: fl + 1;
}


END_NCBI_SCOPE

#endif /* ALGO_ALIGN_UTIL_ALIGN_SHADOW__HPP  */
