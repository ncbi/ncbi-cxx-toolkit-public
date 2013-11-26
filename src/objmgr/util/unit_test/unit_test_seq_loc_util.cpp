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


CRef<CSeq_loc> MakeBond(TIntId giA, TSeqPos posA, int giB = 0, TSeqPos posB = 0)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->SetBond().SetA().SetId().SetGi(GI_FROM(TIntId, giA));
    ret->SetBond().SetA().SetPoint(posA);
    if (giB > 0) {
        ret->SetBond().SetB().SetId().SetGi(GI_FROM(TIntId, giB));
        ret->SetBond().SetB().SetPoint(posB);
    }
    return ret;
}


CRef<CSeq_loc> MakeInterval(TIntId        gi,
                            TSeqPos    from,
                            TSeqPos    to,
                            ENa_strand strand = eNa_strand_unknown)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->SetInt().SetId().SetGi(GI_FROM(TIntId, gi));
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


CRef<CSeq_loc> MakePoint(TIntId gi, TSeqPos pos)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->SetPnt().SetId().SetGi(GI_FROM(TIntId, gi));
    ret->SetPnt().SetPoint(pos);
    return ret;
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_whole)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3, wl2, wl3;
    wg2.SetWhole().SetGi(GI_FROM(TIntId, 2));
    wg3.SetWhole().SetGi(GI_FROM(TIntId, 3));
    wl2.SetWhole().SetLocal().SetStr("local2");
    wl3.SetWhole().SetLocal().SetStr("local3");

    BOOST_CHECK_EQUAL(Compare(wg2, wg2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(wg2, wl2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(wg2, wg3, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(wg2, wl3, scope), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_interval)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3;
    wg2.SetWhole().SetGi(GI_FROM(TIntId, 2));
    wg3.SetWhole().SetGi(GI_FROM(TIntId, 3));

    // Partial overlap
    CRef<CSeq_loc> i = MakeInterval(2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(wg2, *i, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i, wg2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, *i, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, wg3, scope), eNoOverlap);

    // Full bioseq
    i = MakeInterval(2, 0, 1441);
    BOOST_CHECK_EQUAL(Compare(wg2, *i, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*i, wg2, scope), eSame);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_packed_interval)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3;
    wg2.SetWhole().SetGi(GI_FROM(TIntId, 2));
    wg3.SetWhole().SetGi(GI_FROM(TIntId, 3));

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");

    CSeq_loc pki;
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(wg2, pki, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pki, wg2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, pki, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pki, wg3, scope), eNoOverlap);

    pki.SetPacked_int().AddInterval(gi3, 30, 40);
    BOOST_CHECK_EQUAL(Compare(wg2, pki, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pki, wg2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(wg3, pki, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pki, wg3, scope), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 0, 1441);
    pki.SetPacked_int().AddInterval(gi3, 0, 374);
    BOOST_CHECK_EQUAL(Compare(wg2, pki, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(pki, wg2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(wg3, pki, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(pki, wg3, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_point)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3;
    wg2.SetWhole().SetGi(GI_FROM(TIntId, 2));
    wg3.SetWhole().SetGi(GI_FROM(TIntId, 3));

    CRef<CSeq_loc> pt = MakePoint(2, 10);
    BOOST_CHECK_EQUAL(Compare(wg2, *pt, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*pt, wg2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, *pt, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*pt, wg3, scope), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_packed_point)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3, wl2, wl3;
    wg2.SetWhole().SetGi(GI_FROM(TIntId, 2));
    wg3.SetWhole().SetGi(GI_FROM(TIntId, 3));
    wl2.SetWhole().SetLocal().SetStr("local2");
    wl3.SetWhole().SetLocal().SetStr("local3");

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pp.SetPacked_pnt().AddPoint(10);
    pp.SetPacked_pnt().AddPoint(20);
    pp.SetPacked_pnt().AddPoint(30);
    BOOST_CHECK_EQUAL(Compare(wg2, pp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pp, wg2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(wl3, pp, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pp, wl3, scope), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_mix)
{
    CScope* scope = &GetScope();

    CSeq_loc w;
    w.SetWhole().SetGi(GI_FROM(TIntId, 2));

    // Check some basic cases
    CSeq_loc mix;
    mix.SetMix().Set().push_back(MakeInterval(3, 10, 20));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope), eNoOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 20));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 20));
    mix.SetMix().Set().push_back(MakeInterval(2, 30, 40));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 20));
    mix.SetMix().Set().push_back(MakeInterval(3, 30, 40));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope), eOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 0, 1441));
    mix.SetMix().Set().push_back(MakeInterval(3, 30, 40));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 0, 1441));
    CRef<CSeq_loc> sub(new CSeq_loc);
    sub->SetWhole().SetGi(GI_FROM(TIntId, 2));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(w, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_bond)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3;
    wg2.SetWhole().SetGi(GI_FROM(TIntId, 2));
    wg3.SetWhole().SetGi(GI_FROM(TIntId, 3));

    // B not set
    CRef<CSeq_loc> bond = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(wg2, *bond, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*bond, wg2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, *bond, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*bond, wg3, scope), eNoOverlap);

    // A and B on different bioseqs
    bond = MakeBond(2, 10, 3, 20);
    BOOST_CHECK_EQUAL(Compare(wg2, *bond, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*bond, wg2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(wg3, *bond, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*bond, wg3, scope), eOverlap);

    // A and B on the same bioseq
    bond = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(wg2, *bond, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*bond, wg2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, *bond, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*bond, wg3, scope), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_interval)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i2 = MakeInterval(2, 10, 20);
    CRef<CSeq_loc> i3 = MakeInterval(3, 10, 20);

    CRef<CSeq_loc> i = MakeInterval(2, 25, 35);
    // No overlap
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope), eNoOverlap);

    // Abutting but not overlapping
    i = MakeInterval(2, 0, 22);
    i2 = MakeInterval(2, 23, 40);
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope), eNoOverlap);

    i2 = MakeInterval(2, 10, 20);
    i = MakeInterval(2, 5, 15);
    // Partial overlap
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i3, *i, scope), eNoOverlap);

    i = MakeInterval(2, 5, 25);
    // Contained/contains
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope), eContains);
    i = MakeInterval(2, 10, 25); // same on the right
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope), eContains);
    i = MakeInterval(2, 5, 20); // same on the left
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope), eContains);

    i = MakeInterval(2, 10, 20);
    // Same
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope), eSame);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_packed_interval)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i2 = MakeInterval(2, 10, 20);
    CRef<CSeq_loc> i3 = MakeInterval(3, 30, 40);

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");

    CSeq_loc pki;
    // Check different combinations of overlaps. Make sure the order
    // of ranges does not affect the result.

    // eNoOverlap + eNoOverlap = eNoOverlap
    pki.SetPacked_int().AddInterval(gi2, 25, 35);
    pki.SetPacked_int().AddInterval(gi2, 35, 45);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eNoOverlap);

    // eNoOverlap + eContained = eOverlap
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eOverlap);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eOverlap);

    // eNoOverlap + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eNoOverlap + eSame = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eNoOverlap + eOverlap = eOverlap
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    pki.SetPacked_int().AddInterval(gi2, 15, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eOverlap);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eOverlap);

    // eContained + eContained = eContained
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 11, 13);
    pki.SetPacked_int().AddInterval(gi2, 15, 18);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContains);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContains);

    // eContained + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 11, 13);
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eContained + eSame = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 11, 13);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContains);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContains);

    // eContained + eOverlap = eOverlap
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 11, 13);
    pki.SetPacked_int().AddInterval(gi2, 15, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eOverlap);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eOverlap);

    // eContains + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    pki.SetPacked_int().AddInterval(gi2, 5, 25);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eContains + eSame = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eContains + eOverlap = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    pki.SetPacked_int().AddInterval(gi2, 15, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eSame + eSame = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContains);

    // eSame + eOverlap = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 15, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eOverlap + eOverlap = eOverlap
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 8, 13);
    pki.SetPacked_int().AddInterval(gi2, 16, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eOverlap);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eOverlap);

    // eNoOverlap + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 25, 35);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eContained + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 12, 18);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContains);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContains);

    // eContains + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);

    // eSame + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContains);

    // eOverlap + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 15, 25);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope), eContained);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i2 = MakeInterval(2, 10, 20);
    CRef<CSeq_loc> i3 = MakeInterval(3, 10, 20);

    CRef<CSeq_loc> pt = MakePoint(2, 5);
    // No overlap
    BOOST_CHECK_EQUAL(Compare(*i2, *pt, scope), eNoOverlap);
    pt = MakePoint(2, 15);
    BOOST_CHECK_EQUAL(Compare(*i3, *pt, scope), eNoOverlap);

    // Overlap
    BOOST_CHECK_EQUAL(Compare(*i2, *pt, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*pt, *i2, scope), eContained);

    // Same - interval of length 1
    CRef<CSeq_loc> i1 = MakeInterval(2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(*pt, *i1, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*i1, *pt, scope), eSame);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_packed_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i2 = MakeInterval(2, 10, 20);
    CRef<CSeq_loc> i3 = MakeInterval(3, 10, 20);

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pp.SetPacked_pnt().AddPoint(5);

    // No overlap
    BOOST_CHECK_EQUAL(Compare(*i2, pp, scope), eNoOverlap);
    pp.SetPacked_pnt().SetPoints().front() = 15;
    BOOST_CHECK_EQUAL(Compare(*i3, pp, scope), eNoOverlap);

    // Contained in the interval
    BOOST_CHECK_EQUAL(Compare(*i2, pp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pp, *i2, scope), eContained);
    
    // Overlap
    pp.SetPacked_pnt().AddPoint(5);
    pp.SetPacked_pnt().AddPoint(25);
    BOOST_CHECK_EQUAL(Compare(*i2, pp, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pp, *i2, scope), eOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_mix)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i = MakeInterval(2, 20, 80);

    CSeq_loc mix;
    CRef<CSeq_loc> sub, sub2;

    mix.SetMix().Set().push_back(MakeInterval(3, 10, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eNoOverlap);

    // Whole
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetWhole().SetGi(GI_FROM(TIntId, 2));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContains);

    // Points
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakePoint(2, 50));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContained);
    mix.SetMix().Set().push_back(MakePoint(2, 60));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContained);
    mix.SetMix().Set().push_back(MakePoint(2, 150));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eOverlap);

    // Packed points - some more complicated cases
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    sub->SetPacked_pnt().AddPoint(30);
    sub->SetPacked_pnt().AddPoint(60);
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContained);
    sub2.Reset(new CSeq_loc);
    sub2->SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    sub2->SetPacked_pnt().AddPoint(10);
    sub2->SetPacked_pnt().AddPoint(50);
    mix.SetMix().Set().push_back(sub2);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eOverlap);

    // Intervals
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 15));
    mix.SetMix().Set().push_back(MakeInterval(2, 85, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eNoOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 25));
    mix.SetMix().Set().push_back(MakeInterval(2, 55, 70));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 35));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eOverlap);

    // This results in eOverlap although the mix covers the whole interval.
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 55));
    mix.SetMix().Set().push_back(MakeInterval(2, 50, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 90));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 80));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 80));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContains);

    // Packed intervals
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 10, 30)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 40, 60)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 70, 90)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eOverlap);

    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(3, 10, 30)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 10, 90)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 70, 90)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    try {
        Compare(*i, mix, scope);
    }
    catch (...) {
    }
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContains);

    // Mixed sub-location types
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPnt().SetId().SetGi(GI_FROM(TIntId, 2));
    sub->SetPnt().SetPoint(30);
    mix.SetMix().Set().push_back(MakePoint(2, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 35, 40));
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 45, 50)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 50, 55)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 90));
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 40, 50)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 50, 85)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eContains);

    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    mix.SetMix().Set().push_back(MakeInterval(2, 40, 50));
    mix.SetMix().Set().push_back(MakeInterval(2, 60, 70));
    mix.SetMix().Set().push_back(sub);
    mix.SetMix().Set().push_back(MakeBond(2, 50, 3, 40));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope), eOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_bond)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> bA2 = MakeBond(2, 10);
    CRef<CSeq_loc> bA3 = MakeBond(3, 10);
    CRef<CSeq_loc> bA2B3 = MakeBond(2, 10, 3, 20);
    CRef<CSeq_loc> bA3B2 = MakeBond(3, 20, 2, 10);
    CRef<CSeq_loc> bA2B2 = MakeBond(2, 10, 2, 20);

    CRef<CSeq_loc> i = MakeInterval(2, 15, 25);
    // No overlap
    BOOST_CHECK_EQUAL(Compare(*bA2, *i, scope), eNoOverlap);

    i = MakeInterval(2, 5, 15);
    // Overlap with one point (no B)
    BOOST_CHECK_EQUAL(Compare(*bA2, *i, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *bA2, scope), eContains);
    // Overlap with only one of A or B
    BOOST_CHECK_EQUAL(Compare(*bA2B3, *i, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *bA2B3, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*bA3B2, *i, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *bA3B2, scope), eOverlap);
    // B is on the same bioseq but out of range
    BOOST_CHECK_EQUAL(Compare(*bA2B2, *i, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *bA2B2, scope), eOverlap);

    i = MakeInterval(2, 5, 25);
    // Overlap with both A and B
    BOOST_CHECK_EQUAL(Compare(*bA2B2, *i, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *bA2B2, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_interval_vs_packed_interval)
{
    CScope* scope = &GetScope();

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");
    CSeq_id lcl2;
    lcl2.SetLocal().SetStr("local2");

    CSeq_loc pk1, pk2;

    // Complicated case: although different seq-ids are used in both
    // locations and the order is wrong, eSame should be returned.
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(lcl2, 30, 40);
    pk2.SetPacked_int().AddInterval(gi2, 30, 40);
    pk2.SetPacked_int().AddInterval(lcl2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eSame);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(gi2, 30, 40);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi3, 10, 20);
    pk2.SetPacked_int().AddInterval(gi2, 50, 60);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eNoOverlap);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 15, 20);
    pk1.SetPacked_int().AddInterval(gi2, 60, 70);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 5, 10);
    pk2.SetPacked_int().AddInterval(gi2, 50, 55);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eNoOverlap);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 40);
    pk1.SetPacked_int().AddInterval(gi2, 60, 90);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 20, 30);
    pk2.SetPacked_int().AddInterval(gi2, 70, 80);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eContained);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eContained);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 40);
    pk1.SetPacked_int().AddInterval(gi3, 60, 90);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 20, 30);
    pk2.SetPacked_int().AddInterval(gi3, 70, 80);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eContained);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eContained);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 40);
    pk1.SetPacked_int().AddInterval(gi2, 50, 70);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 20, 30);
    pk2.SetPacked_int().AddInterval(gi2, 60, 80);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eOverlap);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eOverlap);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(gi2, 50, 70);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 10, 20);
    pk2.SetPacked_int().AddInterval(gi2, 60, 80);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eOverlap);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eOverlap);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(gi2, 30, 40);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 10, 20);
    pk2.SetPacked_int().AddInterval(gi2, 30, 40);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eSame);
    // The order does not matter.
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eSame);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(gi2, 10, 90);
    pk1.SetPacked_int().AddInterval(gi2, 80, 90);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 10, 90);
    pk2.SetPacked_int().AddInterval(gi2, 50, 60);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eContains);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eContains);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 19);
    pk1.SetPacked_int().AddInterval(gi3, 20, 29);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 20, 29);
    pk2.SetPacked_int().AddInterval(gi3, 10, 19);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eNoOverlap);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_interval_vs_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> pt = MakePoint(2, 15);

    CSeq_id gi2("gi|2");
    CSeq_loc pki;

    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope), eNoOverlap);
    pki.SetPacked_int().AddInterval(gi2, 20, 25);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope), eSame);

    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_interval_vs_packed_point)
{
    CScope* scope = &GetScope();

    CSeq_loc pkp;
    pkp.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pkp.SetPacked_pnt().AddPoint(15);

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");

    CSeq_loc pki;

    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eNoOverlap);
    pki.SetPacked_int().AddInterval(gi2, 20, 25);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eSame);

    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eContains);

    pki.SetPacked_int().AddInterval(gi2, 25, 25);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eContained);

    pkp.SetPacked_pnt().AddPoint(25);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 17, 23);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 30, 40);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 5, 10);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eOverlap);

    // Complicated case: each interval contains just one of the
    // points.
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 21, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi3, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope), eOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_interval_vs_mix)
{
    CScope* scope = &GetScope();

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");

    CSeq_loc pki;
    pki.SetPacked_int().AddInterval(gi2, 20, 80);

    CSeq_loc mix;
    CRef<CSeq_loc> sub, sub2;

    mix.SetMix().Set().push_back(MakeInterval(3, 10, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eNoOverlap);

    // Whole
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetWhole().SetGi(GI_FROM(TIntId, 2));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContains);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 0, 1441);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eSame);

    // Points
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 30, 40);
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakePoint(2, 15));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContained);
    mix.SetMix().Set().push_back(MakePoint(2, 35));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContained);
    mix.SetMix().Set().push_back(MakePoint(2, 150));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eOverlap);

    // Packed points - some more complicated cases
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    sub->SetPacked_pnt().AddPoint(15);
    sub->SetPacked_pnt().AddPoint(33);
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContained);
    sub2.Reset(new CSeq_loc);
    sub2->SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    sub2->SetPacked_pnt().AddPoint(5);
    sub2->SetPacked_pnt().AddPoint(37);
    mix.SetMix().Set().push_back(sub2);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eOverlap);

    // Intervals
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 20, 80);
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 15));
    mix.SetMix().Set().push_back(MakeInterval(2, 85, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eNoOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 25));
    mix.SetMix().Set().push_back(MakeInterval(2, 55, 70));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 35));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eOverlap);

    // This results in eOverlap although the mix covers the whole interval.
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 55));
    mix.SetMix().Set().push_back(MakeInterval(2, 50, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eOverlap);

    // The same problem here.
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 60);
    pki.SetPacked_int().AddInterval(gi2, 56, 90);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 20, 80);
    pki.SetPacked_int().AddInterval(gi2, 30, 70);
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 90));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 80));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 80));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContains);

    // Packed intervals
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 20, 40);
    pki.SetPacked_int().AddInterval(gi2, 50, 70);
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 10, 30)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 50, 60)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 70, 90)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eOverlap);

    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(3, 10, 30)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 10, 90)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 70, 90)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContains);

    // Mixed sub-location types
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPnt().SetId().SetGi(GI_FROM(TIntId, 2));
    sub->SetPnt().SetPoint(30);
    mix.SetMix().Set().push_back(MakePoint(2, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 35, 40));
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 55, 60)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 60, 65)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 40));
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 40, 50)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 50, 85)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_interval_vs_bond)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> b = MakeBond(2, 15);

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");

    CSeq_loc pki;

    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eNoOverlap);
    pki.SetPacked_int().AddInterval(gi2, 20, 25);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eContained);

    // For bonds we only detect no-overlap/overlap/contained, not same.
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eSame);

    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eContains);

    b->SetBond().SetB().SetId().SetGi(GI_FROM(TIntId, 2));
    b->SetBond().SetB().SetPoint(25);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 17, 23);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 30, 40);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 5, 10);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eContained);

    b->SetBond().SetB().SetId().SetGi(GI_FROM(TIntId, 3));

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi3, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope), eContained);
}


