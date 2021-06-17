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


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_whole)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3, wl2, wl3;
    wg2.SetWhole().SetGi(2);
    wg3.SetWhole().SetGi(3);
    wl2.SetWhole().SetLocal().SetStr("local2");
    wl3.SetWhole().SetLocal().SetStr("local3");

    BOOST_CHECK_EQUAL(Compare(wg2, wg2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(wg2, wl2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(wg2, wg3, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(wg2, wl3, scope, fCompareOverlapping), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_interval)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3;
    wg2.SetWhole().SetGi(2);
    wg3.SetWhole().SetGi(3);

    // Partial overlap
    CRef<CSeq_loc> i = MakeInterval(2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(wg2, *i, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i, wg2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, *i, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, wg3, scope, fCompareOverlapping), eNoOverlap);

    // Full bioseq
    i = MakeInterval(2, 0, 1441);
    BOOST_CHECK_EQUAL(Compare(wg2, *i, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*i, wg2, scope, fCompareOverlapping), eSame);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_packed_interval)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3;
    wg2.SetWhole().SetGi(2);
    wg3.SetWhole().SetGi(3);

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");

    CSeq_loc pki;
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(wg2, pki, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pki, wg2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, pki, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pki, wg3, scope, fCompareOverlapping), eNoOverlap);

    pki.SetPacked_int().AddInterval(gi3, 30, 40);
    BOOST_CHECK_EQUAL(Compare(wg2, pki, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pki, wg2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(wg3, pki, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pki, wg3, scope, fCompareOverlapping), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 0, 1441);
    pki.SetPacked_int().AddInterval(gi3, 0, 374);
    BOOST_CHECK_EQUAL(Compare(wg2, pki, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(pki, wg2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(wg3, pki, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(pki, wg3, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_point)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3;
    wg2.SetWhole().SetGi(2);
    wg3.SetWhole().SetGi(3);

    CRef<CSeq_loc> pt = MakePoint(2, 10);
    BOOST_CHECK_EQUAL(Compare(wg2, *pt, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*pt, wg2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, *pt, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*pt, wg3, scope, fCompareOverlapping), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_packed_point)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3, wl2, wl3;
    wg2.SetWhole().SetGi(2);
    wg3.SetWhole().SetGi(3);
    wl2.SetWhole().SetLocal().SetStr("local2");
    wl3.SetWhole().SetLocal().SetStr("local3");

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(2);
    pp.SetPacked_pnt().AddPoint(10);
    pp.SetPacked_pnt().AddPoint(20);
    pp.SetPacked_pnt().AddPoint(30);
    BOOST_CHECK_EQUAL(Compare(wg2, pp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pp, wg2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(wl3, pp, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pp, wl3, scope, fCompareOverlapping), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_mix)
{
    CScope* scope = &GetScope();

    CSeq_loc w;
    w.SetWhole().SetGi(2);

    // Check some basic cases
    CSeq_loc mix;
    mix.SetMix().Set().push_back(MakeInterval(3, 10, 20));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope, fCompareOverlapping), eNoOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 20));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope, fCompareOverlapping), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 20));
    mix.SetMix().Set().push_back(MakeInterval(2, 30, 40));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope, fCompareOverlapping), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 20));
    mix.SetMix().Set().push_back(MakeInterval(3, 30, 40));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope, fCompareOverlapping), eOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 0, 1441));
    mix.SetMix().Set().push_back(MakeInterval(3, 30, 40));
    BOOST_CHECK_EQUAL(Compare(w, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope, fCompareOverlapping), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 0, 1441));
    CRef<CSeq_loc> sub(new CSeq_loc);
    sub->SetWhole().SetGi(2);
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(w, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, w, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_whole_vs_bond)
{
    CScope* scope = &GetScope();

    CSeq_loc wg2, wg3;
    wg2.SetWhole().SetGi(2);
    wg3.SetWhole().SetGi(3);

    // B not set
    CRef<CSeq_loc> bond = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(wg2, *bond, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*bond, wg2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, *bond, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*bond, wg3, scope, fCompareOverlapping), eNoOverlap);

    // A and B on different bioseqs
    bond = MakeBond(2, 10, 3, 20);
    BOOST_CHECK_EQUAL(Compare(wg2, *bond, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*bond, wg2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(wg3, *bond, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*bond, wg3, scope, fCompareOverlapping), eOverlap);

    // A and B on the same bioseq
    bond = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(wg2, *bond, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*bond, wg2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(wg3, *bond, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*bond, wg3, scope, fCompareOverlapping), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_interval)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i2 = MakeInterval(2, 10, 20);
    CRef<CSeq_loc> i3 = MakeInterval(3, 10, 20);

    CRef<CSeq_loc> i = MakeInterval(2, 25, 35);
    // No overlap
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope, fCompareOverlapping), eNoOverlap);

    // Abutting but not overlapping
    i = MakeInterval(2, 0, 22);
    i2 = MakeInterval(2, 23, 40);
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope, fCompareOverlapping), eNoOverlap);

    i2 = MakeInterval(2, 10, 20);
    i = MakeInterval(2, 5, 15);
    // Partial overlap
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i3, *i, scope, fCompareOverlapping), eNoOverlap);

    i = MakeInterval(2, 5, 25);
    // Contained/contains
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope, fCompareOverlapping), eContains);
    i = MakeInterval(2, 10, 25); // same on the right
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope, fCompareOverlapping), eContains);
    i = MakeInterval(2, 5, 20); // same on the left
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope, fCompareOverlapping), eContains);

    i = MakeInterval(2, 10, 20);
    // Same
    BOOST_CHECK_EQUAL(Compare(*i2, *i, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*i, *i2, scope, fCompareOverlapping), eSame);
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
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eNoOverlap);

    // eNoOverlap + eContained = eOverlap
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eOverlap);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eOverlap);

    // eNoOverlap + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eNoOverlap + eSame = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eNoOverlap + eOverlap = eOverlap
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    pki.SetPacked_int().AddInterval(gi2, 15, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eOverlap);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eOverlap);

    // eContained + eContained = eContained
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 11, 13);
    pki.SetPacked_int().AddInterval(gi2, 15, 18);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContains);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContains);

    // eContained + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 11, 13);
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eContained + eSame = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 11, 13);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContains);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContains);

    // eContained + eOverlap = eOverlap
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 11, 13);
    pki.SetPacked_int().AddInterval(gi2, 15, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eOverlap);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eOverlap);

    // eContains + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    pki.SetPacked_int().AddInterval(gi2, 5, 25);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eContains + eSame = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eContains + eOverlap = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    pki.SetPacked_int().AddInterval(gi2, 15, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eSame + eSame = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContains);

    // eSame + eOverlap = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 15, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eOverlap + eOverlap = eOverlap
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 8, 13);
    pki.SetPacked_int().AddInterval(gi2, 16, 22);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eOverlap);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eOverlap);

    // eNoOverlap + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 25, 35);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eContained + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 12, 18);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContains);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContains);

    // eContains + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 9, 21);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);

    // eSame + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContains);

    // eOverlap + eContains = eContains
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 11, 19);
    pki.SetPacked_int().AddInterval(gi2, 15, 25);
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
    pki.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pki, *i2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*i2, pki, scope, fCompareOverlapping), eContained);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i2 = MakeInterval(2, 10, 20);
    CRef<CSeq_loc> i3 = MakeInterval(3, 10, 20);

    CRef<CSeq_loc> pt = MakePoint(2, 5);
    // No overlap
    BOOST_CHECK_EQUAL(Compare(*i2, *pt, scope, fCompareOverlapping), eNoOverlap);
    pt = MakePoint(2, 15);
    BOOST_CHECK_EQUAL(Compare(*i3, *pt, scope, fCompareOverlapping), eNoOverlap);

    // Overlap
    BOOST_CHECK_EQUAL(Compare(*i2, *pt, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*pt, *i2, scope, fCompareOverlapping), eContained);

    // Same - interval of length 1
    CRef<CSeq_loc> i1 = MakeInterval(2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(*pt, *i1, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*i1, *pt, scope, fCompareOverlapping), eSame);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_packed_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i2 = MakeInterval(2, 10, 20);
    CRef<CSeq_loc> i3 = MakeInterval(3, 10, 20);

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(2);
    pp.SetPacked_pnt().AddPoint(5);

    // No overlap
    BOOST_CHECK_EQUAL(Compare(*i2, pp, scope, fCompareOverlapping), eNoOverlap);
    pp.SetPacked_pnt().SetPoints().front() = 15;
    BOOST_CHECK_EQUAL(Compare(*i3, pp, scope, fCompareOverlapping), eNoOverlap);

    // Contained in the interval
    BOOST_CHECK_EQUAL(Compare(*i2, pp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pp, *i2, scope, fCompareOverlapping), eContained);
    
    // Overlap
    pp.SetPacked_pnt().AddPoint(5);
    pp.SetPacked_pnt().AddPoint(25);
    BOOST_CHECK_EQUAL(Compare(*i2, pp, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pp, *i2, scope, fCompareOverlapping), eOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_interval_vs_mix)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> i = MakeInterval(2, 20, 80);

    CSeq_loc mix;
    CRef<CSeq_loc> sub, sub2;

    mix.SetMix().Set().push_back(MakeInterval(3, 10, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eNoOverlap);

    // Whole
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetWhole().SetGi(2);
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContains);

    // Points
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakePoint(2, 50));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContained);
    mix.SetMix().Set().push_back(MakePoint(2, 60));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContained);
    mix.SetMix().Set().push_back(MakePoint(2, 150));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eOverlap);

    // Packed points - some more complicated cases
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_pnt().SetId().SetGi(2);
    sub->SetPacked_pnt().AddPoint(30);
    sub->SetPacked_pnt().AddPoint(60);
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContained);
    sub2.Reset(new CSeq_loc);
    sub2->SetPacked_pnt().SetId().SetGi(2);
    sub2->SetPacked_pnt().AddPoint(10);
    sub2->SetPacked_pnt().AddPoint(50);
    mix.SetMix().Set().push_back(sub2);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eOverlap);

    // Intervals
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 15));
    mix.SetMix().Set().push_back(MakeInterval(2, 85, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eNoOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 25));
    mix.SetMix().Set().push_back(MakeInterval(2, 55, 70));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 35));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eOverlap);

    // This results in eOverlap although the mix covers the whole interval.
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 55));
    mix.SetMix().Set().push_back(MakeInterval(2, 50, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 90));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 80));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 80));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContains);

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
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eOverlap);

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
        Compare(*i, mix, scope, fCompareOverlapping);
    }
    catch (...) {
    }
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContains);

    // Mixed sub-location types
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPnt().SetId().SetGi(2);
    sub->SetPnt().SetPoint(30);
    mix.SetMix().Set().push_back(MakePoint(2, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 35, 40));
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 45, 50)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 50, 55)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 90));
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 40, 50)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 50, 85)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eContains);

    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    mix.SetMix().Set().push_back(MakeInterval(2, 40, 50));
    mix.SetMix().Set().push_back(MakeInterval(2, 60, 70));
    mix.SetMix().Set().push_back(sub);
    mix.SetMix().Set().push_back(MakeBond(2, 50, 3, 40));
    BOOST_CHECK_EQUAL(Compare(*i, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *i, scope, fCompareOverlapping), eOverlap);
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
    BOOST_CHECK_EQUAL(Compare(*bA2, *i, scope, fCompareOverlapping), eNoOverlap);

    i = MakeInterval(2, 5, 15);
    // Overlap with one point (no B)
    BOOST_CHECK_EQUAL(Compare(*bA2, *i, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *bA2, scope, fCompareOverlapping), eContains);
    // Overlap with only one of A or B
    BOOST_CHECK_EQUAL(Compare(*bA2B3, *i, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *bA2B3, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*bA3B2, *i, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *bA3B2, scope, fCompareOverlapping), eOverlap);
    // B is on the same bioseq but out of range
    BOOST_CHECK_EQUAL(Compare(*bA2B2, *i, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*i, *bA2B2, scope, fCompareOverlapping), eOverlap);

    i = MakeInterval(2, 5, 25);
    // Overlap with both A and B
    BOOST_CHECK_EQUAL(Compare(*bA2B2, *i, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*i, *bA2B2, scope, fCompareOverlapping), eContains);
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
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eSame);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(gi2, 30, 40);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi3, 10, 20);
    pk2.SetPacked_int().AddInterval(gi2, 50, 60);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eNoOverlap);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 15, 20);
    pk1.SetPacked_int().AddInterval(gi2, 60, 70);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 5, 10);
    pk2.SetPacked_int().AddInterval(gi2, 50, 55);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eNoOverlap);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 40);
    pk1.SetPacked_int().AddInterval(gi2, 60, 90);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 20, 30);
    pk2.SetPacked_int().AddInterval(gi2, 70, 80);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eContained);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eContained);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 40);
    pk1.SetPacked_int().AddInterval(gi3, 60, 90);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 20, 30);
    pk2.SetPacked_int().AddInterval(gi3, 70, 80);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eContained);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eContained);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 40);
    pk1.SetPacked_int().AddInterval(gi2, 50, 70);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 20, 30);
    pk2.SetPacked_int().AddInterval(gi2, 60, 80);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eOverlap);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eOverlap);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(gi2, 50, 70);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 10, 20);
    pk2.SetPacked_int().AddInterval(gi2, 60, 80);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eOverlap);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eOverlap);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(gi2, 30, 40);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 10, 20);
    pk2.SetPacked_int().AddInterval(gi2, 30, 40);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eSame);
    // The order does not matter.
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eSame);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 20);
    pk1.SetPacked_int().AddInterval(gi2, 10, 90);
    pk1.SetPacked_int().AddInterval(gi2, 80, 90);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 10, 90);
    pk2.SetPacked_int().AddInterval(gi2, 50, 60);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eContains);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eContains);

    pk1.SetPacked_int().Set().clear();
    pk1.SetPacked_int().AddInterval(gi2, 10, 19);
    pk1.SetPacked_int().AddInterval(gi3, 20, 29);
    pk2.SetPacked_int().Set().clear();
    pk2.SetPacked_int().AddInterval(gi2, 20, 29);
    pk2.SetPacked_int().AddInterval(gi3, 10, 19);
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eNoOverlap);
    pk2.SetPacked_int().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(pk1, pk2, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pk2, pk1, scope, fCompareOverlapping), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_interval_vs_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> pt = MakePoint(2, 15);

    CSeq_id gi2("gi|2");
    CSeq_loc pki;

    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope, fCompareOverlapping), eNoOverlap);
    pki.SetPacked_int().AddInterval(gi2, 20, 25);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope, fCompareOverlapping), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope, fCompareOverlapping), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope, fCompareOverlapping), eSame);

    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, *pt, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*pt, pki, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_interval_vs_packed_point)
{
    CScope* scope = &GetScope();

    CSeq_loc pkp;
    pkp.SetPacked_pnt().SetId().SetGi(2);
    pkp.SetPacked_pnt().AddPoint(15);

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");

    CSeq_loc pki;

    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eNoOverlap);
    pki.SetPacked_int().AddInterval(gi2, 20, 25);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eSame);

    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eContains);

    pki.SetPacked_int().AddInterval(gi2, 25, 25);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eContained);

    pkp.SetPacked_pnt().AddPoint(25);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 17, 23);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 30, 40);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 5, 10);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eOverlap);

    // Complicated case: each interval contains just one of the
    // points.
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 21, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi3, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, pkp, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pkp, pki, scope, fCompareOverlapping), eOverlap);
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
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eNoOverlap);

    // Whole
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetWhole().SetGi(2);
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContains);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 0, 1441);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eSame);

    // Points
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 30, 40);
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakePoint(2, 15));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContained);
    mix.SetMix().Set().push_back(MakePoint(2, 35));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContained);
    mix.SetMix().Set().push_back(MakePoint(2, 150));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eOverlap);

    // Packed points - some more complicated cases
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_pnt().SetId().SetGi(2);
    sub->SetPacked_pnt().AddPoint(15);
    sub->SetPacked_pnt().AddPoint(33);
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContained);
    sub2.Reset(new CSeq_loc);
    sub2->SetPacked_pnt().SetId().SetGi(2);
    sub2->SetPacked_pnt().AddPoint(5);
    sub2->SetPacked_pnt().AddPoint(37);
    mix.SetMix().Set().push_back(sub2);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eOverlap);

    // Intervals
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 20, 80);
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 15));
    mix.SetMix().Set().push_back(MakeInterval(2, 85, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eNoOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 25));
    mix.SetMix().Set().push_back(MakeInterval(2, 55, 70));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 35));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eOverlap);

    // This results in eOverlap although the mix covers the whole interval.
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 55));
    mix.SetMix().Set().push_back(MakeInterval(2, 50, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eOverlap);

    // The same problem here.
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 60);
    pki.SetPacked_int().AddInterval(gi2, 56, 90);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 20, 80);
    pki.SetPacked_int().AddInterval(gi2, 30, 70);
    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 90));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 80));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 80));
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContains);

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
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eOverlap);

    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(3, 10, 30)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 10, 90)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 70, 90)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContains);

    // Mixed sub-location types
    mix.SetMix().Set().clear();
    sub.Reset(new CSeq_loc);
    sub->SetPnt().SetId().SetGi(2);
    sub->SetPnt().SetPoint(30);
    mix.SetMix().Set().push_back(MakePoint(2, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 35, 40));
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 55, 60)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 60, 65)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContained);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 40));
    sub.Reset(new CSeq_loc);
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 40, 50)->SetInt()));
    sub->SetPacked_int().Set().push_back(
        Ref(&MakeInterval(2, 50, 85)->SetInt()));
    mix.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(pki, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pki, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_interval_vs_bond)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> b = MakeBond(2, 15);

    CSeq_id gi2("gi|2");
    CSeq_id gi3("gi|3");

    CSeq_loc pki;

    pki.SetPacked_int().AddInterval(gi2, 1, 5);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eNoOverlap);
    pki.SetPacked_int().AddInterval(gi2, 20, 25);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eContained);

    // For bonds we only detect no-overlap/overlap/contained, not same.
    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eSame);

    pki.SetPacked_int().AddInterval(gi2, 15, 15);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eContains);

    b->SetBond().SetB().SetId().SetGi(2);
    b->SetBond().SetB().SetPoint(25);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 17, 23);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eContained);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 30, 40);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 5, 10);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eContained);

    b->SetBond().SetB().SetId().SetGi(3);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eNoOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi2, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eOverlap);

    pki.SetPacked_int().Set().clear();
    pki.SetPacked_int().AddInterval(gi2, 10, 20);
    pki.SetPacked_int().AddInterval(gi3, 20, 30);
    BOOST_CHECK_EQUAL(Compare(pki, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pki, scope, fCompareOverlapping), eContained);
}


