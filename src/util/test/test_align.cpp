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
 * Author: Andrey Yazhuk
 *
 * File Description: 
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbimisc.hpp>

#include "test_align.hpp"

#include <util/align_range.hpp>
#include <util/align_range_coll.hpp>

#include <boost/version.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_suite.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE

typedef CAlignRange<TSignedSeqPos>  TAlignRange;
typedef CAlignRangeCollection<TAlignRange>  TAlignColl;
typedef CRange<TSignedSeqPos>   TSignedRange;


///////////////////////////////////////////////////////////////////////////////
///  TAlignRange - Helper functions

// checks that "r" satisfies design assumptions for CAlignRange
// evaluates internal invariants that must be true for any instnace of CAlignRange
void AR_Test_Invariants(const TAlignRange& r)
{
    // check from <= to_open
    BOOST_CHECK(r.GetFirstFrom() <= r.GetFirstToOpen());
    BOOST_CHECK(r.GetSecondFrom() <= r.GetSecondToOpen());

    // check to + 1 == to_open
    BOOST_CHECK_EQUAL(r.GetFirstTo() + 1, r.GetFirstToOpen());
    BOOST_CHECK_EQUAL(r.GetSecondTo() + 1, r.GetSecondToOpen());

    // check length
    BOOST_CHECK_EQUAL(r.GetLength(), r.GetFirstToOpen() - r.GetFirstFrom());
    BOOST_CHECK_EQUAL(r.GetLength(), r.GetSecondToOpen() - r.GetSecondFrom());

    // check direction
    BOOST_CHECK_EQUAL(r.IsDirect(), ! r.IsReversed());

    if(r.Empty()) {
        BOOST_CHECK(r.GetLength() <= 0);            
    } else  {
        BOOST_CHECK(r.GetLength() > 0);            

        // check mapping of the ends
        // first -> second
        TSignedSeqPos tr_from = r.GetSecondPosByFirstPos(r.GetFirstFrom());
        TSignedSeqPos tr_to = r.GetSecondPosByFirstPos(r.GetFirstTo());
        if(r.IsDirect())    {
            BOOST_CHECK_EQUAL(tr_from, r.GetSecondFrom());
            BOOST_CHECK_EQUAL(tr_to, r.GetSecondTo());
        } else {
            BOOST_CHECK_EQUAL(tr_from, r.GetSecondTo());
            BOOST_CHECK_EQUAL(tr_to, r.GetSecondFrom());
        }
        // second -> first
        tr_from = r.GetFirstPosBySecondPos(r.GetSecondFrom());
        tr_to = r.GetFirstPosBySecondPos(r.GetSecondTo());
        if(r.IsDirect())    {
            BOOST_CHECK_EQUAL(tr_from, r.GetFirstFrom());
            BOOST_CHECK_EQUAL(tr_to, r.GetFirstTo());
        } else {
            BOOST_CHECK_EQUAL(tr_from, r.GetFirstTo());
            BOOST_CHECK_EQUAL(tr_to, r.GetFirstFrom());
        }
    }
}


// checks that "r" indeed represents the specified range
void AR_Test_Identical(const TAlignRange& r, TSignedSeqPos from_1, TSignedSeqPos to_1, 
                       TSignedSeqPos from_2, TSignedSeqPos to_2, bool direct)
{
    // first check that the range is valid 
    AR_Test_Invariants(r);

    // now check that the range does represent given values
    BOOST_CHECK_EQUAL(r.GetFirstFrom(), from_1);
    BOOST_CHECK_EQUAL(r.GetFirstTo(),  to_1);
    BOOST_CHECK_EQUAL(r.GetFirstToOpen(), to_1 + 1);

    BOOST_CHECK_EQUAL(r.GetSecondFrom(), from_2);
    BOOST_CHECK_EQUAL(r.GetSecondTo(),  to_2);
    BOOST_CHECK_EQUAL(r.GetSecondToOpen(), to_2 + 1);

    BOOST_REQUIRE(to_1 - from_1 == to_2 - from_2); // test validity 

    BOOST_CHECK_EQUAL(r.GetLength(), to_1 - from_1 + 1);

    BOOST_CHECK_EQUAL(r.IsDirect(), direct);
}


void AR_Test_Direction(const TAlignRange& r, bool dir)
{
    BOOST_CHECK_EQUAL(r.IsDirect(), dir);
    BOOST_CHECK_EQUAL(r.IsReversed(), ! dir);
}

///////////////////////////////////////////////////////////////////////////////
/// Test Cases
void AR_Test_SetGet()
{
    TAlignRange r;

    // test position set/get
    r.SetFirstFrom(1);
    r.SetSecondFrom(5);
    r.SetLength(20);    
    AR_Test_Identical(r, 1, 20, 5, 24, true);

    r.SetFirstFrom(7);
    r.SetSecondFrom(13);
    r.SetLength(1);    
    AR_Test_Identical(r, 7, 7, 13, 13, true);

    r.SetLength(0);    
    AR_Test_Identical(r, 7, 6, 13, 12, true);


    // test direction set/get
    r.SetDirect(true);
    AR_Test_Direction(r, true);
    r.SetDirect(false);
    AR_Test_Direction(r, false);
    r.SetDirect();  // test default
    AR_Test_Direction(r, true);
    
    r.SetReversed(true);
    AR_Test_Direction(r, false);
    r.SetReversed(false);
    AR_Test_Direction(r, true);
    r.SetReversed();  // test default
    AR_Test_Direction(r, false);
}


void AR_Test_Constructors()
{
    // check default constructor
    {
        TAlignRange r;
        AR_Test_Identical(r, TAlignRange::GetPositionMax(), TAlignRange::GetPositionMax() - 1,
                             TAlignRange::GetPositionMax(), TAlignRange::GetPositionMax()-1, true);
    }
    {
        TAlignRange r(7, 13, 100, true);
        AR_Test_Identical(r, 7, 106, 13, 112, true);
    }
    {
        TAlignRange r(1000, 1, 100000, false);
        AR_Test_Identical(r, 1000, 100999, 1, 100000, false);

        TAlignRange r2 = r;
        //AR_Test_Identical(r2, 1000, 100999, 1, 100000, false);
    }
}


