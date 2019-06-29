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
 * Authors:  Anatoliy Kuznetsov, Rafael Sadyrov
 *
 * File Description:  Network ICache client test
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/neticache_client.hpp>

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_types.h>
#include <connect/ncbi_core_cxx.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/test_boost.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/rwstream.hpp>

#include <util/cache/cache_async.hpp>

#include <random>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////


// Configuration parameters
NCBI_PARAM_DECL(string, netcache, service_name);
typedef NCBI_PARAM_TYPE(netcache, service_name) TNetCache_ServiceName;
NCBI_PARAM_DEF(string, netcache, service_name, "NC_UnitTest");

NCBI_PARAM_DECL(string, netcache, cache_name);
typedef NCBI_PARAM_TYPE(netcache, cache_name) TNetCache_CacheName;
NCBI_PARAM_DEF(string, netcache, cache_name, "blobs");

static const string s_ClientName("test_ic_client");

static int s_Run()
{
    const string service  = TNetCache_ServiceName::GetDefault();
    const string cache_name  = TNetCache_CacheName::GetDefault();

    CNetICacheClient cl(service, cache_name, s_ClientName);
    cl.SetFlags(ICache::fBestReliability);

    const static char test_data[] =
            "The quick brown fox jumps over the lazy dog.";
    const static size_t data_size = sizeof(test_data) - 1;

    string uid = GetDiagContext().GetStringUID();
    string key1 = "TEST_IC_CLIENT_KEY1_" + uid;
    string key2 = "TEST_IC_CLIENT_KEY2_" + uid;
    string subkey = "TEST_IC_CLIENT_SUBKEY_" + uid;
    int version = 0;

    {{
    cl.Store(key1, version, subkey, test_data, data_size);
    SleepMilliSec(700);

    bool hb = cl.HasBlobs(key1, subkey);
    BOOST_REQUIRE(hb);

    size_t sz = cl.GetSize(key1, version, subkey);
    BOOST_REQUIRE(sz == data_size);

    char buf[1024] = {0,};
    cl.Read(key1, version, subkey, buf, sizeof(buf) - 1);

    BOOST_REQUIRE(strcmp(buf, test_data) == 0);

    memset(buf, 0, sizeof(buf));
    cl.Read(key1, version, subkey, buf, sizeof(buf) - 1);

    BOOST_REQUIRE(strcmp(buf, test_data) == 0);

    memset(buf, 0, sizeof(buf));
    cl.ReadPart(key1, version, subkey, sizeof("The ") - 1,
        sizeof("The quick") - sizeof("The "), buf, sizeof(buf) - 1);

    BOOST_REQUIRE(strcmp(buf, "quick") == 0);


    sz = cl.GetSize(key1, version, subkey);
    BOOST_REQUIRE(sz == data_size);
    hb = cl.HasBlobs(key1, subkey);
    BOOST_REQUIRE(hb);

    cl.Remove(key1, version, subkey);
    hb = cl.HasBlobs(key1, subkey);
    BOOST_REQUIRE(!hb);

    }}

    {{
    size_t test_size = 1024 * 1024 * 2;
    vector<unsigned char> test_buf(test_size+10);
    memset(test_buf.data(), 127, test_size);

    cl.Store(key2, version, subkey, test_buf.data(), test_size);

    memset(test_buf.data(), 0, test_size);

    SleepMilliSec(700);

    size_t sz = cl.GetSize(key2, version, subkey);
    BOOST_REQUIRE(sz == test_size);


    cl.Read(key2, version, subkey, test_buf.data(), test_size);

    for (size_t i = 0; i < test_size; ++i) {
        if (test_buf.data()[i] != 127)  // otherwise, too slow under Valgrind
            BOOST_REQUIRE(test_buf[i] == 127);
    }

    sz = cl.GetSize(key2, version, subkey);
    BOOST_REQUIRE(sz == test_size);

    cl.Remove(key2, version, subkey);
    bool hb = cl.HasBlobs(key2, subkey);
    BOOST_REQUIRE(!hb);
    }}

    char test_buf[240];
    // Initialize buffer to avoid mem-checker "Read from uninit memory" errors
    memset(test_buf, 125, sizeof(test_buf));

    NcbiCout << "stress write" << NcbiEndl;

    // stress write
    {{
        size_t test_size = sizeof(test_buf);
        for (int i = 0; i < 100; ++i) {
            string key = "TEST_IC_CLIENT_KEY_" + NStr::IntToString(i) + "_" + uid;
            cl.Store(key, 0, subkey, test_buf, test_size);
            cl.Remove(key, 0, subkey);
        }
    }}

    NcbiCout << "check reader/writer" << NcbiEndl;
    {{
        {
        unique_ptr<IWriter> writer1(cl.GetWriteStream(key1, version, subkey));
        string str = "qwerty";
        writer1->Write(str.c_str(), str.size());
        }
        size_t size = cl.GetSize(key1, version, subkey);
        BOOST_REQUIRE(size <= sizeof(test_buf));
        BOOST_REQUIRE(cl.Read(key1, version, subkey, test_buf, sizeof(test_buf)));
        cout << size << endl << string(test_buf, size) << endl;
        cl.Remove(key1, version, subkey);
    }}

#ifdef DISABLED_UNTIL_NC_WITH_AGE_SUPPORT_IS_DEPLOYED
    NcbiCout << "blob age test" << NcbiEndl;
    {{
        ++version;
        cl.Store(key1, version, subkey, test_data, data_size);

        char buffer[1024];

        ICache::SBlobAccessDescr blob_access(buffer, sizeof(buffer));

        blob_access.return_current_version = true;
        blob_access.maximum_age = 5;

        SleepSec(2);

        cl.GetBlobAccess(key1, 0, subkey, &blob_access);

        BOOST_REQUIRE(blob_access.blob_found);
        BOOST_REQUIRE(blob_access.reader.get() == NULL);
        BOOST_REQUIRE(blob_access.blob_size < blob_access.buf_size);
        BOOST_REQUIRE(memcmp(blob_access.buf, test_data, data_size) == 0);
        BOOST_REQUIRE(blob_access.current_version == version);
        BOOST_REQUIRE(blob_access.current_version_validity == ICache::eCurrent);
        BOOST_REQUIRE(blob_access.actual_age >= 2);

        blob_access.return_current_version = false;
        blob_access.maximum_age = 2;

        SleepSec(1);

        cl.GetBlobAccess(key1, version, subkey, &blob_access);

        BOOST_REQUIRE(!blob_access.blob_found);

        cl.Remove(key1, version, subkey);
    }}
#endif /* DISABLED_UNTIL_NC_WITH_AGE_SUPPORT_IS_DEPLOYED */

    return 0;
}


