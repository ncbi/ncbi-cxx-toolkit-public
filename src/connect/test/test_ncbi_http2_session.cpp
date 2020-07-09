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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/request_status.hpp>
#include <connect/ncbi_http2_session.hpp>

#include <atomic>
#include <mutex>
#include <sstream>
#include <thread>

#include "test_assert.h"  // This header must go last


USING_NCBI_SCOPE;


class CNCBITestHttp2SessionApp : public CNcbiApplication
{
    int  Run (void);
};


struct SOut
{
    void operator<<(istream& is)
    {
        if (is.good()) {
            // Have to read to the buf first,
            // as there might be some output (e.g. TRACE) while reading is
            stringstream ss;
            ss << is.rdbuf();

            unique_lock<mutex> lock(m_Mutex);
            cout << ss.rdbuf() << '\n';
        }
    }

private:
    mutex m_Mutex;
};

int CNCBITestHttp2SessionApp::Run(void)
{
    const size_t kThreadNum = 4;
    const string kGoodUrl("http://psg21.be-md.ncbi.nlm.nih.gov:10000/ID/resolve?seq_id=3150015&all_info=yes&use_cache=no");
    const string kBadUrl("http://psg21.be-md.ncbi.nlm.nih.gov:10000/ID/resolve?seq_id=abc&all_info=yes");
    atomic_size_t wait(kThreadNum);
    SOut out;

    auto f = [&]() {
        size_t n = kThreadNum;

        do {
            this_thread::yield();
        }
        while (!wait.compare_exchange_weak(n, n - 1));

        while (wait) {
            this_thread::yield();
        }

        CHttp2Session session;

        if (n % 2) {
            CHttpRequest request = session.NewRequest(kGoodUrl);
            CHttpResponse response = request.Execute();
            _ASSERT(response.GetStatusCode() == CRequestStatus::e200_Ok);
            out << response.ContentStream();
        } else {
            CHttpRequest request = session.NewRequest(kBadUrl);
            CHttpResponse response = request.Execute();
            _ASSERT(response.GetStatusCode() == CRequestStatus::e404_NotFound);
            out << response.ErrorStream();
        }
    };

    vector<thread> threads;

    for (auto i = kThreadNum; i > 0; --i) {
        threads.emplace_back(f);
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CNCBITestHttp2SessionApp().AppMain(argc, argv);
}
