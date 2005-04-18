#ifndef ALGO_ALIGN_UTIL_BLAST_TABULAR__HPP
#define ALGO_ALIGN_UTIL_BLAST_TABULAR__HPP

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
*   CBlastTabular (a.k.a "m8") format representation
*
*/

#include <corelib/ncbistd.hpp>
#include <algo/align/util/align_shadow.hpp>

BEGIN_NCBI_SCOPE


template<class TId>
class CBlastTabular: public CAlignShadow<TId>
{
public:

    typedef CAlignShadow<TId> TParent;
    typedef typename TParent::TCoord TCoord;

    // c'tors
    CBlastTabular(void) {};
    CBlastTabular(const objects::CSeq_align& seq_align);
    CBlastTabular(const char* m8);

    // getters / setters
    void   SetLength(TCoord length);
    TCoord GetLength(void) const;

    void   SetMismatches(TCoord mismatches);
    TCoord GetMismatches(void) const;

    void   SetGaps(TCoord gaps);
    TCoord GetGaps(void) const;

    void   SetEValue(double evalue);
    double GetEValue(void) const;

    void   SetIdentity(float identity);
    float  GetIdentity(void) const;

    void   SetScore(float evalue);
    float  GetScore(void) const;

protected:
    
    TCoord  m_Length;     // length of the alignment           
    TCoord  m_Mismatches; // number of mismatches              
    TCoord  m_Gaps;       // number of gap openings            
    double  m_EValue;     // e-Value                           
    float   m_Identity;   // Percent identity (ranging from 0 to 1)
    float   m_Score;      // bit score

    virtual void   x_PartialSerialize(CNcbiOstream& os) const;
    virtual void   x_PartialDeserialize(const char* m8);
};


/////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// getters and  setters

template<class TId>
void CBlastTabular<TId>::SetLength(TCoord length)
{
    m_Length = length;
}


template<class TId>
typename CBlastTabular<TId>::TCoord CBlastTabular<TId>::GetLength(void) const
{
    return m_Length;
}


template<class TId>
void CBlastTabular<TId>::SetMismatches(TCoord mismatches)
{
    m_Mismatches = mismatches;
}


template<class TId>
typename CBlastTabular<TId>::TCoord CBlastTabular<TId>::GetMismatches(void) 
const
{
    return m_Mismatches;
}


template<class TId>
void CBlastTabular<TId>::SetGaps(TCoord gaps)
{
    m_Gaps = gaps;
}


template<class TId>
typename CBlastTabular<TId>::TCoord CBlastTabular<TId>::GetGaps(void) const
{
    return m_Gaps;
}


template<class TId>
void CBlastTabular<TId>::SetEValue(double EValue)
{
    m_EValue = EValue;
}


template<class TId>
double CBlastTabular<TId>::GetEValue(void) const
{
    return m_EValue;
}


template<class TId>
void CBlastTabular<TId>::SetScore(float score)
{
    m_Score = score;
}


template<class TId>
float CBlastTabular<TId>::GetScore(void) const
{
    return m_Score;
}

template<class TId>
void CBlastTabular<TId>::SetIdentity(float identity)
{
    m_Identity = identity;
}


template<class TId>
float CBlastTabular<TId>::GetIdentity(void) const
{
    return m_Identity;
}



/////////////////////////////////////////////////////////////////////////////
// tabular serialization / deserialization

template<class TId>
void CBlastTabular<TId>::x_PartialSerialize(CNcbiOstream& os) const
{
    os << 100.0 * GetIdentity() << '\t' << GetLength() << '\t'
       << GetMismatches() << '\t' << GetGaps() << '\t'
       << TParent::GetQueryStart() + 1 << '\t' 
       << TParent::GetQueryStop() + 1 << '\t'
       << TParent::GetSubjStart() + 1 << '\t' 
       << TParent::GetSubjStop() + 1 << '\t'
       << GetEValue() << '\t' << GetScore();
}



template<class TId>
void CBlastTabular<TId>::x_PartialDeserialize(const char* m8)
{
    CNcbiIstrstream iss (m8);
    double identity100, evalue, score;
    TCoord a, b, c, d;
    iss >> identity100 >> m_Length >> m_Mismatches >> m_Gaps
        >> a >> b >> c >> d >> evalue >> score;
    
    if(iss.fail() == false) {

        m_Identity = float(identity100 / 100.0);
        m_EValue = evalue;
        m_Score = float(score);

        SetQueryStart(a - 1);
        SetQueryStop(b - 1);
        SetSubjStart(c - 1);
        SetSubjStop(d - 1);
    }
    else {
        
        NCBI_THROW(CAlgoAlignUtilException, eFormat,
                   "Failed to init from m8 string");
    }
}

/////////////////////////////////////////////////////////////////////////////
// c'tors


// prohibited when TId is not CConstRef<CSeq_id>
template<class TId>
CBlastTabular<TId>::CBlastTabular(const objects::CSeq_align& seq_align)
{
    NCBI_THROW(CAlgoAlignUtilException, eBadParameter,
               "CBlastTabular: Invoked with improper template argument");
}


template<>
NCBI_XALGOALIGN_EXPORT
CBlastTabular<CConstRef<objects::CSeq_id> >::CBlastTabular(
    const objects::CSeq_align& seq_align);


// generic version; explicit specialization provided 
// for TID == CConstRef<CSeq_id>
template<class TId>
CBlastTabular<TId>::CBlastTabular(const char* m8)
{
    const char* p = m8;
    for(; *p && isspace(*p); ++p); // skip spaces
    const char* p0 = p;
    for(; *p && !isspace(*p); ++p); // get first id
    CNcbiIstrstream iss1 (p0);
    iss1 >> TParent::m_Id[0]; 
    
    for(; *p && isspace(*p); ++p); // skip spaces
    p0 = p;
    for(; *p && !isspace(*p); ++p); // get second id
    CNcbiIstrstream iss2 (p0);
    iss2 >> TParent::m_Id[1];
    
    for(; *p && isspace(*p); ++p); // skip trailing spaces

    if(iss1.fail() || iss2.fail()) {
        NCBI_THROW(CAlgoAlignUtilException, eFormat, 
                   "CBlastTabular: Failed to init from string");
    }

    x_PartialDeserialize(p);
}


template<>
NCBI_XALGOALIGN_EXPORT
CBlastTabular<CConstRef<objects::CSeq_id> >::CBlastTabular(const char* m8);


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/18 15:23:00  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_BLAST_TABULAR__HPP  */

