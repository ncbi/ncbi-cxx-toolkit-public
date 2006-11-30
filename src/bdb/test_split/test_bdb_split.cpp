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

#include <bdb/bdb_expt.hpp>
#include <bdb/bdb_types.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_map.hpp>
#include <bdb/bdb_blobcache.hpp>
#include <bdb/bdb_filedump.hpp>
#include <bdb/bdb_trans.hpp>
#include <bdb/bdb_query.hpp>
#include <bdb/bdb_util.hpp>
#include <bdb/bdb_split_blob.hpp>

#include <bdb/bdb_query_parser.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;




////////////////////////////////
// Test functions, classes, etc.
//




////////////////////////////////
// Test application
//

/// @internal
class CBDB_SplitTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


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
    cout << "Run BDB split storage test" << endl << endl;

    char* buf_small = new char[256];
    char* buf_large = new char[1024*1024];
    char* buf_read = new char[2*1024*1024];
    void* buf = buf_read;

    ::strcpy(buf_small, "test small 1");
    ::strcpy(buf_large, "test large 1");

    try
    {
        typedef CBDB_BlobSplitStore<bm::bvector<> > TBlobSplitStore;

        {{
        TBlobSplitStore split_store(new CBDB_BlobDeMux(1024*1024));

        split_store.Open("split", CBDB_RawFile::eCreate);

        split_store.Insert(2, buf_large, 1024 * 1024);
        split_store.Insert(1, buf_small, 256);

        ::strcpy(buf_small, "test small 2");
        ::strcpy(buf_large, "test large 2");

        split_store.UpdateInsert(3, buf_small, 256);
        split_store.UpdateInsert(4, buf_large, 1024 * 1024);

        split_store.Save();
        }}

        {{
        TBlobSplitStore split_store(new CBDB_BlobDeMux(1024*1024));

        split_store.Open("split", CBDB_RawFile::eReadOnly);
        EBDB_ErrCode err;
        err = 
            split_store.Fetch(1, &buf, 
                              2*1024*1024, CBDB_RawFile::eReallocForbidden);
        assert(err == eBDB_Ok);
        int res = strcmp(buf_read, "test small 1");
        assert(res == 0);
        err = 
            split_store.Fetch(4, &buf, 
                              2*1024*1024, CBDB_RawFile::eReallocForbidden);
        assert(err == eBDB_Ok);
        res = strcmp(buf_read, "test large 2");
        assert(res == 0);

        size_t buf_size;
        CBDB_RawFile::TBuffer chbuf(10);
        err = split_store.ReadRealloc(4, chbuf, &buf_size);
        assert(err == eBDB_Ok);
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
    return CBDB_SplitTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/11/30 12:42:09  dicuccio
 * Standardize buffer handling around CBDB_RawFile::TBuffer, a typedef for
 * vector<unsigned char>
 *
 * Revision 1.3  2006/05/02 20:14:53  kuznets
 * Fixed misprint
 *
 * Revision 1.2  2006/04/03 13:15:27  kuznets
 * +test for ReadRealloc()
 *
 * Revision 1.1  2006/03/29 16:59:27  kuznets
 * Moving bdb split test to a separate directory
 *
 * Revision 1.1  2006/03/28 16:51:55  kuznets
 * intial revision
 *
 * ===========================================================================
 */
