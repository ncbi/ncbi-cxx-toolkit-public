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
* Author: Michael Kholodov
*
* File Description:
*   String representation of the database character types.
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <dbapi/simple/sdbapi.hpp>


USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  MAIN

class CSdbapiTest : public CNcbiApplication
{
private:
    virtual void Init();
    virtual int Run();
    virtual void Exit();


    CArgDescriptions *argList;
};


void CSdbapiTest::Init()
{
    argList = new CArgDescriptions();

    argList->SetUsageContext(GetArguments().GetProgramBasename(),
                             "SDBAPI test program");

    argList->AddDefaultKey("s", "string",
                           "Server name",
                           CArgDescriptions::eString, "DBAPI_MS_TEST");

    SetupArgDescriptions(argList);
}

int CSdbapiTest::Run()
{
    CArgs args = GetArgs();

    try {
        string server = args["s"].AsString();

        CSDB_ConnectionParam db_params("dbapi://anyone:allowed@/DBAPI_Sample");
        db_params.Set(CSDB_ConnectionParam::eService, server);
        CDatabase db(db_params);
        db.Connect();

        CQuery query = db.NewQuery();
        string sql;
        try  {
            NcbiCout << "Creating SelectSample table...";
            sql = "if exists( select * from sysobjects \
                    where name = 'SelectSample' \
                    AND type = 'U') \
                    begin \
                        drop table SelectSample \
                    end";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            query.VerifyDone();

            sql = "create table SelectSample (\
                    int_val int not null, \
                    fl_val real not null, \
                    date_val smalldatetime not null, \
                    str_val varchar(255) not null, \
                    text_val text not null)";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            query.VerifyDone();

            sql = "insert SelectSample values (1, 2.5, '11/05/2005', 'Test string1', 'TextBlobTextBlobTextBlobTextBlobTextBlob') \
                  insert SelectSample values (2, 3.3, '11/06/2005', 'Test string2', 'TextBlobTextBlobTextBlobTextBlobTextBlob') \
                  insert SelectSample values (3, 4.4, '11/07/2005', 'Test string3', 'TextBlobTextBlobTextBlobTextBlobTextBlob') \
                  insert SelectSample values (4, 5.5, '11/08/2005', 'Test string4', 'TextBlobTextBlobTextBlobTextBlobTextBlob') \
                  insert SelectSample values (5, 6.6, '11/09/2005', 'Test string5', 'TextBlobTextBlobTextBlobTextBlobTextBlob')";
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            query.VerifyDone();


            sql = "select int_val, fl_val, date_val, str_val from SelectSample";
            NcbiCout << endl << "Testing simple select..." << endl
                    << sql << endl;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(5);

            bool show_names = true;
            ITERATE(CQuery, row, query) {
                if (show_names) {
                    for(int i = 1; i <= row.GetTotalColumns(); ++i) {
                        NcbiCout << row.GetColumnName(i) << "  ";
                    }
                    NcbiCout << endl;
                    show_names = false;
                }

                for(int i = 1;  i <= row.GetTotalColumns(); ++i) {
                    NcbiCout << row[i].AsString() << "|";
                }
                NcbiCout << endl;

#if 1
                NcbiCout << row[1].AsInt4() << "|"
                         << row[2].AsFloat() << "|"
                         << row["date_val"].AsString() << "|"
                         << row[4].AsString() << "|"
                         << endl;
#endif

            }

            query.VerifyDone();
            NcbiCout << "Rows : " << query.GetRowCount() << endl;
        }
        catch(CSDB_Exception& e) {
            NcbiCout << "Exception: " << e.what() << endl;
        }

        // Testing bulk insert w/o BLOBs
        NcbiCout << endl << "Creating BulkSample table..." << endl;
        sql = "if exists( select * from sysobjects \
                where name = 'BulkSample' \
                AND type = 'U') \
                begin \
                    drop table BulkSample \
                end";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        query.VerifyDone();

        sql = "create table BulkSample (\
                id int not null, \
                ord int not null, \
                mode tinyint not null, \
                date datetime not null)";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        query.VerifyDone();

        try {
            //Initialize table using bulk insert
            NcbiCout << "Initializing BulkSample table..." << endl;
            CBulkInsert bi = db.NewBulkInsert("BulkSample", 5);
            bi.Bind(1, eSDB_Int4);
            bi.Bind(2, eSDB_Int4);
            bi.Bind(3, eSDB_Byte);
            bi.Bind(4, eSDB_DateTime);
            for(int i = 0; i < 10; ++i) {
                bi << i << (i * 2) << Uint1(i + 1) << CTime(CTime::eCurrent)
                   << EndRow;
            }
            bi.Complete();
        }
        catch(...) {
            throw;
        }

        // create a stored procedure
        sql = "if exists( select * from sysobjects \
                where name = 'SampleProc' \
                AND type = 'P') \
                begin \
                    drop proc SampleProc \
                end";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        query.VerifyDone();

        if(NStr::CompareNocase(server, "DBAPI_DEV1") == 0
           ||  NStr::CompareNocase(server, "DBAPI_SYB_TEST") == 0)
        {
            sql = "create procedure SampleProc \
    @id int, \
    @f float, \
    @o int output \
as \
begin \
  select int_val, fl_val, date_val from SelectSample \
  where int_val < @id and fl_val <= @f \
  select @o = 555 \
  select 2121, 'Parameter @id:', @id, 'Parameter @f:', @f, 'Parameter @o:', @o  \
  print 'Print test output' \
  return @id \
end";
        }
        else {
            sql = "create procedure SampleProc \
    @id int, \
    @f float, \
    @o int output \
as \
begin \
  select int_val, fl_val, date_val from SelectSample \
  where int_val < @id and fl_val <= @f \
  select @o = 555 \
  select 2121, 'Parameter @id:', @id, 'Parameter @f:', @f, 'Parameter @o:', @o  \
  print 'Print test output' \
  raiserror('Raise Error test output', 1, 1) \
  return @id \
end";
        }
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        query.VerifyDone();

        float f = 2.999f;

        // call stored procedure
        NcbiCout << endl << "Calling stored procedure..." << endl;

        query.SetParameter("@id", 5);
        query.SetParameter("@f", f);
        query.SetParameter("@o", 0, eSDB_Int4, eSP_InOut);
        query.ExecuteSP("SampleProc");
        query.RequireRowCount(1, kMax_Auto);

        ITERATE(CQuery, row, query.SingleSet()) {
            if(row[1].AsInt4() == 2121) {
                NcbiCout << row[2].AsString() << " "
                         << row[3].AsString() << " "
                         << row[4].AsString() << " "
                         << row[5].AsString() << " "
                         << row[6].AsString() << " "
                         << row[7].AsString() << " "
                         << endl;
            }
            else {
                NcbiCout << row[1].AsInt4() << "|"
                         << row[2].AsFloat() << "|"
                         << row["date_val"].AsString() << "|"
                         << endl;
            }
        }
        query.VerifyDone();
        NcbiCout << "Output param: "
                 << query.GetParameter("@o").AsInt4()
                 << endl;
        NcbiCout << "Status : " << query.GetStatus() << endl;

        // Reconnect
        NcbiCout << endl << "Reconnecting..." << endl;

        db.Close();
        db.Connect();

        query = db.NewQuery();

        // create a table
        NcbiCout << endl << "Creating BlobSample table..." << endl;
        sql = "if exists( select * from sysobjects \
                where name = 'BlobSample' \
                AND type = 'U') \
                begin \
                    drop table BlobSample \
                end";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        query.VerifyDone();


        sql = "create table BlobSample (\
                id int null, \
                blob2 text null, blob text null, unique (id))";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        query.VerifyDone();

        // Write BLOB several times
        const int COUNT = 5;

        //Initialize table using bulk insert
        NcbiCout << "Initializing BlobSample table..." << endl;
        CBulkInsert bi = db.NewBulkInsert("BlobSample", 1);
        bi.Bind(1, eSDB_Int4);
        bi.Bind(2, eSDB_Text);
        bi.Bind(3, eSDB_Text);

        for(int i = 0; i < COUNT; ++i ) {
            string im = "BLOB data " + NStr::IntToString(i);
            bi << i << im << im << EndRow;
        }
        bi.Complete();

        // check if Blob is there
        NcbiCout << "Checking BLOB size..." << endl;
        query.SetSql("select 'Written blob size' as size, datalength(blob) \
                        from BlobSample where id = 1");
        query.Execute();
        query.RequireRowCount(1);

        ITERATE(CQuery, row, query.SingleSet()) {
            NcbiCout << row[1].AsString() << ": "
                     << row[2].AsInt4() << endl;
        }

        query.VerifyDone();

        // ExecuteUpdate rowcount test
        NcbiCout << "Rowcount test..." << endl;
        sql = "update BlobSample set blob ='deleted'";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        query.VerifyDone();
        NcbiCout << "Rows updated: " << query.GetRowCount() << endl;

        // drop BlobSample table
        NcbiCout << "Deleting BlobSample table..." << endl;
        sql = "drop table BlobSample";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        query.VerifyDone();
        NcbiCout << "Done." << endl;
    }
    catch(out_of_range) {
        NcbiCout << "Exception: Out of range" << endl;
        return 1;
    }
    catch(exception& e) {
        NcbiCout << "Exception: " << e.what() << endl;
        return 1;
    }

    return 0;
}

void CSdbapiTest::Exit()
{

}


int main(int argc, const char* argv[])
{
    return CSdbapiTest().AppMain(argc, argv);
}