BOOST_AUTO_TEST_CASE(Test_Compare_point_vs_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> p2a = MakePoint(2, 10);
    CRef<CSeq_loc> p2b = MakePoint(2, 15);
    CRef<CSeq_loc> p3 = MakePoint(3, 20);

    BOOST_CHECK_EQUAL(Compare(*p2a, *p2a, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*p2a, *p2b, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*p2a, *p3, scope), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_point_vs_packed_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> p = MakePoint(2, 5);

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pp.SetPacked_pnt().AddPoint(10);

    // No overlap
    BOOST_CHECK_EQUAL(Compare(*p, pp, scope), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pp, *p, scope), eNoOverlap);

    p = MakePoint(2, 10);
    // Single entry in packed points
    BOOST_CHECK_EQUAL(Compare(*p, pp, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(pp, *p, scope), eSame);

    pp.SetPacked_pnt().AddPoint(20);
    pp.SetPacked_pnt().AddPoint(30);
    // Multiple points
    BOOST_CHECK_EQUAL(Compare(*p, pp, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(pp, *p, scope), eContains);

    // Special case: all packed points are the same.
    // The first seq-loc contains the second one in any direction.
    pp.Reset();
    pp.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pp.SetPacked_pnt().AddPoint(10);
    pp.SetPacked_pnt().AddPoint(10);
    BOOST_CHECK_EQUAL(Compare(*p, pp, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pp, *p, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_point_vs_mix)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> p = MakePoint(2, 50);

    CSeq_loc mix;

    mix.SetMix().Set().push_back(MakeInterval(3, 10, 90));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope), eNoOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 40));
    mix.SetMix().Set().push_back(MakeInterval(2, 60, 90));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope), eNoOverlap);
    mix.SetMix().Set().push_back(MakeInterval(2, 40, 60));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *p, scope), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakePoint(2, 50));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *p, scope), eSame);
    mix.SetMix().Set().push_back(MakePoint(2, 50));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *p, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_point_vs_bond)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> bA2 = MakeBond(2, 10);
    CRef<CSeq_loc> bA2B3 = MakeBond(2, 10, 3, 20);
    CRef<CSeq_loc> bA2B2 = MakeBond(2, 10, 2, 20);
    CRef<CSeq_loc> bA2B2eq = MakeBond(2, 10, 2, 10);

    CRef<CSeq_loc> p = MakePoint(2, 5);
    // No overlap
    BOOST_CHECK_EQUAL(Compare(*bA2, *p, scope), eNoOverlap);

    p = MakePoint(2, 10);
    // Overlap with A
    BOOST_CHECK_EQUAL(Compare(*bA2, *p, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*bA2B3, *p, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*p, *bA2B3, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*bA2B2, *p, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*p, *bA2B2, scope), eContained);

    // Special case - A==B, contains in both directions.
    BOOST_CHECK_EQUAL(Compare(*bA2B2eq, *p, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*p, *bA2B2eq, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_point_vs_packed_point)
{
    CScope* scope = &GetScope();

    CSeq_loc pp1;
    pp1.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pp1.SetPacked_pnt().AddPoint(10);

    CSeq_loc pp2;
    pp2.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 3));
    pp2.SetPacked_pnt().AddPoint(10);

    // No overlap for different bioseqs
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eNoOverlap);
    pp1.SetPacked_pnt().AddPoint(20);
    pp2.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pp2.SetPacked_pnt().SetPoints().front() = 5;
    pp2.SetPacked_pnt().AddPoint(15);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eNoOverlap);
    pp1.SetPacked_pnt().AddPoint(30);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eNoOverlap);

    // Same
    pp2.Assign(pp1);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eSame);

    // Overlap
    pp2.SetPacked_pnt().SetPoints().clear();
    pp2.SetPacked_pnt().AddPoint(5);
    pp2.SetPacked_pnt().AddPoint(10);
    pp2.SetPacked_pnt().AddPoint(15);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope), eOverlap);

    // Contained/contains
    pp1.SetPacked_pnt().AddPoint(40); // 10, 20, 30, 40
    pp2.SetPacked_pnt().SetPoints().clear();
    pp2.SetPacked_pnt().AddPoint(20); // 20
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope), eContained);
    pp2.SetPacked_pnt().AddPoint(30); // 20, 30
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope), eContained);
    // Wrong order of points should still work
    pp2.SetPacked_pnt().AddPoint(10); // 20, 30, 10
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope), eContained);
    // Duplicate points - same result
    pp2.SetPacked_pnt().AddPoint(20); // 20, 30, 10, 20
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope), eContained);

    // Special case - due to duplicate points both sets contain each other
    // but are not equal
    pp2.SetPacked_pnt().AddPoint(40); // 20, 30, 10, 20, 40
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope), eContains);

    // Now they just overlap
    pp1.SetPacked_pnt().AddPoint(45);
    pp2.SetPacked_pnt().AddPoint(5);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope), eOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_point_vs_mix)
{
    CScope* scope = &GetScope();

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pp.SetPacked_pnt().AddPoint(25);
    pp.SetPacked_pnt().AddPoint(85);

    CSeq_loc mix;

    // Each point is contained in a separate sub-location.
    mix.SetMix().Set().push_back(MakeInterval(2, 30, 70));
    BOOST_CHECK_EQUAL(Compare(pp, mix, scope), eNoOverlap);
    pp.SetPacked_pnt().AddPoint(50);
    BOOST_CHECK_EQUAL(Compare(pp, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pp, mix, scope), eOverlap);
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(pp, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pp, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_point_vs_bond)
{
    CScope* scope = &GetScope();

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(GI_FROM(TIntId, 2));
    pp.SetPacked_pnt().AddPoint(10);

    CRef<CSeq_loc> b = MakeBond(3, 10);
    // No overlap
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eNoOverlap);
    b = MakeBond(2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eNoOverlap);

    b = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eSame);
    b = MakeBond(2, 10, 3, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContains);
    b = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContains);
    b = MakeBond(3, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContains);
    b = MakeBond(2, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContains);

    pp.SetPacked_pnt().AddPoint(20);
    b = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContained);
    b = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eSame);
    // The order of points does not matter.
    b = MakeBond(2, 20, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eSame);
    b = MakeBond(2, 10, 3, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eOverlap);
    b = MakeBond(3, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eOverlap);
    b = MakeBond(2, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContained);
    b = MakeBond(2, 20, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContained);
    b = MakeBond(2, 10, 2, 40);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eOverlap);

    pp.SetPacked_pnt().AddPoint(30);
    b = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContained);
    b = MakeBond(2, 10, 2, 30);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContained);
    b = MakeBond(2, 30, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContained);
    b = MakeBond(2, 10, 3, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eOverlap);
    b = MakeBond(3, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eOverlap);
    b = MakeBond(2, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContained);
    b = MakeBond(2, 10, 2, 40);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eOverlap);

    pp.SetPacked_pnt().SetPoints().clear();
    pp.SetPacked_pnt().AddPoint(10);
    pp.SetPacked_pnt().AddPoint(20);
    pp.SetPacked_pnt().AddPoint(20);
    b = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContained);
    b = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContains);
    b = MakeBond(2, 10, 2, 30);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eOverlap);
    b = MakeBond(2, 30, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eOverlap);
    b = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContains);
    b = MakeBond(2, 20, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_mix_vs_mix)
{
    CScope* scope = &GetScope();

    CSeq_loc mix1, mix2;

    mix1.SetMix().Set().push_back(MakeInterval(2, 10, 20));
    mix1.SetMix().Set().push_back(MakeInterval(2, 50, 60));
    mix2.SetMix().Set().push_back(MakeInterval(2, 30, 40));
    mix2.SetMix().Set().push_back(MakeInterval(2, 70, 80));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope), eNoOverlap);
    mix1.SetMix().Set().push_back(MakeInterval(3, 30, 40));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope), eNoOverlap);
    mix2.SetMix().Set().push_front(MakeInterval(3, 20, 35));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope), eOverlap);

    mix1.SetMix().Set().clear();
    mix1.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    mix1.SetMix().Set().push_back(MakeInterval(2, 50, 70));
    mix2.SetMix().Set().clear();
    mix2.SetMix().Set().push_back(MakeInterval(2, 60, 65));
    mix2.SetMix().Set().push_back(MakeInterval(2, 20, 25));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope), eContained);

    mix2.SetMix().Set().push_back(MakeInterval(2, 50, 70));
    mix2.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope), eContains);

    // Empty should not change anything (?)
    CRef<CSeq_loc> sub(new CSeq_loc);
    sub->SetEmpty().SetGi(GI_FROM(TIntId, 2));
    mix2.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope), eContains);

    mix2.SetMix().Set().push_back(MakePoint(3, 100));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope), eContains);

    mix1.SetMix().Set().push_back(MakePoint(2, 110));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope), eOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_mix_vs_bond)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> b = MakeBond(2, 50);

    CSeq_loc mix;

    // Each point is contained in a separate sub-location.
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eNoOverlap);
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eNoOverlap);
    mix.SetMix().Set().push_back(MakeInterval(2, 40, 60));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eContains);

    b = MakeBond(2, 20, 3, 40);
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eOverlap);

    mix.SetMix().Set().push_back(MakeInterval(3, 30, 50));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakePoint(2, 20));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eContained);

    mix.SetMix().Set().push_back(MakePoint(3, 40));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eSame);
    mix.SetMix().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eSame);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 20));
    mix.SetMix().Set().push_back(MakeInterval(3, 40, 40));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eSame);
    mix.SetMix().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eSame);
    mix.SetMix().Set().push_back(MakeInterval(3, 40, 40));
    mix.SetMix().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_bond_vs_bond)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> b1 = MakeBond(2, 10);
    CRef<CSeq_loc> b2 = MakeBond(3, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eNoOverlap);

    b2 = MakeBond(2, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eNoOverlap);

    b2 = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eSame);

    b2 = MakeBond(2, 10, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eContains);
    b1 = MakeBond(2, 10, 3, 25);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eOverlap);
    b1 = MakeBond(2, 10, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eSame);

    b2 = MakeBond(2, 15, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eOverlap);
    b2 = MakeBond(2, 10, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eSame);
    // The order or ranges is not important
    b2 = MakeBond(3, 20, 2, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eSame);

    b1 = MakeBond(2, 10, 2, 10);
    b2 = MakeBond(2, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eSame);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eSame);

    b1 = MakeBond(2, 10, 3, 20);
    b2 = MakeBond(3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eContained);
    b2 = MakeBond(3, 20, 2, 15);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eOverlap);

    b1 = MakeBond(2, 10, 2, 10);
    b2 = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eContains);
    b2 = MakeBond(2, 10, 3, 15);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eContained);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eContains);

    b1 = MakeBond(2, 15, 3, 20);
    b2 = MakeBond(3, 20, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope), eContains);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope), eContained);
}