void AR_Test_Equality()
{
    {
        TAlignRange r1, r2;
        BOOST_CHECK(r1 == r2);
        BOOST_CHECK( ! (r1 != r2));
    }
    {
        TAlignRange r1(1, 13, 1000, false);
        TAlignRange r2(1, 13, 1000, false);

        BOOST_CHECK(r1 == r2);
        BOOST_CHECK( ! (r1 != r2));

        // change FirstFrom, check that not equal
        TAlignRange r3(100000, 13, 1000, false);
        BOOST_CHECK(r1 != r3);
        BOOST_CHECK( ! (r1 == r3));

        // change SecondFrom, check that not equal
        TAlignRange r4(1, 100013, 1000, false);
        BOOST_CHECK(r1 != r4);
        BOOST_CHECK( ! (r1 == r4));

        // change Length, check that not equal
        TAlignRange r5(1, 13, 101000, false);
        BOOST_CHECK(r1 != r5);
        BOOST_CHECK( ! (r1 == r5));

        // change direction, check that not equal
        TAlignRange r6(1, 13, 1000, true);
        BOOST_CHECK(r1 != r6);
        BOOST_CHECK( ! (r1 == r6));
    }
}


// check that both positions are transformed to invalid values
void AR_Test_OutOfBounds_Transform(const TAlignRange& r, TSignedSeqPos pos_1, TSignedSeqPos pos_2)
{
    BOOST_CHECK_EQUAL(r.FirstContains(pos_1), false);
    TSignedSeqPos pos = r.GetSecondPosByFirstPos(pos_1);
    BOOST_CHECK_EQUAL(pos, -1);

    BOOST_CHECK_EQUAL(r.SecondContains(pos_2), false);
    pos = r.GetFirstPosBySecondPos(pos_2);
    BOOST_CHECK_EQUAL(pos, -1);
}


void AR_Test_CoordTransform()
{  
    // AR_Test_Invariants() already checks that ends map to each other 
    // from <-> from or from <-> to
    
    // seg [10, 59], [100, 149] direct
    TAlignRange r(10, 100, 50, true);

    // a point inside 
    TSignedSeqPos pos = r.GetSecondPosByFirstPos(20);
    BOOST_CHECK_EQUAL(pos, 110);
    pos = r.GetFirstPosBySecondPos(110);
    BOOST_CHECK_EQUAL(pos, 20);
    
    // check the edges
    AR_Test_OutOfBounds_Transform(r, 9, 99);
    AR_Test_OutOfBounds_Transform(r, 60, 150);

    // check values far away
    AR_Test_OutOfBounds_Transform(r, 0, 0);
    AR_Test_OutOfBounds_Transform(r, 1000, 1000);

    // seg [10, 59], [100, 149] reversed    
    r.SetReversed();

    // a point inside 
    pos = r.GetSecondPosByFirstPos(20);
    BOOST_CHECK_EQUAL(pos, 139);
    pos = r.GetFirstPosBySecondPos(139);
    BOOST_CHECK_EQUAL(pos, 20);
    
    // check the edges
    AR_Test_OutOfBounds_Transform(r, 9, 99);
    AR_Test_OutOfBounds_Transform(r, 60, 150);

    // check values far away
    AR_Test_OutOfBounds_Transform(r, 0, 0);
    AR_Test_OutOfBounds_Transform(r, 1000, 1000);
}       

ostream& operator<<(ostream& out, const TAlignRange& r)
{
    return out << "TAlignRange [" << r.GetFirstFrom() << ", " << r.GetSecondFrom()
        << ", " << r.GetLength() << "]";
}


void AR_Test_Empty()
{
    TAlignRange r = TAlignRange::GetEmpty();
    BOOST_CHECK(r.Empty());
    BOOST_CHECK( !r.NotEmpty() );
    BOOST_CHECK_EQUAL(r.GetFirstFrom(), TAlignRange::GetEmptyFrom());
    BOOST_CHECK_EQUAL(r.GetFirstToOpen(), TAlignRange::GetEmptyToOpen());
    BOOST_CHECK_EQUAL(r.GetFirstTo(), TAlignRange::GetEmptyTo());
    BOOST_CHECK_EQUAL(r.GetSecondFrom(), TAlignRange::GetEmptyFrom());
    BOOST_CHECK_EQUAL(r.GetSecondToOpen(), TAlignRange::GetEmptyToOpen());
    BOOST_CHECK_EQUAL(r.GetSecondTo(), TAlignRange::GetEmptyTo());
    BOOST_CHECK_EQUAL(r.GetLength(), TAlignRange::GetEmptyLength());
}


void AR_Test_Clip(TSignedSeqPos clip_from, TSignedSeqPos clip_to, const TAlignRange& r, 
                  TSignedSeqPos res_from_1, TSignedSeqPos res_from_2, TSignedSeqPos res_len)
{   
     typedef CRange<TSignedSeqPos>   TRange;

    TAlignRange r1(r);
    TRange clip_1(clip_from, clip_to);
    
    TAlignRange r2 = r1.IntersectionWith(clip_1);
    r1.IntersectWith(clip_1);

    TAlignRange res_1(res_from_1, res_from_2, res_len, r.IsDirect());
    BOOST_CHECK_EQUAL(res_1, r1);
    BOOST_CHECK_EQUAL(res_1, r2);
}


void AR_Test_Intersect()
{
    {
        // seg [1001, 1100], [2001, 2100] direct    
        TAlignRange r(1001, 2001, 100, true);

        AR_Test_Clip(0, 1050, r, 1001, 2001, 50);    // left overlapping
        AR_Test_Clip(1081, 5000, r, 1081, 2081, 20); // right overlapping
        AR_Test_Clip(1021, 1060, r, 1021, 2021, 40); // clip in the middle
        AR_Test_Clip(0, 5000, r, 1001, 2001, 100);   // clip the whole range - no changes
        AR_Test_Clip(0, 10, r, 1001, 2001, 0);    // no intersection - empty
        AR_Test_Clip(1001, 2001, r, 1001, 2001, 100);   // exact clip - no changes
    }
    {
        // seg [1001, 1100], [2001, 2100] reversed    
        TAlignRange r(1001, 2001, 100, false);

        AR_Test_Clip(0, 1050, r, 1001, 2051, 50);    // left overlapping
        AR_Test_Clip(1081, 5000, r, 1081, 2001, 20); // right overlapping
        AR_Test_Clip(1021, 1060, r, 1021, 2041, 40); // clip in the middle
        AR_Test_Clip(0, 5000, r, 1001, 2001, 100);   // clip the whole range - no changes
        AR_Test_Clip(0, 10, r, 1001, 2101, 0);    // no intersection - empty
        AR_Test_Clip(1001, 2001, r, 1001, 2001, 100);   // exact clip - no changes
    }
}


