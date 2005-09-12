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
*
*/

#include <ncbi_pch.hpp>
#include <algo/align/util/align_shadow.hpp>

#include <numeric>

BEGIN_NCBI_SCOPE


CAlignShadow::CAlignShadow(const objects::CSeq_align& seq_align)
{
    USING_SCOPE(objects);

    if (seq_align.CheckNumRows() != 2) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "Pairwise seq-align required to init align-shadow");
    }

    if (seq_align.GetSegs().IsDenseg() == false) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "Must be a dense-seg to init align-shadow");
    }

    const CDense_seg &ds = seq_align.GetSegs().GetDenseg();
    const CDense_seg::TStrands& strands = ds.GetStrands();

    char query_strand = 0, subj_strand = 0;
    if(strands[0] == eNa_strand_plus || strands[0] == eNa_strand_unknown) {
        query_strand = '+';
    }
    else if(strands[0] == eNa_strand_minus) {
        query_strand = '-';
    }

    if(strands[1] == eNa_strand_plus || strands[1] == eNa_strand_unknown) {
        subj_strand = '+';
    }
    else if(strands[1] == eNa_strand_minus) {
        subj_strand = '-';
    }

    if(query_strand == 0 || subj_strand == 0) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "Unexpected strand found when initializing "
                   "align-shadow from dense-seg");
    }

    m_Id.first.Reset(&seq_align.GetSeq_id(0));
    m_Id.second.Reset(&seq_align.GetSeq_id(1));

    if(query_strand == '+') {
        m_Box[0] = seq_align.GetSeqStart(0);
        m_Box[1] = seq_align.GetSeqStop(0);
    }
    else {
        m_Box[1] = seq_align.GetSeqStart(0);
        m_Box[0] = seq_align.GetSeqStop(0);
    }

    if(subj_strand == '+') {
        m_Box[2] = seq_align.GetSeqStart(1);
        m_Box[3] = seq_align.GetSeqStop(1);
    }
    else {
        m_Box[3] = seq_align.GetSeqStart(1);
        m_Box[2] = seq_align.GetSeqStop(1);
    }
}


CNcbiOstream& operator << (CNcbiOstream& os, const CAlignShadow& align_shadow)
{
    USING_SCOPE(objects);
    
    os  << align_shadow.GetId(0)->GetSeqIdString(true) << '\t'
        << align_shadow.GetId(1)->GetSeqIdString(true) << '\t';
    
    align_shadow.x_PartialSerialize(os);
    
    return os;
}



/////////////////////////////////////////////////////////////////////////////
// ctors/initializers

CAlignShadow::CAlignShadow(void)
{
    m_Box[0] = m_Box[1] = m_Box[2] = m_Box[3] = TCoord(-1);
}


//////////////////////////////////////////////////////////////////////////////
// getters and  setters


const CAlignShadow::TId& CAlignShadow::GetId(Uint1 where) const
{
    switch(where) {
    case 0: return m_Id.first;
    case 1: return m_Id.second;
    default: {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetId() - argument out of range");
    }
    }
}


const CAlignShadow::TId& CAlignShadow::GetQueryId(void) const
{
    return m_Id.first;
}


const CAlignShadow::TId& CAlignShadow::GetSubjId(void) const
{
    return m_Id.second;
}


void CAlignShadow::SetId(Uint1 where, const TId& id)
{
    switch(where) {
    case 0: m_Id.first = id; break;
    case 1: m_Id.second = id; break;
    default: {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetId() - argument out of range");
    }
    }
}


void CAlignShadow::SetQueryId(const TId& id)
{
    m_Id.first = id;
}


void CAlignShadow::SetSubjId(const TId& id)
{
    m_Id.second = id;
}


bool CAlignShadow::GetStrand(Uint1 where) const
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStrand() - argument out of range");
    }
#endif

    return where == 0? m_Box[0] <= m_Box[1]: m_Box[2] <= m_Box[3];
}


bool CAlignShadow::GetQueryStrand(void) const
{
    return m_Box[0] <= m_Box[1];
}



bool CAlignShadow::GetSubjStrand(void) const
{
    return m_Box[2] <= m_Box[3];
}


void CAlignShadow::SetStrand(Uint1 where, bool strand)
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


