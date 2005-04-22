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
 * File Description:  NetCache client test
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
#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


/// Test application
///
/// @internal
///
class CTestNetCacheClient : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


/// @internal

struct STransactionInfo
{
    string    key;
    size_t    blob_size;
    double    transaction_time;
    double    connection_time;
};



/// @internal
static 
bool s_CheckExists(const string&  /* host */, 
                   unsigned short /* port */, 
                   const string&  key,
                   unsigned char* buf = 0,
                   size_t         buf_size = 0,
                   vector<STransactionInfo>* log = 0)
{
//    CSocket sock(host, port);
//    CNetCacheClient nc_client(&sock);

    STransactionInfo info;
    info.blob_size = buf_size;
    info.connection_time = 0;

    CNetCacheClient nc_client("test");
    CStopWatch sw(true);

    unsigned char dataBuf[1024] = {0,};

    if (buf == 0 || buf_size == 0) {
        buf = dataBuf;
        buf_size = sizeof(dataBuf);
    }

    try {
        CNetCacheClient::EReadResult rres = 
            nc_client.GetData(key, buf, buf_size);

        if (rres == CNetCacheClient::eNotFound)
            return false;

        info.transaction_time = sw.Elapsed();
        if (log) {
            log->push_back(info);
        }
    } 
    catch (CNetCacheException& ex)
    {
        cout << ex.what() << endl;
        throw;
        return false;
    }

    return true;
}

/// @internal
static
string s_PutBlob(const string&           host,
               unsigned short            port,
               const void*               buf, 
               size_t                    size, 
               vector<STransactionInfo>* log)
{
    STransactionInfo info;
    info.blob_size = size;

    CStopWatch sw(true);
    CNetCacheClient nc_client(host, port, "test");
    STimeout to = {120, 0};
    nc_client.SetCommunicationTimeout(to);

    info.connection_time = sw.Elapsed();
    sw.Restart();
    info.key = nc_client.PutData(buf, size, 60 * 8);
    info.transaction_time = sw.Elapsed();

    log->push_back(info);
    return info.key;
}

/// @internal
static
void s_ReportStatistics(const vector<STransactionInfo>& log)
{
    NcbiCout << "Statistical records=" << log.size() << NcbiEndl;
    double sum, sum_conn, sum_tran;
    sum = sum_conn = sum_tran = 0.0;
    
    double max_time = 0.0;

    ITERATE(vector<STransactionInfo>, it, log) {
        sum_conn += it->connection_time;
        sum_tran += it->transaction_time;
        double t = it->connection_time + it->transaction_time;
        sum += t;
        if (t > max_time) {
            max_time = t;
        }
    }
    double avg, avg_conn, avg_tran;
    avg = sum / double(log.size());
    if (avg > 0.0000001) {
        avg_conn = sum_conn / double(log.size());
    } else {
        avg_conn = 0;
    }
    avg_tran = sum_tran / double(log.size());


    double slow_median = avg + (avg / 2.0);
        //avg + ((max_time - avg) / 4.0);
    unsigned slow_cnt = 0;
    ITERATE(vector<STransactionInfo>, it, log) {
        double t = it->connection_time + it->transaction_time;
        if (t >= slow_median) {
            ++slow_cnt;
        }
    }


    NcbiCout << "Sum, Conn, Trans" << endl;
    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << sum << ", " << sum_conn << ", " << sum_tran << NcbiEndl;
    NcbiCout << "Avg, Conn, Trans" << endl;
    NcbiCout << avg << ", " << avg_conn << ", " << avg_tran << NcbiEndl;

    NcbiCout << "Max(slowest) turnaround time:" << max_time 
             << "  " << slow_cnt << " transactions in [" 
             << max_time << " .. " << slow_median << "]" 
             << " out of " << log.size()
             << NcbiEndl;
}



