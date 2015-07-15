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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:  NetCache stress test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/request_ctx.hpp>

#include <connect/services/netcache_api.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_types.h>
#include <connect/ncbi_core_cxx.hpp>

#include <deque>


USING_NCBI_SCOPE;

///////////////////////////////////////////////////////////////////////


/// Test application
///
/// @internal
///
class CTestNetCacheStress
{
public:
    struct STransactionInfo
    {
        string    key;
        size_t    blob_size;
        string    time_stamp;
        bool      deleted;
        STransactionInfo() : blob_size(0), deleted(false) {}
    };

    typedef deque<STransactionInfo> TTransactionLog;

public:
    CTestNetCacheStress(CNetCacheAPI::TInstance api, CNcbiOstream& os, int num)
        : m_API(api), m_Num(num), m_Os(os) {}

    int Run(void);
private:

    void StressTestPutGet(size_t           blob_size,
                          unsigned         bcount,
                          TTransactionLog* tlog);

    void StressTestPut(size_t           blob_size,
                       unsigned         bcount,
                       TTransactionLog* tlog);

    void DeleteRandomBlobs(TTransactionLog* tlog, size_t bcount);

    /// Read BLOB according to the transaction log
    /// (all of them straight or randomly)
    void StressTestGet(const TTransactionLog& tlog,
                       bool                   random,
                       size_t                 count);

    unsigned char* AllocateTestBlob(size_t blob_size) const;

    void CheckBlob(unsigned char* blob, size_t blob_size) const;

    void CheckGetBlob(const STransactionInfo& ti,
                      unsigned char*          buf);
    void AddToAppLog(const TTransactionLog& tlog);

private:
    CNetCacheAPI   m_API;
    int            m_Num;

    TTransactionLog m_Log;
    CNcbiOstream& m_Os;
};


static int  s_Count;
static Int8 s_MaxSize;


void CTestNetCacheStress::StressTestPutGet(size_t           blob_size,
                                           unsigned         bcount,
                                           TTransactionLog* tlog)
{
    StressTestPut(blob_size, bcount, tlog);

    StressTestGet(*tlog, false /*random*/, bcount);
}


void CTestNetCacheStress::CheckGetBlob(const STransactionInfo& ti,
                                       unsigned char*          buf)
{
    try {
        size_t n_read, bsize;

        CNetCacheAPI::EReadResult rres =
            m_API.GetData(ti.key, buf, ti.blob_size, &n_read, &bsize);

        if (rres == CNetCacheAPI::eNotFound && ti.deleted == false) {
            LOG_POST(Error << "BLOB not found: "
                            << ti.key << " "
                            << ti.blob_size << " "
                            << ti.time_stamp);
            return;
        }

        if (ti.deleted && rres != CNetCacheAPI::eNotFound) {
            LOG_POST(Error << "Deleted BLOB found: "
                            << ti.key << " "
                            << bsize << " "
                            << ti.time_stamp);
            return;
        }
        if (rres == CNetCacheAPI::eNotFound) {
            return;
        }

        if (n_read != ti.blob_size) {
            LOG_POST(Error << "BLOB read error: "
                            << ti.key << " "
                            << ti.blob_size << " "
                            << ti.time_stamp << " "
                            << "n_read=" << n_read << " "
                            << "blob_size=" << ti.blob_size << " "
                            << "bsize=" << bsize);
            return;
        }
        if (bsize != ti.blob_size) {
            LOG_POST(Error << "BLOB size error: "
                            << ti.key << " "
                            << ti.blob_size << " "
                            << ti.time_stamp << " "
                            << "n_read=" << n_read << " "
                            << "blob_size=" << ti.blob_size << " "
                            << "bsize=" << bsize);
            return;
        }

        CheckBlob(buf, bsize);
    }
    catch (CNetCacheException& ex)
    {
        if (!ti.deleted) {
            LOG_POST(Error << "BLOB check error (NetCache exception): "
                            << ti.key << " "
                            << ti.blob_size << " "
                            << ti.time_stamp << " "
                            << ex.what());
        }
    }
    catch (exception& ex)
    {
        LOG_POST(Error << "BLOB check error: "
                        << ti.key << " "
                        << ti.blob_size << " "
                        << ti.time_stamp << " "
                        << ex.what());
    }
}


