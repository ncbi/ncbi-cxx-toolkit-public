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


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbitime.hpp>
#include <stdio.h>

#include <bdb/bdb_expt.hpp>
#include <bdb/bdb_types.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_blob.hpp>


#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;



static const char* s_TestStrTempl = "Test str %i";
static const char* s_StrKeyTempl = "%i test key %i";


////////////////////////////////
// Test functions, classes, etc.
//




//////////////////////////////////////////////////////////////////
// 
// Structure implements simple database table with integer id primary key
//
//

struct TestDBF1 : public CBDB_IdFile
{
    CBDB_FieldInt4      idata;
    CBDB_FieldString    str;
    CBDB_FieldInt4      i2;
    
    TestDBF1()
    {
        BindData("idata", &idata);
        BindData("str", &str, 256);
        BindData("i2", &i2);
    }
};

// Utility function to check record consistency 
void ValidateRecord(const TestDBF1& dbf1, unsigned int id)
{
    char buf[256];
    sprintf(buf, s_TestStrTempl, id);

    int idata1 = dbf1.idata;
    unsigned int id_key = dbf1.IdKey;
    string s = (const char*)dbf1.str;
    int i21 = dbf1.i2;

    assert(s == string(buf));
    assert(id_key == id);
    assert(idata1 == 400 + id);
    assert(i21 == id + 3);
}



const char* s_TestFileName = "testbase.db";
const char* s_TestFileName2= "testbase2.db";

const unsigned int s_RecsInTable = 100;


//////////////////////////////////////////////////////////////////
// 
// Structure implements simple database table with case insensitive string 
// as a primary key
//
//

struct TestDBF2 : public CBDB_File
{
    CBDB_FieldStringCase  str_key;
    CBDB_FieldInt4        idata;
    CBDB_FieldString      str;
    CBDB_FieldInt4        i2;

    TestDBF2()
    {
        BindKey("strkey", &str_key, 256);
        BindData("idata", &idata);
        BindData("str", &str, 256);
        BindData("i2", &i2);
    }
};

// Utility function to check record consistency 
void ValidateRecord(const TestDBF2& dbf2, unsigned i)
{
    char buf[256];
    sprintf(buf, s_StrKeyTempl, i, i);
    string s = (const char*)dbf2.str_key;
    assert(s == string(buf));
    assert(dbf2.idata == 400+i);
    sprintf(buf, s_TestStrTempl, i);
    s = (const char*)dbf2.str;
    assert(s == string(buf));
    assert((int)dbf2.i2 == (i+3));
}



//////////////////////////////////////////////////////////////////
//
// 
// BDB types test
//
//

static void s_TEST_BDB_Types(void)
{
    cout << "======== BDB types test." << endl;

    TestDBF1  dbf1;
    cout << "======== BDB types test ok." << endl;
}


//////////////////////////////////////////////////////////////////
//
// 
// DBD Id table fill test
//
//


static void s_TEST_BDB_IdTable_Fill(void)
{
    cout << "======== Id table filling test." << endl;

    TestDBF1  dbf1;

    dbf1.Open(s_TestFileName, CBDB_File::eCreate);
    assert(dbf1.idata.IsNull());

    // Fill the table

    unsigned i;
    for (i = 1; i < s_RecsInTable; ++i) {
        char buf[256];
        sprintf(buf, s_TestStrTempl, i);
        dbf1.IdKey = i;
        dbf1.idata = 400+i;
        dbf1.str = buf;
        dbf1.i2.Set(i+3);
        int i2 = dbf1.i2;
        assert (i2 == i+3);

        assert(!dbf1.idata.IsNull());

        EBDB_ErrCode err = dbf1.Insert();
        assert(err == eBDB_Ok);
        assert(dbf1.idata.IsNull());
    }

    cout << "Table " << s_TestFileName 
         << " loaded ok. Checking consistency." 
         << endl;

    // Read the table check if all records are in place

    dbf1.Reopen(CBDB_File::eReadOnly);

    for (i = 1; i < s_RecsInTable; ++i) {
        dbf1.IdKey = i;
        EBDB_ErrCode err = dbf1.Fetch();
        assert (err == eBDB_Ok);

        ValidateRecord(dbf1, i);

    } // for

    cout << "======== Id table filling test ok." << endl;


}