/// @internal
static
void s_StressTest(const string&             host,
                  unsigned short            port,
                  size_t                    size, 
                  unsigned int              repeats,
                  vector<STransactionInfo>* log_write,
                  vector<STransactionInfo>* log_read,

                  vector<string>*           rep_keys,  // key representatives
                  unsigned                  key_factor // repr. choose factor
                  )
{
    cout << "Stress test. BLOB size = " << size 
         << " Repeats = " << repeats
         << ". Please wait... " << endl;

    AutoPtr<char, ArrayDeleter<char> > buf  = new char[size];
    AutoPtr<char, ArrayDeleter<char> > buf2 = new char[size];
    memset(buf.get(),  0, size);
    memset(buf2.get(), 0, size);

    if (log_write) {
        log_write->clear();
    }
    if (log_read) {
        log_read->clear();
    }

    string key;
    for (unsigned i = 0; i < repeats; ) {
        
        vector<string> keys;
        vector<unsigned> idx0;
        vector<unsigned> idx1;

        unsigned j;
        for (j = 0; j < 100 && i < repeats; ++j, ++i) {
            unsigned i0, i1;
            i0 = rand() % (size-1);
            i1 = rand() % (size-1);

            char* ch = buf.get();
            ch[i0] = 10;
            ch[i1] = 127;

            key = s_PutBlob(host, port, buf.get(), size, log_write);

            keys.push_back(key);
            idx0.push_back(i0);
            idx1.push_back(i1);

            // take every "key_factor" key, 
            // so we have evenly (across db pages)
            // distributed slice of the database
            if (i % key_factor == 0 && rep_keys) {
                rep_keys->push_back(key);
            }

            ch[i0] = ch[i1] = 0;

            if (i % 1000 == 0) 
                cout << "." << flush;
        }

        for (unsigned k = 0; k < j; ++k) {
            key = keys[k];

            unsigned i0, i1;
            i0 = idx0[k];
            i1 = idx1[k];

            char* ch = buf.get();
            ch[i0] = 10;
            ch[i1] = 127;

            bool exists = 
                s_CheckExists(host, port, 
                              key, (unsigned char*)buf2.get(), size, log_read);
            if (!exists) {
                cerr << "Not found: " << key << endl;
            }
            assert(exists);

            int cmp = memcmp(buf.get(), buf2.get(), size);
            assert(cmp == 0);
            ch[i0] = ch[i1] = 0;
        }

        keys.resize(0);
        idx0.resize(0);
        idx1.resize(0);

    }
    cout << endl;
}

///@internal
static
void s_TestKeysRead(vector<string>&           rep_keys,
                    unsigned                  size,
                    vector<STransactionInfo>* log_read)
{
    AutoPtr<char, ArrayDeleter<char> > buf  = new char[size];

    ITERATE(vector<string>, it, rep_keys) {
        const string& key = *it;
        bool exists = 
            s_CheckExists("", 0, 
                            key, (unsigned char*)buf.get(), size, log_read);
        assert(exists);
    }
}


void CTestNetCacheClient::Init(void)
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Net cache client");
    
    arg_desc->AddPositional("hostname", 
                            "NetCache host name.", CArgDescriptions::eString);

    arg_desc->AddPositional("port",
                            "Port number.", 
                            CArgDescriptions::eInteger);
    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

//    CONNECT_Init(&GetConfig());

    SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
}

static
void s_RemoveBLOB_Test(const string& host, unsigned short port)
{
    CNetCacheClient nc(host, port, "test");

    char z = 'Z';
    string key = nc.PutData(&z, 1, 1);

    NcbiCout << "Removing: " << key << NcbiEndl;
    nc.Remove(key);
    z = 0;

    SleepMilliSec(700);


    CNetCacheClient::EReadResult rr =  nc.GetData(key, &z, 1);
    assert(rr == CNetCacheClient::eNotFound);
}

static
void s_ReadUpdateCharTest(const string& host, unsigned short port)
{
    CNetCacheClient nc(host, port, "test");

    char z = 'Z';
    string key = nc.PutData(&z, 1, 100);
    cout << endl << key << endl << endl;;
    z = 'Q';
    nc.PutData(key, &z, 1, 100);

    z = 0;
    size_t blob_size = 0;
    CNetCacheClient::EReadResult rr =  
        nc.GetData(key, &z, 1, &blob_size);

    assert(rr == CNetCacheClient::eReadComplete);
    assert(z == 'Q');
    assert(blob_size == 1);

    for (int i = 0; i < 10; ++i) {
        char z = 'X';
        nc.PutData(key, &z, 1, 100);

        z = 'Y';
        nc.PutData(key, &z, 1, 100);

        z = 0;
        size_t blob_size = 0;
        CNetCacheClient::EReadResult rr =  
            nc.GetData(key, &z, 1, &blob_size);

        assert(rr == CNetCacheClient::eReadComplete);
        assert(z == 'Y');
        assert(blob_size == 1);
    }

}


/*
void s_TestAlive(const string& host, unsigned short port)
{
    CNetCacheClient ncc (host, port, "test");
    bool b = ncc.IsAlive();
    assert(b);
    b = ncc.IsAlive();
    assert(b);
    
    for (int i = 0; i < 2000; ++i) {
        b = ncc.IsAlive();
        assert(b);        
    }
}
*/
        

