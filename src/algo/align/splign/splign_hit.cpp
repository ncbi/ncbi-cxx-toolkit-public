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
*   Temporary code - will be part of the hit filtering library
*
*/

#include <ncbi_pch.hpp>
#include <algo/align/splign/splign_hit.hpp>
#include <algo/align/align_exception.hpp>
#include <corelib/ncbistre.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <algorithm>
#include <numeric>
#include <math.h>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CHit::CHit()
{
    x_InitDefaults();
}

void CHit::x_InitDefaults()
{
    m_Idnty = m_Score = m_Value = m_MM = m_Gaps = 0;
    m_GroupID = -1;
    m_MaxDistClustID = -1;
    m_an[0] = m_an[1] = m_an[2] = m_an[3] = 0;
    m_ai[0] = m_ai[1] = m_ai[2] = m_ai[3] = 0;
}

// constructs new hit by merging two existing
CHit::CHit(const CHit& a, const CHit& b)
{
    x_InitDefaults();

    bool bsa = a.IsStraight();
    bool bsb = b.IsStraight();
    bool bBothStraight = (bsa && bsb);
    bool bBothInverse = (!bsa && !bsb);
    if(!bBothStraight && !bBothInverse) {

        NCBI_THROW( CAlgoAlignException,
                    eInternal,
                    "Cannot merge hits with diferent strands");
    }
  
    m_an[0] = min(a.m_an[0],min(a.m_an[1],min(b.m_an[0],b.m_an[1])));
    m_an[1] = max(a.m_an[0],max(a.m_an[1],max(b.m_an[0],b.m_an[1])));
    m_an[2] = min(a.m_an[2],min(a.m_an[3],min(b.m_an[2],b.m_an[3])));
    m_an[3] = max(a.m_an[2],max(a.m_an[3],max(b.m_an[2],b.m_an[3])));
    
    // make size the same
    int n1 = m_an[1]-m_an[0];
    int n2 = m_an[3]-m_an[2];
    if(n1 != n2)
    {
        int n = min(n1,n2);
        m_an[1] = m_an[0] + n;
        m_an[3] = m_an[2] + n;
    }

    if(bBothInverse)
    {
        int k = m_an[2];
        m_an[2] = m_an[3];
        m_an[3] = k;
    }
    
    // adjust parameters
    m_Query = a.m_Query;
    m_Subj = a.m_Subj;

    m_GroupID = a.m_GroupID;

    m_Score = a.m_Score + b.m_Score;

    m_anLength[0] = abs(m_an[1]-m_an[0]) + 1;
    m_anLength[1] = abs(m_an[3]-m_an[2]) + 1;
    m_Length = max(m_anLength[0], m_anLength[1]);

    m_Value = (double(a.m_Length)*a.m_Value + double(b.m_Length)*b.m_Value) /
              (a.m_Length + b.m_Length);
    m_Idnty = (a.m_Length*a.m_Idnty + b.m_Length*b.m_Idnty) / 
             (a.m_Length + b.m_Length);
    
    m_Gaps = a.m_Gaps + b.m_Gaps;
    m_MM = a.m_MM + b.m_MM;
    
    SetEnvelop();
}


