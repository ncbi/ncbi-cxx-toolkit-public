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
 * Authors:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:  NetCache client test
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netcache_api.hpp>

#include <connect/ncbi_types.h>
#include <connect/ncbi_core_cxx.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////

static const string s_ClientName("test_netcache_api");

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
bool s_CheckExists(CNetCacheAPI   nc_client,
                   const string&  key,
                   unsigned char* buf = 0,
                   size_t         buf_size = 0,
                   vector<STransactionInfo>* log = 0)
{

    STransactionInfo info;
    info.blob_size = buf_size;
    info.connection_time = 0;

    CStopWatch sw(CStopWatch::eStart);

    unsigned char dataBuf[1024] = {0,};

    if (buf == 0 || buf_size == 0) {
        buf = dataBuf;
        buf_size = sizeof(dataBuf);
    }

    try {
        CNetCacheAPI::EReadResult rres =
            nc_client.GetData(key, buf, buf_size);
        if (rres == CNetCacheAPI::eNotFound)
            return false;

        if (rres == CNetCacheAPI::eReadPart)
            ERR_POST("Blob too big");

        info.transaction_time = sw.Elapsed();
        if (log) {
            log->push_back(info);
        }
    }
    catch (CNetCacheException& ex)
    {
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound)
            return false;
        cout << ex.what() << endl;
        throw;
    }

    return true;
}

