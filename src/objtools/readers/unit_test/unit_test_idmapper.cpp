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
* Author:  Mike DiCuccio
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objtools/readers/idmapper.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objmgr/util/sequence.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


///
/// Test simple seq-id -> seq-id mapping, given a mapping table of seq-id ->
/// seq-id
///
BOOST_AUTO_TEST_CASE(Test_SimpleIdMapper)
{
    CSeq_id id1("lcl|1");
    CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(id1);

    CSeq_id id2("lcl|2");
    CSeq_id_Handle idh2 = CSeq_id_Handle::GetHandle(id2);

    CIdMapper mapper;
    mapper.AddMapping(idh1, idh2);

    CSeq_id_Handle mapped_idh = mapper.Map(idh1);
    BOOST_CHECK_EQUAL(idh2, mapped_idh);

    ///
    /// by default, we echo the given handle if a mapping cannot be found.
    ///
    CSeq_id id3("lcl|3");
    CSeq_id_Handle idh3 = CSeq_id_Handle::GetHandle(id3);
    mapped_idh = mapper.Map(idh3);
    BOOST_CHECK_EQUAL(idh3, mapped_idh);
}


///
/// Test location mapping given a context in which we only map seq-ids
/// The goal here is to output the same location that we start with, except we
/// change the seq-ids
///
BOOST_AUTO_TEST_CASE(Test_SimpleLocMapper)
{
    CSeq_id id1("lcl|1");
    CSeq_id id2("lcl|2");

    CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(id1);
    CSeq_id_Handle idh2 = CSeq_id_Handle::GetHandle(id2);

    CIdMapper mapper;
    mapper.AddMapping(idh1, idh2);

    CSeq_loc loc1;
    loc1.SetInt().SetFrom(  0);
    loc1.SetInt().SetTo  (100);
    loc1.SetId(id1);

    CRef<CSeq_loc> loc2 = mapper.Map(loc1);

    CSeq_id_Handle mapped_idh = CSeq_id_Handle::GetHandle(*loc2->GetId());
    BOOST_CHECK_EQUAL(idh2, mapped_idh);
    BOOST_CHECK_EQUAL(loc1.GetInt().GetFrom(), loc2->GetInt().GetFrom());
    BOOST_CHECK_EQUAL(loc1.GetInt().GetTo(),   loc2->GetInt().GetTo());
}


///
/// More Complex case involving mapping of location ranges that will shift
/// positions
///
BOOST_AUTO_TEST_CASE(Test_ComplexLocMapper)
{
    CSeq_id id1("lcl|1");
    CSeq_id id2("lcl|2");

    CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(id1);
    CSeq_id_Handle idh2 = CSeq_id_Handle::GetHandle(id2);

    CSeq_loc loc1;
    loc1.SetInt().SetFrom( 10);
    loc1.SetInt().SetTo  (100);
    loc1.SetId(id1);

    CSeq_loc loc2;
    loc2.SetInt().SetFrom( 0);
    loc2.SetInt().SetTo  (90);
    loc2.SetId(id2);

    CIdMapper mapper;
    mapper.AddMapping(loc1, loc2);

    CSeq_loc loc3;
    loc3.SetPnt().SetPoint(55);
    loc3.SetId(id1);

    CRef<CSeq_loc> loc4 = mapper.Map(loc3);

    CSeq_id_Handle mapped_idh = CSeq_id_Handle::GetHandle(*loc4->GetId());
    BOOST_CHECK_EQUAL(idh2, mapped_idh);
    BOOST_CHECK_EQUAL(loc4->GetPnt().GetPoint(), (TSeqPos)45);
}