void CTestNetCacheStress::StressTestGet(const TTransactionLog& tlog,
                                        bool                   random,
                                        size_t                 count)
{
    size_t   buf_size = 0;
    unsigned char* buf = NULL;

    if (random) {
        for (size_t i = 0; i < count; ++i) {
            size_t idx = rand() % count;
            const STransactionInfo& ti = tlog[idx];

            if (buf_size < ti.blob_size || buf == NULL) {
                delete [] buf;
                buf = new unsigned char[buf_size = ti.blob_size];
            }

            try {
                CheckGetBlob(ti, buf);
            }
            catch (exception& ex)
            {
                LOG_POST(Error << "BLOB check error: "
                                << ti.key << " "
                                << ti.blob_size << " "
                                << ti.time_stamp << " "
                                << ex.what());
            }
        } // for

    } else {
        for (size_t i = 0; i < count; ++i) {
            const STransactionInfo& ti = tlog[i];

            if (buf_size < ti.blob_size || buf == NULL) {
                delete [] buf;
                buf = new unsigned char[buf_size = ti.blob_size];
            }

            try {
                CheckGetBlob(ti, buf);
            }
            catch (exception& ex)
            {
                LOG_POST(Error << "BLOB check error: "
                                << ti.key << " "
                                << ti.blob_size << " "
                                << ti.time_stamp << " "
                                << ex.what());
            }
        } // for
    }

    delete[] buf;
}


void CTestNetCacheStress::StressTestPut(size_t           blob_size,
                                        unsigned         bcount,
                                        TTransactionLog* tlog)
{
    unsigned char* buf = AllocateTestBlob(blob_size);

    try {
        // Put blobs

        for (unsigned i = 0; i < bcount; ++i) {
            STransactionInfo ti;
            ti.blob_size = blob_size;
            ti.time_stamp = GetFastLocalTime().AsString();
            ti.deleted = false;

            if (rand() % 2) {
                ti.key = m_API.PutData(buf, blob_size);
            } else {
                auto_ptr<IWriter> writer(m_API.PutData(&ti.key));
                auto_ptr<CWStream> os(new CWStream(writer.release(), 0,0,
                                                   CRWStreambuf::fOwnWriter));
                os->write((char*)buf, blob_size);
            }

            if ((rand() & 3) == 0 && !m_API.HasBlob(ti.key))
                LOG_POST(Error << "\nERROR: newly added BLOB disappeared: " <<
                    ti.key << " size " << ti.blob_size <<
                    " timestamp " << ti.time_stamp);

            if (tlog) {
                tlog->push_back(ti);
            }
        } // for
    }
    catch (...) {
        delete[] buf;
        throw;
    }

    delete [] buf;
}


void CTestNetCacheStress::DeleteRandomBlobs(TTransactionLog* tlog,
                                            size_t           bcount)
{
    if (!bcount)
        return;

    for (size_t i = 0; i < bcount; ++i) {
        size_t idx = rand() % bcount;
        STransactionInfo& ti = (*tlog)[idx];

        if (ti.deleted)
            continue;

        try {
            m_API.Remove(ti.key);
            ti.deleted = true;
        }
        catch (exception& ex)
        {
            LOG_POST(Error << "BLOB remove error: "
                            << ti.key << " "
                            << ti.blob_size << " "
                            << ti.time_stamp << " "
                            << ex.what());
        }
    } // for
}


unsigned char* CTestNetCacheStress::AllocateTestBlob(size_t blob_size) const
{
    if (blob_size == 0) {
        throw runtime_error("AllocateTestBlob(): Invalid blob size");
    }

    unsigned char* buf = new unsigned char[blob_size];

    unsigned char init = (unsigned char) (rand() % 10);

    *buf = init;

    for (size_t i = 1; i < blob_size; ++i) {
        unsigned char prev = buf[i-1] + init;
        buf[i] = prev;
    }

    return buf;
}


void CTestNetCacheStress::CheckBlob(unsigned char* blob,
                                    size_t blob_size) const
{
    if (blob_size == 0) {
        throw runtime_error("CheckBlob(): Invalid blob size");
    }

    unsigned char init = *blob;

    for (size_t i = 1; i < blob_size; ++i) {
        unsigned char prev = blob[i-1] + init;
        if (blob[i] != prev) {
            throw runtime_error("Incorrect blob value at position:" +
                                NStr::NumericToString(i));
        }
    } // for
}

