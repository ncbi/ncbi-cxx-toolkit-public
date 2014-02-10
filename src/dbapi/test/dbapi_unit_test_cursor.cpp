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

#include "dbapi_unit_test_pch.hpp"


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Cursor)
{
    const size_t rec_num = 2;
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Initialize a test table ...
        {
            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (size_t i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
            // Check record number ...
            BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
        }

        // Check that cursor can be opened twice ...
        {
            sql = "select int_field from " + GetTableName();

            // Open a cursor for the first time ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }

            // Open a cursor for the second time ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }
        }


        // Check cursor with "FOR UPDATE OF" clause ...
        // First test ...
        {
            sql  = "select int_field from " + GetTableName();
            sql += " FOR UPDATE OF int_field";

            // Open a cursor for the first time ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }
        }

        // Second test ...
        if (GetArgs().GetServerType() != CDBConnParams::eSybaseSQLServer)
        {
            sql  =
                " CREATE TABLE #AgpFiles ( \n"
                "     id   INT IDENTITY, \n"
                "     submit_id INT, \n"
                "     file_num  INT, \n"
                "     \n"
                "     cr_date   DATETIME DEFAULT getdate(), \n"
                "     path      VARCHAR(255), \n"
                "     name      VARCHAR(255), \n"
                "     notes     VARCHAR(255), \n"
                "     file_size INT, \n"
                "     objects INT default 0, \n"
                "     live_objects INT default 0, \n"
                "      \n"
                "     blob_num INT DEFAULT 0, \n"
                // "     data0 IMAGE NULL, \n"
                // "     data1 IMAGE NULL, \n"
                // "     data2 IMAGE NULL, \n"
                // "     data3 IMAGE NULL, \n"
                // "     data4 IMAGE NULL, \n"
                " )  \n"
                ;
            auto_stmt->ExecuteUpdate(sql);

            sql  =
                " CREATE TABLE #Objects ( \n"
                "     submit_id    INT, \n"
                "     file_id      INT, \n"
                "     rm_file_id   INT DEFAULT 0x7FFFFFF, \n"
                "     name         VARCHAR(80), \n"
                "     blob_ofs     INT NOT NULL, \n"
                // "     blob_len     INT NOT NULL, \n"
                // "     obj_len      INT NOT NULL, \n"
                // "      \n"
                // "     comps        INT NOT NULL, \n"
                // "     gaps         INT NOT NULL, \n"
                // "     scaffolds    INT NOT NULL, \n"
                // "     singletons   INT NOT NULL, \n"
                // "     checksum     INT NOT NULL, \n"
                "      \n"
                "     chr          SMALLINT DEFAULT -1, \n"
                "     is_scaffold  BIT, \n"
                "     is_unlinked  BIT, \n"
                "      \n"
                // "     line_num     INT, \n"
                "     PRIMARY KEY(submit_id, file_id, blob_ofs) \n"
                " ) \n"
                ;
            auto_stmt->ExecuteUpdate(sql);

            string cursor_sql  =
                " SELECT obj.name, f.name, obj.chr, obj.is_scaffold, obj.is_unlinked \n"
                "   FROM #Objects obj \n"
                "   JOIN #AgpFiles f ON obj.file_id = f.id \n"
                //"  WHERE obj.rm_file_id=0x7FFFFFF AND obj.submit_id = 1 \n"
                "  WHERE obj.rm_file_id=0x7FFFFFF \n"
                "  ORDER BY obj.submit_id \n"
                "    FOR UPDATE OF obj.chr, obj.is_scaffold, obj.is_unlinked"
                ;

            // Insert something ...
            {
                sql  = "INSERT INTO #AgpFiles(submit_id, file_num, path, name, notes, file_size) ";
                sql += "VALUES(1, 0, '', '', '', 0)";

                auto_stmt->ExecuteUpdate(sql);

                Int8 agp_files_id = GetIdentity(auto_stmt);

                // Make 2 different inserts
                sql  = "INSERT INTO #Objects(submit_id, file_id, name, blob_ofs) ";
                sql += "VALUES(1, " + NStr::Int8ToString(agp_files_id) + ", '', 0)";

                auto_stmt->ExecuteUpdate(sql);

                sql  = "INSERT INTO #Objects(submit_id, file_id, name, blob_ofs) ";
                sql += "VALUES(2, " + NStr::Int8ToString(agp_files_id) + ", '', 0)";

                auto_stmt->ExecuteUpdate(sql);
            }

            // Just read data ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }

            // Update something ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                // Make 2 different updates
                sql = "UPDATE #Objects SET chr=123, is_scaffold=234, is_unlinked=345";

                BOOST_CHECK(rs->Next());
                auto_cursor->Update("#Objects", sql);

                sql = "UPDATE #Objects SET chr=456, is_scaffold=234, is_unlinked=345";

                rs->Next();
                auto_cursor->Update("#Objects", sql);
            }

            // Check that update was successful
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                int check_val = 123;
                while (rs->Next()) {
                    BOOST_CHECK(rs->GetVariant(3).GetInt4() == check_val);
                    check_val = 456;
                }
            }

            // Delete something
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                BOOST_CHECK(rs->Next());
                auto_cursor->Delete("#Objects");

                auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
                BOOST_CHECK_EQUAL(size_t(1), GetNumOfRecords(auto_stmt, "#Objects"));
            }
        } else {
            GetArgs().PutMsgDisabled("Test_Cursor Second test");
        } // Second test ...
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Cursor2)
{
    const size_t rec_num = 2;
    string sql;

    try {
        // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (size_t i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
            // Check record number ...
            BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
        }

        // Test CLOB field update ...
        // It doesn't work right now.
        {
            const char* clob = "abc";

            sql = "select text_field from " + GetTableName() + " for update of text_field \n";
            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", sql));

            {
                // blobRs should be destroyed before auto_cursor ...
                auto_ptr<IResultSet> blobRs(auto_cursor->Open());

                if (blobRs->Next()) {
                    ostream& out = auto_cursor->GetBlobOStream(1, strlen(clob),
                                                               kBOSFlags);
                    out.write(clob, strlen(clob));
                    out.flush();
                } else {
                    BOOST_FAIL( msg_record_expected );
                }
            }

            // Check record number ...
            {
                auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
                BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
            }

            // Another cursor ...
            sql  = " select text_field from " + GetTableName();
            sql += " where int_field = 1 for update of text_field";

            auto_cursor.reset(GetConnection().GetCursor("test02", sql));
            {
                // blobRs should be destroyed before auto_cursor ...
                auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                if ( !blobRs->Next() ) {
                    BOOST_FAIL( msg_record_expected );
                }

                ostream& out = auto_cursor->GetBlobOStream(1, strlen(clob),
                                                           kBOSFlags);
                out.write(clob, strlen(clob));
                out.flush();
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Cursor_Param)
{
    string sql;
    const size_t rec_num = 2;

    try {
         // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (size_t i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
            // Check record number ...
            BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
        }

        {
            sql = "select int_field from " + GetTableName() + " WHERE int_field = @int_value";

            // Open a cursor for the first time ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test10", sql));

                auto_cursor->SetParam(CVariant(Int4(0)), "@int_value" );

                // First Open ...
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK_EQUAL(rs->GetVariant(1).GetInt4(), 0);

                ///////////////
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // No ClearParamList in ICursor at the moment ...
                // auto_cursor->ClearParamList();

                auto_cursor->SetParam(CVariant(Int4(1)), "@int_value" );

                //  Second Open ...
                rs.reset(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK_EQUAL(rs->GetVariant(1).GetInt4(), 1);
            }

            // Open a cursor for the second time ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test10", sql));

                auto_cursor->SetParam(CVariant(Int4(1)), "@int_value" );

                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK_EQUAL(rs->GetVariant(1).GetInt4(), 1);
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Cursor_Multiple)
{
    string sql;
    const size_t rec_num = 2;

    try {
         // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (size_t i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
            // Check record number ...
            BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
        }

        // Open ...
        {
            sql = "select int_field from " + GetTableName() + " WHERE int_field = @int_value";

            auto_ptr<ICursor> auto_cursor1(GetConnection().GetCursor("test10", sql));
            auto_ptr<ICursor> auto_cursor2(GetConnection().GetCursor("test11", sql));

            auto_cursor1->SetParam(CVariant(Int4(0)), "@int_value" );
            auto_cursor2->SetParam(CVariant(Int4(0)), "@int_value" );

            auto_ptr<IResultSet> rs1(auto_cursor1->Open());
            BOOST_CHECK(rs1.get() != NULL);
            auto_ptr<IResultSet> rs2(auto_cursor2->Open());
            BOOST_CHECK(rs2.get() != NULL);

            BOOST_CHECK(rs1->Next());
            BOOST_CHECK_EQUAL(rs1->GetVariant(1).GetInt4(), 0);
            BOOST_CHECK(rs2->Next());
            BOOST_CHECK_EQUAL(rs2->GetVariant(1).GetInt4(), 0);
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


END_NCBI_SCOPE
