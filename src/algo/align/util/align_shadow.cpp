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
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algorithm>
#include <numeric>

#include <math.h>

BEGIN_NCBI_SCOPE

namespace {

    const CAlignShadow::TCoord g_UndefCoord (
        numeric_limits<CAlignShadow::TCoord>::max());

    typedef pair<objects::ENa_strand, objects::ENa_strand> TStrands;
}

TStrands RetrieveStdStrands(const objects::CSeq_align& seq_align)
{
    USING_SCOPE(objects);

    const CSeq_align::TSegs::TStd & std_list (seq_align.GetSegs().GetStd());
    const CStd_seg & stdseg (*(std_list.front()));
    const CStd_seg::TLoc & locs (stdseg.GetLoc());
    return TStrands(locs[0]->GetStrand(), locs[1]->GetStrand());
}


CAlignShadow::CAlignShadow(const objects::CSeq_align& seq_align, bool save_xcript)
{
    USING_SCOPE(objects);

    if (seq_align.CheckNumRows() != 2) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "Pairwise seq-align required to init align-shadow");
    }

    const bool is_denseg (seq_align.GetSegs().IsDenseg());
    const bool is_stdseg (seq_align.GetSegs().IsStd());
    if(!is_denseg && !is_stdseg) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "Must be a dense-seg or std_seg to init align-shadow");
    }

    m_Id.first.Reset(&seq_align.GetSeq_id(0));
    m_Id.second.Reset(&seq_align.GetSeq_id(1));

    ENa_strand qstrand (eNa_strand_unknown);
    ENa_strand sstrand (eNa_strand_unknown);
    
    if(is_denseg) {
        qstrand = seq_align.GetSeqStrand(0);
        sstrand = seq_align.GetSeqStrand(1);
    }
    else {
        const TStrands strands (RetrieveStdStrands(seq_align));
        qstrand = strands.first;
        sstrand = strands.second;
    }

    if(qstrand == eNa_strand_minus) {
        m_Box[1] = seq_align.GetSeqStart(0);
        m_Box[0] = seq_align.GetSeqStop(0);
    }
    else {
        m_Box[0] = seq_align.GetSeqStart(0);
        m_Box[1] = seq_align.GetSeqStop(0);
    }
        
    if(sstrand == eNa_strand_minus) {
        m_Box[3] = seq_align.GetSeqStart(1);
        m_Box[2] = seq_align.GetSeqStop(1);
    }
    else {
        m_Box[2] = seq_align.GetSeqStart(1);
        m_Box[3] = seq_align.GetSeqStop(1);
    }

    if(save_xcript) {

        if(is_denseg) {

            // compile edit transcript treating diags as matches
            const CDense_seg & ds (seq_align.GetSegs().GetDenseg());
            const CDense_seg::TStarts& starts (ds.GetStarts());
            const CDense_seg::TLens& lens (ds.GetLens());
            size_t i (0);
            ITERATE(CDense_seg::TLens, ii_lens, lens) {
                char c;
                if(starts[i] < 0) {
                    c = 'I';
                }
                else if(starts[i+1] < 0) {
                    c = 'D';
                }
                else {
                    c = 'M';
                }
                m_Transcript.push_back(c);
                if(*ii_lens > 1) {
                    m_Transcript.append(NStr::NumericToString(*ii_lens));
                }
                i += 2;
            }
        }
        else {
            NCBI_THROW(CAlgoAlignUtilException,
                       eInternal,
                       "CAlignShadow(): save_xcript mode not supported "
                       "for seq-align segments other than dense-seg.");
        }
    }
}


CAlignShadow::CAlignShadow(void)
{
    m_Box[0] = m_Box[1] = m_Box[2] = m_Box[3] = g_UndefCoord;
}


CAlignShadow::CAlignShadow(const TId& idquery, TCoord qstart, bool qstrand,
                           const TId& idsubj, TCoord sstart, bool sstrand,
                           const string& xcript)
{
    m_Id.first  = idquery;
    m_Id.second = idsubj;

    m_Box[0] = qstart;
    m_Box[2] = sstart;

    TCoord q = qstart, q0 = q, s = sstart, s0 = s;
    const int qinc (qstrand? 1: -1), sinc (sstrand? 1: -1);

    ITERATE(string, ii, xcript) {

        switch(*ii) {

        case 'M': case 'R':
            q0 = q;
            q += qinc; 
            s0 = s;
            s += sinc; 
            break;

        case 'I':
            s0 = s;
            s += sinc;
            break;

        case 'D':
            q0 = q;
            q += qinc;
            break;

        default:
            NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                       "CAlignShadow()::CAlignShadow(): "
                       "Unexpected transcript symbol found");
        }
    }

    m_Box[1] = q0;
    m_Box[3] = s0;

    m_Transcript = s_RunLengthEncode(xcript);
}


