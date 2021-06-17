#ifndef UTIL__DIAG_RANGE_COLL__HPP
#define UTIL__DIAG_RANGE_COLL__HPP

/*  $Id$
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Collection of diagonal alignment ranges.
*
*/


#include <util/align_range.hpp>
#include <util/align_range_coll.hpp>


BEGIN_NCBI_SCOPE


class CDiagRangeCollection : public CAlignRangeCollection<CAlignRange<TSeqPos> >
{
public:
    typedef CAlignRange<TSeqPos>                 TAlnRng;
    typedef CAlignRangeCollection<TAlnRng>       TAlnRngColl;
    typedef CAlignRangeCollExtender<TAlnRngColl> TAlnRngCollExt;

    /// Constructor
    CDiagRangeCollection(int first_width = 1,
                    int second_width = 1);
    
    /// Calculate a difference
    void Diff(const TAlnRngColl& substrahend,
              TAlnRngColl& difference);

    /// Trimming methods
    void TrimFirstFrom (TAlnRng& rng, int trim);
    void TrimFirstTo   (TAlnRng& rng, int trim);
    void TrimSecondFrom(TAlnRng& rng, int trim);
    void TrimSecondTo  (TAlnRng& rng, int trim);

private:
    void x_Diff(const TAlnRng& rng,
                TAlnRngColl&   result,
                TAlnRngColl::const_iterator& r_it);

    void x_DiffSecond(const TAlnRng& rng,
                      TAlnRngColl&   result,
                      TAlnRngCollExt::const_iterator& r_it);

    struct PItLess
    {
        typedef TAlnRng::position_type position_type;
        bool operator()
            (const TAlnRngCollExt::TFrom2Range::value_type& p,
             position_type pos)
        { 
            return p.second->GetSecondTo() < pos;  
        }    
        bool operator()
            (position_type pos,
             const TAlnRngCollExt::TFrom2Range::value_type& p)
        { 
            return pos < p.second->GetSecondTo();  
        }    
        bool operator()
            (const TAlnRngCollExt::TFrom2Range::value_type& p1,
             const TAlnRngCollExt::TFrom2Range::value_type& p2)
        { 
            return p1.second->GetSecondTo() < p2.second->GetSecondTo();  
        }    
    };

    TAlnRngCollExt m_Extender;
    int m_FirstWidth;
    int m_SecondWidth;
};


inline
void CDiagRangeCollection::TrimFirstFrom(TAlnRng& rng, int trim)
{
    rng.SetLength(rng.GetLength() - trim);
    rng.SetFirstFrom(rng.GetFirstFrom() + trim * m_FirstWidth);
    if (rng.IsDirect()) {
        rng.SetSecondFrom(rng.GetSecondFrom() + trim * m_SecondWidth);
    }
}

inline
void CDiagRangeCollection::TrimFirstTo(TAlnRng& rng, int trim)
{
    if (rng.IsReversed()) {
        rng.SetSecondFrom(rng.GetSecondFrom() +  trim * m_SecondWidth);
    }
    rng.SetLength(rng.GetLength() - trim);
}

inline
void CDiagRangeCollection::TrimSecondFrom(TAlnRng& rng, int trim)
{
    rng.SetLength(rng.GetLength() - trim);
    rng.SetSecondFrom(rng.GetSecondFrom() + trim * m_SecondWidth);
    if (rng.IsDirect()) {
        rng.SetFirstFrom(rng.GetFirstFrom() + trim * m_FirstWidth);
    }
}

inline
void CDiagRangeCollection::TrimSecondTo(TAlnRng& rng, int trim)
{
    if (rng.IsReversed()) {
        rng.SetFirstFrom(rng.GetFirstFrom() + trim * m_FirstWidth);
    }
    rng.SetLength(rng.GetLength() - trim);
}


END_NCBI_SCOPE

#endif // UTIL__DIAG_RANGE_COLL__HPP
