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
 * Author: Sergey Sikorskiy
 *
 * File Description: DBAPI unit-test
 *
 * ===========================================================================
 */

#include "sdbapi_unit_test_pch.hpp"


BEGIN_NCBI_SCOPE


BOOST_AUTO_TEST_CASE(Test_LOB)
{
    static string clob_value("1234567890");
    string sql;

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            // Clean table ...
            query.SetSql("DELETE FROM "+ GetTableName());
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...
            sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
            sql += " VALUES(0, '')";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            sql  = " SELECT text_field FROM " + GetTableName();
            // sql += " FOR UPDATE OF text_field";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);

            CQuery::iterator it = query.SingleSet().begin();
            CBlobBookmark bm = it[1].GetBookmark();
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            ostream& out = bm.GetOStream(clob_value.size(), kBOSFlags);
            out.write(clob_value.data(), clob_value.size());
            out.flush();
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field FROM "+ GetTableName();

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            ITERATE(CQuery, it, query.SingleSet()) {
                BOOST_CHECK(!it[1].IsNull());

                const string str_value = it[1].AsString();
                size_t blob_size = str_value.size();

                BOOST_CHECK_EQUAL(clob_value.size(), blob_size);
                BOOST_CHECK_EQUAL(clob_value, str_value);
                // Int8 value = rs->GetVariant(1).GetInt8();
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Test NULL values ...
        {
            enum {rec_num = 10};

            // Insert records ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

                // Insert data ...
                vector<CBlobBookmark> bms;
                for (long ind = 0; ind < rec_num; ++ind) {
                    query.SetParameter("@int_field", Int4(ind));
                    if (ind % 2 == 0) {
                        sql  = " INSERT INTO " + GetTableName() +
                            "(int_field, text_field) VALUES(@int_field, '')";
                    } else {
                        sql  = " INSERT INTO " + GetTableName() +
                            "(int_field, text_field) VALUES(@int_field, NULL)";
                    }

                    // Execute a statement with parameters ...
                    query.SetSql(sql);
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW
                        (query.VerifyDone(CQuery::eAllResultSets));

                    query.ClearParameters();

                    sql  = " SELECT text_field FROM " + GetTableName();
                    sql += " WHERE int_field = " + NStr::NumericToString(ind);

                    if (ind % 2 == 0) {
                        query.SetSql(sql);
                        query.Execute();
                        query.RequireRowCount(1);

                        bms.push_back(query.begin()[1].GetBookmark());
                        BOOST_CHECK_NO_THROW
                            (query.VerifyDone(CQuery::eAllResultSets));
                    }
                }
                ITERATE(vector<CBlobBookmark>, it, bms) {
                    ostream& out = it->GetOStream(clob_value.size(),
                                                  kBOSFlags);
                    out.write(clob_value.data(), clob_value.size());
                    out.flush();
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(query, GetTableName()));
            }

            // Read blob
            {
                sql = "SELECT text_field FROM "+ GetTableName();
                sql += " ORDER BY id";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(rec_num);

                CQuery::iterator it = query.begin();
                for (long ind = 0; ind < rec_num; ++ind, ++it) {
                    BOOST_CHECK(it != query.end());
                    if (ind % 2 == 0) {
                        BOOST_CHECK(!it[1].IsNull());
                        string result = it[1].AsString();
                        BOOST_CHECK_EQUAL(result, clob_value);
                    } else {
                        BOOST_CHECK(it[1].IsNull());
                    }
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        } // Test NULL values ...
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


BOOST_AUTO_TEST_CASE(Test_LOB_NewConn)
{
    static string table_name("sdbapi_test_lob");
    static string clob_value("1234567890");
    string sql;

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            // Create and clean table ...
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
            sql += "    int_field INT NULL, \n";
            sql += "    vc1000_field VARCHAR(1000) NULL, \n";
            sql += "    text_field TEXT NULL, \n";
            sql += "    image_field IMAGE NULL \n";
            sql += " )";

            try {
                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
            catch (CSDB_Exception& ex) {
                LOG_POST(Info << ex);
            }
            query.SetSql("DELETE FROM "+ table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...
            sql  = " INSERT INTO " + table_name + "(int_field, text_field)";
            sql += " VALUES(0, '')";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            sql  = " SELECT text_field FROM " + table_name;
            // sql += " FOR UPDATE OF text_field";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);

            ITERATE(CQuery, it, query.SingleSet()) {
                ostream& out = it[1].GetOStream(clob_value.size(), kBOSFlags);
                out.write(clob_value.data(), clob_value.size());
                out.flush();
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field FROM "+ table_name;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            ITERATE(CQuery, it, query.SingleSet()) {
                BOOST_CHECK(!it[1].IsNull());

                const string str_value = it[1].AsString();
                size_t blob_size = str_value.size();

                BOOST_CHECK_EQUAL(clob_value.size(), blob_size);
                BOOST_CHECK_EQUAL(clob_value, str_value);
                // Int8 value = rs->GetVariant(1).GetInt8();
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Test NULL values ...
        {
            enum {rec_num = 10};

            // Insert records ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + table_name;
                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    query.SetParameter("@int_field", Int4(ind));
                    if (ind % 2 == 0) {
                        sql  = " INSERT INTO " + table_name +
                            "(int_field, text_field) VALUES(@int_field, '')";
                    } else {
                        sql  = " INSERT INTO " + table_name +
                            "(int_field, text_field) VALUES(@int_field, NULL)";
                    }

                    // Execute a statement with parameters ...
                    query.SetSql(sql);
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW
                        (query.VerifyDone(CQuery::eAllResultSets));

                    query.ClearParameters();

                    sql  = " SELECT text_field FROM " + table_name;
                    sql += " WHERE int_field = " + NStr::NumericToString(ind);

                    if (ind % 2 == 0) {
                        query.SetSql(sql);
                        query.Execute();
                        query.RequireRowCount(1);

                        ITERATE(CQuery, it, query.SingleSet()) {
                            ostream& out = it[1].GetOStream(clob_value.size(),
                                                            kBOSFlags);
                            out.write(clob_value.data(), clob_value.size());
                            out.flush();
                        }
                        BOOST_CHECK_NO_THROW
                            (query.VerifyDone(CQuery::eAllResultSets));
                    }
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(query, table_name));
            }

            // Read blob
            {
                sql = "SELECT text_field FROM "+ table_name;
                sql += " ORDER BY id";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(rec_num);

                CQuery::iterator it = query.begin();
                for (long ind = 0; ind < rec_num; ++ind, ++it) {
                    BOOST_CHECK(it != query.end());
                    if (ind % 2 == 0) {
                        BOOST_CHECK(!it[1].IsNull());
                        string result = it[1].AsString();
                        BOOST_CHECK_EQUAL(result, clob_value);
                    } else {
                        BOOST_CHECK(it[1].IsNull());
                    }
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        } // Test NULL values ...
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB2)
{
    static char clob_value[] = "1234567890";
    string sql;
    enum {num_of_records = 10};

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            // Clean table ...
            query.SetSql("DELETE FROM "+ GetTableName());
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
                sql += " VALUES(0, '')";

                query.SetSql(sql);
                for (int i = 0; i < num_of_records; ++i) {
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW
                        (query.VerifyDone(CQuery::eAllResultSets));
                }
            }

            // Update CLOB value.
            {
                sql  = " SELECT text_field FROM " + GetTableName();
                // sql += " FOR UPDATE OF text_field";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(num_of_records);

                vector<CBlobBookmark> bms;
                ITERATE(CQuery, it, query.SingleSet()) {
                    bms.push_back(it[1].GetBookmark());
                }
                ITERATE(vector<CBlobBookmark>, it, bms) {
                    ostream& out = it->GetOStream(sizeof(clob_value) - 1,
                                                  kBOSFlags);
                    out.write(clob_value, sizeof(clob_value) - 1);
                    out.flush();
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field FROM "+ GetTableName();

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(num_of_records);
            ITERATE(CQuery, it, query.SingleSet()) {
                BOOST_CHECK( !it[1].IsNull() );

                size_t blob_size = it[1].AsString().size();
                BOOST_CHECK_EQUAL(sizeof(clob_value) - 1, blob_size);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB2_NewConn)
{
    static string table_name("sdbapi_test_lob2");
    static char clob_value[] = "1234567890";
    string sql;
    enum {num_of_records = 10};

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            // Create and clean table ...
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
            sql += "    int_field INT NULL, \n";
            sql += "    vc1000_field VARCHAR(1000) NULL, \n";
            sql += "    text_field TEXT NULL, \n";
            sql += "    image_field IMAGE NULL \n";
            sql += " )";

            try {
                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
            catch (CSDB_Exception& ex) {
                LOG_POST(Info << ex);
            }
            query.SetSql("DELETE FROM "+ table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + table_name + "(int_field, text_field)";
                sql += " VALUES(0, '')";

                query.SetSql(sql);
                for (int i = 0; i < num_of_records; ++i) {
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW
                        (query.VerifyDone(CQuery::eAllResultSets));
                }
            }

            // Update CLOB value.
            {
                sql  = " SELECT text_field FROM " + table_name;
                // sql += " FOR UPDATE OF text_field";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(num_of_records);

                ITERATE(CQuery, it, query.SingleSet()) {
                    ostream& out = it[1].GetOStream(sizeof(clob_value) - 1,
                                                    kBOSFlags);
                    out.write(clob_value, sizeof(clob_value) - 1);
                    out.flush();
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field FROM "+ table_name;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(num_of_records);
            ITERATE(CQuery, it, query.SingleSet()) {
                BOOST_CHECK( !it[1].IsNull() );

                size_t blob_size = it[1].AsString().size();
                BOOST_CHECK_EQUAL(sizeof(clob_value) - 1, blob_size);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB3)
{
    const string clob_value =
        "Seq-align ::= { type partial, dim 2, score "
        "{ { id str \"score\", value int 6771 }, { id str "
        "\"e_value\", value real { 0, 10, 0 } }, { id str "
        "\"bit_score\", value real { 134230121751674, 10, -10 } }, "
        "{ id str \"num_ident\", value int 7017 } }, segs denseg "
        // "{ dim 2, numseg 3, ids { gi 3021694, gi 3924652 }, starts "
        // "{ 6767, 32557, 6763, -1, 0, 25794 }, lens { 360, 4, 6763 }, "
        // "strands { minus, minus, minus, minus, minus, minus } } }"
        ;

    string sql;
    enum {num_of_records = 10};

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            // Clean table ...
            query.SetSql("DELETE FROM "+ GetTableName());
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...
            {
                sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field, image_field)";
                sql += " VALUES(0, @tv, @iv)";

                query.SetParameter("@tv", clob_value);
                query.SetParameter("@iv", clob_value, eSDB_Binary);

                query.SetSql(sql);
                for (int i = 0; i < num_of_records; ++i) {
                    // Execute statement with parameters ...
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW
                        (query.VerifyDone(CQuery::eAllResultSets));
                }

                query.ClearParameters();
            }
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field, image_field FROM "+ GetTableName();

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(num_of_records);
            ITERATE(CQuery, it, query.SingleSet()) {
                BOOST_CHECK( !it[1].IsNull() );
                string text_str = it[1].AsString();
                size_t text_blob_size = text_str.size();
                BOOST_CHECK_EQUAL(clob_value.size(), text_blob_size);
                BOOST_CHECK(NStr::Compare(clob_value, text_str) == 0);

                BOOST_CHECK( !it[2].IsNull() );
                string image_str = it[2].AsString();
                size_t image_blob_size = image_str.size();
                BOOST_CHECK_EQUAL(clob_value.size(), image_blob_size);
                BOOST_CHECK(NStr::Compare(clob_value, image_str) == 0);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB4)
{
    const string table_name = "#test_lob4";
    const string clob_value = "hello";
    string sql;

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            // Create table ...
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   text01  TEXT NULL \n"
                ") \n";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...
            sql = "INSERT INTO " + table_name + "(text01) VALUES ('abcdef')";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Update data ...
            sql =
                "DECLARE @p binary(16) \n"
                "SELECT @p = textptr(text01) FROM " + table_name + " WHERE id = 1 \n"
                "WRITETEXT " + table_name + ".text01 @p '" + clob_value + "'"
                ;
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Retrieve data ...
        {
            sql = "SELECT text01 FROM "+ table_name;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            ITERATE(CQuery, it, query.SingleSet()) {
                BOOST_CHECK( !it[1].IsNull() );
                BOOST_CHECK_EQUAL(it[1].AsString(), clob_value);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB_Multiple)
{
    const string table_name = "#sdbapi_test_lob_multiple";
    static string clob_value("1234567890");
    string sql;
    // enum {num_of_records = 10};

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            // Create table ...
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   text01  TEXT NULL, \n"
                "   text02  TEXT NULL, \n"
                "   image01 IMAGE NULL, \n"
                "   image02 IMAGE NULL \n"
                ") \n";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + table_name + "(text01, text02, image01, image02)";
                sql += " VALUES('', '', '', '')";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Update LOB value.
            {
                sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
                // With next line MS SQL returns incorrect blob descriptors in select
                //sql += " ORDER BY id";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(1);

                vector<CBlobBookmark> bms;
                ITERATE(CQuery, it, query.SingleSet()) {
                    for (int pos = 1; pos <= 4; ++pos) {
                        bms.push_back(it[pos].GetBookmark());
                    }
                }
                ITERATE(vector<CBlobBookmark>, it, bms) {
                    ostream& out = it->GetOStream(clob_value.size(),
                                                  kBOSFlags);
                    out.write(clob_value.data(), clob_value.size());
                    out.flush();
                    BOOST_CHECK(out.good());
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }

        // Retrieve data ...
        {
            sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
            sql += " ORDER BY id";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            ITERATE(CQuery, it, query.SingleSet()) {
                for (int pos = 1; pos <= 4; ++pos) {
                    BOOST_CHECK( !it[pos].IsNull() );
                    string value = it[pos].AsString();
                    size_t blob_size = value.size();
                    BOOST_CHECK_EQUAL(clob_value.size(), blob_size);
                    BOOST_CHECK_EQUAL(value, clob_value);
                }
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB_Multiple_NewConn)
{
    const string table_name = "sdbapi_test_lob_multiple";
    static string clob_value("1234567890");
    string sql;
    // enum {num_of_records = 10};

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            // Create table ...
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   text01  TEXT NULL, \n"
                "   text02  TEXT NULL, \n"
                "   image01 IMAGE NULL, \n"
                "   image02 IMAGE NULL \n"
                ") \n";
            try {
                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
            catch (CSDB_Exception& ex) {
                LOG_POST(Info << ex);
            }
            query.SetSql("DELETE FROM "+ table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + table_name + "(text01, text02, image01, image02)";
                sql += " VALUES('', '', '', '')";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Update LOB value.
            {
                sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
                // With next line MS SQL returns incorrect blob descriptors in select
                //sql += " ORDER BY id";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(1);

                ITERATE(CQuery, it, query.SingleSet()) {
                    for (int pos = 1; pos <= 4; ++pos) {
                        ostream& out = it[pos].GetOStream(clob_value.size(),
                                                          kBOSFlags);
                        out.write(clob_value.data(), clob_value.size());
                        out.flush();
                        BOOST_CHECK(out.good());
                    }
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }

        // Retrieve data ...
        {
            sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
            sql += " ORDER BY id";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            ITERATE(CQuery, it, query.SingleSet()) {
                for (int pos = 1; pos <= 4; ++pos) {
                    BOOST_CHECK( !it[pos].IsNull() );
                    string value = it[pos].AsString();
                    size_t blob_size = value.size();
                    BOOST_CHECK_EQUAL(clob_value.size(), blob_size);
                    BOOST_CHECK_EQUAL(value, clob_value);
                }
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_BlobStream)
{
    string sql;
    enum {test_size = 10000};
    long data_len = 0;
    long write_data_len = 0;

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            ostrstream out;

            for (int i = 0; i < test_size; ++i) {
                out << i << " ";
            }

            data_len = long(out.pcount());
            BOOST_CHECK(data_len > 0);

            // Clean table ...
            query.SetSql("DELETE FROM "+ GetTableName());
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...
            sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
            sql += " VALUES(0, '')";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            sql  = " SELECT text_field FROM " + GetTableName();

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);

            vector<CBlobBookmark> bms;
            ITERATE(CQuery, it, query.SingleSet()) {
                bms.push_back(it[1].GetBookmark());
            }
            ITERATE(vector<CBlobBookmark>, it, bms) {
                ostream& ostrm = it->GetOStream(data_len, kBOSFlags);

                ostrm.write(out.str(), data_len);
                out.freeze(false);

                BOOST_CHECK_EQUAL(ostrm.fail(), false);

                ostrm.flush();

                BOOST_CHECK_EQUAL(ostrm.fail(), false);
                BOOST_CHECK_EQUAL(ostrm.good(), true);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            query.SetSql("SELECT datalength(text_field) FROM " + GetTableName());
            query.Execute();
            query.RequireRowCount(1);

            ITERATE(CQuery, it, query.SingleSet()) {
                write_data_len = it[1].AsInt4();
                BOOST_CHECK_EQUAL( data_len, write_data_len );
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Retrieve data ...
        {
            query.SetSql("set textsize 2000000");
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            sql = "SELECT id, int_field, text_field FROM "+ GetTableName();

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            ITERATE(CQuery, it, query.SingleSet()) {
                istream& strm = it[3].AsIStream();
                int j = 0;
                for (int i = 0; i < test_size; ++i) {
                    strm >> j;
                    BOOST_CHECK_EQUAL(strm.good(), true);
                    BOOST_CHECK_EQUAL(strm.eof(), false);
                    BOOST_CHECK_EQUAL(j, i);
                }
                long read_data_len = long(strm.tellg());
                // Calculate a trailing space.
                BOOST_CHECK_EQUAL(data_len, read_data_len + 1);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_BlobStream_NewConn)
{
    static string table_name("sdbapi_test_blobstream");
    string sql;
    enum {test_size = 10000};
    long data_len = 0;
    long write_data_len = 0;

    try {
        CQuery query = GetDatabase().NewQuery();

        // Prepare data ...
        {
            ostrstream out;

            for (int i = 0; i < test_size; ++i) {
                out << i << " ";
            }

            data_len = long(out.pcount());
            BOOST_CHECK(data_len > 0);

            // Create and clean table ...
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
            sql += "    int_field INT NULL, \n";
            sql += "    vc1000_field VARCHAR(1000) NULL, \n";
            sql += "    text_field TEXT NULL, \n";
            sql += "    image_field IMAGE NULL \n";
            sql += " )";

            try {
                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
            catch (CSDB_Exception& ex) {
                LOG_POST(Info << ex);
            }
            query.SetSql("DELETE FROM "+ table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...
            sql  = " INSERT INTO " + table_name + "(int_field, text_field)";
            sql += " VALUES(0, '')";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            sql  = " SELECT text_field FROM " + table_name;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);

            ITERATE(CQuery, it, query.SingleSet()) {
                ostream& ostrm = it[1].GetOStream(data_len, kBOSFlags);

                ostrm.write(out.str(), data_len);
                out.freeze(false);

                BOOST_CHECK_EQUAL(ostrm.fail(), false);

                ostrm.flush();

                BOOST_CHECK_EQUAL(ostrm.fail(), false);
                BOOST_CHECK_EQUAL(ostrm.good(), true);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            query.SetSql("SELECT datalength(text_field) FROM " + table_name);
            query.Execute();
            query.RequireRowCount(1);

            ITERATE(CQuery, it, query.SingleSet()) {
                write_data_len = it[1].AsInt4();
                BOOST_CHECK_EQUAL( data_len, write_data_len );
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Retrieve data ...
        {
            query.SetSql("set textsize 2000000");
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            sql = "SELECT id, int_field, text_field FROM "+ table_name;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            ITERATE(CQuery, it, query.SingleSet()) {
                istream& strm = it[3].AsIStream();
                int j = 0;
                for (int i = 0; i < test_size; ++i) {
                    strm >> j;
                    BOOST_CHECK_EQUAL(strm.good(), true);
                    BOOST_CHECK_EQUAL(strm.eof(), false);
                    BOOST_CHECK_EQUAL(j, i);
                }
                long read_data_len = long(strm.tellg());
                // Calculate a trailing space.
                BOOST_CHECK_EQUAL(data_len, read_data_len + 1);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

END_NCBI_SCOPE
