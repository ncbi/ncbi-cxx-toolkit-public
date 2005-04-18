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

#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/align/util/algo_align_util_exceptions.hpp>


BEGIN_NCBI_SCOPE

template<class TId>
class CAlignShadow
{
public:

    // c'tors
    CAlignShadow(void);
    CAlignShadow(const objects::CSeq_align& seq_align);

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

    typedef TSeqPos TCoord;
    
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

    // tabular serialization
    template<class T>
    friend CNcbiOstream& operator << (CNcbiOstream& os, 
                                      const CAlignShadow<T>& align_shadow);

protected:
    
    TId     m_Id [2];     // Query and subj IDs

    TCoord  m_Box [4];    // [0] = query_start, [1] = query_stop
                          // [2] = subj_start,[3] = subj_stop, all zero-based;
                          // Order in which query and subj coordinates go
                          // reflects strand.

    // tabular serialization without IDs which are
    // template parameters and may need explicit specialization
    virtual void   x_PartialSerialize(CNcbiOstream& os) const;
};


/////////////////////////////////////////////////////////////////////////////
// ctors/initializers

template<class TId>
CAlignShadow<TId>::CAlignShadow(void)
{
    m_Box[0] = m_Box[1] = m_Box[2] = m_Box[3] = TCoord(-1);
}


template<class TId>
CAlignShadow<TId>::CAlignShadow(const objects::CSeq_align& seq_align)
{
    // prohibited except when TId == CConstRef<CSeq_id>
    NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
               "CAlignShadow: Invoked with improper template argument");
}

template<>
NCBI_XALGOALIGN_EXPORT  
CAlignShadow<CConstRef<objects::CSeq_id> >::CAlignShadow(
    const objects::CSeq_align& seq_align);

/////////////////////////////////////////////////////////////////////////////
// tabular serialization

template<class TId> 
CNcbiOstream& operator << (
    CNcbiOstream& os, const CAlignShadow<TId>& align_shadow)
{
    os  << align_shadow.GetId(0) << '\t'
        << align_shadow.GetId(1) << '\t';

    align_shadow.x_PartialSerialize(os);

    return os;
}

template<>
NCBI_XALGOALIGN_EXPORT  
CNcbiOstream& operator << (
    CNcbiOstream& os,
    const CAlignShadow<CConstRef<objects::CSeq_id> >& align_shadow );


//////////////////////////////////////////////////////////////////////////////
// getters and  setters

template<class TId>
const TId& CAlignShadow<TId>::GetId(Uint1 where) const
{

#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetId() - argument out of range");
    }
#endif

    return m_Id[where];
}


template<class TId>
const TId& CAlignShadow<TId>::GetQueryId(void) const
{
    return m_Id[0];
}


template<class TId>
const TId& CAlignShadow<TId>::GetSubjId(void) const
{
    return m_Id[1];
}


template<class TId>
void CAlignShadow<TId>::SetId(Uint1 where, const TId& id)
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetId() - argument out of range");
    }
#endif

    m_Id[where] = id;
}


template<class TId>
void CAlignShadow<TId>::SetQueryId(const TId& id)
{
    m_Id[0] = id;
}


template<class TId>
void CAlignShadow<TId>::SetSubjId(const TId& id)
{
    m_Id[1] = id;
}


template<class TId>
bool  CAlignShadow<TId>::GetStrand(Uint1 where) const
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStrand() - argument out of range");
    }
#endif

    return where == 0? m_Box[0] <= m_Box[1]: m_Box[2] <= m_Box[3];
}


template<class TId>
bool CAlignShadow<TId>::GetQueryStrand(void) const
{
    return m_Box[0] <= m_Box[1];
}


template<class TId>
bool CAlignShadow<TId>::GetSubjStrand(void) const
{
    return m_Box[2] <= m_Box[3];
}


template<class TId>
void CAlignShadow<TId>::SetStrand(Uint1 where, bool strand)
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetStrand() - argument out of range");
    }
#endif

    const TCoord undef_coord = TCoord(-1);
    const Uint1 i1 = where << 1, i2 = i1 + 1;

    if(m_Box[i1] == undef_coord || m_Box[i1] == undef_coord) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetStrand() -start and/or stop not yet set");
    }

    bool cur_strand = GetStrand(where);
    if(strand != cur_strand) {
        swap(m_Box[i1], m_Box[i2]);
    }
}


template<class TId>
void CAlignShadow<TId>::SetQueryStrand(bool strand)
{
    SetStrand(0, strand);
}


template<class TId>
void CAlignShadow<TId>::SetSubjStrand(bool strand)
{
    SetStrand(1, strand);
}


template<class TId>
const typename CAlignShadow<TId>::TCoord* CAlignShadow<TId>::GetBox(void) const
{
    return m_Box;
}
 


template<class TId>
void CAlignShadow<TId>::SetBox(const TCoord box [4])
{
    copy(box, box + 4, m_Box);
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetStart(Uint1 where)
const
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStart() - argument out of range");
    }
#endif

    return m_Box[where << 1];
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetStop(Uint1 where)
const
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStop() - argument out of range");
    }