BOOST_AUTO_TEST_CASE(Test_Compare_point_vs_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> p2a = MakePoint(2, 10);
    CRef<CSeq_loc> p2b = MakePoint(2, 15);
    CRef<CSeq_loc> p3 = MakePoint(3, 20);

    BOOST_CHECK_EQUAL(Compare(*p2a, *p2a, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*p2a, *p2b, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(*p2a, *p3, scope, fCompareOverlapping), eNoOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_point_vs_packed_point)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> p = MakePoint(2, 5);

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(2);
    pp.SetPacked_pnt().AddPoint(10);

    // No overlap
    BOOST_CHECK_EQUAL(Compare(*p, pp, scope, fCompareOverlapping), eNoOverlap);
    BOOST_CHECK_EQUAL(Compare(pp, *p, scope, fCompareOverlapping), eNoOverlap);

    p = MakePoint(2, 10);
    // Single entry in packed points
    BOOST_CHECK_EQUAL(Compare(*p, pp, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(pp, *p, scope, fCompareOverlapping), eSame);

    pp.SetPacked_pnt().AddPoint(20);
    pp.SetPacked_pnt().AddPoint(30);
    // Multiple points
    BOOST_CHECK_EQUAL(Compare(*p, pp, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(pp, *p, scope, fCompareOverlapping), eContains);

    // Special case: all packed points are the same.
    // The first seq-loc contains the second one in any direction.
    pp.Reset();
    pp.SetPacked_pnt().SetId().SetGi(2);
    pp.SetPacked_pnt().AddPoint(10);
    pp.SetPacked_pnt().AddPoint(10);
    BOOST_CHECK_EQUAL(Compare(*p, pp, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pp, *p, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_point_vs_mix)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> p = MakePoint(2, 50);

    CSeq_loc mix;

    mix.SetMix().Set().push_back(MakeInterval(3, 10, 90));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope, fCompareOverlapping), eNoOverlap);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 40));
    mix.SetMix().Set().push_back(MakeInterval(2, 60, 90));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope, fCompareOverlapping), eNoOverlap);
    mix.SetMix().Set().push_back(MakeInterval(2, 40, 60));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *p, scope, fCompareOverlapping), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakePoint(2, 50));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *p, scope, fCompareOverlapping), eSame);
    mix.SetMix().Set().push_back(MakePoint(2, 50));
    BOOST_CHECK_EQUAL(Compare(*p, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *p, scope, fCompareOverlapping), eContains);
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
    BOOST_CHECK_EQUAL(Compare(*bA2, *p, scope, fCompareOverlapping), eNoOverlap);

    p = MakePoint(2, 10);
    // Overlap with A
    BOOST_CHECK_EQUAL(Compare(*bA2, *p, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*bA2B3, *p, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*p, *bA2B3, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*bA2B2, *p, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*p, *bA2B2, scope, fCompareOverlapping), eContained);

    // Special case - A==B, contains in both directions.
    BOOST_CHECK_EQUAL(Compare(*bA2B2eq, *p, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*p, *bA2B2eq, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_point_vs_packed_point)
{
    CScope* scope = &GetScope();

    CSeq_loc pp1;
    pp1.SetPacked_pnt().SetId().SetGi(2);
    pp1.SetPacked_pnt().AddPoint(10);

    CSeq_loc pp2;
    pp2.SetPacked_pnt().SetId().SetGi(3);
    pp2.SetPacked_pnt().AddPoint(10);

    // No overlap for different bioseqs
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eNoOverlap);
    pp1.SetPacked_pnt().AddPoint(20);
    pp2.SetPacked_pnt().SetId().SetGi(2);
    pp2.SetPacked_pnt().SetPoints().front() = 5;
    pp2.SetPacked_pnt().AddPoint(15);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eNoOverlap);
    pp1.SetPacked_pnt().AddPoint(30);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eNoOverlap);

    // Same
    pp2.Assign(pp1);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eSame);

    // Overlap
    pp2.SetPacked_pnt().SetPoints().clear();
    pp2.SetPacked_pnt().AddPoint(5);
    pp2.SetPacked_pnt().AddPoint(10);
    pp2.SetPacked_pnt().AddPoint(15);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope, fCompareOverlapping), eOverlap);

    // Contained/contains
    pp1.SetPacked_pnt().AddPoint(40); // 10, 20, 30, 40
    pp2.SetPacked_pnt().SetPoints().clear();
    pp2.SetPacked_pnt().AddPoint(20); // 20
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope, fCompareOverlapping), eContained);
    pp2.SetPacked_pnt().AddPoint(30); // 20, 30
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope, fCompareOverlapping), eContained);
    // Wrong order of points should still work
    pp2.SetPacked_pnt().AddPoint(10); // 20, 30, 10
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope, fCompareOverlapping), eContained);
    // Duplicate points - same result
    pp2.SetPacked_pnt().AddPoint(20); // 20, 30, 10, 20
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope, fCompareOverlapping), eContained);

    // Special case - due to duplicate points both sets contain each other
    // but are not equal
    pp2.SetPacked_pnt().AddPoint(40); // 20, 30, 10, 20, 40
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope, fCompareOverlapping), eContains);

    // Now they just overlap
    pp1.SetPacked_pnt().AddPoint(45);
    pp2.SetPacked_pnt().AddPoint(5);
    BOOST_CHECK_EQUAL(Compare(pp1, pp2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pp2, pp1, scope, fCompareOverlapping), eOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_point_vs_mix)
{
    CScope* scope = &GetScope();

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(2);
    pp.SetPacked_pnt().AddPoint(25);
    pp.SetPacked_pnt().AddPoint(85);

    CSeq_loc mix;

    // Each point is contained in a separate sub-location.
    mix.SetMix().Set().push_back(MakeInterval(2, 30, 70));
    BOOST_CHECK_EQUAL(Compare(pp, mix, scope, fCompareOverlapping), eNoOverlap);
    pp.SetPacked_pnt().AddPoint(50);
    BOOST_CHECK_EQUAL(Compare(pp, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(pp, mix, scope, fCompareOverlapping), eOverlap);
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 30));
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(pp, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, pp, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_packed_point_vs_bond)
{
    CScope* scope = &GetScope();

    CSeq_loc pp;
    pp.SetPacked_pnt().SetId().SetGi(2);
    pp.SetPacked_pnt().AddPoint(10);

    CRef<CSeq_loc> b = MakeBond(3, 10);
    // No overlap
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eNoOverlap);
    b = MakeBond(2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eNoOverlap);

    b = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eSame);
    b = MakeBond(2, 10, 3, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContains);
    b = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContains);
    b = MakeBond(3, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContains);
    b = MakeBond(2, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContains);

    pp.SetPacked_pnt().AddPoint(20);
    b = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContained);
    b = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eSame);
    // The order of points does not matter.
    b = MakeBond(2, 20, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eSame);
    b = MakeBond(2, 10, 3, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eOverlap);
    b = MakeBond(3, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eOverlap);
    b = MakeBond(2, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContained);
    b = MakeBond(2, 20, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContained);
    b = MakeBond(2, 10, 2, 40);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eOverlap);

    pp.SetPacked_pnt().AddPoint(30);
    b = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContained);
    b = MakeBond(2, 10, 2, 30);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContained);
    b = MakeBond(2, 30, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContained);
    b = MakeBond(2, 10, 3, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eOverlap);
    b = MakeBond(3, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eOverlap);
    b = MakeBond(2, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContained);
    b = MakeBond(2, 10, 2, 40);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eOverlap);

    pp.SetPacked_pnt().SetPoints().clear();
    pp.SetPacked_pnt().AddPoint(10);
    pp.SetPacked_pnt().AddPoint(20);
    pp.SetPacked_pnt().AddPoint(20);
    b = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContained);
    b = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContains);
    b = MakeBond(2, 10, 2, 30);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eOverlap);
    b = MakeBond(2, 30, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eOverlap);
    b = MakeBond(2, 10, 2, 20);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContains);
    b = MakeBond(2, 20, 2, 10);
    BOOST_CHECK_EQUAL(Compare(pp, *b, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b, pp, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_mix_vs_mix)
{
    CScope* scope = &GetScope();

    CSeq_loc mix1, mix2;

    mix1.SetMix().Set().push_back(MakeInterval(2, 10, 20));
    mix1.SetMix().Set().push_back(MakeInterval(2, 50, 60));
    mix2.SetMix().Set().push_back(MakeInterval(2, 30, 40));
    mix2.SetMix().Set().push_back(MakeInterval(2, 70, 80));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope, fCompareOverlapping), eNoOverlap);
    mix1.SetMix().Set().push_back(MakeInterval(3, 30, 40));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope, fCompareOverlapping), eNoOverlap);
    mix2.SetMix().Set().push_front(MakeInterval(3, 20, 35));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope, fCompareOverlapping), eOverlap);

    mix1.SetMix().Set().clear();
    mix1.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    mix1.SetMix().Set().push_back(MakeInterval(2, 50, 70));
    mix2.SetMix().Set().clear();
    mix2.SetMix().Set().push_back(MakeInterval(2, 60, 65));
    mix2.SetMix().Set().push_back(MakeInterval(2, 20, 25));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope, fCompareOverlapping), eContained);

    mix2.SetMix().Set().push_back(MakeInterval(2, 50, 70));
    mix2.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope, fCompareOverlapping), eContains);

    // Empty should not change anything (?)
    CRef<CSeq_loc> sub(new CSeq_loc);
    sub->SetEmpty().SetGi(2);
    mix2.SetMix().Set().push_back(sub);
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope, fCompareOverlapping), eContains);

    mix2.SetMix().Set().push_back(MakePoint(3, 100));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope, fCompareOverlapping), eContains);

    mix1.SetMix().Set().push_back(MakePoint(2, 110));
    BOOST_CHECK_EQUAL(Compare(mix1, mix2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix2, mix1, scope, fCompareOverlapping), eOverlap);
}


