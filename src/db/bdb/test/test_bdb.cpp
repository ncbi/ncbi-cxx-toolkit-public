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
#include <db/bdb/bdb_ext_blob.hpp>
#include <db/bdb/bdb_query_parser.hpp>

#include <util/compress/zlib.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


namespace {
    const char* s_TestStrTempl = "Test str %i";
    const char* s_StrKeyTempl = "%i test key %i";
}


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
    CBDB_FieldInt2      ishort;

    TestDBF1()
    {
        BindData("idata", &idata);
        BindData("str", &str, 256);
        BindData("i2", &i2);
        BindData("ishort", &ishort);
    }
};

struct TestDBF1L : public CBDB_IdFile
{
    CBDB_FieldInt4      idata;
    CBDB_FieldLString   str;
    CBDB_FieldInt4      i2;
    CBDB_FieldInt2      ishort;

    TestDBF1L()
    {
        BindData("idata", &idata);
        BindData("str", &str, 256);
        BindData("i2", &i2);
        BindData("ishort", &ishort);

        SetLegacyStringsCheck(true);
    }
};


// Utility function to check record consistency
void ValidateRecord(const TestDBF1 &  dbf1, unsigned int  id)
{
    char buf[256];
    sprintf(buf, s_TestStrTempl, id);

    int idata1 = dbf1.idata;
    unsigned int id_key = dbf1.IdKey;
    string s;
    s = dbf1.str;
    int i21 = dbf1.i2;

    assert(s == string(buf));
    assert(id_key == id);
    assert(idata1 == (int)(400 + id));
    assert(i21 == (int)(id + 3));
}

void ValidateRecord(const TestDBF1L &  dbf1, unsigned int  id)
{
    char buf[256];
    sprintf(buf, s_TestStrTempl, id);

    int idata1 = dbf1.idata;
    unsigned int id_key = dbf1.IdKey;
    string s;
    s = dbf1.str;
    int i21 = dbf1.i2;


    assert(s == string(buf));
    assert(id_key == id);
    assert(idata1 == (int)(400 + id));
    assert(i21 == (int)(id + 3));
}


const char* s_TestFileName = "testbase.db";
const char* s_TestFileName2= "testbase2.db";
const char* s_TestFileName3= "testbase3.db";

const char* s_db_map1 = "i2s.db";
const char* s_db_map2 = "ii2s.db";

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

/// @internal
struct TestQueue : public CBDB_File
{
    CBDB_FieldInt4        key;

    CBDB_FieldInt4        v1;
    CBDB_FieldInt4        v2;

    TestQueue()
        : CBDB_File(CBDB_RawFile::eDuplicatesDisable, CBDB_RawFile::eQueue)
    {
        DisableNull();
        BindKey("key", &key);

        BindData("v1", &v1);
        BindData("v2", &v2);
    }
};

/// @internal
struct TestHash : public CBDB_File
{
    CBDB_FieldString      key;
    CBDB_FieldString      value;

    TestHash()
        : CBDB_File(CBDB_RawFile::eDuplicatesDisable, CBDB_RawFile::eHash)
    {
        DisableNull();
        BindKey("key", &key, 256);
        BindData("value", &value);
    }
};


/// @internal
struct TestQueueBlob : public CBDB_BLobFile
{
    CBDB_FieldInt4        key;


    TestQueueBlob()
        : CBDB_BLobFile(CBDB_RawFile::eDuplicatesDisable,
                        CBDB_RawFile::eQueue)
    {
        BindKey("key", &key);
    }
};


/// Utility function to check record consistency
/// @internal
void ValidateRecord(const TestDBF2& dbf2, unsigned i)
{
    char buf[256];
    sprintf(buf, s_StrKeyTempl, i, i);
    string s = (const char*)dbf2.str_key;
    assert(s == string(buf));
    assert(dbf2.idata == (int)(400+i));
    sprintf(buf, s_TestStrTempl, i);
    s = (const char*)dbf2.str;
    assert(s == string(buf));
    assert((int)dbf2.i2 == (int)(i+3));
}


struct TestDBF3 : public CBDB_File
{
    CBDB_FieldInt4        ikey;
    CBDB_FieldInt4        idata;

    TestDBF3()
    {
        BindKey("ikey",   &ikey);
        BindData("idata", &idata);
    }
};


//////////////////////////////////////////////////////////////////
//
//
// BDB transaction test
//
//

