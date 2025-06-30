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
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>

#include "../newcleanupp.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);


static CRef<CSeq_loc> s_CreateNewMix() 
{
    auto pLoc = Ref(new CSeq_loc());
    pLoc->SetMix();
    return pLoc;
}


BOOST_AUTO_TEST_CASE(Test_RW_2301)
{
    auto pNullLoc = Ref(new CSeq_loc());
    pNullLoc->SetNull();

    CNewCleanup_imp cleanup_imp(Ref(new CCleanupChange()));
    {
        // mix(null, null) -> null
        auto pLoc = Ref(new CSeq_loc());
        auto& mix = pLoc->SetMix();
        mix.AddSeqLoc(*pNullLoc);
        mix.AddSeqLoc(*pNullLoc);
        BOOST_CHECK_EQUAL(mix.Get().size(), 2);
        cleanup_imp.SeqLocBC(*pLoc);
        // Check that a mix consisting of 2 Nulls is converted to a Null loc
        BOOST_CHECK(pLoc->IsNull());
    }

    auto pId = Ref(new CSeq_id());
    pId->SetLocal().SetStr("dummyId");
    {
        // mix(mix(null, interval, null)) -> interval
        // Add an interval
        auto pLoc = s_CreateNewMix();
        auto pInnerMix = s_CreateNewMix();
        auto& innerMix = pInnerMix->SetMix();
        innerMix.AddSeqLoc(*pNullLoc);
        innerMix.AddInterval(*pId, 25, 1006);
        innerMix.AddSeqLoc(*pNullLoc);

        BOOST_CHECK_EQUAL(innerMix.Get().size(), 3);
        pLoc->SetMix().Set().push_back(pInnerMix);

        BOOST_CHECK_EQUAL(pLoc->GetMix().Get().size(), 1);
        cleanup_imp.SeqLocBC(*pLoc);
        BOOST_CHECK(pLoc->IsInt());
    }

    {
        // mix(interval, mix(null, interval), interval) -> mix(interval, null, interval, null, interval)
        auto pLoc = s_CreateNewMix();
        auto& mix = pLoc->SetMix();

        auto pInnerMix = s_CreateNewMix();
        auto& innerMix = pInnerMix->SetMix();
        pLoc->SetMix().AddInterval(*pId, 2, 20);
        innerMix.AddSeqLoc(*pNullLoc);
        innerMix.AddInterval(*pId, 22, 30);
        mix.Set().push_back(pInnerMix);
        mix.AddInterval(*pId, 32, 40);

        cleanup_imp.SeqLocBC(*pLoc);

        BOOST_CHECK(pLoc->IsMix());
        BOOST_CHECK_EQUAL(pLoc->GetMix().Get().size(), 5);
        auto it = pLoc->GetMix().Get().begin();
        BOOST_CHECK((*it)->IsInt());
        ++it;
        BOOST_CHECK((*it)->IsNull()); // The '*' is critical. Check for a null location, not an empty CRef.
        ++it;
        BOOST_CHECK((*it)->IsInt());
        ++it;
        BOOST_CHECK((*it)->IsNull()); // The '*' is critical. Check for a null location, not an empty CRef.
        ++it;
        BOOST_CHECK((*it)->IsInt());
    }
}


CRef<CBioseq> s_MakeNucSeq()
{
    auto pBioseq = Ref(new CBioseq());
    pBioseq->SetInst().SetMol(CSeq_inst::eMol_dna);
    pBioseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    string seq_data{"ACTGCCAAAGAGTCAA"};
    pBioseq->SetInst().SetSeq_data().SetIupacna().Set(seq_data);
    pBioseq->SetInst().SetLength(seq_data.size());

    auto pId = Ref(new CSeq_id());
    pId->SetLocal().SetStr("nuc");
    pBioseq->SetId().push_back(pId);

    return pBioseq;
}