//////////////////////////////////////////////////////////////////
//
// 
// DBD Id table cursor test
//
//

static void s_TEST_BDB_IdTable_Cursor(void)
{
    cout << "======== Id table cursor test." << endl;

    TestDBF1  dbf1;

    dbf1.Open(s_TestFileName, CBDB_File::eReadOnly);

    // Search point test

    unsigned search_id = s_RecsInTable / 2;
    if (search_id == 0) {
        cout << "Too few records to test cursor operation." << endl;
        assert(0);
    }

    // -----------------------------------
    {{
    cout << "Testing Eq cursor" << endl;

    CBDB_FileCursor cur1(dbf1);
    cur1.SetCondition(CBDB_FileCursor::eEQ);

    cur1.From << search_id;
    
    unsigned recs_fetched = 0;

    while (cur1.Fetch() == eBDB_Ok) {
        unsigned fetched_id = dbf1.IdKey;
        assert(search_id == fetched_id);

        ValidateRecord(dbf1, fetched_id);

        ++recs_fetched;
    }

    assert(recs_fetched == 1);

    }}


    // -----------------------------------

    {{

    cout << "Testing reusable Eq cursor" << endl;
    search_id = s_RecsInTable / 2;

    CBDB_FileCursor cur1(dbf1);
    cur1.SetCondition(CBDB_FileCursor::eEQ);

    for ( ;search_id > 0; --search_id) {
        cur1.From << search_id;
    
        unsigned recs_fetched = 0;

        while (cur1.Fetch() == eBDB_Ok) {
            unsigned fetched_id = dbf1.IdKey;
            assert(search_id == fetched_id);

            ValidateRecord(dbf1, fetched_id);
            ++recs_fetched;
        }

        assert(recs_fetched == 1);

    } // for

    }}

    // -----------------------------------

    {{

    cout << "Testing forward GE - LE cursor" << endl;
    search_id = s_RecsInTable / 2;

    CBDB_FileCursor cur1(dbf1);
    cur1.SetCondition(CBDB_FileCursor::eGE, CBDB_FileCursor::eLE);

    cur1.From << search_id;
    cur1.To   << s_RecsInTable - 1;

    unsigned recs_fetched = 0;

    for (unsigned i = search_id; cur1.Fetch() == eBDB_Ok; ++i) {
        unsigned fetched_id = dbf1.IdKey;
        assert(i == fetched_id);

        ValidateRecord(dbf1, fetched_id);
        ++recs_fetched;
    }

    assert(recs_fetched == (s_RecsInTable) - search_id);

    }}

    // -----------------------------------

    {{

    cout << "Testing forward GE - LT cursor" << endl;
    search_id = s_RecsInTable / 2;

    CBDB_FileCursor cur1(dbf1);
    cur1.SetCondition(CBDB_FileCursor::eGE, CBDB_FileCursor::eLT);

    cur1.From << search_id;
    cur1.To   << s_RecsInTable - 1;

    unsigned recs_fetched = 0;

    for (unsigned i = search_id; cur1.Fetch() == eBDB_Ok; ++i) {
        unsigned fetched_id = dbf1.IdKey;
        assert(i == fetched_id);

        ValidateRecord(dbf1, fetched_id);
        ++recs_fetched;
    }

    assert(recs_fetched == (s_RecsInTable-1) - search_id);

    }}

    // -----------------------------------

    {{

    cout << "Testing forward GT - LT cursor" << endl;
    search_id = s_RecsInTable / 2;

    CBDB_FileCursor cur1(dbf1);
    cur1.SetCondition(CBDB_FileCursor::eGT, CBDB_FileCursor::eLT);

    cur1.From << search_id;
    cur1.To   << s_RecsInTable - 1;

    unsigned recs_fetched = 0;

    for (unsigned i = search_id+1; cur1.Fetch() == eBDB_Ok; ++i) {
        unsigned fetched_id = dbf1.IdKey;
        assert(i == fetched_id);

        ValidateRecord(dbf1, fetched_id);
        ++recs_fetched;
    }

    assert(recs_fetched == (s_RecsInTable-2) - search_id);

    }}

    // -----------------------------------

    {{

    cout << "Testing backward LE - GE cursor" << endl;
    search_id = s_RecsInTable / 2;

    CBDB_FileCursor cur1(dbf1);
    cur1.SetCondition(CBDB_FileCursor::eLE, CBDB_FileCursor::eGE);

    cur1.From << search_id;
    cur1.To   << 1;

    unsigned recs_fetched = 0;

    for (unsigned i = search_id; cur1.Fetch() == eBDB_Ok; --i) {
        unsigned fetched_id = dbf1.IdKey;
        assert(i == fetched_id);

        ValidateRecord(dbf1, fetched_id);
        ++recs_fetched;
    }

    assert(recs_fetched == search_id);

    }}

    // -----------------------------------

    {{

    cout << "Testing backward LE - GT cursor" << endl;
    search_id = s_RecsInTable / 2;

    CBDB_FileCursor cur1(dbf1);
    cur1.SetCondition(CBDB_FileCursor::eLE, CBDB_FileCursor::eGT);

    cur1.From << search_id;
    cur1.To   << 1;

    unsigned recs_fetched = 0;

    for (unsigned i = search_id; cur1.Fetch() == eBDB_Ok; --i) {
        unsigned fetched_id = dbf1.IdKey;
        assert(i == fetched_id);

        ValidateRecord(dbf1, fetched_id);
        ++recs_fetched;
    }

    assert(recs_fetched == search_id - 1);

    }}

    // -----------------------------------

    {{

    cout << "Testing backward LT - GT cursor" << endl;
    search_id = s_RecsInTable / 2;

    CBDB_FileCursor cur1(dbf1);
    cur1.SetCondition(CBDB_FileCursor::eLT, CBDB_FileCursor::eGT);

    cur1.From << search_id;
    cur1.To   << 1;

    unsigned recs_fetched = 0;

    for (unsigned i = search_id-1; cur1.Fetch() == eBDB_Ok; --i) {
        unsigned fetched_id = dbf1.IdKey;
        assert(i == fetched_id);

        ValidateRecord(dbf1, fetched_id);
        ++recs_fetched;
    }

    assert(recs_fetched == search_id - 2);

    }}

    // -----------------------------------

    // TO DO: Test illegitimate cases, suppoed to return empty set


    dbf1.Close();

    cout << "======== Id table cursor test ok." << endl;    
}

