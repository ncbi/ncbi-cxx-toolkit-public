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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  DBAPI BLOB cache test
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <dbapi/cache/dbapi_blob_cache.hpp>
#include <dbapi/driver/drivers.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

/// Test application
///
/// @internal
///
class CDBAPI_CacheTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CDBAPI_CacheTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_bdb",
                       "test BDB library");
    SetupArgDescriptions(d.release());
}


int CDBAPI_CacheTest::Run(void)
{
    cout << "Run CDBAPI_CacheTest test" << endl << endl;

    try {
        CDriverManager &db_drv_man = CDriverManager::GetInstance(); 
        string drv_name;
#ifdef NCBI_OS_MSWIN
        DBAPI_RegisterDriver_ODBC(db_drv_man);
        drv_name = "odbc";
#else
        DBAPI_RegisterDriver_FTDS(db_drv_man);
        drv_name = "ftds";
#endif
        const string server = "MSSQL10";
        const string database = "NCBI_Cache";
        const string login = "cwrite";
        const string password = "allowed";

        {{
        CDBAPI_Cache  blob_cache;
        blob_cache.Open(drv_name, server, database, login, password);



        int top = blob_cache.GetTimeStampPolicy();
        blob_cache.SetTimeStampPolicy(top, 30);

        blob_cache.Purge("", "", 30);

        const char* szTest = "01234567890";

        blob_cache.Store("key_1",
                         1,
                         "",
                         szTest,
                         strlen(szTest));


        size_t blob_size = blob_cache.GetSize("key_1",
                                               1,
                                               "");

        int s = strlen(szTest);
        assert(blob_size == s);

        char blob_buf[1024] = {0,};
        blob_cache.Read("key_1",
                        1,
                        "", 
                        blob_buf,
                        1024);

        int cmp = strcmp(blob_buf, szTest);
        assert(cmp == 0);

        {
        auto_ptr<IWriter> wrt(blob_cache.GetWriteStream("key_3", 1, "sk1"));
        size_t bytes_written;
        int s = strlen(szTest);
        wrt->Write(szTest, s, &bytes_written);
        wrt->Flush();
        }

        {
        char str[1024] = {0,};
        char* sp = str;
        auto_ptr<IReader> rdr(blob_cache.GetReadStream("key_3", 1, "sk1"));
        char buf[1024];
        ERW_Result r;
        size_t rd;
        do {
            r = rdr->Read(buf, 1, &rd);
            if (r != eRW_Success)
                break;
            *sp++ = buf[0];
        } while (rd > 0);
        int c = strcmp(szTest, str);
        assert(c == 0);

        }

        // dump huge BLOB
        {
            unsigned bsize = 10 * 1024 * 1024;
            int* big_blob = new int[bsize];

            for (unsigned i = 0; i < bsize; ++i) {
                big_blob[i] = i;
            }

            {
            auto_ptr<IWriter> wrt(blob_cache.GetWriteStream("key_big", 1, ""));
            unsigned written = 0;
            int* p = big_blob;
            while (written < bsize) {
                size_t bytes_written;
                unsigned elements_to_write = 10;
                unsigned chunk_size = elements_to_write * sizeof(int);
                wrt->Write(p, chunk_size, &bytes_written);
                p += elements_to_write;
                written += elements_to_write;
                assert(bytes_written == chunk_size);
            } // while
            wrt->Flush();
            }
    
            auto_ptr<IReader> rdr(blob_cache.GetReadStream("key_big", 1, ""));
            unsigned bn = 0;
            
            ERW_Result r;
            size_t rd;
            unsigned int cnt = 0;
            do {
                int buf[1];
                r = rdr->Read(buf, sizeof(int), &rd);
                if (r != eRW_Success)
                    break;
                if (cnt != buf[0]) {
                    cerr << "BLOB comparison error element idx = " << cnt <<
                            " value=" << buf[0] << endl;
                    assert(cnt == buf[0]);
                }
                cnt++;
            } while (rd > 0);
        }
        blob_cache.Remove("key_big", 1, "");

        }}

    } catch( CDB_Exception& dbe ) {
        NcbiCerr  << dbe.what() << " " << dbe.Message() 
                  << NcbiEndl;
    }

    cout << endl;
    cout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


int main(int argc, const char* argv[])
{
    return CDBAPI_CacheTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/07/26 14:07:35  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