/// @internal
static
string s_PutBlob(
               CNetCacheAPI              nc_client,
               const void*               buf,
               size_t                    size,
               vector<STransactionInfo>* log)
{
    STransactionInfo info;
    info.blob_size = size;

    CStopWatch sw(CStopWatch::eStart);

    STimeout to = {120, 0};
    nc_client.SetCommunicationTimeout(to);

    info.connection_time = sw.Elapsed();
    sw.Restart();
    info.key = nc_client.PutData(buf, size, nc_blob_ttl = 60 * 8);
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
void s_StressTest(const string&             service,
                  size_t                    size,
                  unsigned int              repeats,
                  vector<STransactionInfo>* log_write,
                  vector<STransactionInfo>* log_read,

                  vector<string>*           rep_keys,  // key representatives
                  unsigned                  key_factor // repr. choose factor
                  )
{
    cout << "BLOB size = " << size
         << " Repeats = " << repeats
         << ". Please wait... " << endl;

    CNetCacheAPI nc_client(service, s_ClientName);

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

            key = s_PutBlob(nc_client, buf.get(), size, log_write);

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
                cout << "Iteration " << (i + 1) << "..." << NcbiEndl;
        }

        for (unsigned k = 0; k < j; ++k) {
            key = keys[k];

            unsigned i0, i1;
            i0 = idx0[k];
            i1 = idx1[k];

            char* ch = buf.get();
            ch[i0] = 10;
            ch[i1] = 127;

            bool exists = s_CheckExists(nc_client,
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
void s_TestKeysRead(CNetCacheAPI              nc_client,
                    vector<string>&           rep_keys,
                    unsigned                  size,
                    vector<STransactionInfo>* log_read)
{
    AutoPtr<char, ArrayDeleter<char> > buf  = new char[size];

    ITERATE(vector<string>, it, rep_keys) {
        const string& key = *it;
        bool exists = s_CheckExists(nc_client, key,
            (unsigned char*) buf.get(), size, log_read);
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

    arg_desc->AddDefaultKey("repeat", "times",
        "Number of stress test repetitions",
        CArgDescriptions::eInteger, "5");

    arg_desc->AddPositional("service",
                            "NetCache service.", CArgDescriptions::eString);


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

static
void s_RemoveBLOB_Test(const string& service)
{
    CNetCacheAPI nc(service, s_ClientName);

    char z = 'Z';
    string key = nc.PutData(&z, 1, nc_blob_ttl = 1);

    NcbiCout << "Removing: " << key << NcbiEndl;
    nc.Remove(key);
    z = 0;

    SleepMilliSec(700);


    CNetCacheAPI::EReadResult rr =  nc.GetData(key, &z, 1);
    assert(rr == CNetCacheAPI::eNotFound);
}

static
void s_ReadUpdateCharTest(const string& service)
{
    CNetCacheAPI nc(service, s_ClientName);

    char z = 'Z';
    string key = nc.PutData(&z, 1, nc_blob_ttl = 100);
    cout << endl << key << endl << endl;;
    z = 'Q';
    nc.PutData(key, &z, 1, nc_blob_ttl = 100);

    z = 0;
    size_t blob_size = 0;
    CNetCacheAPI::EReadResult rr =
        nc.GetData(key, &z, 1, &blob_size);

    assert(rr == CNetCacheAPI::eReadComplete);
    assert(z == 'Q');
    assert(blob_size == 1);

    for (int i = 0; i < 10; ++i) {
        char z = 'X';
        nc.PutData(key, &z, 1, nc_blob_ttl = 100);

        z = 'Y';
        nc.PutData(key, &z, 1, nc_blob_ttl = 100);

        z = 0;
        size_t blob_size = 0;
        CNetCacheAPI::EReadResult rr =
            nc.GetData(key, &z, 1, &blob_size);

        assert(rr == CNetCacheAPI::eReadComplete);
        assert(z == 'Y');
        assert(blob_size == 1);
    }

}

static int s_PasswordTest(const string& service)
{
    static const int err_code = 4;
    CNetCacheAPI nc(service, s_ClientName);
    string password("password");

    static const char data[] = "data";
    string key = nc.PutData(data, 4, nc_blob_password = password);

    char buffer[4];
    size_t bytes_read;

    CNetCacheAPI::EReadResult res = nc.GetData(key, buffer, 4, &bytes_read);
    if (res != CNetCacheAPI::eNotFound) {
        NcbiCout << "Error: successfully read a password-protected "
            "blob without supplying a password; key=" << key << NcbiEndl;
        return err_code;
    }

    res = nc.GetData(key, buffer, 4, &bytes_read,
            NULL, nc_blob_password = password);
    if (res != CNetCacheAPI::eReadComplete || bytes_read != 4 ||
            memcmp(data, buffer, 4) != 0) {
        NcbiCout << "Error reading a password-protected blob; key=" <<
            key << NcbiEndl;
        return err_code;
    }

    return 0;
}

static int s_AbortTest(const string& service)
{
    static const int err_code = 8;

    CNetCacheAPI nc(service, s_ClientName);

    string key;

    {{
        auto_ptr<IEmbeddedStreamWriter> writer(nc.PutData(&key,
                nc_caching_mode = CNetCacheAPI::eCaching_Disable));

        writer->Write("Hello", 5);
        writer->Abort();
    }}

    try {
        nc.GetBlobSize(key);
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return 0;
        NcbiCout << e.what();
    }

    NcbiCout << "Error: blob " << key << " exist after Abort()." << NcbiEndl;

    return err_code;
}

static int s_ServiceInBlobKeyTest(const string& service)
{
    static const int err_code = 16;

    static char data[] = "test_data";
    static size_t data_size = sizeof(data) - 1;

    CNetCacheAPI nc(service, s_ClientName);

    string blob_id = nc.PutData(data, data_size,
            (nc_mirroring_mode = CNetCacheAPI::eMirroringDisabled,
            nc_server_check_hint = false));
    CNetCacheKey key(blob_id);

    if (key.GetServiceName().empty() ||
            !key.GetFlag(CNetCacheKey::eNCKey_SingleServer))
        return err_code;

    string mangled_blob_id;
    CNetCacheKey::GenerateBlobKey(&mangled_blob_id, key.GetId(),
            key.GetHost(), key.GetPort(), key.GetVersion(),
            key.GetRandomPart(), key.GetCreationTime());
    // Change the service name, so that the primary server cannot be discovered.
    CNetCacheKey::AddExtensions(mangled_blob_id,
            "NoSuchService", key.GetFlags());

    try {
        string buffer;

        nc.ReadData(mangled_blob_id, buffer);

        if (buffer != data)
            return err_code;
    }
    catch (CException&) {
        return err_code;
    }

    try {
        string buffer;

        nc.ReadData(mangled_blob_id, buffer, nc_server_check = eOn);

        return err_code;
    }
    catch (CNetSrvConnException& e) {
        if (e.GetErrCode() != CNetSrvConnException::eServerNotInService)
            return err_code;
    }
    catch (CException&) {
        return err_code;
    }

    blob_id = nc.PutData(data, data_size,
            nc_mirroring_mode = CNetCacheAPI::eMirroringEnabled);
    key.Assign(blob_id);

    if (key.GetServiceName().empty() ||
            key.GetFlag(CNetCacheKey::eNCKey_SingleServer))
        return err_code;

    return 0;
}

static int s_BlobAgeTest(const string& service)
{
#ifdef DISABLED_UNTIL_NC_WITH_AGE_SUPPORT_IS_DEPLOYED
    static const int err_code = 32;

    static char data[] = "test_data";
    static size_t data_size = sizeof(data) - 1;

    CNetCacheAPI nc(service, s_ClientName);

    string blob_id = nc.PutData(data, data_size);

    SleepSec(2);

    try {
        string buffer;

        unsigned actual_age;

        nc.ReadData(blob_id, buffer,
                (nc_max_age = 5, nc_actual_age = &actual_age));

        if (buffer != data || actual_age < 2)
            return err_code;
    }
    catch (CException&) {
        return err_code;
    }

    SleepSec(1);

    try {
        string buffer;

        nc.ReadData(blob_id, buffer, nc_max_age = 2);

        return err_code;
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() != CNetCacheException::eBlobNotFound)
            return err_code;
    }
    catch (CException&) {
        return err_code;
    }
#endif /* DISABLED_UNTIL_NC_WITH_AGE_SUPPORT_IS_DEPLOYED */

    return 0;
}

static size_t s_ReadIntoBuffer(IReader* reader, char* buf_ptr, size_t buf_size)
{
    size_t bytes_read;
    size_t total_bytes_read = 0;

    while (buf_size > 0) {
        ERW_Result rw_res = reader->Read(buf_ptr, buf_size, &bytes_read);
        if (rw_res == eRW_Success) {
            total_bytes_read += bytes_read;
            buf_ptr += bytes_read;
            buf_size -= bytes_read;
        } else if (rw_res == eRW_Eof) {
            break;
        } else {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                "Error while reading BLOB");
        }
    }

    return total_bytes_read;
}

int CTestNetCacheClient::Run(void)
{
    int error_level = 0;

    CArgs args = GetArgs();
    const string& service  = args["service"].AsString();
    int stress_test_repetitions = args["repeat"].AsInteger();

    const char test_data[] = "The quick brown fox jumps over the lazy dog.";
    const char test_data2[] = "New data.";
    string key;

    {{
        CNetCacheAPI nc_client(service, s_ClientName);

        key = nc_client.PutData(NULL, 0, nc_blob_ttl = 120);
        NcbiCout << key << NcbiEndl;
        assert(!key.empty());

        SleepMilliSec(700);


        size_t bsize;
        auto_ptr<IReader> rdr(nc_client.GetData(key, &bsize));

        assert(rdr.get() != NULL);

        cout << bsize << endl;
        assert(bsize == 0);
    }}


    {{
        CNetCacheAPI nc_client(service, s_ClientName);

        key = nc_client.PutData(test_data, sizeof(test_data));
        NcbiCout << key << NcbiEndl;
        assert(!key.empty());

        unsigned id = CNetCacheKey(key).GetId();
        CNetCacheKey pk(key);
        assert(pk.GetId() == id);

    }}

    {{
        CNetCacheAPI nc_client(service, s_ClientName);

        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));

        size_t blob_size;
        IReader* reader = nc_client.GetData(key, &blob_size);
        assert(reader);
        assert(blob_size == sizeof(test_data));

        size_t bytes_read = s_ReadIntoBuffer(reader, dataBuf, sizeof(dataBuf));
        delete reader;

        assert(bytes_read == sizeof(test_data));

        int res = memcmp(dataBuf, test_data, sizeof(test_data));
        assert(res == 0);

        reader = nc_client.GetPartReader(key,
            sizeof(test_data) - sizeof("dog."), sizeof(dataBuf), &blob_size);

        assert(blob_size == sizeof("dog."));

        bytes_read = s_ReadIntoBuffer(reader, dataBuf, sizeof(dataBuf));

        assert(bytes_read == sizeof("dog."));

        delete reader;

        res = strcmp(dataBuf, "dog.");
        assert(res == 0);
    }}

    {{
        CNetCacheAPI nc_client(s_ClientName);

        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));

        CNetCacheAPI::EReadResult rres =
            nc_client.GetData(key, dataBuf, sizeof(dataBuf));
        assert(rres == CNetCacheAPI::eReadComplete);

        int res = strcmp(dataBuf, test_data);
        assert(res == 0);

        string str;

        nc_client.ReadPart(key, sizeof("The ") - 1,
            sizeof("The quick") - sizeof("The "), str);

        assert(str == "quick");
    }}

    // update existing BLOB
    {{
        {
        CNetCacheAPI nc_client(service, s_ClientName);
        nc_client.PutData(key, test_data2, sizeof(test_data2));
        }
        {
        CNetCacheAPI nc_client(service, s_ClientName);
        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));
        CNetCacheAPI::EReadResult rres =
            nc_client.GetData(key, dataBuf, sizeof(dataBuf));
        assert(rres == CNetCacheAPI::eReadComplete);
        int res = strcmp(dataBuf, test_data2);
        assert(res == 0);

        }
    }}


    // timeout test
    {{
        CNetCacheAPI nc_client(service, s_ClientName);

        key = nc_client.PutData(test_data, sizeof(test_data), nc_blob_ttl = 80);
        assert(!key.empty());
    }}

    CNetCacheAPI nc_client(service, s_ClientName);

    bool exists = s_CheckExists(nc_client, key);
    assert(exists);

    nc_client.Remove(key);

    SleepMilliSec(1800);

    exists = s_CheckExists(nc_client, key);
    assert(!exists);

    s_RemoveBLOB_Test(service);

    s_ReadUpdateCharTest(service);

    error_level |= s_PasswordTest(service);
    error_level |= s_AbortTest(service);
    error_level |= s_ServiceInBlobKeyTest(service);
    error_level |= s_BlobAgeTest(service);

    vector<STransactionInfo> log;
    vector<STransactionInfo> log_read;
    vector<string>           rep_keys;

    unsigned repeats = 1000;

    for (int i=0; i < stress_test_repetitions; ++i)
    {
        NcbiCout << "STRESS TEST " << (i + 1) << "/" <<
            stress_test_repetitions << NcbiEndl << NcbiEndl;

        s_StressTest(service, 256, repeats, &log, &log_read, &rep_keys, 10);
        NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
        s_ReportStatistics(log);
        NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
        s_ReportStatistics(log_read);
        NcbiCout << NcbiEndl << NcbiEndl;


        s_StressTest(service, 1024 * 5, repeats, &log, &log_read, &rep_keys, 10);
        NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
        s_ReportStatistics(log);
        NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
        s_ReportStatistics(log_read);
        NcbiCout << NcbiEndl;

        s_StressTest(service, 1024 * 100, repeats/2, &log, &log_read, &rep_keys, 20);
        NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
        s_ReportStatistics(log);
        NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
        s_ReportStatistics(log_read);
        NcbiCout << NcbiEndl;

        s_StressTest(service, 1024 * 1024 * 5, repeats/50, &log, &log_read, &rep_keys, 30);
        NcbiCout << NcbiEndl << "BLOB write statistics:" << NcbiEndl;
        s_ReportStatistics(log);
        NcbiCout << NcbiEndl << "BLOB read statistics:" << NcbiEndl;
        s_ReportStatistics(log_read);
        NcbiCout << NcbiEndl;

        log_read.resize(0);
        NcbiCout << NcbiEndl << "Random BLOB read statistics. Number of BLOBs="
                 << rep_keys.size()
                 << NcbiEndl;
        s_TestKeysRead(nc_client, rep_keys, 1024 * 1024 * 10, &log_read);
        s_ReportStatistics(log_read);
        NcbiCout << NcbiEndl;

        SleepSec(10);
    }

    return error_level;
}


int main(int argc, const char* argv[])
{
    SetDiagPostLevel(eDiag_Warning);
    return CTestNetCacheClient().AppMain(argc, argv, 0, eDS_Default);
}