BOOST_AUTO_TEST_CASE(Test_RemoveSingleStrand) 
{
    auto pBioseq = s_MakeNucSeq();
    
    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    auto bsh = pScope->AddBioseq(*pBioseq);

    CNewCleanup_imp cleanup_imp(Ref(new CCleanupChange()));
    cleanup_imp.SetScope(*pScope);

    // Strangely, the strand can be set with eStrand_not_set.
    // In this case, RemoveSingleStrand() will reset the strand field.
    bsh.GetEditHandle().SetInst_Strand(CSeq_inst::eStrand_not_set);

    BOOST_CHECK(pBioseq->GetInst().IsSetStrand());
    BOOST_CHECK(bsh.IsSetInst_Strand());
    {
        auto madeChange = cleanup_imp.RemoveSingleStrand(bsh);
        BOOST_CHECK(madeChange);
    }

    BOOST_CHECK(! pBioseq->GetInst().IsSetStrand());
    BOOST_CHECK(! bsh.IsSetInst_Strand());

    // Now test eStrand_ss
    //
    // Need a biosource descriptor with lineage to trigger the cleanup operation
    auto pDesc = Ref(new CSeqdesc());
    pDesc->SetSource().SetOrg().SetOrgname().SetLineage() = "dummy lineage";

    bsh.GetEditHandle().SetInst_Strand(CSeq_inst::eStrand_ss);
    bsh.GetEditHandle().AddSeqdesc(*pDesc);

    BOOST_CHECK(pBioseq->GetInst().IsSetStrand());
    BOOST_CHECK_EQUAL(pBioseq->GetInst().GetStrand(), CSeq_inst::eStrand_ss);
    BOOST_CHECK(bsh.IsSetInst_Strand());
    {
        auto madeChange = cleanup_imp.RemoveSingleStrand(bsh);
        BOOST_CHECK(madeChange);
    }

    BOOST_CHECK(! pBioseq->GetInst().IsSetStrand());
    BOOST_CHECK(! bsh.IsSetInst_Strand());
}


static CRef<CBioseq> s_MakeProtein()
{
    auto pBioseq = Ref(new CBioseq());
    pBioseq->SetInst().SetMol(CSeq_inst::eMol_aa);
    pBioseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    string seq_data{"MPRKTEIN"};
    pBioseq->SetInst().SetSeq_data().SetIupacaa().Set(seq_data);
    pBioseq->SetInst().SetLength(seq_data.size());
    pBioseq->SetInst().SetTopology(CSeq_inst::eTopology_linear);
    
    
    auto pId = Ref(new CSeq_id());
    pId->SetGibbsq(1);
    pBioseq->SetId().push_back(pId);

    // Add mol-info descriptor
    {   
        auto pDesc = Ref(new CSeqdesc());
        pDesc->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_complete);
        pBioseq->SetDescr().Set().push_back(pDesc);
    }

    // Add title descriptor to trigger changes to partialness
    {
        auto pDesc = Ref(new CSeqdesc());
        pDesc->SetTitle("{N-terminal}");
        pBioseq->SetDescr().Set().push_back(pDesc);
    }

    // Add a protein feature
    auto pFeat = Ref(new CSeq_feat());
    pFeat->SetData().SetProt().SetName().push_back("dummy protein");
    pFeat->SetLocation().SetInt().SetId(*pId);
    pFeat->SetLocation().SetInt().SetFrom(0);
    pFeat->SetLocation().SetInt().SetTo(seq_data.size()-1); 
    pFeat->SetPartial(true);

    auto pAnnot = Ref(new CSeq_annot());
    pAnnot->SetData().SetFtable().push_back(move(pFeat));
    pBioseq->SetAnnot().push_back(move(pAnnot));

    return pBioseq;
}



BOOST_AUTO_TEST_CASE(Test_ProtSeqBC)
{
    auto pProtSeq = s_MakeProtein();
    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    auto bsh = pScope->AddBioseq(*pProtSeq);

    CNewCleanup_imp cleanup_imp(Ref(new CCleanupChange()));
    cleanup_imp.SetScope(*pScope);

    BOOST_CHECK_EQUAL(pProtSeq->GetInst().GetTopology(),CSeq_inst::eTopology_linear);

    cleanup_imp.ProtSeqBC(bsh);

    // Check that linear topology has been cleared
    BOOST_CHECK(! pProtSeq->GetInst().IsSetTopology());

    BOOST_CHECK(pProtSeq->IsSetDescr());
    const auto& descrList = pProtSeq->GetDescr().Get();
    auto molinfoIt = find_if(descrList.begin(), descrList.end(), 
        [](CRef<CSeqdesc> pDesc){ return pDesc && pDesc->IsMolinfo(); });

    BOOST_CHECK_EQUAL((*molinfoIt)->GetMolinfo().GetCompleteness(), CMolInfo::eCompleteness_no_right);
    
    const auto pProtFeat = pProtSeq->GetAnnot().front()->GetData().GetFtable().front();
    BOOST_CHECK(! pProtFeat->GetLocation().IsPartialStart(eExtreme_Biological));
    BOOST_CHECK(pProtFeat->GetLocation().IsPartialStop(eExtreme_Biological));
}