CNcbiOstream& operator << (CNcbiOstream& os, const CAlignShadow& align_shadow)
{
    USING_SCOPE(objects);
    
    os  << align_shadow.GetId(0)->AsFastaString() << '\t'
        << align_shadow.GetId(1)->AsFastaString() << '\t';
    
    align_shadow.x_PartialSerialize(os);
    
    return os;
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

    const Uint1 i1 (where << 1), i2 (i1 + 1);

    if(m_Box[i1] == g_UndefCoord || m_Box[i2] == g_UndefCoord) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::SetStrand() -start and/or stop not yet set");
    }

    const bool cur_strand (GetStrand(where));
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


void CAlignShadow::SwapQS(void)
{
    TCoord a = m_Box[0], b = m_Box[1];
    m_Box[0] = m_Box[2]; 
    m_Box[1] = m_Box[3];
    m_Box[2] = a; 
    m_Box[3] = b;
    TId id = GetQueryId();
    SetQueryId(GetSubjId());
    SetSubjId(id);
}


void CAlignShadow::FlipStrands(void) 
{
    SetQueryStrand(!GetQueryStrand());
    SetSubjStrand(!GetSubjStrand());
    if(m_Transcript.size()) {
        m_Transcript = s_RunLengthDecode(m_Transcript);
        reverse(m_Transcript.begin(), m_Transcript.end());
        m_Transcript = s_RunLengthEncode(m_Transcript);
    }
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

    const Uint1 i1 (where << 1), i2 (i1 + 1);

    if(m_Box[i1] == g_UndefCoord || m_Box[i2] == g_UndefCoord) {
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

    const Uint1 i1 (where << 1), i2 (i1 + 1);

    if(m_Box[i1] == g_UndefCoord || m_Box[i2] == g_UndefCoord) {
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


const CAlignShadow::TTranscript& CAlignShadow::GetTranscript(void) const
{
    return m_Transcript;
}


/////////////////////////////////////////////////////////////////////////////
// partial serialization

void CAlignShadow::x_PartialSerialize(CNcbiOstream& os) const
{
    os << GetQueryStart() + 1 << '\t' << GetQueryStop() + 1 << '\t'
       << GetSubjStart() + 1 << '\t' << GetSubjStop() + 1;
    if(m_Transcript.size() > 0) {
        os << '\t' << m_Transcript;
    }
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


void CAlignShadow::Modify(Uint1 point, TCoord new_pos)
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

    bool newpos_invalid = false;
    if(point <= 1) {
        if(new_pos < qmin || new_pos > qmax) {
            newpos_invalid = true;
        }
    }
    else {
        if(new_pos < smin || new_pos > smax) {
            newpos_invalid = true;
        }        
    }
    
    if(newpos_invalid) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   "CAlignShadow::Modify(): requested new position invalid"); 
    }

    const bool same_strands = qstrand == sstrand;

    TCoord q = 0, s = 0;

    if(m_Transcript.size() > 0) {

        q = GetQueryStart();
        s = GetSubjStart();
        Int1 dq = (qstrand? +1: -1), ds = (sstrand?  +1: -1);
        string xcript (s_RunLengthDecode(m_Transcript));

        size_t n1 = 0;
        bool need_trace = true;
        if(point <= 1) {
            if(q == new_pos) need_trace = false;
        }
        else {
            if(s == new_pos) need_trace = false;
        }

        const bool point_is_start((point%2) ^ (GetStrand(point/2)? 1: 0));
        if(need_trace) {
            
            char c = 0;
            ITERATE(TTranscript, ii, xcript) {

                ++n1;
                switch(c = *ii) {
                case 'M': case 'R': q += dq; s += ds; break;
                case 'D': q += dq; break;
                case 'I': s += ds; break;
                default: {
                    NCBI_THROW(CAlgoAlignUtilException, eInternal,
                             "CAlignShadow::Modify(): unexpected transcript symbol"); 
                }
                }

                if(point_is_start) {
                    if(point <= 1) {
                        if(q == new_pos) break;
                    }
                    else {
                        if(s == new_pos) break;
                    }
                }
                else {
                    if(point <= 1) {
                        if(q == new_pos + dq) break;
                    }
                    else {
                        if(s == new_pos + ds) break;
                    }
                }
            }

            if(!point_is_start && n1 > 0) {
                switch(c) {
                case 'M': case 'R': q -= dq; s -= ds; break;
                case 'D': q -= dq; break;
                case 'I': s -= ds; break;
                }
            }

        }

        switch(point) {
        case 0: // query min
            SetQueryMin(q);
            if(same_strands) { SetSubjMin(s); } else { SetSubjMax(s); }
            break;
        case 1: // query max
            SetQueryMax(q);
            if(same_strands) { SetSubjMax(s); } else { SetSubjMin(s); }
            break;
        case 2: // subj min
            SetSubjMin(s);
            if(same_strands) { SetQueryMin(q); } else { SetQueryMax(q); }
            break;
        case 3: // subj max
            SetSubjMax(s);
            if(same_strands) { SetQueryMax(q); } else { SetQueryMin(q); }
            break;
        default:
            NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                       "CAlignShadow::Modify(): Invalid end point requested."); 
        }

        if(n1 > 0) {
            if( point_is_start ) {
                xcript = xcript.substr(n1, xcript.size() - n1);
            }
            else {
                xcript.resize(n1);
            }
            m_Transcript = s_RunLengthEncode(xcript);
        }
    }
    else {

        //note that with the following logic calculated delta might exceed hit length
        //example query [0-6] subj [307-288] new_pos=307 point=2
        //leads to a disaster.  The hit becomes
        // 0	4294967295	307	307
        // In the situation like that delta must be adjusted
        TCoord qlen = 1 + qmax - qmin, slen = 1 + smax - smin;
        double k = double(qlen) / slen;
        Int4 delta_q, delta_s;
        switch(point) {

        case 0: // query min

            delta_q = new_pos - qmin;
            delta_s = Int4(round(delta_q / k));
            if( smin + delta_s > smax ) {
                delta_s = smax - smin;
            }

            SetQueryMin(qmin + delta_q);
            if(same_strands) {
                SetSubjMin(smin + delta_s);
            }
            else {
                SetSubjMax(smax - delta_s);
            }

            break;

        case 1: // query max

            delta_q = qmax - new_pos;
            delta_s = Int4(round(delta_q / k));
            if( smin + delta_s > smax ) {
                delta_s = smax - smin;
            }

            SetQueryMax(qmax - delta_q);
            if(same_strands) {
                SetSubjMax(smax - delta_s);
            }
            else {
                SetSubjMin(smin + delta_s);
            }

            break;

        case 2: // subj min

            delta_s = new_pos - smin;
            delta_q = Int4(round(delta_s * k));
            if( qmin + delta_q > qmax ) {
                delta_q = qmax - qmin;
            }

            SetSubjMin(smin + delta_s);
            if(same_strands) {
                SetQueryMin(qmin + delta_q);
            }
            else {
                SetQueryMax(qmax - delta_q);
            }

            break;

        case 3: // subj max

            delta_s = smax - new_pos;
            delta_q = Int4(round(delta_s * k));
            if( qmin + delta_q > qmax ) {
                delta_q = qmax - qmin;
            }

            SetSubjMax(smax - delta_s);
            if(same_strands) {
                SetQueryMax(qmax - delta_q);
            }
            else {
                SetQueryMin(qmin + delta_q);
            }

            break;

        default:
            NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                       "CAlignShadow::Modify(): invalid end requested"); 
        };
    }
}


string CAlignShadow::s_RunLengthEncode(const string& in)
{
    string out;
    const size_t dim (in.size());
    if(dim == 0) {
        return kEmptyStr;
    }

    const char* p (in.data());
    char c0 (p[0]);
    out.append(1, c0);
    size_t count (1);
    for(size_t k (1); k < dim; ++k) {

        char c (p[k]);
        if(isdigit(c)) { // consider already encoded
            return in;
        }

        if(c != c0) {
            c0 = c;
            if(count > 1) {
                out += NStr::NumericToString(count);
            }
            count = 1;
            out.append(1, c0);
        }
        else {
            ++count;
        }
    }

    if(count > 1) {
        out += NStr::NumericToString(count);
    }

    return out;
}


string CAlignShadow::s_RunLengthDecode(const string& in)
{
    string out;
    char C = 0;
    Uint4 N = 0;
    ITERATE(string, ii, in) {

        char c = *ii;
        if('0' <= c && c <= '9') {
            N = N * 10 + c - '0';
        }
        else {
            if(N > 0) {
                out.append(N - 1, C);
                N = 0;
            }
            out.push_back(C = c);
        }
    }
    if(N > 0) {
        out.append(N - 1, C);
    }
    return out;
}


END_NCBI_SCOPE