BOOST_AUTO_TEST_CASE(Test_Compare_mix_vs_bond)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> b = MakeBond(2, 50);

    CSeq_loc mix;

    // Each point is contained in a separate sub-location.
    mix.SetMix().Set().push_back(MakeInterval(2, 10, 30));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eNoOverlap);
    mix.SetMix().Set().push_back(MakeInterval(2, 70, 90));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eNoOverlap);
    mix.SetMix().Set().push_back(MakeInterval(2, 40, 60));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eContains);

    b = MakeBond(2, 20, 3, 40);
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eOverlap);

    mix.SetMix().Set().push_back(MakeInterval(3, 30, 50));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eContains);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakePoint(2, 20));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eContained);

    mix.SetMix().Set().push_back(MakePoint(3, 40));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eSame);
    mix.SetMix().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eSame);

    mix.SetMix().Set().clear();
    mix.SetMix().Set().push_back(MakeInterval(2, 20, 20));
    mix.SetMix().Set().push_back(MakeInterval(3, 40, 40));
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eSame);
    mix.SetMix().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eSame);
    mix.SetMix().Set().push_back(MakeInterval(3, 40, 40));
    mix.SetMix().Set().reverse();
    BOOST_CHECK_EQUAL(Compare(*b, mix, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(mix, *b, scope, fCompareOverlapping), eContains);
}


