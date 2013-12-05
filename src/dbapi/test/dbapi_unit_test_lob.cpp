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

#include <dbapi/driver/util/blobstore.hpp>


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB_Replication)
{
    string sql, sql2;
    CDB_Text txt;

    try {
        auto_ptr<IConnection> auto_conn(
            GetDS().CreateConnection(CONN_OWNERSHIP)
            );
        auto_conn->Connect(
                "DBAPI_test",
                "allowed",
                "MSSQL67"
                );
        CDB_Connection* conn = auto_conn->GetCDB_Connection();
        auto_ptr<CDB_LangCmd> auto_stmt;

        /* DBAPI_Sample..blobRepl is replicated from MSSQL67 to MSSQL8 */
        sql = "SELECT blob, TEXTPTR(blob) FROM DBAPI_Sample..blobRepl";

        {
            auto_stmt.reset(conn->LangCmd("BEGIN TRAN"));
            auto_stmt->Send();
            auto_stmt->DumpResults();

            auto_stmt.reset(conn->LangCmd(sql));
            auto_stmt->Send();

            txt.Append(NStr::Int8ToString(CTime(CTime::eCurrent).GetTimeT()));

            auto_ptr<I_ITDescriptor> descr[2];
            int i = 0;
            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                while(rs->Fetch()) {
                    rs->ReadItem(NULL, 0);
                    descr[i].reset(rs->GetImageOrTextDescriptor());
                    ++i;

                }

                BOOST_CHECK(descr[0].get() != NULL);
                BOOST_CHECK(descr[1].get() != NULL);
            }

            conn->SendData(*descr[0], txt);
            txt.MoveTo(0);
            conn->SendData(*descr[1], txt);

            auto_stmt.reset(conn->LangCmd("COMMIT TRAN"));
            auto_stmt->Send();
            auto_stmt->DumpResults();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

BOOST_AUTO_TEST_CASE(Test_LOB)
{
    // static char clob_value[] = "1234567890";
    static string clob_value("1234567890");
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Prepare data ...
        {
            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

            // Insert data ...
            sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
            sql += " VALUES(0, '')";
            auto_stmt->ExecuteUpdate( sql );

            sql  = " SELECT text_field FROM " + GetTableName();
            // sql += " FOR UPDATE OF text_field";

            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));

            // blobRs should be destroyed before auto_cursor ...
            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            while(blobRs->Next()) {
                ostream& out = auto_cursor->GetBlobOStream(1,
                                                           clob_value.size(),
                                                           eDisableLog);
                out.write(clob_value.data(), clob_value.size());
                out.flush();
            }

    //         auto_stmt->SendSql( sql );
    //         while( auto_stmt->HasMoreResults() ) {
    //             if( auto_stmt->HasRows() ) {
    //                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //                 while ( rs->Next() ) {
    //                     ostream& out = rs->GetBlobOStream(clob_value.size(), eDisableLog);
    //                     out.write(clob_value.data(), clob_value.size());
    //                     out.flush();
    //                 }
    //             }
    //         }
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while ( rs->Next() ) {
                        const CVariant& value = rs->GetVariant(1);


                        BOOST_CHECK( !value.IsNull() );

                        size_t blob_size = value.GetBlobSize();
                        const string str_value = value.GetString();

                        BOOST_CHECK_EQUAL(clob_value.size(), blob_size);
                        BOOST_CHECK_EQUAL(clob_value, str_value);
                        // Int8 value = rs->GetVariant(1).GetInt8();
                    }
                }
            }
        }

        // Read blob via Read method ...
        {
            string result;
            char buff[3];

            sql = "SELECT text_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while ( rs->Next() ) {
                        const CVariant& value = rs->GetVariant(1);


                        BOOST_CHECK( !value.IsNull() );
                        size_t read_bytes = 0;
                        while ((read_bytes = value.Read(buff, sizeof(buff)))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK_EQUAL(result, clob_value);
                    }
                }
            }
        }

        // Read Blob
        {
            string result;
            char buff[3];

            sql = "SELECT text_field FROM "+ GetTableName();

            auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

            bool rc = auto_stmt->Send();
            BOOST_CHECK( rc );

            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                while(rs->Fetch()) {
                    size_t read_bytes = 0;
                    bool is_null = true;

                    while ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                        result += string(buff, read_bytes);
                    }

                    BOOST_CHECK(!is_null);
                    BOOST_CHECK_EQUAL(result, clob_value);
                }

            }
        }

        // Test NULL values ...
        {
            enum {rec_num = 10};

            // Insert records ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                auto_stmt->ExecuteUpdate(sql);

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    auto_stmt->SetParam(CVariant( Int4(ind) ), "@int_field");

                    if (ind % 2 == 0) {
                        sql  = " INSERT INTO " + GetTableName() +
                            "(int_field, text_field) VALUES(@int_field, '')";
                    } else {
                        sql  = " INSERT INTO " + GetTableName() +
                            "(int_field, text_field) VALUES(@int_field, NULL)";
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate(sql);

                    if (false) {
                        // Use cursor with parameters.
                        // Unfortunately that doesn't work with the new ftds
                        // driver ....
                        sql  = " SELECT text_field FROM " + GetTableName();
                        sql += " WHERE int_field = @int_field ";

                        if (ind % 2 == 0) {
                            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));
                            auto_cursor->SetParam(CVariant(Int4(ind)), "@int_field");

                            // blobRs should be destroyed before auto_cursor ...
                            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                            while(blobRs->Next()) {
                                ostream& out = auto_cursor->GetBlobOStream(1,
                                        clob_value.size(),
                                        eDisableLog);
                                out.write(clob_value.data(), clob_value.size());
                                out.flush();
                            }
                        }
                    } else {
                        sql  = " SELECT text_field FROM " + GetTableName();
                        sql += " WHERE int_field = " + NStr::NumericToString(ind);

                        if (ind % 2 == 0) {
                            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));

                            // blobRs should be destroyed before auto_cursor ...
                            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                            while(blobRs->Next()) {
                                ostream& out = auto_cursor->GetBlobOStream(1,
                                        clob_value.size(),
                                        eDisableLog);
                                out.write(clob_value.data(), clob_value.size());
                                out.flush();
                            }
                        }
                    }
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num),
                                  GetNumOfRecords(auto_stmt, GetTableName())
                                  );
            }

            // Read blob via Read method ...
            {
                char buff[3];

                sql = "SELECT text_field FROM "+ GetTableName();
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                        for (long ind = 0; ind < rec_num; ++ind) {
                            BOOST_CHECK(rs->Next());

                            const CVariant& value = rs->GetVariant(1);
                            string result;

                            if (ind % 2 == 0) {
                                BOOST_CHECK(!value.IsNull());
                                size_t read_bytes = 0;
                                while ((read_bytes = value.Read(buff, sizeof(buff)))) {
                                    result += string(buff, read_bytes);
                                }

                                BOOST_CHECK_EQUAL(result, clob_value);
                            } else {
                                BOOST_CHECK(value.IsNull());
                            }
                        }
                    }
                }
            }

            // Read Blob
            {
                char buff[3];

                sql = "SELECT text_field FROM "+ GetTableName();

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while(auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL) {
                        continue;
                    }

                    if (rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    for (long ind = 0; ind < rec_num; ++ind) {
                        BOOST_CHECK(rs->Fetch());
                        string result;

                        if (ind % 2 == 0) {
                            size_t read_bytes = 0;
                            bool is_null = true;

                            while ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                                result += string(buff, read_bytes);
                            }

                            BOOST_CHECK(!is_null);
                            BOOST_CHECK_EQUAL(result, clob_value);
                        } else {
                            size_t read_bytes = 0;
                            bool is_null = true;

                            while ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                                result += string(buff, read_bytes);
                            }

                            BOOST_CHECK(is_null);
                        }
                    }

                }
            } // Read Blob
        } // Test NULL values ...

        if (false) {
            const string sql("select blob from Request where ri = 31960321 order by oid");

            auto_ptr<IConnection> conn(
                GetDS().CreateConnection(CONN_OWNERSHIP)
                );

            conn->Connect("*****","******","mssql31", "BlastQNew");
            auto_ptr<ICursor> auto_cursor(conn->GetCursor("test05", sql));

            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            while(blobRs->Next()) {
                char buff[8192];

                istream& strm = blobRs->GetBlobIStream();

                BOOST_CHECK_EQUAL(strm.good(), true);

                /*
                strm.read(buff, sizeof(buff));
                long read_data_len = strm.gcount();
                const string str_value(buff, read_data_len);
                char tmp = str_value[4097];

                BOOST_CHECK_EQUAL(str_value.size(), 5099U);
                BOOST_CHECK_EQUAL(str_value[4096], 40);
                */

                long read_data_len = 0;
                int c;
                while ( (c = strm.get()) != CT_EOF ) {
                    buff[read_data_len++] = c;
                }

                FILE* fd = fopen("dump.blob", "w");
                if (fd) {
                    fwrite(buff, 1, read_data_len, fd);
                    fclose(fd);
                }

                break;
            }
        }
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
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Prepare data ...
        {
            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

            // Insert data ...

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
                sql += " VALUES(0, '')";

                for (int i = 0; i < num_of_records; ++i) {
                    auto_stmt->ExecuteUpdate( sql );
                }
            }

            // Update CLOB value.
            {
                sql  = " SELECT text_field FROM " + GetTableName();
                // sql += " FOR UPDATE OF text_field";

                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));

                // blobRs should be destroyed before auto_cursor ...
                auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                while(blobRs->Next()) {
                    ostream& out =
                        auto_cursor->GetBlobOStream(1,
                                                    sizeof(clob_value) - 1,
                                                    eDisableLog
                                                    );
                    out.write(clob_value, sizeof(clob_value) - 1);
                    out.flush();
                }
            }
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while ( rs->Next() ) {
                        const CVariant& value = rs->GetVariant(1);

                        BOOST_CHECK( !value.IsNull() );

                        size_t blob_size = value.GetBlobSize();
                        BOOST_CHECK_EQUAL(sizeof(clob_value) - 1, blob_size);
                    }
                }
            }
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
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Prepare data ...
        {
            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

            // Insert data ...
            {
                sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field, image_field)";
                sql += " VALUES(0, @tv, @iv)";

                auto_stmt->SetParam( CVariant(clob_value), "@tv" );
                auto_stmt->SetParam( CVariant(clob_value), "@iv" );

                for (int i = 0; i < num_of_records; ++i) {
                    // Execute statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                auto_stmt->ClearParamList();
            }
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field, image_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if ( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while ( rs->Next() ) {
                        const CVariant& text_value = rs->GetVariant(1);
                        const CVariant& image_value = rs->GetVariant(2);

                        BOOST_CHECK( !text_value.IsNull() );
                        size_t text_blob_size = text_value.GetBlobSize();
                        BOOST_CHECK_EQUAL(clob_value.size(), text_blob_size);
                        BOOST_CHECK(NStr::Compare(clob_value, text_value.GetString()) == 0);

                        BOOST_CHECK( !image_value.IsNull() );
                        size_t image_blob_size = image_value.GetBlobSize();
                        BOOST_CHECK_EQUAL(clob_value.size(), image_blob_size);
                        BOOST_CHECK(NStr::Compare(clob_value, image_value.GetString()) == 0);
                    }
                }
            }
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
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Prepare data ...
        {
            // Create table ...
            if (table_name[0] =='#') {
                sql =
                    "CREATE TABLE " + table_name + " ( \n"
                    "   id NUMERIC IDENTITY NOT NULL, \n"
                    "   text01  TEXT NULL \n"
                    ") \n";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Insert data ...
            sql = "INSERT INTO " + table_name + "(text01) VALUES ('abcdef')";
            auto_stmt->ExecuteUpdate( sql );

            // Update data ...
            sql =
                "DECLARE @p binary(16) \n"
                "SELECT @p = textptr(text01) FROM " + table_name + " WHERE id = 1 \n"
                "WRITETEXT " + table_name + ".text01 @p '" + clob_value + "'"
                ;
            auto_stmt->ExecuteUpdate( sql );
        }

        // Retrieve data ...
        {
            sql = "SELECT text01 FROM "+ table_name;

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if ( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while ( rs->Next() ) {
                        const CVariant& value = rs->GetVariant(1);

                        BOOST_CHECK( !value.IsNull() );

                        BOOST_CHECK_EQUAL(value.GetString(), clob_value);
                    }
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB_Multiple)
{
    const string table_name = "#test_lob_multiple";
    static string clob_value("1234567890");
    string sql;
    // enum {num_of_records = 10};

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Prepare data ...
        {
            // Create table ...
            if (table_name[0] =='#') {
                sql =
                    "CREATE TABLE " + table_name + " ( \n"
                    "   id NUMERIC IDENTITY NOT NULL, \n"
                    "   text01  TEXT NULL, \n"
                    "   text02  TEXT NULL, \n"
                    "   image01 IMAGE NULL, \n"
                    "   image02 IMAGE NULL \n"
                    ") \n";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Insert data ...

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + table_name + "(text01, text02, image01, image02)";
                sql += " VALUES('', '', '', '')";

                auto_stmt->ExecuteUpdate( sql );

                /*
                for (int i = 0; i < num_of_records; ++i) {
                    auto_stmt->ExecuteUpdate( sql );
                }
                */
            }

            // Update LOB value.
            {
                sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
                // With next line MS SQL returns incorrect blob descriptors in select
                //sql += " ORDER BY id";

                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test_lob_multiple", sql));

                auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                while(blobRs->Next()) {
                    for (int pos = 1; pos <= 4; ++pos) {
                        ostream& out =
                            auto_cursor->GetBlobOStream(pos,
                                                        clob_value.size(),
                                                        eDisableLog
                                                        );
                        out.write(clob_value.data(), clob_value.size());
                        out.flush();
                        BOOST_CHECK(out.good());
                    }
                }
            }
        }

        // Retrieve data ...
        {
            sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
            sql += " ORDER BY id";

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while ( rs->Next() ) {
                        for (int pos = 1; pos <= 4; ++pos) {
                            const CVariant& value = rs->GetVariant(pos);

                            BOOST_CHECK( !value.IsNull() );

                            size_t blob_size = value.GetBlobSize();
                            BOOST_CHECK_EQUAL(clob_value.size(), blob_size);
                            BOOST_CHECK_EQUAL(value.GetString(), clob_value);
                        }
                    }
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB_LowLevel)
{
    string sql;
    CDB_Text txt;

    try {
        bool rc = false;
        auto_ptr<CDB_LangCmd> auto_stmt;
        CDB_Connection* conn = GetConnection().GetCDB_Connection();
        BOOST_CHECK(conn != NULL);

        // Clean table ...
        {
            sql = "DELETE FROM " + GetTableName();
            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);
            auto_stmt->DumpResults();
        }

        // Create descriptor ...
        {
            sql = "INSERT INTO " + GetTableName() + "(text_field, int_field) VALUES('', 1)";
            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);
            auto_stmt->DumpResults();
        }

        // Retrieve descriptor ...
        auto_ptr<I_ITDescriptor> descr;
        {
            sql = "SELECT text_field FROM " + GetTableName();
            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);

            // Retrieve descriptor ...
            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                while(rs->Fetch()) {
                    rs->ReadItem(NULL, 0);
                    descr.reset(rs->GetImageOrTextDescriptor());
                }
            }

            BOOST_CHECK(descr.get() != NULL);
        }

        // Send data ...
        {
            txt.Append("test clob data ...");
            conn->SendData(*descr, txt);
        }

        // Use CDB_ITDescriptor explicitely ..,
        {
            CDB_ITDescriptor descriptor(GetTableName(), "text_field", "int_field = 1");
            CDB_Text text;

            text.Append("test clob data ...");
            conn->SendData(descriptor, text);
        }

        // Create relatively big CLOB ...
        {
            CDB_ITDescriptor descriptor(GetTableName(), "text_field", "int_field = 1");
            CDB_Text text;

            for(int i = 0; i < 1000; ++i) {
                string str_value;

                str_value += "A quick brown fox jumps over the lazy dog, message #";
                str_value += NStr::IntToString(i);
                str_value += "\n";
                text.Append(str_value);
            }
        }

        // Read CLOB value back ...
        {
            sql = "SELECT text_field FROM " + GetTableName() + " WHERE int_field = 1";
            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);

            // Retrieve descriptor ...
            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                char buff[4096];
                while(rs->Fetch()) {
                    if (rs->ReadItem(buff, sizeof(buff)) < sizeof(buff)) {
                        break;
                    };
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_LOB_Multiple_LowLevel)
{
    const string table_name = "#test_lob_ll_multiple";
    static string clob_value("1234567890");
    string sql;
    // enum {num_of_records = 10};

    try {
        bool rc = false;
        auto_ptr<CDB_LangCmd> auto_stmt;
        CDB_Connection* conn = GetConnection().GetCDB_Connection();
        BOOST_CHECK(conn != NULL);

        // Prepare data ...
        {
            // Create table ...
            if (table_name[0] =='#') {
                sql =
                    "CREATE TABLE " + table_name + " ( \n"
                    "   id NUMERIC IDENTITY NOT NULL, \n"
                    "   text01  TEXT NULL, \n"
                    "   text02  TEXT NULL, \n"
                    "   image01 IMAGE NULL, \n"
                    "   image02 IMAGE NULL \n"
                    ") \n";

                auto_stmt.reset(conn->LangCmd(sql));
                rc = auto_stmt->Send();
                BOOST_CHECK(rc);
                auto_stmt->DumpResults();
            }

            // Insert data ...

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + table_name + "(text01, text02, image01, image02)";
                sql += " VALUES('', '', '', '')";

                auto_stmt.reset(conn->LangCmd(sql));
                rc = auto_stmt->Send();
                BOOST_CHECK(rc);
                auto_stmt->DumpResults();

                /*
                for (int i = 0; i < num_of_records; ++i) {
                    auto_stmt->ExecuteUpdate( sql );
                }
                */
            }

            // Update LOB value.
            {
                sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
                // With next line MS SQL returns incorrect blob descriptors in select
                //sql += " ORDER BY id";

                auto_ptr<CDB_CursorCmd> auto_cursor;
                auto_cursor.reset(conn->Cursor("test_lob_multiple_ll", sql));

                auto_ptr<CDB_Result> rs(auto_cursor->Open());
                BOOST_CHECK (rs.get() != NULL);

                auto_ptr<I_ITDescriptor> text01;
                auto_ptr<I_ITDescriptor> text02;
                auto_ptr<I_ITDescriptor> image01;
                auto_ptr<I_ITDescriptor> image02;

                while (rs->Fetch()) {
                    // ReadItem must not be called here
                    //rs->ReadItem(NULL, 0);
                    text01.reset(rs->GetImageOrTextDescriptor());
                    BOOST_CHECK(text01.get());
                    rs->SkipItem();

                    text02.reset(rs->GetImageOrTextDescriptor());
                    BOOST_CHECK(text02.get());
                    rs->SkipItem();

                    image01.reset(rs->GetImageOrTextDescriptor());
                    BOOST_CHECK(image01.get());
                    rs->SkipItem();

                    image02.reset(rs->GetImageOrTextDescriptor());
                    BOOST_CHECK(image02.get());
                    rs->SkipItem();
                }

                // Send data ...
                {
                    CDB_Text    obj_text;
                    CDB_Image   obj_image;

                    obj_text.Append(clob_value);
                    obj_image.Append(clob_value.data(), clob_value.size());

                    conn->SendData(*text01, obj_text);
                    obj_text.MoveTo(0);
                    conn->SendData(*text02, obj_text);
                    conn->SendData(*image01, obj_image);
                    obj_image.MoveTo(0);
                    conn->SendData(*image02, obj_image);
                }

            }
        }

        // Retrieve data ...
        {
            sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
            sql += " ORDER BY id";

            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);

            // Retrieve descriptor ...
            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                CDB_Stream* obj_lob = NULL;
                CDB_Text    obj_text;
                CDB_Image   obj_image;
                char        buffer[128];

                while (rs->Fetch()) {
                    for (int pos = 0; pos < 4; ++pos) {
                        switch (rs->ItemDataType(pos)) {
                            case eDB_Image:
                                obj_lob = &obj_image;
                                break;
                            case eDB_Text:
                                obj_lob = &obj_text;
                            default:
                                break;
                        };

                        // You have to call either
                        // obj_lob->Truncate();
                        // rs->GetItem(obj_lob);
                        // or
                        // rs->GetItem(obj_lob, I_Result::eAssignLOB);

                        BOOST_CHECK(obj_lob);

                        rs->GetItem(obj_lob, I_Result::eAssignLOB);

                        BOOST_CHECK( !obj_lob->IsNULL() );

                        size_t blob_size = obj_lob->Size();
                        BOOST_CHECK_EQUAL(clob_value.size(), blob_size);

                        size_t size_read = obj_lob->Read(buffer, sizeof(buffer));

                        BOOST_CHECK_EQUAL(clob_value, string(buffer, size_read));
                    }
                }
            }
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
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Prepare data ...
        {
            ostrstream out;

            for (int i = 0; i < test_size; ++i) {
                out << i << " ";
            }

            data_len = long(out.pcount());
            BOOST_CHECK(data_len > 0);

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

            // Insert data ...
            sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
            sql += " VALUES(0, '')";
            auto_stmt->ExecuteUpdate( sql );

            sql  = " SELECT text_field FROM " + GetTableName();

            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));

            // blobRs should be destroyed before auto_cursor ...
            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            while (blobRs->Next()) {
                ostream& ostrm = auto_cursor->GetBlobOStream(1,
                                                             data_len,
                                                             eDisableLog);

                ostrm.write(out.str(), data_len);
                out.freeze(false);

                BOOST_CHECK_EQUAL(ostrm.fail(), false);

                ostrm.flush();

                BOOST_CHECK_EQUAL(ostrm.fail(), false);
                BOOST_CHECK_EQUAL(ostrm.good(), true);
                // BOOST_CHECK_EQUAL(int(ostrm.tellp()), int(out.pcount()));
            }

            auto_stmt->SendSql( "SELECT datalength(text_field) FROM " +
                                GetTableName() );

            while (auto_stmt->HasMoreResults()) {
                if (auto_stmt->HasRows()) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );
                    write_data_len = rs->GetVariant(1).GetInt4();
                    BOOST_CHECK_EQUAL( data_len, write_data_len );
                    while (rs->Next()) {}
                }
            }
        }

        // Retrieve data ...
        {
            auto_stmt->ExecuteUpdate("set textsize 2000000");

            sql = "SELECT id, int_field, text_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while ( auto_stmt->HasMoreResults() ) {
                if ( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    try {
                        while (rs->Next()) {
                            istream& strm = rs->GetBlobIStream();
                            int j = 0;
                            for (int i = 0; i < test_size; ++i) {
                                strm >> j;

                                // if (i == 5000) {
                                //     DATABASE_DRIVER_ERROR( "Exception safety test.", 0 );
                                // }

                                BOOST_CHECK_EQUAL(strm.good(), true);
                                BOOST_CHECK_EQUAL(strm.eof(), false);
                                BOOST_CHECK_EQUAL(j, i);
                            }
                            long read_data_len = long(strm.tellg());
                            // Calculate a trailing space.
                            BOOST_CHECK_EQUAL(data_len, read_data_len + 1);
                        }
                    }
                    catch( const CDB_Exception& ex) {
                        _TRACE(ex);
                        rs->Close();
                    }
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_BlobStore)
{
    string sql;
    string table_name = "#TestBlobStore";

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create a table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "  id   INT, \n"
                "  cr_date   DATETIME DEFAULT getdate(), \n"
                " \n"
                "  -- columns needed to satisy blobstore.cpp classes \n"
                "  blob_num INT DEFAULT 0, -- always 0 (we need not accept data that does not fit into 1 row) \n"
                "  data0 IMAGE NULL, \n"
                "  data1 IMAGE NULL, \n"
                "  data2 IMAGE NULL, \n"
                "  data3 IMAGE NULL, \n"
                "  data4 IMAGE NULL \n"
//                 "  data0 TEXT NULL, \n"
//                 "  data1 TEXT NULL, \n"
//                 "  data2 TEXT NULL, \n"
//                 "  data3 TEXT NULL, \n"
//                 "  data4 TEXT NULL \n"
                ") "
                ;
            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert a row
        {
            auto_stmt->ExecuteUpdate("INSERT INTO " + table_name + " (id) values (66)");
        }

        {
            enum {
                IMAGE_BUFFER_SIZE = (64*1024*1024),
                // The number of dataN fields in the table
                BLOB_COL_COUNT = 5,
                // If you need to increase MAX_FILE_SIZE,
                // try increasing IMAGE_BUFFER_SIZE,
                // or alter table AgpFiles and BLOB_COL_COUNT.
                MAX_FILE_SIZE = (BLOB_COL_COUNT*IMAGE_BUFFER_SIZE)
                };

            string blob_cols[BLOB_COL_COUNT];

            for(int i=0; i<BLOB_COL_COUNT; i++) {
                blob_cols[i] = string("data") + NStr::IntToString(i);
            }

            CBlobStoreStatic blobrw(
                GetConnection().GetCDB_Connection(),
                table_name,     // tableName
                "id",           // keyColName
                "blob_num",     // numColName
                blob_cols,      // blobColNames
                BLOB_COL_COUNT, // nofBC - number of blob columns
                false,          // bool isText
                eNone, //eZLib, // ECompressMethod cm
                IMAGE_BUFFER_SIZE,
                false
            );

            // Write blob to the row ...
            {
                auto_ptr<ostream> pStream(blobrw.OpenForWrite( "66" ));
                BOOST_CHECK(pStream.get() != NULL);

                for(int i = 0; i < 10000; ++i) {
                    *pStream <<
                        "A quick brown fox jumps over the lazy dog, message #" <<
                        i << "\n";
                }

                pStream->flush();
            }

            // Read blob ...
            {
                auto_ptr<istream> pStream(blobrw.OpenForRead( "66" ));
                BOOST_CHECK(pStream.get() != NULL);

                string line;
                while (!pStream->eof()) {
                    NcbiGetline(*pStream, line, '\n');
                }
                // while (NcbiGetline(*pStream, line, "\r\n")) {
                //     ;
                // }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Based on Soussov's API.
BOOST_AUTO_TEST_CASE(Test_Iskhakov)
{
    string sql;

    try {
        CDBSetConnParams params(
            "LINK_OS_INTERNAL",
            "anyone",
            "allowed",
            125,
            GetArgs().GetConnParams()
            );

        auto_ptr<CDB_Connection> auto_conn(
            GetDS().GetDriverContext()->MakeConnection(params)
        );

        BOOST_CHECK( auto_conn.get() != NULL );

        sql  = "get_iodesc_for_link pubmed_pubmed";

        auto_ptr<CDB_LangCmd> auto_stmt( auto_conn->LangCmd(sql) );
        BOOST_CHECK( auto_stmt.get() != NULL );

        bool rc = auto_stmt->Send();
        BOOST_CHECK( rc );

        auto_ptr<I_ITDescriptor> descr;

        while(auto_stmt->HasMoreResults()) {
            auto_ptr<CDB_Result> rs(auto_stmt->Result());

            if (rs.get() == NULL) {
                continue;
            }

            if (rs->ResultType() != eDB_RowResult) {
                continue;
            }

            while(rs->Fetch()) {
                rs->ReadItem(NULL, 0);

                descr.reset(rs->GetImageOrTextDescriptor());
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


BOOST_AUTO_TEST_CASE(Test_Empty_Blob)
{
    const string             kTableName = "#TestEmptyBlob";
    static const char* const kValues[]  = { NULL, kEmptyCStr, " ", "  " };
    const int                kValueCount = sizeof(kValues) / sizeof(*kValues);
    static const char* const kTypes[][2] = {
        { "IMAGE", "TEXT", },
        { "VARBINARY(MAX)", "VARCHAR(MAX)" }
    };
    const int                kTypesCount = sizeof(kTypes) / sizeof(*kTypes);

    try {
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

        for (int k = 0;  k < kTypesCount;  ++k) {
            auto_stmt->ExecuteUpdate("CREATE TABLE " + kTableName
                                     + "(i1 " + kTypes[k][0] + ","
                                       " t1 " + kTypes[k][1] + ","
                                       " i INTEGER NOT NULL,"
                                       " j INTEGER NOT NULL,"
                                       " i2 " + kTypes[k][0] + ","
                                       " t2 " + kTypes[k][1] + ")");

            auto_ptr<IBulkInsert> bi
                (GetConnection().GetBulkInsert(kTableName));
            for (int i = 0;  i < kValueCount;  ++i) {
                for (int j = 0;  j < kValueCount;  ++j) {
                    CVariant i1(eDB_Image), t1(eDB_Text), vi(i), vj(j),
                             i2(eDB_Image), t2(eDB_Text);
                    if (i > 0) {
                        i1.Append(kValues[i], i - 1);
                        i2 = i1;
                    }
                    BOOST_CHECK_EQUAL(i1.IsNull(), i == 0);
                    BOOST_CHECK_EQUAL(i2.IsNull(), i == 0);
                    if (j > 0) {
                        t1.Append(kValues[j], j - 1);
                        t2 = t1;
                    }
                    BOOST_CHECK_EQUAL(t1.IsNull(), j == 0);
                    BOOST_CHECK_EQUAL(t2.IsNull(), j == 0);
                    bi->Bind(1, &i1);
                    bi->Bind(2, &t1);
                    bi->Bind(3, &vi);
                    bi->Bind(4, &vj);
                    bi->Bind(5, &i2);
                    bi->Bind(6, &t2);
                    bi->AddRow();
                }
            }
            bi->Complete();

            auto_stmt->SendSql("SELECT * FROM " + kTableName);
            while (auto_stmt->HasMoreResults()) {
                if (auto_stmt->HasRows()) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    rs->BindBlobToVariant(true);
                    while (rs->Next()) {
                        size_t i = rs->GetVariant(3).GetInt4();
                        size_t j = rs->GetVariant(4).GetInt4();

                        const CVariant &i1 = rs->GetVariant(1);
                        const CVariant &t1 = rs->GetVariant(2);
                        const CVariant &i2 = rs->GetVariant(5);
                        const CVariant &t2 = rs->GetVariant(6);

                        if (i == 0) {
                            BOOST_CHECK(i1.IsNull());
                            BOOST_CHECK(i2.IsNull());
                        } else {
                            BOOST_CHECK( !i1.IsNull() );
                            BOOST_CHECK_EQUAL(i1.GetString(),
                                              string(kValues[i], i - 1));

                            BOOST_CHECK( !i2.IsNull() );
                            BOOST_CHECK_EQUAL(i2.GetString(),
                                              string(kValues[i], i - 1));
                        }

                        if (j == 0) {
                            BOOST_CHECK(t1.IsNull());
                            BOOST_CHECK(t2.IsNull());
                        } else {
                            BOOST_CHECK( !t1.IsNull() );
                            BOOST_CHECK_EQUAL(t1.GetString(),
                                              string(kValues[j], j - 1));
                            BOOST_CHECK( !t2.IsNull() );
                            BOOST_CHECK_EQUAL(t2.GetString(),
                                              string(kValues[j], j-1));
                        }
                    }
                }
            }
            auto_stmt->ExecuteUpdate("DROP TABLE " + kTableName);
        }
    } catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


END_NCBI_SCOPE
