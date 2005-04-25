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

#include <connect/services/netcache_client.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_types.h>
#include <connect/ncbi_core_cxx.hpp>

#include <util/rwstream.hpp>

#include <deque>


USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


/// Test application
///
/// @internal
///
class CTestNetCacheStress : public CNcbiApplication
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
    void Init(void);
    int Run(void);
private:

    void StressTestPutGet(size_t           blob_size, 
                          unsigned         bcount, 
                          TTransactionLog* tlog);

    void StressTestPut(size_t           blob_size, 
                       unsigned         bcount, 
                       TTransactionLog* tlog);

    void DeleteRandomBlobs(TTransactionLog* tlog, unsigned bcount);

    /// Read BLOB according to the transaction log
    /// (all of them straight or randomly)
    void StressTestGet(const TTransactionLog& tlog, 
                       bool                   random,
                       unsigned               count);

    CNetCacheClient*  CreateClient() const;

    unsigned char* AllocateTestBlob(size_t blob_size) const;

    void CheckBlob(unsigned char* blob, size_t blob_size) const;

    void CheckGetBlob(const STransactionInfo& ti, 
                      unsigned char*          buf,
                      CNetCacheClient*        cl);
    void AddToAppLog(const TTransactionLog& tlog);

private:
    string         m_Host;
    string         m_ServiceName;
    unsigned short m_Port;
    int            m_Count;

    TTransactionLog m_Log;
};


void CTestNetCacheStress::StressTestPutGet(size_t           blob_size,
                                           unsigned         bcount,
                                           TTransactionLog* tlog)
{
    NcbiCout << "Put BLOBs size=" << blob_size << " count=" << bcount 
             << NcbiEndl;

    StressTestPut(blob_size, bcount, tlog);

    NcbiCout << "Get BLOBs size=" << blob_size << " count=" << bcount 
             << NcbiEndl << NcbiEndl;

    StressTestGet(*tlog, false /*random*/, bcount);
}


