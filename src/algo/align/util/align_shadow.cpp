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
*   CAlignShadow class definition
*
*/

#include <ncbi_pch.hpp>
#include "messages.hpp"
#include <algo/align/util/align_shadow.hpp>
#include <algo/align/util/algo_align_util_exceptions.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <numeric>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
// ctors/initializers

template<class TId>
CAlignShadow<TId>::CAlignShadow(void)
{
}


// prohibited except when TId == CConstRef<CSeq_id>
template<class TId>
CAlignShadow<TId>::CAlignShadow(const CSeq_align& seq_align)
{
    NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
               STATIC_MSG(eImproperTemplateArgument));
}


// explicit specialization
template<>
CAlignShadow<CConstRef<CSeq_id> >::CAlignShadow(const CSeq_align& seq_align)
{
    if (seq_align.CheckNumRows() != 2) {

        NCBI_THROW( CAlgoAlignUtilException, eBadParameter,
                    STATIC_MSG(eIncorrectSeqAlignDim) );
    }

    m_Id[0].Reset(&seq_align.GetSeq_id(0));
    m_Id[1].Reset(&seq_align.GetSeq_id(1));

    m_Box[0] = seq_align.GetSeqStart(0);
    m_Box[1] = seq_align.GetSeqStop(0);
    m_Box[2] = seq_align.GetSeqStart(1);
    m_Box[3] = seq_align.GetSeqStop(1);

    if (!seq_align.GetSegs().IsDenseg()) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eIncorrectSeqAlignDim) );
    }

    const CDense_seg &ds = seq_align.GetSegs().GetDenseg();
    const CDense_seg::TStrands& strands = ds.GetStrands();
    const CDense_seg::TLens& lens = ds.GetLens();

    if(strands[0] == eNa_strand_plus) {
        m_Strand[0] = true;
    }
    else if(strands[0] == eNa_strand_minus) {
        m_Strand[0] = false;
    }
    else {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eUnexpectedStrand));
    }

    if( strands[1] == eNa_strand_plus) {
        m_Strand[1] = true;
    }
    else if(strands[1] == eNa_strand_minus) {
        m_Strand[1] = false;
    }
    else {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eUnexpectedStrand));
    }

    m_Length = accumulate(lens.begin(), lens.end(), 0);

    double matches;
    seq_align.GetNamedScore("num_ident", matches);
    m_Identity = float(matches / m_Length);

    double score;
    seq_align.GetNamedScore("bit_score", score);
    m_Score = float(score);

    double evalue;
    seq_align.GetNamedScore("sum_e", evalue);
    m_EValue = evalue;
}


template<class TId>
CAlignShadow<TId>::CAlignShadow(const char* str)
{
    const char* p = str;
    for(; *p && isspace(*p); ++p); // skip spaces
    const char* p0 = p;
    for(; *p && !isspace(*p); ++p); // get first id
    string id1 (p0, p - p0);
    CNcbiIstrstream iss1 (id1.c_str());
    iss1 >> m_Id[0]; 

    for(; *p && isspace(*p); ++p); // skip spaces
    p0 = p;
    for(; *p && !isspace(*p); ++p); // get second id
    string id2 (p0, p - p0);
    CNcbiIstrstream iss2 (id2.c_str());
    iss2 >> m_Id[1];

    for(; *p && isspace(*p); ++p); // skip spaces
    x_InitFromString(p);
}


// explicit specialization
template<>
CAlignShadow<CConstRef<CSeq_id> >::CAlignShadow(const char* str)
{
    const char* p0 = str, *p = p0;
    for(; *p && isspace(*p); ++p); // skip spaces
    for(p0 = p; *p && !isspace(*p); ++p); // get token
    if(*p) {
        string id1 (p0, p - p0);
        m_Id[0].Reset(new CSeq_id(id1));
    }

    for(; *p && isspace(*p); ++p); // skip spaces
    for(p0 = p; *p && !isspace(*p); ++p); // get token
    if(*p) {
        string id2 (p0, p - p0);
        m_Id[1].Reset(new CSeq_id(id2));
    }

    for(; *p && isspace(*p); ++p); // skip spaces
    x_InitFromString(p);
}