void AR_Test_Combine_Abutting(const TAlignRange& r, const TAlignRange& r_a, const TAlignRange& r_res)
{
    BOOST_REQUIRE(r.IsAbutting(r_a)); // precondition
    BOOST_REQUIRE(r_a.IsAbutting(r));

    TAlignRange r_1(r);
    r_1.CombineWithAbutting(r_a);
    TAlignRange r_2 = r.CombinationWithAbutting(r_a);

    // check that the operation is symmetrical
    TAlignRange r_3(r_a);
    r_3.CombineWithAbutting(r);
    TAlignRange r_4 = r_a.CombinationWithAbutting(r);

    BOOST_CHECK_EQUAL(r_res, r_1);
    BOOST_CHECK_EQUAL(r_res, r_2);
    BOOST_CHECK_EQUAL(r_res, r_3);
    BOOST_CHECK_EQUAL(r_res, r_4);
}

/// check CAlignRange::IsAbutting(), CombineWithAbutting() and 
/// CombinationWithAbutting() functions
void AR_Test_Combine()
{
    {
        // seg [1001, 1100], [2001, 2100] direct    
        TAlignRange r(1001, 2001, 100, true);

        TAlignRange r_1(0, 0, 10, true); // r_1 is far away from r
        BOOST_CHECK_EQUAL(r.IsAbutting(r_1), false);
        BOOST_CHECK_EQUAL(r_1.IsAbutting(r), false);

        TAlignRange r_2(1050, 2050, 10, true);  // r_2 is inside r
        BOOST_CHECK_EQUAL(r.IsAbutting(r_2), false);
        BOOST_CHECK_EQUAL(r_2.IsAbutting(r), false);

        TAlignRange r_3(1101, 2101, 50, false); // close but reversed
        BOOST_CHECK_EQUAL(r.IsAbutting(r_3), false);
        BOOST_CHECK_EQUAL(r_3.IsAbutting(r), false);
    }
    // combine direct ranges
    {
        // seg [1001, 1100], [2001, 2100] direct    
        TAlignRange r(1001, 2001, 100, true);

        // [1101, 1150], [2101, 2150] direct
        TAlignRange r_a_1(1101, 2101, 50, true);
        TAlignRange res_1(1001, 2001, 150, true);
        AR_Test_Combine_Abutting(r, r_a_1, res_1);  

        // empty adjacent ranges
        TAlignRange r_a_2(1001, 2001, 0, true);
        AR_Test_Combine_Abutting(r, r_a_2, r);

        TAlignRange r_a_3(1101, 2101, 0, true);
        AR_Test_Combine_Abutting(r, r_a_3, r);
    }
    // combine reversed ranges
    {
        // seg [1001, 1100], [2001, 2100] reversed    
        TAlignRange r(1001, 2001, 100, false);

        // [1101, 1150], [1951, 2000] reversed
        TAlignRange r_a_1(1101, 1951, 50, false);
        TAlignRange res_1(1001, 1951, 150, false);
        AR_Test_Combine_Abutting(r, r_a_1, res_1); 

        // empty adjacent ranges
        TAlignRange r_a_2(1001, 2101, 0, false);
        AR_Test_Combine_Abutting(r, r_a_2, r);

        TAlignRange r_a_3(1101, 2001, 0, false);
        AR_Test_Combine_Abutting(r, r_a_3, r);
    }
    // combine empty ranges
    {
        // empty seg [1001, 1000], [2001, 2000] direct    
        TAlignRange r_1(1001, 2001, 0, true);
        AR_Test_Combine_Abutting(r_1, r_1, r_1);

        // empty seg [1001, 1000], [2001, 2000] reversed        
        TAlignRange r_2(1001, 2001, 0, false);
        AR_Test_Combine_Abutting(r_2, r_2, r_2);
    }
}


///////////////////////////////////////////////////////////////////////////////
///  TAlignColl - Helper functions

struct SARange {
    TSignedSeqPos m_FirstFrom;
    TSignedSeqPos m_SecondFrom;
    TSignedSeqPos m_Length;
    bool    m_Direct;

    bool operator==(const TAlignRange& r) const
    {
        return m_FirstFrom == r.GetFirstFrom()  &&  m_SecondFrom == r.GetSecondFrom() &&
                m_Length == r.GetLength()  &&  m_Direct == r.IsDirect();
    }
};


ostream& operator<<(ostream& out, const SARange& r)
{
    return out << "SARange  [ " << r.m_FirstFrom << ", " << r.m_SecondFrom
               << ", " <<  r.m_Length << ", " << r.m_Direct << "]";
}


void AC_AddToCollection(TAlignColl& coll, const SARange* ranges, int count)
{
    for( int i = 0; i < count;  i++ )   {
        const SARange& st = ranges[i];
        TAlignRange r(st.m_FirstFrom, st.m_SecondFrom, st.m_Length, st.m_Direct);
        coll.insert(r);
    }
}

void AC_Collection_Equals(TAlignColl& coll, const SARange* ranges, int count)
{
    TAlignColl::const_iterator it = coll.begin();
    BOOST_REQUIRE(count == (int) coll.size());

    for( int i = 0; i < count;  ++i, ++it )   {
        const SARange& st = ranges[i];
        const TAlignRange& r = *it;
        BOOST_CHECK_EQUAL(st, r);
    }
}

ostream& operator<<(ostream& out, const TAlignColl& coll)
{
    char s[32];
    sprintf(s, "%X", (unsigned int) coll.GetFlags());
    out << "CAlignRangeCollection  Flags = " << s << ", : ";

    for ( TAlignColl::const_iterator it = coll.begin();  it != coll.end();  ++it) {
        const TAlignRange& r = *it;
        out << "[" << r.GetFirstFrom() << ", " << r.GetSecondFrom() << ", "
            << r.GetLength() << ", " << (r.IsDirect() ? "true" : "false") << "]";
    }
    return out;
}

ostream& operator<<(ostream& out, const TSignedRange& r)
{
    return out << "Range [" << r.GetFrom() << ", " << r.GetTo() << "]";
}


///////////////////////////////////////////////////////////////////////////////
///  TAlignColl - Test Cases

