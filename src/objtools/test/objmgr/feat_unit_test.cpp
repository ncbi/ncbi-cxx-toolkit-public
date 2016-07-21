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
* Authors:  Eugene Vasilchenko
*
* File Description:
*   Unit test for feature loading and processing
*/

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <corelib/test_boost.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <util/random_gen.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static CRef<CScope> s_InitScope(void)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CDataLoader* loader =
        om->FindDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
    if ( loader ) {
        BOOST_CHECK(om->RevokeDataLoader(*loader));
    }
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    return scope;
}

struct SFeatInfo
{
    CSeqFeatData::ESubtype m_Subtype;
    int m_Id;
    int m_Xref;
    CRef<CSeq_feat> m_Feat;
};

int s_GetId(const CFeat_id& id)
{
    if ( id.IsLocal() && id.GetLocal().IsId() ) {
        return id.GetLocal().GetId();
    }
    return 0;
}

int s_GetId(const CSeqFeatXref& xref)
{
    if ( xref.IsSetId() ) {
        return s_GetId(xref.GetId());
    }
    return 0;
}

int s_GetId(const CSeq_feat::TIds & ids)
{
    ITERATE ( CSeq_feat::TIds, it, ids ) {
        int id = s_GetId(**it);
        if ( id ) {
            return id;
        }
    }
    return 0;
}

int s_GetId(const CSeq_feat::TXref & ids)
{
    ITERATE ( CSeq_feat::TXref, it, ids ) {
        int id = s_GetId(**it);
        if ( id ) {
            return id;
        }
    }
    return 0;
}

BOOST_AUTO_TEST_CASE(CheckSplit)
{
    CSeq_id_Handle id = CSeq_id_Handle::GetHandle(GI_CONST(27501067));
    vector<SFeatInfo> feats;
    {{
        CRef<CScope> scope = s_InitScope();
        CBioseq_Handle bh = scope->GetBioseqHandle(id);
        BOOST_REQUIRE(bh);
        for ( CFeat_CI it(bh.GetTSE_Handle()); it; ++it ) {
            SFeatInfo info;
            CRef<CSeq_feat> feat(SerialClone(it->GetOriginalFeature()));
            info.m_Subtype = it->GetFeatSubtype();
            info.m_Feat = feat;
            info.m_Id = 0;
            info.m_Xref = 0;
            if ( !info.m_Id && feat->IsSetId() ) {
                info.m_Id = s_GetId(feat->GetId());
            }
            if ( !info.m_Id && feat->IsSetIds() ) {
                info.m_Id = s_GetId(feat->GetIds());
            }
            if ( !info.m_Xref && feat->IsSetXref() ) {
                info.m_Xref = s_GetId(feat->GetXref());
            }
            if ( info.m_Id || info.m_Xref ) {
                feats.push_back(info);
            }
        }
    }}
    BOOST_REQUIRE(!feats.empty());

    {{
        CRandom random;
        CRef<CScope> scope = s_InitScope();
        CBioseq_Handle bh = scope->GetBioseqHandle(id);
        CTSE_Handle tse = bh.GetTSE_Handle();
        BOOST_REQUIRE(bh);
        ITERATE ( vector<SFeatInfo>, it, feats ) {
            if ( it->m_Id && random.GetRand(0, 1) ) {
                //ERR_POST("Get by id");
                vector<CSeq_feat_Handle> found;
                if ( random.GetRand(0, 1) ) {
                    CSeqFeatData::E_Choice type =
                        CSeqFeatData::GetTypeFromSubtype(it->m_Subtype);
                    found = tse.GetFeaturesWithId(type, it->m_Id);
                }
                else {
                    found = tse.GetFeaturesWithId(it->m_Subtype, it->m_Id);
                }
                BOOST_CHECK(!found.empty());
            }
            if ( it->m_Xref && random.GetRand(0, 1) ) {
                //ERR_POST("Get by xref");
                vector<CSeq_feat_Handle> found;
                if ( random.GetRand(0, 1) ) {
                    CSeqFeatData::E_Choice type =
                        CSeqFeatData::GetTypeFromSubtype(it->m_Subtype);
                    found = tse.GetFeaturesWithXref(type, it->m_Xref);
                }
                else {
                    found = tse.GetFeaturesWithXref(it->m_Subtype, it->m_Xref);
                }
                BOOST_CHECK(!found.empty());
            }
        }
    }}
}