CHit::CHit(const string& strTemplate)
{
    x_InitDefaults();
    
    int nCount = 0;
    string::const_iterator ii = strTemplate.begin(),
        ii_end = strTemplate.end(), jj = ii;

    while( jj < ii_end ) {

        ii = find(jj, ii_end, '\t');
        string s (jj, ii);

        if(s == "-") {
            s = "-1";
        }
        CNcbiIstrstream ss (s.c_str());

        switch(nCount++)   {
        
        case 0:    m_Query = s;  break;
        case 1:    m_Subj = s;   break;
        case 2:    ss >> m_Idnty;  break;
        case 3:    ss >> m_Length; break;
        case 4:    ss >> m_MM;     break;
        case 5:    ss >> m_Gaps;   break;
        case 6:    ss >> m_an[0];   break;
        case 7:    ss >> m_an[1];   break;
        case 8:    ss >> m_an[2];   break;
        case 9:    ss >> m_an[3];   break;
        case 10:   ss >> m_Value;  break;
        case 11:   ss >> m_Score;  break;
        default: {
            NCBI_THROW( CAlgoAlignException,
                        eFormat,
                        "Too many fields while parsing hit line.");
            }
        }
        jj = ii + 1;
    }

    if (nCount < 12) {
      NCBI_THROW( CAlgoAlignException,
                  eFormat,
                  "One or more fields missing.");
    }
    
    // u-turn the hit if necessary
    if(m_an[0] > m_an[1] && m_an[2] > m_an[3]) {
        int n = m_an[0]; m_an[0] = m_an[1]; m_an[1] = n;
        n = m_an[2]; m_an[2] = m_an[3]; m_an[3] = n;
    }
    
    SetEnvelop();

    m_anLength[0] = m_ai[1] - m_ai[0] + 1;
    m_anLength[1] = m_ai[3] - m_ai[2] + 1;
}


CHit::CHit(const CSeq_align& sa)
{
    x_InitDefaults();

    if (sa.CheckNumRows() != 2) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "CHit::CHit passed seq-align of dimension != 2");
    }
    m_Query = sa.GetSeq_id(0).GetSeqIdString(true);
    m_Subj  = sa.GetSeq_id(1).GetSeqIdString(true);
    m_an[0] = sa.GetSeqStart(0) + 1;
    m_an[1] = sa.GetSeqStop(0) + 1;
    m_an[2] = sa.GetSeqStart(1) + 1;
    m_an[3] = sa.GetSeqStop(1) + 1;

    if (!sa.GetSegs().IsDenseg()) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "CHit::CHit passed a seq-align that isn't a dense-seg.");
    }

    const CDense_seg &ds = sa.GetSegs().GetDenseg();
    const CDense_seg::TStrands& strands = ds.GetStrands();
    const CDense_seg::TLens& lens = ds.GetLens();

    bool strand_query;
    if(strands[0] == eNa_strand_plus) {
        strand_query = true;
    }
    else if(strands[0] == eNa_strand_minus) {
        strand_query = false;
    }
    else {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Unexpected query strand reported to CHit::CHit(CSeq_align).");
    }

    bool strand_subj;
    if( strands[1] == eNa_strand_plus) {
        strand_subj = true;
    }
    else if(strands[1] == eNa_strand_minus) {
        strand_subj = false;
    }
    else {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Unexpected subject strand reported to CHit::CHit(CSeq_align).");
    }

    if (strand_query != strand_subj) {
        swap(m_an[2], m_an[3]);
    }

    sa.GetNamedScore("num_ident", m_Idnty);
    sa.GetNamedScore("bit_score", m_Score);
    sa.GetNamedScore("sum_e", m_Value);

    SetEnvelop();    

    m_anLength[0] = m_ai[1] - m_ai[0] + 1;
    m_anLength[1] = m_ai[3] - m_ai[2] + 1;
    m_Length = accumulate(lens.begin(), lens.end(), 0);

    m_Idnty = 100 * m_Idnty / m_Length; //convert # identities to % identity
}


bool CHit::IsConsistent() const
{
    return m_ai[0] <= m_ai[1] && m_ai[2] <= m_ai[3];
}


void CHit::SetEnvelop()
{
    m_ai[0] = min(m_an[0],m_an[1]);    m_ai[1] = max(m_an[0],m_an[1]);
    m_ai[2] = min(m_an[2],m_an[3]);    m_ai[3] = max(m_an[2],m_an[3]);
}