template<class TId>
void CAlignShadow<TId>::x_InitFromString(const char* str)
{
    CNcbiIstrstream iss (str);
    double identity100, evalue, score;
    Uint4 a, b, c, d;
    iss >> identity100 >> m_Length >> m_Mismatches >> m_Gaps
        >> a >> b >> c >> d >> evalue >> score;
    
    if(!iss.bad()) {

        m_Identity = float(identity100 / 100.0);
        m_EValue = evalue;
        m_Score = float(score);

        m_Strand[0] = a <= b;

        if(m_Strand[0]) {
            m_Box[0] = a - 1;
            m_Box[1] = b - 1;
        }
        else {
            m_Box[0] = b - 1;
            m_Box[1] = a - 1;
        }

        m_Strand[1] = c <= d;

        if(m_Strand[1]) {
            m_Box[2] = c - 1;
            m_Box[3] = d - 1;
        }
        else {
            m_Box[2] = d - 1;
            m_Box[3] = c - 1;
        }
    }
    else {
        
        string err_msg (STATIC_MSG(eFailedToInitFromString));
        err_msg += str;
        NCBI_THROW(CAlgoAlignUtilException, eFormat, err_msg.c_str());
    }
}


//////////////////////////////////////////////////////////////////////////////
// getters and  setters

template<class TId>
const TId& CAlignShadow<TId>::GetId(unsigned char where) const
{
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eArgOutOfRange));
    }
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
void CAlignShadow<TId>::SetId(unsigned char where, const TId& id)
{
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eArgOutOfRange));
    }
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
bool  CAlignShadow<TId>::GetStrand(unsigned char where) const
{
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eArgOutOfRange));
    }
    return m_Strand[where];
}


template<class TId>
bool CAlignShadow<TId>::GetQueryStrand(void) const
{
    return m_Strand[0];
}


template<class TId>
bool CAlignShadow<TId>::GetSubjStrand(void) const
{
    return m_Strand[1];
}


template<class TId>
void CAlignShadow<TId>::SetStrand(unsigned char where, bool strand)
{
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eArgOutOfRange));
    }
    m_Strand[where] = strand;
}


template<class TId>
void CAlignShadow<TId>::SetQueryStrand(bool strand)
{
    m_Strand[0] = strand;
}


template<class TId>
void CAlignShadow<TId>::SetSubjStrand(bool strand)
{
    m_Strand[1] = strand;
}


template<class TId>
const Uint4* CAlignShadow<TId>::GetBox(void) const
{
    return m_Box;
}
 

template<class TId>
Uint4 CAlignShadow<TId>::GetMin(unsigned char where) const
{
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eArgOutOfRange));
    }
    return m_Box[where << 1];
}


template<class TId>
Uint4 CAlignShadow<TId>::GetMax(unsigned char where) const
{
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eArgOutOfRange));
    }
    return m_Box[(where << 1) | 1];
}


template<class TId>
Uint4 CAlignShadow<TId>::GetQueryMin(void) const
{
    return m_Box[0];
}


template<class TId>
Uint4 CAlignShadow<TId>::GetQueryMax(void) const
{
    return m_Box[1];
}


template<class TId>
Uint4 CAlignShadow<TId>::GetSubjMin(void) const
{
    return m_Box[2];
}


template<class TId>
Uint4 CAlignShadow<TId>::GetSubjMax(void) const
{
    return m_Box[3];
}


template<class TId>
void CAlignShadow<TId>::SetBox(const Uint4 box [4])
{
    copy(box, box + 4, m_Box);
}


template<class TId>
void CAlignShadow<TId>::SetMax(unsigned char where, Uint4 val)
{
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eArgOutOfRange));
    }
    m_Box[(where << 1) | 1] = val;
}