static
void s_TEST_BDB_Transaction(void)
{
    cout << "======== Transactions test." << endl;

    CBDB_Env env;
    env.OpenWithTrans(".", CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);
    env.OpenErrFile("err_test.txt");
    env.CleanLog();

    TestDBF3  dbf3;

    dbf3.SetEnv(env);

    dbf3.Open("trans_test.db", CBDB_File::eReadWriteCreate);
    cout << dbf3.CountRecs() << endl;

    CBDB_Transaction trans(env);

    dbf3.SetTransaction(&trans);

    dbf3.ikey = 10;
    dbf3.idata = 11;
    dbf3.Insert();

    dbf3.ikey = 12;
    dbf3.idata = 13;
    dbf3.Insert();

//    trans.Commit();

    dbf3.ikey = 10;
    EBDB_ErrCode ret = dbf3.Fetch();
    assert (ret == eBDB_Ok);

    int idata = dbf3.idata;
    assert(idata == 11);

    dbf3.ikey = 10;
    dbf3.idata = 20;
    dbf3.UpdateInsert();

    ret = dbf3.Fetch();
    assert (ret == eBDB_Ok);

    idata = dbf3.idata;
    assert(idata == 20);

    dbf3.ikey = 12;
    ret = dbf3.Fetch();
    assert (ret == eBDB_Ok);

    idata = dbf3.idata;
    assert(idata == 13);

    trans.Abort();

    dbf3.SetTransaction(0);
    dbf3.ikey = 10;
    ret = dbf3.Fetch();
    assert (ret == eBDB_NotFound);

//    idata = dbf3.idata;
//    assert(idata == 11);

    env.TransactionCheckpoint();

    env.CleanLog();

    cout << "======== Transactions test ok." << endl;
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
// BDB Id table fill test
//
//

static
void s_IdTableFill(TestDBF1& dbf,
                   unsigned int records,
                   int seed = 0)
{
    for (unsigned int i = 1; i < records; ++i) {
        char buf[256];
        sprintf(buf, s_TestStrTempl, i);
        dbf.IdKey = i + seed;
        dbf.idata = 400+i;
        dbf.str = buf;
        dbf.i2.Set((int)(i+3));
        int i2 = dbf.i2;
        assert (i2 == (int)(i+3));

        assert(!dbf.idata.IsNull());

        EBDB_ErrCode err = dbf.Insert();
        assert(err == eBDB_Ok);
        assert(dbf.idata.IsNull());
    }

}


static void s_TEST_BDB_IdTable_Fill(void)
{
    cout << "======== Id table filling test." << endl;

    CBDB_Env env;
    env.OpenWithLocks(".");
    env.OpenErrFile("err_test.txt");

    TestDBF1  dbf1;

    dbf1.SetEnv(env);

    dbf1.SetPageSize(32 * 1024);
    dbf1.SetCacheSize(5 * (1024 * 1024));

    dbf1.Open(s_TestFileName, CBDB_File::eCreate);
    assert(dbf1.idata.IsNull());

    // Fill the table

    unsigned i;

    s_IdTableFill(dbf1, s_RecsInTable);

    // Trying to put duplicate record

    dbf1.IdKey = 1;
    dbf1.idata = 400 + 1;
    dbf1.str = "test";
    EBDB_ErrCode err = dbf1.Insert();
    assert(err == eBDB_KeyDup);

    cout << "Table " << s_TestFileName
         << " loaded ok. Checking consistency."
         << endl;

    // Read the table check if all records are in place

    dbf1.Reopen(CBDB_File::eReadOnly);

    for (i = 1; i < s_RecsInTable; ++i) {
        dbf1.IdKey = i;
        EBDB_ErrCode ret = dbf1.Fetch();
        assert (ret == eBDB_Ok);

        ValidateRecord(dbf1, i);

    } // for

    {{
    TestDBF1  dbf11;
    dbf11.Attach(dbf1);

    for (i = 1; i < s_RecsInTable; ++i) {
        dbf11.IdKey = i;
        EBDB_ErrCode ret = dbf11.Fetch();
        assert (ret == eBDB_Ok);

        ValidateRecord(dbf11, i);

        // Checking that attached buffer doesn't change status of the main one
        dbf1.IdKey = 1;
        ret = dbf1.Fetch();
        assert (ret == eBDB_Ok);
        ValidateRecord(dbf1, 1);

        ValidateRecord(dbf11, i);

    } // for

    }}


    {{
    TestDBF1L  dbf11;
    dbf11.Open(s_TestFileName, CBDB_File::eReadWrite);

    #if 0
    // NB: this check does not pass.
    // The DB table is prepared without legacy string check but it is checked
    // via another DB descibing class with the legacy string check.
    // The very first integer field after a string field is not read properly.
    // It seems that the read misses the integer field shift on a fixed number
    // of bytes (2 or 4).
    for (i = 1; i < s_RecsInTable; ++i) {
        dbf11.IdKey = i;
        EBDB_ErrCode ret = dbf11.Fetch();
        assert (ret == eBDB_Ok);

        ValidateRecord(dbf11, i);

    } // for
    #endif

    string s = "123";
    s.append(1, (char)0);
    s.append("123");

    cout << "(" << s << ")" << s.length() << endl;

    dbf11.IdKey = 1000;
    dbf11.idata = 1000;
    dbf11.str = s;

    dbf11.Insert();

    dbf11.IdKey = 1000;
    EBDB_ErrCode ret = dbf11.Fetch();
    assert (ret == eBDB_Ok);

    string s2 = dbf11.str;
    string s3 = dbf11.str.GetString();
    size_t l2 = s2.length();
    size_t l3 = s3.length();
    assert(s3 == s);
    assert(s2 == s);
    assert(l2 == l3);

    // Dumping the database to screen

    CBDB_FileDumper dump;
    dump.Dump(cout, dbf11);

    }}

    cout << "======== Id table filling test ok." << endl;
}



static void s_TEST_BDB_IdTable_FillStress(void)
{
    cout << "======== Id table filling stress test." << endl;

    CBDB_Env env;
    {{
//    env.SetCacheSize(100 * (1024 * 1024));
    env.OpenErrFile("err.txt");

    env.OpenWithTrans(".", CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);
    cout << "Removing LOG files" << endl;
    env.CleanLog();
    cout << "Removing LOG files. ok." << endl;

    env.SetLockTimeout(30 * 1000000);
    env.SetTransactionTimeout(30 * 1000000);

    TestDBF1  dbf1;
    dbf1.SetEnv(env);
    dbf1.Open(s_TestFileName, CBDB_File::eCreate);
    assert(dbf1.idata.IsNull());

    unsigned i;
    //const unsigned int recs = 1000000;
    const unsigned int recs = 10000;

    for (i = 0; i < 10; ++i) {
        cout << i << " " << flush;
       CBDB_Transaction trans(env);
       dbf1.SetTransaction(&trans);

       s_IdTableFill(dbf1, recs, i * 1000000);

       trans.Commit();
       env.TransactionCheckpoint();
    }


    cout << endl << "Removing LOG files." << endl;
    env.CleanLog();
    cout << "Removing LOG files. ok." << endl;

    cout << "Table loaded..." << endl;

    {{
    int idata;

    CBDB_FileCursor cur(dbf1, CBDB_FileCursor::eReadModifyUpdate);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << 1 + (1 * 1000000);
    EBDB_ErrCode ret = cur.FetchFirst();
    assert (ret == eBDB_Ok);
    idata = dbf1.idata;
    NcbiCout << idata << NcbiEndl;

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << 2 + (1 * 1000000);
    ret = cur.FetchFirst();
    assert (ret == eBDB_Ok);


    idata = dbf1.idata;
    NcbiCout << idata << NcbiEndl;


    }}

    {{

    CBDB_FileCursor cur(dbf1, CBDB_FileCursor::eReadModifyUpdate);
    cur.Close();
    CBDB_Transaction trans(env);

    for (i = 1; i < recs; ++i) {
        cur.ReOpen(&trans);
        bool b = cur.IsOpen();
        assert(b);
        cur.SetCondition(CBDB_FileCursor::eEQ);

        cur.From << i+ (1 * 1000000);
        EBDB_ErrCode ret = cur.FetchFirst();
        assert (ret == eBDB_Ok);
        cur.Close();

    } // for

    }}


    // Read the table check if all records are in place

    dbf1.Reopen(CBDB_File::eReadOnly);

    for (i = 1; i < recs; ++i) {
        dbf1.IdKey = i + (1 * 1000000);
        EBDB_ErrCode ret = dbf1.Fetch();
        assert (ret == eBDB_Ok);

    } // for

    cout << "Check 1 complete" << endl;

    dbf1.Reopen(CBDB_File::eReadOnly);

    for (i = 1; i < recs; ++i) {

        {{
            CBDB_FileCursor cur(dbf1);
            cur.SetCondition(CBDB_FileCursor::eEQ);

            cur.From << i+ (1 * 1000000);
            EBDB_ErrCode ret = cur.FetchFirst();
            assert (ret == eBDB_Ok);
        }}


    } // for

    }}

    env.CheckRemove();

    cout << "======== Id table stress filling test ok." << endl;


}


class CTestScanner : public CBDB_FileScanner
{
public:

    CTestScanner(CBDB_File& db_file)
    : CBDB_FileScanner(db_file)
    {}

    virtual EScanAction OnRecordFound()
    {
        return eContinue;
    }

protected:

};


static void s_TEST_BDB_Query(void)
{
    bool bres;

    cout << "======== Query test." << endl;

    TestDBF1  dbf1;
    dbf1.Open(s_TestFileName, CBDB_File::eReadOnly);

    dbf1.IdKey = 1;
    dbf1.Fetch();

    CTestScanner  scanner(dbf1);

    {{
        const char* ch = " test";
        CBDB_Query    query;
        BDB_PrintQueryTree(cout, query);

        BDB_ParseQuery(ch, &query);
        BDB_PrintQueryTree(cout, query);
    }}

    {{
        const char* ch = "  '2' And ((name3 OR 1) & name3)";
        CBDB_Query    query;
        BDB_ParseQuery(ch, &query);
        BDB_PrintQueryTree(cout, query);
    }}

    CBDB_Query    query;
    // Testing the query evaluation logic
    //

    // "1" == "2" => false
    {{
    CBDB_Query::TQueryClause* eq_node =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "1", "2");
    query.SetQueryClause(eq_node);

    bres = scanner.StaticEvaluate(query);

    assert(!bres);
    BDB_PrintQueryTree(cout, query);
    }}

    {{
    const char* ch = "'1' = 2";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(!bres);
    }}

    {{
    const char* ch = "not '1' = 2";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    cout << endl;
    BDB_PrintQueryTree(cout, query);
    assert(bres);
    }}

    {{
    const char* ch = "'1' != 2";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    {{
    const char* ch = "'1' <> 2";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    {{
    const char* ch = "'1' < 2";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    {{
    const char* ch = "'1' <= 1";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    {{
    const char* ch = "'1' >= 1";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    {{
    const char* ch = "'2' > 1";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    // "2" == "2" => true
    {{
    CBDB_Query::TQueryClause* eq_node =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "2", "2");
    query.SetQueryClause(eq_node);

    bres = scanner.StaticEvaluate(query);

    assert(bres);
    }}


    {{
    const char* ch = "2 = 2";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}


    // ("1" == "1" AND "2" == "2") => true
    {{
    CBDB_Query::TQueryClause* eq_node1 =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "2", "2");
    CBDB_Query::TQueryClause* eq_node2 =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "1", "1");

    CBDB_Query::TQueryClause* and_node =
        new CBDB_Query::TQueryClause(CBDB_QueryNode::eAnd);
    and_node->AddNode(eq_node1);
    and_node->AddNode(eq_node2);

    query.SetQueryClause(and_node);

    bres = scanner.StaticEvaluate(query);

    assert(bres);
    }}

    {{
    const char* ch = "(1=1 AND 2=2)";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    // ("1" == "1" AND "2" == "0") => false
    {{
    CBDB_Query::TQueryClause* eq_node1 =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "2", "0");
    CBDB_Query::TQueryClause* eq_node2 =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "1", "1");

    CBDB_Query::TQueryClause* and_node =
        new CBDB_Query::TQueryClause(CBDB_QueryNode::eAnd);
    and_node->AddNode(eq_node1);
    and_node->AddNode(eq_node2);

    query.SetQueryClause(and_node);

    bres = scanner.StaticEvaluate(query);

    assert(!bres);

    BDB_PrintQueryTree(cout, query);
    }}

    {{
    const char* ch = "('1'='1' AND '2'='0')";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(!bres);
    }}

    // ("1" == "1" OR "2" == "0") => true
    {{
    CBDB_Query::TQueryClause* eq_node1 =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "2", "0");
    CBDB_Query::TQueryClause* eq_node2 =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "1", "1");

    CBDB_Query::TQueryClause* and_node =
        new CBDB_Query::TQueryClause(CBDB_QueryNode::eOr);
    and_node->AddNode(eq_node1);
    and_node->AddNode(eq_node2);

    query.SetQueryClause(and_node);

    bres = scanner.StaticEvaluate(query);

    assert(bres);
    }}

    {{
    const char* ch = "('1'='1' OR '2'='0')";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}


    // Testing the query evaluation involving data fields
    //

    // (id = 1) =>true
    {{
    CBDB_Query::TQueryClause* eq_node =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "id", "1");
    query.SetQueryClause(eq_node);

    bres = scanner.StaticEvaluate(query);

    assert(bres);
    }}

    {{
    const char* ch = "id = '1'";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}


    // ("id" == "1" AND "idata" == "401") => true
    {{
    CBDB_Query::TQueryClause* eq_node1 =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "id", "1");
    CBDB_Query::TQueryClause* eq_node2 =
          CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, "idata", "401");

    CBDB_Query::TQueryClause* and_node =
        new CBDB_Query::TQueryClause(CBDB_QueryNode::eAnd);
    and_node->AddNode(eq_node1);
    and_node->AddNode(eq_node2);

    query.SetQueryClause(and_node);

    bres = scanner.StaticEvaluate(query);

    assert(bres);
    BDB_PrintQueryTree(cout, query);
    }}

    {{
    const char* ch = "id = '1' AND idata = '401'";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

#if 0
    {{
    const char* ch = "test";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    {{
    const char* ch = "yyy";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(!bres);
    }}
#endif


    {{
    const char* ch = "id = '1' AND test";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    assert(bres);
    }}

    {{
    const char* ch = "NOT test AND test";
    BDB_ParseQuery(ch, &query);
    bres = scanner.StaticEvaluate(query);
    BDB_PrintQueryTree(cout, query);
    assert(!bres);
    }}

    cout << "======== Query test ok." << endl;
}


static void s_TEST_BDB_IdTable_Fill2(void)
{
    cout << "======== Skipping the Id table filling test 2. "
            "It is broken" << endl;

#if 0
    cout << "======== Id table filling test 2." << endl;

    CBDB_Env env;
    env.OpenWithLocks("");

    TestDBF1L  dbf1;

    dbf1.SetEnv(env);

    dbf1.SetPageSize(32 * 1024);
    dbf1.SetCacheSize(5 * (1024 * 1024));

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
        assert (i2 == (int)(i+3));

        assert(!dbf1.idata.IsNull());

        EBDB_ErrCode err = dbf1.Insert();
        assert(err == eBDB_Ok);
        assert(dbf1.idata.IsNull());
    }

    // Trying to put duplicate record

    dbf1.IdKey = 1;
    dbf1.idata = 400 + 1;
    dbf1.str = "test";
    EBDB_ErrCode err = dbf1.Insert();
    assert(err == eBDB_KeyDup);

    cout << "Table " << s_TestFileName
         << " loaded ok. Checking consistency."
         << endl;

    // Read the table check if all records are in place
    dbf1.Reopen(CBDB_File::eReadOnly);

    for (i = 1; i < s_RecsInTable; ++i) {
        dbf1.IdKey = i;
        EBDB_ErrCode ret = dbf1.Fetch();
        assert (ret == eBDB_Ok);

        ValidateRecord(dbf1, i);

    } // for


    {{
    TestDBF1  dbf11;
    dbf11.Attach(dbf1);

    for (i = 1; i < s_RecsInTable; ++i) {
        dbf11.IdKey = i;
        EBDB_ErrCode ret = dbf11.Fetch();
        assert (ret == eBDB_Ok);

        ValidateRecord(dbf11, i);

        // Checking that attached buffer doesn't change status of the main one
        dbf1.IdKey = 1;
        ret = dbf1.Fetch();
        assert (ret == eBDB_Ok);
        ValidateRecord(dbf1, 1);

        ValidateRecord(dbf11, i);

    } // for

    }}


    {{
    TestDBF1L  dbf11;
    dbf11.Open(s_TestFileName, CBDB_File::eReadWrite);

    for (i = 1; i < s_RecsInTable; ++i) {
        dbf11.IdKey = i;
        EBDB_ErrCode ret = dbf11.Fetch();
        assert (ret == eBDB_Ok);

        ValidateRecord(dbf11, i);

    } // for

    // Dumping the database to screen

    CBDB_FileDumper dump;
    dump.Dump(cout, dbf11);
    }}
    cout << "======== Id table filling test 2 ok." << endl;
#endif
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


    {{
    CBDB_LobFile lob;

    lob.SetCacheSize(25 * 1024 * 1024);
    lob.Open("big_lobtest_.db", CBDB_LobFile::eCreate);

    size_t bsize = 50 * 1024 * 1024;
    char* buf = new char[bsize];

    int test_size = 50;

    cout << "Big blob write timing test" << endl;
    for (int i = 0; i < test_size; ++i) {

        size_t lob_len = (i+1) * 1024 * 1024;
        cout << "Writing " << lob_len / (1024 * 1024) << "MB" << endl;
        time_t start = time(0);
        EBDB_ErrCode ret = lob.Insert(i+1, buf, lob_len);
        assert(ret == eBDB_Ok);
        time_t finish = time(0);
        cout << "Time=" << finish - start << "sec." << endl;
    }

    cout << "Big blob write timing test OK." << endl;

    cout << "Big blob file timing test." << endl;

    for (int i = 0; i < test_size; ++i) {

        size_t lob_len = (i+1) * 1024 * 1024;
        cout << "Writing " << lob_len / (1024 * 1024) << "MB" << endl;
        time_t start = time(0);

        ofstream fs("test_file.bin");
        fs.write(buf, lob_len);

        time_t finish = time(0);
        cout << "Time=" << finish - start << "sec." << endl;
    }
    cout << "Big blob file timing test OK." << endl;
    delete buf;


    }}


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
// LOB test
//
//

struct BLobTestDBF1 : public CBDB_BLobFile
{
    CBDB_FieldInt4        i1;
    CBDB_FieldInt4        i2;

    BLobTestDBF1()
    {
        BindKey("i1", &i1);
        BindKey("i2", &i2);
    }
};


static void s_TEST_BDB_BLOB_File(void)
{
    cout << "======== BLob file test." << endl;

    BLobTestDBF1 blob;
    blob.Open("blobtest.db", "blob", CBDB_LobFile::eCreate);

    const char* test_data = "This is a BLOB test data";
    unsigned lob_len = ::strlen(test_data)+1;

    blob.i1 = 1;
    blob.i2 = 2;

    EBDB_ErrCode ret = blob.Insert(test_data, lob_len);
    assert(ret == eBDB_Ok);

    blob.i1 = 1;
    blob.i2 = 3;


    const char* test_data2
        = "This is a BLOB test data.TEst test test test....BDB. test.";
    lob_len = ::strlen(test_data2)+1;

    ret = blob.Insert(test_data2, lob_len);
    assert(ret == eBDB_Ok);

    {{

    blob.i1 = 1;
    blob.i2 = 4;

    CBDB_BLobStream* bstream = blob.CreateStream();

    unsigned len = strlen(test_data);
    for (unsigned i = 0; i < len+1; ++i) {
        char ch = test_data[i];
        bstream->Write(&ch, 1);
    }

    delete bstream;

    blob.i1 = 1;
    blob.i2 = 4;

    ret = blob.Fetch();
    assert(ret == eBDB_Ok);
    unsigned len1 = blob.LobSize();
    assert(len1 == strlen(test_data)+1);

    char buf[256] = {0,};
    ret = blob.GetData(buf, sizeof(buf));
    assert(ret == eBDB_Ok);

    if (strcmp(buf, test_data) != 0) {
        cout << "BLOB content comparison error!" << endl;
        cout << "BLobData:" << buf << endl;
        assert(0);
    }


    }}

    cout << "Testing two-phase read." << endl;
//    blob.Reopen(CBDB_LobFile::eReadOnly);

    blob.i1 = 1;
    blob.i2 = 2;

    ret = blob.Fetch();
    assert(ret == eBDB_Ok);
    unsigned len1 = blob.LobSize();
    assert(len1 == strlen(test_data)+1);

    char buf[256] = {0,};
    ret = blob.GetData(buf, sizeof(buf));
    assert(ret == eBDB_Ok);

    if (strcmp(buf, test_data) != 0) {
        cout << "BLOB content comparison error!" << endl;
        cout << "BLobData:" << buf << endl;
        assert(0);
    }



    buf[0] = 0;

    cout << "Testing BLOB based cursor" << endl;

    {{
    CBDB_FileCursor cur(blob);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << 1;

    const char* tdata = test_data;
    while (cur.Fetch() == eBDB_Ok) {

        assert(blob.i2 == 2 || blob.i2 == 3 || blob.i2 == 4);
        unsigned len = blob.LobSize();
        ret = blob.GetData(buf, sizeof(buf));
        assert(ret == eBDB_Ok);
        if (strcmp(buf, tdata) != 0) {
            int i2 = blob.i2;
            if (i2 == 4) {
                assert(strcmp(buf, test_data) ==0);
            }
            else {
                cout << "BLOB content comparison error!" << endl;
                cout << "BLobData:" << buf << endl;
                assert(0);
            }
        }
        cout << "Lob len=" << len << endl;
        tdata = test_data2;
    }

    cout << "Testing read stream" << endl;

    blob.i1 = 1;
    blob.i2 = 2;

    CBDB_BLobStream* bstream = blob.CreateStream();

    char ch;
    size_t bytes_read = 0;

    size_t pending = bstream->PendingCount();


    for(int i=0;;++i) {
        bstream->Read(&ch, 1, &bytes_read);
        if (bytes_read == 0)
            break;
        assert(ch == test_data[i]);

        size_t pend = bstream->PendingCount();
        assert(pend == pending - 1);

        pending = pend;
    }

    delete bstream;

    }}

    cout << "Testing BLOB based update cursor" << endl;

    unsigned i2 = 0;
    {{
    CBDB_FileCursor cur(blob);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << 1;

    //const char* tdata = test_data;
    while (cur.Fetch() == eBDB_Ok) {
        i2 = blob.i2;
        assert(i2 == 2 || i2 == 3 || i2 == 4);
        char tbuf[] = "TEst";
        ret = cur.UpdateBlob(tbuf, sizeof(tbuf));
        break;
    }
    }}

    blob.i1 = 1;
    blob.i2 = i2;

    char tbuf[256];

    ret = blob.Fetch();
    assert(ret == eBDB_Ok);
    len1 = blob.LobSize();
    assert(len1 == 5);
    ret = blob.GetData(tbuf, sizeof(tbuf));
    assert(strcmp("TEst", tbuf) == 0);


    cout << "======== BLob file test ok." << endl;
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

    // Test UpdateInsert

    dbf1.Reopen(CBDB_File::eReadWrite);
    dbf1.IdKey = 2;
    dbf1.idata = 250;
    dbf1.str = "test";
    err = dbf1.UpdateInsert();
    assert(err == eBDB_Ok);

    dbf1.IdKey = 2;
    dbf1.idata = 0;
    err = dbf1.Fetch();
    assert(err == eBDB_Ok);
    int idata = dbf1.idata;
    assert(idata == 250);

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
        assert (i2 == (int)(i+3));
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


//////////////////////////////////////////////////////////////////
//
//
// Database record structure to test with duplicate records
//
//

struct TestDuplicateKeys : public CBDB_File
{
    CBDB_FieldInt4        idata;
    CBDB_FieldString      str;

    TestDuplicateKeys()
    : CBDB_File(CBDB_File::eDuplicatesEnable)
    {
        BindKey("idata", &idata);
        BindData("str", &str, 256);
    }
};

//////////////////////////////////////////////////////////////////
//
//
// BDB string key table fill test
//
//


static void s_TEST_BDB_Duplicates(void)
{
    cout << "======== Duplicate keys test." << endl;

    TestDuplicateKeys  dbf;

    dbf.Open(s_TestFileName3, CBDB_File::eCreate);

    // Fill the table

    dbf.idata = 1;
    dbf.str = "Str1";
    EBDB_ErrCode ret = dbf.Insert();
    assert (ret == eBDB_Ok);

    dbf.idata = 1;
    dbf.str = "Str11";
    ret = dbf.Insert();
    assert (ret == eBDB_Ok);

    dbf.idata = 10;
    dbf.str = "Str100";
    ret = dbf.Insert();
    assert (ret == eBDB_Ok);

    {{
        CBDB_FileCursor cur(dbf);
        cur.SetCondition(CBDB_FileCursor::eEQ);

        cur.From << 1;
        unsigned int recs_fetched = 0;
        while (cur.Fetch() == eBDB_Ok) {
            unsigned idata = dbf.idata;
            assert(idata == 1);

            string str = (const char*)dbf.str;
            assert(str == "Str1" || str == "Str11");

            ++recs_fetched;
        }

        assert(recs_fetched == 2);

    }}

    dbf.idata = 1;
    ret = dbf.Delete();
    assert (ret == eBDB_Ok);
    {{
        CBDB_FileCursor cur(dbf);
        cur.SetCondition(CBDB_FileCursor::eEQ);

        cur.From << 1;
        unsigned int recs_fetched = 0;
        while (cur.Fetch() == eBDB_Ok) {
            assert(0);
            ++recs_fetched;
        }
        assert(recs_fetched == 0);

    }}


    cout << "======== Duplicate keys test ok." << endl;

}


//////////////////////////////////////////////////////////////////
//
//
// db_map test
//
//

bool CheckMapDataValid_i2s(int first, const string& second)
{
    char szBuf[256];
    sprintf(szBuf, "Data%i", first);
    return second == string(szBuf);
}

static void s_TEST_db_map(void)
{
    cout << "======== db_map test." << endl;

    db_map<int, string>  i2s;

    i2s.open(s_db_map1, IOS_BASE::out|IOS_BASE::trunc);

    i2s.insert(pair<const int, string>(1, "Data1"));
    i2s.insert(pair<const int, string>(2, "Data2"));
    i2s.insert(pair<const int, string>(3, "Data3"));

    string v = i2s[2];

    assert(v == "Data2");

    {{
    db_map<int, string>::const_iterator it(i2s.begin());
    while (it.valid()) {
        bool b = CheckMapDataValid_i2s((*it).first, (*it).second);
        assert(b);
        ++it;
    }

    }}

    {{
    db_map<int, string>::const_iterator it(i2s.begin());
    db_map<int, string>::const_iterator it_end(i2s.end());
    for (;it != it_end; ++it) {
        bool b = CheckMapDataValid_i2s(it->first, it->second);
        assert(b);
    }

    }}

    i2s.clear();
    size_t sz = i2s.size();
    assert(sz == 0);

    // testing string -> int case

    db_map<string, int>  s2i;

    s2i.open("s2i.db", IOS_BASE::out|IOS_BASE::trunc);

    s2i.insert(pair<const string, int>("Data1", 1));
    s2i.insert(pair<const string, int>("Data2", 2));
    s2i.insert(pair<const string, int>("Data3", 3));

    int i = s2i[string("Data2")];

    assert(i == 2);


    cout << "======== db_map test ok." << endl;

}

static void s_TEST_db_multimap(void)
{
    cout << "======== db_multimap test." << endl;

    db_multimap<int, string>  ii2s;

    ii2s.open(s_db_map2, IOS_BASE::out|IOS_BASE::trunc);

    ii2s.insert(pair<const int, string>(1, "Data1"));
    ii2s.insert(pair<const int, string>(2, "Data2"));
    ii2s.insert(pair<const int, string>(3, "Data3"));
    ii2s.insert(pair<const int, string>(3, "Data31"));

    size_t sz = ii2s.size();
    assert(sz == 4);

    sz = 0;
    {{
    db_multimap<int, string>::const_iterator it(ii2s.find(3));
    while (it.valid()) {
        const string& data = it->second;
        if (sz == 0) {
            assert(data == "Data3");
        } else {
            assert(data == "Data31");
        }

        ++sz;
        ++it;
    }

    }}
    assert(sz == 2);

    ii2s.erase(3);
    sz = ii2s.size();
    assert(sz == 2);


    cout << "======== db_multimap test ok." << endl;

}


/// @internal
static void s_TEST_BDB_Queue()
{
    cout << "======== Queue test." << endl;

    TestQueue  db_q;

    db_q.Open("db_queue.que", CBDB_File::eCreate);

    vector<unsigned> recs;
    for (int i = 0; i < 100; ++i) {
        db_q.v1 = i;
        db_q.v2 = i * 10;

        unsigned rec_id = db_q.Append();
        recs.push_back(rec_id);
        cout << rec_id << endl;
    }

    EBDB_ErrCode ret;
    db_q.key = recs[1];
    ret = db_q.Fetch();
    assert (ret == eBDB_Ok);

    int v1 = db_q.v1;
    int v2 = db_q.v2;

    cout << v1 << " " << v2 << endl;
    assert(v1 == 1);
    assert(v2 == 10);



    db_q.key = 99999900;
    ret = db_q.Fetch();
    assert (ret != eBDB_Ok);

    db_q.key = recs[0];
    db_q.Delete();

    db_q.key = recs[0];
    ret = db_q.Fetch();
    assert (ret != eBDB_Ok);



    {{
    TestQueueBlob db_q_blob;
    const char* szTest = "Hello queue!";
    unsigned len = strlen(szTest)+1;
    db_q_blob.Open("db_queue_blob.que", CBDB_File::eCreate,
                   false /*disable dirty reads*/, 100 /*max record length*/);

    unsigned rec_id = db_q_blob.Append(szTest, len);
    cout << rec_id << endl;
    char szBuf[100];

    EBDB_ErrCode ret;
    db_q_blob.key = rec_id;
    ret = db_q_blob.GetData(szBuf, 100);
    cout << "Read value: " << szBuf << " BDB error code: " << ret << endl;

    }}

    cout << "======== Queue test ok." << endl;
}



/// @internal
static void s_TEST_BDB_Hash()
{
    cout << "======== Hash test." << endl;
    EBDB_ErrCode ret;

    TestHash  db_h;

    db_h.Open("db_hash.db", CBDB_File::eCreate);

    for (int i = 0; i < 100; ++i) {
        string k = "key_" + NStr::IntToString(i);
        string d = "data_" + NStr::IntToString(i);
        db_h.key = k;
        db_h.value = d;

        ret = db_h.Insert();
        assert (ret == eBDB_Ok);
    }

    db_h.key = "key_0";
    ret = db_h.Fetch();
    assert (ret == eBDB_Ok);

    string d = db_h.value.GetString();
    assert (d == "data_0");


    db_h.key = "update1";
    db_h.value = "udata1";
    ret = db_h.Insert();
    assert (ret == eBDB_Ok);

    db_h.key = "update1";
    db_h.value = "udata2";
    ret = db_h.UpdateInsert();
    assert (ret == eBDB_Ok);

    db_h.key = "update1";
    ret = db_h.Fetch();
    assert (ret == eBDB_Ok);
    string udata = db_h.value.GetString();
    assert(udata == "udata2");

    db_h.key = "update1";
    ret = db_h.Delete();
    assert (ret == eBDB_Ok);

    db_h.key = "update1";
    ret = db_h.Fetch();
    assert (ret == eBDB_NotFound);

    for (int i = 0; i < 100; ++i) {
        string k = "key_" + NStr::IntToString(i);
        string d = "data_" + NStr::IntToString(i);
        db_h.key = k;

        ret = db_h.Fetch();
        assert (ret == eBDB_Ok);
        string v = db_h.value.GetString();
        assert (d == v);
    }

    cout << "======== Hash test ok." << endl;
}






static void s_TEST_ICache(void)
{
    cout << "======== ICache test." << endl;

    vector<int> data;
    data.push_back(10);
    data.push_back(20);
    data.push_back(30);
    data.push_back(40);

    const void* dp = &data[0];

    {{
     CBDB_Cache  bdb_cache;
     int top = bdb_cache.GetTimeStampPolicy();
     bdb_cache.SetTimeStampPolicy(top, 30);
     bdb_cache.SetVersionRetention( ICache::eDropAll );

     bdb_cache.Open("nt_cache", "bcache",
                    CBDB_Cache::ePidLock, 1024*1024 * 100,
                    CBDB_Cache::eNoTrans);

     char buf[100] = {0,};
     bdb_cache.Store("a", 0, "", buf, 1 );
     bdb_cache.Store("b", 0, "", buf, 1 );
     size_t sz = bdb_cache.GetSize("a", 0, "");
     assert(sz == 1);

    }}


    CBDB_Cache  bdb_cache;
    int top = bdb_cache.GetTimeStampPolicy();
    bdb_cache.SetTimeStampPolicy(top, 30);
    bdb_cache.Open("cache", "bcache", CBDB_Cache::ePidLock, 1024*1024 * 100);
    {{
    CBDB_Cache  bdb_cache2;
    bdb_cache2.Open("cache", "bcache", CBDB_Cache::ePidLock, 1024*1024 * 100);

    }}

    bdb_cache.Store("test_key1", 1, "", dp, data.size() * sizeof(int));

    vector<int> data2;

    size_t sz = bdb_cache.GetSize("test_key1", 1, "");
    assert(sz);

    sz = sz / sizeof(int);
    assert(sz == data.size());

    data2.resize(sz);
    void* dp2 = &data2[0];

    /* bool res = */ bdb_cache.Read("test_key1", 1, "", dp2, sz * sizeof(int));

    assert(data2[0] == data[0]);
    assert(data2[1] == data[1]);
    assert(data2[2] == data[2]);
    assert(data2[3] == data[3]);


    // Test read-only mode of operation
    cout << "======== ICache test; readonly mode skipped. "
            "It is completely broken" << endl;

    #if 0
    vector<int> data3;

    bdb_cache.OpenReadOnly("cache", "bcache");

    sz = bdb_cache.GetSize("test_key1", 1, "");
    assert(sz);

    sz = sz / sizeof(int);
    assert(sz == data.size());

    data3.resize(sz);
    void* dp3 = &data3[0];

    res = bdb_cache.Read("test_key1", 1, "", dp3, sz * sizeof(int));

    assert(data3[0] == data[0]);
    assert(data3[1] == data[1]);
    assert(data3[2] == data[2]);
    assert(data3[3] == data[3]);

    bdb_cache.Store("test_key11", 1, "", dp, data.size() * sizeof(int));
    sz = bdb_cache.GetSize("test_key11", 1, "");
    assert(sz == 0);
    #endif
}

static void s_TEST_ExtBlob(void)
{
    cout << "======== Ext BLOB attributes test." << endl;

    {{
    CBDB_ExtBlobMap ext_attr;

    assert(ext_attr.Size() == 0);

    ext_attr.Add(10, 123, 10);
    ext_attr.Add(11, 149, 101);
    ext_attr.Add(13, 1,  20);
    ext_attr.Add(20, 0,  0xFFFFAAAA);

    assert(ext_attr.Size() == 4);

    bool b;
    b = ext_attr.HasBlob(13);
    assert(b);
    b = ext_attr.HasBlob(14);
    assert(!b);

    Uint8 offset, size;
    b = ext_attr.GetBlobLoc(13, &offset, &size);
    assert(b);
    assert(offset == 1 && size == 20);
    b = ext_attr.GetBlobLoc(20, &offset, &size);
    assert(b);
    assert(offset == 0 && size == 0xFFFFAAAA);


    Uint4 min_id, max_id;
    ext_attr.GetBlobIdRange(&min_id, &max_id);
    assert(min_id == 10 && max_id == 20);


    CBDB_RawFile::TBuffer buf;
    ext_attr.Serialize(&buf);

    CBDB_ExtBlobMap ext_attr2;
    ext_attr2.Deserialize(buf);

    assert(ext_attr2.Size() == 4);

    b = ext_attr2.HasBlob(13);
    assert(b);
    b = ext_attr2.HasBlob(14);
    assert(!b);

    b = ext_attr2.GetBlobLoc(13, &offset, &size);
    assert(b);
    assert(offset == 1 && size == 20);
    b = ext_attr2.GetBlobLoc(20, &offset, &size);
    assert(b);
    assert(offset == 0 && size == 0xFFFFAAAA);


    ext_attr2.GetBlobIdRange(&min_id, &max_id);
    assert(min_id == 10 && max_id == 20);
    }}

    // test meta container

    {{
    CBDB_BlobMetaContainer mcont;

    mcont.SetLoc(10, 200);
    Uint8 offset, size;
    mcont.GetLoc(&offset, &size);
    assert(offset == 10 && size == 200);

    CBDB_ExtBlobMap& ext_attr = mcont.SetBlobMap();

    ext_attr.Add(10, 123, 10);
    ext_attr.Add(11, 149, 101);
    ext_attr.Add(13, 1,  20);
    ext_attr.Add(20, 0,  15);

    assert(ext_attr.Size() == 4);

    CBDB_RawFile::TBuffer buf;
    mcont.Serialize(&buf);


    CBDB_BlobMetaContainer mcont2;
    mcont2.Deserialize(buf);
    mcont2.GetLoc(&offset, &size);
    assert(offset == 10 && size == 200);

    CBDB_ExtBlobMap& ext_attr2 = mcont2.SetBlobMap();
    assert(ext_attr2.Size() == 4);

    bool b;
    b = ext_attr2.HasBlob(13);
    assert(b);
    b = ext_attr2.HasBlob(14);
    assert(!b);

    b = ext_attr2.GetBlobLoc(13, &offset, &size);
    assert(b);
    assert(offset == 1 && size == 20);
    b = ext_attr2.GetBlobLoc(20, &offset, &size);
    assert(b);
    assert(offset == 0 && size == 15);


    Uint4 min_id, max_id;
    ext_attr2.GetBlobIdRange(&min_id, &max_id);
    assert(min_id == 10 && max_id == 20);

    }}

    // test meta container store
    {{
    CBlobMetaDB meta_db;
    meta_db.Open("meta.db", CBDB_RawFile::eCreate);

    // 5-15 range
    {{
        CBDB_BlobMetaContainer mcont;

        mcont.SetLoc(11, 210);
        CBDB_ExtBlobMap& ext_attr = mcont.SetBlobMap();
        ext_attr.Add(5, 123, 10);
        ext_attr.Add(8, 149, 101);
        ext_attr.Add(15, 1,  20);

        meta_db.UpdateInsert(mcont);
    }}
    // 3-400 range
    {{
        CBDB_BlobMetaContainer mcont;

        mcont.SetLoc(11, 210);
        CBDB_ExtBlobMap& ext_attr = mcont.SetBlobMap();
        ext_attr.Add(3, 123, 10);
        ext_attr.Add(7, 149, 101);
        ext_attr.Add(400, 1,  20);

        meta_db.UpdateInsert(mcont);
    }}


    // 10-20 range
    {{
        CBDB_BlobMetaContainer mcont;

        mcont.SetLoc(10, 200);
        CBDB_ExtBlobMap& ext_attr = mcont.SetBlobMap();
        ext_attr.Add(10, 123, 10);
        ext_attr.Add(11, 149, 101);
        ext_attr.Add(13, 1,  20);
        ext_attr.Add(20, 0,  15);

        meta_db.UpdateInsert(mcont);
    }}

    // 14-21 range
    {{
        CBDB_BlobMetaContainer mcont;

        mcont.SetLoc(10, 200);
        CBDB_ExtBlobMap& ext_attr = mcont.SetBlobMap();
        ext_attr.Add(14, 123, 10);
        ext_attr.Add(19, 12, 100);
        ext_attr.Add(21, 0,  15);

        meta_db.UpdateInsert(mcont);
    }}
    // 100-210 range
    {{
        CBDB_BlobMetaContainer mcont;

        mcont.SetLoc(10, 200);
        CBDB_ExtBlobMap& ext_attr = mcont.SetBlobMap();
        ext_attr.Add(100, 123, 10);
        ext_attr.Add(101, 12, 100);
        ext_attr.Add(210, 0,  15);

        meta_db.UpdateInsert(mcont);
    }}
    // 300-310 range
    {{
        CBDB_BlobMetaContainer mcont;

        mcont.SetLoc(10, 200);
        CBDB_ExtBlobMap& ext_attr = mcont.SetBlobMap();
        ext_attr.Add(300, 123, 10);
        ext_attr.Add(301, 12, 100);
        ext_attr.Add(310, 0,  15);

        meta_db.UpdateInsert(mcont);
    }}

    {{
        CBDB_FileCursor cur(meta_db);
        cur.SetCondition(CBDB_FileCursor::eGE);
        cur.From << 0;
        while (cur.Fetch() == eBDB_Ok) {
            unsigned from = meta_db.id_from;
            unsigned to   = meta_db.id_to;
            cout << from << " - " << to << endl;
        }
    }}


    unsigned from, to;
    CBDB_BlobMetaContainer mcont;
    EBDB_ErrCode ret = meta_db.FetchMeta(15, &mcont, &from, &to);

    assert(ret == eBDB_Ok);
    assert(from == 5);
    assert(to == 15);

    const CBDB_ExtBlobMap& ext_attr = mcont.GetBlobMap();
    Uint8 offset, size;
    mcont.GetLoc(&offset, &size);
    assert(offset == 11 && size == 210);

    bool b = ext_attr.GetBlobLoc(15, &offset, &size);
    assert(b);
    assert(offset == 1 && size == 20);


    }}

    NcbiCout << "Store test..." << NcbiEndl;

    // Save blobs to ext store
    {{
    CBDB_ExtBlobStore<bm::bvector<> > ext_store;
    ext_store.Open("extstore", CBDB_RawFile::eCreate);
    ext_store.SetContainerMaxSize(4 * 1024);

    for (int i = 0; i < 10; ++i) {
        CBDB_RawFile::TBuffer buf;
        string s = NStr::IntToString(i);
        char ch = s[0];
        buf.resize_mem(i * 200);
        for (size_t j=0; j < buf.size(); ++j) {
            buf[j] = ch;
        }
        ext_store.StoreBlob(i+1, buf);
    }

    ext_store.Save();
    ext_store.Close();
    }}

    // Load blobs from ext.store
    {{
    CBDB_ExtBlobStore<bm::bvector<> > ext_store;
    ext_store.Open("extstore", CBDB_RawFile::eReadOnly);

    for (unsigned int i = 0; i < 10; ++i) {
        CBDB_RawFile::TBuffer buf;
        EBDB_ErrCode ret = ext_store.ReadBlob(i+1, buf);
        assert(ret == eBDB_Ok);

        string s = NStr::IntToString(i);
        char ch = s[0];

        assert(buf.size() == i * 200);
        for (size_t j=0; j < buf.size(); ++j) {
            assert(buf[j] == ch);
        }
    }

    }}
    NcbiCout << "Store test...ok" << NcbiEndl;


    NcbiCout << "Compressed Store test..." << NcbiEndl;

    // Save blobs to ext store
    {{
    CBDB_ExtBlobStore<bm::bvector<> > ext_store;
    ext_store.SetCompressor(new CZipCompression(), eTakeOwnership);
    ext_store.Open("extstore_z", CBDB_RawFile::eCreate);
    ext_store.SetContainerMaxSize(4 * 1024);

    for (int i = 0; i < 10; ++i) {
        CBDB_RawFile::TBuffer buf;
        string s = NStr::IntToString(i);
        char ch = s[0];
        buf.resize_mem(i * 200);
        for (size_t j=0; j < buf.size(); ++j) {
            buf[j] = ch;
        }
        ext_store.StoreBlob(i+1, buf);
    }

    ext_store.Save();
    ext_store.Close();
    }}

    // Load blobs from ext.store
    {{
    CBDB_ExtBlobStore<bm::bvector<> > ext_store;
    ext_store.SetCompressor(new CZipCompression(), eTakeOwnership);
    ext_store.Open("extstore_z", CBDB_RawFile::eReadOnly);

    for (unsigned int i = 0; i < 10; ++i) {
        CBDB_RawFile::TBuffer buf;
        EBDB_ErrCode ret = ext_store.ReadBlob(i+1, buf);
        assert(ret == eBDB_Ok);

        string s = NStr::IntToString(i);
        char ch = s[0];

        assert(buf.size() == i * 200);
        for (size_t j=0; j < buf.size(); ++j) {
            assert(buf[j] == ch);
        }
    }

    }}
    NcbiCout << "Compressed Store test...ok" << NcbiEndl;


    cout << "======== Ext BLOB attributes test. OK." << endl;
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
    SetDiagTrace(eDT_Enable);

    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    SetDiagPostFlag(eDPF_Trace);


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

        s_TEST_BDB_Queue();

        s_TEST_BDB_Hash();

        s_TEST_BDB_IdTable_Fill();

        s_TEST_BDB_IdTable_Fill2();

        s_TEST_BDB_IdTable_Cursor();

        s_TEST_BDB_IdTable_DeleteInsert();

        s_TEST_BDB_LOB_File();

        s_TEST_BDB_BLOB_File();

        s_TEST_BDB_StrTable_Fill();

        s_TEST_BDB_Duplicates();

        s_TEST_BDB_Transaction();

        s_TEST_ExtBlob();

        s_TEST_db_map();

        s_TEST_db_multimap();

        s_TEST_ICache();

        s_TEST_BDB_Query();

        s_TEST_BDB_IdTable_FillStress();
    }
    catch (CBDB_ErrnoException& ex)
    {
        cout << "Error! BDB errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        cout << "Error! BDB library exception:" << ex.what();
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
    return CBDB_Test().AppMain(argc, argv);
}


