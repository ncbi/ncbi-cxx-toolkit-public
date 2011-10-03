#ifndef OBJTOOLS_ALNMGR__ALN_RNG_COLL_OPER__HPP
#define OBJTOOLS_ALNMGR__ALN_RNG_COLL_OPER__HPP
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
*   CAlignRangeCollection operations
*
*/


#include <corelib/ncbistd.hpp>

#include <util/align_range_coll.hpp>


BEGIN_NCBI_SCOPE


template<class TAlnRng>
void SubtractAlnRngCollections(const CAlignRangeCollection<TAlnRng>& minuend,
                               const CAlignRangeCollection<TAlnRng>& subtrahend,
                               CAlignRangeCollection<TAlnRng>& difference)
{
    typedef CAlignRangeCollection<TAlnRng> TAlnRngColl;
    _ASSERT( !subtrahend.empty() );

    /// Calc differece on the first row
    TAlnRngColl difference_on_first(minuend.GetPolicyFlags());
    {
        typename TAlnRngColl::const_iterator subtrahend_it = subtrahend.begin();
        ITERATE (typename TAlnRngColl, minuend_it, minuend) {
            SubtractOnFirst(*minuend_it,
                            subtrahend,
                            difference_on_first,
                            subtrahend_it);
        }
    }

    /// Second row
    typedef CAlignRangeCollExtender<TAlnRngColl> TAlnRngCollExt;
    TAlnRngCollExt subtrahend_ext(subtrahend);
    subtrahend_ext.UpdateIndex();

    typename TAlnRngCollExt::const_iterator subtrahend_ext_it = subtrahend_ext.begin();
    TAlnRngCollExt diff_on_first_ext(difference_on_first);
    diff_on_first_ext.UpdateIndex();
    ITERATE (typename TAlnRngCollExt,
             minuend_it,
             diff_on_first_ext) {
        SubtractOnSecond(*(minuend_it->second),
                         subtrahend_ext,
                         difference,
                         subtrahend_ext_it);
    }
}    
           

template<class Range>
struct PRangeLess
{
    typedef typename Range::position_type   position_type;
    bool    operator()(const Range& r, position_type pos)  
    { 
        return r.GetFirstToOpen() <= pos;
    }    
    bool    operator()(position_type pos, const Range& r)  
    { 
        return pos < r.GetFirstToOpen();
    }    
    bool    operator()(const Range& r1, const Range& r2)  
    { 
        return r1.GetFirstToOpen() <= r2.GetFirstToOpen();
    }    
    bool    operator()(const Range* r, position_type pos)  
    { 
        return r->GetFirstToOpen() <= pos;  
    }
    bool    operator()(position_type pos, const Range* r)  
    { 
        return pos < r->GetFirstToOpen();
    }
    bool    operator()(const Range* r1, const Range* r2)  
    { 
        return r1->GetFirstToOpen() <= r2->GetFirstToOpen();  
    }
};


template<class TAlnRng>
void SubtractOnFirst(const TAlnRng& minuend,
                     const CAlignRangeCollection<TAlnRng>& subtrahend,
                     CAlignRangeCollection<TAlnRng>& difference,
                     typename CAlignRangeCollection<TAlnRng>::const_iterator& r_it) 
{
    PRangeLess<TAlnRng> p;

    r_it = std::lower_bound(r_it,
                            subtrahend.end(),
                            minuend.GetFirstFrom(),
                            p); /* NCBI_FAKE_WARNING: WorkShop */

    if (r_it == subtrahend.end()) {
        difference.insert(minuend);
        return;
    }

    int trim; // whether and how much to trim when truncating

    trim = (r_it->GetFirstFrom() <= minuend.GetFirstFrom());

    TAlnRng r = minuend;
    TAlnRng tmp_r;

    while (1) {
        if (trim) {
            // x--------)
            //  ...---...
            trim = r_it->GetFirstToOpen() - r.GetFirstFrom();
            TrimFirstFrom(r, trim);
            if ((int) r.GetLength() <= 0) {
                return;
            }
            ++r_it;
            if (r_it == subtrahend.end()) {
                difference.insert(r);
                return;
            }
        }

        //      x------)
        // x--...
        trim = r.GetFirstToOpen() - r_it->GetFirstFrom();

        if (trim <= 0) {
            //     x----)
            // x--)
            difference.insert(r);
            return;
        } else {
            //     x----)
            // x----...
            tmp_r = r;
            TrimFirstTo(tmp_r, trim);
            difference.insert(tmp_r);
        }
    }
}


