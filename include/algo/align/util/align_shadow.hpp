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
END_SCOPE(objects)


template<class TId>
class NCBI_XALGOALIGN_EXPORT CAlignShadow: public CObject
{
public:

    // ctors
    CAlignShadow(void);
    CAlignShadow(const char* str);
    CAlignShadow(const objects::CSeq_align& seq_align);

    // getters / setters
    inline const TId& GetId(unsigned char where) const;
    inline const TId& GetQueryId(void) const;
    inline const TId& GetSubjId(void) const;

    inline void  SetId(unsigned char where, const TId& id);
    inline void  SetQueryId(const TId& id);
    inline void  SetSubjId(const TId& id);

    inline bool  GetStrand(unsigned char where) const;
    inline bool  GetQueryStrand(void) const;
    inline bool  GetSubjStrand(void) const;

    inline void  SetStrand(unsigned char where, bool strand);
    inline void  SetQueryStrand(bool strand);
    inline void  SetSubjStrand(bool strand);

    inline const Uint4* GetBox(void) const;
    inline Uint4 GetMin(unsigned char where) const;
    inline Uint4 GetMax(unsigned char where) const;
    inline Uint4 GetQueryMin(void) const;
    inline Uint4 GetQueryMax(void) const;
    inline Uint4 GetSubjMin(void) const;
    inline Uint4 GetSubjMax(void) const;

    inline void SetBox(const Uint4 box [4]);
    inline void SetMax(unsigned char where, Uint4 val);
    inline void SetMin(unsigned char where, Uint4 val);
    inline void SetQueryMin(Uint4 val);
    inline void SetQueryMax(Uint4 val);
    inline void SetSubjMin(Uint4 val);
    inline void SetSubjMax(Uint4 val);

    inline void  SetLength(Uint4 length);
    inline Uint4 GetLength(void) const;

    inline void  SetMismatches(Uint4 mismatches);
    inline Uint4 GetMismatches(void) const;

    inline void  SetGaps(Uint4 gaps);
    inline Uint4 GetGaps(void) const;

    inline void   SetEValue(double evalue);
    inline double GetEValue(void) const;

    inline void  SetIdentity(float identity);
    inline float GetIdentity(void) const;

    inline void  SetScore(float evalue);
    inline float GetScore(void) const;

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


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/12/21 19:59:58  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_ALIGN_SHADOW__HPP  */

