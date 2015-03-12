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
 * File Description: Test application for NCBI Berkeley DB library (BDB)
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbitime.hpp>
#include <stdio.h>

#include <db/bdb/bdb_expt.hpp>
#include <db/bdb/bdb_types.hpp>
#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_blob.hpp>
#include <db/bdb/bdb_map.hpp>
#include <db/bdb/bdb_blobcache.hpp>
#include <db/bdb/bdb_filedump.hpp>
#include <db/bdb/bdb_trans.hpp>
#include <db/bdb/bdb_query.hpp>
#include <db/bdb/bdb_util.hpp>
#include <db/bdb/bdb_split_blob.hpp>

#include <util/line_reader.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;




////////////////////////////////
// Test functions, classes, etc.
//


/// @internal
struct TestRec
{
    unsigned count;
    unsigned blob_size;
};


////////////////////////////////
// Test application
//

/// @internal
class CBDB_SplitTest : public CNcbiApplication
{
public:
    typedef CBDB_BlobSplitStore<bm::bvector<> > TBlobSplitStore;

public:
    void Init(void);
    int Run(void);

    void LoadTestSet(const string& file_name);

    void ReadTestSet(vector<TestRec> & test_set, const string& file_name);
    void LoadSplitStore(vector<TestRec>& test_set,
                        TBlobSplitStore& split_store);

};


void
CBDB_SplitTest::LoadSplitStore(vector<TestRec>& test_set,
                               TBlobSplitStore& split_store)
{
    unsigned blob_id = 1;
    unsigned round = 0;
    CBDB_RawFile::TBuffer buffer;

    for (size_t i = 0; i < test_set.size(); ++i) {
        TestRec& r = test_set[i];
        buffer.resize(r.blob_size);
        cout << "\nsize=" << r.blob_size << " count=" << r.count << endl;
        CStopWatch sw(CStopWatch::eStart);
        for (;r.count;r.count--) {
            split_store.UpdateInsert(blob_id,
                                     buffer.data(),
                                     buffer.size());
            ++blob_id;
            if ((r.count % 10000) == 0) {
                cerr << ".";
            }
        }
        cerr << "Elapsed = " << sw.Elapsed() << endl;
    } // for
}

void CBDB_SplitTest::LoadTestSet(const string& file_name)
{
    CBDB_Env env;
    env.SetLogInMemory(true);
    env.SetLogBSize(50 * 1024 * 1024);
    env.SetCacheSize(400 * 1024 * 1024);
    env.OpenWithTrans("e:\\db_split", CBDB_Env::eThreaded);

    vector<TestRec> test_set;
    ReadTestSet(test_set, file_name);
    vector<TestRec> test_set2(test_set);

    if (test_set.size() == 0) {
        cout << "Empty test load." << endl;
        return;
    }
    cout << "Loaded " << test_set.size() << " records." << endl;

    {{
    TBlobSplitStore split_store_hash(new CBDB_BlobDeMux);
    split_store_hash.Open("split_hash",
                          CBDB_RawFile::eCreate,
                          CBDB_RawFile::eHash);
    split_store_hash.SetEnv(env);
    //split_store_hash.SetVolumeCacheSize(100* 1024 * 1024);

    CStopWatch sw(CStopWatch::eStart);
    cout << "Loading hash store." << endl;
    LoadSplitStore(test_set, split_store_hash);
    cout << "Ok. elapsed=" << sw.Elapsed() << endl << endl;
    }}

    {{
    TBlobSplitStore split_store_btree(new CBDB_BlobDeMux);
    split_store_btree.Open("split_btree",
                            CBDB_RawFile::eCreate,
                            CBDB_RawFile::eBtree);
    split_store_btree.SetVolumeCacheSize(100 * 1024 * 1024);

    CStopWatch sw(CStopWatch::eStart);
    cout << "Loading btree store." << endl;
    LoadSplitStore(test_set2, split_store_btree);
    cout << "Ok. elapsed=" << sw.Elapsed() << endl << endl;
    }}

}

