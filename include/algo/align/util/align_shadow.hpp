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
*   CAlignShadow declaration
*
* CAlignShadow is a simplified yet convenient and compact representation of
* a pairwise sequence alignment which keeps only information about
* the alignment's borders and some basic statistics.
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_align;
    class CSeq_id;
END_SCOPE(objects)


template<class TId>
class CAlignShadow: public CObject
{
public:

    // ctors
    CAlignShadow(void);
    CAlignShadow(const char* str);
    CAlignShadow(const objects::CSeq_align& seq_align);

    // getters / setters
    const TId& GetId(unsigned char where) const;
    const TId& GetQueryId(void) const;
    const TId& GetSubjId(void) const;

    void  SetId(unsigned char where, const TId& id);
    void  SetQueryId(const TId& id);
    void  SetSubjId(const TId& id);

    bool  GetStrand(unsigned char where) const;
    bool  GetQueryStrand(void) const;
    bool  GetSubjStrand(void) const;

    void  SetStrand(unsigned char where, bool strand);
    void  SetQueryStrand(bool strand);
    void  SetSubjStrand(bool strand);

    const Uint4* GetBox(void) const;
    Uint4 GetMin(unsigned char where) const;
    Uint4 GetMax(unsigned char where) const;
    Uint4 GetQueryMin(void) const;
    Uint4 GetQueryMax(void) const;
    Uint4 GetSubjMin(void) const;
    Uint4 GetSubjMax(void) const;

    void SetBox(const Uint4 box [4]);
    void SetMax(unsigned char where, Uint4 val);
    void SetMin(unsigned char where, Uint4 val);
    void SetQueryMin(Uint4 val);
    void SetQueryMax(Uint4 val);
    void SetSubjMin(Uint4 val);
    void SetSubjMax(Uint4 val);

    void  SetLength(Uint4 length);
    Uint4 GetLength(void) const;

    void  SetMismatches(Uint4 mismatches);
    Uint4 GetMismatches(void) const;

    void  SetGaps(Uint4 gaps);
    Uint4 GetGaps(void) const;

    void   SetEValue(double evalue);
    double GetEValue(void) const;

    void  SetIdentity(float identity);
    float GetIdentity(void) const;

    void  SetScore(float evalue);
    float GetScore(void) const;

    // serialization
    template<class T>
    friend CNcbiOstream& operator<< (CNcbiOstream& os, 
                                     const CAlignShadow<T>& align_shadow);

protected:
    
    TId    m_Id [2];
    bool   m_Strand [2]; // plus = true
    Uint4  m_Box [4];    // [0] = query_min, [1] = query_max
                         // [2] = subj_min,[3] = subj_max, all zero-based
    Uint4  m_Length;     // length of the alignment           
    Uint4  m_Mismatches; // number of mismatches              
    Uint4  m_Gaps;       // number of gap openings            
    double m_EValue;     // e-Value                           
    float  m_Identity;   // Percent identity (ranging from 0 to 1)
    float  m_Score;      // bit score

    void   x_InitFromString(const char* str);
    void   x_Ser(CNcbiOstream& os) const;
};


//////////////////////////////////////////////////////////////////////////////
// 

template<class TId> 
CNcbiOstream& operator << (CNcbiOstream& os,
                          const CAlignShadow<TId>& align_shadow)
{
    os  << align_shadow.GetId(0) << '\t'
        << align_shadow.GetId(1) << '\t';

    align_shadow.x_Ser(os);

    return os;
}


template<> 
CNcbiOstream& operator << (
    CNcbiOstream& os,
    const CAlignShadow<CConstRef<objects::CSeq_id> >& align_shadow);


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/03/03 19:33:19  ucko
 * Drop empty "inline" promises from member declarations.
 *
 * Revision 1.3  2004/12/22 21:25:15  kapustin
 * Move friend template definition to the header. Declare explicit specialization.
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

