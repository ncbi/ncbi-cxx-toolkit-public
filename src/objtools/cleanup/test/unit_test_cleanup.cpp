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
* Author:  Justin Foley, NCBI
*
* File Description:
*   Sample unit tests file for cleanup.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>

#include <corelib/test_boost.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/cleanup/cleanup.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);





static CRef<CBioseq> s_MakeProtein()  // RW-2486
{
    auto pBioseq = Ref(new CBioseq());
    pBioseq->SetInst().SetMol(CSeq_inst::eMol_aa);
    pBioseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    string seq_data{"MPRKTEIN"};
    pBioseq->SetInst().SetSeq_data().SetIupacaa().Set(seq_data);
    pBioseq->SetInst().SetLength(seq_data.size());
    pBioseq->SetInst().SetTopology(CSeq_inst::eTopology_linear);
    
    
    auto pId = Ref(new CSeq_id());
    pId->SetLocal().SetStr("seqid");
    pBioseq->SetId().push_back(pId);

    // Add mol-info descriptor
    {   
        auto pDesc = Ref(new CSeqdesc());
        pDesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
        pDesc->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_right);
        pBioseq->SetDescr().Set().push_back(pDesc);
    }

    // Add BioSource descriptor
    {
        auto pDesc = Ref(new CSeqdesc());
        pDesc->SetSource().SetOrg().SetTaxname("dummy taxname");
        pBioseq->SetDescr().Set().push_back(pDesc);
    }

    // Add title descriptor
    {
        auto pDesc = Ref(new CSeqdesc());
        pDesc->SetTitle("ABCD [dummy taxname]");
        pBioseq->SetDescr().Set().push_back(pDesc);
    }

    // Add a protein feature
    auto pFeat1 = Ref(new CSeq_feat());
    pFeat1->SetData().SetProt().SetName().push_back("ABCD [dummy taxname]");
    pFeat1->SetLocation().SetInt().SetId(*pId);
    pFeat1->SetLocation().SetInt().SetFrom(0);
    pFeat1->SetLocation().SetInt().SetTo(seq_data.size()-1); 
    pFeat1->SetPartial(true);

    // Adding different feature types to check that the iteration over features
    // works as expected.
    auto pFeat2 = Ref(new CSeq_feat());
    pFeat2->Assign(*pFeat1);
    pFeat2->SetData().Reset();
    pFeat2->SetData().SetComment();
    pFeat2->SetComment("Dummy comment");

    auto pFeat3 = Ref(new CSeq_feat());
    pFeat3->Assign(*pFeat1);
    pFeat3->SetData().SetProt().SetName().push_back("EFGH [not a dummy taxname]");

    auto pAnnot = Ref(new CSeq_annot());
    pAnnot->SetData().SetFtable().push_back(move(pFeat1));
    pAnnot->SetData().SetFtable().push_back(move(pFeat2));
    pAnnot->SetData().SetFtable().push_back(move(pFeat3));
    pBioseq->SetAnnot().push_back(move(pAnnot));

    return pBioseq;
}


BOOST_AUTO_TEST_CASE(Test_RW_2486_OM) 
{
    auto pBioseq = s_MakeProtein();
    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    auto bsh = pScope->AddBioseq(*pBioseq);

    auto madeChange = CCleanup::AddPartialToProteinTitle(bsh);
    BOOST_CHECK(madeChange);

    CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_Title);    
    BOOST_CHECK_EQUAL(desc_ci->GetTitle(), "ABCD, partial [dummy taxname]");

    const auto& bioseq = *(bsh.GetCompleteBioseq());
    BOOST_CHECK(bioseq.IsSetAnnot() && bioseq.GetAnnot().front()->IsFtable());

    const auto& feat_list = bioseq.GetAnnot().front()->GetData().GetFtable();
    BOOST_CHECK_EQUAL(feat_list.size(), 3);

    auto it = feat_list.begin();
    {
        const auto& feat = **it;
        const string& name = feat.GetData().GetProt().GetName().front();
        BOOST_CHECK_EQUAL(name, "ABCD"); // check that '[taxname]' as been removed from end of string
    }

    ++it;
    {
        const auto& feat = **it;
        BOOST_CHECK(feat.GetData().IsComment());
    }

    ++it;
    {
        const auto& feat = **it;
        const auto& names = feat.GetData().GetProt().GetName();
        BOOST_CHECK_EQUAL(names.size(), 2);
        BOOST_CHECK_EQUAL(names.front(), "ABCD"); // check that '[taxname]' as been removed from end of string
        BOOST_CHECK_EQUAL(names.back(), "EFGH [not a dummy taxname]"); // nothing removed from end of string
    }
}


