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
 * Author:  Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimtx.hpp>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <atomic>
#include <future>
#include <numeric>
#include <random>
#include <thread>
#if defined(NCBI_OS_DARWIN)
#include <signal.h>
#endif

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


BOOST_AUTO_TEST_SUITE(Semaphore)

BOOST_AUTO_TEST_CASE(SemaphorePostRace)
{
    random_device rd;
    default_random_engine e(rd());
    uniform_int_distribution<> d(3, 10);

    for (auto i = 0; i < 1000; ++i) {
        CSemaphore sem(0, 10);
        auto n = d(e);
        atomic<int> latch(n);
        auto post = [&]() {
            xncbi_SetValidateAction(eValidate_Throw);

            // Wait for other threads
            for (latch--; latch; this_thread::yield());

            try {
                sem.Post(4);
                return 1;
            }
            catch (...) {
                return 0;
            }
        };

        vector<future<int>> fs;
        vector<thread> ts;

        for (auto j = n; j > 0; --j) {
            packaged_task<int()> t(post);
            fs.emplace_back(t.get_future());
            ts.emplace_back(std::move(t));
        }

        auto r = accumulate(fs.begin(), fs.end(), 0, [](int i, future<int>& f) { return i + f.get(); });

        for (auto& t : ts) {
            t.join();
        }

        BOOST_REQUIRE_MESSAGE(r <= 2, "CSemaphore::Post() race occured at iteration " << i << " for " << n << " threads");
    }
}

BOOST_AUTO_TEST_CASE(MutexRace)
{
    const size_t kObjectCount = 10;
    const size_t kThreadCount = 50;
    const size_t kPassCount = 100000;

    struct SObject {
        CMutex mutex;
        size_t counter;
    };
#if defined(NCBI_OS_DARWIN)
    signal(SIGXCPU, SIG_IGN);
#endif
    vector<unique_ptr<SObject>> objs;
    for ( size_t i = 0; i < kObjectCount; ++i ) {
        objs.push_back(make_unique<SObject>());
    }
    vector<thread> tt;
    for ( size_t i = 0; i < kThreadCount; ++i ) {
        tt.push_back(thread([&]() {
            for ( size_t p = 0; p < kPassCount; ++p ) {
                for ( size_t i = 0; i < kObjectCount; ++i ) {
                    CMutexGuard guard(objs[i]->mutex);
                    objs[i]->counter += 1;
                    guard.Release();
                }
            }
        }));
    }
    for ( auto& t : tt ) {
        t.join();
    }
    for ( size_t i = 0; i < kObjectCount; ++i ) {
        BOOST_CHECK_EQUAL(objs[i]->counter, kThreadCount*kPassCount);
    }
}

BOOST_AUTO_TEST_SUITE_END()