BOOST_AUTO_TEST_CASE(Test_TestForOverlap)
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

    // Multirange, same
    loc1.Reset(new CSeq_loc);
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 20));
    loc1->SetMix().Set().push_back(MakeInterval(2, 30, 40));
    loc2.Reset(new CSeq_loc);
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

    // Multirange, simple (by total range only)
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 20));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 60));
    loc2->SetMix().Set().clear();
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

    // Multirange, overlap
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 70));
    loc2->SetMix().Set().clear();
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

    // Multirange, contained. Contained/contains only check the
    // extremes, not each range.
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().clear();
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

    // Multirange, subset
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 40));
    loc1->SetMix().Set().push_back(MakeInterval(2, 50, 80));
    loc2->SetMix().Set().clear();
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

    // CheckIntervals - extra intervals before/after
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 30));
    loc1->SetMix().Set().push_back(MakeInterval(2, 40, 50));
    loc1->SetMix().Set().push_back(MakeInterval(2, 60, 80));
    loc2->SetMix().Set().clear();
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

    // Check intervals fails - the first interval boundaries do not match
    loc2->SetMix().Set().clear();
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

    // Check intervals fails - the second interval boundaries do not match
    loc2->SetMix().Set().clear();
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

    // Check intervals fails - the second interval boundaries do not match
    loc2->SetMix().Set().clear();
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

    // Check intervals fails - the last interval boundaries do not match
    loc2->SetMix().Set().clear();
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

    // Check intervals, extra-ranges in the first/last intervals
    loc2->SetMix().Set().clear();
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

    // Subset - two intervals whithin a single interval
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

    // Subset - overlapping ranges whithin the same location (loc2)
    loc2->SetMix().Set().clear();
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


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multiseq)
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