BOOST_AUTO_TEST_CASE(Test_Compare_bond_vs_bond)
{
    CScope* scope = &GetScope();

    CRef<CSeq_loc> b1 = MakeBond(2, 10);
    CRef<CSeq_loc> b2 = MakeBond(3, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eNoOverlap);

    b2 = MakeBond(2, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eNoOverlap);

    b2 = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eSame);

    b2 = MakeBond(2, 10, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eContains);
    b1 = MakeBond(2, 10, 3, 25);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eOverlap);
    b1 = MakeBond(2, 10, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eSame);

    b2 = MakeBond(2, 15, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eOverlap);
    b2 = MakeBond(2, 10, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eSame);
    // The order or ranges is not important
    b2 = MakeBond(3, 20, 2, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eSame);

    b1 = MakeBond(2, 10, 2, 10);
    b2 = MakeBond(2, 10, 2, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eSame);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eSame);

    b1 = MakeBond(2, 10, 3, 20);
    b2 = MakeBond(3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eContained);
    b2 = MakeBond(3, 20, 2, 15);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eOverlap);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eOverlap);

    b1 = MakeBond(2, 10, 2, 10);
    b2 = MakeBond(2, 10);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eContains);
    b2 = MakeBond(2, 10, 3, 15);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eContained);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eContains);

    b1 = MakeBond(2, 15, 3, 20);
    b2 = MakeBond(3, 20, 3, 20);
    BOOST_CHECK_EQUAL(Compare(*b1, *b2, scope, fCompareOverlapping), eContains);
    BOOST_CHECK_EQUAL(Compare(*b2, *b1, scope, fCompareOverlapping), eContained);
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
