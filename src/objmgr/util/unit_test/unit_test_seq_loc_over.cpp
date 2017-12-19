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
* Author:  Aleksey Grichenko
*
* File Description:
*   Unit tests for seq_loc_util functions.
*
* ===========================================================================
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/seqloc/seqloc__.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/misc/sequence_macros.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);


extern const char* sc_TestEntry;

CScope& GetScope()
{
    static CScope s_Scope(*CObjectManager::GetInstance());
    static CRef<CSeq_entry> s_Entry;
    if (!s_Entry) {
        s_Entry.Reset(new CSeq_entry);
        CNcbiIstrstream istr(sc_TestEntry);
        istr >> MSerial_AsnText >> *s_Entry;
        s_Scope.AddTopLevelSeqEntry(*s_Entry);
    }
    return s_Scope;
}


CRef<CSeq_loc> MakeBond(TGi giA, TSeqPos posA, TGi giB = ZERO_GI, TSeqPos posB = 0)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->SetBond().SetA().SetId().SetGi(giA);
    ret->SetBond().SetA().SetPoint(posA);
    if (giB) {
        ret->SetBond().SetB().SetId().SetGi(giB);
        ret->SetBond().SetB().SetPoint(posB);
    }
    return ret;
}


CRef<CSeq_loc> MakeInterval(TGi        gi,
                            TSeqPos    from,
                            TSeqPos    to,
                            ENa_strand strand = eNa_strand_unknown)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->SetInt().SetId().SetGi(gi);
    ret->SetInt().SetFrom(from);
    ret->SetInt().SetTo(to);
    if (strand != eNa_strand_unknown) {
        ret->SetInt().SetStrand(strand);
    }
    return ret;
}


CRef<CSeq_loc> MakeInterval(CRef<CSeq_id> id,
                            TSeqPos    from,
                            TSeqPos    to,
                            ENa_strand strand = eNa_strand_unknown)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->SetInt().SetId(*id);
    ret->SetInt().SetFrom(from);
    ret->SetInt().SetTo(to);
    if (strand != eNa_strand_unknown) {
        ret->SetInt().SetStrand(strand);
    }
    return ret;
}