BOOST_AUTO_TEST_CASE(Test_TestForOverlap_Multistrand)
{
    CScope* scope = &GetScope();
    CRef<CSeq_loc> loc1, loc2;

    // Different strands
    loc1.Reset(new CSeq_loc);
    loc1->SetMix().Set().push_back(MakeInterval(2, 10, 20, eNa_strand_plus));
    loc1->SetMix().Set().push_back(MakeInterval(2, 30, 40, eNa_strand_plus));
    loc2.Reset(new CSeq_loc);
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

    // Check intervals, extra-ranges in the first/last intervals
    // NOTE: Only the first interval's strand is used to detect the direction.
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


BOOST_AUTO_TEST_CASE(Test_TestForOverlap64_Circular)
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

    // Overlap on the left end, second is not circular
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

    // Overlap on the right end, second is not circular
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

    // Overlap on both ends, second is not circular
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

    // The second contained in the first
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

    // The second's ranges (but not extremes) are contained in the first
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

    // Matching intervals, but loc2 is not circular
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

    // Two circular locations - overlap
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

    // Two circular locations - contained
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

    // Two circular locations - subset
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

    // Two circular locations - not a subset anymore
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

    // Two circular locations - matching intervals
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

    // Circular location vs interval, minus strand - contained
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

    // Two circular locations, minus strand - contained
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

    // Two circular locations, minus strand - subset
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

    // Two circular locations, minus strand - matching intervals
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


BOOST_AUTO_TEST_CASE(Test_TestForOverlapEx_Circular)
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

    // Overlap on the left end, second is not circular
    loc2->SetMix().Set().clear();
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

    // Overlap on the right end, second is not circular
    loc2->SetMix().Set().clear();
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

    // Overlap on both ends, second is not circular
    loc2->SetMix().Set().clear();
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

    // The second contained in the first
    loc2->SetMix().Set().clear();
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

    // The second's ranges (but not extremes) are contained in the first
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

    // Matching intervals, but loc2 is not circular
    loc2->SetMix().Set().clear();
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

    // Two circular locations - overlap
    loc2->SetMix().Set().clear();
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

    // Two circular locations - contained
    loc2->SetMix().Set().clear();
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

    // Two circular locations - subset
    loc2->SetMix().Set().clear();
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

    // Two circular locations - not a subset anymore
    loc2->SetMix().Set().clear();
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

    // Two circular locations - matching intervals
    loc2->SetMix().Set().clear();
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

    // Two circular locations - more matching intervals
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 1300, 1400));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc2->SetMix().Set().clear();
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

    // Two circular locations, minus strand - overlap
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200, eNa_strand_minus));
    loc2->SetMix().Set().clear();
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

    // Circular location vs interval, minus strand - contained
    loc2->SetMix().Set().clear();
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

    // Two circular locations, minus strand - contained
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

    // Two circular locations, minus strand - subset
    loc2->SetMix().Set().clear();
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

    // Two circular locations, minus strand - matching intervals
    loc2->SetMix().Set().clear();
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

    // Test with multiple circular bioseqs.
    // No overlap
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc1->SetMix().Set().push_back(MakeInterval(202, 400, 450));
    loc1->SetMix().Set().push_back(MakeInterval(202, 100, 150));
    loc2->SetMix().Set().clear();
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

    // Overlap
    loc2->SetMix().Set().clear();
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

    // Contained
    loc2->SetMix().Set().clear();
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

    // Overlap, mixed strands
    loc1->SetMix().Set().clear();
    loc1->SetMix().Set().push_back(MakeInterval(102, 1100, 1200));
    loc1->SetMix().Set().push_back(MakeInterval(102, 100, 200));
    loc1->SetMix().Set().push_back(MakeInterval(202, 100, 150, eNa_strand_minus));
    loc1->SetMix().Set().push_back(MakeInterval(202, 400, 450, eNa_strand_minus));
    loc2->SetMix().Set().clear();
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