template <class TAlnRng>
struct PItLess
{
    typedef typename TAlnRng::position_type position_type;
    typedef typename CAlignRangeCollExtender<CAlignRangeCollection<TAlnRng> >::TFrom2Range::value_type value_type;
    bool operator()
        (const value_type& p,
         position_type pos)
    { 
        return p.second->GetSecondTo() < pos;  
    }    
    bool operator()
        (position_type pos,
         const value_type& p)
    { 
        return pos < p.second->GetSecondTo();  
    }    
    bool operator()
        (const value_type& p1,
         const value_type& p2)
    { 
        return p1.second->GetSecondTo() < p2.second->GetSecondTo();  
    }    
};


template<class TAlnRng>
void SubtractOnSecond(const TAlnRng& minuend,
                      const CAlignRangeCollExtender<CAlignRangeCollection<TAlnRng> >& subtrahend_ext,
                      CAlignRangeCollection<TAlnRng>& difference,
                      typename CAlignRangeCollExtender<CAlignRangeCollection<TAlnRng> >::const_iterator& r_it)
{
    if (minuend.GetSecondFrom() < 0) {
        difference.insert(minuend);
        return;
    }

    PItLess<TAlnRng> p;

    r_it = std::lower_bound(r_it,
                            subtrahend_ext.end(), 
                            minuend.GetSecondFrom(),
                            p); /* NCBI_FAKE_WARNING: WorkShop */

    if (r_it == subtrahend_ext.end()) {
        difference.insert(minuend);
        return;
    }

    int trim; // whether and how much to trim when truncating

    trim = (r_it->second->GetSecondFrom() <= minuend.GetSecondFrom());

    TAlnRng r = minuend;
    TAlnRng tmp_r;

    while (1) {
        if (trim) {
            // x--------)
            //  ...---...
            trim = r_it->second->GetSecondToOpen() - r.GetSecondFrom();
            TrimSecondFrom(r, trim);
            if ((int) r.GetLength() <= 0) {
                return;
            }
            ++r_it;
            if (r_it == subtrahend_ext.end()) {
                difference.insert(r);
                return;
            }
        }

        //      x------)
        // x--...
        trim = r.GetSecondToOpen() - r_it->second->GetSecondFrom();

        if (trim <= 0) {
            //     x----)
            // x--)
            difference.insert(r);
            return;
        } else {
            //     x----)
            // x----...
            tmp_r = r;
            TrimSecondTo(tmp_r, trim);
            difference.insert(tmp_r);
        }
    }
}


template <class TAlnRng>
void TrimFirstFrom(TAlnRng& rng, int trim)
{
    rng.SetLength(rng.GetLength() - trim);
    rng.SetFirstFrom(rng.GetFirstFrom() + trim);
    if (rng.IsDirect()) {
        rng.SetSecondFrom(rng.GetSecondFrom() + trim);
    }
}


template <class TAlnRng>
void TrimFirstTo(TAlnRng& rng, int trim)
{
    if (rng.IsReversed()) {
        rng.SetSecondFrom(rng.GetSecondFrom() +  trim);
    }
    rng.SetLength(rng.GetLength() - trim);
}

template <class TAlnRng>
void TrimSecondFrom(TAlnRng& rng, int trim)
{
    rng.SetLength(rng.GetLength() - trim);
    rng.SetSecondFrom(rng.GetSecondFrom() + trim);
    if (rng.IsDirect()) {
        rng.SetFirstFrom(rng.GetFirstFrom() + trim);
    }
}

template <class TAlnRng>
void TrimSecondTo(TAlnRng& rng, int trim)
{
    if (rng.IsReversed()) {
        rng.SetFirstFrom(rng.GetFirstFrom() + trim);
    }
    rng.SetLength(rng.GetLength() - trim);
}


END_NCBI_SCOPE

#endif // OBJTOOLS_ALNMGR__ALN_RNG_COLL_OPER__HPP