CRef<CSeq_loc> MakePoint(TGi gi, TSeqPos pos)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->SetPnt().SetId().SetGi(gi);
    ret->SetPnt().SetPoint(pos);
    return ret;
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap1)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    id2->SetLocal().SetStr("local2");

    // No overlap
    CRef<CSeq_loc> loc1 = MakeInterval(2, 10, 20);
    CRef<CSeq_loc> loc2 = MakeInterval(3, 10, 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), -1);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), -1);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap2)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1, loc2;
    id2->SetLocal().SetStr("local2");

    // Same
    loc1 = MakeInterval(2, 10, 20);
    loc2 = MakeInterval(id2, 10, 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 0);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 0);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap3)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1, loc2;
    id2->SetLocal().SetStr("local2");

    // Overlap
    loc1 = MakeInterval(2, 10, 30);
    loc2 = MakeInterval(id2, 20, 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap4)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1, loc2;
    id2->SetLocal().SetStr("local2");

    // Contained
    loc1 = MakeInterval(2, 10, 40);
    loc2 = MakeInterval(id2, 20, 30);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap5)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Multirange, same
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 20));
    loc1->SetMix().Set().push_back(MakeInterval(2, 30, 40));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 10, 20));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 30, 40));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 0);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 0);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 0);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap6)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Multirange, simple (by total range only)
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 20));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 60));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 30, 40));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), -1);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), -1);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap7)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Multirange, overlap
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 70));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 40));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 60, 80));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap8)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Multirange, contained. Contained/contains only check the
    // extremes, not each range.
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 40));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 50, 70));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap9)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Multirange, subset
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 40));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 60, 70));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap10)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // CheckIntervals - extra intervals before/after
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 40, 50));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 60);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 60);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 42);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 42);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 60);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 60);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 60);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 42);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 42);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 60);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap11)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Check intervals fails - the first interval boundaries do not match
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 25));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 60, 70));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 25);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 25);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap12)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Check intervals fails - the second interval boundaries do not match
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 40, 45));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 60, 70));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 25);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 25);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap13)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Check intervals fails - the second interval boundaries do not match
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 45, 50));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 60, 70));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 25);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 25);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap14)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Check intervals fails - the last interval boundaries do not match
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 65, 70));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 25);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 25);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap15)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Check intervals, extra-ranges in the first/last intervals
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 60, 70));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap16)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Subset - two intervals whithin a single interval
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 60, 70));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 73, 78));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 12);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 12);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 14);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 12);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 12);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 12);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 14);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 12);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap17)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> id2(new CSeq_id);
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);
    id2->SetLocal().SetStr("local2");

    // Subset - overlapping ranges whithin the same location (loc2)
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 25));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 60, 70));
    loc2->SetMix().Set().push_back(MakeInterval(id2, 65, 70));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq1)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 50, 70));
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 20, 40));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 60, 80));

    // Invalid combination of arguments.
    BOOST_CHECK_THROW(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple,
        scope, fOverlap_NoMultiSeq), CObjmgrUtilException);
    BOOST_CHECK_THROW(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), CObjmgrUtilException);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq2)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap on some seqs
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(12, 20, 39));
    loc1->SetMix().Set().push_back(MakeInterval(3, 20, 39));
    loc1->SetMix().Set().push_back(MakeInterval(14, 20, 39));
    loc1->SetMix().Set().push_back(MakeInterval(5, 20, 39));
    loc1->SetMix().Set().push_back(MakeInterval(6, 20, 39));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(22, 20, 39));
    loc2->SetMix().Set().push_back(MakeInterval(3, 10, 29));
    loc2->SetMix().Set().push_back(MakeInterval(24, 20, 39));
    loc2->SetMix().Set().push_back(MakeInterval(5, 30, 49));
    loc2->SetMix().Set().push_back(MakeInterval(6, 50, 59));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 150);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 150);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 150);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 150);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq3)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap, multistrand
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 10, 30, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 50, 70, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 20, 40, eNa_strand_plus));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 60, 80, eNa_strand_minus));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq4)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Contained (on each sequence)
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 10, 20));
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 30, 40));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 50, 60));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 70, 80));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 15, 35));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 55, 75));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq5)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Contained, multistrand
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 10, 20, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 30, 40, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 70, 80, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 50, 60, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 15, 35, eNa_strand_plus));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 55, 75, eNa_strand_minus));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq6)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Subset
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 10, 40));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 50, 80));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 60, 70));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq7)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Subset, multistrand
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 10, 40, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 50, 80, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 20, 30, eNa_strand_plus));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 60, 70, eNa_strand_minus));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq8)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Check-intervals
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 10, 20));
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 30, 40));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 50, 60));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 70, 80));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 15, 20));
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 30, 40));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 50, 60));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 70, 75));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 10);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 10);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq9)
{
    CScope* scope = &GetScope();
    CRef<CSeq_id> gi2(new CSeq_id("gi|2"));
    CRef<CSeq_id> gi3(new CSeq_id("gi|3"));
    CRef<CSeq_id> lcl2(new CSeq_id);
    lcl2->SetLocal().SetStr("local2");
    CRef<CSeq_id> lcl3(new CSeq_id);
    lcl3->SetLocal().SetStr("local3");

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Check-intervals, minus strand
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 70, 80, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(gi3, 50, 60, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 30, 40, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(gi2, 10, 20, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 70, 75, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(lcl3, 50, 60, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 30, 40, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(lcl2, 15, 20, eNa_strand_minus));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 10);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 10);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 10);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand1)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Different strands
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 20, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 30, 40, eNa_strand_plus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 30, 40, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 10, 20, eNa_strand_minus));

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), -1);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand2)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Mixed strand in the same location
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 20, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 30, 40, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 10, 20));
    loc2->SetMix().Set().push_back(MakeInterval(2, 30, 40));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 31);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 31);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 31);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 31);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand3)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Mixed strand in the first location, minus in the second one.
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 20, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 30, 40, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 30, 40, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 10, 20, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 31);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 31);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 31);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 31);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand4)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Multistrand, overlap
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 70, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 150, 170, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 130, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 40));
    loc2->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(2, 160, 180, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 140, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand5)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Multistrand, overlap
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 70, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 150, 170, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 130, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 40));
    loc2->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(2, 160, 180, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 140, eNa_strand_minus));

    // Invalid combination of arguments.
    BOOST_CHECK_THROW(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple,
        scope, fOverlap_NoMultiStrand), CObjmgrUtilException);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand6)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Multistrand, overlap 2
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 70, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 130, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 150, 170, eNa_strand_plus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 40));
    loc2->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(2, 160, 180, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 140, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 161);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 161);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 161);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 161);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand7)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Multistrand, overlap 3
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 70, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 130));
    loc1->SetMix().Set().push_back(MakeInterval(2, 150, 170));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 40));
    loc2->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().push_back(MakeInterval(2, 160, 180, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 140, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 161);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 161);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 161);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 161);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand8)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Multistrand, contained
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 80, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 150, 180, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 130, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 40));
    loc2->SetMix().Set().push_back(MakeInterval(2, 60, 70));
    loc2->SetMix().Set().push_back(MakeInterval(2, 160, 170, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 140, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand9)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Multistrand, subset
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 40));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 80));
    loc1->SetMix().Set().push_back(MakeInterval(2, 150, 180, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 140, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(2, 60, 70));
    loc2->SetMix().Set().push_back(MakeInterval(2, 160, 170, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 130, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand10)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // CheckIntervals - extra intervals before/after.
    // Note, that ranges on minus strand have wrong order, so the
    // extremes will be calculated for each minus-strand range.
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 140, 150, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 160, 180, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(2, 140, 150, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 51);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 51);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 42);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 42);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 51);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 51);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 51);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 42);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 42);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 51);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand11)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Check intervals, extra-ranges in the first/last intervals
    // NOTE: Only the first interval's strand is used to detect the direction.
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 140, 150, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 160, 180, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(2, 140, 150, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 160, 170, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 20);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 20);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand12)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Subset - several intervals whithin a single interval
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 60));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 160, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 130, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 140, 150, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 49);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 49);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 58);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 49);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 49);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 49);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 58);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 49);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand13)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Subset - several intervals whithin a single interval. Same as
    // above, but minus strand ranges are ordered correctly.
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 60));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 160, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc2->SetMix().Set().push_back(MakeInterval(2, 140, 150, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 130, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 58);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 58);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand14)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1(new CSeq_loc), loc2(new CSeq_loc);

    // Not a subset - strands do not match
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 60));
    loc1->SetMix().Set().push_back(MakeInterval(2, 110, 160, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 30));
    loc2->SetMix().Set().push_back(MakeInterval(2, 40, 50, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 130, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 140, 150));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 102);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 102);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 102);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 102);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular1)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // No overlap
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(2, 300, 400));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), -1);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), -1);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular2)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap on the left end, second is not circular
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 190, 220));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 552);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 552);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 552);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 552);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular3)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap on the right end, second is not circular
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 1080, 1110));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 552);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 552);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 552);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 552);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular4)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap on both ends, second is not circular
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 190, 1110));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 1420);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 1420);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 1420);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 1420);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular5)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // The second contained in the first
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 110, 190));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 462);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 462);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), 121);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), 121);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 462);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 462);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 462);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), 121);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), 121);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 462);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular6)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // The second's ranges (but not extremes) are contained in the first
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(2, 110, 190));
    loc2->SetMix().Set().push_back(MakeInterval(2, 1110, 1190));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 1260);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 1260);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 1260);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 1260);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular7)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Matching intervals, but loc2 is not circular
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 100, 190));
    loc2->SetMix().Set().push_back(MakeInterval(2, 1110, 1200));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 1240);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 1240);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 1240);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 1240);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular8)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - overlap
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 1020, 1120));
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 120));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 160);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 160);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 160);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 160);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular9)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - contained
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 1020, 1120));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 220));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 100);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 100);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular10)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - subset
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 1120, 1180));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 180));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular11)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - not a subset anymore
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 1120, 1180));
    loc2->SetMix().Set().push_back(MakeInterval(2, 1320, 1380));
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 180));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular12)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - matching intervals
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 1120, 1200));
    loc2->SetMix().Set().push_back(MakeInterval(2, 100, 180));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular13)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - more matching intervals
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(2, 1300, 1400));
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 1120, 1200));
    loc2->SetMix().Set().push_back(MakeInterval(2, 1300, 1400));
    loc2->SetMix().Set().push_back(MakeInterval(2, 100, 180));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular14)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations, minus strand - overlap
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 120, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 1020, 1120, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 160);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 160);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 160);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 160);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular15)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Circular location vs interval, minus strand - contained
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 120, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 442);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 442);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 442);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 442);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 442);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 442);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular16)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations, minus strand - contained
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 20, 120, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 1120, 1220, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 100);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 100);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular17)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations, minus strand - subset
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 120, 180, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 1120, 1180, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular18)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations, minus strand - matching intervals
    loc1->SetMix().Set().push_back(MakeInterval(2, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().clear();
    loc2->SetMix().Set().push_back(MakeInterval(2, 100, 180, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(2, 1120, 1200, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contained,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Contains,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Subset,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_SubsetRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntervals,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_CheckIntRev,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc1, *loc2, eOverlap_Interval,
        1442, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Simple,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contained,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Contains,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Subset,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_SubsetRev,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntervals,
        1442, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_CheckIntRev,
        1442, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlap64(*loc2, *loc1, eOverlap_Interval,
        1442, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular1)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // No overlap
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 300, 400));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), -1);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), -1);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular2)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap on the left end, second is not circular
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 190, 220));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 552);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 552);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 552);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 552);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular3)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap on the right end, second is not circular
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1080, 1110));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 552);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 552);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 552);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 552);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular4)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap on both ends, second is not circular
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 190, 1110));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 1420);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 1420);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 1420);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 1420);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular5)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // The second contained in the first
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 110, 190));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 462);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 462);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 121);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 121);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 462);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 462);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 462);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 121);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 121);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 462);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular6)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // The second's ranges (but not extremes) are contained in the first
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 110, 190));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1110, 1190));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 1260);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 1260);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 1260);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 1260);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular7)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Matching intervals, but loc2 is not circular
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 100, 190));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1110, 1200));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 1240);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 1240);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 1240);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 20);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 1240);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular8)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - overlap
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1020, 1120));
    loc2->SetMix().Set().push_back(MakeInterval(102, 20, 120));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 160);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 160);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 160);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 160);

    // Without topology some results must be different
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple,
        scope, fOverlap_IgnoreTopology), 320);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval,
        scope, fOverlap_IgnoreTopology), 320);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular9)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - contained
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1020, 1120));
    loc2->SetMix().Set().push_back(MakeInterval(102, 120, 220));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 100);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 100);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular10)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - subset
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1120, 1180));
    loc2->SetMix().Set().push_back(MakeInterval(102, 120, 180));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular11)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - not a subset anymore
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1120, 1180));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1320, 1380));
    loc2->SetMix().Set().push_back(MakeInterval(102, 120, 180));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular12)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - matching intervals
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1120, 1200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 100, 180));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular13)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations - more matching intervals
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 1300, 1400));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1120, 1200));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1300, 1400));
    loc2->SetMix().Set().push_back(MakeInterval(102, 100, 180));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular14)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations, minus strand - overlap
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 20, 120, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1020, 1120, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 160);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 160);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 160);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 160);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular15)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Circular location vs interval, minus strand - contained
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 20, 120, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 442);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 442);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 442);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 442);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 442);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 442);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular16)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations, minus strand - contained
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 20, 120, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1120, 1220, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 100);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 100);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 100);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular17)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations, minus strand - subset
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 120, 180, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1120, 1180, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 80);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular18)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Two circular locations, minus strand - matching intervals
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 100, 180, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1120, 1200, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 40);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), 40);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 40);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular19)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Test with multiple circular bioseqs.
    // No overlap
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc1->SetMix().Set().push_back(MakeInterval(202, 400, 450));
    loc1->SetMix().Set().push_back(MakeInterval(202, 100, 150));
    loc2->SetMix().Set().push_back(MakeInterval(102, 400, 500));
    loc2->SetMix().Set().push_back(MakeInterval(202, 200, 300));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), -1);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), -1);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular20)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc1->SetMix().Set().push_back(MakeInterval(202, 400, 450));
    loc1->SetMix().Set().push_back(MakeInterval(202, 100, 150));
    loc2->SetMix().Set().push_back(MakeInterval(102, 180, 280));
    loc2->SetMix().Set().push_back(MakeInterval(202, 420, 520));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 894);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 894);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 894);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 894);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular21)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Contained
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc1->SetMix().Set().push_back(MakeInterval(202, 400, 450));
    loc1->SetMix().Set().push_back(MakeInterval(202, 100, 150));
    loc2->SetMix().Set().push_back(MakeInterval(102, 80, 120));
    loc2->SetMix().Set().push_back(MakeInterval(202, 520, 580));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 834);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 834);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), 834);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 834);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 834);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), 834);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular22)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> loc1(new CSeq_loc);
    CRef<CSeq_loc> loc2(new CSeq_loc);

    // Overlap, mixed strands
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc1->SetMix().Set().push_back(MakeInterval(202, 100, 150, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(202, 400, 450, eNa_strand_minus));
    loc2->SetMix().Set().push_back(MakeInterval(102, 1300, 1400));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 835);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 835);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), -1);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 835);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 835);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), -1);

    loc2->SetMix().Set().push_back(MakeInterval(202, 20, 80, eNa_strand_minus));
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Simple, scope), 774);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contained, scope), 774);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Contains, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc1, *loc2, eOverlap_Interval, scope), -1);

    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Simple, scope), 774);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contained, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Contains, scope), 774);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Subset, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_SubsetRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntervals, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_CheckIntRev, scope), -1);
    BOOST_CHECK_EQUAL(TestForOverlapEx(*loc2, *loc1, eOverlap_Interval, scope), -1);
}


const char* sc_TestEntry = "\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        local str \"local2\",\
        gi 2\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        local str \"local3\",\
        gi 3\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILY\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      }\
    },\
    seq {\
      id {\
        local str \"local102\",\
        gi 102\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        topology circular,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        local str \"local202\",\
        gi 202\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 642,\
        topology circular,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    }\
  }\
}";