void CAlignShadow::SetQueryStrand(bool strand)
{
    SetStrand(0, strand);
}


void CAlignShadow::SetSubjStrand(bool strand)
{
    SetStrand(1, strand);
}


const  CAlignShadow::TCoord* CAlignShadow::GetBox(void) const
{
    return m_Box;
}
 

void CAlignShadow::SetBox(const TCoord box [4])
{
    copy(box, box + 4, m_Box);
}


CAlignShadow::TCoord CAlignShadow::GetStart(Uint1 where) const
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStart() - argument out of range");
    }
#endif

    return m_Box[where << 1];
}


CAlignShadow::TCoord CAlignShadow::GetStop(Uint1 where) const
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStop() - argument out of range");
    }
#endif

    return m_Box[(where << 1) | 1];
}


CAlignShadow::TCoord CAlignShadow::GetQueryStart(void) const
{
    return m_Box[0];
}


CAlignShadow::TCoord CAlignShadow::GetQueryStop(void) const
{
    return m_Box[1];
}


CAlignShadow::TCoord CAlignShadow::GetSubjStart(void) const
{
    return m_Box[2];
}


CAlignShadow::TCoord CAlignShadow::GetSubjStop(void) const
{
    return m_Box[3];
}


void CAlignShadow::SetStart(Uint1 where, TCoord val)
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStart() - argument out of range");
    }
#endif

    m_Box[(where << 1) | 1] = val;
}


void CAlignShadow::SetStop(Uint1 where, TCoord val)
{
#ifdef _DEBUG
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::GetStop() - argument out of range");
    }
#endif

    m_Box[where << 1] = val;
}


void CAlignShadow::SetQueryStart(TCoord val)
{
    m_Box[0] = val;
}


void CAlignShadow::SetQueryStop(TCoord val)
{
     m_Box[1] = val;
}


void CAlignShadow::SetSubjStart(TCoord val)
{
    m_Box[2] = val;
}


void CAlignShadow::SetSubjStop(TCoord val)
{
    m_Box[3] = val;
}


// // // // 


CAlignShadow::TCoord CAlignShadow::GetMin(Uint1 where) const
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


CAlignShadow::TCoord CAlignShadow::GetMax(Uint1 where) const
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


void CAlignShadow::SetMin(Uint1 where, TCoord val)
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



void CAlignShadow::SetMax(Uint1 where, TCoord val)
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


void CAlignShadow::SetQueryMax(TCoord val)
{
    SetMax(0, val);
}


void CAlignShadow::SetSubjMax(TCoord val)
{
    SetMax(1, val);
}


void CAlignShadow::SetQueryMin(TCoord val)
{
    SetMin(0, val);
}


void CAlignShadow::SetSubjMin(TCoord val)
{
    SetMin(1, val);
}


/////////////////////////////////////////////////////////////////////////////
// partial serialization

void CAlignShadow::x_PartialSerialize(CNcbiOstream& os) const
{
    os << GetQueryStart() + 1 << '\t' << GetQueryStop() + 1 << '\t'
       << GetSubjStart() + 1 << '\t' << GetSubjStop() + 1;
}


CAlignShadow::TCoord CAlignShadow::GetQueryMin() const
{
    return min(m_Box[0], m_Box[1]);
}


CAlignShadow::TCoord CAlignShadow::GetSubjMin() const
{
    return min(m_Box[2], m_Box[3]);
}


CAlignShadow::TCoord CAlignShadow::GetQueryMax() const
{
    return max(m_Box[0], m_Box[1]);
}


CAlignShadow::TCoord CAlignShadow::GetSubjMax() const
{
    return max(m_Box[2], m_Box[3]);
}


CAlignShadow::TCoord CAlignShadow::GetQuerySpan(void) const
{
    return 1 + GetQueryMax() - GetQueryMin();
}


CAlignShadow::TCoord CAlignShadow::GetSubjSpan(void) const
{
    return 1 + GetSubjMax() - GetSubjMin();
}


void CAlignShadow::Shift(Int4 shift_query, Int4 shift_subj)
{
    m_Box[0] += shift_query;
    m_Box[1] += shift_query;
    m_Box[2] += shift_subj;
    m_Box[3] += shift_subj;
}


