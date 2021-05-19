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
*   Unit tests for WGS data loader
*
* ===========================================================================
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <corelib/ncbi_system.hpp>
#include <objtools/readers/idmapper.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <util/random_gen.hpp>
#include <thread>
#include "../../bam/test/vdb_user_agent.hpp"

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

enum EMasterDescrType
{
    eWithoutMasterDescr,
    eWithMasterDescr
};

static EMasterDescrType s_master_descr_type = eWithoutMasterDescr;

void sx_InitGBLoader(CObjectManager& om)
{
    CGBDataLoader* gbloader = dynamic_cast<CGBDataLoader*>
        (CGBDataLoader::RegisterInObjectManager(om, "id1", om.eNonDefault).GetLoader());
    _ASSERT(gbloader);
    gbloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
}

CRef<CObjectManager> sx_GetEmptyOM(void)
{
    SetDiagPostLevel(eDiag_Info);
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames names;
    om->GetRegisteredNames(names);
    ITERATE ( CObjectManager::TRegisteredNames, it, names ) {
        om->RevokeDataLoader(*it);
    }
    return om;
}

CRef<CObjectManager> sx_InitOM(EMasterDescrType master_descr_type)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    s_master_descr_type = master_descr_type;
    CWGSDataLoader* wgsloader = dynamic_cast<CWGSDataLoader*>
        (CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault).GetLoader());
    wgsloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
    if ( master_descr_type == eWithMasterDescr ) {
        sx_InitGBLoader(*om);
    }
    return om;
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr)
{
    LOG_POST("Checking WGS master sequence descriptors");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRandom r(1);
    const size_t NQ = 12;
    const size_t NS = 20;
    const char* accs[] = {
        "BART01",
        "BARU01",
        "BARG01",
        "BASA01",
        "BARW01",
        "BASC01",
        "BARV01",
        "BASE01",
        "BASF01",
        "BASG01",
        "BASJ01",
        "BASL01",
        "BASN01",
        "BASR01",
        "BASS01",
    };
    vector<vector<string>> ids(NQ);
    for ( size_t k = 0; k < NQ; ++k ) {
        for ( size_t i = 0; i < NS; ++i ) {
            char id[99];
            sprintf(id, "%s%06u.1", accs[r.GetRandIndexSize_t(ArraySize(accs))], r.GetRand(1, 100));
            ids[k].push_back(id);
        }
    }
    vector<thread> tt(NQ);
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i] =
            thread([&](const vector<string>& ids)
                   {
                       CScope scope(*CObjectManager::GetInstance());
                       scope.AddDefaults();
                       for ( auto& id : ids ) {
                           try {
                               CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
                               BOOST_REQUIRE_MT_SAFE(bh);
                               int desc_mask = 0;
                               map<string, int> user_count;
                               int comment_count = 0;
                               int pub_count = 0;
                               for ( CSeqdesc_CI it(bh); it; ++it ) {
                                   desc_mask |= 1<<it->Which();
                                   switch ( it->Which() ) {
                                   case CSeqdesc::e_Comment:
                                       ++comment_count;
                                       break;
                                   case CSeqdesc::e_Pub:
                                       ++pub_count;
                                       break;
                                   case CSeqdesc::e_User:
                                       ++user_count[it->GetUser().GetType().GetStr()];
                                       break;
                                   default:
                                       break;
                                   }
                               }
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Title));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Source));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Molinfo));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Pub));
                               BOOST_CHECK_MT_SAFE(pub_count == 2 || pub_count == 3);
                               //BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Genbank));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Create_date));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Update_date));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_User));
                               BOOST_CHECK_EQUAL_MT_SAFE(user_count.size(), 2u);
                               BOOST_CHECK_EQUAL_MT_SAFE(user_count["StructuredComment"], 1);
                               BOOST_CHECK_EQUAL_MT_SAFE(user_count["DBLink"], 1);
                           }
                           catch (...) {
                               ERR_POST("Failed id: "<<id);
                               throw;
                           }
                       }
                   }, ids[i]);
    }
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i].join();
    }
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescrProt)
{
    LOG_POST("Checking WGS master sequence descriptors on proteins");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRandom r(1);
    const size_t NQ = 12;
    const size_t NS = 2000;
    const char* accs[] = {
        "BART01",
        "BARU01",
        "BARG01",
        "BASA01",
        "BARW01",
        "BASC01",
        "BARV01",
        "BASE01",
        "BASF01",
        "BASG01",
        "BASJ01",
        "BASL01",
        "BASN01",
        "BASR01",
        "BASS01",
    };
    vector<vector<string>> ids(NQ);
    {{
        CVDBMgr mgr;
        for ( size_t k = 0; k < NQ; ++k ) {
            CWGSDb wgs(mgr, accs[k]);
            for ( CWGSProteinIterator it(wgs); it && ids[k].size() < NS; ++it ) {
                ids[k].push_back(it.GetAccession());
            }
            LOG_POST("WGS "<<accs[k]<<" testing "<<ids[k].size()<<" proteins");
        }
    }}
    vector<thread> tt(NQ);
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i] =
            thread([&](const vector<string>& ids)
                   {
                       CScope scope(*CObjectManager::GetInstance());
                       scope.AddDefaults();
                       for ( auto& id : ids ) {
                           try {
                               CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
                               BOOST_REQUIRE_MT_SAFE(bh);
                               int desc_mask = 0;
                               map<string, int> user_count;
                               int comment_count = 0;
                               int pub_count = 0;
                               for ( CSeqdesc_CI it(bh); it; ++it ) {
                                   desc_mask |= 1<<it->Which();
                                   switch ( it->Which() ) {
                                   case CSeqdesc::e_Comment:
                                       ++comment_count;
                                       break;
                                   case CSeqdesc::e_Pub:
                                       ++pub_count;
                                       break;
                                   case CSeqdesc::e_User:
                                       ++user_count[it->GetUser().GetType().GetStr()];
                                       break;
                                   default:
                                       break;
                                   }
                               }
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Title));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Source));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Molinfo));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Pub));
                               BOOST_CHECK_MT_SAFE(pub_count == 2 || pub_count == 3);
                               //BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Genbank));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Create_date));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Update_date));
                               BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_User));
                               BOOST_CHECK_EQUAL_MT_SAFE(user_count.size(), 2u);
                               BOOST_CHECK_EQUAL_MT_SAFE(user_count["StructuredComment"], 1);
                               BOOST_CHECK_EQUAL_MT_SAFE(user_count["DBLink"], 1);
                           }
                           catch (...) {
                               ERR_POST("Failed id: "<<id);
                               throw;
                           }
                       }
                   }, ids[i]);
    }
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i].join();
    }
}