template<class TId>
void CAlignShadow<TId>::SetMin(unsigned char where, Uint4 val)
{
    if(0 != where && where != 1) {
        NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
                   STATIC_MSG(eArgOutOfRange));
    }
    m_Box[where << 1] = val;
}


template<class TId>
void CAlignShadow<TId>::SetQueryMin(Uint4 val)
{
    m_Box[0] = val;
}


template<class TId>
void CAlignShadow<TId>::SetQueryMax(Uint4 val)
{
     m_Box[1] = val;
}


template<class TId>
void CAlignShadow<TId>::SetSubjMin(Uint4 val)
{
    m_Box[2] = val;
}


template<class TId>
void CAlignShadow<TId>::SetSubjMax(Uint4 val)
{
    m_Box[3] = val;
}


template<class TId>
void  CAlignShadow<TId>::SetLength(Uint4 length)
{
    m_Length = length;
}


template<class TId>
Uint4 CAlignShadow<TId>::GetLength(void) const
{
    return m_Length;
}


template<class TId>
void  CAlignShadow<TId>::SetMismatches(Uint4 mismatches)
{
    m_Mismatches = mismatches;
}


template<class TId>
Uint4 CAlignShadow<TId>::GetMismatches(void) const
{
    return m_Mismatches;
}


template<class TId>
void  CAlignShadow<TId>::SetGaps(Uint4 gaps)
{
    m_Gaps = gaps;
}


template<class TId>
Uint4 CAlignShadow<TId>::GetGaps(void) const
{
    return m_Gaps;
}


template<class TId>
void CAlignShadow<TId>::SetEValue(double evalue)
{
    m_EValue = evalue;
}
 

template<class TId>
double CAlignShadow<TId>::GetEValue(void) const
{
    return m_EValue;
}


template<class TId>
void   CAlignShadow<TId>::SetIdentity(float identity)
{
    m_Identity = identity;
}


template<class TId>
float CAlignShadow<TId>::GetIdentity(void) const
{
    return m_Identity;
}


template<class TId>
void   CAlignShadow<TId>::SetScore(float score)
{
    m_Score = score;
}


template<class TId>
float CAlignShadow<TId>::GetScore(void) const
{
    return m_Score;
}


/////////////////////////////////////////////////////////////////////////////
// serialization

template<class TId>
void CAlignShadow<TId>::x_Ser(CNcbiOstream& os) const
{
    const float identity100 = 100.0f * m_Identity;
    os << identity100 << '\t'   << m_Length << '\t'
       << m_Mismatches << '\t' << m_Gaps   << '\t';
    
    if(m_Strand[0]) {
        os << m_Box[0] + 1 << '\t' << m_Box[1] + 1 << '\t';
    }
    else {
        os << m_Box[1] + 1 << '\t' << m_Box[0] + 1 << '\t';
    }

    if(m_Strand[1]) {
        os << m_Box[2] + 1 << '\t'  << m_Box[3] + 1 << '\t';
    }
    else {
        os << m_Box[3] + 1 << '\t'  << m_Box[2] + 1 << '\t';
    }

    os << m_EValue << '\t' << m_Score;
}


// explicit specialization
template<> 
CNcbiOstream& operator<< (
    CNcbiOstream& os, const CAlignShadow<CConstRef<CSeq_id> >& align_shadow)

{
    os  << align_shadow.GetId(0)->GetSeqIdString(true) << '\t'
        << align_shadow.GetId(1)->GetSeqIdString(true) << '\t';

    align_shadow.x_Ser(os);

    return os;
}


////////////////////////////////////////////////////////////////////////////
// explicit instantiations

template class CAlignShadow<Uint4>;
template class CAlignShadow<string>;
template class CAlignShadow<CConstRef<CSeq_id> >;


END_NCBI_SCOPE



/* 
 * $Log$
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
 * Move friend template definition to the header. Declare explicit specialization.
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