/// insertion must be tested first as all other tests rely on insert()
void AC_Test_Insert()
{
   TAlignColl coll(0);
   BOOST_CHECK_EQUAL(coll.empty(), true);

   TAlignColl::const_iterator it = coll.insert(coll.end(), TAlignRange(501, 1501, 10, true));
   
   BOOST_CHECK(it == coll.begin());
   SARange res_1[] = { {501, 1501, 10, true} };
   AC_Collection_Equals(coll, res_1, sizeof(res_1) / sizeof(SARange));
   BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fNotValidated);
   BOOST_CHECK_EQUAL(coll.empty(), false);

   // insert empty range - this should have no effect
   it = coll.insert(coll.begin(), TAlignRange(100, 100, 0, false));

   BOOST_CHECK(it == coll.end()); 
   BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fNotValidated);

   // insert a segment in the first position
   it = coll.insert(coll.begin(), TAlignRange(1, 1001, 10, true));
   
   BOOST_CHECK(it == coll.begin());    
   SARange res_2[] = { {1, 1001, 10, true}, {501, 1501, 10, true} };
   AC_Collection_Equals(coll, res_2, sizeof(res_2) / sizeof(SARange));
   BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fNotValidated);

   it = coll.insert(coll.begin() + 1, TAlignRange(201, 1201, 10, true));

   BOOST_CHECK(it == coll.begin() + 1);       
   SARange res_3[] = { {1, 1001, 10, true}, {201, 1201, 10, true}, {501, 1501, 10, true} };
   AC_Collection_Equals(coll, res_3, sizeof(res_3) / sizeof(SARange));
   BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fNotValidated);

   // reset
   coll.clear();
   BOOST_CHECK_EQUAL(coll.GetFlags(), 0);

   coll.insert(coll.begin(), TAlignRange(1, 1001, 10, false));
   coll.insert(coll.begin(), TAlignRange(101, 1101, 20, true));

   SARange res_4[] = { {101, 1101, 20, true}, {1, 1001, 10, false} };
   AC_Collection_Equals(coll, res_4, sizeof(res_4) / sizeof(SARange));
   BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fNotValidated | TAlignColl::fMixedDir );

   coll.Validate();
   BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fInvalid | TAlignColl::fMixedDir |
                                      TAlignColl::fUnsorted);
}


void AC_Test_Constructors()
{
    // check state of an empty collection
    TAlignColl coll_1;

    BOOST_CHECK_EQUAL((int) coll_1.size(), 0);
    BOOST_CHECK_EQUAL(coll_1.empty(), true);
    BOOST_CHECK_EQUAL(coll_1.GetFlags(), TAlignColl::fKeepNormalized);

    // add ranges
    SARange ranges_1[] = { {101, 1101, 10, true}, {151, 1151, 20, true},  {201, 1201, 30, true} };  
    AC_AddToCollection(coll_1, ranges_1, sizeof(ranges_1) / sizeof(SARange));

    BOOST_CHECK_EQUAL((int) coll_1.size(), 3);
    BOOST_CHECK_EQUAL(coll_1.empty(), false);
    BOOST_CHECK_EQUAL(coll_1.GetFlags(), TAlignColl::fKeepNormalized | TAlignColl::fDirect);

    TAlignColl coll_2(coll_1);

    AC_Collection_Equals(coll_1, ranges_1, sizeof(ranges_1) / sizeof(SARange));
    AC_Collection_Equals(coll_2, ranges_1, sizeof(ranges_1) / sizeof(SARange));    
}


void AC_Test_Erase()
{
    TAlignColl coll;

    SARange ranges_1[] = { {101, 1101, 10, true}, {151, 1151, 15, true},  {201, 1201, 20, true} };  
    AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));

    // erase second segment
    TAlignColl::const_iterator it = coll.erase(coll.begin() + 1);
    
    BOOST_CHECK(it == coll.begin() + 1);
    SARange res_1[] = { {101, 1101, 10, true}, {201, 1201, 20, true} };  
    AC_Collection_Equals(coll, res_1, sizeof(res_1) / sizeof(SARange));
    
    // erase first segment
    it = coll.erase(coll.begin());

    BOOST_CHECK(it == coll.begin());
    SARange res_2[] = { {201, 1201, 20, true} };  
    AC_Collection_Equals(coll, res_2, sizeof(res_2) / sizeof(SARange));
    
    // erase the remaining one
    it = coll.erase(coll.begin());
    
    BOOST_CHECK_EQUAL((int) coll.size(), 0);
    BOOST_CHECK_EQUAL(coll.empty(), true);
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fKeepNormalized);   
}


void AC_Test_PushPop()
{
    TAlignColl coll(0);

    coll.push_back(TAlignRange(1, 1201, 10, false));
    coll.push_back(TAlignRange(101, 1101, 20, false));

    SARange ranges_1[] = { {1, 1201, 10, false}, {101, 1101, 20, false} };  
    AC_Collection_Equals(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));

    coll.pop_back();    
    coll.push_back(TAlignRange(201, 1001, 30, false));

    SARange ranges_2[] = { {1, 1201, 10, false}, {201, 1001, 30, false} };  
    AC_Collection_Equals(coll, ranges_2, sizeof(ranges_2) / sizeof(SARange));

    coll.pop_back();

    SARange ranges_3[] = { {1, 1201, 10, false} };      
    AC_Collection_Equals(coll, ranges_3, sizeof(ranges_3) / sizeof(SARange));

    coll.pop_back();
    BOOST_CHECK_EQUAL(coll.empty(), true);
}


/// Tests insert(), Validate(), Sort(), CombineAbutting() methods in a collection
/// that does not have fKeepNormalized flag set
void AC_TestDenormalized()
{   
    TAlignColl coll(0);

    // Test Insertion
    SARange ranges_1[] = { {101, 1001, 10, true}, {151, 1051, 50, true},  {111, 1011, 20, true} };  
    AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));    
    
    // the result must be identical to the input    
    AC_Collection_Equals(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fNotValidated);

    SARange ranges_2[] = { {201, 1101, 50, true}, {1, 1, 100, true} };
    AC_AddToCollection(coll, ranges_2, sizeof(ranges_2) / sizeof(SARange));    
    
    SARange res_2[] = { {101, 1001, 10, true}, {151, 1051, 50, true},  {111, 1011, 20, true},
                        {201, 1101, 50, true}, {1, 1, 100, true} };
    
    AC_Collection_Equals(coll, res_2, sizeof(res_2) / sizeof(SARange));
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fNotValidated);

    // Validate
    coll.Validate();
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fInvalid | 
                                       TAlignColl::fUnsorted);

    // Now Sorting
    coll.Sort();
    
    SARange res_3[] = { {1, 1, 100, true}, {101, 1001, 10, true}, {111, 1011, 20, true},
                        {151, 1051, 50, true}, {201, 1101, 50, true} };

    AC_Collection_Equals(coll, res_3, sizeof(res_3) / sizeof(SARange));
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fNotValidated |
                                        TAlignColl::fInvalid);

    // Validate
    coll.Validate();
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fInvalid | TAlignColl::fAbutting);

    // Merge
    coll.CombineAbutting();
    
    SARange res_4[] = { {1, 1, 100, true}, {101, 1001, 30, true}, {151, 1051, 100, true} };    
    AC_Collection_Equals(coll, res_4, sizeof(res_4) / sizeof(SARange));
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fInvalid |
                                       TAlignColl::fNotValidated);

    coll.Validate();
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect);

    // test overlapping 
    TAlignRange r_1(5000, 110, 10, true); // overlaps with the second segement on the second seq
    // this is fine (by design )
    coll.insert(r_1);
    coll.Validate();    
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect); // Perfect

    TAlignRange r_2(110, 5000, 10, true); // this should produce overlap
    coll.insert(r_1);
    coll.Validate();
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fDirect | TAlignColl::fInvalid | 
                                       TAlignColl::fOverlap);
}