///
/// More Complex case involving mapping of location ranges that will shift
/// positions
///
BOOST_AUTO_TEST_CASE(Test_EvenMoreComplexLocMapper)
{
    CSeq_id id1("lcl|1");
    CSeq_id id2("lcl|2");
    CSeq_id id3("lcl|3");

    CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(id1);
    CSeq_id_Handle idh2 = CSeq_id_Handle::GetHandle(id2);
    CSeq_id_Handle idh3 = CSeq_id_Handle::GetHandle(id3);

    CSeq_loc loc1;
    loc1.SetInt().SetFrom( 10);
    loc1.SetInt().SetTo  (100);
    loc1.SetId(id1);

    CSeq_loc loc2;
    {{
         CRef<CSeq_loc> sub;

         sub.Reset(new CSeq_loc);
         sub->SetInt().SetFrom(0);
         sub->SetInt().SetTo(45);
         sub->SetId(id2);
         loc2.SetMix().Set().push_back(sub);

         sub.Reset(new CSeq_loc);
         sub->SetInt().SetFrom(0);
         sub->SetInt().SetTo(44);
         sub->SetId(id3);
         loc2.SetMix().Set().push_back(sub);
     }}

    CIdMapper mapper;
    mapper.AddMapping(loc1, loc2);

    {{
         CSeq_loc loc3;
         loc3.SetPnt().SetPoint(40);
         loc3.SetId(id1);

         CRef<CSeq_loc> loc4 = mapper.Map(loc3);

         CSeq_id_Handle mapped_idh = CSeq_id_Handle::GetHandle(*loc4->GetId());
         BOOST_CHECK_EQUAL(idh2, mapped_idh);
         BOOST_CHECK_EQUAL(loc4->GetPnt().GetPoint(), (TSeqPos)30);
     }}

    {{
         CSeq_loc loc3;
         loc3.SetPnt().SetPoint(60);
         loc3.SetId(id1);

         CRef<CSeq_loc> loc4 = mapper.Map(loc3);

         CSeq_id_Handle mapped_idh = CSeq_id_Handle::GetHandle(*loc4->GetId());
         BOOST_CHECK_EQUAL(idh3, mapped_idh);
         BOOST_CHECK_EQUAL(loc4->GetPnt().GetPoint(), (TSeqPos)4);
     }}

    {{
         CSeq_loc loc3;
         loc3.SetInt().SetFrom(45);
         loc3.SetInt().SetTo(60);
         loc3.SetId(id1);

         CRef<CSeq_loc> loc4 = mapper.Map(loc3);

         BOOST_CHECK_EQUAL(loc4->GetId(), (const CSeq_id*)NULL);
         BOOST_CHECK_EQUAL(loc4->Which(), CSeq_loc::e_Packed_int);

         CSeq_loc_CI loc_iter(*loc4);

         /// interval 1
         BOOST_CHECK_EQUAL(idh2, loc_iter.GetSeq_id_Handle());
         BOOST_CHECK_EQUAL(loc_iter.GetRange(), TSeqRange(35, 45));
         ++loc_iter;

         BOOST_CHECK_EQUAL(idh3, loc_iter.GetSeq_id_Handle());
         BOOST_CHECK_EQUAL(loc_iter.GetRange(), TSeqRange(0, 4));
         ++loc_iter;

         BOOST_CHECK( !loc_iter );

     }}
}


///
/// Test of built-in ID mapper
/// This also tests the configuration-based methods as well
///
BOOST_AUTO_TEST_CASE(Test_BuiltinIdMapper)
{
    CSeq_id id1("lcl|chr1");
    CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(id1);

    CSeq_id id2("lcl|2");
    CSeq_id_Handle idh2 = CSeq_id_Handle::GetHandle(id2);

    CIdMapperBuiltin mapper("hg19");

    {{
         CSeq_loc loc1;
         loc1.SetInt().SetFrom( 10);
         loc1.SetInt().SetTo  (100);
         loc1.SetId(id1);

         CSeq_id_Handle mapped_idh = mapper.Map(idh1);
         BOOST_CHECK_EQUAL(GI_CONST(224589800), mapped_idh.GetGi());

         CRef<CSeq_loc> loc2 = mapper.Map(loc1);
         mapped_idh = CSeq_id_Handle::GetHandle(*loc2->GetId());
         BOOST_CHECK_EQUAL(GI_CONST(224589800), mapped_idh.GetGi());
         BOOST_CHECK_EQUAL(loc2->GetInt().GetFrom(), (TSeqPos)10);
         BOOST_CHECK_EQUAL(loc2->GetInt().GetTo(), (TSeqPos)100);
     }}

    {{
         CSeq_loc loc2;
         loc2.SetInt().SetFrom( 10);
         loc2.SetInt().SetTo  (100);
         loc2.SetId(id2);

         CSeq_id_Handle mapped_idh = mapper.Map(idh2);
         BOOST_CHECK_EQUAL(GI_CONST(224589811), mapped_idh.GetGi());

         CRef<CSeq_loc> loc3 = mapper.Map(loc2);
         mapped_idh = CSeq_id_Handle::GetHandle(*loc3->GetId());
         BOOST_CHECK_EQUAL(GI_CONST(224589811), mapped_idh.GetGi());
         BOOST_CHECK_EQUAL(loc3->GetInt().GetFrom(), (TSeqPos)10);
         BOOST_CHECK_EQUAL(loc3->GetInt().GetTo(), (TSeqPos)100);
     }}
}