//////////////////////////////////////////////////////////////////
//
// 
// LOB test
//
//

static void s_TEST_BDB_LOB_File(void)
{
    cout << "======== LOB file test." << endl;    

    CBDB_LobFile lob;
    lob.Open("lobtest.db", "lob", CBDB_LobFile::eCreate);

    const char* test_data = "This is a LOB test data";
    unsigned lob_len = ::strlen(test_data)+1;

    EBDB_ErrCode ret = lob.Insert(1, test_data, lob_len);
    assert(ret == eBDB_Ok);

    cout << "Testing two-phase read." << endl;
    lob.Reopen(CBDB_LobFile::eReadOnly);
    
    ret = lob.Fetch(1);
    assert(ret == eBDB_Ok);
    unsigned len1 = lob.LobSize();
    assert(len1 == lob_len);

    char buf[256] = {0,};
    ret = lob.GetData(buf, sizeof(buf));
    assert(ret == eBDB_Ok);

    if (strcmp(buf, test_data) != 0) {
        cout << "LOB content comparison error!" << endl;
        cout << "LobData:" << buf << endl;
        assert(0);
    }

    buf[0] = 0;

    // Reallocate read cannot be used in some cases.
    // Under Windows if BerkeleyDB libdb*.lib has been build with runtime 
    // different from application runtime reallocate will crash the application
    // In this test case test_bdb is build with "Multithreaded Debug DLL" runtime
    // and Berkeley DB with uses "Mutithereaded DLL". Reallocation is not possible
    // in this configuration.
#ifndef _DEBUG
    cout << "Testing reallocate read." << endl;

    unsigned buf_size = 5;         // insufficient memory to get the BLOB
    void* buf2=::malloc(buf_size); 

    // Here we create a check buf if reallocate fails for some reasons
    // chances are it will result in checkblock signature failure
    unsigned check_buf_size = 2000;
    unsigned* check_buf = new unsigned[check_buf_size];
    unsigned i;
    for (i = 0; i < buf_size; ++i) {
        check_buf[i] = i;
    }
    

    ret = lob.Fetch(1, &buf2, 5, CBDB_LobFile::eReallocAllowed);
    assert(ret == eBDB_Ok);

    unsigned new_buf_size = lob.LobSize();
    assert(buf_size < new_buf_size);

    if (strcmp((char*)buf2, test_data) != 0) {
        cout << "LOB content comparison error!" << endl;
        cout << "LobData:" << buf2 << endl;
        assert(0);
    }
    
    ::free(buf2);   // should not crash here

    // Check the signature
    for (i = 0; i < buf_size; ++i) {
        assert(check_buf[i] == i);
    }
#endif
    cout << "======== LOB file test ok." << endl;
}