static const char * sc_TestFramePlus = "\
Seq-entry ::= set {\
  class nuc-prot ,\
  descr {\
    source {\
      genome genomic ,\
      org {\
        taxname \"Homo sapiens\" ,\
        common \"human\" ,\
        db {\
          {\
            db \"taxon\" ,\
            tag\
              id 9606 } } ,\
        syn {\
          \"woman\" ,\
          \"man\" } ,\
        orgname {\
          name\
            binomial {\
              genus \"Homo\" ,\
              species \"sapiens\" } ,\
          lineage \"Eukaryota; Metazoa; Chordata; Craniata; Vertebrata;\
 Euteleostomi; Mammalia; Eutheria; Euarchontoglires; Primates; Haplorrhini;\
 Catarrhini; Hominidae; Homo\" ,\
          gcode 1 ,\
          mgcode 2 ,\
          div \"PRI\" } } ,\
      subtype {\
        {\
          subtype chromosome ,\
          name \"22\" } ,\
        {\
          subtype map ,\
          name \"22q13.31-q13.33\" } } } } ,\
  seq-set {\
    seq {\
      id {\
        other {\
          accession \"NG_032160\" } } ,\
      descr {\
        title \"Homo sapiens tubulin, gamma complex associated protein 6\
 (TUBGCP6), RefSeqGene on chromosome 22\" ,\
        molinfo {\
          biomol genomic } ,\
        create-date\
          std {\
            year 2012 ,\
            month 3 ,\
            day 15 ,\
            hour 15 ,\
            minute 39 ,\
            second 2 } } ,\
      inst {\
        repr raw ,\
        mol dna ,\
        length 1683 ,\
        seq-data\
          ncbi2na '527578925EEF52B961F44045F71F9E275F54BA2722D94A5E9A0F5E94A78\
E892EE5D5E8A278A7A5F5A5523A355AEAC74BD71F7FA3FBF5200A0930D201229EAA209C88489E9\
EA952BA79D6754A848F4B4A9F4A937798E7AA5AFAD122C88BA629E112BBAA9212EB779EEAEA7D2\
2025DEAFDCA883A9D522B427F544537D40A82A795EDE08B56546E4A3886E8033EF9FC1F0004214\
BAAFA7A8916949295E78930155D47A20A6E9579517A15F7A038A82E70492E54D51093C076A2BA2\
1DD492027A9248BAD7957695102A5FA5893A9397AEEE547AAD4D5E52EABD4A85DAA16A79FA95FA\
1DCAE25B889A9EA929A249E5E4A92A11AD2A9C5D52115E9775129273538E78E7A64B405649EF66\
7CEA456A4A4D94A89E89BBA2492DDA7A249E2E692279224A052A51EA787A7129B12B895E6D4EB4\
5AA869AB66E7A002D495A7B87522568ED3D4895A7A10A17A0A67AA19E597A4A7891B99B8E46504\
1582C6E78A07136648394D8A796266A87DD229091197B57BAD5EA2AAB6954855DD15D78D752B9A\
6AE78071E8855F14798A6AA59448658A51A0961AA6869292675C492C255677A92481EE6E138DF6\
C18'H } ,\
      annot {\
        {\
          data\
            ftable {\
              {\
                data\
                  cdregion {\
                    frame three ,\
                    code {\
                      id 1 } ,\
                    code-break {\
                      {\
                        loc\
                          int {\
                            from 30 ,\
                            to 32 ,\
                            strand plus ,\
                            id\
                              other {\
                                accession \"NG_032160\" } } ,\
                        aa\
                          ncbieaa 85 } } } ,\
                partial TRUE ,\
                product\
                  whole\
                    local\
                      id 32880229 ,\
                location\
                  int {\
                    from 28 ,\
                    to 178 ,\
                    strand plus ,\
                    id\
                      other {\
                        accession \"NG_032160\" } ,\
                    fuzz-from\
                      lim lt } ,\
                dbxref {\
                  {\
                    db \"CCDS\" ,\
                    tag\
                      str \"CCDS43034.1\" } ,\
                  {\
                    db \"GeneID\" ,\
                    tag\
                      id 83642 } } } ,\
              {\
                data\
                  gene {\
                    locus \"SELO\" } ,\
                partial TRUE ,\
                location\
                  int {\
                    from 28 ,\
                    to 1680 ,\
                    strand plus ,\
                    id\
                      other {\
                        accession \"NG_032160\" } ,\
                    fuzz-from\
                      lim lt } } } } } } ,\
    seq {\
      id {\
        local\
          id 32880229 } ,\
      descr {\
        molinfo {\
          biomol peptide ,\
          tech concept-trans ,\
          completeness no-left } } ,\
      inst {\
        repr raw ,\
        mol aa ,\
        length 218 ,\
        seq-data\
          ncbieaa \"ADFTNTFYLLSSFPVELESPGLAEFLARLMEQCASLEELRLAFRPQMDPRQLSMMLMLA\
QSNPQLFALMGTRAGIARELERVEQQSRLEQLSAAELQSRNQGHWADWLQAYRARLDKDLEGAGDAAAWQAEHVRVMH\
ANNPKYVLRNYIAQNAIEAAERGDFSEVRRVLKLLETPYHCEAGAATDAEATEADGADGRQRSYSSKPPLWAAELCVT\
USS\" } ,\
      annot {\
        {\
          data\
            ftable {\
              {\
                data\
                  prot {\
                    name {\
                      \"selenoprotein O\" } } ,\
                partial TRUE ,\
                location\
                  int {\
                    from 0 ,\
                    to 217 ,\
                    id\
                      local\
                        id 32880229 ,\
                    fuzz-from\
                      lim lt } } } } } } } }\
";