struct SCtx
{
    string key;
    int version = 0;
    string subkey;

    friend ostream& operator<< (ostream& os, const SCtx& ctx)
    {
        return os << ". Blob: " << ctx.key << ' ' << ctx.version << ' ' << ctx.subkey;
    }
};

static void s_SimpleTest()
{
    const string service  = TNetCache_ServiceName::GetDefault();
    const string cache_name  = TNetCache_CacheName::GetDefault();

    CNetICacheClient api(service, cache_name, s_ClientName);
    api.SetFlags(ICache::fBestReliability);

    const int kIterations = 50;
    const size_t kSrcSize = 20 * 1024 * 1024; // 20MB
    const size_t kBufSize = 100 * 1024; // 100KB

    vector<Uint8> src;
    src.resize(kSrcSize / sizeof(Uint8));
    auto random_uint8 = bind(uniform_int_distribution<Uint8>(), mt19937());
    generate_n(src.begin(), src.size(), random_uint8);

    vector<char> buf;
    buf.resize(kBufSize);

    SCtx ctx;
    ctx.key = to_string(time(NULL)) + "t" + to_string(random_uint8());
    vector<string> subkeys;

    for (ctx.version = 0; ctx.version < kIterations; ++ctx.version) {
        ctx.subkey = to_string(random_uint8());
        subkeys.push_back(ctx.subkey);

        try {
            // Creating blob
            auto ptr = reinterpret_cast<const char*>(src.data());

            api.Store(ctx.key, ctx.version, ctx.subkey, ptr, kSrcSize);
            BOOST_REQUIRE_MESSAGE(api.HasBlob(ctx.key, ctx.subkey),
                    "Blob does not exist" << ctx);
            BOOST_REQUIRE_MESSAGE(api.GetBlobSize(ctx.key, ctx.version, ctx.subkey) == kSrcSize,
                    "Blob size (GetBlobSize) differs from the source" << ctx);

            // Checking blob
            size_t size = 0;
            unique_ptr<IReader> reader(api.GetReadStream(ctx.key, ctx.version, ctx.subkey, &size));

            BOOST_REQUIRE_MESSAGE(size == kSrcSize,
                    "Blob size (GetData) differs from the source" << ctx);
            BOOST_REQUIRE_MESSAGE(reader.get(),
                    "Failed to get reader" << ctx);

            for (;;) {
                size_t read = 0;

                switch (reader->Read(buf.data(), kBufSize, &read)) {
                case eRW_Success:
                    BOOST_REQUIRE_MESSAGE(!memcmp(buf.data(), ptr, read),
                            "Blob content does not match the source" << ctx);
                    BOOST_REQUIRE_MESSAGE(size >= read,
                            "Blob size is greater than the source" << ctx);
                    ptr += read;
                    size -= read;
                    continue;

                case eRW_Eof:
                    BOOST_REQUIRE_MESSAGE(!size,
                            "Blob size is less than the source" << ctx);
                    break;

                default:
                    BOOST_FAIL("Reading blob failed" << ctx);
                }

                break;
            }
        }
        catch (...) {
            BOOST_ERROR("An exception has been caught" << ctx);
            throw;
        }
    }

    try {
        using namespace ncbi::grid::netcache::search;

        auto received = api.Search(fields::key == ctx.key);

        if (received.size() != subkeys.size()) {
            BOOST_ERROR("Received unexpected number of subkeys: " <<
                    received.size() << " vs " << subkeys.size());
        }

        set<string> expected(subkeys.begin(), subkeys.end());

        for (auto& i : received) {
            ctx.subkey = i[fields::subkey];

            if (expected.find(ctx.subkey) == expected.end()) {
                BOOST_ERROR("Received unexpected subkey " << ctx.subkey <<
                        " for key " << ctx.key);
            }
        }
    }
    catch (...) {
        BOOST_ERROR("An exception has been caught. Key: " << ctx.key);
        throw;
    }

    // Removing every other blob

    ctx.version = 0;

    for (const auto& subkey : subkeys) {
        if (++ctx.version % 2) continue;

        ctx.subkey = subkey;

        try {
            // Removing blob
            api.RemoveBlob(ctx.key, ctx.version, ctx.subkey);

            // Checking removed blob
            BOOST_REQUIRE_MESSAGE(!api.HasBlob(ctx.key, ctx.subkey),
                    "Removed blob still exists" << ctx);
            size_t size = 0;
            unique_ptr<IReader> fail_reader(api.GetReadStream(ctx.key, ctx.version, ctx.subkey, &size));
            BOOST_REQUIRE_MESSAGE(!fail_reader.get(),
                    "Got reader for removed blob" << ctx);
        }
        catch (...) {
            BOOST_ERROR("An exception has been caught" << ctx);
            throw;
        }
    }


    // Creating blob with a different key to check Purge()

    SCtx purge_ctx;
    purge_ctx.key = to_string(time(NULL)) + "t" + to_string(random_uint8());
    purge_ctx.subkey = subkeys.back();
    auto ptr = reinterpret_cast<const char*>(src.data());

    api.Store(purge_ctx.key, purge_ctx.version, purge_ctx.subkey, ptr, kSrcSize);

    BOOST_REQUIRE_MESSAGE(api.HasBlob(purge_ctx.key, purge_ctx.subkey),
            "Check blob does not exist" << purge_ctx);


    // Removing all remaining blobs with the same key

    api.Purge(ctx.key, kEmptyStr, 0);


    // Checking Purge() result

    BOOST_REQUIRE_MESSAGE(api.HasBlob(purge_ctx.key, purge_ctx.subkey),
            "Check blob was purged" << purge_ctx);

    ctx.version = 0;

    for (const auto& subkey : subkeys) {
        ++ctx.version;
        ctx.subkey = subkey;

        try {
            // Checking removed blob
            BOOST_REQUIRE_MESSAGE(!api.HasBlob(ctx.key, ctx.subkey),
                    "Removed blob still exists" << ctx);
            size_t size = 0;
            unique_ptr<IReader> fail_reader(api.GetReadStream(ctx.key, ctx.version, ctx.subkey, &size));
            BOOST_REQUIRE_MESSAGE(!fail_reader.get(),
                    "Got reader for removed blob" << ctx);
        }
        catch (...) {
            BOOST_ERROR("An exception has been caught" << ctx);
            throw;
        }
    }


    // Removing check blob
    api.RemoveBlob(purge_ctx.key, purge_ctx.version, purge_ctx.subkey);
}

