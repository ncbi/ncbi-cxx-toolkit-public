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

#include <bdb/bdb_query_parser.hpp>

#include <test/test_assert.h>  /* This header must go last */

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
template<class T>
void ValidateRecord(const T& dbf1, unsigned int id)
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

// Utility function to check record consistency 
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

    //trans.Commit();

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
    assert (ret == eBDB_Ok);
    
    idata = dbf3.idata;
    assert(idata == 11);

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
        dbf.i2.Set(i+3);
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

    for (i = 1; i < s_RecsInTable; ++i) {
        dbf11.IdKey = i;
        EBDB_ErrCode ret = dbf11.Fetch();
        assert (ret == eBDB_Ok);

        ValidateRecord(dbf11, i);

    } // for

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
    cout << "======== Id table filling test 2." << endl;

    CBDB_Env env;
    env.OpenWithLocks(0);

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

    unsigned i2;
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
    assert(len1 = 5);
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

    bool res = bdb_cache.Read("test_key1", 1, "", dp2, sz * sizeof(int));

    assert(data2[0] == data[0]);
    assert(data2[1] == data[1]);
    assert(data2[2] == data[2]);
    assert(data2[3] == data[3]);


    // Test read-only mode of operation

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

        s_TEST_BDB_IdTable_Fill();

        // s_TEST_BDB_IdTable_Fill2();

        s_TEST_BDB_IdTable_Cursor();

        s_TEST_BDB_IdTable_DeleteInsert();

        s_TEST_BDB_LOB_File();

        s_TEST_BDB_BLOB_File();

        s_TEST_BDB_StrTable_Fill();

        s_TEST_BDB_Duplicates();

        s_TEST_BDB_Transaction();

        s_TEST_db_map();

        s_TEST_db_multimap();

        s_TEST_ICache();

        s_TEST_BDB_Query();

        s_TEST_BDB_IdTable_FillStress();


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
    return CBDB_Test().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.60  2005/03/15 14:47:28  kuznets
 * Removed unnecessary transaction commit()
 *
 * Revision 1.59  2005/03/08 17:43:27  vasilche
 * Put file-scope static names in anonymous namespace instead to please WorkShop.
 *
 * Revision 1.58  2005/02/22 14:08:40  kuznets
 * Cursor reopen test
 *
 * Revision 1.57  2005/02/02 19:49:54  grichenk
 * Fixed more warnings
 *
 * Revision 1.56  2004/12/17 12:10:35  kuznets
 * Code cleanup: removed unused variable
 *
 * Revision 1.55  2004/11/23 17:09:11  kuznets
 * Implemented BLOB update in cursor
 *
 * Revision 1.54  2004/11/01 16:55:15  kuznets
 * test for RMW locks
 *
 * Revision 1.53  2004/10/14 17:45:50  vasilche
 * Use IOS_BASE instead of non-portable ios_base.
 *
 * Revision 1.52  2004/08/24 14:20:14  kuznets
 * Fixed opening a read-only cache(wrong path)
 *
 * Revision 1.51  2004/08/24 14:08:29  kuznets
 * Cache test improved
 *
 * Revision 1.50  2004/08/13 11:51:16  kuznets
 * Stress test improvements
 *
 * Revision 1.49  2004/08/13 11:13:48  kuznets
 * changes in tests
 *
 * Revision 1.48  2004/06/21 15:11:15  kuznets
 * Added recovery flag to the transactional test
 *
 * Revision 1.47  2004/06/14 16:11:15  kuznets
 * Test for read-only cache
 *
 * Revision 1.46  2004/05/25 18:48:51  kuznets
 * Added cache RAM size parameter to CBDB_Cache::Open.
 *
 * Revision 1.45  2004/05/17 20:55:27  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.44  2004/04/28 12:18:27  kuznets
 * Remove obsolete IntCache test
 *
 * Revision 1.43  2004/03/23 16:37:55  kuznets
 * Implemented NOT predicate
 *
 * Revision 1.42  2004/03/23 14:51:19  kuznets
 * Implemented logical NOT, <, <=, >, >=
 *
 * Revision 1.41  2004/03/12 12:41:50  kuznets
 * + stress test for cursors
 *
 * Revision 1.40  2004/03/10 16:22:22  kuznets
 * + more tests
 *
 * Revision 1.39  2004/03/10 16:19:55  kuznets
 * + test case for single word queries
 *
 * Revision 1.38  2004/03/08 13:36:09  kuznets
 * One more test case for queries
 *
 * Revision 1.37  2004/03/01 14:07:11  kuznets
 * + test case for queries
 *
 * Revision 1.36  2004/02/04 19:16:50  kuznets
 * Fixed compilation bug (Workshop)
 *
 * Revision 1.35  2004/02/04 17:05:47  kuznets
 * Minor fix (compilation).
 *
 * Revision 1.34  2004/02/04 15:13:11  kuznets
 * + test case for length prefixed strings, when source std::string
 * contains zero chars.
 *
 * Revision 1.33  2003/12/23 22:32:24  ucko
 * Rename the second s_TEST_BDB_IdTable_Fill, but don't run it yet
 * because it crashes. :-/
 *
 * Revision 1.32  2003/12/22 18:58:02  kuznets
 * Added l-string test
 *
 * Revision 1.31  2003/12/12 19:13:31  kuznets
 * Transaction test: minor change file opening option.
 *
 * Revision 1.30  2003/12/12 14:09:53  kuznets
 * + s_TEST_BDB_Transaction()
 *
 * Revision 1.29  2003/11/26 13:09:44  kuznets
 * Re-enables icache test
 *
 * Revision 1.28  2003/11/25 19:58:37  kuznets
 * Temporary disabled ICache test (fails on Solaris)
 *
 * Revision 1.27  2003/11/25 19:36:54  kuznets
 * + test for ICache implementation
 *
 * Revision 1.26  2003/11/14 04:31:10  ucko
 * bytes_read should be size_t, not unsigned.
 *
 * Revision 1.25  2003/10/27 14:21:15  kuznets
 * + DBD dumper test
 *
 * Revision 1.24  2003/10/24 13:41:51  kuznets
 * Tested blob stream PendingCount
 *
 * Revision 1.23  2003/10/20 20:15:54  kuznets
 * Cache test improved
 *
 * Revision 1.22  2003/10/20 19:59:03  kuznets
 * + Unit test for Int cache
 *
 * Revision 1.21  2003/10/15 18:14:33  kuznets
 * Changed test to work with alternative page size and larger cache.
 *
 * Revision 1.20  2003/09/29 16:54:58  kuznets
 * Reverting unnecessary commit
 *
 * Revision 1.19  2003/09/29 16:27:07  kuznets
 * Cleaned up 64-bit compilation warnings
 *
 * Revision 1.18  2003/09/17 18:19:02  kuznets
 * Added test for BLOB streaming
 *
 * Revision 1.17  2003/09/16 15:15:16  kuznets
 * Test corrected to use Int2 field
 *
 * Revision 1.16  2003/08/27 20:05:33  kuznets
 * Added test working with file locking environment
 *
 * Revision 1.15  2003/07/25 15:35:59  kuznets
 * Added simple db_map<string, int> test
 *
 * Revision 1.14  2003/07/24 15:44:44  kuznets
 * Clened up several compiler warnings
 *
 * Revision 1.13  2003/07/23 20:23:37  kuznets
 * + test for clean, erase (db_map)
 *
 * Revision 1.12  2003/07/22 19:21:56  kuznets
 * Added test case for CBDB_File::Attach function
 *
 * Revision 1.11  2003/07/22 16:38:30  kuznets
 * Polishing test
 *
 * Revision 1.10  2003/07/22 15:21:17  kuznets
 * Sketched two tet cases for db_map and db_multimap
 *
 * Revision 1.9  2003/07/09 14:29:47  kuznets
 * Added DB_DUP mode test.
 *
 * Revision 1.8  2003/05/27 18:05:08  kuznets
 * Fixed compilation warnings
 *
 * Revision 1.7  2003/05/08 13:44:04  kuznets
 * Minor test improvements
 *
 * Revision 1.6  2003/05/07 14:13:45  kuznets
 * + test case for cursor reading of BLOB storage.
 *
 * Revision 1.5  2003/05/05 20:15:35  kuznets
 * Added CBDB_BLobFile
 *
 * Revision 1.4  2003/05/02 14:10:57  kuznets
 * Added test for UpdateInsert
 *
 * Revision 1.3  2003/04/29 20:50:22  kuznets
 * Code cleanup
 *
 * Revision 1.2  2003/04/29 19:04:13  kuznets
 * +Test makefiles
 *
 * Revision 1.1  2003/04/28 18:12:52  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