//////////////////////////////////////////////////////////////////
//
// 
// Id table record delete test
//
//

static void s_TEST_BDB_IdTable_DeleteInsert(void)
{
    cout << "======== BDB Id Table Delete/Insert test." << endl;

    TestDBF1  dbf1;

    dbf1.Open(s_TestFileName, CBDB_File::eReadWrite);
    
    
    unsigned int search_id = s_RecsInTable / 2;
    dbf1.IdKey = search_id;
    
    EBDB_ErrCode err = dbf1.Delete();
    assert(err == eBDB_Ok);

    dbf1.IdKey = search_id;
    err = dbf1.Fetch();
    assert(err == eBDB_NotFound);

    dbf1.IdKey = search_id;
    dbf1.idata = 800;
    dbf1.str = "123456789";
    dbf1.i2.Set(300);
    dbf1.Insert();

    dbf1.IdKey = search_id;
    err = dbf1.Fetch();
    assert(err == eBDB_Ok);


    dbf1.Close();


    cout << "======== BDB Id Table Delete/Insert test ok." << endl;
}


//////////////////////////////////////////////////////////////////
//
// 
// BDB string key table fill test
//
//



static void s_TEST_BDB_StrTable_Fill(void)
{
    cout << "======== StrKey table filling test." << endl;

    TestDBF2  dbf;

    dbf.Open(s_TestFileName2, CBDB_File::eCreate);

    // Fill the table

    unsigned i;
    for (i = 1; i < s_RecsInTable; ++i) {
        char buf[256];
        sprintf(buf, s_StrKeyTempl, i, i);
        dbf.str_key = buf;
        dbf.idata = 400+i;
        sprintf(buf, s_TestStrTempl, i);
        dbf.str = buf;
        dbf.i2.Set(i+3);
        int i2 = dbf.i2;
        assert (i2 == i+3);
        EBDB_ErrCode err = dbf.Insert();
        assert(err == eBDB_Ok);
    }

    cout << "Table " << s_TestFileName2 
         << " loaded ok. Checking consistency." 
         << endl;

    // Read the table check if all records are in place

    dbf.Reopen(CBDB_File::eReadOnly);

    for (i = 1; i < s_RecsInTable; ++i) {
        char buf[256];
        sprintf(buf, s_StrKeyTempl, i, i);
        dbf.str_key = buf;
        EBDB_ErrCode err = dbf.Fetch();
        assert (err == eBDB_Ok);

        ValidateRecord(dbf, i);

    } // for

    cout << "======== StrKey table filling test ok." << endl;


}





////////////////////////////////
// Test application
//

class CBDB_Test : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CBDB_Test::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_bdb",
                       "test BDB library");
    SetupArgDescriptions(d.release());
}


int CBDB_Test::Run(void)
{
    cout << "Run BDB test" << endl << endl;

    try
    {
        s_TEST_BDB_Types();

        s_TEST_BDB_IdTable_Fill();

        s_TEST_BDB_IdTable_Cursor();

        s_TEST_BDB_IdTable_DeleteInsert();

        s_TEST_BDB_LOB_File();

        s_TEST_BDB_StrTable_Fill();

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

#include <vector>

using namespace std;


template<class T> class MyFunc
{
public:
    void operator() (T& val)
    {
        cout << *val << ";";
    }
};




///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBDB_Test().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/04/29 19:04:13  kuznets
 * +Test makefiles
 *
 * Revision 1.1  2003/04/28 18:12:52  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