void CTestNetCacheStress::AddToAppLog(const TTransactionLog& tlog)
{
    for (size_t i = 0; i < tlog.size(); ++i) {
        m_Log.push_back(tlog[i]);
    }
}


int CTestNetCacheStress::Run(void)
{
    string client_ip;
    client_ip += NStr::IntToString(m_Num);
    client_ip += ".";
    client_ip += NStr::IntToString(m_Num);
    client_ip += ".";
    client_ip += NStr::IntToString(m_Num);
    client_ip += ".";
    client_ip += NStr::IntToString(m_Num);
    CDiagContext::GetRequestContext().SetClientIP(client_ip);

    Int8 total_count = 0;
    for (int count = 0; count < s_Count; ++count) {
        try {
            TTransactionLog tlog;
            int blob_size = int(rand() * s_MaxSize / RAND_MAX + 1);
            total_count += blob_size;
            CDiagContext::GetRequestContext().SetSessionID(NStr::IntToString(count + 1));
            StressTestPutGet(blob_size, 1, &tlog);
            SleepMilliSec(20);
            AddToAppLog(tlog);
            if (count % 100 == 0) {
                m_Os << "Progress: " << count << " out of " << s_Count
                     << " in thread " << CThread::GetSelf() << NcbiEndl;

                DeleteRandomBlobs(&m_Log, m_Log.size()/10);
            }
        }
        catch (CException e) {
            ERR_POST(e);
        }
        catch (exception e) {
            ERR_POST(e.what());
        }
    }

    m_Os << "Total bytes count in thread " << CThread::GetSelf()
         << " = " << total_count << NcbiEndl;


    DeleteRandomBlobs(&m_Log, m_Log.size()/10);

    StressTestGet(m_Log, true /*random*/, m_Log.size());

    return 0;
}

class CTestNetCacheStressThread : public CThread
{
public:
    CTestNetCacheStressThread(CNetCacheAPI::TInstance api,
            CNcbiOstream& os, int num) :
        m_Test(new CTestNetCacheStress(api, os, num))
    {
    }

    virtual void* Main()
    {
        m_Test->Run();
        return NULL;
    }
private:
    auto_ptr<CTestNetCacheStress> m_Test;
};

class CTestNetCacheStressApp : public CNcbiApplication
{
public:

    void Init(void);
    int Run(void);
private:

};


void CTestNetCacheStressApp::Init(void)
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Net cache client");

    arg_desc->AddPositional("service",
        "NetCache service name or host:port",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("count",
                             "count",
                             "Number of blobs to submit",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("size",
                             "size",
                             "Maximum size of blobs to submit",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("threads",
                             "threads",
                             "Number of threads",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("timeout",
                             "timeout",
                             "Communication timeout in msec",
                             CArgDescriptions::eInteger);
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CTestNetCacheStressApp::Run(void)
{
    const CArgs& args = GetArgs();
    string service_name = args["service"].AsString();

    s_Count = 500;
    if (args["count"]) {
        s_Count = args["count"].AsInteger();
    }

    s_MaxSize = 1 * 1024 * 1024;
    if (args["size"]) {
        s_MaxSize = args["size"].AsInteger();
    }

    unsigned threads = 3;
    if (args["threads"]) {
        threads = (unsigned) args["threads"].AsInteger();
    }

    unsigned timeout = 2000;
    if (args["timeout"]) {
        timeout = (unsigned) args["timeout"].AsInteger();
    }


    CNetCacheAPI nc(service_name, "stress_test");

    STimeout to = {timeout/1000, timeout*1000};
    nc.SetCommunicationTimeout(to);

    if (threads < 2) {
       auto_ptr<CTestNetCacheStress> test(new CTestNetCacheStress(nc, cout, 0));
       return test->Run();
    }

    vector<CRef<CThread> > thread_list;
    thread_list.reserve(threads);
    for (unsigned i = 0; i < threads; ++i) {
        CRef<CThread> thread(
               new CTestNetCacheStressThread(nc, cout, i + 1));
        thread_list.push_back(thread);
        thread->Run();
    } // for
    NON_CONST_ITERATE(vector<CRef<CThread> >, it, thread_list) {
        CRef<CThread> thread(*it);
        thread->Join();
    }
    cout << "Test finished successfully" << NcbiEndl;
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetCacheStressApp().AppMain(argc, argv);
}
