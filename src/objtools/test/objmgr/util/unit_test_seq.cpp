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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Test various functions from objects::sequence namespace
*     (objmgr/util/sequence.hpp)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(sequence);

BOOST_AUTO_TEST_CASE(Test_GetProteinName)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();

    // null handle
    {
        try {
            GetProteinName(CBioseq_Handle());
            BOOST_CHECK(("no expected CObjMgrException" && false));
        }
        catch ( CObjMgrException& exc ) {
            LOG_POST("Got expected: " << exc);
            BOOST_CHECK(exc.GetErrCode() == exc.eInvalidHandle);
        }
    }

    // non-protein
    {
        try {
            CBioseq_Handle bh =
                scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("NT_077402"));
            GetProteinName(bh);
            BOOST_CHECK(("no expected CObjmgrUtilException" && false));
        }
        catch ( CObjmgrUtilException& exc ) {
            LOG_POST("Got expected: " << exc);
            BOOST_CHECK(exc.GetErrCode() == exc.eBadSequenceType);
        }
    }

    CBioseq_Handle bh =
        scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("NP_001005484"));
    CRef<CBioseq> seq(SerialClone(*bh.GetCompleteBioseq()));

    // regular protein
    {
        BOOST_CHECK_EQUAL(GetProteinName(bh), "olfactory receptor 4F5");
        scope.ResetDataAndHistory();
    }
    
    // regular protein
    {
        BOOST_CHECK_EQUAL(GetProteinName(scope.AddBioseq(*seq)),
                          "olfactory receptor 4F5");
        scope.ResetDataAndHistory();
    }

    CRef<CSeq_feat> prot_feat =
        seq->GetAnnot().front()->GetData().GetFtable().front();

    // Prot-ref feature without name
    {
        seq->SetAnnot().front()->SetData().SetFtable().clear();
        CRef<CSeq_feat> feat(SerialClone(*prot_feat));
        feat->SetData().SetProt().ResetName();
        seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        try {
            GetProteinName(scope.AddBioseq(*seq));
            BOOST_CHECK(("no expected CObjmgrUtilException" && false));
        }
        catch ( CObjmgrUtilException& exc ) {
            LOG_POST("Got expected: " << exc);
            BOOST_CHECK(exc.GetErrCode() == exc.eBadFeature);
        }
        scope.ResetDataAndHistory();
    }

    // no Prot-ref features
    {
        try {
            seq->SetAnnot().front()->SetData().SetFtable().clear();
            GetProteinName(scope.AddBioseq(*seq));
            BOOST_CHECK(("no expected CObjMgrException" && false));
        }
        catch ( CObjMgrException& exc ) {
            LOG_POST("Got expected: " << exc);
            BOOST_CHECK(exc.GetErrCode() == exc.eFindFailed);
        }
        scope.ResetDataAndHistory();
    }

    // multiple Prot-ref features
    {
        seq->SetAnnot().front()->SetData().SetFtable().clear();
        seq->SetAnnot().front()->SetData().SetFtable().push_back(prot_feat);
        seq->SetAnnot().front()->SetData().SetFtable().push_back(prot_feat);
        try {
            GetProteinName(scope.AddBioseq(*seq));
            BOOST_CHECK(("no expected CObjMgrException" && false));
        }
        catch ( CObjMgrException& exc ) {
            LOG_POST("Got expected: " << exc);
            BOOST_CHECK(exc.GetErrCode() == exc.eFindConflict);
        }
        scope.ResetDataAndHistory();
    }

    CRef<CSeq_id> id = seq->GetId().front();

    // multiple Prot-ref features of differently represented whole size
    {
        seq->SetAnnot().front()->SetData().SetFtable().clear();
        {
            CRef<CSeq_feat> feat(SerialClone(*prot_feat));
            feat->SetLocation().SetWhole(*id);
            seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        }
        {
            CRef<CSeq_feat> feat(SerialClone(*prot_feat));
            feat->SetLocation().SetInt().SetId(*id);
            feat->SetLocation().SetInt().SetFrom(0);
            feat->SetLocation().SetInt().SetTo(seq->GetInst().GetLength()-1);
            seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        }
        try {
            GetProteinName(scope.AddBioseq(*seq));
            BOOST_CHECK(("no expected CObjMgrException" && false));
        }
        catch ( CObjMgrException& exc ) {
            LOG_POST("Got expected: " << exc);
            BOOST_CHECK(exc.GetErrCode() == exc.eFindConflict);
        }
        scope.ResetDataAndHistory();
    }

    // multiple Prot-ref features of differently sizes and conflict
    {
        seq->SetAnnot().front()->SetData().SetFtable().clear();
        {
            CRef<CSeq_feat> feat(SerialClone(*prot_feat));
            feat->SetLocation().SetInt().SetId(*id);
            feat->SetLocation().SetInt().SetFrom(0);
            feat->SetLocation().SetInt().SetTo(seq->GetInst().GetLength()-2);
            seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        }
        {
            CRef<CSeq_feat> feat(SerialClone(*prot_feat));
            feat->SetLocation().SetWhole(*id);
            seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        }
        {
            CRef<CSeq_feat> feat(SerialClone(*prot_feat));
            feat->SetLocation().SetInt().SetId(*id);
            feat->SetLocation().SetInt().SetFrom(0);
            feat->SetLocation().SetInt().SetTo(seq->GetInst().GetLength()-1);
            seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        }
        try {
            GetProteinName(scope.AddBioseq(*seq));
            BOOST_CHECK(("no expected CObjMgrException" && false));
        }
        catch ( CObjMgrException& exc ) {
            LOG_POST("Got expected: " << exc);
            BOOST_CHECK(exc.GetErrCode() == exc.eFindConflict);
        }
        scope.ResetDataAndHistory();
    }

    string name = prot_feat->GetData().GetProt().GetName().front();

    // multiple Prot-ref features of differently sizes and no conflict
    {
        seq->SetAnnot().front()->SetData().SetFtable().clear();
        {
            CRef<CSeq_feat> feat(SerialClone(*prot_feat));
            feat->SetLocation().SetInt().SetId(*id);
            feat->SetLocation().SetInt().SetFrom(1);
            feat->SetLocation().SetInt().SetTo(seq->GetInst().GetLength()-1);
            feat->SetData().SetProt().SetName().front() = name + " v1";
            seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        }
        {
            CRef<CSeq_feat> feat(SerialClone(*prot_feat));
            feat->SetLocation().SetWhole(*id);
            feat->SetData().SetProt().SetName().front() = name + " v2";
            seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        }
        {
            CRef<CSeq_feat> feat(SerialClone(*prot_feat));
            feat->SetLocation().SetInt().SetId(*id);
            feat->SetLocation().SetInt().SetFrom(0);
            feat->SetLocation().SetInt().SetTo(seq->GetInst().GetLength()-2);
            feat->SetData().SetProt().SetName().front() = name + " v3";
            seq->SetAnnot().front()->SetData().SetFtable().push_back(feat);
        }
        BOOST_CHECK_EQUAL(GetProteinName(scope.AddBioseq(*seq)), name+" v2");
        scope.ResetDataAndHistory();
    }
}
