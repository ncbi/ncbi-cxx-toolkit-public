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
* Author:
*           Eugene Vasilchenko
*
* File Description:
*           Test of OM load locks
*
* ===========================================================================
*/

#define NCBI_TEST_APPLICATION

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/seqref.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>

#ifdef NCBI_THREADS
# include <thread>
# include <future>
#endif // NCBI_THREADS

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static string Log(CStopWatch& sw)
{
    ostringstream str;
    str << fixed << setprecision(3) << sw.Elapsed() << " T"<<CThread::GetSelf()<<": ";
    return str.str();
}

BOOST_AUTO_TEST_CASE(LoadLock)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader* gbloader = CGBDataLoader::RegisterInObjectManager(*om).GetLoader();
    BOOST_REQUIRE(gbloader);
    CScope scope(*om);
    scope.AddDefaults();
    CBioseq_Handle bh1 = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("2"));
    BOOST_REQUIRE(bh1);
    CDataSource& ds = bh1.x_GetInfo().GetDataSource();

    CStopWatch sw(CStopWatch::eStart);

    {{
        LOG_POST(Log(sw)<<"Starting sequential test");
        // sequential
        int sat = -1;
        int sat_key = 1;
    
        {{
            auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
            LOG_POST(Log(sw)<<"Calling GetLoadedTSE_Lock(1)");
            auto tse_lock = ds.GetLoadedTSE_Lock(blob_id, CTimeout(1));
            LOG_POST(Log(sw)<<"Returned GetLoadedTSE_Lock(1)");
            BOOST_CHECK(!tse_lock);
        }}
    
        {{
            auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
            LOG_POST(Log(sw)<<"Calling GetTSE_LoadLock()");
            auto load_lock = ds.GetTSE_LoadLock(blob_id);
            LOG_POST(Log(sw)<<"Returned GetTSE_LoadLock()");
            BOOST_REQUIRE(load_lock);
            CRef<CSeq_entry> entry(new CSeq_entry);
            entry->SetSet().SetSeq_set();
            load_lock->SetSeq_entry(*entry);
            LOG_POST(Log(sw)<<"Calling SetLoaded()");
            load_lock.SetLoaded();
            LOG_POST(Log(sw)<<"Returned SetLoaded()");
        }}

        {{
            auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
            LOG_POST(Log(sw)<<"Calling GetTSE_LoadLockIfLoaded()");
            auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
            LOG_POST(Log(sw)<<"Returned GetTSE_LoadLockIfLoaded()");
            BOOST_CHECK(load_lock);
        }}
    
        {{
            auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
            LOG_POST(Log(sw)<<"Calling GetLoadedTSE_Lock(1)");
            auto tse_lock = ds.GetLoadedTSE_Lock(blob_id, CTimeout(1));
            LOG_POST(Log(sw)<<"Returned GetLoadedTSE_Lock(1)");
            BOOST_CHECK(tse_lock);
        }}
        LOG_POST(Log(sw)<<"Finished sequential test");
    }}

    sw.Restart();
    {{
        LOG_POST(Log(sw)<<"Starting parallel test");
        // parallel
        int sat = -1;
        int sat_key = 2;
        thread t1 = thread([&]
            {
                auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
                LOG_POST(Log(sw)<<"Waiting before getting TSE load lock");
                SleepMilliSec(1000);
                LOG_POST(Log(sw)<<"Calling GetTSE_LoadLock()");
                auto load_lock = ds.GetTSE_LoadLock(blob_id);
                LOG_POST(Log(sw)<<"Returned GetTSE_LoadLock()");
                BOOST_REQUIRE_MT_SAFE(load_lock);
                SleepMilliSec(1000);
                CRef<CSeq_entry> entry(new CSeq_entry);
                entry->SetSet().SetSeq_set();
                load_lock->SetSeq_entry(*entry);
                LOG_POST(Log(sw)<<"Calling SetLoaded()");
                load_lock.SetLoaded();
                LOG_POST(Log(sw)<<"Returned SetLoaded()");
            });
        thread t2 = thread([&]
            {
                auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
                {{
                    LOG_POST(Log(sw)<<"Calling GetTSE_LoadLockIfLoaded()");
                    auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
                    LOG_POST(Log(sw)<<"Returned GetTSE_LoadLockIfLoaded()");
                    BOOST_CHECK_MT_SAFE(!load_lock);
                }}
                SleepMilliSec(1000);
                {{
                    LOG_POST(Log(sw)<<"Calling GetTSE_LoadLockIfLoaded()");
                    auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
                    LOG_POST(Log(sw)<<"Returned GetTSE_LoadLockIfLoaded()");
                    BOOST_CHECK_MT_SAFE(!load_lock);
                }}
                SleepMilliSec(500);
                {{
                    LOG_POST(Log(sw)<<"Calling GetTSE_LoadLockIfLoaded()");
                    auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
                    LOG_POST(Log(sw)<<"Returned GetTSE_LoadLockIfLoaded()");
                    BOOST_CHECK_MT_SAFE(!load_lock);
                }}
                SleepMilliSec(1000);
                {{
                    LOG_POST(Log(sw)<<"Calling GetTSE_LoadLockIfLoaded()");
                    auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
                    LOG_POST(Log(sw)<<"Returned GetTSE_LoadLockIfLoaded()");
                    BOOST_CHECK_MT_SAFE(load_lock);
                }}
            });
        thread t3 = thread([&]
            {
                auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
                LOG_POST(Log(sw)<<"Calling GetLoadedTSE_Lock(3)");
                auto tse_lock = ds.GetLoadedTSE_Lock(blob_id, CTimeout(3));
                LOG_POST(Log(sw)<<"Returned GetLoadedTSE_Lock(3)");
                BOOST_CHECK_MT_SAFE(tse_lock);
            });
        thread t4 = thread([&]
            {
                auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
                LOG_POST(Log(sw)<<"Waiting before calling GetLoadedTSE_Lock(2)");
                SleepMilliSec(1500);
                LOG_POST(Log(sw)<<"Calling GetLoadedTSE_Lock(2)");
                auto tse_lock = ds.GetLoadedTSE_Lock(blob_id, CTimeout(2));
                LOG_POST(Log(sw)<<"Returned GetLoadedTSE_Lock(2)");
                BOOST_CHECK_MT_SAFE(tse_lock);
            });
        t1.join();
        t2.join();
        t3.join();
        t4.join();
        LOG_POST(Log(sw)<<"Finished parallel test");
    }}
}