void CHit::Move(unsigned char i, int pos_new, bool prot2nucl)
{
    bool strand_plus = IsStraight();
    // for minus strand, flip the hit if necessary
    if(!strand_plus && m_an[0] > m_an[1]) {
        int n = m_an[0]; m_an[0] = m_an[1]; m_an[1] = n;
        n = m_an[2]; m_an[2] = m_an[3]; m_an[3] = n;
    }
    
    // i --> m_ai, n --> m_an
    unsigned char n = 0;
    switch(i) {
        case 0: n = 0; break; 
        case 1: n = 1; break; 
        case 2: n = strand_plus? 2: 3; break; 
        case 3: n = strand_plus? 3: 2; break; 
    }
    
    int shift = pos_new - m_an[n], shiftQ, shiftS;
    if(shift == 0) return;

    // figure out shifts on query and subject
    if(prot2nucl) {
        if(n < 2) {
            shiftQ = shift;
            shiftS = 3*shiftQ;
        }
        else {
            if(shift % 3) {
                shiftQ = (shift > 0)? (shift/3 + 1): (shift/3 - 1);
                shiftS = 3*shiftQ;
            }
            else {
                shiftQ = shift / 3;
                shiftS = shift;
            }
        }
    }
    else {
        shiftQ = shiftS = shift;
    }

    // make sure hit ratio doesn't change (todo: prot2nucl)
    if(!prot2nucl) {
        bool plus = shift >= 0;
        unsigned shift_abs = plus? shift: -shift;
        if(n < 2) {
            double D = double(shift_abs) / (m_ai[1] - m_ai[0] + 1);
            shiftQ = shift;
            shiftS = int(ceil(D*(m_ai[3] - m_ai[2] + 1)));
            if(!plus) shiftS = -shiftS;
        }
        else {
            double D = double(shift_abs) / (m_ai[3] - m_ai[2] + 1);
            shiftS = shift;
            shiftQ = int(ceil(D*(m_ai[1] - m_ai[0] + 1)));
            if(!plus) shiftQ = -shiftQ;
        }
    }


    if(strand_plus) {
        m_an[n % 2] += shiftQ; m_an[n % 2 + 2] += shiftS;
        m_ai[i % 2] += shiftQ; m_ai[i % 2 + 2] += shiftS;
    }
    else {
        switch(i) {
            case 0: m_an[0] = m_ai[0] += shiftQ;
                    m_an[2] = m_ai[3] -= shiftS;
                    break;
            case 1: m_an[1] = m_ai[1] += shiftQ;
                    m_an[3] = m_ai[2] -= shiftS;
                    break;
            case 2: m_an[3] = m_ai[2] += shiftS;
                    m_an[1] = m_ai[1] -= shiftQ;
                    break;
            case 3: m_an[2] = m_ai[3] += shiftS;
                    m_an[0] = m_ai[0] -= shiftQ;
                    break;
        }
    }
}


bool CHit::Inside(const CHit& b) const
{
    bool b0 = b.m_ai[0] <= m_ai[0] && m_ai[1] <= b.m_ai[1];
    if(!b0) return false;
    bool b1 = b.m_ai[2] <= m_ai[2] && m_ai[3] <= b.m_ai[3];
    return b1;
}

// This function must be called once after a series of Move() calls.
// Changes are made based on previous values of the parameters.
void CHit::UpdateAttributes()
{
    int nLength0 = m_Length;
    m_anLength[0] = m_ai[1] - m_ai[0] + 1;
    m_anLength[1] = m_ai[3] - m_ai[2] + 1;
    m_Length = max(m_anLength[0], m_anLength[1]);
    double dk = nLength0? double(m_Length) / nLength0: 1.0;
    m_Score *= dk;
    m_Gaps = int(m_Gaps*dk);
    m_MM = int(m_MM*dk);
    // do not change m_Value, m_Idnty
}

ostream& operator << (ostream& os, const CHit& hit) {
    const int nLen = min(hit.m_anLength[0], hit.m_anLength[1]);
    return os
       << hit.m_Query << '\t' << hit.m_Subj << '\t'
       << hit.m_Idnty   << '\t' << nLen          << '\t'
       << hit.m_MM      << '\t' << hit.m_Gaps   << '\t'
       << hit.m_an[0]    << '\t' << hit.m_an[1]   << '\t'
       << hit.m_an[2]    << '\t' << hit.m_an[3]   << '\t'
       << hit.m_Value   << '\t' << hit.m_Score;
}