#endif

    return m_Box[(where << 1) | 1];
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetQueryStart(void) const
{
    return m_Box[0];
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetQueryStop(void) const
{
    return m_Box[1];
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetSubjStart(void) const
{
    return m_Box[2];
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetSubjStop(void) const
{
    return m_Box[3];
}


template<class TId>
void CAlignShadow<TId>::SetStart(Uint1 where, TCoord val)
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStart() - argument out of range");
    }
#endif

    m_Box[(where << 1) | 1] = val;
}


template<class TId>
void CAlignShadow<TId>::SetStop(Uint1 where, TCoord val)
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStop() - argument out of range");
    }
#endif

    m_Box[where << 1] = val;
}


template<class TId>
void CAlignShadow<TId>::SetQueryStart(TCoord val)
{
    m_Box[0] = val;
}


template<class TId>
void CAlignShadow<TId>::SetQueryStop(TCoord val)
{
     m_Box[1] = val;
}


template<class TId>
void CAlignShadow<TId>::SetSubjStart(TCoord val)
{
    m_Box[2] = val;
}


template<class TId>
void CAlignShadow<TId>::SetSubjStop(TCoord val)
{
    m_Box[3] = val;
}


// // // // 


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetMin(Uint1 where) const
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetMin() - argument out of range");
    }
#endif

    Uint1 i1 = where << 1, i2 = i1 + 1;
    return min(m_Box[i1], m_Box[i2]);
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetMax(Uint1 where) const
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetMax() - argument out of range");
    }
#endif

    Uint1 i1 = where << 1, i2 = i1 + 1;
    return max(m_Box[i1], m_Box[i2]);
}


template<class TId>
void CAlignShadow<TId>::SetMin(Uint1 where, TCoord val)
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetMin() - argument out of range");
    }
#endif

    Uint1 i1 = where << 1, i2 = i1 + 1;

    const TCoord undef_coord = TCoord(-1);
    if(m_Box[i1] == undef_coord || m_Box[i1] == undef_coord) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetMin() - start and/or stop not yet set");
    }
    else {

        if(m_Box[i1] <= m_Box[i2] && val <= m_Box[i2]) {
            m_Box[i1] = val;
        }
        else if(val <= m_Box[i1]) {
            m_Box[i2] = val;
        }
        else {
            NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                       "CAlignShadow::SetMin() - new position is invalid");
        }
    }
}


template<class TId>
void CAlignShadow<TId>::SetMax(Uint1 where, TCoord val)
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetMax() - argument out of range");
    }
#endif

    Uint1 i1 = where << 1, i2 = i1 + 1;

    const TCoord undef_coord = TCoord(-1);
    if(m_Box[i1] == undef_coord || m_Box[i1] == undef_coord) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetMax() - start and/or stop not yet set");
    }
    else {

        if(m_Box[i1] <= m_Box[i2] && val >= m_Box[i1]) {
            m_Box[i2] = val;
        }
        else if(val >= m_Box[i2]) {
            m_Box[i1] = val;
        }
        else {
            NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                       "CAlignShadow::SetMax() - new position is invalid");
        }
    }
}


template<class TId>
void CAlignShadow<TId>::SetQueryMax(TCoord val)
{
    SetMax(0, val);
}


template<class TId>
void CAlignShadow<TId>::SetSubjMax(TCoord val)
{
    SetMax(1, val);
}

template<class TId>
void CAlignShadow<TId>::SetQueryMin(TCoord val)
{
    SetMin(0, val);
}


template<class TId>
void CAlignShadow<TId>::SetSubjMin(TCoord val)
{
    SetMin(1, val);
}


/////////////////////////////////////////////////////////////////////////////
// partial serialization

template<class TId>
void CAlignShadow<TId>::x_PartialSerialize(CNcbiOstream& os) const
{
    os << GetQueryStart() + 1 << '\t' << GetQueryStop() + 1 << '\t'
       << GetSubjStart() + 1 << '\t' << GetSubjStop() + 1;
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetQueryMin() const
{
    return min(m_Box[0], m_Box[1]);
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetSubjMin() const
{
    return min(m_Box[2], m_Box[3]);
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetQueryMax() const
{
    return max(m_Box[0], m_Box[1]);
}


template<class TId>
typename CAlignShadow<TId>::TCoord CAlignShadow<TId>::GetSubjMax() const
{
    return max(m_Box[2], m_Box[3]);
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/04/18 15:24:08  kapustin
 * Split CAlignShadow into core and blast tabular representation
 *
 * Revision 1.5  2005/03/04 17:17:43  dicuccio
 * Added export specifiers
 *
 * Revision 1.4  2005/03/03 19:33:19  ucko
 * Drop empty "inline" promises from member declarations.
 *
 * Revision 1.3  2004/12/22 21:25:15  kapustin
 * Move friend template definition to the header.
 * Declare explicit specialization.
 *
 * Revision 1.2  2004/12/22 15:50:02  kapustin
 * Drop dllexport specifier
 *
 * Revision 1.1  2004/12/21 19:59:58  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_ALIGN_SHADOW__HPP  */