void CTestNetCacheStress::CheckGetBlob(const STransactionInfo& ti, 
                                       unsigned char*          buf,
                                       CNetCacheClient*        cl)
{
    try {
        size_t n_read, bsize;

        CNetCacheClient::EReadResult rres = 
            cl->GetData(ti.key, buf, ti.blob_size, &n_read, &bsize);

        if (rres == CNetCacheClient::eNotFound && ti.deleted == false) {
            LOG_POST(Error << "BLOB not found: " 
                            << ti.key << " " 
                            << ti.blob_size << " " 
                            << ti.time_stamp);
            return;
        }

        if (ti.deleted && rres != CNetCacheClient::eNotFound) {
            LOG_POST(Error << "Deleted BLOB found: " 
                            << ti.key << " " 
                            << bsize << " " 
                            << ti.time_stamp);
            return;
        }
        if (rres == CNetCacheClient::eNotFound) {
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
                                        unsigned               count)
{
    auto_ptr<CNetCacheClient> cl(CreateClient());

    size_t   buf_size = 0;
    unsigned char* buf = 0;

    if (random) {
        for (unsigned i = 0; i < count; ++i) {
            unsigned idx = rand() % count;
            const STransactionInfo& ti = tlog[idx];

            if (buf_size < ti.blob_size || buf == 0) {
                delete [] buf;
                buf = new unsigned char[buf_size = ti.blob_size];
            }

            try {
                CheckGetBlob(ti, buf, cl.get());
            }
            catch (exception& ex)
            {
                LOG_POST(Error << "BLOB check error: " 
                                << ti.key << " " 
                                << ti.blob_size << " " 
                                << ti.time_stamp << " "
                                << ex.what());
            }

            if (i % 50 == 0) {
                NcbiCout << "." << flush;
            }
        } // for

    } else {
        for (unsigned i = 0; i < count; ++i) {
            const STransactionInfo& ti = tlog[i];

            if (buf_size < ti.blob_size || buf == 0) {
                delete [] buf;
                buf = new unsigned char[buf_size = ti.blob_size];
            }

            try {
                CheckGetBlob(ti, buf, cl.get());
            }
            catch (exception& ex)
            {
                LOG_POST(Error << "BLOB check error: " 
                                << ti.key << " " 
                                << ti.blob_size << " " 
                                << ti.time_stamp << " "
                                << ex.what());
            }

            if (i % 50 == 0) {
                NcbiCout << "." << flush;
            }
        } // for

    }

    NcbiCout << NcbiEndl;

    delete buf;
}


void CTestNetCacheStress::StressTestPut(size_t           blob_size, 
                                        unsigned         bcount,
                                        TTransactionLog* tlog)
{
    auto_ptr<CNetCacheClient> cl(CreateClient());

    unsigned char* buf = AllocateTestBlob(blob_size);
    CheckBlob(buf, blob_size); // just in case

    // Put blobs

    for (unsigned i = 0; i < bcount; ++i) {
        STransactionInfo ti;
        ti.blob_size = blob_size;
        ti.time_stamp = CTime(CTime::eCurrent).AsString();
        ti.deleted = false;

        if (rand() % 2) {
            ti.key = cl->PutData(buf, blob_size);
        } else {
            auto_ptr<IWriter> writer;
            writer.reset(cl->PutData(&ti.key));
            auto_ptr<CWStream> os(new CWStream(writer.release(), 0,0, 
                                               CRWStreambuf::fOwnWriter));
            os->write((char*)buf, blob_size);
        }

        if (tlog) {
            tlog->push_back(ti);
        }

        if (i % 50 == 0) {
            NcbiCout << "." << flush;
        }

    } // for

    delete [] buf;

    NcbiCout << NcbiEndl;
}


void CTestNetCacheStress::DeleteRandomBlobs(TTransactionLog* tlog, 
                                            unsigned         bcount)
{
    if (!bcount)
        return;

    auto_ptr<CNetCacheClient> cl(CreateClient());

    for (unsigned i = 0; i < bcount; ++i) {
        unsigned idx = rand() % bcount;
        STransactionInfo& ti = (*tlog)[idx];

        if (ti.deleted)
            continue;

        try {
            cl->Remove(ti.key);
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

        if (i % 50 == 0) {
            NcbiCout << "." << flush;
        }
    } // for
}


CNetCacheClient*  CTestNetCacheStress::CreateClient() const
{
    auto_ptr<CNetCacheClient> 
        cl(new CNetCacheClient(m_Host, m_Port, "stress_test"));
    STimeout to = {120, 0};
    cl->SetCommunicationTimeout(to);
    return cl.release();
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
        throw runtime_error("AllocateTestBlob(): Invalid blob size");
    }

    unsigned char init = *blob;

    for (size_t i = 1; i < blob_size; ++i) {
        unsigned char prev = blob[i-1] + init;
        if (blob[i] != prev) {
            throw runtime_error("Incorrect blob value at position:" + 
                                NStr::IntToString(i));
        }
    } // for

}

void CTestNetCacheStress::AddToAppLog(const TTransactionLog& tlog)
{
    for (size_t i = 0; i < tlog.size(); ++i) {
        m_Log.push_back(tlog[i]);
    }
}


void CTestNetCacheStress::Init(void)
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Net cache client");
    
    arg_desc->AddPositional("host_port", 
        "NetCache host and port (host:port) or service name.", 
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("count", 
                             "count",
                             "Number of blobs to submit",
                             CArgDescriptions::eInteger);
    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
}



int CTestNetCacheStress::Run(void)
{
    CArgs args = GetArgs();
    const string& host  = args["host_port"].AsString();

    string port_num;
    NStr::SplitInTwo(host, ":", m_Host, port_num);

    if (m_Host.empty() || port_num.empty()) {
        NcbiCerr << "Invalid server address" << host << NcbiEndl;
        return 1;
    }

    m_Port = atoi(port_num.c_str());

    if (m_Port == 0) {
        NcbiCerr << "Invalid port number" << m_Port << NcbiEndl;
        return 1;
    }

    m_Count = 0;
    if (args["count"]) {
        m_Count = args["count"].AsInteger();
    }
    if (!m_Count) m_Count = 5000;


    size_t  bsizes[] = {256, 512, 1024, 
                        1024 * 4, 1024 * 10, 1024 * 100, 1024 * 256,
                        1024 * 1024, 1024 * 1024 * 2, 1024 * 1024 * 12};

    unsigned bins = sizeof(bsizes) / sizeof(bsizes[0]);
    int bin_count = (m_Count / bins) * 2;
    for (int count = 0; count < m_Count;) {
        for (unsigned i = 0; i < bins; ++i) {
            TTransactionLog tlog;
            StressTestPutGet(bsizes[i], bin_count, &tlog);
            SleepMilliSec(1000);
            AddToAppLog(tlog);
            count += bin_count;
            if (count >= m_Count) {
                break;
            }
            bin_count -= bin_count / 2;
        }
        bin_count = (count / bins) * 2;
        NcbiCout << "Progress: " << count << " out of " << m_Count << NcbiEndl;
    }


    NcbiCout << "Waiting...." << NcbiEndl;
    SleepMilliSec(10 * 1000);

    NcbiCout << "Random delete...." << NcbiEndl;
    DeleteRandomBlobs(&m_Log, m_Log.size()/10);

    NcbiCout << "Random read test...." << NcbiEndl;

    StressTestGet(m_Log, true /*random*/, m_Log.size());

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetCacheStress().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/04/25 15:10:05  kuznets
 * Implemented stress test
 *
 * Revision 1.1  2005/04/22 13:17:28  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