void AC_Test_Remove()
{
    TAlignColl coll(0);

    // Test Insertion
    SARange ranges_1[] = { {101, 1001, 10, true}, {201, 1201, 20, false},  {301, 1301, 30, true} };    
    AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));    

    coll.erase(coll.begin() + 1);
    
    SARange res_1[] = { {101, 1001, 10, true}, {301, 1301, 30, true} };
    AC_Collection_Equals(coll, res_1, sizeof(res_1) / sizeof(SARange));
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fMixedDir | TAlignColl::fNotValidated);
    
    coll.erase(coll.begin() + 1);
    
    SARange res_2[] = { {101, 1001, 10, true} };
    AC_Collection_Equals(coll, res_2, sizeof(res_2) / sizeof(SARange));
    BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fMixedDir | TAlignColl::fNotValidated);
    
    coll.erase(coll.begin());
    
    BOOST_CHECK_EQUAL(coll.empty(), true);
    BOOST_CHECK_EQUAL(coll.GetFlags(), 0);
    
    // check thta no exceptions is thrown
    coll.erase(coll.end());
    coll.erase(coll.begin());    
}


void AC_TestDirections()
{
    SARange ranges_1[] = { {101, 1001, 10, true} };
    
    TAlignColl coll_1(0); // reset fKeepNormalized
    AC_AddToCollection(coll_1, ranges_1, sizeof(ranges_1) / sizeof(SARange));        
    coll_1.Validate();
    BOOST_CHECK_EQUAL(coll_1.GetFlags(), TAlignColl::fDirect);


    SARange ranges_2[] = { {1, 1, 10, false} };    

    TAlignColl coll_2(0); // reset fKeepNormalized    
    AC_AddToCollection(coll_2, ranges_2, sizeof(ranges_2) / sizeof(SARange));        
    BOOST_CHECK_EQUAL(coll_2.GetFlags(), TAlignColl::fReversed | TAlignColl::fNotValidated);

    TAlignRange r_1(201, 1201, 20, false);

    coll_1.insert(r_1);
    BOOST_CHECK_EQUAL(coll_1.GetFlags(), TAlignColl::fMixedDir | TAlignColl::fNotValidated);
    coll_1.Validate();
    BOOST_CHECK_EQUAL(coll_1.GetFlags(), TAlignColl::fMixedDir | TAlignColl::fInvalid);
    
    coll_2.insert(r_1);
    BOOST_CHECK_EQUAL(coll_2.GetFlags(), TAlignColl::fReversed | TAlignColl::fNotValidated);
    coll_2.Validate();
    BOOST_CHECK_EQUAL(coll_2.GetFlags(), TAlignColl::fReversed);    
}


void AC_TestNormalized_Strict()
{
    {
       TAlignColl coll(TAlignColl::fKeepNormalized);
        
        SARange ranges_1[] = { {101, 101, 20, true}, {201, 201, 50, true}, {1, 1, 10, true} };    
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));    

        // add - results should be sorted, validated and valid   
        SARange res_1[] = { {1, 1, 10, true}, {101, 101, 20, true}, {201, 201, 50, true} };
        AC_Collection_Equals(coll, res_1, sizeof(res_1) / sizeof(SARange));
        BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fKeepNormalized | TAlignColl::fDirect);
        
        // add adjacent segment - it should merge automatically
        TAlignRange r_1(121, 121, 30, true);
        coll.insert(r_1);

        SARange res_2[] = { {1, 1, 10, true}, {101, 101, 50, true}, {201, 201, 50, true} };
        AC_Collection_Equals(coll, res_2, sizeof(res_2) / sizeof(SARange));
        BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fKeepNormalized | TAlignColl::fDirect);
        
        // add a segment with wrong direction - becomes invalid
        TAlignRange r_2(1000, 1000, 10, false);
        BOOST_CHECK_THROW(coll.insert(r_2), CAlignRangeCollException);

        SARange res_3[] = { {1, 1, 10, true}, {101, 101, 50, true}, {201, 201, 50, true}, {1000, 1000, 10, false} };
        AC_Collection_Equals(coll, res_3, sizeof(res_3) / sizeof(SARange));
        BOOST_CHECK_EQUAL(coll.GetFlags(), TAlignColl::fKeepNormalized | TAlignColl::fMixedDir | TAlignColl::fInvalid);    
    }
    {
        TAlignColl coll(TAlignColl::fKeepNormalized);
        
        SARange ranges_1[] = { {1, 1, 10, true}, {201, 201, 50, true}, };    
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));    

        // add Overlapping segment - becomes invalid
        TAlignRange r(210, 500, 30, true);
        BOOST_CHECK_THROW(coll.insert(r), CAlignRangeCollException);

        SARange res[] = { {1, 1, 10, true}, {201, 201, 50, true}, {210, 500, 30, true} };
        AC_Collection_Equals(coll, res, sizeof(res) / sizeof(SARange));
        BOOST_CHECK_EQUAL(coll.GetFlags(), 
                          TAlignColl::fKeepNormalized | TAlignColl::fDirect |
                          TAlignColl::fOverlap | TAlignColl::fInvalid);    

    }
    {
        TAlignColl coll(TAlignColl::fKeepNormalized);
        
        SARange ranges_1[] = { {1, 1, 10, true}, {201, 201, 50, true}, };    
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));    

        // add Overlapping Reversed segment - both fOverlap & fMixedDir must be flagged
        TAlignRange r(210, 500, 30, false);
        BOOST_CHECK_THROW(coll.insert(r), CAlignRangeCollException);
        
        SARange res[] = { {1, 1, 10, true}, {201, 201, 50, true}, {210, 500, 30, false} };
        AC_Collection_Equals(coll, res, sizeof(res) / sizeof(SARange));
        BOOST_CHECK_EQUAL(coll.GetFlags(), 
                          TAlignColl::fKeepNormalized | TAlignColl::fMixedDir |
                          TAlignColl::fOverlap | TAlignColl::fInvalid);    
    }
}


