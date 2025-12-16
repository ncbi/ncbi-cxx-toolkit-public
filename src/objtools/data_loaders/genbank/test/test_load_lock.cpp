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
# include <condition_variable>
#endif // NCBI_THREADS

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static CStopWatch run_sw;

static void reset_Log()
{
    run_sw.Restart();
}
static string Log()
{
    ostringstream str;
    str << fixed << setprecision(3) << run_sw.Elapsed() << " T"<<CThread::GetSelf()<<": ";
    return str.str();
}

static void SimulateDelay(unsigned ms)
{
    // sleep for 'ms' milliseconds, probably more by factor of up to 3
    double factor = 1+rand()%2000*1e-3;
    ms = unsigned(ms*factor);
    LOG_POST(Log()<<"Sleeping for "<<ms<<" milliseconds");
    SleepMilliSec(ms);
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

    reset_Log();

    {{
        LOG_POST(Log()<<"Starting sequential test");
        // sequential
        int sat = -1;
        int sat_key = 1;
    
        {{
            auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
            LOG_POST(Log()<<"Calling GetLoadedTSE_Lock(1)");
            auto tse_lock = ds.GetLoadedTSE_Lock(blob_id, CTimeout(1));
            LOG_POST(Log()<<"Returned GetLoadedTSE_Lock(1)");
            BOOST_CHECK(!tse_lock);
        }}
    
        {{
            auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
            LOG_POST(Log()<<"Calling GetTSE_LoadLock()");
            auto load_lock = ds.GetTSE_LoadLock(blob_id);
            LOG_POST(Log()<<"Returned GetTSE_LoadLock()");
            BOOST_REQUIRE(load_lock);
            CRef<CSeq_entry> entry(new CSeq_entry);
            entry->SetSet().SetSeq_set();
            load_lock->SetSeq_entry(*entry);
            LOG_POST(Log()<<"Calling SetLoaded()");
            load_lock.SetLoaded();
            LOG_POST(Log()<<"Returned SetLoaded()");
        }}

        {{
            auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
            LOG_POST(Log()<<"Calling GetTSE_LoadLockIfLoaded()");
            auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
            LOG_POST(Log()<<"Returned GetTSE_LoadLockIfLoaded()");
            BOOST_CHECK(load_lock);
        }}
    
        {{
            auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
            LOG_POST(Log()<<"Calling GetLoadedTSE_Lock(1)");
            auto tse_lock = ds.GetLoadedTSE_Lock(blob_id, CTimeout(1));
            LOG_POST(Log()<<"Returned GetLoadedTSE_Lock(1)");
            BOOST_CHECK(tse_lock);
        }}
        LOG_POST(Log()<<"Finished sequential test");
    }}

    reset_Log();
    {{
        LOG_POST(Log()<<"Starting parallel test");
        // parallel
        atomic<bool> all_done_before_SetLoaded{false};
        mutex mutex_all_done_before_SetLoaded;
        condition_variable cv_all_done_before_SetLoaded;
        
        enum SetLoadedState {
            before_Setloaded,
            during_SetLoaded,
            after_SetLoaded
        };
        atomic<SetLoadedState> SetLoaded_state{before_Setloaded};
        mutex mutex_SetLoaded_state;
        condition_variable cv_SetLoaded_state;

        function<void(SetLoadedState)> set_SetLoaded_state = [&](SetLoadedState s){
            {{
                unique_lock lock(mutex_SetLoaded_state);
                SetLoaded_state = s;
            }}
            cv_SetLoaded_state.notify_all();
        };
        function<void(SetLoadedState)> wait_for_SetLoaded_state = [&](SetLoadedState s){
            if ( SetLoaded_state >= s ) {
                return;
            }
            LOG_POST(Log()<<"Waiting for SetLoaded_state");
            unique_lock lock(mutex_SetLoaded_state);
            cv_SetLoaded_state.wait(lock, [&](){return SetLoaded_state >= s;});
        };
        
        int sat = -1;
        int sat_key = 2;
        thread t1 = thread([&]
            {
                auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
                LOG_POST(Log()<<"Waiting before getting TSE load lock");
                SimulateDelay(1000);
                LOG_POST(Log()<<"Calling GetTSE_LoadLock()");
                auto load_lock = ds.GetTSE_LoadLock(blob_id);
                LOG_POST(Log()<<"Returned GetTSE_LoadLock()");
                BOOST_REQUIRE_MT_SAFE(load_lock);
                SimulateDelay(1000);
                LOG_POST(Log()<<"Creating Seq-entry");
                CRef<CSeq_entry> entry(new CSeq_entry);
                entry->SetSet().SetSeq_set();
                load_lock->SetSeq_entry(*entry);
                if ( !all_done_before_SetLoaded ) {
                    LOG_POST(Log()<<"Waiting for all_done_before_SetLoaded");
                    unique_lock lock(mutex_all_done_before_SetLoaded);
                    cv_all_done_before_SetLoaded.wait(lock, [&]()->bool{
                        return all_done_before_SetLoaded;
                    });
                }
                LOG_POST(Log()<<"Calling SetLoaded()");
                set_SetLoaded_state(during_SetLoaded);
                load_lock.SetLoaded();
                set_SetLoaded_state(after_SetLoaded);
                LOG_POST(Log()<<"Returned SetLoaded()");
            });
        thread t2 = thread([&]
            {
                auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
                {{
                    LOG_POST(Log()<<"Calling GetTSE_LoadLockIfLoaded()");
                    auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
                    LOG_POST(Log()<<"Returned GetTSE_LoadLockIfLoaded()");
                    BOOST_CHECK_MT_SAFE(!load_lock);
                }}
                SimulateDelay(1000);
                {{
                    LOG_POST(Log()<<"Calling GetTSE_LoadLockIfLoaded()");
                    auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
                    LOG_POST(Log()<<"Returned GetTSE_LoadLockIfLoaded()");
                    BOOST_CHECK_MT_SAFE(!load_lock);
                }}
                SimulateDelay(500);
                {{
                    LOG_POST(Log()<<"Calling GetTSE_LoadLockIfLoaded()");
                    auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
                    LOG_POST(Log()<<"Returned GetTSE_LoadLockIfLoaded()");
                    BOOST_CHECK_MT_SAFE(!load_lock);
                }}

                {{
                    unique_lock lock(mutex_all_done_before_SetLoaded);
                    all_done_before_SetLoaded = true;
                }}
                cv_all_done_before_SetLoaded.notify_all();
                
                SimulateDelay(1000);
                wait_for_SetLoaded_state(after_SetLoaded);
                {{
                    LOG_POST(Log()<<"Calling GetTSE_LoadLockIfLoaded()");
                    auto load_lock = ds.GetTSE_LoadLockIfLoaded(blob_id);
                    LOG_POST(Log()<<"Returned GetTSE_LoadLockIfLoaded()");
                    BOOST_CHECK_MT_SAFE(load_lock);
                }}
            });
        thread t3 = thread([&]
            {
                auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
                double timeout = 10;
                LOG_POST(Log()<<"Calling GetLoadedTSE_Lock("<<timeout<<")");
                auto tse_lock = ds.GetLoadedTSE_Lock(blob_id, CTimeout(timeout));
                LOG_POST(Log()<<"Returned GetLoadedTSE_Lock("<<timeout<<")");
                BOOST_CHECK_MT_SAFE(tse_lock);
            });
        thread t4 = thread([&]
            {
                auto blob_id = gbloader->GetBlobIdFromSatSatKey(sat, sat_key);
                double timeout = 8;
                LOG_POST(Log()<<"Waiting before calling GetLoadedTSE_Lock("<<timeout<<")");
                SimulateDelay(1500);
                LOG_POST(Log()<<"Calling GetLoadedTSE_Lock("<<timeout<<")");
                auto tse_lock = ds.GetLoadedTSE_Lock(blob_id, CTimeout(timeout));
                LOG_POST(Log()<<"Returned GetLoadedTSE_Lock("<<timeout<<")");
                BOOST_CHECK_MT_SAFE(tse_lock);
            });
        t1.join();
        t2.join();
        t3.join();
        t4.join();
        LOG_POST(Log()<<"Finished parallel test");
    }}
}
