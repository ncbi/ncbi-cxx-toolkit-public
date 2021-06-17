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
 * Author: Sergey Satskiy
 *
 * File Description: MS SQL XACT_ABORT option test
 *
 * ===========================================================================
 */

#include "sdbapi_unit_test_pch.hpp"


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_xact_abort)
{
    try {
        // Check the server brand. Only MS SQL should be tested
        {
            bool    is_ms_sql = false;

            CQuery query = GetDatabase().NewQuery();
            query.SetSql("SELECT @@version");
            query.Execute();
            for (const auto&  row: query.SingleSet()) {
                if (NStr::Find(row[1].AsString(),
                               "Microsoft", NStr::eNocase) != NPOS)
                    is_ms_sql = true;
            }

            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            if (!is_ms_sql)
                return;
        }

        // Grab the lock to avoid simultaneous access
        {
            CQuery query = GetDatabase().NewQuery();

            query.SetParameter("@Resource", "test_xact_abort_lock");
            query.SetParameter("@LockMode", "Exclusive");
            query.SetParameter("@LockOwner", "Session");
            query.SetParameter("@LockTimeout", 5000);   // ms

            query.ExecuteSP("sp_getapplock");
            int     status = query.GetStatus();

            // status >= 0 => success
            if (status < 0)
                return;
        }



        // Note: temporary tables do not support referencial integrity
        // It is just silently ignored!
        string  table_name_1 = "test_xact_abort_1";
        string  table_name_2 = "test_xact_abort_2";

        // Create the required tables and populate data
        {
            CQuery query = GetDatabase().NewQuery();

            query.SetSql(
                "IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='" +
                table_name_1 + "' and xtype='U') "
                "CREATE TABLE " + table_name_1 +
                "(a INT NOT NULL PRIMARY KEY)");
            query.Execute();
            query.PurgeResults();

            query.SetSql(
                "IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='" +
                table_name_2 + "' and xtype='U') "
                "CREATE TABLE " + table_name_2 +
                "(a INT NOT NULL REFERENCES " +
                table_name_1 + "(a))");
            query.Execute();
            query.PurgeResults();

            query.SetSql("DELETE FROM " + table_name_2);
            query.Execute();
            query.PurgeResults();

            query.SetSql("DELETE FROM " + table_name_1);
            query.Execute();
            query.PurgeResults();

            query.SetSql("INSERT INTO " + table_name_1 + " VALUES (1)");
            query.Execute();
            query.PurgeResults();

            query.SetSql("INSERT INTO " + table_name_1 + " VALUES (3)");
            query.Execute();
            query.PurgeResults();
        }

        // The XACT_ABORT option is set ON by default so test it first
        {
            CQuery query = GetDatabase().NewQuery();

            // Note: the second insert is to fail
            query.SetSql("BEGIN TRANSACTION\n"
                         "INSERT INTO " + table_name_2 + " VALUES (1)\n"
                         "INSERT INTO " + table_name_2 + " VALUES (2)\n"
                         "INSERT INTO " + table_name_2 + " VALUES (3)\n"
                         "COMMIT TRANSACTION");
            try {
                // second insert fails so an exception is expected
                query.Execute();
                BOOST_FAIL("Exception is expected");
            } catch (...) {
            }

            // No records are expected
            int num = 0;
            query.SetSql("SELECT * FROM " + table_name_2);
            query.Execute();

            for (const auto& row: query.SingleSet()) {
                ++num;
            }
            query.PurgeResults();

            BOOST_CHECK(num == 0);
        }

        // Test with XACT_ABORT option set to OFF
        {
            CQuery query = GetDatabase().NewQuery();

            // Note: the second insert is to fail
            query.SetSql("SET XACT_ABORT OFF\n"
                         "BEGIN TRANSACTION\n"
                         "INSERT INTO " + table_name_2 + " VALUES (1)\n"
                         "INSERT INTO " + table_name_2 + " VALUES (2)\n"
                         "INSERT INTO " + table_name_2 + " VALUES (3)\n"
                         "COMMIT TRANSACTION");
            try {
                // second insert fails so an exception is expected
                query.Execute();
                BOOST_FAIL("Exception is expected");
            } catch (...) {
            }

            // Two records are expected inspite of a transaction
            int num = 0;
            query.SetSql("SELECT * FROM " + table_name_2);
            query.Execute();

            for (const auto& row: query.SingleSet()) {
                ++num;
            }
            query.PurgeResults();

            BOOST_CHECK(num == 2);
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

END_NCBI_SCOPE