void CBDB_SplitTest::ReadTestSet(vector<TestRec>&  test_set,
                                 const string&     file_name)
{
    cout << "Loading " << file_name << " ... " << endl;
    test_set.resize(0);
    CNcbiIfstream is(file_name.c_str());
    if (!is.good()) {
        return;
    }
    CStreamLineReader lr(is);
    for(++lr; !lr.AtEOF(); ++lr) {
        CTempString st = *lr;
        string count_str, size_str;
        string s = NStr::TruncateSpaces(st);
        NStr::SplitInTwo(s, " \t", count_str, size_str);
        TestRec rec;
        rec.count = NStr::StringToUInt(count_str,
           NStr::fAllowLeadingSpaces |
           NStr::fAllowTrailingSpaces |
           NStr::fConvErr_NoThrow);
        rec.blob_size = NStr::StringToUInt(size_str,
                NStr::fAllowLeadingSpaces |
                NStr::fAllowTrailingSpaces |
                NStr::fConvErr_NoThrow);
        if (rec.count && rec.blob_size)     {
            test_set.push_back(rec);
        } else {
            cout << "Blank record: " << st << endl;
        }
    } // for
    cout << "ok " << endl;

}



void CBDB_SplitTest::Init(void)
{
    SetDiagTrace(eDT_Enable);

    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    SetDiagPostFlag(eDPF_Trace);


    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_bdb_split",
                       "test BDB split storage");
    SetupArgDescriptions(d.release());
}



int CBDB_SplitTest::Run(void)
{
    //LoadTestSet("e:\\db_split\\db_split.txt");
    //return 0;

    cout << "Run BDB split storage test" << endl << endl;

    char* buf_small = new char[256];
    char* buf_large = new char[1024*1024];
    char* buf_read = new char[2*1024*1024];
    void* buf = buf_read;

    ::strcpy(buf_small, "test small 1");
    ::strcpy(buf_large, "test large 1");

    try
    {
        {{
        TBlobSplitStore split_store(new CBDB_BlobDeMux(1024*1024));

        split_store.Open("split", CBDB_RawFile::eCreate);

        split_store.Insert(2, buf_large, 1024 * 1024);
        split_store.Insert(1, buf_small, 256);

        ::strcpy(buf_small, "test small 2");
        ::strcpy(buf_large, "test large 2");

        split_store.UpdateInsert(3, buf_small, 256);
        split_store.UpdateInsert(4, buf_large, 1024 * 1024);

        split_store.FreeUnusedMem();

        TBlobSplitStore::TBitVector bv;
        split_store.GetIdVector(&bv);

        split_store.Save();
        }}

        {{
        TBlobSplitStore split_store(new CBDB_BlobDeMux(1024*1024));

        split_store.Open("split", CBDB_RawFile::eReadOnly);
        EBDB_ErrCode err;
        err =
            split_store.Fetch(1, &buf,
                              2*1024*1024, CBDB_RawFile::eReallocForbidden, 0);
        assert(err == eBDB_Ok);
        int res = strcmp(buf_read, "test small 1");
        assert(res == 0);
        err =
            split_store.Fetch(4, &buf,
                              2*1024*1024, CBDB_RawFile::eReallocForbidden, 0);
        assert(err == eBDB_Ok);
        res = strcmp(buf_read, "test large 2");
        assert(res == 0);

        CBDB_RawFile::TBuffer chbuf(10);
        err = split_store.ReadRealloc(4, chbuf);
        assert(err == eBDB_Ok);
        assert(chbuf.size() > 12);
        res = strcmp((const char*)&chbuf[0], "test large 2");
        assert(res == 0);


        }}

    }
    catch (CBDB_ErrnoException& ex)
    {
        cout << "Error! DBD errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        cout << "Error! DBD library exception:" << ex.what();
        return 1;
    }

    cout << endl;
    cout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBDB_SplitTest().AppMain(argc, argv);
}