void s_TestClientLB(const string& service_name)
{
    cout << "Load Balanced test." << endl;

    CNetCacheClient_LB nc_client("test", service_name, 2, 10);
    const char test_data[] = "A quick brown fox, jumps over lazy dog.";
    const char test_data2[] = "Test 2.";
    string key = nc_client.PutData(test_data, sizeof(test_data));
    NcbiCout << key << NcbiEndl;

    assert(!key.empty());
    char dataBuf[1024];
    memset(dataBuf, 0xff, sizeof(dataBuf));
    size_t blob_size;
    IReader* reader = nc_client.GetData(key, &blob_size);
    assert(reader);
    reader->Read(dataBuf, 1024);
    delete reader;

    int res = strcmp(dataBuf, test_data);
    assert(res == 0);

    assert(blob_size == sizeof(test_data));

    vector<string> keys;

    cout << "Saving BLOBS..." << endl;
    {{
    for (unsigned i = 0; i < 1000; ++i) {
        key = nc_client.PutData(test_data, sizeof(test_data));
        keys.push_back(key);
        if (i % 200 == 0) cout << "." << flush;
    }
    }}

    cout << endl << "Done." << endl;

    cout << "Read/Update test..." << endl;
    {{
    for (unsigned i = 0; i < keys.size(); ++i) {
        key = keys[i];

        char data_Buf[1024];
        memset(data_Buf, 0xff, sizeof(data_Buf));
        size_t blob_size;
        IReader* reader = nc_client.GetData(key, &blob_size);
        assert(reader);
        reader->Read(data_Buf, 1024);
        delete reader;
        int res = strcmp(data_Buf, test_data);
        assert(res == 0);
        assert(blob_size == sizeof(test_data));

        IWriter* wrt = nc_client.PutData(&key);
        size_t bytes_written;
        //ERW_Result wres = 
            wrt->Write(test_data2, sizeof(test_data2), &bytes_written);
        delete wrt;

        memset(data_Buf, 0xff, sizeof(data_Buf));
        reader = nc_client.GetData(key, &blob_size);
        assert(reader);
        reader->Read(data_Buf, 1024);
        delete reader;
        res = strcmp(data_Buf, test_data2);
        assert(res == 0);
        assert(blob_size == sizeof(test_data2));

        if (i % 200 == 0) cout << "." << flush;

    }
    }}

    cout << endl << "Done." << endl;

    cout << "Load Balanced test complete." << endl;

}


int CTestNetCacheClient::Run(void)
{
    CArgs args = GetArgs();
    const string& host  = args["hostname"].AsString();
    unsigned short port = args["port"].AsInteger();

    const char test_data[] = "A quick brown fox, jumps over lazy dog.";
    const char test_data2[] = "New data.";
    string key;
/*
string key1;
  const string nc_server = "NC_Sage";
  string client_application="sagexp1";

{{
size_t sz = 1024 * 1024;
char* buf = new char[sz];

  CNetCacheClient_LB nc_client1( client_application, nc_server);
  key1=nc_client1.PutData(buf, sz);
cout << key1 << endl;
  delete buf;
}}

{{
  CNetCacheClient_LB nc_client1( client_application, nc_server);
  IReader* rd = nc_client1.GetData(key1);
  if (rd == 0) {
      cerr << "Problem" << endl;
      return 1;
  }
  delete rd;
}}
return 1;
*/
    // test load balancer
    /*
    {{
        CNetCacheClient nc_client("test");
        NetCache_ConfigureWithLB(&nc_client, "NetCache_shared");
        key = nc_client.PutData(test_data, sizeof(test_data));
        NcbiCout << key << NcbiEndl;

        assert(!key.empty());
        return 1;
    }}
    */

    {{
        CNetCacheClient nc_client(host, port, "test");

        key = nc_client.PutData((const void*)0, 0, 120);
        NcbiCout << key << NcbiEndl;
        assert(!key.empty());

        SleepMilliSec(700);
       

        size_t bsize;
        auto_ptr<IReader> rdr(nc_client.GetData(key, &bsize));

        assert(bsize == 0);


    }}


    {{
        CSocket sock(host, port);
        CNetCacheClient nc_client(&sock, "test");

        key = nc_client.PutData(test_data, sizeof(test_data));
        NcbiCout << key << NcbiEndl;
        assert(!key.empty());

        unsigned id = CNetCache_GetBlobId(key);
        CNetCache_Key pk;
        CNetCache_ParseBlobKey(&pk, key);
        assert(pk.id == id);

    }}

    {{
        CSocket sock(host, port);
        CNetCacheClient nc_client(&sock, "test");

        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));
        size_t blob_size;
        IReader* reader = nc_client.GetData(key, &blob_size);
        assert(reader);
        reader->Read(dataBuf, 1024);
        delete reader;

        int res = strcmp(dataBuf, test_data);
        assert(res == 0);

        assert(blob_size == sizeof(test_data));
    }}

    {{
        CNetCacheClient nc_client("test");

        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));

        CNetCacheClient::EReadResult rres = 
            nc_client.GetData(key, dataBuf, sizeof(dataBuf));
        assert(rres == CNetCacheClient::eReadComplete);

        int res = strcmp(dataBuf, test_data);
        assert(res == 0);
    }}

    // update existing BLOB
    {{
        {
        CNetCacheClient nc_client(host, port, "test");
        nc_client.PutData(key, test_data2, sizeof(test_data2)+1);
        }
        {
        CNetCacheClient nc_client(host, port, "test");
        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));
        CNetCacheClient::EReadResult rres = 
            nc_client.GetData(key, dataBuf, sizeof(dataBuf));
        assert(rres == CNetCacheClient::eReadComplete);
        int res = strcmp(dataBuf, test_data2);
        assert(res == 0);

        }
    }}


    // timeout test
    {{
        CSocket sock(host, port);
        CNetCacheClient nc_client(&sock, "test");

        key = nc_client.PutData(test_data, sizeof(test_data), 80);
        assert(!key.empty());
        sock.Close();
    }}

    bool exists;
    exists = s_CheckExists(host, port, key);
    assert(exists);

    CNetCacheClient nc_client(host, port, "test");
    nc_client.Remove(key);

    exists = s_CheckExists(host, port, key);
    assert(!exists);

    s_RemoveBLOB_Test(host, port);

    s_ReadUpdateCharTest(host, port);