static void s_AsyncTest()
{
    const string service  = TNetCache_ServiceName::GetDefault();
    const string cache_name  = TNetCache_CacheName::GetDefault();

    unique_ptr<CNetICacheClient> main_cache(new CNetICacheClient(service, cache_name, s_ClientName));
    main_cache->SetFlags(ICache::fBestReliability);

    unique_ptr<CNetICacheClient> writer_cache(new CNetICacheClient(service, cache_name, s_ClientName));
    writer_cache->SetFlags(ICache::fBestReliability);

    CNetICacheClient api(service, cache_name, s_ClientName);

    const int kIterations = 50;
    const size_t kSrcSize = 20 * 1024 * 1024; // 20MB
    const size_t kBufSize = 100 * 1024; // 100KB
    vector<Uint8> src;
    vector<char> buf;

    src.resize(kSrcSize / sizeof(Uint8));
    buf.resize(kBufSize);

    auto random_uint8 = bind(uniform_int_distribution<Uint8>(), mt19937());

    SCtx ctx;
    ctx.key = to_string(time(NULL)) + "t" + to_string(random_uint8());
    vector<string> subkeys;

    {
        CAsyncWriteCache async_api(main_cache.release(), writer_cache.release(), 100.0);
        vector<unique_ptr<IWriter>> writers;


        for (ctx.version = 0; ctx.version < kIterations; ++ctx.version) {
            ctx.subkey = to_string(random_uint8());
            subkeys.push_back(ctx.subkey);

            try {
                // Creating blob
                generate_n(src.begin(), src.size(), random_uint8);
                auto ptr = reinterpret_cast<const char*>(src.data());

                writers.emplace_back(async_api.GetWriteStream(ctx.key, ctx.version, ctx.subkey));
                CWStream os(writers.back().get());
                os.write(ptr, kSrcSize);

                BOOST_REQUIRE_MESSAGE(os.good(), "Write failed" << ctx);

#ifdef NCBI_THREADS
                // Blob is not yet created as writer is not yet destroyed (actual async write happens after)
                BOOST_REQUIRE_MESSAGE(!async_api.HasBlobs(ctx.key, ctx.subkey), "Blob does exist" << ctx);
#endif
            }
            catch (...) {
                BOOST_ERROR("An exception has been caught" << ctx);
                throw;
            }
        }

        // Actual async write starts here
    }

    try {
        using namespace ncbi::grid::netcache::search;

        auto received = api.Search(fields::key == ctx.key);

        if (received.size() != subkeys.size()) {
            BOOST_ERROR("Received unexpected number of subkeys: " <<
                    received.size() << " vs " << subkeys.size());
        }

        set<string> expected(subkeys.begin(), subkeys.end());

        for (auto& i : received) {
            ctx.subkey = i[fields::subkey];

            if (expected.find(ctx.subkey) == expected.end()) {
                BOOST_ERROR("Received unexpected subkey " << ctx.subkey << " for key " << ctx.key);
            }
        }
    }
    catch (...) {
        BOOST_ERROR("An exception has been caught. Key: " << ctx.key);
        throw;
    }

    // Checking all blobs and removing them

    ctx.version = 0;

    for (const auto& subkey : subkeys) {
        ctx.subkey = subkey;

        try {
            BOOST_REQUIRE_MESSAGE(api.HasBlobs(ctx.key, ctx.subkey), "Blob does not exist" << ctx);
            BOOST_REQUIRE_MESSAGE(api.GetSize(ctx.key, ctx.version, ctx.subkey) == kSrcSize,
                    "Blob size (GetSize) differs from the source" << ctx);

            // Removing blob
            api.Remove(ctx.key, ctx.version, ctx.subkey);

            // Checking removed blob
            BOOST_REQUIRE_MESSAGE(!api.HasBlobs(ctx.key, ctx.subkey), "Removed blob still exists" << ctx);
        }
        catch (...) {
            BOOST_ERROR("An exception has been caught" << ctx);
            throw;
        }

        ++ctx.version;
    }
}

BOOST_AUTO_TEST_SUITE(NetICacheClient)

NCBITEST_AUTO_INIT()
{
    // Set client IP so AppLog entries could be filtered by it
    CRequestContext& req = CDiagContext::GetRequestContext();
    unsigned addr = CSocketAPI::GetLocalHostAddress();
    req.SetClientIP(CSocketAPI::ntoa(addr));
}

BOOST_AUTO_TEST_CASE(OldTest)
{
    EDiagSev prev_level = SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
    s_Run();
    SetDiagTrace(eDT_Disable);
    SetDiagPostLevel(prev_level);
}

BOOST_AUTO_TEST_CASE(SimpleTest)
{
    s_SimpleTest();
}

BOOST_AUTO_TEST_CASE(AsyncTest)
{
    s_AsyncTest();
}

BOOST_AUTO_TEST_SUITE_END()