static void TestOneCDS (CRef<CSeq_feat> feat, CScope * scope, int expected_frame)
{
    CCdregion& cds = feat->SetData().SetCdregion();
    CRef<CCode_break> cbr = cds.GetCode_break().front();
    CSeq_loc& cbr_loc = cbr->SetLoc();
    int frame = 0;
    if (cbr_loc.GetStrand() == eNa_strand_minus) {
      cbr_loc.SetInt().SetTo(feat->GetLocation().GetStart(eExtreme_Biological) - 3);
      cbr_loc.SetInt().SetFrom(cbr_loc.SetInt().SetTo() - 2);
    } else {
      cbr_loc.SetInt().SetFrom(feat->GetLocation().GetStart(eExtreme_Biological) + 3);
      cbr_loc.SetInt().SetTo(cbr_loc.SetInt().SetFrom() + 2);
    }

    CRef<CSeq_loc> p_loc = SourceToProduct(*feat, cbr_loc, fS2P_AllowTer, scope, &frame);
    BOOST_CHECK_EQUAL(expected_frame, frame);
    if (cbr_loc.GetStrand() == eNa_strand_minus) {
      cbr_loc.SetInt().SetTo(cbr_loc.GetInt().GetTo() - 1);
      cbr_loc.SetInt().SetFrom(cbr_loc.SetInt().SetTo() - 2);
    } else {
      cbr_loc.SetInt().SetFrom(cbr_loc.GetInt().GetFrom() + 1);
      cbr_loc.SetInt().SetTo(cbr_loc.SetInt().SetFrom() + 2);
    }
    p_loc = SourceToProduct(*feat, cbr_loc, fS2P_AllowTer, scope, &frame);
    expected_frame ++;
    if (expected_frame > 3) {
      expected_frame = 1;
    }
    BOOST_CHECK_EQUAL(expected_frame, frame);
    if (cbr_loc.GetStrand() == eNa_strand_minus) {
      cbr_loc.SetInt().SetTo(cbr_loc.GetInt().GetTo() - 1);
      cbr_loc.SetInt().SetFrom(cbr_loc.SetInt().SetTo() - 2);
    } else {
      cbr_loc.SetInt().SetFrom(cbr_loc.GetInt().GetFrom() + 1);
      cbr_loc.SetInt().SetTo(cbr_loc.SetInt().SetFrom() + 2);
    }
    p_loc = SourceToProduct(*feat, cbr_loc, fS2P_AllowTer, scope, &frame);
    expected_frame ++;
    if (expected_frame > 3) {
      expected_frame = 1;
    }
    BOOST_CHECK_EQUAL(expected_frame, frame);
}