BEGIN_LOCAL_NAMESPACE;

static string get_cip(size_t index)
{
    return "1.2.3."+to_string(100+index);
}

static string get_sid(size_t index)
{
    return "session_"+to_string(index);
}

static string get_hid(size_t index)
{
    return "hit_"+to_string(index);
}

END_LOCAL_NAMESPACE;


BOOST_AUTO_TEST_CASE(CheckWGSUserAgent)
{
    LOG_POST("Checking WGS retrieval User-Agent");
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRandom r(1);
    const char* accs[] = {
        "BART01",
        "BARU01",
        "BARG01",
        "BASA01",
        "BARW01",
        "BASC01",
        "BARV01",
        "BASE01",
        "BASF01",
        "BASG01",
        "BASJ01",
        "BASL01",
        "BASN01",
        "BASR01",
        "BASS01",
    };
    {{
        // check if WGS is resolved to remote files
        CVDBMgr mgr;
        if ( !NStr::StartsWith(mgr.FindAccPath(accs[0]), "http") ) {
            LOG_POST("Skipping User-Agent test because VDB files are local");
            return;
        }
    }}

    
    const size_t NQ = sizeof(accs)/sizeof(accs[0]);
    const size_t NS = 20;
    vector<vector<string>> ids(NQ);
    map<string, size_t> id2index;
    for ( size_t k = 0; k < NQ; ++k ) {
        for ( size_t i = 0; i < NS; ++i ) {
            char id[99];
            size_t index = r.GetRandIndexSize_t(ArraySize(accs));
            sprintf(id, "%s%06d.1", accs[index], int(r.GetRand(1, 100)));
            ids[k].push_back(id);
            id2index[id] = index;
        }
    }
    CVDBUserAgentMonitor::Initialize();
    for ( size_t i = 0; i < NQ; ++i ) {
        CVDBUserAgentMonitor::SUserAgentValues values = {
            get_cip(i),
            get_sid(i),
            get_hid(i)
        };
        CVDBUserAgentMonitor::SetExpectedUserAgentValues(accs[i], values);
        for ( size_t j = 1; j < 100; ++j ) {
            string suffix = "."+to_string(j);
            CVDBUserAgentMonitor::SetExpectedUserAgentValues(accs[i]+suffix, values);
        }
    }
    vector<thread> tt(NQ);
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i] =
            thread([&](size_t index, const vector<string>& ids)
                {
                    try {
                        CScope scope(*CObjectManager::GetInstance());
                        scope.AddDefaults();
                        size_t last_index = size_t(-1);
                        for ( auto& id : ids ) {
                            try {
                                auto index_iter = id2index.find(id);
                                BOOST_REQUIRE_MT_SAFE(index_iter != id2index.end());
                                size_t index = index_iter->second;
                                if ( index != last_index ) {
                                    _TRACE("index="<<index<<" wgs="<<accs[index]);
                                }
                                {
                                    auto& ctx = CDiagContext::GetRequestContext();
                                    ctx.SetClientIP(get_cip(index));
                                    ctx.SetSessionID(get_sid(index));
                                    ctx.SetHitID(get_hid(index));
                                }
                                
                                CBioseq_Handle bh =
                                    scope.GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
                                BOOST_REQUIRE_MT_SAFE(bh);
                                int desc_mask = 0;
                                map<string, int> user_count;
                                int comment_count = 0;
                                int pub_count = 0;
                                for ( CSeqdesc_CI it(bh); it; ++it ) {
                                    desc_mask |= 1<<it->Which();
                                    switch ( it->Which() ) {
                                    case CSeqdesc::e_Comment:
                                        ++comment_count;
                                        break;
                                    case CSeqdesc::e_Pub:
                                        ++pub_count;
                                        break;
                                    case CSeqdesc::e_User:
                                        ++user_count[it->GetUser().GetType().GetStr()];
                                        break;
                                    default:
                                        break;
                                    }
                                }
                                BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Title));
                                BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Source));
                                BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Molinfo));
                                BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Pub));
                                BOOST_CHECK_MT_SAFE(pub_count == 2 || pub_count == 3);
                                //BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Genbank));
                                BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Create_date));
                                BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_Update_date));
                                BOOST_CHECK_MT_SAFE(desc_mask & (1<<CSeqdesc::e_User));
                                BOOST_CHECK_EQUAL_MT_SAFE(user_count.size(), 2u);
                                BOOST_CHECK_EQUAL_MT_SAFE(user_count["StructuredComment"], 1);
                                BOOST_CHECK_EQUAL_MT_SAFE(user_count["DBLink"], 1);
                            }
                            catch (...) {
                                ERR_POST("Failed id: "<<id);
                                throw;
                            }
                        }
                    }
                    catch ( CException& exc ) {
                        ERR_POST("MT4["<<i<<"]: "<<exc);
                        BOOST_CHECK_EQUAL_MT_SAFE(exc.what(), "---");
                    }
                }, i, ids[i]);
    }
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i].join();
    }
    CVDBUserAgentMonitor::ReportErrors();
    BOOST_CHECK_MT_SAFE(!CVDBUserAgentMonitor::GetErrorFlags());
}


NCBITEST_INIT_TREE()
{
    NCBITEST_DISABLE(CheckWGSUserAgent);
}