//    s_TestClientLB("NetCache_shared");
    
    NcbiCout << "Testing IsAlive()... ";
//    s_TestAlive(host, port);
    NcbiCout << "Ok." << NcbiEndl;

/*
    unsigned delay = 70;
    cout << "Sleeping for " << delay << " seconds. Please wait...." << flush;
    SleepSec(delay);
    cout << "ok." << endl;

    exists = s_CheckExists(host, port, key);
    assert(!exists);
*/

  

    vector<STransactionInfo> log;
    vector<STransactionInfo> log_read;
    vector<string>           rep_keys;

    // Test writing HUGE BLOBs (uncomment and use carefully)
/*
    s_StressTest(host, port, 1024 * 1024 * 50, 10, &log, &log_read, &rep_keys, 30);
    NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
    s_ReportStatistics(log);
    NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
    s_ReportStatistics(log_read);
    NcbiCout << NcbiEndl;

    return 0;
*/

    s_StressTest(host, port, 921600, 50, &log, &log_read, &rep_keys, 30);
    NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
    s_ReportStatistics(log);
    NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
    s_ReportStatistics(log_read);
    NcbiCout << NcbiEndl;


    unsigned repeats = 5000;

    s_StressTest(host, port, 256, repeats, &log, &log_read, &rep_keys, 10);
    NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
    s_ReportStatistics(log);
    NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
    s_ReportStatistics(log_read);
    NcbiCout << NcbiEndl << NcbiEndl;


    s_StressTest(host, port, 1024 * 5, repeats, &log, &log_read, &rep_keys, 10);
    NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
    s_ReportStatistics(log);
    NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
    s_ReportStatistics(log_read);
    NcbiCout << NcbiEndl;

    s_StressTest(host, port, 1024 * 100, repeats/2, &log, &log_read, &rep_keys, 20);
    NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
    s_ReportStatistics(log);
    NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
    s_ReportStatistics(log_read);
    NcbiCout << NcbiEndl;

    s_StressTest(host, port, 1024 * 1024 * 5, repeats/50, &log, &log_read, &rep_keys, 30);
    NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
    s_ReportStatistics(log);
    NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
    s_ReportStatistics(log_read);
    NcbiCout << NcbiEndl;


    log_read.resize(0);
    NcbiCout << NcbiEndl << "Random BLOB read statistics. Number of BLOBs=" 
             << rep_keys.size() 
             << NcbiEndl;
    s_TestKeysRead(rep_keys, 1024 * 1024 * 10, &log_read);
    s_ReportStatistics(log_read);
    NcbiCout << NcbiEndl;



/*
    cout << "Shutdown server" << endl;
    // Shutdown server

    {{
        CNetCacheClient nc_client(host, port);
        nc_client.ShutdownServer();
    }}
*/    
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetCacheClient().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/22 13:17:28  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