bool hit_same_order::operator() (const CHit& hm, const CHit& h0) const
{
    bool bStraightM = hm.IsStraight();
    bool bStraight0 = h0.IsStraight();
    bool b0 = bStraightM && bStraight0;
    bool b1 = !bStraightM && !bStraight0;
    if(b0 || b1)
    {
        int vm [4]; copy(hm.m_an, hm.m_an + 4, vm);
        int v0 [4]; copy(h0.m_an, h0.m_an + 4, v0);
        if(b1)
        {
                // we need to change direction for one axis
            double dmin1 = 1e38, dmax1 = -dmin1;
            int i = 0;
            for(i=2; i<4; i++)
            {
                if(dmin1 > vm[i]) dmin1 = vm[i];
                   if(dmin1 > v0[i]) dmin1 = v0[i];
                if(dmax1 < vm[i]) dmax1 = vm[i];
                   if(dmax1 < v0[i]) dmax1 = v0[i];
            }
            for(i=2; i<4; i++)
            {
                vm[i] = int(-vm[i] + dmin1 + dmax1);
                v0[i] = int(-v0[i] + dmin1 + dmax1);
            }
                // now u-turn hits if necessary
            if(vm[0] > vm[1] && vm[2] > vm[3])
            {
                int n = vm[0]; vm[0] = vm[1]; vm[1] = n;
                n = vm[2]; vm[2] = vm[3]; vm[3] = n;
            }
            if(v0[0] > v0[1] && v0[2] > v0[3])
            {
                int n = v0[0]; v0[0] = v0[1]; v0[1] = n;
                n = v0[2]; v0[2] = v0[3]; v0[3] = n;
            }
        }
        double dx = .1*0.5*(abs(vm[1] - vm[0]) + abs(v0[1] - v0[0]));
        if(vm[0] > v0[1] - dx) // left bottom
            return vm[2] > v0[3] - dx? true: false;
        else
        if(vm[2] > v0[3] - dx) // left top
            return vm[0] > v0[1] - dx? true: false;
        else
        if(vm[1] < v0[0] + dx) // right bottom
            return vm[3] < v0[2] + dx? true: false;
        else
        if(vm[3] < v0[2] + dx) // right top
            return vm[1] < v0[0] + dx? true: false;
        else return false;
    }
    else
        return false;   // different strands and same order are incompatible
}

bool hit_not_same_order::operator()(const CHit& hm, const CHit& h0) const
{
    return !hit_same_order::operator() (hm,h0);
}

// Calculates distance btw. two hits on query or subject
int CHit::GetHitDistance(const CHit& h1, const CHit& h2, int nWhere)
{
    int i0 = -1, i1 = -1;
    switch(nWhere)
    {
        case 0: // query
            i0 = 0; i1 = 1;
            break;
        case 1: // subject
            i0 = 2; i1 = 3;
            break;
        default:
            return -2; // wrong parameter
    }
    if(h1.m_ai[i1] < h2.m_ai[i0])  // no intersection
        return h2.m_ai[i0] - h1.m_ai[i1];
    else
    if(h2.m_ai[i1] < h1.m_ai[i0])  // no intersection
        return h1.m_ai[i0] - h2.m_ai[i1];
    else // overlapping
        return 0;
}

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2004/05/24 16:13:57  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2004/05/10 16:33:52  kapustin
* Report unexpected strands in CHit(CSeq_align&). Calculate length and identity based on alignment data.
*
* Revision 1.6  2004/05/06 17:43:27  johnson
* CHit(CSeq_align&) now indicates minus strand via swapping subject
* coordinates (*not* query coordinates)
*
* Revision 1.5  2004/05/04 15:23:45  ucko
* Split splign code out of xalgoalign into new xalgosplign.
*
* Revision 1.4  2004/05/03 15:23:08  johnson
* Added CHit constructor that takes a CSeq_align
*
* Revision 1.3  2004/04/23 14:37:44  kapustin
* *** empty log message ***
*
* 
* ===========================================================================
*/