BOOST_AUTO_TEST_CASE(Test_SourceToProductFrame)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_entry> s_Entry (new CSeq_entry);
    CNcbiIstrstream istr(sc_TestFramePlus);
    istr >> MSerial_AsnText >> *s_Entry;
    scope.AddTopLevelSeqEntry(*s_Entry);

    CRef<CSeq_annot> annot = s_Entry->SetSet().SetSeq_set().front()->SetSeq().SetAnnot().front();
    CRef<CSeq_feat> feat = annot->SetData().SetFtable().front();
    CCdregion& cds = feat->SetData().SetCdregion();
    cds.SetFrame (CCdregion::eFrame_one);
    TestOneCDS (feat, &scope, 1); 
    cds.SetFrame (CCdregion::eFrame_two);
    TestOneCDS (feat, &scope, 3); 
    cds.SetFrame (CCdregion::eFrame_three);
    TestOneCDS (feat, &scope, 2); 

    feat->SetLocation().SetStrand(eNa_strand_minus);
    cds.SetCode_break().front()->SetLoc().SetStrand(eNa_strand_minus);
    cds.SetFrame (CCdregion::eFrame_one);
    TestOneCDS (feat, &scope, 1); 
    cds.SetFrame (CCdregion::eFrame_two);
    TestOneCDS (feat, &scope, 3); 
    cds.SetFrame (CCdregion::eFrame_three);
    TestOneCDS (feat, &scope, 2); 

    
}


const char* sc_TestRevCmpEntryRaw = "\
Seq-entry ::= seq {\
  id {\
    local str \"seq_1\" } , \
  inst {\
    repr raw,\
    mol dna,\
    length 20,\
    seq-data iupacna \"ATGCATGCAAATTTGGGCCC\"\
  }\
}\
";


const char* sc_TestRevCmpEntryDelta = "\
Seq-entry ::= seq { \
  id {\
    local str \"seq_2\" } , \
  inst { \
    repr delta,\
    mol dna,\
    length 40,\
    ext delta {\
      literal {\
        length 8,\
        seq-data iupacna \"ATGCATGC\"},\
      literal {\
        length 5},\
      literal {\
        length 12,\
        seq-data iupacna \"AAATTTGGGCCC\"},\
      literal {\
        length 7,\
        seq-data gap { type unknown } },\
      literal {\
        length 8,\
        seq-data iupacna \"AATTGGCC\"}\
    }\
  }\
}\
";


