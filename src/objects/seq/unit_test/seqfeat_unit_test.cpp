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
 * Author:  Michael Kornbluh, NCBI
 *
 * File Description:
 *   Unit test for CSeqFeatData and some closely related code
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>


#include <corelib/ncbimisc.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/ncbi_autoinit.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <boost/test/parameterized_test.hpp>
#include <util/util_exception.hpp>
#include <util/util_misc.hpp>
#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {

    CRef<CUser_object> s_CreateSimpleUserObj(const string & type_str)
    {
        CRef<CUser_object> uobj(new CUser_object);
        uobj->SetType().SetStr(type_str);
        return uobj;
    }
    
}

BOOST_AUTO_TEST_CASE(Test_FindAddRemoveExts)
{
    CAutoInitRef<CSeq_feat> seq_feat;

    const static string exts_names[] = {
        "ext1",
        "ext2",
        "ext3",
    };

    // add them
    for( size_t idx = 0; idx < ArraySize(exts_names); ++idx ) {
        seq_feat->AddExt(s_CreateSimpleUserObj(exts_names[idx]));
    }
    BOOST_CHECK_EQUAL(seq_feat->GetExts().size(), ArraySize(exts_names));

    // make sure can find them all
    for( size_t idx = 0; idx <  ArraySize(exts_names); ++idx ) {
        CRef<CUser_object> uobj = seq_feat->FindExt(exts_names[idx]);
        BOOST_CHECK_EQUAL(uobj->GetType().GetStr(), exts_names[idx]);
    }

    // test remove
    const static size_t idx_of_name_to_remove = 1;
    seq_feat->RemoveExt(exts_names[idx_of_name_to_remove]);
    BOOST_CHECK_EQUAL(seq_feat->GetExts().size(),
                      (ArraySize(exts_names) - 1));
    for( size_t idx = 0; idx <  ArraySize(exts_names); ++idx ) {
        CRef<CUser_object> uobj = seq_feat->FindExt(exts_names[idx]);
        if( idx == idx_of_name_to_remove ) {
            BOOST_CHECK( ! uobj );
        } else {
            BOOST_CHECK_EQUAL(uobj->GetType().GetStr(), exts_names[idx]);
        }
    }

    // test plain Ext instead of Exts
    static const string plain_ext_name = "foo";
    seq_feat->SetExt().SetType().SetStr(plain_ext_name);
    BOOST_CHECK( !! seq_feat->IsSetExt() );
    BOOST_CHECK_EQUAL(
        seq_feat->FindExt(plain_ext_name)->GetType().GetStr(),
        plain_ext_name);
    seq_feat->RemoveExt(plain_ext_name);
    BOOST_CHECK( ! seq_feat->IsSetExt() );

    // check "CombinedFeatureUserObjects", which allows packing multiple
    // exts into the plain TExt object
    static const string combo_ext_name = "CombinedFeatureUserObjects";
    static const string combo_test_ext_names[] = {
        "combo1",
        "combo2",
        "combo3",
        "combo4",
    };
    seq_feat->SetExt().SetType().SetStr(combo_ext_name);
    CSeq_feat::TExt::TData & combo_data = seq_feat->SetExt().SetData();
    for( size_t idx = 0; idx < ArraySize(combo_test_ext_names) ; ++idx ) {
        CRef<CUser_field> new_elem_ext(new CUser_field);
        new_elem_ext->SetData().SetObject().SetType().SetStr(combo_test_ext_names[idx]);
        combo_data.push_back(new_elem_ext);
    }
    BOOST_CHECK_EQUAL(
        seq_feat->GetExt().GetData().size(), ArraySize(combo_test_ext_names));

    // check CombinedFeatureUserObjects removal:
    static const size_t combo_idx_to_remove = 2;
    seq_feat->RemoveExt(combo_test_ext_names[combo_idx_to_remove]);
    BOOST_CHECK_EQUAL(
        seq_feat->GetExt().GetData().size(),
        (ArraySize(combo_test_ext_names) - 1));
    for( size_t idx = 0; idx < ArraySize(combo_test_ext_names) ; ++idx ) {
        CRef<CUser_object> uobj = seq_feat->FindExt(combo_test_ext_names[idx]);
        if( idx == combo_idx_to_remove ) {
            BOOST_CHECK( ! uobj );
        } else {
            BOOST_CHECK_EQUAL(uobj->GetType().GetStr(),
                              combo_test_ext_names[idx]);
        }
    }
}
