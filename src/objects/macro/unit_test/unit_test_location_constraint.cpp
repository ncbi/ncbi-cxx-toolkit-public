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
* Author:  Colleen Bollin
*
* File Description:
*   Simple unit test for CString_constraint.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/macro/Location_constraint.hpp>
#include <objects/macro/Location_pos_constraint.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <util/util_misc.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_AUTO_INIT()
{
}

BOOST_AUTO_TEST_CASE(Test_Location_Distance_Constraint)
{
    CRef<CBioseq> seq(new CBioseq());
    seq->SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    seq->SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    seq->SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    seq->SetInst().SetLength(60);

    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr("seq1");
    seq->SetId().push_back(id);


    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetGene().SetLocus("X");
    feat->SetLocation().SetInt().SetId().Assign(*id);
    feat->SetLocation().SetInt().SetFrom(5);
    feat->SetLocation().SetInt().SetTo(50);

    CRef<CLocation_constraint> loc_con(new CLocation_constraint());
    loc_con->SetEnd5().SetDist_from_end(5);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd5().SetDist_from_end(3);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);
    loc_con->SetEnd5().SetMax_dist_from_end(10);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd5().SetMax_dist_from_end(3);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);
    loc_con->SetEnd5().SetMin_dist_from_end(3);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd5().SetMin_dist_from_end(10);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);

    feat->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    loc_con->SetEnd5().SetDist_from_end(10);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd5().SetDist_from_end(8);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);
    loc_con->SetEnd5().SetMax_dist_from_end(15);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd5().SetMax_dist_from_end(8);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);
    loc_con->SetEnd5().SetMin_dist_from_end(8);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd5().SetMin_dist_from_end(15);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);

    loc_con->ResetEnd5();
    loc_con->SetEnd3().SetDist_from_end(5);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd3().SetDist_from_end(3);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);
    loc_con->SetEnd3().SetMax_dist_from_end(10);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd3().SetMax_dist_from_end(3);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);
    loc_con->SetEnd3().SetMin_dist_from_end(3);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd3().SetMin_dist_from_end(10);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);

    feat->SetLocation().ResetStrand();
    loc_con->SetEnd3().SetDist_from_end(10);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd3().SetDist_from_end(8);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);
    loc_con->SetEnd3().SetMax_dist_from_end(15);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd3().SetMax_dist_from_end(8);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);
    loc_con->SetEnd3().SetMin_dist_from_end(8);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), true);
    loc_con->SetEnd3().SetMin_dist_from_end(15);
    BOOST_CHECK_EQUAL(loc_con->Match(*feat, CConstRef <CSeq_feat>(NULL), seq), false);

}


BOOST_AUTO_TEST_CASE(Test_IsEmpty)
{
    CRef<CLocation_constraint> loc_con(new CLocation_constraint());
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), true);
    loc_con->SetStrand(eStrand_constraint_any);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), true);
    loc_con->SetStrand(eStrand_constraint_plus);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), false);
    loc_con->SetStrand(eStrand_constraint_minus);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), false);
    loc_con->ResetStrand();

    loc_con->SetSeq_type(eSeqtype_constraint_any);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), true);
    loc_con->SetSeq_type(eSeqtype_constraint_nuc);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), false);
    loc_con->ResetSeq_type();

    loc_con->SetPartial5(ePartial_constraint_either);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), true);
    loc_con->SetPartial5(ePartial_constraint_partial);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), false);
    loc_con->ResetPartial5();

    loc_con->SetPartial3(ePartial_constraint_either);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), true);
    loc_con->SetPartial3(ePartial_constraint_partial);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), false);
    loc_con->ResetPartial3();

    loc_con->SetLocation_type(eLocation_type_constraint_any);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), true);
    loc_con->SetLocation_type(eLocation_type_constraint_joined);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), false);
    loc_con->ResetLocation_type();

    loc_con->SetEnd5().SetDist_from_end(5);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), false);
    loc_con->ResetEnd5();
    loc_con->SetEnd3().SetMax_dist_from_end(5);
    BOOST_CHECK_EQUAL(loc_con->IsEmpty(), false);
    loc_con->ResetEnd3();

}