void CAlignShadow::Modify(Uint1 where, TCoord new_pos)
{
    TCoord qmin, qmax;
    bool qstrand;
    if(m_Box[0] < m_Box[1]) {
        qmin = m_Box[0];
        qmax = m_Box[1];
        qstrand = true;
    }
    else {
        qmin = m_Box[1];
        qmax = m_Box[0];
        qstrand = false;
    }

    TCoord smin, smax;
    bool sstrand;
    if(m_Box[2] < m_Box[3]) {
        smin = m_Box[2];
        smax = m_Box[3];
        sstrand = true;
    }
    else {
        smin = m_Box[3];
        smax = m_Box[2];
        sstrand = false;
    }

    TCoord qlen = 1 + qmax - qmin, slen = 1 + smax - smin;
    double k = double(qlen) / slen;

    Int4 delta_q, delta_s;
    switch(where) {

    case 0: // query min

        if(new_pos >= qmax) {
            goto invalid_newpos;
        }

        delta_q = new_pos - qmin;
        delta_s = Int4( delta_q / k);

        SetQueryMin(qmin + delta_q);
        if(qstrand == sstrand) {
            SetSubjMin(smin + delta_s);
        }
        else {
            SetSubjMax(smax - delta_s);
        }

        break;

    case 1: // query max

        if(new_pos <= qmin) {
            goto invalid_newpos;
        }

        delta_q = new_pos - qmax;
        delta_s = Int4(delta_q / k);

        SetQueryMax(qmax + delta_q);
        if(qstrand == sstrand) {
            SetSubjMax(smax + delta_s);
        }
        else {
            SetSubjMin(smin - delta_s);
        }

        break;

    case 2: // subj min

        if(new_pos >= smax) {
            goto invalid_newpos;
        }

        delta_s = new_pos - smin;
        delta_q = Int4(delta_s * k);

        SetSubjMin(smin + delta_s);
        if(qstrand == sstrand) {
            SetQueryMin(qmin + delta_q);
        }
        else {
            SetQueryMax(qmax - delta_q);
        }

        break;

    case 3: // subj max

        if(new_pos <= smin) {
            goto invalid_newpos;
        }

        delta_s = new_pos - smax;
        delta_q = Int4(delta_s * k);

        SetSubjMax(smax + delta_s);
        if(qstrand == sstrand) {
            SetQueryMax(qmin + delta_q);
        }
        else {
            SetQueryMin(qmax - delta_q);
        }

        break;

    default:
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::Modify(): invalid end requested"); 
    };

    return;

 invalid_newpos:
    NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
               "CAlignShadow::Modify(): requested new position invalid"); 
}


END_NCBI_SCOPE


/* 
 * $Log$
 * Revision 1.12  2005/09/12 16:23:15  kapustin
 * +Modify()
 *
 * Revision 1.11  2005/07/28 14:55:35  kapustin
 * Use std::pair instead of array to fix gcc304 complains
 *
 * Revision 1.10  2005/07/28 12:29:35  kapustin
 * Convert to non-templatized classes where causing compilation incompatibility
 *
 * Revision 1.9  2005/07/27 18:54:50  kapustin
 * When constructing from seq-align allow unknown strand (protein hits)
 *
 * Revision 1.8  2005/04/18 15:24:47  kapustin
 * Split CAlignShadow into core and blast tabular representation
 *
 * Revision 1.7  2005/03/07 19:05:45  ucko
 * Don't mark methods that should be visible elsewhere as inline.
 * (In that case, their definitions would need to be in the header.)
 *
 * Revision 1.6  2004/12/22 22:14:18  kapustin
 * Move static messages to CSimpleMessager to satisfy Solaris/forte6u2
 *
 * Revision 1.5  2004/12/22 21:33:22  kapustin
 * Uncomment AlgoAlignUtil scope
 *
 * Revision 1.4  2004/12/22 21:26:18  kapustin
 * Move friend template definition to the header. 
 * Declare explicit specialization.
 *
 * Revision 1.3  2004/12/22 15:55:53  kapustin
 * A few type conversions to make msvc happy
 *
 * Revision 1.2  2004/12/21 21:27:42  ucko
 * Don't explicitly instantiate << for CConstRef<CSeq_id>, for which it
 * has instead been specialized.
 *
 * Revision 1.1  2004/12/21 20:07:47  kapustin
 * Initial revision
 *
 */