void AC_CheckBounds(const TAlignColl& coll, const TSignedRange& r_1, const TSignedRange& /*r_2*/)
{
    BOOST_CHECK_EQUAL(coll.GetFirstRange(), r_1);
    BOOST_CHECK_EQUAL(coll.GetFirstFrom(), r_1.GetFrom());
    BOOST_CHECK_EQUAL(coll.GetFirstTo(), r_1.GetTo());
    BOOST_CHECK_EQUAL(coll.GetFirstToOpen(), r_1.GetToOpen());
}


void AC_TestBounds()
{
    SARange ranges_1[] = { {1, 1001, 10, true}, {101, 1101, 50, true}, {201, 1201, 50, true} };        
    TAlignColl coll; // normalized
    AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));    

    TSignedRange r_1(1, 250), r_2(1001, 1250);
    AC_CheckBounds(coll, r_1, r_2);

    coll.clear();
    SARange ranges_2[] = { {1, 1201, 10, false}, {101, 1101, 50, false}, {201, 1001, 50, false} };
    AC_AddToCollection(coll, ranges_2, sizeof(ranges_2) / sizeof(SARange));    
    AC_CheckBounds(coll, r_1, r_2);
}


// test CAlignRangeCollection::find(), find_2()
void AC_TestFind()
{
    {
        // test direct
        SARange ranges_1[] = { {1, 1, 10, true}, {101, 101, 50, true}, {201, 201, 50, true} };

        TAlignColl coll; // normalized
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));    

        TAlignColl::const_iterator it = coll.find(5);
        BOOST_CHECK(it == coll.begin());

        it = coll.find(120);
        BOOST_CHECK(it == coll.begin() + 1);

        // first and last positions
        it = coll.find(201); 
        BOOST_CHECK(it == coll.begin() + 2);
        it = coll.find(250); 
        BOOST_CHECK(it == coll.begin() + 2);

        // invalid positions
        it = coll.find(0); 
        BOOST_CHECK(it == coll.end());
        it = coll.find(200); 
        BOOST_CHECK(it == coll.end());
        it = coll.find(5000); 
        BOOST_CHECK(it == coll.end());
    }
    // test find with adjacent
    {
        TAlignColl coll(TAlignColl::fKeepNormalized | TAlignColl::fAllowAbutting);
        
        SARange ranges_1[] = { {101, 101, 10, true}, {111, 111, 20, true}, {132, 132, 69, true} };        
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));  
        
        TAlignColl::const_iterator it = coll.find(110);
        BOOST_CHECK(it == coll.begin());

        it = coll.find(111);
        BOOST_CHECK(it == coll.begin() + 1);
        it = coll.find(130);
        BOOST_CHECK(it == coll.begin() + 1);

        it = coll.find(131);
        BOOST_CHECK(it == coll.end()); // gap here
        it = coll.find(132);
        BOOST_CHECK(it == coll.begin() + 2);
    }
    // test reversed
    {
        TAlignColl coll;
        
        SARange ranges_1[] = { {101, 1301, 10, false}, {201, 1201, 20, false}, {301, 1101, 50, false} };        
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));  
        
        TAlignColl::const_iterator it = coll.find(105);
        BOOST_CHECK(it == coll.begin());

        // edeges of the second segment
        it = coll.find(201);
        BOOST_CHECK(it == coll.begin() + 1);
        it = coll.find(220);
        BOOST_CHECK(it == coll.begin() + 1);

        it = coll.find(350);
        BOOST_CHECK(it == coll.begin() + 2);
        it = coll.find(351);
        BOOST_CHECK(it == coll.end());         
    }
    // TODO - find in overlapped
}


void AC_Test_GetSecondByFirst()
{
    {
        TAlignColl coll;
        
        // 101-----110  201-----220  301-----350 ->
        // 1101---1110  1201---1220  1101---1150 ->
        SARange ranges_1[] = { {101, 1101, 10, true}, {201, 1201, 20, true}, {301, 1101, 50, true} };        
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));  
        
        TSignedSeqPos pos, pos_2;
        pos = coll.GetSecondPosByFirstPos(100); // out of bounds to the left
        BOOST_CHECK_EQUAL(pos, -1);   

        pos = coll.GetSecondPosByFirstPos(101);
        BOOST_CHECK_EQUAL(pos, 1101);

        pos = coll.GetSecondPosByFirstPos(220);
        BOOST_CHECK_EQUAL(pos, 1220);

        pos = coll.GetSecondPosByFirstPos(351); // out of bounds to the right
        BOOST_CHECK_EQUAL(pos, -1);

        // use "seek" option - eForward, eRight
        pos = coll.GetSecondPosByFirstPos(0, TAlignColl::eForward);
        pos_2 = coll.GetSecondPosByFirstPos(0, TAlignColl::eRight);    
        BOOST_CHECK_EQUAL(pos, 1101);  // position closest to the right
        BOOST_CHECK_EQUAL(pos_2, pos);

        pos = coll.GetSecondPosByFirstPos(150, TAlignColl::eForward);
        pos_2 = coll.GetSecondPosByFirstPos(150, TAlignColl::eRight);    
        BOOST_CHECK_EQUAL(pos, 1201);  // position closest to the right
        BOOST_CHECK_EQUAL(pos_2, pos);

        // eBackwards, eLeft
        pos = coll.GetSecondPosByFirstPos(150, TAlignColl::eBackwards);
        pos_2 = coll.GetSecondPosByFirstPos(150, TAlignColl::eLeft);    
        BOOST_CHECK_EQUAL(pos, 1110);  // position closest to the left
        BOOST_CHECK_EQUAL(pos_2, pos);

        pos = coll.GetSecondPosByFirstPos(5000, TAlignColl::eBackwards);
        pos_2 = coll.GetSecondPosByFirstPos(5000, TAlignColl::eLeft);    
        BOOST_CHECK_EQUAL(pos, 1150);  // position closest to the left
        BOOST_CHECK_EQUAL(pos_2, pos);
    }
    // reversed
    {
        TAlignColl coll;
        
        //    1------10  51-----70 ->
        // <- 260---251  220---201
        SARange ranges_1[] = { {1, 251, 10, false}, {51, 201, 20, false}};        
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));  
        
        TSignedSeqPos pos, pos_2;
        pos = coll.GetSecondPosByFirstPos(0); // out of bounds to the left
        BOOST_CHECK_EQUAL(pos, -1);

        pos = coll.GetSecondPosByFirstPos(71); // out of bounds to the right
        BOOST_CHECK_EQUAL(pos, -1);

        // inside a segment
        pos = coll.GetSecondPosByFirstPos(1);
        BOOST_CHECK_EQUAL(pos, 260);
        pos = coll.GetSecondPosByFirstPos(10);
        BOOST_CHECK_EQUAL(pos, 251);

        pos = coll.GetSecondPosByFirstPos(61);
        BOOST_CHECK_EQUAL(pos, 210);

        // use "seek" option - eForward, eRight
        pos = coll.GetSecondPosByFirstPos(0, TAlignColl::eForward);
        pos_2 = coll.GetSecondPosByFirstPos(0, TAlignColl::eRight);    
        BOOST_CHECK_EQUAL(pos, 260);  // position closest to the right
        BOOST_CHECK_EQUAL(pos_2, pos);

        pos = coll.GetSecondPosByFirstPos(30, TAlignColl::eForward);
        pos_2 = coll.GetSecondPosByFirstPos(30, TAlignColl::eRight);    
        BOOST_CHECK_EQUAL(pos, 220);  // position closest to the right
        BOOST_CHECK_EQUAL(pos_2, pos);

        // eBackwards, eLeft
        pos = coll.GetSecondPosByFirstPos(30, TAlignColl::eBackwards);
        pos_2 = coll.GetSecondPosByFirstPos(30, TAlignColl::eLeft);    
        BOOST_CHECK_EQUAL(pos, 251);  // position closest to the left
        BOOST_CHECK_EQUAL(pos_2, pos);

        pos = coll.GetSecondPosByFirstPos(5000, TAlignColl::eBackwards);
        pos_2 = coll.GetSecondPosByFirstPos(5000, TAlignColl::eLeft);    
        BOOST_CHECK_EQUAL(pos, 201);  // position closest to the left
        BOOST_CHECK_EQUAL(pos_2, pos);
    }
}


