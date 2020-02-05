#ifndef OBJTOOLS_ALNMGR__ALN_RNG_COLL_LIST_OPER__HPP
#define OBJTOOLS_ALNMGR__ALN_RNG_COLL_LIST_OPER__HPP
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
#include <objtools/alnmgr/aln_rng_coll_oper.hpp>


BEGIN_NCBI_SCOPE


/// Subtract one range collection from another. Both first and second
/// sequences are checked, so that the result does not intersect with
/// any row of the minuend.
template<class TAlnRng>
void SubtractAlnRngCollections(
    const CAlignRangeCollectionList<TAlnRng>& minuend,
    const CAlignRangeCollectionList<TAlnRng>& subtrahend,
    CAlignRangeCollectionList<TAlnRng>&       difference)
{
    typedef CAlignRangeCollectionList<TAlnRng> TAlnRngColl;
    _ASSERT( !subtrahend.empty() );

    // Calc differece on the first row
    TAlnRngColl difference_on_first(minuend.GetPolicyFlags());
    for ( auto& minuend_it : minuend.GetIndexByFirst()) {
        SubtractOnFirst(*minuend_it,
                        subtrahend,
                        difference_on_first);
    }

    // Second row
    for ( auto& minuend_it : difference_on_first.GetIndexBySecond()) {
        SubtractOnSecond(*minuend_it,
                         subtrahend,
                         difference);
    }
}    
           

template<class TAlnRng>
void SubtractOnFirst(
    const TAlnRng&                                           minuend,
    const CAlignRangeCollectionList<TAlnRng>&                    subtrahend,
    CAlignRangeCollectionList<TAlnRng>&                          difference) 
{
    typename CAlignRangeCollectionList<TAlnRng>::TIndexByFirst::const_iterator r_it;
    r_it = subtrahend.GetIndexByFirst().upper_bound(minuend.GetFirstFrom());
    if ( r_it != subtrahend.GetIndexByFirst().begin() ) {
        --r_it;
        if ( (*r_it)->GetFirstToOpen() <= minuend.GetFirstFrom() ) {
            ++r_it;
        }
    }

    if (r_it == subtrahend.GetIndexByFirst().end()) {
        difference.insert(minuend);
        return;
    }

    int trim; // whether and how much to trim when truncating

    trim = (*r_it)->GetFirstFrom() <= minuend.GetFirstFrom();

    TAlnRng r = minuend;
    TAlnRng tmp_r;

    while (1) {
        if (trim) {
            // x--------)
            //  ...---...
            trim = (*r_it)->GetFirstToOpen() - r.GetFirstFrom();
            TrimFirstFrom(r, trim);
            if ((int) r.GetLength() <= 0) {
                return;
            }
            ++r_it;
            if (r_it == subtrahend.GetIndexByFirst().end()) {
                difference.insert(r);
                return;
            }
        }

        //      x------)
        // x--...
        trim = r.GetFirstToOpen() - (*r_it)->GetFirstFrom();

        if (trim <= 0) {
            //     x----)
            // x--)
            difference.insert(r);
            return;
        }
        else {
            //     x----)
            // x----...
            tmp_r = r;
            TrimFirstTo(tmp_r, trim);
            difference.insert(tmp_r);
        }
    }
}

template<class TAlnRng>
void SubtractOnSecond(
    const TAlnRng& minuend,
    const CAlignRangeCollectionList<TAlnRng>& subtrahend,
    CAlignRangeCollectionList<TAlnRng>& difference)
{
    if (minuend.GetSecondFrom() < 0) {
        difference.insert(minuend);
        return;
    }

    typename CAlignRangeCollectionList<TAlnRng>::TIndexBySecond::const_iterator r_it;
    r_it = subtrahend.GetIndexBySecond().upper_bound(minuend.GetSecondFrom());
    if ( r_it != subtrahend.GetIndexBySecond().begin() ) {
        --r_it;
        if ( (*r_it)->GetSecondToOpen() <= minuend.GetSecondFrom() ) {
            ++r_it;
        }
    }

    if (r_it == subtrahend.GetIndexBySecond().end()) {
        difference.insert(minuend);
        return;
    }

    int trim; // whether and how much to trim when truncating

    trim = (*r_it)->GetSecondFrom() <= minuend.GetSecondFrom();

    TAlnRng r = minuend;
    TAlnRng tmp_r;

    while (1) {
        if (trim) {
            // x--------)
            //  ...---...
            trim = (*r_it)->GetSecondToOpen() - r.GetSecondFrom();
            TrimSecondFrom(r, trim);
            if ((int) r.GetLength() <= 0) {
                return;
            }
            ++r_it;
            if (r_it == subtrahend.GetIndexBySecond().end()) {
                difference.insert(r);
                return;
            }
        }

        //      x------)
        // x--...
        trim = r.GetSecondToOpen() - (*r_it)->GetSecondFrom();

        if (trim <= 0) {
            //     x----)
            // x--)
            difference.insert(r);
            return;
        }
        else {
            //     x----)
            // x----...
            tmp_r = r;
            TrimSecondTo(tmp_r, trim);
            difference.insert(tmp_r);
        }
    }
}


END_NCBI_SCOPE

#endif // OBJTOOLS_ALNMGR__ALN_RNG_COLL_LIST_OPER__HPP
