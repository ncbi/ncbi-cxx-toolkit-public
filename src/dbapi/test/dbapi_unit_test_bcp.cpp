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
BOOST_AUTO_TEST_CASE(Test_DateTimeBCP)
{
    string table_name("#test_bcp_datetime");
    // string table_name("DBAPI_Sample..test_bcp_datetime");
    string sql;
    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
    CVariant value(eDB_DateTime);
    CTime t;
    CVariant null_date(t, eLong);
    CTime dt_value;

    try {
        // Initialization ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id INT, \n"
                "   dt_field DATETIME NULL \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data using BCP ...
        {
            value = CTime(CTime::eCurrent);

            // Insert data using parameters ...
            {
                CVariant col1(eDB_Int);
                CVariant col2(eDB_DateTime);

                auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name);

                auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);

                col1 = 1;
                col2 = value;

                bi->AddRow();
                bi->Complete();

                auto_stmt->ExecuteUpdate("SELECT * FROM " + table_name);
                int nRows = auto_stmt->GetRowCount();
                BOOST_CHECK_EQUAL(nRows, 1);
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM " + table_name;

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );

                const CVariant& value2 = rs->GetVariant(2);

                BOOST_CHECK( !value2.IsNull() );

                const CTime& dt_value2 = value2.GetCTime();
                BOOST_CHECK( !dt_value2.IsEmpty() );
                CTime dt_value3 = value.GetCTime();
                BOOST_CHECK_EQUAL( dt_value2.AsString(), dt_value3.AsString() );

                // Tracing ...
                if (dt_value2.AsString() != dt_value3.AsString()) {
                    cout << "dt_value2 nanoseconds = " << dt_value2.NanoSecond()
                        << " dt_value3 nanoseconds = " << dt_value3.NanoSecond()
                        << endl;
                }

                // Failed for some reason ...
                // BOOST_CHECK(dt_value2 == dt_value);
            }

            // Insert NULL data using parameters ...
            {
                CVariant col1(eDB_Int);
                CVariant col2(eDB_DateTime);

                auto_stmt->ExecuteUpdate("DELETE FROM " + table_name);

                auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);

                col1 = 1;

                bi->AddRow();
                bi->Complete();

                auto_stmt->ExecuteUpdate("SELECT * FROM " + table_name);
                int nRows = auto_stmt->GetRowCount();
                BOOST_CHECK_EQUAL(nRows, 1);
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM " + table_name;

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );

                const CVariant& value2 = rs->GetVariant(2);

                BOOST_CHECK( value2.IsNull() );
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_BulkInsertBlob)
{
    string sql;
    enum { record_num = 100 };
    // string table_name = GetTableName();
    string table_name = "#dbapi_bcp_table2";

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // First test ...
        {
            // Prepare data ...
            {
                // Clean table ...
                auto_stmt->ExecuteUpdate("DELETE FROM "+ table_name);
            }

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(GetConnection().CreateBulkInsert(table_name));
                CVariant col1(eDB_Int);
                CVariant col2(eDB_Int);
                CVariant col3(eDB_VarChar);
                CVariant col4(eDB_Text);
                // CVariant col4(eDB_VarChar);

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);
                bi->Bind(3, &col3);
                bi->Bind(4, &col4);

                for( int i = 0; i < record_num; ++i ) {
                    string im = NStr::IntToString(i);
                    col1 = i;
                    col2 = i;
                    col3 = im;
                    // col4 = im;
                    col4.Truncate();
                    col4.Append(im.data(), im.size());

                    bi->AddRow();
                }

                // bi->StoreBatch();
                bi->Complete();
            }

            // Check inserted data ...
            {
                size_t rec_num = GetNumOfRecords(auto_stmt, table_name);
                BOOST_CHECK_EQUAL(rec_num, (size_t)record_num);
            }
        }

        // Second test ...
        {
            enum { batch_num = 10 };
            // Prepare data ...
            {
                // Clean table ...
                auto_stmt->ExecuteUpdate("DELETE FROM "+ table_name);
            }

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(GetConnection().CreateBulkInsert(table_name));
                CVariant col1(eDB_Int);
                CVariant col2(eDB_Int);
                CVariant col3(eDB_VarChar);
                CVariant col4(eDB_Text);

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);
                bi->Bind(3, &col3);
                bi->Bind(4, &col4);

                for (int j = 0; j < batch_num; ++j) {
                    for( int i = 0; i < record_num; ++i ) {
                        string im = NStr::IntToString(i);
                        col1 = i;
                        col2 = i;
                        col3 = im;
                        col4.Truncate();
                        col4.Append(im.data(), im.size());
                        bi->AddRow();
                    }

                    bi->StoreBatch();
                }

                bi->Complete();
            }

            // Check inserted data ...
            {
                size_t rec_num = GetNumOfRecords(auto_stmt, table_name);
                BOOST_CHECK_EQUAL(rec_num, size_t(record_num) * size_t(batch_num));
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_BulkInsertBlob_LowLevel)
{
    string sql;
    // enum { record_num = 10 };
    string table_name = "#bcp_table1";
    const string data = "testing";

    try {
        CDB_Connection* conn = GetConnection().GetCDB_Connection();

        // Create table ...
        {
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    geneId INT NOT NULL, \n";
            sql += "    dataImage IMAGE NULL, \n";
            sql += "    dataText TEXT NULL \n";
            sql += " )";

            auto_ptr<CDB_LangCmd> auto_stmt(conn->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Insert not-null value ...
        {
            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(2, &dataTextVal);
                bcp->Bind(1, &dataImageVal);

                geneIdVal = 2;

                dataTextVal.Append(data);
                dataTextVal.MoveTo(0);

                bcp->SendRow();
                bcp->CompleteBCP();
            }

            // Retrieve data back ...
            {
                string result;
                char buff[64];
                int rec_num = 0;

                sql = "SELECT dataText, dataImage FROM "+ table_name;

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
                        ++rec_num;

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK(!is_null);
                        BOOST_CHECK_EQUAL(result, data);

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK(is_null);
                    }
                    BOOST_CHECK_EQUAL(rec_num, 1);

                }
            }
        }


        // Insert null-value ...
        {
            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(2, &dataTextVal);
                bcp->Bind(1, &dataImageVal);

                geneIdVal = 2;

                dataTextVal.AssignNULL();

                bcp->SendRow();
                bcp->CompleteBCP();
            }

            // Retrieve data back ...
            {
                string result;
                char buff[64];
                int rec_num = 0;

                sql = "SELECT dataText FROM "+ table_name;

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
                        ++rec_num;

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK(is_null);
                    }
                    BOOST_CHECK_EQUAL(rec_num, 1);

                }
            }
        }

        // Insert two values ...
        {
            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(2, &dataTextVal);
                bcp->Bind(1, &dataImageVal);

                // First row ...
                geneIdVal = 2;

                dataTextVal.AssignNULL();

                bcp->SendRow();

                // Second row ...
                geneIdVal = 1;

                dataTextVal.Append(data);
                dataTextVal.MoveTo(0);

                bcp->SendRow();

                // Complete transaction ...
                bcp->CompleteBCP();
            }

            // Retrieve data back ...
            {
                string result;
                char buff[64];

                sql = "SELECT dataText FROM "+ table_name + " ORDER BY geneId";

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

                    size_t read_bytes = 0;
                    bool is_null = true;

                    // First record ...
                    BOOST_CHECK(rs->Fetch());
                    if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                        result += string(buff, read_bytes);
                    }

                    BOOST_CHECK(!is_null);
                    BOOST_CHECK_EQUAL(result, data);

                    // Second record ...
                    read_bytes = 0;
                    is_null = true;

                    BOOST_CHECK(rs->Fetch());
                    if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                        result += string(buff, read_bytes);
                    }

                    BOOST_CHECK(is_null);
                }
            }
        }


        // Check NULL-values ...
        {
            int num_of_tests = 10;

            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(2, &dataTextVal);
                bcp->Bind(1, &dataImageVal);

                for(int i = 0; i < num_of_tests; ++i ) {
                    geneIdVal = i;

                    if (i % 2 == 0) {
                        dataTextVal.Append(data);
                        dataTextVal.MoveTo(0);
                    } else {
                        dataTextVal.AssignNULL();
                    }

                    bcp->SendRow();
                }

                bcp->CompleteBCP();
            }

            // Check inserted data ...
            {
                char buff[64];

                sql = "SELECT dataText FROM "+ table_name;

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

                    for(int i = 0; i < num_of_tests; ++i ) {
                        string result;
                        size_t read_bytes = 0;
                        bool is_null = true;

                        BOOST_CHECK(rs->Fetch());

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                            result += string(buff, read_bytes);
                        }


                        if (i % 2 == 0) {
                            BOOST_CHECK(!is_null);
                            BOOST_CHECK_EQUAL(result, data);
                        } else {
                            BOOST_CHECK(is_null);
                        }
                    }
                }
            }
        }

        // Check NULL-values in a different way ...
        if (false) {
            int num_of_tests = 10;

            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(1, &dataTextVal);
                bcp->Bind(2, &dataImageVal);

                for(int i = 0; i < num_of_tests; ++i ) {
                    geneIdVal = i;

                    if (i % 2 == 0) {
                        dataTextVal.Append(data);
                        dataTextVal.MoveTo(0);

                        dataImageVal.AssignNULL();
                    } else {
                        dataTextVal.AssignNULL();

                        dataImageVal.Append((void*)data.data(), data.size());
                        dataImageVal.MoveTo(0);
                    }

                    bcp->SendRow();
                }

                bcp->CompleteBCP();
            }

            // Check inserted data ...
            if (false) {
                char buff[64];

                sql = "SELECT dataText FROM "+ table_name;

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

                    for(int i = 0; i < num_of_tests; ++i ) {
                        string result;
                        size_t read_bytes = 0;
                        bool is_null = true;

                        BOOST_CHECK(rs->Fetch());

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                            result += string(buff, read_bytes);
                        }


                        if (i % 2 == 0) {
                            BOOST_CHECK(!is_null);
                            BOOST_CHECK_EQUAL(result, data);
                        } else {
                            BOOST_CHECK(is_null);
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
BOOST_AUTO_TEST_CASE(Test_BulkInsertBlob_LowLevel2)
{
    string sql;
    // enum { record_num = 10 };
    string table_name = "#bcp_table2";
    string data;

    data =
        "9606    2       GO:0019899      IPI     -       enzyme binding  11435418        Function"
        "9606    2       GO:0019966      IDA     -       interleukin-1 binding   9714181 Function"
        "9606    2       GO:0019959      IPI     -       interleukin-8 binding   10880251        Function"
        "9606    2       GO:0008320      NR      -       protein carrier activity        -       Function"
        "9606    2       GO:0004867      IEA     -       serine-type endopeptidase inhibitor activity    -       Function"
        "9606    2       GO:0043120      IDA     -       tumor necrosis factor binding   9714181 Function"
        "9606    2       GO:0017114      IEA     -       wide-spectrum protease inhibitor activity       -       Function"
        "9606    2       GO:0006886      NR      -       intracellular protein transport -       Process"
        "9606    2       GO:0051260      NAS     -       protein homooligomerization     -       Process"
        "9606    2       GO:0005576      NAS     -       extracellular region    14718574        Component";
    // data = "testing";

    try {
        CDB_Connection* conn = GetConnection().GetCDB_Connection();
        const size_t first_ind = 9;
        // const size_t first_ind = 0;

        // Create table ...
        {
            sql  = " CREATE TABLE " + table_name + "( \n";

            if(first_ind) {
                sql += "    vkey int NOT NULL , \n";
                sql += "    geneId int NOT NULL , \n";
                sql += "    modate datetime NOT NULL , \n";
                sql += "    dtype int NOT NULL , \n";
                sql += "    dsize int NOT NULL , \n";
                sql += "    dataStr varchar (255) NULL , \n";
                sql += "    dataInt int NULL , \n";
                sql += "    dataBin varbinary (255) NULL , \n";
                sql += "    cnt int NOT NULL , \n";
            }

            sql += "    dataText text NULL , \n";
            sql += "    dataImg image NULL  \n";
            sql += " )";

            auto_ptr<CDB_LangCmd> auto_stmt(conn->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Insert data ...
        if (true) {

            auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

            // Declare data-holders ...
            CDB_Int vkeyVal;
            vkeyVal = 1;

            CDB_Int geneIdVal;
            geneIdVal = 2;

            CTime curr_time(CTime::eCurrent);
            CDB_DateTime modateVal(curr_time);

            CDB_Int dtypeVal;
            dtypeVal = 106;

            CDB_Int dsizeVal;
            dsizeVal = Int4(data.size());

            CDB_VarChar dataStrVal;
            dataStrVal.AssignNULL();

            CDB_Int dataIntVal;
            dataIntVal = 0;

            CDB_VarBinary dataBinVal;
            dataBinVal.AssignNULL();

            CDB_Int cntVal;
            cntVal = 1;

            CDB_Text dataTextVal;

            CDB_Image dataImgVal;


            // Bind ...
            if (first_ind) {
                bcp->Bind(0, &vkeyVal);
                bcp->Bind(1, &geneIdVal);
                bcp->Bind(2, &modateVal);
                bcp->Bind(3, &dtypeVal);
                bcp->Bind(4, &dsizeVal);
                bcp->Bind(5, &dataStrVal);
                bcp->Bind(6, &dataIntVal);
                bcp->Bind(7, &dataBinVal);
                bcp->Bind(8, &cntVal);
            }

            dataTextVal.AssignNULL();
            // doesn't matter, null or not null data both fail
            //    dataTextVal.Append(data);
            //    dataTextVal.MoveTo(0);
            bcp->Bind(first_ind, &dataTextVal);

            dataImgVal.AssignNULL();
            bcp->Bind(first_ind + 1, &dataImgVal);

            bcp->SendRow();

            bcp->CompleteBCP();
        }

        // Delete previously inserted data ...
        {
            sql = "DELETE FROM " + table_name;

            auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Insert data again ...
        if (true) {

            auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

            // Declare data-holders ...
            CDB_Int vkeyVal;
            vkeyVal = 1;

            CDB_Int geneIdVal;
            geneIdVal = 2;

            CTime curr_time(CTime::eCurrent);
            CDB_DateTime modateVal(curr_time);

            CDB_Int dtypeVal;
            dtypeVal = 106;

            CDB_Int dsizeVal;
            dsizeVal = Int4(data.size());

            CDB_VarChar dataStrVal;
            dataStrVal.AssignNULL();

            CDB_Int dataIntVal;
            dataIntVal = 0;

            CDB_VarBinary dataBinVal;
            dataBinVal.AssignNULL();

            CDB_Int cntVal;
            cntVal = 1;

            CDB_Text dataTextVal;

            CDB_Image dataImgVal;


            // Bind ...
            if (first_ind) {
                bcp->Bind(0, &vkeyVal);
                bcp->Bind(1, &geneIdVal);
                bcp->Bind(2, &modateVal);
                bcp->Bind(3, &dtypeVal);
                bcp->Bind(4, &dsizeVal);
                bcp->Bind(5, &dataStrVal);
                bcp->Bind(6, &dataIntVal);
                bcp->Bind(7, &dataBinVal);
                bcp->Bind(8, &cntVal);
            }

            // dataTextVal.AssignNULL();
            // doesn't matter, null or not null data both fail
            dataTextVal.Append(data);
            dataTextVal.MoveTo(0);
            bcp->Bind(first_ind, &dataTextVal);

            dataImgVal.AssignNULL();
            bcp->Bind(first_ind + 1, &dataImgVal);

            /*
            CDB_Text dataTextVal;
            // dataTextVal.AssignNULL();
            // doesn't matter, null or not null data both fail
            dataTextVal.Append(data);
            dataTextVal.MoveTo(0);
            bcp->Bind(0, &dataTextVal);

            CDB_Image dataImgVal;
            dataImgVal.AssignNULL();
            bcp->Bind(1, &dataImgVal);
            */

            bcp->SendRow();

            bcp->CompleteBCP();
        }

        // Delete previously inserted data ...
        {
            sql = "DELETE FROM " + table_name;

            auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Third scenario ...
        if (GetArgs().GetDriverName() != ctlib_driver) {
            CDB_Int vkeyVal;
            vkeyVal = 1;

            CDB_Int geneIdVal;
            geneIdVal = 2;

            CTime curr_time(CTime::eCurrent);
            CDB_DateTime modateVal(curr_time);

            CDB_Int dtypeVal;
            dtypeVal = 106;

            CDB_Int dsizeVal;
            dsizeVal = Int4(data.size());

            CDB_VarChar dataStrVal;
            dataStrVal.AssignNULL();

            CDB_Int dataIntVal;
            dataIntVal = 0;

            CDB_VarBinary dataBinVal;
            dataBinVal.AssignNULL();

            CDB_Int cntVal;
            cntVal = 1;

            CDB_Text dataTextVal;
            dataTextVal.AssignNULL();

            CDB_Image dataImgVal;
            dataImgVal.AssignNULL();

            bool have_img = true;

            // case 1: text data is null;
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                if (first_ind) {
                    bcp->Bind(0, &vkeyVal);
                    bcp->Bind(1, &geneIdVal);
                    bcp->Bind(2, &modateVal);
                    bcp->Bind(3, &dtypeVal);
                    bcp->Bind(4, &dsizeVal);
                    bcp->Bind(5, &dataStrVal);
                    bcp->Bind(6, &dataIntVal);
                    bcp->Bind(7, &dataBinVal);
                    bcp->Bind(8, &cntVal);
                }

                bcp->Bind(first_ind, &dataTextVal);
                if ( have_img ) {
                    bcp->Bind(first_ind + 1, &dataImgVal);
                }

                bcp->SendRow();

                bcp->CompleteBCP();
            }

            // case 2: text data is not null
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                if (first_ind) {
                    bcp->Bind(0, &vkeyVal);
                    bcp->Bind(1, &geneIdVal);
                    bcp->Bind(2, &modateVal);
                    bcp->Bind(3, &dtypeVal);
                    bcp->Bind(4, &dsizeVal);
                    bcp->Bind(5, &dataStrVal);
                    bcp->Bind(6, &dataIntVal);
                    bcp->Bind(7, &dataBinVal);
                    bcp->Bind(8, &cntVal);
                }

                bcp->Bind(first_ind, &dataTextVal);
                if ( have_img ) {
                    bcp->Bind(first_ind + 1, &dataImgVal);
                }


                dataTextVal.Append(data);
                dataTextVal.MoveTo(0);

                bcp->SendRow();

                bcp->CompleteBCP();
            }


            // case 3: text data is null but img is not null
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                if (first_ind) {
                    bcp->Bind(0, &vkeyVal);
                    bcp->Bind(1, &geneIdVal);
                    bcp->Bind(2, &modateVal);
                    bcp->Bind(3, &dtypeVal);
                    bcp->Bind(4, &dsizeVal);
                    bcp->Bind(5, &dataStrVal);
                    bcp->Bind(6, &dataIntVal);
                    bcp->Bind(7, &dataBinVal);
                    bcp->Bind(8, &cntVal);
                }

                bcp->Bind(first_ind, &dataTextVal);
                bcp->Bind(first_ind + 1, &dataImgVal);

                dataTextVal.AssignNULL();
                dataImgVal.Append(data.data(), data.size()); // pretend this data is bi
                dataImgVal.MoveTo(0);

                bcp->SendRow();

                bcp->CompleteBCP();
            }

        } else {
            GetArgs().PutMsgDisabled("Test_BulkInsertBlob_LowLevel2 - third scenario");
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_BCP_Cancel)
{
    string sql;

    try {
        auto_ptr<IConnection> conn(
                GetDS().CreateConnection(CONN_OWNERSHIP)
                );
        conn->Connect(GetArgs().GetConnParams());

        auto_ptr<IStatement> auto_stmt( conn->GetStatement() );

        // Initialize ...
        // Block 1 ...
        {
            sql =
                "CREATE TABLE #test_bcp_cancel ( \n"
                "   int_field int \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data ...
        // Block 2 ...
        {
            auto_ptr<IBulkInsert> bi(
                conn->GetBulkInsert("#test_bcp_cancel")
                );

            CVariant col1(eDB_Int);

            bi->Bind(1, &col1);

            for (int i = 0; i < 10; ++i) {
                col1 = i;
                bi->AddRow();
            }
            bi->StoreBatch();
            for (int i = 0; i < 10; ++i) {
                col1 = i;
                bi->AddRow();
            }
            bi->Cancel();
        }

        // Retrieve data ...
        // Block 3 ...
        if (true) {
            BOOST_CHECK_EQUAL( GetNumOfRecords(auto_stmt, "#test_bcp_cancel"), size_t(10) );
        }

        // Initialize ...
        // Block 4 ...
        if (true) {
            sql = "CREATE INDEX test_bcp_cancel_ind ON #test_bcp_cancel (int_field)";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data ...
        // Block 5 ...
        if (true) {
            auto_ptr<IBulkInsert> bi(
                conn->GetBulkInsert("#test_bcp_cancel")
                );

            CVariant col1(eDB_Int);

            bi->Bind(1, &col1);

            for (int i = 0; i < 10; ++i) {
                col1 = i;
                bi->AddRow();
            }
            bi->StoreBatch();
            for (int i = 0; i < 10; ++i) {
                col1 = i;
                bi->AddRow();
            }
            bi->Cancel();
        }

        // Retrieve data ...
        // Block 6 ...
        if (true) {
            BOOST_CHECK_EQUAL( GetNumOfRecords(auto_stmt, "#test_bcp_cancel"), size_t(20) );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Overflow)
{
    string sql;
    enum {column_size = 32, data_size = 64};

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Initialize ...
        {
            sql =
                "CREATE TABLE #test_bulk_overflow ( \n"
                "   vc32_field VARCHAR(32) \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        string str_data = string(data_size, 'O');

        // Insert data ...
        {
            bool exception_catched = false;
            auto_ptr<IBulkInsert> bi(
                GetConnection().GetBulkInsert("#test_bulk_overflow")
                );

            CVariant col1(eDB_VarChar, data_size);

            bi->Bind(1, &col1);

            col1 = str_data;

            // Neither AddRow() nor Complete() should throw an exception.
            try
            {
                bi->AddRow();
                bi->Complete();
            } catch(const CDB_Exception& ex)
            {
                _TRACE(ex);
                exception_catched = true;
            }

            if (GetArgs().GetDriverName() == dblib_driver) {
                GetArgs().PutMsgDisabled("Unexceptional overflow of varchar in bulk-insert");

                if ( !exception_catched ) {
                    BOOST_FAIL("Exception CDB_ClientEx was expected.");
                }
            }
            else {
                if ( exception_catched ) {
                    BOOST_FAIL("Exception CDB_ClientEx was not expected.");
                }
            }
        }

        // Retrieve data ...
        if (GetArgs().GetDriverName() != dblib_driver)
        {
            sql = "SELECT * FROM #test_bulk_overflow";

            auto_stmt->SendSql( sql );
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

            BOOST_CHECK( rs.get() );
            BOOST_CHECK( rs->Next() );

            const CVariant& value = rs->GetVariant(1);

            BOOST_CHECK( !value.IsNull() );

            string str_value = value.GetString();
            BOOST_CHECK_EQUAL( string::size_type(column_size), str_value.size() );
        }

        // Initialize ...
        {
            sql = "DROP TABLE #test_bulk_overflow";

            auto_stmt->ExecuteUpdate( sql );

            sql =
                "CREATE TABLE #test_bulk_overflow ( \n"
                "   vb32_field VARBINARY(32) \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data ...
        {
            bool exception_catched = false;
            auto_ptr<IBulkInsert> bi(
                GetConnection().GetBulkInsert("#test_bulk_overflow")
                );

            CVariant col1(eDB_VarBinary, data_size);

            bi->Bind(1, &col1);

            col1 = CVariant::VarBinary(str_data.data(), str_data.size());

            // Here either AddRow() or Complete() should throw an exception.
            try
            {
                bi->AddRow();
                bi->Complete();
            } catch(const CDB_Exception& ex)
            {
                _TRACE(ex);
                exception_catched = true;
            }

            if (GetArgs().GetDriverName() == ctlib_driver) {
                GetArgs().PutMsgDisabled("Exception when overflow of varbinary in bulk-insert");

                if ( exception_catched ) {
                    BOOST_FAIL("Exception CDB_ClientEx was not expected.");
                }
            }
            else {
                if ( !exception_catched ) {
                    BOOST_FAIL("Exception CDB_ClientEx was expected.");
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
static
void
BulkAddRow(const auto_ptr<IBulkInsert>& bi, const CVariant& col)
{
    string msg(8000, 'A');
    CVariant col2(col);

    try {
        bi->Bind(2, &col2);
        col2 = msg;
        bi->AddRow();
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing)
{
    string sql;
    string table_name = "#bin_bulk_insert_table";
    // string table_name = "DBAPI_Sample..bin_bulk_insert_table";

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        if (table_name[0] == '#') {
            // Table for bulk insert ...
            if ( GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) {
                sql  = " CREATE TABLE " + table_name + "( \n";
                sql += "    id INT PRIMARY KEY, \n";
                sql += "    vc8000_field VARCHAR(8000) NULL, \n";
                sql += "    vb8000_field VARBINARY(8000) NULL, \n";
                sql += "    int_field INT NULL, \n";
                sql += "    bigint_field BIGINT NULL \n";
                sql += " )";

                // Create the table
                auto_stmt->ExecuteUpdate(sql);
            } else
            {
                sql  = " CREATE TABLE " + table_name + "( \n";
                sql += "    id INT PRIMARY KEY, \n";
                sql += "    vc8000_field VARCHAR(1900) NULL, \n";
                sql += "    vb8000_field VARBINARY(1900) NULL, \n";
                sql += "    int_field INT NULL \n";
                sql += " )";

                // Create the table
                auto_stmt->ExecuteUpdate(sql);
            }
        }


        // VARBINARY ...
        {
            enum { num_of_tests = 10 };
            const char char_val('2');

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

                CVariant col1(eDB_Int);
                CVariant col2(eDB_VarChar);
                CVariant col3(eDB_LongBinary, GetMaxVarcharSize());

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);
                bi->Bind(3, &col3);

                for(int i = 0; i < num_of_tests; ++i ) {
                    int int_value = GetMaxVarcharSize() / num_of_tests * i;
                    string str_val(int_value , char_val);

                    col1 = int_value;
                    col3 = CVariant::LongBinary(GetMaxVarcharSize(),
                                                str_val.data(),
                                                str_val.size()
                                                );
                    bi->AddRow();
                }
                bi->Complete();
            }

            // Retrieve data ...
            // Some drivers limit size of text/binary to 255 bytes ...
            if ( GetArgs().GetDriverName() != dblib_driver
                ) {
                auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

                sql  = " SELECT id, vb8000_field FROM " + table_name;
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );

                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );

                for(int i = 0; i < num_of_tests; ++i ) {
                    BOOST_CHECK( rs->Next() );

                    int int_value = GetMaxVarcharSize() / num_of_tests * i;
                    Int4 id = rs->GetVariant(1).GetInt4();
                    string vb8000_value = rs->GetVariant(2).GetString();

                    BOOST_CHECK_EQUAL( int_value, id );
                    BOOST_CHECK_EQUAL(string::size_type(int_value),
                                      vb8000_value.size()
                                      );
                }

                // Dump results ...
                DumpResults( auto_stmt.get() );
            } else {
                // GetArgs().PutMsgDisabled("Test_Bulk_Writing Retrieve data");
            }
        }

        // INT, BIGINT
        {

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

            // INT collumn ...
            {
                enum { num_of_tests = 8 };

                // Insert data ...
                {
                    auto_ptr<IBulkInsert> bi(
                        GetConnection().GetBulkInsert(table_name)
                        );

                    CVariant col1(eDB_Int);
                    CVariant col2(eDB_Int);

                    bi->Bind(1, &col1);
                    bi->Bind(4, &col2);

                    for(int i = 0; i < num_of_tests; ++i ) {
                        col1 = i;
                        Int4 value = Int4( 1 ) << (i * 4);
                        col2 = value;
                        bi->AddRow();
                    }
                    bi->Complete();
                }

                // Retrieve data ...
                {
                    sql  = " SELECT int_field FROM " + table_name;
                    sql += " ORDER BY id";

                    auto_stmt->SendSql( sql );

                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );

                    for(int i = 0; i < num_of_tests; ++i ) {
                        BOOST_CHECK( rs->Next() );
                        Int4 value = rs->GetVariant(1).GetInt4();
                        Int4 expected_value = Int4( 1 ) << (i * 4);
                        BOOST_CHECK_EQUAL( expected_value, value );
                    }

                    // Dump results ...
                    DumpResults( auto_stmt.get() );
                }
            }

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

            // BIGINT collumn ...
            // Sybase doesn't have BIGINT data type ...
            if (GetArgs().GetServerType() != CDBConnParams::eSybaseSQLServer)
            {
                enum { num_of_tests = 16 };

                // Insert data ...
                {
                    auto_ptr<IBulkInsert> bi(
                        GetConnection().GetBulkInsert(table_name)
                        );

                    CVariant col1(eDB_Int);
                    CVariant col2(eDB_BigInt);
                    //CVariant col_tmp2(eDB_VarChar);
                    //CVariant col_tmp3(eDB_Int);

                    bi->Bind(1, &col1);
                    //bi->Bind(2, &col_tmp2);
                    //bi->Bind(3, &col_tmp3);
                    bi->Bind(5, &col2);

                    for(int i = 0; i < num_of_tests; ++i ) {
                        col1 = i;
                        Int8 value = Int8( 1 ) << (i * 4);
                        col2 = value;
                        bi->AddRow();
                    }
                    bi->Complete();
                }

                // Retrieve data ...
                {
                    sql  = " SELECT bigint_field FROM " + table_name;
                    sql += " ORDER BY id";

                    auto_stmt->SendSql( sql );

                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );

                    for(int i = 0; i < num_of_tests; ++i ) {
                        BOOST_CHECK( rs->Next() );
                        Int8 value = rs->GetVariant(1).GetInt8();
                        Int8 expected_value = Int8( 1 ) << (i * 4);
                        BOOST_CHECK_EQUAL( expected_value, value );
                    }

                    // Dump results ...
                    DumpResults( auto_stmt.get() );
                }
            }
            else {
                GetArgs().PutMsgDisabled("Bigint in Sybase");
            }
        }

        // Yet another BIGINT test (and more) ...
        // Sybase doesn't have BIGINT data type ...
        if (GetArgs().GetServerType() != CDBConnParams::eSybaseSQLServer)
        {
            auto_ptr<IStatement> stmt( GetConnection().CreateStatement() );

            // Create table ...
            {
                stmt->ExecuteUpdate(
                    // "create table #__blki_test ( name char(32) not null, value bigint null )" );
                    "create table #__blki_test ( name char(32), value bigint null )"
                    );
                stmt->Close();
            }

            // First test ...
            {
                auto_ptr<IBulkInsert> blki(
                    GetConnection().CreateBulkInsert("#__blki_test")
                    );

                CVariant col1(eDB_Char,32);
                CVariant col2(eDB_BigInt);

                blki->Bind(1, &col1);
                blki->Bind(2, &col2);

                col1 = "Hello-1";
                col2 = Int8( 123 );
                blki->AddRow();

                col1 = "Hello-2";
                col2 = Int8( 1234 );
                blki->AddRow();

                col1 = "Hello-3";
                col2 = Int8( 12345 );
                blki->AddRow();

                col1 = "Hello-4";
                col2 = Int8( 123456 );
                blki->AddRow();

                blki->Complete();
                blki->Close();
            }

            // Second test ...
            // Overflow test.
            // !!! Current behavior is not defined properly and not consistent between drivers.
    //         {
    //             auto_ptr<IBulkInsert> blki( GetConnection().CreateBulkInsert("#__blki_test") );
    //
    //             CVariant col1(eDB_Char,64);
    //             CVariant col2(eDB_BigInt);
    //
    //             blki->Bind(1, &col1);
    //             blki->Bind(2, &col2);
    //
    //             string name(8000, 'A');
    //             col1 = name;
    //             col2 = Int8( 123 );
    //             blki->AddRow();
    //         }
        }
        else {
            GetArgs().PutMsgDisabled("Bigint in Sybase");
        }

        // VARCHAR ...
        {
            int num_of_tests;

            if (GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) {
                num_of_tests = 7;
            } else {
                // Sybase
                num_of_tests = 3;
            }

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

                CVariant col1(eDB_Int);

                bi->Bind(1, &col1);

                for(int i = 0; i < num_of_tests; ++i ) {
                    col1 = i;
                    switch (i) {
                    case 0:
                        BulkAddRow(bi, CVariant(eDB_VarChar));
                        break;
                    case 1:
                        BulkAddRow(bi, CVariant(eDB_LongChar, GetMaxVarcharSize()));
                        break;
                    case 2:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 1024));
                        break;
                    case 3:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 4112));
                        break;
                    case 4:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 4113));
                        break;
                    case 5:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 4138));
                        break;
                    case 6:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 4139));
                        break;
                    };

                }
                bi->Complete();
            }

            // Retrieve data ...
            // Some drivers limit size of text/binary to 255 bytes ...
            if (GetArgs().GetDriverName() != dblib_driver
                ) {
                sql  = " SELECT id, vc8000_field FROM " + table_name;
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                        // Retrieve results, if any
                        while( rs->Next() ) {
                            Int4 i = rs->GetVariant(1).GetInt4();
                            string col1 = rs->GetVariant(2).GetString();

                            switch (i) {
                            case 0:
                                BOOST_CHECK_EQUAL(
                                    col1.size(),
                                    string::size_type(GetMaxVarcharSize())
                                    );
                                break;
                            case 1:
                                BOOST_CHECK_EQUAL(
                                    col1.size(),
                                    string::size_type(GetMaxVarcharSize())
                                    );
                                break;
                            case 2:
                                BOOST_CHECK_EQUAL(col1.size(),
                                                  string::size_type(1024)
                                                  );
                                break;
                            case 3:
                                BOOST_CHECK_EQUAL(col1.size(),
                                                  string::size_type(4112)
                                                  );
                                break;
                            case 4:
                                BOOST_CHECK_EQUAL(col1.size(),
                                                  string::size_type(4113)
                                                  );
                                break;
                            case 5:
                                BOOST_CHECK_EQUAL(col1.size(),
                                                  string::size_type(4138)
                                                  );
                                break;
                            case 6:
                                BOOST_CHECK_EQUAL(col1.size(),
                                                  string::size_type(4139)
                                                  );
                                break;
                            };
                        }
                    }
                }
            } else {
                GetArgs().PutMsgDisabled("Test_Bulk_Writing Retrieve data");
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing2)
{
    string sql;
    const string table_name("#SbSubs");

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + "( \n"
                "    sacc int NOT NULL , \n"
                "    ver smallint NOT NULL , \n"
                "    live bit DEFAULT (1) NOT NULL, \n"
                "    sid int NOT NULL , \n"
                "    date datetime DEFAULT (getdate()) NULL, \n"
                "    rlsdate smalldatetime NULL, \n"
                "    depdate datetime DEFAULT (getdate()) NOT NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_Int);
            CVariant col4(eDB_Int);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);
            bi->Bind(4, &col4);

            col1 = 15001;
            col2 = 1;
            col4 = 0;

            bi->AddRow();
            bi->Complete();
        }

        // Retrieve data ...
        {
            sql  = " SELECT live, date, rlsdate, depdate FROM " + table_name;

            auto_stmt->SendSql( sql );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->GetVariant(1).IsNull() );
            BOOST_CHECK( rs->GetVariant(1).GetBit() );
            BOOST_CHECK( !rs->GetVariant(2).IsNull() );
            BOOST_CHECK( !rs->GetVariant(2).GetCTime().IsEmpty() );
            BOOST_CHECK( rs->GetVariant(3).IsNull() );
            BOOST_CHECK( !rs->GetVariant(4).IsNull() );
            BOOST_CHECK( !rs->GetVariant(4).GetCTime().IsEmpty() );

            // Dump results ...
            DumpResults( auto_stmt.get() );
        }

        // Second test
        {
            sql = "DELETE FROM " + table_name;
            auto_stmt->ExecuteUpdate(sql);

            auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_Int);
            CVariant col3(eDB_Int);
            CVariant col4(eDB_Int);
            CVariant col5(eDB_DateTime);
            CVariant col6(eDB_DateTime);
            CVariant col7(eDB_DateTime);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);
            bi->Bind(3, &col3);
            bi->Bind(4, &col4);
            bi->Bind(5, &col5);
            bi->Bind(6, &col6);
            bi->Bind(7, &col7);

            col1 = 15001;
            col2 = 1;
            col3 = 2;
            if (GetArgs().IsODBCBased()
                || GetArgs().GetDriverName() == dblib_driver
                )
            {
                GetArgs().PutMsgDisabled("Bulk-insert NULLs when there are defaults");

                bi->AddRow();
                bi->Complete();

                // Retrieve data ...
                {
                    sql  = " SELECT live, date, rlsdate, depdate FROM " + table_name;

                    auto_stmt->SendSql( sql );

                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );

                    BOOST_CHECK( rs->Next() );
                    BOOST_CHECK( !rs->GetVariant(1).IsNull() );
                    BOOST_CHECK( rs->GetVariant(1).GetBit() );
                    BOOST_CHECK( !rs->GetVariant(2).IsNull() );
                    BOOST_CHECK( !rs->GetVariant(2).GetCTime().IsEmpty() );
                    BOOST_CHECK( rs->GetVariant(3).IsNull() );
                    BOOST_CHECK( !rs->GetVariant(4).IsNull() );
                    BOOST_CHECK( !rs->GetVariant(4).GetCTime().IsEmpty() );

                    // Dump results ...
                    DumpResults( auto_stmt.get() );
                }
            }
            else if (GetArgs().GetDriverName() != ctlib_driver) {
                try {
                    bi->AddRow();
                    bi->Complete();

                    BOOST_FAIL("Exception was not thrown");
                }
                catch (CDB_ClientEx& ex) {
                    // exception must be thrown
                    _TRACE(ex);
                }
            }
            else {
                // driver crushes after trying to insert
                GetArgs().PutMsgDisabled("Bulk-insert NULL into NOT-NULL column");
            }
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing3)
{
    string sql;
    string table_name("#blk_table3");
    string table_name2 = "#dbapi_bcp_table2";
    const int test_num = 10;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Side-effect ...
        // Tmp table ...
        auto_ptr<IBulkInsert> bi_tmp(
            GetConnection().GetBulkInsert(table_name2)
            );

        CVariant col_tmp(eDB_Int);
        bi_tmp->Bind(1, &col_tmp);

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    uid int NOT NULL , \n"
                "    info_id int NOT NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi(
                GetConnection().GetBulkInsert(table_name)
                );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_Int);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);

            for (int j = 0; j < test_num; ++j) {
                for (int i = 0; i < test_num; ++i) {
                    col1 = i;
                    col2 = i * 2;

                    bi->AddRow();
                }
                bi->StoreBatch();
            }

            bi->Complete();
        }


    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing4)
{
    string sql;
    string table_name("#blk_table4");
    const int test_num = 10;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
        const string test_data("Test, test, tEST.");

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                // Identity doesn't work with old ctlib driver (12.5.0.6-ESD13-32bit)
                //"    id int IDENTITY NOT NULL , \n"
                "    id_nwparams int NOT NULL, \n"
                "    gi1 int NOT NULL, \n"
                "    gi2 int NOT NULL, \n"
                "    idty FLOAT NOT NULL, \n"
                "    transcript TEXT NOT NULL, \n"
                "    idty_tup2 FLOAT NULL, \n"
                "    idty_tup4 FLOAT NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            //
            auto_ptr<IBulkInsert> bi(
                GetConnection().CreateBulkInsert(table_name)
                );

            CVariant b_idnwparams(eDB_Int);
            CVariant b_gi1(eDB_Int);
            CVariant b_gi2(eDB_Int);
            CVariant b_idty(eDB_Float);
            CVariant b_idty2(eDB_Float);
            CVariant b_idty4(eDB_Float);
            CVariant b_trans(eDB_Text);

            Uint2 pos = 0;
            bi->Bind(++pos, &b_idnwparams);
            bi->Bind(++pos, &b_gi1);
            bi->Bind(++pos, &b_gi2);
            bi->Bind(++pos, &b_idty);
            bi->Bind(++pos, &b_trans);
            bi->Bind(++pos, &b_idty2);
            bi->Bind(++pos, &b_idty4);

            b_idnwparams = 123456;

            for (int j = 0; j < test_num; ++j) {

                b_gi1 = j + 1;
                b_gi2 = j + 2;
                b_idty = float(j + 1.1);
                b_idty2 = float(j + 2.2);
                b_idty4 = float(j + 3.3);

                b_trans.Truncate();
                b_trans.Append(
                        test_data.data(),
                        test_data.size()
                        );

                bi->AddRow();
            }

            bi->Complete();
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing5)
{
    string sql;
    const string table_name("#blk_table5");
    // const string table_name("DBAPI_Sample..blk_table5");

    try {
        CDB_Connection* conn(GetConnection().GetCDB_Connection());


        // Create table ...
        if (true) {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    file_name varchar(512) NULL, \n"
                "    line_num int NULL, \n"
                "    pid int NULL, \n"
                "    tid int NULL, \n"
                "    iteration int NULL, \n"
                "    proc_sn int NULL, \n"
                "    thread_sn int NULL, \n"
                "    host varchar(128) NULL, \n"
                "    session_id varchar(128) NULL, \n"
                "    app_name varchar(128) NULL, \n"
                "    req_type varchar(128) NULL, \n"
                "    client_ip int NULL, \n"
                "    date datetime NULL, \n"
                "    exec_time_sec float NULL, \n"
                "    params_unparsed varchar(8000) NULL, \n"
                "    extra varchar(8000) NULL, \n"
                "    error varchar(8000) NULL, \n"
                "    warning varchar(8000) NULL \n"
                ") \n"
                ;

            auto_ptr<CDB_LangCmd> cmd(conn->LangCmd(sql));
            cmd->Send();
            cmd->DumpResults();
        }

        // Insert data ...
        {
            auto_ptr<CDB_BCPInCmd> vBcp(conn->BCPIn(table_name));

            CDB_VarChar     s_file_name;
            CDB_Int         n_line_num;
            CDB_Int         n_pid;
            CDB_Int         n_tid;
            CDB_Int         n_iteration;
            CDB_Int         n_proc_sn;
            CDB_Int         n_thread_sn;
            CDB_VarChar     sm_host;
            CDB_VarChar     s_session_id;
            CDB_VarChar     s_app_name;
            CDB_VarChar     s_req_type;
            CDB_Int         n_client_ip;
            CDB_DateTime    dt_date;
            CDB_Float       f_exec_time_secs;
            CDB_VarChar     s_params_unparsed;
            CDB_VarChar     s_extra;
            CDB_VarChar     s_error;
            CDB_VarChar     s_warning;

            Uint2 pos = 0;
            vBcp->Bind(pos++, &s_file_name);
            vBcp->Bind(pos++, &n_line_num);
            vBcp->Bind(pos++, &n_pid);
            vBcp->Bind(pos++, &n_tid);
            vBcp->Bind(pos++, &n_iteration);
            vBcp->Bind(pos++, &n_proc_sn);
            vBcp->Bind(pos++, &n_thread_sn);
            vBcp->Bind(pos++, &sm_host);
            vBcp->Bind(pos++, &s_session_id);
            vBcp->Bind(pos++, &s_app_name);
            vBcp->Bind(pos++, &s_req_type);
            vBcp->Bind(pos++, &n_client_ip);
            vBcp->Bind(pos++, &dt_date);
            vBcp->Bind(pos++, &f_exec_time_secs);
            vBcp->Bind(pos++, &s_params_unparsed);
            vBcp->Bind(pos++, &s_extra);
            vBcp->Bind(pos++, &s_error);
            vBcp->Bind(pos++, &s_warning);

            int i = 2;
            while (i-- > 0) {
                if (i % 2 == 1)
                    s_file_name = "";
                else
                    s_file_name = "some name";
                n_line_num = 12;
                n_pid = 12345;
                n_tid = 67890;
                n_iteration = 23;
                n_proc_sn = 34;
                n_thread_sn = 45;
                sm_host = "some host";
                s_session_id = "some id";
                s_app_name = "some name";
                s_req_type = "some type";
                n_client_ip = 12345;
                dt_date = CTime();
                f_exec_time_secs = 0;
                if (i % 2 == 1)
                    s_params_unparsed = "";
                else
                    s_params_unparsed = "some params";
                s_extra = "";
                s_error = "";
                s_warning = "";

                vBcp->SendRow();
            }

            vBcp->CompleteBCP();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing6)
{
    string sql;
    string table_name("#blk_table6");
    const string& str_value("Oops ...");

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
        const string test_data("Test, test, tEST.");

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "[ctg_id] INT NOT NULL, \n"
                "[ctg_name] VARCHAR(64) NOT NULL, \n"
                "[ctg_gi] INT NOT NULL, \n"
                "[ctg_acc] VARCHAR(20) NULL, \n"
                "[ctg_ver] TINYINT NOT NULL, \n"
                "[chrm] VARCHAR(64) NOT NULL, \n"
                "[start] INT NOT NULL, \n"
                "[stop] INT NOT NULL, \n"
                "[orient] CHAR(1) NOT NULL, \n"
                "[group_term] VARCHAR(64) NOT NULL, \n"
                "[group_label] VARCHAR(64) NOT NULL, \n"
                "[ctg_label] VARCHAR(64) NOT NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            //
            auto_ptr<IBulkInsert> stmt(
                GetConnection().GetBulkInsert(table_name)
                );

            // Declare variables ...
            CVariant ctg_id(eDB_Int);
            CVariant ctg_name(eDB_VarChar);
            CVariant ctg_gi(eDB_Int);
            CVariant ctg_acc(eDB_VarChar);
            CVariant ctg_ver(eDB_TinyInt);
            CVariant chrm(eDB_VarChar);
            CVariant ctg_start(eDB_Int);
            CVariant ctg_stop(eDB_Int);
            CVariant ctg_orient(eDB_VarChar);
            CVariant group_term(eDB_VarChar);
            CVariant group_label(eDB_VarChar);
            CVariant ctg_label(eDB_VarChar);

            // Bind ...
            int idx(0);
            stmt->Bind(++idx, &ctg_id);
            stmt->Bind(++idx, &ctg_name);
            stmt->Bind(++idx, &ctg_gi);
            stmt->Bind(++idx, &ctg_acc);
            stmt->Bind(++idx, &ctg_ver);
            stmt->Bind(++idx, &chrm);
            stmt->Bind(++idx, &ctg_start);
            stmt->Bind(++idx, &ctg_stop);
            stmt->Bind(++idx, &ctg_orient);
            stmt->Bind(++idx, &group_term);
            stmt->Bind(++idx, &group_label);
            stmt->Bind(++idx, &ctg_label);

            // Assign values ...
            ctg_id = 1;
            ctg_name = str_value;
            ctg_gi = 2;
            ctg_acc = str_value;
            ctg_ver = Uint1(3);
            chrm = str_value;
            ctg_start = 4;
            ctg_stop = 5;
            ctg_orient = "V";
            group_term = str_value;
            group_label = str_value;
            ctg_label = str_value;

            stmt->AddRow();
            stmt->Complete();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing7)
{
    string sql;
    string table_name("#blk_table7");
    const int test_num = 10;
    const string str1 = "Test, test, tEST.";
    const string str2 = "asdfghjkl";
    const string str3 = "Test 1234567890";
    const int i1 = int(0xdeadbeaf);
    const int i2 = int(0xcac0ffee);
    const int i3 = int(0xba1c0c0a);
    const short s1 = short(0xfade);
    const short s2 = short(0x0ec0);
    const Uint1 t1 = char(0x42);
    const Uint1 b_vals[test_num][11] = {
          {1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1},
          {1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1},
          {0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1},
          {0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0},
          {1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1},
          {0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0},
          {0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1},
          {0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0},
          {0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1},
          {1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0},
        };

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    id int, \n"
                "    i1 int, \n"
                "    str1 varchar(100), \n"
                "    b1 bit, \n"
                "    b2 bit, \n"
                "    i2 int, \n"
                "    b3 bit, \n"
                "    s1 smallint, \n"
                "    b4 bit, \n"
                "    b5 bit, \n"
                "    i3 int, \n"
                "    b6 bit, \n"
                "    str2 varchar(100), \n"
                "    b7 bit, \n"
                "    s2 smallint, \n"
                "    b8 bit, \n"
                "    b9 bit, \n"
                "    t1 tinyint, \n"
                "    b10 bit, \n"
                "    str3 varchar(100), \n"
                "    b11 bit \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            //
            auto_ptr<IBulkInsert> bi(
                GetConnection().CreateBulkInsert(table_name)
                );

            CVariant b_id(eDB_Int);
            CVariant b_i1(eDB_Int);
            CVariant b_str1(eDB_VarChar);
            CVariant b_b1(eDB_TinyInt);
            CVariant b_b2(eDB_TinyInt);
            CVariant b_i2(eDB_Int);
            CVariant b_b3(eDB_TinyInt);
            CVariant b_s1(eDB_SmallInt);
            CVariant b_b4(eDB_TinyInt);
            CVariant b_b5(eDB_TinyInt);
            CVariant b_i3(eDB_Int);
            CVariant b_b6(eDB_TinyInt);
            CVariant b_str2(eDB_VarChar);
            CVariant b_b7(eDB_TinyInt);
            CVariant b_s2(eDB_SmallInt);
            CVariant b_b8(eDB_TinyInt);
            CVariant b_b9(eDB_TinyInt);
            CVariant b_t1(eDB_TinyInt);
            CVariant b_b10(eDB_TinyInt);
            CVariant b_str3(eDB_VarChar);
            CVariant b_b11(eDB_TinyInt);

            Uint2 pos = 0;
            bi->Bind(++pos, &b_id);
            bi->Bind(++pos, &b_i1);
            bi->Bind(++pos, &b_str1);
            bi->Bind(++pos, &b_b1);
            bi->Bind(++pos, &b_b2);
            bi->Bind(++pos, &b_i2);
            bi->Bind(++pos, &b_b3);
            bi->Bind(++pos, &b_s1);
            bi->Bind(++pos, &b_b4);
            bi->Bind(++pos, &b_b5);
            bi->Bind(++pos, &b_i3);
            bi->Bind(++pos, &b_b6);
            bi->Bind(++pos, &b_str2);
            bi->Bind(++pos, &b_b7);
            bi->Bind(++pos, &b_s2);
            bi->Bind(++pos, &b_b8);
            bi->Bind(++pos, &b_b9);
            bi->Bind(++pos, &b_t1);
            bi->Bind(++pos, &b_b10);
            bi->Bind(++pos, &b_str3);
            bi->Bind(++pos, &b_b11);

            b_i1 = i1;
            b_str1 = str1;
            b_i2 = i2;
            b_s1 = s1;
            b_i3 = i3;
            b_str2 = str2;
            b_s2 = s2;
            b_t1 = t1;
            b_str3 = str3;

            for (int j = 0; j < test_num; ++j) {

                b_id = j;
                b_b1 = b_vals[j][0];
                b_b2 = b_vals[j][1];
                b_b3 = b_vals[j][2];
                b_b4 = b_vals[j][3];
                b_b5 = b_vals[j][4];
                b_b6 = b_vals[j][5];
                b_b7 = b_vals[j][6];
                b_b8 = b_vals[j][7];
                b_b9 = b_vals[j][8];
                b_b10 = b_vals[j][9];
                b_b11 = b_vals[j][10];

                bi->AddRow();
            }

            bi->Complete();
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Late_Bind)
{
    string sql;
    const string table_name("#blk_late_bind_table");
    bool exception_thrown = false;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    id int NULL, \n"
                "    name varchar(200) NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Check that data cannot be binded after rows sending
        {
            auto_ptr<IBulkInsert> bi(
                GetConnection().CreateBulkInsert(table_name)
                );

            CVariant  id(eDB_Int);
            CVariant  name(eDB_VarChar);

            bi->Bind(1, &id);

            id = 5;
            bi->AddRow();

            try {
                bi->Bind(2, &name);
                BOOST_FAIL("Exception after late binding wasn't thrown");
            }
            catch (CDB_Exception& ex) {
                _TRACE(ex);
                exception_thrown = true;
                // ok
            }

            BOOST_CHECK(exception_thrown);
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

BOOST_AUTO_TEST_CASE(Test_Bulk_Writing8)
{
    string sql;
    const string table_name("#blk_table8");

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + "( \n"
                "    id int, \n"
                "    str varchar(100) NOT NULL"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_VarChar);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);

            col1 = 15001;
            col2 = "";

            bi->AddRow();
            bi->Complete();
        }

        // Retrieve data ...
        {
            sql  = " SELECT id, str FROM " + table_name;

            auto_stmt->SendSql( sql );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->GetVariant(1).IsNull() );
            BOOST_CHECK_EQUAL(rs->GetVariant(1).GetInt4(), 15001);
            BOOST_CHECK( !rs->GetVariant(2).IsNull() );
            // Old protocol version has this strange feature
            if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer
                || GetArgs().GetDriverName() == dblib_driver
                )
            {
                BOOST_CHECK_EQUAL(rs->GetVariant(2).GetString(), string(" "));
            }
            else {
                BOOST_CHECK_EQUAL(rs->GetVariant(2).GetString(), string());
            }

            // Dump results ...
            DumpResults( auto_stmt.get() );
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


END_NCBI_SCOPE