const char* sc_TestRevCmpEntryDeltaFar = "\
Seq-entry ::= seq { \
  id {\
    local str \"seq_3\" } , \
  inst { \
    repr delta,\
    mol dna,\
    length 16,\
    ext delta {\
      loc int {\
        from 0,\
        to 3,\
        id local str \"seq_1\" },\
      literal {\
        length 5},\
      loc int {\
        from 13,\
        to 19,\
        id local str \"seq_1\" }\
    }\
  }\
}\
";


const char* sc_TestRevCmpError = "\
Seq-entry ::= seq { \
    id { \
        local str \"Euplotes\", \
        general { \
            db \"NCBIFILE\", \
            tag str \"Euplotes vannus GPx mRNA sequence-2013-05-16.sqn/Euplotes\" \
        }, \
        general { \
            db \"TMSMART\", \
            tag id 37854013 \
        }, \
        genbank { \
            accession \"KF049698\" \
        } \
    }, \
    inst { \
        repr raw, \
        mol rna, \
        length 549, \
        strand ss, \
        seq-data ncbi2na '3AA0FF0FDF0E8F4D775209D503777F89CDE40E33E3A41DD2\
DDF9E37E70A1C4090D78FB81BE7DC0EEA74702947389078788D4E30BD2E102BFD037F9FD5F9052\
F6B9D0894A8706288D227FE44A3031A2603D4BEFE300C86D0EA8631D3DADFC4F7909B0F5D5D4E3\
D741221C7A21354E81FE50BDF8D04982882DBE2CFF854880496820ECF5003E08177E8C80'H \
    }, \
    annot { \
    { \
        data ftable { \
        { \
            data rna { \
            type mRNA \
            }, \
            location int { \
            from 0, \
            to 548, \
            strand plus, \
            id local str \"Euplotes\" \
            } \
        } \
        } \
    } \
    } \
} \
";

BOOST_AUTO_TEST_CASE(Test_RevCompBioseq)
{
    CScope scope(*CObjectManager::GetInstance());
    {{
        CRef<CSeq_entry> s_Entry (new CSeq_entry);
        CNcbiIstrstream istr(sc_TestRevCmpEntryRaw);
        istr >> MSerial_AsnText >> *s_Entry;
        ReverseComplement(s_Entry->SetSeq().SetInst(), &scope);
        scope.AddTopLevelSeqEntry(*s_Entry);
        CBioseq_Handle bsh = scope.GetBioseqHandle(s_Entry->GetSeq());
        CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
        string rev = "";
        vec.GetSeqData(0, bsh.GetInst_Length(), rev);
        BOOST_CHECK_EQUAL(rev, "GGGCCCAAATTTGCATGCAT");
    }}
    {{
        CRef<CSeq_entry> s_Entry (new CSeq_entry);
        CNcbiIstrstream istr(sc_TestRevCmpEntryDelta);
        istr >> MSerial_AsnText >> *s_Entry;
        ReverseComplement(s_Entry->SetSeq().SetInst(), &scope);
        scope.AddTopLevelSeqEntry(*s_Entry);
        CBioseq_Handle bsh = scope.GetBioseqHandle(s_Entry->GetSeq());
        CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
        string rev = "";
        vec.GetSeqData(0, bsh.GetInst_Length(), rev);
        BOOST_CHECK_EQUAL(rev, "GGCCAATTNNNNNNNGGGCCCAAATTTNNNNNGCATGCAT");
    }}
    {{
        CRef<CSeq_entry> s_Entry (new CSeq_entry);
        CNcbiIstrstream istr(sc_TestRevCmpEntryDeltaFar);
        istr >> MSerial_AsnText >> *s_Entry;
        ReverseComplement(s_Entry->SetSeq().SetInst(), &scope);
        scope.AddTopLevelSeqEntry(*s_Entry);

        CBioseq_Handle bsh = scope.GetBioseqHandle(s_Entry->GetSeq());
        CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
        
        string rev = "";
        vec.GetSeqData(0, bsh.GetInst_Length(), rev);
        BOOST_CHECK_EQUAL(rev, "ATGCATGNNNNNGCCC");
    }}

    {{
        CRef<CSeq_entry> s_Entry (new CSeq_entry);
        CNcbiIstrstream istr(sc_TestRevCmpError);
        istr >> MSerial_AsnText >> *s_Entry;
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*s_Entry);
        CSeqVector orig_vec(scope.GetBioseqHandle(s_Entry->GetSeq()), CBioseq_Handle::eCoding_Iupac);
        string orig = "";
        orig_vec.GetSeqData(0, s_Entry->GetSeq().GetLength(), orig);
        scope.RemoveTopLevelSeqEntry(seh);

        ReverseComplement(s_Entry->SetSeq().SetInst(), &scope);
        scope.AddTopLevelSeqEntry(*s_Entry);

        CBioseq_Handle bsh = scope.GetBioseqHandle(s_Entry->GetSeq());
        CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);        
        
        string rev = "";
        vec.GetSeqData(0, bsh.GetInst_Length(), rev);
        string expected = "CTATCCAAGAGGTTCTTCAATTTTTGGAATACATTCTTCCGGCTGTTTCTCTGGGTCAAAATACTCAACG\
ACTTCTCCTTCGCTGTTGATCAAGAACTTGGCAAAGTTCCATGGGATGTCTCCAGTAGTCTCTGTTGAGG\
AATCATGGAGGGAGGAATTACGCTTGCAGAATGTAAAGACCGAATGAGTATCGTCCCCATTGACGTCTAT\
TTTATCAAACAACTGGAATTTCGCTCCGTATTTATCCTGTGCAAAAGCTCTGATCTCCTCGTTAGTCCCT\
GGCTCTTGAGCACCGAACTGGTTGCAAGGGAAAGCAAAGATTTGAAAACCTTTGTCACTGAACTTATCAT\
GGATCTCAGTCAGTTGCTCATAGTGGCCTTTAGTGAGCCCACATTTAGAAGCAACGTTCACAATCAGGAT\
TGCTTTGTAGTCCTTAGCAAGATCAGCAAGAGACTGAGAGTTGCCATCAATATCATTTGCAGATAGCTCA\
AAGAGAGATTTGGGAGCTTCTGGAGAGGATGAATCCATTAAGAAATTAAAATTCCCCAT";
        BOOST_CHECK_EQUAL(rev, expected);
    }}
}