BOOST_AUTO_TEST_CASE(Test_SeqIdBC) // RW-2482
{
    auto pId = Ref(new CSeq_id());
    pId->SetLocal().SetStr("  dummy id  ");

    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));

    auto pBioseq = s_MakeNucSeq();
    pBioseq->SetId().push_back(pId);
    auto bsh = pScope->AddBioseq(*pBioseq);

    CNewCleanup_imp cleanup_imp(Ref(new CCleanupChange()));
    cleanup_imp.SetScope(*pScope);

    auto pNewId = Ref(new CSeq_id());
    pNewId->Assign(*pId);

    cleanup_imp.SeqIdBC(*pNewId);
    bsh.GetEditHandle().RemoveId(CSeq_id_Handle::GetHandle(*pId));
    bsh.GetEditHandle().AddId(CSeq_id_Handle::GetHandle(*pNewId));

    BOOST_CHECK_EQUAL(pNewId->GetLocal().GetStr(), "dummy id");
    BOOST_CHECK(bsh.GetBioseqCore()->GetId().back()->Match(*pNewId)); 
    BOOST_CHECK(bsh.GetScope().Exists(*pNewId)); 
}


BOOST_AUTO_TEST_CASE(Test_BasicCleanupSeqIds) // RW-2482
{
    auto pId = Ref(new CSeq_id());
    pId->SetLocal().SetStr("  dummy id  ");

    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));

    auto pBioseq = s_MakeNucSeq();
    pBioseq->SetId().push_back(pId);
    auto bsh = pScope->AddBioseq(*pBioseq);

    CNewCleanup_imp cleanup_imp(Ref(new CCleanupChange()));
    cleanup_imp.SetScope(*pScope);

    cleanup_imp.BasicCleanupSeqIds(bsh);

    // Original id is no longer in scope
    BOOST_CHECK(! bsh.GetScope().Exists(*pId));

    auto pNewId = Ref(new CSeq_id());
    pNewId->SetLocal().SetStr("dummy id");

    BOOST_CHECK(pBioseq->GetId().back()->Match(*pNewId)); 
    BOOST_CHECK(bsh.GetScope().Exists(*pNewId));
}


BOOST_AUTO_TEST_CASE(Test_BasicCleanupDeltaExt) // RW-2507
{
    auto pDeltaExt = Ref(new CDelta_ext());
    {
        auto pDeltaSeq = Ref(new CDelta_seq());
        pDeltaSeq->SetLiteral().SetLength(5);
        pDeltaSeq->SetLiteral().SetSeq_data().SetIupacna().Set("AGGCC");
        pDeltaExt->Set().push_back(pDeltaSeq);
    }
    {
        auto pDeltaSeq = Ref(new CDelta_seq());
        pDeltaSeq->SetLiteral().SetLength(0);
        pDeltaSeq->SetLiteral().SetSeq_data().SetIupacna().Set("");
        pDeltaExt->Set().push_back(pDeltaSeq);
    }
    {
        auto pDeltaSeq = Ref(new CDelta_seq());
        pDeltaSeq->SetLiteral().SetLength(1);
        pDeltaSeq->SetLiteral().SetSeq_data().SetIupacna().Set("C");
        pDeltaExt->Set().push_back(pDeltaSeq);
    }

    auto changes = Ref(new CCleanupChange());
    CNewCleanup_imp cleanup_imp(changes);
    cleanup_imp.RemoveZeroLengthLiterals(*pDeltaExt);

    BOOST_CHECK_EQUAL(pDeltaExt->Get().size(), 2);
    BOOST_CHECK_EQUAL(pDeltaExt->Get().front()->GetLiteral().GetLength(), 5);
    BOOST_CHECK_EQUAL(pDeltaExt->Get().back()->GetLiteral().GetLength(), 1);
    BOOST_CHECK_EQUAL(changes->ChangeCount(), 1); // just a single change
    BOOST_CHECK(changes->IsChanged(CCleanupChange::eCleanDeltaExt));
}