void AC_Test_GetFirstBySecond()
{
    {
        TAlignColl coll;
        
        // 101-----110  201-----220  301-----350 ->
        // 1101---1110  1201---1220  1301---1350 ->
        SARange ranges_1[] = { {101, 1101, 10, true}, {201, 1201, 20, true}, {301, 1301, 50, true} };        
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));  
        
        TSignedSeqPos pos, pos_2;
        pos = coll.GetFirstPosBySecondPos(1100); // out of bounds to the left
        BOOST_CHECK_EQUAL(pos, -1);   

        pos = coll.GetFirstPosBySecondPos(1351); // out of bounds to the right
        BOOST_CHECK_EQUAL(pos, -1);

        pos = coll.GetFirstPosBySecondPos(1201);
        BOOST_CHECK_EQUAL(pos, 201);

        pos = coll.GetFirstPosBySecondPos(1220);
        BOOST_CHECK_EQUAL(pos, 220);

        pos = coll.GetFirstPosBySecondPos(1310);
        BOOST_CHECK_EQUAL(pos, 310);
        
        // use "seek" option - eForward, eRight
        pos = coll.GetFirstPosBySecondPos(0, TAlignColl::eForward);
        pos_2 = coll.GetFirstPosBySecondPos(0, TAlignColl::eRight);    
        BOOST_CHECK_EQUAL(pos, 101);  // position closest to the right
        BOOST_CHECK_EQUAL(pos_2, 101);

        pos = coll.GetFirstPosBySecondPos(1150, TAlignColl::eForward);
        pos_2 = coll.GetFirstPosBySecondPos(1150, TAlignColl::eRight);    
        BOOST_CHECK_EQUAL(pos, 201);  // position closest to the right
        BOOST_CHECK_EQUAL(pos_2, 201);

        // eBackwards, eLeft
        pos = coll.GetFirstPosBySecondPos(1150, TAlignColl::eBackwards);
        pos_2 = coll.GetFirstPosBySecondPos(1150, TAlignColl::eLeft);    
        BOOST_CHECK_EQUAL(pos, 110);  // position closest to the left
        BOOST_CHECK_EQUAL(pos_2, 110);

        pos = coll.GetFirstPosBySecondPos(5000, TAlignColl::eBackwards);
        pos_2 = coll.GetFirstPosBySecondPos(5000, TAlignColl::eLeft);    
        BOOST_CHECK_EQUAL(pos, 350);  // position closest to the left
        BOOST_CHECK_EQUAL(pos_2, 350);
    }
    // reversed
    {
        TAlignColl coll;
        
        //    1------10  51-----70 ->
        // <- 260---251  220---201
        SARange ranges_1[] = { {1, 251, 10, false}, {51, 201, 20, false}};        
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange));  
        
        TSignedSeqPos pos, pos_2;
        pos = coll.GetFirstPosBySecondPos(200); // out of bounds to the left
        BOOST_CHECK_EQUAL(pos, -1);

        pos = coll.GetFirstPosBySecondPos(261); // out of bounds to the right
        BOOST_CHECK_EQUAL(pos, -1);

        // inside a segment
        pos = coll.GetFirstPosBySecondPos(201);
        BOOST_CHECK_EQUAL(pos, 70);
        pos = coll.GetFirstPosBySecondPos(220);
        BOOST_CHECK_EQUAL(pos, 51);

        pos = coll.GetFirstPosBySecondPos(255);
        BOOST_CHECK_EQUAL(pos, 6);

        // use "seek" option - eForward, eLeft
        pos = coll.GetFirstPosBySecondPos(0, TAlignColl::eForward);
        pos_2 = coll.GetFirstPosBySecondPos(0, TAlignColl::eLeft);    
        BOOST_CHECK_EQUAL(pos, 70);  // position closest to the left
        BOOST_CHECK_EQUAL(pos_2, pos);

        pos = coll.GetFirstPosBySecondPos(230, TAlignColl::eForward);
        pos_2 = coll.GetFirstPosBySecondPos(230, TAlignColl::eLeft);    
        BOOST_CHECK_EQUAL(pos, 10);  // position closest to the left
        BOOST_CHECK_EQUAL(pos_2, pos);

        // eBackwards, eRight
        pos = coll.GetFirstPosBySecondPos(230, TAlignColl::eBackwards);
        pos_2 = coll.GetFirstPosBySecondPos(230, TAlignColl::eRight);    
        BOOST_CHECK_EQUAL(pos, 51);  // position closest to the right
        BOOST_CHECK_EQUAL(pos_2, pos);

        pos = coll.GetFirstPosBySecondPos(5000, TAlignColl::eBackwards);
        pos_2 = coll.GetFirstPosBySecondPos(5000, TAlignColl::eRight);    
        BOOST_CHECK_EQUAL(pos, 1);  // position closest to the right
        BOOST_CHECK_EQUAL(pos_2, pos);
    }
}


