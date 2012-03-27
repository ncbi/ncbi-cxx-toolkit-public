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

#include <dbapi/dbapi.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <vector>

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  MAIN

class CDbapiTest : public CNcbiApplication
{
private:
    virtual void Init();
    virtual int Run();
    virtual void Exit();

  
    CArgDescriptions *argList;
};


void CDbapiTest::Init()
{

    argList = new CArgDescriptions();

    argList->SetUsageContext(GetArguments().GetProgramBasename(),
                             "DBAPI test program");


#ifdef WIN32

   argList->AddDefaultKey("s", "string",
                           "Server name",
                           CArgDescriptions::eString, "MS_DEV1");
   argList->AddDefaultKey("d", "string",
                           "Driver <ctlib|dblib|ftds|odbc>",
                           CArgDescriptions::eString, 
                           "odbc");
#else

	argList->AddDefaultKey("s", "string",
                           "Server name",
                           CArgDescriptions::eString, "TAPER");

	argList->AddDefaultKey("d", "string",
                           "Driver <ctlib|dblib|ftds>",
                           CArgDescriptions::eString, 
                           "ctlib");
#endif

    SetupArgDescriptions(argList);
}
  
int CDbapiTest::Run() 
{

    DBLB_INSTALL_DEFAULT();

    CArgs args = GetArgs();

    IDataSource *ds = 0;
    //CNcbiOfstream log("debug.log");
    //SetDiagStream(&log);
    try {
        
        CDriverManager &dm = CDriverManager::GetInstance();

        string server = args["s"].AsString();
        string driver = args["d"].AsString();


        // Create data source - the root object for all other
        // objects in the library.

        if ( NStr::CompareNocase(server, "STRAUSS") == 0 ||
             NStr::CompareNocase(server, "TAPER") == 0 ||
             NStr::CompareNocase(server, "MOZART") == 0 ) {

            map<string,string> attr;
            if ( NStr::CompareNocase(driver, "ftds") == 0 ||
                NStr::CompareNocase(driver, "ftds63") == 0 ) {

                // set TDS version
                attr["version"] = "42";
            } else if ( NStr::CompareNocase(driver, "dblib") == 0 ) {
                // set TDS version
                attr["version"] = "100";
            }
            ds = dm.CreateDs(driver, &attr);
        } else {
            ds = dm.CreateDs(driver);
        }

        // Redirect error messages to CMultiEx storage in the
        // data source object (global). Default output is sent
        // to standard error
        //
        //CNcbiOfstream logfile("test.log");
        //ds->SetLogStream(&log);


        // Create connection. 
        IConnection* conn = ds->CreateConnection();
        if ( conn == NULL) {
            return 1;
        }

        // Set this mode to be able to use bulk insert
        conn->SetMode(IConnection::eBulkInsert);

        // To ensure, that only one database connection is used
        // use this method. It will throw an exception, if the
        // library tries to open additional connections implicitly, 
        // like creating multiple statements.
        //conn->ForceSingle(true);
		CTrivialConnValidator val("DBAPI_Sample");

		conn->ConnectValidated(val, "anyone",
                      "allowed",
                      server,
                      "DBAPI_Sample");
    
        NcbiCout << "Using server: " << server
                 << ", driver: " << driver << endl;

        IStatement *stmt = conn->GetStatement();
        string sql;
        // Begin transaction
#if 0
        sql = "begin transaction";
        stmt->ExecuteUpdate(sql);

        // Get trancount
        sql = "select @@trancount";
        IResultSet *tc = stmt->ExecuteQuery(sql);
        while(tc->Next()) {
            NcbiCout << "Begin transaction, count: " << tc->GetVariant(1).GetString();
        }
#endif
        try 
		{
			NcbiCout << "Creating SelectSample table...";
			sql = "if exists( select * from sysobjects \
where name = 'SelectSample' \
AND type = 'U') \
begin \
	drop table SelectSample \
end";
			stmt->ExecuteUpdate(sql);


        sql = "create table SelectSample (\
	int_val int not null, \
	fl_val real not null, \
    date_val smalldatetime not null, \
	str_val varchar(255) not null, \
	text_val text not null)";
			stmt->ExecuteUpdate(sql);

			sql = "insert SelectSample values (1, 2.5, '11/05/2005', 'Test string1', 'TextBlobTextBlobTextBlobTextBlobTextBlob') \
				  insert SelectSample values (2, 3.3, '11/06/2005', 'Test string2', 'TextBlobTextBlobTextBlobTextBlobTextBlob') \
				  insert SelectSample values (3, 4.4, '11/07/2005', 'Test string3', 'TextBlobTextBlobTextBlobTextBlobTextBlob') \
				  insert SelectSample values (4, 5.5, '11/08/2005', 'Test string4', 'TextBlobTextBlobTextBlobTextBlobTextBlob') \
				  insert SelectSample values (5, 6.6, '11/09/2005', 'Test string5', 'TextBlobTextBlobTextBlobTextBlobTextBlob')";
			stmt->ExecuteUpdate(sql);


            sql = "select int_val, fl_val, date_val, str_val from SelectSample";
            NcbiCout << endl << "Testing simple select..." << endl
                    << sql << endl;

            conn->MsgToEx(true);

            stmt->SendSql(sql);
    
        // Below is an example of using auto_ptr to avoid resource wasting
        // in case of multiple resultsets, statements, etc.
        // NOTE: Use it with caution, when the wrapped parent object
        // goes out of scope, all child objects are destroyed.
            while( stmt->HasMoreResults() ) {
                if( stmt->HasRows() ) {   
                    auto_ptr<IResultSet> rs(stmt->GetResultSet());

                    const IResultSetMetaData* rsMeta = rs->GetMetaData();

                    rs->BindBlobToVariant(true);

                    for(unsigned int i = 1; i <= rsMeta->GetTotalColumns(); ++i )
                        NcbiCout << rsMeta->GetName(i) << "  ";

                    NcbiCout << endl;

                    while(rs->Next()) { 
                        for(unsigned int i = 1;  i <= rsMeta->GetTotalColumns(); ++i ) {
                            if( rsMeta->GetType(i) == eDB_Text
                                || rsMeta->GetType(i) == eDB_Image ) {

                                const CVariant& b = rs->GetVariant(i);

                                char *buf = new char[b.GetBlobSize() + 1];
                                b.Read(buf, b.GetBlobSize());
                                buf[b.GetBlobSize()] = '\0';
                                NcbiCout << buf << "|";
                                delete buf;
                                
                            }
                            else
                                NcbiCout << rs->GetVariant(i).GetString() << "|";
                        }
                        NcbiCout << endl;
                                
                            
#if 0
                        NcbiCout << rs->GetVariant(1).GetInt4() << "|"
                                 << rs->GetVariant(2).GetFloat() << "|"
                                 << rs->GetVariant("date_val").GetString() << "|"
                                 << rs->GetVariant(4).GetString()
                                 << endl;
#endif
                    
                    } 
                    // Use Close() to free database resources and leave DBAPI framwork intact
                    // All closed DBAPI objects become invalid.
                    //rs->Close();
                    NcbiCout << "Rows : " << stmt->GetRowCount() << endl;
               }
            }
        }
        catch(CDB_Exception& e) {
            NcbiCout << "Exception: " << e.what() << endl;
            NcbiCout << conn->GetErrorInfo();
        } 

        //stmt->PurgeResults();

        conn->MsgToEx(false);
/*
        // Testing MSSQL XML output
        sql += " for xml auto, elements";
        NcbiCout << "Testing simple select with XML output..." << endl
                 << sql << endl;

        conn->MsgToEx(true);

        try {
            stmt->SendSql(sql);
    
            while( stmt->HasMoreResults() ) {
                if( stmt->HasRows() ) {   
                    IResultSet *rs = stmt->GetResultSet();
                    const IResultSetMetaData *rsMeta = rs->GetMetaData();

                    rs->BindBlobToVariant(true);

                    for(int i = 1; i <= rsMeta->GetTotalColumns(); ++i )
                        NcbiCout << rsMeta->GetName(i) << "  ";

                    NcbiCout << endl;

                    while(rs->Next()) { 
                        for(int i = 1;  i <= rsMeta->GetTotalColumns(); ++i ) {
                            if( rsMeta->GetType(i) == eDB_Text
                                || rsMeta->GetType(i) == eDB_Image ) {

                                CDB_Stream *b = dynamic_cast<CDB_Stream*>(rs->GetVariant(i).GetData());
                                _ASSERT(b);

                                char *buf = new char[b->Size() + 1];
                                b->Read(buf, b->Size());
                                buf[b->Size()] = '\0';
                                NcbiCout << buf << "|";
                                delete buf;
                                
                            }
                            else
                                NcbiCout << rs->GetVariant(i).GetString() << "|";
                        }
                        NcbiCout << endl;
                    
                    } 
                    
                }
            }
        }
        catch(CDB_Exception& e) {
            NcbiCout << "Exception: " << e.what() << endl;
            NcbiCout << ds->GetErrorInfo();
        } 
        
        exit(1);
        //delete stmt;
*/

        //delete stmt;
        //stmt = conn->GetStatement();

        // Testing bulk insert w/o BLOBs
        NcbiCout << endl << "Creating BulkSample table..." << endl;
        sql = "if exists( select * from sysobjects \
where name = 'BulkSample' \
AND type = 'U') \
begin \
	drop table BulkSample \
end";
        stmt->ExecuteUpdate(sql);


        sql = "create table BulkSample (\
	id int not null, \
	ord int not null, \
    mode tinyint not null, \
    date datetime not null)";

        stmt->ExecuteUpdate(sql);

        //I_DriverContext *ctx = ds->GetDriverContext();
        //delete ctx;

        try {
            //Initialize table using bulk insert
            NcbiCout << "Initializing BulkSample table..." << endl;
            IBulkInsert *bi = conn->GetBulkInsert("BulkSample");
            CVariant b1(eDB_Int);
            CVariant b2(eDB_Int);
            CVariant b3(eDB_TinyInt);
            CVariant b4(eDB_DateTime);
            bi->Bind(1, &b1);
            bi->Bind(2, &b2);
            bi->Bind(3, &b3);
            bi->Bind(4, &b4);
            int i;
            for( i = 0; i < 10; ++i ) {
                b1 = i;
                b2 = i * 2;
                b3 = Uint1(i + 1);
                b4 = CTime(CTime::eCurrent);
                bi->AddRow();
                //bi->StoreBatch();
            }
            
            bi->Complete();

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
        stmt->ExecuteUpdate(sql);

	if( NStr::CompareNocase(server, "STRAUSS") == 0 || 
        NStr::CompareNocase(server, "TAPER") == 0 ||
        NStr::CompareNocase(server, "MOZART") == 0 )
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
   /* raiserror 20000  'Raise Error test output' */ \
  return @id \
end";
	else
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
        stmt->ExecuteUpdate(sql);
#if 0
        // commit transaction
        sql = "commit transaction";
        stmt->ExecuteUpdate(sql);

        sql = "select @@trancount";
        tc = stmt->ExecuteQuery(sql);
        while(tc->Next()) {
            NcbiCout << "Transaction committed, count: " << tc->GetVariant(1).GetString();
        }

#endif
        /*stmt->ExecuteUpdate("print 'test'");*/


        float f = 2.999f;

        // call stored procedure
        NcbiCout << endl << "Calling stored procedure..." << endl;
        
        ICallableStatement *cstmt = conn->GetCallableStatement("SampleProc");
        cstmt->SetParam(CVariant(5), "@id");
        cstmt->SetParam(CVariant::Float(&f), "@f");
        cstmt->SetOutputParam(CVariant(eDB_Int), "@o");
        cstmt->Execute(); 
    
        while(cstmt->HasMoreResults()) {
            IResultSet *rs = cstmt->GetResultSet();

            //NcbiCout << "Row count: " << cstmt->GetRowCount() << endl;

            if( !cstmt->HasRows() ) {
                continue;
            }

            switch( rs->GetResultType() ) {
            case eDB_ParamResult:
                while( rs->Next() ) {
                    NcbiCout << "Output param: "
                             << rs->GetVariant(1).GetInt4()
                             << endl;
                }
                break;
            case eDB_RowResult:
                while( rs->Next() ) {
                    if( rs->GetVariant(1).GetInt4() == 2121 ) {
                            NcbiCout << rs->GetVariant(2).GetString() << " "
                                     << rs->GetVariant(3).GetString() << " "
                                     << rs->GetVariant(4).GetString() << " "
                                     << rs->GetVariant(5).GetString() << " "
                                     << rs->GetVariant(6).GetString() << " "
                                     << rs->GetVariant(7).GetString() << " "
                                     << endl;
                    }
                    else {
                        NcbiCout << rs->GetVariant(1).GetInt4() << "|"
                                 << rs->GetVariant(2).GetFloat() << "|"
                                 << rs->GetVariant("date_val").GetString() << "|"
                                 << endl;
                    }
                }
                break;
            default:
                break;
            }
            // Delete object to get rid of it completely. All child objects will be also deleted
            // There is no need to close object before deleting. All objects close automatically
            // when deleted.
            delete rs;
         }
        NcbiCout << "Status : " << cstmt->GetReturnStatus() << endl;
        NcbiCout << endl << ds->GetErrorInfo() << endl;



        cstmt->Close();
        delete cstmt;
        cstmt = conn->GetCallableStatement("SampleProc");

        // call stored procedure using language call
        NcbiCout << endl << "Calling stored procedure using language call..." << endl;
        
        sql = "exec SampleProc @id, @f, @o output";
        stmt->SetParam(CVariant(5), "@id");
        stmt->SetParam(CVariant::Float(&f), "@f");
        stmt->SetParam(CVariant(5), "@o");
        stmt->SendSql(sql); 
    
        while(stmt->HasMoreResults()) {
            IResultSet *rs = stmt->GetResultSet();

            //NcbiCout << "Row count: " << stmt->GetRowCount() << endl;

            if( rs == 0 )
                continue;

            switch( rs->GetResultType() ) {
            case eDB_ParamResult:
                while( rs->Next() ) {
                    NcbiCout << "Output param: "
                             << rs->GetVariant(1).GetInt4()
                             << endl;
                }
                break;
            case eDB_StatusResult:
                while( rs->Next() ) {
                    NcbiCout << "Return status: "
                             << rs->GetVariant(1).GetInt4()
                             << endl;
                }
                break;
            case eDB_RowResult:
                while( rs->Next() ) {
                    if( rs->GetVariant(1).GetInt4() == 2121 ) {
                            NcbiCout << rs->GetVariant(2).GetString() << "|"
                                     << rs->GetVariant(3).GetString() << "|"
                                     << rs->GetVariant(4).GetString() << "|"
                                     << rs->GetVariant(5).GetString() << "|"
                                     << rs->GetVariant(6).GetString() << "|"
                                     << rs->GetVariant(7).GetString() << "|"
                                     << endl;
                    }
                    else {
                        NcbiCout << rs->GetVariant(1).GetInt4() << "|"
                                 << rs->GetVariant(2).GetFloat() << "|"
                                 << rs->GetVariant("date_val").GetString() << "|"
                                 << endl;
                    }
                }
                break;
            default:
                break;
            }
        }

        stmt->ClearParamList();
        //exit(1);

        // Reconnect
        NcbiCout << endl << "Reconnecting..." << endl;

        delete conn;
        conn = ds->CreateConnection();

        conn->SetMode(IConnection::eBulkInsert);

        conn->Connect("anyone",
                      "allowed",
                      server,
                      "DBAPI_Sample");

        //conn->ForceSingle(true);

        stmt = conn->CreateStatement();

        NcbiCout << endl << "Reading BLOB..." << endl;

        // Read blob to vector
        vector<char> blob;

 
        NcbiCout << "Retrieve BLOBs using streams and reader" << endl;

        stmt->ExecuteUpdate("set textsize 2000000");
    
        stmt->SendSql("select str_val, text_val, text_val \
from SelectSample where int_val = 1");
    
        while( stmt->HasMoreResults() ) { 
            if( stmt->HasRows() ) {
                IResultSet *rs = stmt->GetResultSet();
                int size = 0;
                while(rs->Next()) { 
                    NcbiCout << "Reading: " << rs->GetVariant(1).GetString() 
                             << endl;
                    istream& in1 = rs->GetBlobIStream();
                    int c = 0; 
                    NcbiCout << "Reading first blob with stream..." << endl;
                    for( ;(c = in1.get()) != CT_EOF; ++size ) {
                        blob.push_back(c);
                    }
                    IReader *ir = rs->GetBlobReader();
                    NcbiCout << "Reading second blob with reader..." << endl;
                    char rBuf[1000];
                    size_t bRead = 0;
                    while( ir->Read(rBuf, 999, &bRead) != eRW_Eof ) {
                        rBuf[bRead] = '\0';
                        for( int i = 0; rBuf[i] != '\0'; ++i ) { 
                            blob.push_back(rBuf[i]);
                        }
                    }
                } 
                NcbiCout << "Bytes read: " << blob.size() << endl;
            }
        }

        // create a table
        NcbiCout << endl << "Creating BlobSample table..." << endl;
        sql = "if exists( select * from sysobjects \
where name = 'BlobSample' \
AND type = 'U') \
begin \
	drop table BlobSample \
end";
        stmt->ExecuteUpdate(sql);


        sql = "create table BlobSample (\
	id int null, \
	blob2 text null, blob text null, unique (id))";
        stmt->ExecuteUpdate(sql);

        // Write BLOB several times
        const int COUNT = 5;

        // Create alternate blob storage
        char *buf = new char[blob.size()];
        char *p = buf;
        vector<char>::iterator i_b = blob.begin();
        for( ; i_b != blob.end(); ++i_b ) {
            *p++ = *i_b;
        }

        //Initialize table using bulk insert
        NcbiCout << "Initializing BlobSample table..." << endl;
        IBulkInsert *bi = conn->CreateBulkInsert("BlobSample");
        CVariant col1 = CVariant(eDB_Int);
        CVariant col2 = CVariant(eDB_Text);
        CVariant col3 = CVariant(eDB_Text);
        bi->Bind(1, &col1);
        bi->Bind(2, &col2);
        bi->Bind(3, &col3);

        for(int i = 0; i < COUNT; ++i ) {
            string im = "BLOB data " + NStr::IntToString(i);
            col1 = i;
            col2.Truncate();
            col2.Append(im.c_str(), im.size());
            col3.Truncate();
            col3.Append(im.c_str(), im.size());
            bi->AddRow();
        }
		//NcbiCout << "Blob size: " << col3.GetBlobSize() << endl;

        bi->Complete();

        delete bi;

        NcbiCout << "Writing BLOB using cursor and streams..." << endl;

        ICursor *blobCur = conn->CreateCursor("test", 
           "select id, blob from BlobSample for update of blob");
    
        IResultSet *blobRs = blobCur->Open();
        while(blobRs->Next()) {
                NcbiCout << "Writing BLOB " << blobRs->GetVariant(1).GetInt4() << endl;
                ostream& out = blobCur->GetBlobOStream(2, blob.size(), eDisableLog);
                out.write(buf, blob.size());
                out.flush();
        }
     
        //blobCur->Close();
        delete blobCur;

        NcbiCout << "Writing BLOB using cursor and writer..." << endl;

        blobCur = conn->CreateCursor("test", 
           "select id, blob from BlobSample for update of blob");
    
        blobRs = blobCur->Open();
        while(blobRs->Next()) {
                NcbiCout << "Writing BLOB " << blobRs->GetVariant(1).GetInt4() << endl;
                IWriter *wr = blobCur->GetBlobWriter(2, blob.size(), eDisableLog);
                wr->Write(buf, blob.size());
        }
     
        //blobCur->Close();
        delete blobCur;

#if 0 // Not supported by ODBC driver
        NcbiCout << "Writing BLOB using resultset..." << endl;

        sql = "select id, blob from BlobSample";
        //if( NStr::CompareNocase(driver, "ctlib") == 0 )
        //    sql += " at isolation read uncommitted";

        stmt->SendSql(sql);
        IConnection *newConn = conn->CloneConnection();

        cnt = 0;
        while( stmt->HasMoreResults() ) {
            if( stmt->HasRows() ) {
                IResultSet *rs = stmt->GetResultSet();
                while(rs->Next()) {
                    NcbiCout << "Writing BLOB " << ++cnt << endl;
                    ostream& out = rs->GetBlobOStream(newConn, blob.size(), eDisableLog);
                    out.write(buf, blob.size());
                    out.flush();
                }
            }
        }

        delete newConn;
#endif

        delete buf;

        // check if Blob is there
        stmt = conn->CreateStatement();
        NcbiCout << "Checking BLOB size..." << endl;
        stmt->SendSql("select 'Written blob size' as size, datalength(blob) \
from BlobSample where id = 1");
        
        while( stmt->HasMoreResults() ) {
            if( stmt->HasRows() ) {
                IResultSet *rs = stmt->GetResultSet();
                while(rs->Next()) {
                    NcbiCout << rs->GetVariant(1).GetString() << ": " 
                             << rs->GetVariant(2).GetInt4() << endl;
                }
            }
        }

        // Reading text blobs
        NcbiCout << "Reading text BLOB..." << endl;
        IResultSet *brs = stmt->ExecuteQuery("select id, blob2, blob from BlobSample");
        brs->BindBlobToVariant(true);
        char *bbuf = new char[50000];
        while(brs->Next()) {
            NcbiCout << brs->GetVariant(1).GetString() << "|";
            //CNcbiIstream &is = brs->GetBlobIStream();
            //while( !is.eof() ) {
            //    NcbiCout << is.get();
            //}
            //NcbiCout << "|";
            //CNcbiIstream &is2 = brs->GetBlobIStream();
            //while( !is2.eof() ) {
            //    NcbiCout << (char)is2.get();
            //}
            //NcbiCout << endl;

            if( brs->GetVariant(2).IsNull() ) {
                NcbiCout << " null ";
            }
            const CVariant &v = brs->GetVariant(3);
            v.Read(bbuf, v.GetBlobSize());
            bbuf[v.GetBlobSize()] = '\0';
            NcbiCout << bbuf << endl;

        }
        delete[] bbuf;

        // Cursor test (remove blob)
        NcbiCout << "Cursor test, removing blobs" << endl;

        ICursor *cur = conn->CreateCursor("test", 
                                          "select id, blob from BlobSample for update of blob");
    
        IResultSet *rs = cur->Open();
        while(rs->Next()) {
            cur->Update("BlobSample", "update BlobSample set blob = ' '");
        }
     
        //cur->Close();
        delete cur;

        // ExecuteUpdate rowcount test
        NcbiCout << "Rowcount test..." << endl;
        sql = "update BlobSample set blob ='deleted'";
        stmt->ExecuteUpdate(sql);
        NcbiCout << "Rows updated: " << stmt->GetRowCount() << endl;

        // drop BlobSample table
        NcbiCout << "Deleting BlobSample table..." << endl;
        sql = "drop table BlobSample";
        stmt->ExecuteUpdate(sql);
        NcbiCout << "Done." << endl;

		delete conn;
    }
    catch(out_of_range) {
        NcbiCout << "Exception: Out of range" << endl;
        return 1;
    }
    catch(exception& e) {
        NcbiCout << "Exception: " << e.what() << endl;
        NcbiCout << ds->GetErrorInfo();
        return 1;
    }


    return 0;
}

void CDbapiTest::Exit()
{

}


int main(int argc, const char* argv[])
{
    return CDbapiTest().AppMain(argc, argv);
}