void AC_Test_Extender()
{
    {
        TAlignColl coll;

        SARange ranges_1[] = { {101, 1101, 10, true}, {201, 1201, 20, true}, {301, 1301, 30, true} };        
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange)); 

        typedef CAlignRangeCollExtender<TAlignColl> TExtender;
        TExtender ext(coll);

        BOOST_CHECK_EQUAL(ext.GetSecondFrom(), 1101);
        BOOST_CHECK_EQUAL(ext.GetSecondTo(), 1330);
        BOOST_CHECK_EQUAL(ext.GetSecondToOpen(), 1331);
        BOOST_CHECK_EQUAL(ext.GetSecondLength(), 230);    
        BOOST_CHECK_EQUAL(ext.GetSecondRange(), CRange<TSignedSeqPos>(1101, 1330));    
        
        TExtender::const_iterator it = ext.FindOnSecond(1100); // out of range on the left
        BOOST_CHECK(it == ext.end());

        it = ext.FindOnSecond(1331); // out of range on the right
        BOOST_CHECK(it == ext.end());

        it = ext.FindOnSecond(1200); // gap
        BOOST_CHECK(it == ext.end());
        it = ext.FindOnSecond(1221); // gap
        BOOST_CHECK(it == ext.end());


        it = ext.FindOnSecond(1101); // first position on the first segment
        const TAlignRange* r = it->second;
        BOOST_CHECK_EQUAL(*r, *coll.begin());

        it = ext.FindOnSecond(1330); // last position on the last segment
        r = it->second;
        BOOST_CHECK_EQUAL(*r, *(coll.begin() + 2));

        it = ext.FindOnSecond(1210); // in the middle of the second segment
        r = it->second;    
        BOOST_CHECK_EQUAL(*r, *(coll.begin() + 1));
    }
    // now test relaxed collection unsorted and with overlaps
    {
        TAlignColl coll(0);

        SARange ranges_1[] = { {111, 1111, 30, true}, {101, 1101, 20, true}, {1, 1001, 10, true}, };        
        AC_AddToCollection(coll, ranges_1, sizeof(ranges_1) / sizeof(SARange)); 

        typedef CAlignRangeCollExtender<TAlignColl> TExtender;
        TExtender ext(coll);

        BOOST_CHECK_EQUAL(ext.GetSecondFrom(), 1001);
        BOOST_CHECK_EQUAL(ext.GetSecondTo(), 1140);
        BOOST_CHECK_EQUAL(ext.GetSecondToOpen(), 1141);
        BOOST_CHECK_EQUAL(ext.GetSecondLength(), 140);    
        BOOST_CHECK_EQUAL(ext.GetSecondRange(), CRange<TSignedSeqPos>(1001, 1140));    
        
        TExtender::const_iterator it = ext.FindOnSecond(1000); // out of range on the left
        BOOST_CHECK(it == ext.end());

        it = ext.FindOnSecond(1141); // out of range on the right
        BOOST_CHECK(it == ext.end());

        it = ext.FindOnSecond(1011); // gap
        BOOST_CHECK(it == ext.end());
        it = ext.FindOnSecond(1100); // gap
        BOOST_CHECK(it == ext.end());


        it = ext.FindOnSecond(1001); 
        const TAlignRange* r = it->second;
        BOOST_CHECK_EQUAL(r->GetFirstFrom(), 1);

        it = ext.FindOnSecond(1105);
        r = it->second;
        BOOST_CHECK_EQUAL(r->GetFirstFrom(), 101);

        it = ext.FindOnSecond(1111); 
        r = it->second;    
        BOOST_CHECK_EQUAL(r->GetFirstFrom(), 101); // the first of overlapped segments
        
        it++;
        BOOST_CHECK(it != ext.end());
        r = it->second; 
        BOOST_CHECK_EQUAL(r->GetFirstFrom(), 111); // the next

        it = ext.FindOnSecond(1130);
        r = it->second;
        BOOST_CHECK_EQUAL(r->GetFirstFrom(), 111);
    }
}


END_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
/// main entry point for tests

std::ofstream out("res.xml"); //TODO


test_suite* init_unit_test_suite(int /*argc*/, char * /*argv*/[])
{    
#if BOOST_VERSION >= 103300
    typedef boost::unit_test_framework::unit_test_log_t TLog; 
    TLog& log = boost::unit_test_framework::unit_test_log;
    log.set_stream(out);    
    log.set_format(boost::unit_test_framework::XML);
    log.set_threshold_level(boost::unit_test_framework::log_test_suites);
#else
    typedef boost::unit_test_framework::unit_test_log TLog; 
    TLog& log = TLog::instance();
    log.set_log_stream(out);    
    log.set_log_format("XML");
    log.set_log_threshold_level(boost::unit_test_framework::log_test_suites);
#endif

    //boost::unit_test_framework::unit_test_result::set_report_format("XML");
    test_suite* test
        = BOOST_TEST_SUITE("UTIL Align Range, Collection Unit Test.");

    test_suite* ar_suite = BOOST_TEST_SUITE("CAlignRange Unit Test");
    ar_suite->add(BOOST_TEST_CASE(ncbi::AR_Test_Empty));
    ar_suite->add(BOOST_TEST_CASE(ncbi::AR_Test_SetGet));
    ar_suite->add(BOOST_TEST_CASE(ncbi::AR_Test_Constructors));
    ar_suite->add(BOOST_TEST_CASE(ncbi::AR_Test_Equality));
    ar_suite->add(BOOST_TEST_CASE(ncbi::AR_Test_CoordTransform));
    ar_suite->add(BOOST_TEST_CASE(ncbi::AR_Test_Intersect)); 
    ar_suite->add(BOOST_TEST_CASE(ncbi::AR_Test_Combine)); 
    test->add(ar_suite);
    
    test_suite* ac_suite = BOOST_TEST_SUITE("CAlignRangeCollection Unit Test");
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_Test_Insert));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_Test_Constructors));    
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_Test_Erase));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_Test_PushPop));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_TestDenormalized));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_TestBounds));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_TestDirections));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_TestNormalized_Strict));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_TestFind));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_Test_GetSecondByFirst));
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_Test_GetFirstBySecond));    
    ac_suite->add(BOOST_TEST_CASE(ncbi::AC_Test_Extender));    
    test->add(ac_suite);

    return test;
}
