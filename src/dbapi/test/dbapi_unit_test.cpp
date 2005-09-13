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

#include <ncbi_pch.hpp>
#include <corelib/ncbiargs.hpp>

#include <boost/test/unit_test_result.hpp>

#include <dbapi/dbapi.hpp>

#include "dbapi_unit_test.hpp"

#include <test/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE

enum { max_text_size = 8000 };
#define CONN_OWNERSHIP  eTakeOwnership
static const char* msg_record_expected = "Record is expected";

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// Patterns to test:
//      I) Statement:
//          1) New/dedicated statement for each test
//          2) Reusable statement for all tests

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
CTestTransaction::CTestTransaction(
    IConnection& conn,
    ETransBehavior tb
    )
    : m_Conn( &conn )
    , m_TransBehavior( tb )
{
    if ( m_TransBehavior != eNoTrans ) {
        auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
        stmt->ExecuteUpdate( "BEGIN TRANSACTION" );
    }
}

CTestTransaction::~CTestTransaction(void)
{
    try {
        if ( m_TransBehavior == eTransCommit ) {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
            stmt->ExecuteUpdate( "COMMIT TRANSACTION" );
        } else if ( m_TransBehavior == eTransRollback ) {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
            stmt->ExecuteUpdate( "ROLLBACK TRANSACTION" );
        }
    }
    catch( ... ) {
        // Just ignore ...
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
class CIConnectionIFPolicy
{
protected:
    static void Destroy(IConnection* obj)
    {
        delete obj;
    }
    static IConnection* Init(IDataSource* ds)
    {
        return ds->CreateConnection( CONN_OWNERSHIP );
    }
};

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
CDBAPIUnitTest::CDBAPIUnitTest(const CTestArguments& args)
    : m_args(args)
    , m_DM( CDriverManager::GetInstance() )
    , m_DS( NULL )
    , m_TableName( "#dbapi_unit_table" )
{
    SetDiagFilter(eDiagFilter_All, "!/dbapi/driver/ctlib");
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void 
CDBAPIUnitTest::TestInit(void)
{
    if ( m_args.GetServerType() == CTestArguments::eMsSql ) {
        m_max_varchar_size = 8000;
    } else {
        // Sybase
        m_max_varchar_size = 1900;
    }
    
    m_DS = m_DM.CreateDs( m_args.GetDriverName(), &m_args.GetDBParameters() );

    m_Conn.reset( m_DS->CreateConnection( CONN_OWNERSHIP ) );
    BOOST_CHECK( m_Conn.get() != NULL );
    
    m_Conn->SetMode(IConnection::eBulkInsert);
    
    m_Conn->Connect( 
        m_args.GetUserName(), 
        m_args.GetUserPassword(), 
        m_args.GetServerName(), 
        m_args.GetDatabaseName() 
        );

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Create a test table ...
    string sql;

    sql  = " CREATE TABLE " + GetTableName() + "( \n";
    sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
    sql += "    int_field INT NOT NULL, \n";
    if ( m_args.GetServerName() != "MOZART" ) {
        sql += "    vc1000_field VARCHAR(1000) NULL, \n";
    }
    sql += "    text_field TEXT NULL \n";
    sql += " )";

    // Create the table
    auto_stmt->ExecuteUpdate(sql);
    
    sql  = " CREATE UNIQUE INDEX #ind01 ON " + GetTableName() + "( id ) \n";
    // Create an index
    auto_stmt->ExecuteUpdate( sql );
    
    // Table for bulk insert ...
    if ( m_args.GetServerType() == CTestArguments::eMsSql ) {
        sql  = " CREATE TABLE #bulk_insert_table( \n";
        sql += "    id INT PRIMARY KEY, \n";
        sql += "    vc8000_field VARCHAR(8000) NULL, \n";
        sql += "    int_field INT NULL, \n";
        sql += "    bigint_field BIGINT NULL \n";
        sql += " )";

        // Create the table
        auto_stmt->ExecuteUpdate(sql);
        
        sql  = " CREATE TABLE #bin_bulk_insert_table( \n";
        sql += "    id INT PRIMARY KEY, \n";
        sql += "    vb8000_field VARBINARY(8000) NULL, \n";
        sql += " )";

        // Create the table
        auto_stmt->ExecuteUpdate(sql);
    } else
    {
        if ( m_args.GetServerName() != "MOZART" ) {
            sql  = " CREATE TABLE #bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vc8000_field VARCHAR(1900) NULL, \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
            
            sql  = " CREATE TABLE #bin_bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vb8000_field VARBINARY(1900) NULL \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        }
    }
}

class CTestErrHandler : public CDB_UserHandler
{
public:
    // c-tor
    CTestErrHandler();
    // d-tor
    virtual ~CTestErrHandler();

public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex);

    // Get current global "last-resort" error handler.
    // If not set, then the default will be "CDB_UserHandler_Default".
    // This handler is guaranteed to be valid up to the program termination,
    // and it will call the user-defined hs_Procandler last set by SetDefault().
    // NOTE:  never pass it to SetDefault, like:  "SetDefault(&GetDefault())"!
    static CDB_UserHandler& GetDefault(void);

    // Alternate the default global "last-resort" error handler.
    // Passing NULL will mean to ignore all errors that reach it.
    // Return previously set (or default-default if not set yet) handler.
    // The returned handler should be delete'd by the caller; the last set
    // handler will be delete'd automagically on the program termination.
    static CDB_UserHandler* SetDefault(CDB_UserHandler* h);

public:
    EDB_Severity getMaxSeverity() {
        return m_max_severity;
    }
    bool GetSucceed(void) const
    {
        return m_Succeed;
    }
    void Init(void)
    {
        m_Succeed = false;
    }

private:
    EDB_Severity    m_max_severity;
    bool            m_Succeed;
};

CTestErrHandler::CTestErrHandler()
: m_max_severity(eDB_Info)
, m_Succeed(false)
{
}

CTestErrHandler::~CTestErrHandler()
{
   ;
}

bool CTestErrHandler::HandleIt(CDB_Exception* ex)
{
    m_Succeed = true;

    if (ex && ex->Severity() > m_max_severity)
    {
        m_max_severity= ex->Severity();
    }

    // return false to find the next handler in the stack
    // there always has one default stack

    return false;
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void 
CDBAPIUnitTest::Test_LOB(void)
{
    static char clob_value[] = "1234567890";
    string sql;
    
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
    
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
        
        auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));

        // blobRs should be destroyed before auto_cursor ...
        auto_ptr<IResultSet> blobRs(auto_cursor->Open());
        while(blobRs->Next()) {
            ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob_value) - 1, eDisableLog);
            out.write(clob_value, sizeof(clob_value) - 1);
            out.flush();
        }
        
//         auto_stmt->Execute( sql );
//         while( auto_stmt->HasMoreResults() ) {
//             if( auto_stmt->HasRows() ) {
//                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
//                 while ( rs->Next() ) {
//                     ostream& out = rs->GetBlobOStream(sizeof(clob_value) - 1, eDisableLog);
//                     out.write(clob_value, sizeof(clob_value) - 1);
//                     out.flush();
//                 }
//             }
//         }
    }
    
    // Retrieve data ...
    {
        sql = "SELECT text_field FROM "+ GetTableName();
        
        auto_stmt->Execute( sql );
        while( auto_stmt->HasMoreResults() ) { 
            if( auto_stmt->HasRows() ) { 
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet()); 
                
                rs->BindBlobToVariant(true);
                
                while ( rs->Next() ) {
                    const CVariant& value = rs->GetVariant(1);


                    BOOST_CHECK( !value.IsNull() );

                    size_t blob_size = value.GetBlobSize();
                    BOOST_CHECK_EQUAL(sizeof(clob_value) - 1, blob_size);
                    // Int8 value = rs->GetVariant(1).GetInt8();
                }
            } 
        }
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void 
CDBAPIUnitTest::Test_GetColumnNo(void)
{ 
    string sql;
    
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
    
    // Create table ...
    {
        sql = 
            "CREATE TABLE #Overlaps ( \n"
            "	pairId int NOT NULL , \n"
            "	overlapNum smallint NOT NULL , \n"
            "	start1 int NOT NULL , \n"
            "	start2 int NOT NULL , \n"
            "	stop1 int NOT NULL , \n"
            "	stop2 int NOT NULL , \n"
            "	orient char (2) NOT NULL , \n"
            "	gaps int NOT NULL , \n"
            "	mismatches int NOT NULL , \n"
            "	adjustedLen int NOT NULL , \n"
            "	length int NOT NULL , \n"
            "	contained tinyint NOT NULL , \n"
            "	seq_align text  NULL , \n"
            "	merged_sa char (1) NOT NULL , \n"
            "	CONSTRAINT PK_Overlaps PRIMARY KEY CLUSTERED  \n"
            "	( \n"
            "		pairId, \n"
            "		overlapNum \n"
            "	) \n"
            ") \n";
        
        auto_stmt->ExecuteUpdate( sql );
    }
    
    // Insert data into the table ...
    {
        sql = 
            "INSERT INTO #Overlaps VALUES( \n"
            "1, 1, 0, 25794, 7126, 32916, '--', 1, 21, 7124, 7127, 0, \n"
            "'Seq-align ::= { }', 'n')";
        
        auto_stmt->ExecuteUpdate( sql );
    }
    
    // Actual check ...
    {
        sql = "SELECT * FROM #Overlaps";
        
        auto_stmt->Execute( sql );
        while( auto_stmt->HasMoreResults() ) { 
            if( auto_stmt->HasRows() ) { 
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet()); 
                int col_num = -2;
                
//                 switch ( rs->GetResultType() ) {
//                 case eDB_RowResult:
//                     col_num = rs->GetColumnNo();
//                     break;
//                 case eDB_ParamResult:
//                     col_num = rs->GetColumnNo();
//                     break;
//                 case eDB_StatusResult:
//                     col_num = rs->GetColumnNo();
//                     break;
//                 case eDB_ComputeResult:
//                 case eDB_CursorResult:
//                     col_num = rs->GetColumnNo();
//                     break;
//                 }
                
                col_num = rs->GetColumnNo();
                BOOST_CHECK_EQUAL( 14, col_num );
            } 
        }
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void 
BulkAddRow(const auto_ptr<IBulkInsert>& bi, const CVariant& col)
{
    string msg(8000, 'A');
    CVariant col2(col);
    
    bi->Bind(2, &col2);
    col2 = msg;
    bi->AddRow();
}

void
CDBAPIUnitTest::DumpResults(const auto_ptr<IStatement>& auto_stmt)
{
    while ( auto_stmt->HasMoreResults() ) {
        if ( auto_stmt->HasRows() ) {
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() ); 
        }
    }
}

void
CDBAPIUnitTest::Test_Bulk_Writing(void)
{
    string sql;
    
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
    
    
    // VARBINARY ...
    {
        enum { num_of_tests = 10 };
        const char char_val('2');
        
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #bin_bulk_insert_table" );
        
        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi( m_Conn->GetBulkInsert("#bin_bulk_insert_table", 2) );
            
            CVariant col1(eDB_Int);
            CVariant col2(eDB_LongBinary, m_max_varchar_size);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);
            
            for(int i = 0; i < num_of_tests; ++i ) {
                int int_value = m_max_varchar_size / num_of_tests * i;
                string str_val(int_value , char_val);
                
                col1 = int_value;
                col2 = CVariant::LongBinary(m_max_varchar_size, str_val.c_str(), str_val.size());
                bi->AddRow();
            }
            bi->Complete();
        }
        
        // Retrieve data ...
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            sql  = " SELECT id, vb8000_field FROM #bin_bulk_insert_table";
            sql += " ORDER BY id";

            auto_stmt->Execute( sql );
            
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() ); 
            BOOST_CHECK( rs.get() );

            for(int i = 0; i < num_of_tests; ++i ) {
                BOOST_CHECK( rs->Next() );
                
                int int_value = m_max_varchar_size / num_of_tests * i;
                Int4 id = rs->GetVariant(1).GetInt4();
                string vb8000_value = rs->GetVariant(2).GetString(); 
                
                BOOST_CHECK_EQUAL( int_value, id );
                BOOST_CHECK_EQUAL( string::size_type(int_value), vb8000_value.size() );
            }

            // Dump results ...
            DumpResults( auto_stmt );
        }
    }
    
    // INT, BIGINT
    {
        
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );
        
        // INT collumn ...
        {
            enum { num_of_tests = 8 };

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi( m_Conn->GetBulkInsert("#bulk_insert_table", 3) );
                
                CVariant col1(eDB_Int);
                CVariant col2(eDB_Int);

                bi->Bind(1, &col1);
                bi->Bind(3, &col2);

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
                sql  = " SELECT int_field FROM #bulk_insert_table";
                sql += " ORDER BY id";
                
                auto_stmt->Execute( sql );
                
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() ); 
                BOOST_CHECK( rs.get() );
                
                for(int i = 0; i < num_of_tests; ++i ) {
                    BOOST_CHECK( rs->Next() );
                    Int4 value = rs->GetVariant(1).GetInt4();
                    Int4 expected_value = Int4( 1 ) << (i * 4);
                    BOOST_CHECK_EQUAL( expected_value, value );
                }
                
                // Dump results ...
                DumpResults( auto_stmt );
            }
        }
        
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );
        
        // BIGINT collumn ...
        {
            // There is a problem at least with the ftds driver ...
            // enum { num_of_tests = 16 }; 
            enum { num_of_tests = 14 };

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi( m_Conn->GetBulkInsert("#bulk_insert_table", 4) );
                
                CVariant col1(eDB_Int);
                CVariant col2(eDB_BigInt);

                bi->Bind(1, &col1);
                bi->Bind(4, &col2);

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
                sql  = " SELECT bigint_field FROM #bulk_insert_table";
                sql += " ORDER BY id";
                
                auto_stmt->Execute( sql );
                
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() ); 
                BOOST_CHECK( rs.get() );
                
                for(int i = 0; i < num_of_tests; ++i ) {
                    BOOST_CHECK( rs->Next() );
                    Int8 value = rs->GetVariant(1).GetInt8();
                    Int8 expected_value = Int8( 1 ) << (i * 4);
                    BOOST_CHECK_EQUAL( expected_value, value );
                }
                
                // Dump results ...
                DumpResults( auto_stmt );
            }
        }
    }

    // Yet another BIGINT test ...
    {
        auto_ptr<IStatement> stmt( m_Conn->CreateStatement() );
        
        stmt->ExecuteUpdate(
            "create table #__blki_test ( name char(32) not null, value bigint null )" );
        stmt->Close();
        
        auto_ptr<IBulkInsert> blki( m_Conn->CreateBulkInsert("#__blki_test", 2) );
        
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
    
    // VARCHAR ...
    {
        int num_of_tests;
        
        if ( m_args.GetServerType() == CTestArguments::eMsSql ) {
            num_of_tests = 7;
        } else {
            // Sybase
            num_of_tests = 3;
        }
        
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );
        
        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi( m_Conn->GetBulkInsert("#bulk_insert_table", 2) );
            
            CVariant col1(eDB_Int);

            bi->Bind(1, &col1);

            for(int i = 0; i < num_of_tests; ++i ) {
                col1 = i;
                switch (i) {
                case 0:
                    BulkAddRow(bi, CVariant(eDB_VarChar));
                    break;
                case 1:
                    BulkAddRow(bi, CVariant(eDB_LongChar, m_max_varchar_size));
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
        {
            sql  = " SELECT id, vc8000_field FROM #bulk_insert_table";
            sql += " ORDER BY id";

            auto_stmt->Execute( sql );
            while( auto_stmt->HasMoreResults() ) { 
                if( auto_stmt->HasRows() ) { 
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet()); 

                    // Retrieve results, if any 
                    while( rs->Next() ) { 
                        Int4 i = rs->GetVariant(1).GetInt4();
                        string col1 = rs->GetVariant(2).GetString(); 
                        
                        switch (i) {
                        case 0:
                            BOOST_CHECK_EQUAL(col1.size(), string::size_type(255));
                            break;
                        case 1:
                            BOOST_CHECK_EQUAL(col1.size(), string::size_type(m_max_varchar_size));
                            break;
                        case 2:
                            BOOST_CHECK_EQUAL(col1.size(), string::size_type(1024));
                            break;
                        case 3:
                            BOOST_CHECK_EQUAL(col1.size(), string::size_type(4112));
                            break;
                        case 4:
                            BOOST_CHECK_EQUAL(col1.size(), string::size_type(4113));
                            break;
                        case 5:
                            BOOST_CHECK_EQUAL(col1.size(), string::size_type(4138));
                            break;
                        case 6:
                            BOOST_CHECK_EQUAL(col1.size(), string::size_type(4139));
                            break;
                        };
                    } 
                } 
            }
        }
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::Test_Variant2(void)
{
    const long rec_num = 3;
    const size_t size_step = 10;
    string sql;

    // Initialize a test table ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Drop all records ...
        sql  = " DELETE FROM " + GetTableName();
        auto_stmt->ExecuteUpdate(sql);

        // Insert new vc1000_field records ...
        sql  = " INSERT INTO " + GetTableName();
        sql += "(int_field, vc1000_field) VALUES(@id, @val) \n";

        string::size_type str_size(10);
        char char_val('1');
        for (long i = 0; i < rec_num; ++i) {
            string str_val(str_size, char_val);
            str_size *= size_step;
            char_val += 1;
            
            auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
            auto_stmt->SetParam( CVariant( str_val ), "@val" );
            // Execute a statement with parameters ...
            auto_stmt->ExecuteUpdate( sql );
        }
    }
    
    // Test VarChar ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        
        sql  = "SELECT vc1000_field FROM " + GetTableName();
        sql += " ORDER BY int_field";
        
        string::size_type str_size(10);
        auto_stmt->Execute( sql );
        while( auto_stmt->HasMoreResults() ) { 
            if( auto_stmt->HasRows() ) { 
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet()); 
                
                // Retrieve results, if any 
                while( rs->Next() ) { 
                    string col1 = rs->GetVariant(1).GetString(); 
                    BOOST_CHECK(col1.size() == str_size || col1.size() == 255);
                    str_size *= size_step;
                } 
            } 
        }
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void 
CDBAPIUnitTest::Test_Cursor(void)
{
    const long rec_num = 2;
    string sql;

    // Initialize a test table ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Drop all records ...
        sql  = " DELETE FROM " + GetTableName();
        auto_stmt->ExecuteUpdate(sql);

        // Insert new LOB records ...
        sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field) VALUES(@id, '') \n";

        // CVariant variant(eDB_Text);
        // variant.Append(" ", 1);

        for (long i = 0; i < rec_num; ++i) {
            auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
            // Execute a statement with parameters ...
            auto_stmt->ExecuteUpdate( sql );
        }
    }

    // Test CLOB field update ...
    {
        const char* clob = "abc";

        sql = "select text_field from " + GetTableName() + " for update of text_field \n";
        auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", sql));

        {
            // blobRs should be destroyed before auto_cursor ...
            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            while(blobRs->Next()) {
                ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
                out.write(clob, sizeof(clob) - 1);
                out.flush();
            }
            blobRs.release();
        }

        // Another cursor ...
        sql  = " select text_field from " + GetTableName();
        sql += " where int_field = 1 for update of text_field";

        auto_cursor.reset(m_Conn->GetCursor("test02", sql));
        {
            // blobRs should be destroyed before auto_cursor ...
            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            if ( !blobRs->Next() ) {
                BOOST_FAIL( msg_record_expected ); 
            }
            ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
            out.write(clob, sizeof(clob) - 1);
            out.flush();
        }
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::Test_SelectStmt(void)
{
    // Scenario:
    // 1) Select recordset with just one record
    // 2) Retrive only one record.
    // 3) Select another recordset with just one record
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        auto_ptr<IResultSet> rs;

        // 1) Select recordset with just one record
        rs.reset( auto_stmt->ExecuteQuery( "select qq = 57 + 33" ) );
        BOOST_CHECK( rs.get() != NULL );

        // 2) Retrive only one record.
        if ( !rs->Next() ) {
            BOOST_FAIL( msg_record_expected ); 
        }

        // 3) Select another recordset with just one record
        rs.reset( auto_stmt->ExecuteQuery( "select qq = 57.55 + 0.0033" ) );
        BOOST_CHECK( rs.get() != NULL );
    }

    // Same as before but uses two differenr connections ...
    if (false) {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        auto_ptr<IResultSet> rs;

        // 1) Select recordset with just one record
        rs.reset( auto_stmt->ExecuteQuery( "select qq = 57 + 33" ) );
        BOOST_CHECK( rs.get() != NULL );

        // 2) Retrive only one record.
        if ( !rs->Next() ) {
            BOOST_FAIL( msg_record_expected ); 
        }

        // 3) Select another recordset with just one record
        auto_ptr<IStatement> auto_stmt2( m_Conn->CreateStatement() );
        rs.reset( auto_stmt2->ExecuteQuery( "select qq = 57.55 + 0.0033" ) );
        BOOST_CHECK( rs.get() != NULL );
    }
    
//     // TMP
//     {
//         auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
//         BOOST_CHECK( conn.get() != NULL );
//
//         conn->SetMode(IConnection::eBulkInsert);
//
//         conn->Connect(
//             "anyone",
//             "allowed",
//             "MSSQL10",
//             "gMapDB"
//             );
//
//         auto_ptr<IStatement> auto_stmt( conn->GetStatement() );
//
//         string sql;
//
//         sql  = "SELECT len(seq_loc) l, seq_loc FROM protein where ProtGi =56964519";
//
//         auto_stmt->Execute( sql );
//         while( auto_stmt->HasMoreResults() ) {
//             if( auto_stmt->HasRows() ) {
//                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
//
//                 // Retrieve results, if any
//                 while( rs->Next() ) {
//                     string col2 = rs->GetVariant(2).GetString();
//                     BOOST_CHECK_EQUAL(col2.size(), 355);
//                 }
//             }
//         }
//     }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::Test_SelectStmtXML(void)
{
    // SQL + XML
    {
        string sql;
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        auto_ptr<IResultSet> rs;

        sql = "select 1 as Tag, null as Parent, 1 as [x!1!id] for xml explicit";
        rs.reset( auto_stmt->ExecuteQuery( sql ) );
        BOOST_CHECK( rs.get() != NULL );

        if ( !rs->Next() ) {
            BOOST_FAIL( msg_record_expected ); 
        }

        // Same but call Execute instead of ExecuteQuery.
        auto_stmt->Execute( sql );
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::Test_UserErrorHandler(void)
{
    // Set up an user-defined error handler ..

    // Push message handler into a context ...
    I_DriverContext* drv_context = m_DS->GetDriverContext();

    // Check PushCntxMsgHandler ...
    // PushCntxMsgHandler - Add message handler "h" to process 'context-wide' (not bound
    // to any particular connection) error messages.
    {
        CTestErrHandler* drv_err_handler = new CTestErrHandler();
        auto_ptr<IConnection> local_conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
        local_conn->Connect( 
            m_args.GetUserName(), 
            m_args.GetUserPassword(), 
            m_args.GetServerName(), 
            m_args.GetDatabaseName() 
            );

        drv_context->PushCntxMsgHandler( drv_err_handler );

        // Connection process should be affected ...
        {
            // Create a new connection ...
            auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );

            try {
                conn->Connect( 
                    "unknown", 
                    "invalid", 
                    m_args.GetServerName(), 
                    m_args.GetDatabaseName() 
                    );
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( drv_err_handler->GetSucceed() );
            }

            drv_err_handler->Init();
        }

        // Current connection should not be affected ...
        {
            try {
                Test_ES_01(*local_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( !drv_err_handler->GetSucceed() );
            }
        }

        // New connection should not be affected ...
        {
            // Create a new connection ...
            auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
            conn->Connect( 
                m_args.GetUserName(), 
                m_args.GetUserPassword(), 
                m_args.GetServerName(), 
                m_args.GetDatabaseName() 
                );

            // Reinit the errot handler because it can be affected during connection.
            drv_err_handler->Init();

            try {
                Test_ES_01(*conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( !drv_err_handler->GetSucceed() );
            }
        }

        // Push a message handler into a connection ...
        {
            CTestErrHandler* err_handler = new CTestErrHandler();

            local_conn->GetCDB_Connection()->PushMsgHandler(err_handler);

            try {
                Test_ES_01(*local_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( err_handler->GetSucceed() );
            }
        }

        // New connection should not be affected
        // after pushing a message handler into another connection
        {
            // Create a new connection ...
            auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
            conn->Connect( 
                m_args.GetUserName(), 
                m_args.GetUserPassword(), 
                m_args.GetServerName(), 
                m_args.GetDatabaseName() 
                );

            // Reinit the errot handler because it can be affected during connection.
            drv_err_handler->Init();

            try {
                Test_ES_01(*conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( !drv_err_handler->GetSucceed() );
            }
        }
    }


    // Check PushDefConnMsgHandler ...
    // PushDefConnMsgHandler - Add `per-connection' err.message handler "h" to the stack of default
    // handlers which are inherited by all newly created connections.
    {
        CTestErrHandler* drv_err_handler = new CTestErrHandler();
        auto_ptr<IConnection> local_conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
        local_conn->Connect( 
            m_args.GetUserName(), 
            m_args.GetUserPassword(), 
            m_args.GetServerName(), 
            m_args.GetDatabaseName() 
            );

        drv_context->PushDefConnMsgHandler( drv_err_handler );

        // Current connection should not be affected ...
        {
            try {
                Test_ES_01(*local_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( !drv_err_handler->GetSucceed() );
            }
        }

        // Push a message handler into a connection ...
        // This is supposed to be okay.
        {
            CTestErrHandler* err_handler = new CTestErrHandler();

            local_conn->GetCDB_Connection()->PushMsgHandler(err_handler);

            try {
                Test_ES_01(*local_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( err_handler->GetSucceed() );
            }
        }


        ////////////////////////////////////////////////////////////////////////
        // Create a new connection.
        auto_ptr<IConnection> new_conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
        new_conn->Connect( 
            m_args.GetUserName(), 
            m_args.GetUserPassword(), 
            m_args.GetServerName(), 
            m_args.GetDatabaseName() 
            );

        // New connection should be affected ...
        {
            try {
                Test_ES_01(*new_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( drv_err_handler->GetSucceed() );
            }
        }

        // Push a message handler into a connection ...
        // This is supposed to be okay.
        {
            CTestErrHandler* err_handler = new CTestErrHandler();

            new_conn->GetCDB_Connection()->PushMsgHandler(err_handler);

            try {
                Test_ES_01(*new_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( err_handler->GetSucceed() );
            }
        }

        // New connection should be affected
        // after pushing a message handler into another connection
        {
            // Create a new connection ...
            auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
            conn->Connect( 
                m_args.GetUserName(), 
                m_args.GetUserPassword(), 
                m_args.GetServerName(), 
                m_args.GetDatabaseName() 
                );

            try {
                Test_ES_01(*conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( drv_err_handler->GetSucceed() );
            }
        }
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::Test_Procedure(void)
{
    // Test a regular IStatement with "exec"
    // Parameters are not allowed with this construction.
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        
        // Execute it first time ...
        // auto_stmt->Execute( "exec sp_databases" );
        auto_stmt->Execute( "SELECT name FROM sysobjects" );
        while( auto_stmt->HasMoreResults() ) { 
            if( auto_stmt->HasRows() ) { 
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() ); 
                
                switch ( rs->GetResultType() ) {
                case eDB_RowResult:
                    while( rs->Next() ) { 
                        // int col1 = rs->GetVariant(1).GetInt4(); 
                    } 
                    break;
                case eDB_ParamResult:
                    while( rs->Next() ) { 
                        // int col1 = rs->GetVariant(1).GetInt4(); 
                    } 
                    break;
                case eDB_StatusResult:
                    while( rs->Next() ) { 
                        int status = rs->GetVariant(1).GetInt4(); 
                        status = status;
                    } 
                    break;
                case eDB_ComputeResult:
                case eDB_CursorResult:
                    break;
                }
            } 
        }
        
        // Execute it second time ...
        // auto_stmt->Execute( "exec sp_databases" );
        auto_stmt->Execute( "SELECT name FROM sysobjects" );
        while( auto_stmt->HasMoreResults() ) { 
            if( auto_stmt->HasRows() ) { 
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() ); 
                
                switch ( rs->GetResultType() ) {
                case eDB_RowResult:
                    while( rs->Next() ) { 
                        // int col1 = rs->GetVariant(1).GetInt4(); 
                    } 
                    break;
                case eDB_ParamResult:
                    while( rs->Next() ) { 
                        // int col1 = rs->GetVariant(1).GetInt4(); 
                    } 
                    break;
                case eDB_StatusResult:
                    while( rs->Next() ) { 
                        int status = rs->GetVariant(1).GetInt4(); 
                        status = status;
                    } 
                    break;
                case eDB_ComputeResult:
                case eDB_CursorResult:
                    break;
                }
            } 
        }
        
        // Same as before but do not retrieve data ...
        auto_stmt->Execute( "exec sp_databases" );
        auto_stmt->Execute( "exec sp_databases" );
    }

    // Test ICallableStatement
    // No parameters at this time.
    {
        // Execute it first time ...
        auto_ptr<ICallableStatement> auto_stmt( m_Conn->GetCallableStatement("sp_databases") );
        auto_stmt->Execute();
        while(auto_stmt->HasMoreResults()) { 
            if( auto_stmt->HasRows() ) { 
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                switch( rs->GetResultType() ) { 
                case eDB_RowResult: 
                    while(rs->Next()) { 
                        // retrieve row results 
                    } 
                    break; 
                case eDB_ParamResult: 
                    while(rs->Next()) { 
                        // Retrieve parameter row 
                    } 
                    break; 
                default:
                    break;
                } 
            } 
        } 
        // Get status 
        int status = auto_stmt->GetReturnStatus();


        // Execute it second time ...
        auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
        auto_stmt->Execute();
        while(auto_stmt->HasMoreResults()) { 
            if( auto_stmt->HasRows() ) { 
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                switch( rs->GetResultType() ) { 
                case eDB_RowResult: 
                    while(rs->Next()) { 
                        // retrieve row results 
                    } 
                    break; 
                case eDB_ParamResult: 
                    while(rs->Next()) { 
                        // Retrieve parameter row 
                    } 
                    break; 
                default:
                    break;
                } 
            } 
        } 
        // Get status 
        status = auto_stmt->GetReturnStatus();


        // Same as before but do not retrieve data ...
        auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
        auto_stmt->Execute();
        auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
        auto_stmt->Execute();
    }
    
    // Temporary test ...
    // !!! This is a bug ...
    if (false) {
        auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
        BOOST_CHECK( conn.get() != NULL );

        conn->Connect(
            "anyone",
            "allowed",
            "PUBSEQ_OS_LXA",
            ""
            );

        auto_ptr<ICallableStatement> auto_stmt( conn->GetCallableStatement("id_seqid4gi") );
        auto_stmt->SetParam( CVariant(1), "@gi" );
        auto_stmt->Execute();
        while(auto_stmt->HasMoreResults()) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                switch( rs->GetResultType() ) {
                case eDB_RowResult:
                    while(rs->Next()) {
                        // retrieve row results
                    }
                    break;
                case eDB_ParamResult:
                    _ASSERT(false);
                    while(rs->Next()) {
                        // Retrieve parameter row
                    }
                    break;
                default:
                    break;
                }
            }
        }
        // Get status 
        int status = auto_stmt->GetReturnStatus();
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::Test_Exception_Safety(void)
{
    // Very first test ...
    // Try to catch a base class ...
    BOOST_CHECK_THROW( Test_ES_01(*m_Conn), CDB_Exception );
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// Throw CDB_Exception ...
void 
CDBAPIUnitTest::Test_ES_01(IConnection& conn)
{
    auto_ptr<IStatement> auto_stmt( conn.GetStatement() );

    auto_stmt->Execute( "select name from wrong table" );
    while( auto_stmt->HasMoreResults() ) { 
        if( auto_stmt->HasRows() ) { 
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() ); 
            
            while( rs->Next() ) { 
                // int col1 = rs->GetVariant(1).GetInt4(); 
            } 
        } 
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void 
CDBAPIUnitTest::Test_StatementParameters(void)
{
    // Very first test ...
    {
        string sql;
        sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( @value ) \n";

        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        auto_stmt->SetParam( CVariant(0), "@value" );
        // Execute a statement with parameters ...
        auto_stmt->ExecuteUpdate( sql );

        // !!! Do not forget to clear a parameter list ....
        // Workaround for the ctlib driver ...
        auto_stmt->ClearParamList();

        sql  = " SELECT int_field, int_field FROM " + GetTableName() + " ORDER BY int_field";
        // Execute a statement without parameters ...
        auto_stmt->ExecuteUpdate( sql );
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::TestGetRowCount()
{
    enum {repeat_num = 5};
    for ( int i = 0; i < repeat_num; ++i ) {
        // Shared/Reusable statement
        {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );

            CheckGetRowCount( i, eNoTrans, stmt.get() );
            CheckGetRowCount( i, eTransCommit, stmt.get() );
            CheckGetRowCount( i, eTransRollback, stmt.get() );
        }
        // Dedicated statement
        {
            CheckGetRowCount( i, eNoTrans );
            CheckGetRowCount( i, eTransCommit );
            CheckGetRowCount( i, eTransRollback );
        }
    }

    for ( int i = 0; i < repeat_num; ++i ) {
        // Shared/Reusable statement
        {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );

            CheckGetRowCount2( i, eNoTrans, stmt.get() );
            CheckGetRowCount2( i, eTransCommit, stmt.get() );
            CheckGetRowCount2( i, eTransRollback, stmt.get() );
        }
        // Dedicated statement
        {
            CheckGetRowCount2( i, eNoTrans );
            CheckGetRowCount2( i, eTransCommit );
            CheckGetRowCount2( i, eTransRollback );
        }
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::CheckGetRowCount(
    int row_count, 
    ETransBehavior tb,
    IStatement* stmt
    )
{
    // Transaction ...
    CTestTransaction transaction(*m_Conn, tb);
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( 1 ) \n";

    // Insert row_count records into the table ...
    for ( int i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->ExecuteUpdate(sql);
        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows, 1 );
    }

    // Check a SELECT statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check an UPDATE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " UPDATE " + GetTableName() + " SET int_field = 0 ";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName() + " WHERE int_field = 0";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a DELETE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again and again ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( 0, nRows );
    }

}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::CheckGetRowCount2(
    int row_count, 
    ETransBehavior tb,
    IStatement* stmt
    )
{
    // Transaction ...
    CTestTransaction transaction(*m_Conn, tb);
    // auto_ptr<IStatement> stmt( m_Conn->CreateStatement() );
    // _ASSERT(curr_stmt->get());
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( @value ) \n";

    // Insert row_count records into the table ...
    for ( Int4 i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->SetParam(CVariant(i), "@value");
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows, 1 );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows2, 1 );
    }

    // Workaround for the CTLIB driver ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->ClearParamList();
    }

    // Check a SELECT statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field, int_field FROM " + GetTableName() + " ORDER BY int_field";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check an UPDATE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " UPDATE " + GetTableName() + " SET int_field = 0 ";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a SELECT statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field, int_field FROM " + GetTableName() + " WHERE int_field = 0";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a DELETE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a DELETE statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows2 );
    }

    // Check a SELECT statement again and again ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field, int_field FROM " + GetTableName() + " ORDER BY int_field";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows2 );
    }

}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::Test_Variant(void)
{
    const Uint1 value_Uint1 = 1;
    const Int2 value_Int2 = -2;
    const Int4 value_Int4 = -4;
    const Int8 value_Int8 = -8;
    const float value_float = float(0.4);
    const double value_double = 0.8;
    const bool value_bool = false;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );

    // Check constructors
    {
        const CVariant variant_Int8( value_Int8 );
        BOOST_CHECK( !variant_Int8.IsNull() );

        const CVariant variant_Int4( value_Int4 );
        BOOST_CHECK( !variant_Int4.IsNull() );

        const CVariant variant_Int2( value_Int2 );
        BOOST_CHECK( !variant_Int2.IsNull() );

        const CVariant variant_Uint1( value_Uint1 );
        BOOST_CHECK( !variant_Uint1.IsNull() );
        
        const CVariant variant_float( value_float );
        BOOST_CHECK( !variant_float.IsNull() );

        const CVariant variant_double( value_double );
        BOOST_CHECK( !variant_double.IsNull() );

        const CVariant variant_bool( value_bool );
        BOOST_CHECK( !variant_bool.IsNull() );

        const CVariant variant_string( value_string );
        BOOST_CHECK( !variant_string.IsNull() );

        const CVariant variant_char( value_char );
        BOOST_CHECK( !variant_char.IsNull() );

        const CVariant variant_CTimeShort( value_CTime, eShort );
        BOOST_CHECK( !variant_CTimeShort.IsNull() );

        const CVariant variant_CTimeLong( value_CTime, eLong );
        BOOST_CHECK( !variant_CTimeLong.IsNull() );

//        explicit CVariant(CDB_Object* obj);

    }

    // Check the CVariant(EDB_Type type, size_t size = 0) constructor
    {
//        enum EDB_Type {
//            eDB_Int,
//            eDB_SmallInt,
//            eDB_TinyInt,
//            eDB_BigInt,
//            eDB_VarChar,
//            eDB_Char,
//            eDB_VarBinary,
//            eDB_Binary,
//            eDB_Float,
//            eDB_Double,
//            eDB_DateTime,
//            eDB_SmallDateTime,
//            eDB_Text,
//            eDB_Image,
//            eDB_Bit,
//            eDB_Numeric,

//            eDB_UnsupportedType,
//            eDB_LongChar,
//            eDB_LongBinary
//        };

        {
            CVariant value_variant( eDB_Int );

            BOOST_CHECK_EQUAL( eDB_Int, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_SmallInt );

            BOOST_CHECK_EQUAL( eDB_SmallInt, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_TinyInt );

            BOOST_CHECK_EQUAL( eDB_TinyInt, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_BigInt );

            BOOST_CHECK_EQUAL( eDB_BigInt, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_VarChar );

            BOOST_CHECK_EQUAL( eDB_VarChar, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Char, max_text_size );

            BOOST_CHECK_EQUAL( eDB_Char, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_VarBinary );

            BOOST_CHECK_EQUAL( eDB_VarBinary, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Binary, max_text_size );

            BOOST_CHECK_EQUAL( eDB_Binary, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Float );

            BOOST_CHECK_EQUAL( eDB_Float, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Double );

            BOOST_CHECK_EQUAL( eDB_Double, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_DateTime );

            BOOST_CHECK_EQUAL( eDB_DateTime, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_SmallDateTime );

            BOOST_CHECK_EQUAL( eDB_SmallDateTime, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Text );

            BOOST_CHECK_EQUAL( eDB_Text, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Image );

            BOOST_CHECK_EQUAL( eDB_Image, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Bit );

            BOOST_CHECK_EQUAL( eDB_Bit, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Numeric );

            BOOST_CHECK_EQUAL( eDB_Numeric, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_LongChar, max_text_size );

            BOOST_CHECK_EQUAL( eDB_LongChar, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_LongBinary, max_text_size );

            BOOST_CHECK_EQUAL( eDB_LongBinary, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        if (false) {
            CVariant value_variant( eDB_UnsupportedType );

            BOOST_CHECK_EQUAL( eDB_UnsupportedType, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
    }

    // Check the copy-constructor CVariant(const CVariant& v)
    {
        const CVariant variant_Int8 = CVariant(value_Int8);
        BOOST_CHECK( !variant_Int8.IsNull() );

        const CVariant variant_Int4 = CVariant(value_Int4);
        BOOST_CHECK( !variant_Int4.IsNull() );

        const CVariant variant_Int2 = CVariant(value_Int2);
        BOOST_CHECK( !variant_Int2.IsNull() );

        const CVariant variant_Uint1 = CVariant(value_Uint1);
        BOOST_CHECK( !variant_Uint1.IsNull() );

        const CVariant variant_float = CVariant(value_float);
        BOOST_CHECK( !variant_float.IsNull() );

        const CVariant variant_double = CVariant(value_double);
        BOOST_CHECK( !variant_double.IsNull() );

        const CVariant variant_bool = CVariant(value_bool);
        BOOST_CHECK( !variant_bool.IsNull() );

        const CVariant variant_string = CVariant(value_string);
        BOOST_CHECK( !variant_string.IsNull() );

        const CVariant variant_char = CVariant(value_char);
        BOOST_CHECK( !variant_char.IsNull() );

        const CVariant variant_CTimeShort = CVariant( value_CTime, eShort );
        BOOST_CHECK( !variant_CTimeShort.IsNull() );

        const CVariant variant_CTimeLong = CVariant( value_CTime, eLong );
        BOOST_CHECK( !variant_CTimeLong.IsNull() );
    }

    // Call Factories for different types
    {
        const CVariant variant_Int8 = CVariant::BigInt( const_cast<Int8*>(&value_Int8) );
        BOOST_CHECK( !variant_Int8.IsNull() );

        const CVariant variant_Int4 = CVariant::Int( const_cast<Int4*>(&value_Int4) );
        BOOST_CHECK( !variant_Int4.IsNull() );

        const CVariant variant_Int2 = CVariant::SmallInt( const_cast<Int2*>(&value_Int2) );
        BOOST_CHECK( !variant_Int2.IsNull() );

        const CVariant variant_Uint1 = CVariant::TinyInt( const_cast<Uint1*>(&value_Uint1) );
        BOOST_CHECK( !variant_Uint1.IsNull() );

        const CVariant variant_float = CVariant::Float( const_cast<float*>(&value_float) );
        BOOST_CHECK( !variant_float.IsNull() );

        const CVariant variant_double = CVariant::Double( const_cast<double*>(&value_double) );
        BOOST_CHECK( !variant_double.IsNull() );

        const CVariant variant_bool = CVariant::Bit( const_cast<bool*>(&value_bool) );
        BOOST_CHECK( !variant_bool.IsNull() );

        const CVariant variant_LongChar = CVariant::LongChar( value_char, strlen(value_char) );
        BOOST_CHECK( !variant_LongChar.IsNull() );

        const CVariant variant_VarChar = CVariant::VarChar( value_char, strlen(value_char) );
        BOOST_CHECK( !variant_VarChar.IsNull() );

        const CVariant variant_Char = CVariant::Char( strlen(value_char), const_cast<char*>(value_char) );
        BOOST_CHECK( !variant_Char.IsNull() );

        const CVariant variant_LongBinary = CVariant::LongBinary( strlen(value_binary), value_binary, strlen(value_binary)) ;
        BOOST_CHECK( !variant_LongBinary.IsNull() );

        const CVariant variant_VarBinary = CVariant::VarBinary( value_binary, strlen(value_binary) );
        BOOST_CHECK( !variant_VarBinary.IsNull() );

        const CVariant variant_Binary = CVariant::Binary( strlen(value_binary), value_binary, strlen(value_binary) );
        BOOST_CHECK( !variant_Binary.IsNull() );

        const CVariant variant_SmallDateTime = CVariant::SmallDateTime( const_cast<CTime*>(&value_CTime) );
        BOOST_CHECK( !variant_SmallDateTime.IsNull() );

        const CVariant variant_DateTime = CVariant::DateTime( const_cast<CTime*>(&value_CTime) );
        BOOST_CHECK( !variant_DateTime.IsNull() );

//        CVariant variant_Numeric = CVariant::Numeric  (unsigned int precision, unsigned int scale, const char* p);
    }

    // Call Get method for different types
    {
        const Uint1 value_Uint1_tmp = CVariant::TinyInt( const_cast<Uint1*>(&value_Uint1) ).GetByte();
        BOOST_CHECK_EQUAL( value_Uint1, value_Uint1_tmp );

        const Int2 value_Int2_tmp = CVariant::SmallInt( const_cast<Int2*>(&value_Int2) ).GetInt2();
        BOOST_CHECK_EQUAL( value_Int2, value_Int2_tmp );

        const Int4 value_Int4_tmp = CVariant::Int( const_cast<Int4*>(&value_Int4) ).GetInt4();
        BOOST_CHECK_EQUAL( value_Int4, value_Int4_tmp );

        const Int8 value_Int8_tmp = CVariant::BigInt( const_cast<Int8*>(&value_Int8) ).GetInt8();
        BOOST_CHECK_EQUAL( value_Int8, value_Int8_tmp );

        const float value_float_tmp = CVariant::Float( const_cast<float*>(&value_float) ).GetFloat();
        BOOST_CHECK_EQUAL( value_float, value_float_tmp );

        const double value_double_tmp = CVariant::Double( const_cast<double*>(&value_double) ).GetDouble();
        BOOST_CHECK_EQUAL( value_double, value_double_tmp );

        const bool value_bool_tmp = CVariant::Bit( const_cast<bool*>(&value_bool) ).GetBit();
        BOOST_CHECK_EQUAL( value_bool, value_bool_tmp );

        const CTime value_CTime_tmp = CVariant::DateTime( const_cast<CTime*>(&value_CTime) ).GetCTime();
        BOOST_CHECK( value_CTime == value_CTime_tmp );

        // GetNumeric() ????
    }

    // Call operator= for different types
    //!!! It *fails* !!!
    if (false) {
        CVariant value_variant(0);
        value_variant.SetNull();

        value_variant = CVariant(0);
        BOOST_CHECK( CVariant(0) == value_variant );

        value_variant = value_Int8;
        BOOST_CHECK( CVariant( value_Int8 ) == value_variant );

        value_variant = value_Int4;
        BOOST_CHECK( CVariant( value_Int4 ) == value_variant );

        value_variant = value_Int2;
        BOOST_CHECK( CVariant( value_Int2 ) == value_variant );

        value_variant = value_Uint1;
        BOOST_CHECK( CVariant( value_Uint1 ) == value_variant );

        value_variant = value_float;
        BOOST_CHECK( CVariant( value_float ) == value_variant );

        value_variant = value_double;
        BOOST_CHECK( CVariant( value_double ) == value_variant );

        value_variant = value_string;
        BOOST_CHECK( CVariant( value_string ) == value_variant );

        value_variant = value_char;
        BOOST_CHECK( CVariant( value_char ) == value_variant );

        value_variant = value_bool;
        BOOST_CHECK( CVariant( value_bool ) == value_variant );

        value_variant = value_CTime;
        BOOST_CHECK( CVariant( value_CTime ) == value_variant );

    }

    // Check assigning of values of same type ...
    {
        CVariant variant_Int8( value_Int8 );
        BOOST_CHECK( !variant_Int8.IsNull() );
        variant_Int8 = CVariant( value_Int8 );
        variant_Int8 = value_Int8;

        CVariant variant_Int4( value_Int4 );
        BOOST_CHECK( !variant_Int4.IsNull() );
        variant_Int4 = CVariant( value_Int4 );
        variant_Int4 = value_Int4;

        CVariant variant_Int2( value_Int2 );
        BOOST_CHECK( !variant_Int2.IsNull() );
        variant_Int2 = CVariant( value_Int2 );
        variant_Int2 = value_Int2;

        CVariant variant_Uint1( value_Uint1 );
        BOOST_CHECK( !variant_Uint1.IsNull() );
        variant_Uint1 = CVariant( value_Uint1 );
        variant_Uint1 = value_Uint1;

        CVariant variant_float( value_float );
        BOOST_CHECK( !variant_float.IsNull() );
        variant_float = CVariant( value_float );
        variant_float = value_float;

        CVariant variant_double( value_double );
        BOOST_CHECK( !variant_double.IsNull() );
        variant_double = CVariant( value_double );
        variant_double = value_double;

        CVariant variant_bool( value_bool );
        BOOST_CHECK( !variant_bool.IsNull() );
        variant_bool = CVariant( value_bool );
        variant_bool = value_bool;

        CVariant variant_string( value_string );
        BOOST_CHECK( !variant_string.IsNull() );
        variant_string = CVariant( value_string );
        variant_string = value_string;

        CVariant variant_char( value_char );
        BOOST_CHECK( !variant_char.IsNull() );
        variant_char = CVariant( value_char );
        variant_char = value_char;

        CVariant variant_CTimeShort( value_CTime, eShort );
        BOOST_CHECK( !variant_CTimeShort.IsNull() );
        variant_CTimeShort = CVariant( value_CTime, eShort );

        CVariant variant_CTimeLong( value_CTime, eLong );
        BOOST_CHECK( !variant_CTimeLong.IsNull() );
        variant_CTimeLong = CVariant( value_CTime, eLong );

//        explicit CVariant(CDB_Object* obj);

    }

    // Call operator= for different Variant types
    {
        // Assign to CVariant(NULL)
        if (false) {
            {
                CVariant value_variant(0);
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_Uint1 )
        if (false) {
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_Int2 )
        if (false) {
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_Int4 )
        if (false) {
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_Int8 )
        if (false) {
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_float )
        if (false) {
            {
                CVariant value_variant( value_float );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_double )
        if (false) {
            {
                CVariant value_variant( value_double );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_bool )
        if (false) {
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_CTime )
        if (false) {
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_CTime );
            }
        }
    }

    // Test Null cases ...
    {
        CVariant value_variant(0);

        BOOST_CHECK( !value_variant.IsNull() );

        value_variant.SetNull();
        BOOST_CHECK( value_variant.IsNull() );

        value_variant.SetNull();
        BOOST_CHECK( value_variant.IsNull() );

        value_variant.SetNull();
        BOOST_CHECK( value_variant.IsNull() );
    }

    // Check operator==
    {
        // Check values of same type ...
        BOOST_CHECK( CVariant( true ) == CVariant( true ) );
        BOOST_CHECK( CVariant( false ) == CVariant( false ) );
        BOOST_CHECK( CVariant( Uint1(1) ) == CVariant( Uint1(1) ) );
        BOOST_CHECK( CVariant( Int2(1) ) == CVariant( Int2(1) ) );
        BOOST_CHECK( CVariant( Int4(1) ) == CVariant( Int4(1) ) );
        BOOST_CHECK( CVariant( Int8(1) ) == CVariant( Int8(1) ) );
        BOOST_CHECK( CVariant( float(1) ) == CVariant( float(1) ) );
        BOOST_CHECK( CVariant( double(1) ) == CVariant( double(1) ) );
        BOOST_CHECK( CVariant( string("abcd") ) == CVariant( string("abcd") ) );
        BOOST_CHECK( CVariant( "abcd" ) == CVariant( "abcd" ) );
        BOOST_CHECK( CVariant( value_CTime, eShort ) == CVariant( value_CTime, eShort ) );
        BOOST_CHECK( CVariant( value_CTime, eLong ) == CVariant( value_CTime, eLong ) );
    }

    // Check operator<
    {
        // Check values of same type ...
        {
            //  Type not supported
            // BOOST_CHECK( CVariant( false ) < CVariant( true ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Uint1(1) ) );
            BOOST_CHECK( CVariant( Int2(-1) ) < CVariant( Int2(1) ) );
            BOOST_CHECK( CVariant( Int4(-1) ) < CVariant( Int4(1) ) );
            BOOST_CHECK( CVariant( Int8(-1) ) < CVariant( Int8(1) ) );
            BOOST_CHECK( CVariant( float(-1) ) < CVariant( float(1) ) );
            BOOST_CHECK( CVariant( double(-1) ) < CVariant( double(1) ) );
            BOOST_CHECK( CVariant( string("abcd") ) < CVariant( string("bcde") ) );
            BOOST_CHECK( CVariant( "abcd" ) < CVariant( "bcde" ) );
            CTime new_time = value_CTime;
            new_time += 1;
            BOOST_CHECK( CVariant( value_CTime, eShort ) < CVariant( new_time, eShort ) );
            BOOST_CHECK( CVariant( value_CTime, eLong ) < CVariant( new_time, eLong ) );
        }

        // Check comparasion wit Uint1(0) ...
        //!!! It *fails* !!!
        if (false) {
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Uint1(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int2(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int4(1) ) );
            // !!! Does not work ...
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int8(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( float(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( double(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( string("bcde") ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( "bcde" ) );
        }
    }

    // Check GetType
    {
        const CVariant variant_Int8( value_Int8 );
        BOOST_CHECK_EQUAL( eDB_BigInt, variant_Int8.GetType() );

        const CVariant variant_Int4( value_Int4 );
        BOOST_CHECK_EQUAL( eDB_Int, variant_Int4.GetType() );

        const CVariant variant_Int2( value_Int2 );
        BOOST_CHECK_EQUAL( eDB_SmallInt, variant_Int2.GetType() );

        const CVariant variant_Uint1( value_Uint1 );
        BOOST_CHECK_EQUAL( eDB_TinyInt, variant_Uint1.GetType() );

        const CVariant variant_float( value_float );
        BOOST_CHECK_EQUAL( eDB_Float, variant_float.GetType() );

        const CVariant variant_double( value_double );
        BOOST_CHECK_EQUAL( eDB_Double, variant_double.GetType() );

        const CVariant variant_bool( value_bool );
        BOOST_CHECK_EQUAL( eDB_Bit, variant_bool.GetType() );

        const CVariant variant_string( value_string );
        BOOST_CHECK_EQUAL( eDB_VarChar, variant_string.GetType() );

        const CVariant variant_char( value_char );
        BOOST_CHECK_EQUAL( eDB_VarChar, variant_char.GetType() );
    }

    // Test BLOB ...
    {
    }

    // Check AsNotNullString
    {
    }

    // Assignment of bound values ...
    {
    }
}

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
void
CDBAPIUnitTest::Test_Bind(void)
{
}

void
CDBAPIUnitTest::Test_Execute(void)
{
}

void
CDBAPIUnitTest::Test_Exception(void)
{
}

void
CDBAPIUnitTest::Create_Destroy(void)
{
}

void
CDBAPIUnitTest::Repeated_Usage(void)
{
}

void
CDBAPIUnitTest::Single_Value_Writing(void)
{
}

void
CDBAPIUnitTest::Single_Value_Reading(void)
{
}

void
CDBAPIUnitTest::Bulk_Reading(void)
{
}

void
CDBAPIUnitTest::Multiple_Resultset(void)
{
}

void
CDBAPIUnitTest::Query_Cancelation(void)
{
}

void
CDBAPIUnitTest::Error_Conditions(void)
{
}

void
CDBAPIUnitTest::Transactional_Behavior(void)
{
}


/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
CDBAPITestSuite::CDBAPITestSuite(const CTestArguments& args)
    : test_suite("DBAPI Test Suite")
{
    // add member function test cases to a test suite
    boost::shared_ptr<CDBAPIUnitTest> DBAPIInstance(new CDBAPIUnitTest(args));
    boost::unit_test::test_case* tc = NULL;
    boost::unit_test::test_case* tc_init = 
        BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::TestInit, DBAPIInstance);

    add(tc_init);
    
    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_GetColumnNo, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);
    
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Variant, DBAPIInstance));

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::TestGetRowCount, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    boost::unit_test::test_case* tc_parameters = 
        BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_StatementParameters, DBAPIInstance);
    tc_parameters->depends_on(tc_init);
    add(tc_parameters);

    boost::unit_test::test_case* except_safety_tc =
        BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Exception_Safety, DBAPIInstance);
    except_safety_tc->depends_on(tc_init);
    add(except_safety_tc);

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UserErrorHandler, DBAPIInstance);
    tc->depends_on(tc_init);
    tc->depends_on(except_safety_tc);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmt, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Procedure, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    if ( args.GetServerName() != "MOZART" ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Variant2, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }
    
    // ctlib/dblib do not work at the moment.
    // ftds works with MS SQL Server only at the moment. 
    if ( args.GetDriverName() == "ftds" && args.GetServerType() ==
        CTestArguments::eMsSql ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if ( args.GetServerType() == CTestArguments::eMsSql && 
         args.GetDriverName() != "msdblib") {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmtXML, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    // Does not work with all databases and drivers currently ...
//     tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB, DBAPIInstance);
//     tc->depends_on(tc_init);
//     add(tc);

//     tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor, DBAPIInstance);
//     tc->depends_on(tc_parameters);
//     add(tc);
}

CDBAPITestSuite::~CDBAPITestSuite(void)
{
}


/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
CTestArguments::CTestArguments(int argc, char * argv[])
{
    CNcbiArguments arguments(argc, argv);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    // Specify USAGE context
    arg_desc->SetUsageContext(arguments.GetProgramBasename(),
                              "dbapi_unit_test");

    // Describe the expected command-line arguments
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "msdblib", "odbc", "gateway"
#else
#define DEF_SERVER    "STRAUSS"
#define DEF_DRIVER    "ctlib"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "gateway"
#endif

    arg_desc->AddDefaultKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString, DEF_SERVER);

    arg_desc->AddDefaultKey("d", "driver",
                            "Name of the DBAPI driver to use",
                            CArgDescriptions::eString,
                            DEF_DRIVER);
    arg_desc->SetConstraint("d", &(*new CArgAllow_Strings, ALL_DRIVERS));

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString, "anyone");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString, "allowed");
    arg_desc->AddDefaultKey("D", "database",
                            "Name of the database to connect",
                            CArgDescriptions::eString,
                            "DBAPI_Sample");

    auto_ptr<CArgs> args_ptr(arg_desc->CreateArgs(arguments));
    const CArgs& args = *args_ptr;


    // Get command-line arguments ...
    m_DriverName    = args["d"].AsString();
    m_ServerName    = args["S"].AsString();
    m_UserName      = args["U"].AsString();
    m_UserPassword  = args["P"].AsString();
    m_DatabaseName  = args["D"].AsString();

    SetDatabaseParameters();
}

CTestArguments::EServerType
CTestArguments::GetServerType(void) const
{
    if ( GetServerName() == "STRAUSS"  ||  GetServerName() == "MOZART" ) {
        return eSybase;
    } else if ( GetServerName().substr(0, 6) == "MS_DEV" ) {
        return eMsSql;
    }

    return eUnknown;
}

void
CTestArguments::SetDatabaseParameters(void)
{
    if ( GetDriverName() == "dblib" && GetServerType() == eSybase ) {
        // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
        // BcpIn to it using protocol version other than "100".
        m_DatabaseParameters["version"] = "100";
    } else if ( GetDriverName() == "ftds" && GetServerType() == eSybase ) {
        // ftds work with Sybase databases using protocol v42 only ...
        m_DatabaseParameters["version"] = "42";
    }

    if ( GetDriverName() == "ftds" || GetDriverName() == "dblib" ) {
        m_DatabaseParameters["client_charset"] = "UTF-8";
    }
}


END_NCBI_SCOPE


/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
test_suite*
init_unit_test_suite( int argc, char * argv[] )
{
    // Configure UTF ...
    // boost::unit_test_framework::unit_test_log::instance().set_log_format( "XML" );
    // boost::unit_test_framework::unit_test_result::set_report_format( "XML" );

    std::auto_ptr<test_suite> test(BOOST_TEST_SUITE( "DBAPI Unit Test." ));

    test->add(new ncbi::CDBAPITestSuite(ncbi::CTestArguments(argc, argv)));

    return test.release();
}


/* ===========================================================================
 *
 * $Log$
 * Revision 1.40  2005/09/13 14:48:30  ssikorsk
 * Added a Test_LOB implementation
 *
 * Revision 1.39  2005/09/07 11:09:39  ssikorsk
 * Added an implementation of the Test_GetColumnNo method
 *
 * Revision 1.38  2005/09/01 15:11:59  ssikorsk
 * Restructured #bulk_insert_table
 *
 * Revision 1.37  2005/08/29 16:07:23  ssikorsk
 * Adapted Bulk_Writing for Sybase.
 *
 * Revision 1.36  2005/08/22 17:01:30  ssikorsk
 * Disabled Test_SelectStmtXML for the msdblib driver.
 *
 * Revision 1.35  2005/08/22 12:09:17  ssikorsk
 * Added test for the SQLVARBINARY data type to the BulKWriting test.
 *
 * Revision 1.34  2005/08/17 20:07:40  ucko
 * Fix previous revision to use Int8 rather than hardcoding long long,
 * which is not always equivalent (or even necessarily defined).
 *
 * Revision 1.33  2005/08/17 18:05:47  ssikorsk
 * Added initial tests for BulkInsert with INT and BIGINT datatypes
 *
 * Revision 1.32  2005/08/15 18:56:56  ssikorsk
 * Added Test_SelectStmtXML to the test-suite
 *
 * Revision 1.31  2005/08/12 16:01:02  ssikorsk
 * Set server type for the bulk-test to MS SQL only.
 *
 * Revision 1.30  2005/08/12 15:46:43  ssikorsk
 * Added an initial bulk test to the test suite.
 *
 * Revision 1.29  2005/08/11 18:23:35  ssikorsk
 * Explicitly set maximal value size for data types with variable data size.
 *
 * Revision 1.28  2005/08/10 16:56:50  ssikorsk
 * Added Test_Variant2 to the test-suite
 *
 * Revision 1.27  2005/08/09 16:37:58  ssikorsk
 * Disabled Test_Cursor
 *
 * Revision 1.26  2005/08/09 16:13:18  ssikorsk
 * Fixed GCC compilation problem
 *
 * Revision 1.25  2005/08/09 16:09:40  ssikorsk
 * Added the 'Test_Cursor' test to the test-suite
 *
 * Revision 1.24  2005/08/09 13:14:41  ssikorsk
 * Added a 'Test_Procedure' test to the test-suite
 *
 * Revision 1.23  2005/07/11 11:13:02  ssikorsk
 * Added a 'TestSelect' test to the test-suite
 *
 * Revision 1.22  2005/05/16 15:06:20  ssikorsk
 * Add PushCntxMsgHandler + invalid connection check.
 *
 * Revision 1.21  2005/05/16 14:49:09  ssikorsk
 * Reinit an errot handler because it can be affected during connection.
 *
 * Revision 1.20  2005/05/12 18:42:57  ssikorsk
 * Improved the "Test_UserErrorHandler" test-case
 *
 * Revision 1.19  2005/05/12 15:33:34  ssikorsk
 * initial version of Test_UserErrorHandler
 *
 * Revision 1.18  2005/05/05 20:29:02  ucko
 * Explicitly scope test_case under boost::unit_test::.
 *
 * Revision 1.17  2005/05/05 15:09:21  ssikorsk
 * Moved from CPPUNIT to Boost.Test
 *
 * Revision 1.16  2005/04/13 13:27:09  ssikorsk
 * Workaround for the ctlib driver in Test_StatementParameters
 *
 * Revision 1.15  2005/04/12 19:11:10  ssikorsk
 * Do not clean a parameter list after Execute (previous behavior restored). This can cause problems with the ctlib driver.
 *
 * Revision 1.14  2005/04/11 14:13:15  ssikorsk
 * Explicitly clean a parameter list after Execute (because of the ctlib driver)
 *
 * Revision 1.13  2005/04/08 15:50:00  ssikorsk
 * Included test/test_assert.h
 *
 * Revision 1.12  2005/04/07 20:29:12  ssikorsk
 * Added more dedicated statements to each test
 *
 * Revision 1.11  2005/04/07 19:16:03  ssikorsk
 *
 * Added two different test patterns:
 * 1) New/dedicated statement for each test;
 * 2) Reusable statement for all tests;
 *
 * Revision 1.10  2005/04/07 15:42:23  ssikorsk
 * Workaround for the CTLIB driver
 *
 * Revision 1.9  2005/04/07 14:55:24  ssikorsk
 * Fixed a GCC compilation error
 *
 * Revision 1.8  2005/04/07 14:07:16  ssikorsk
 * Added CheckGetRowCount2
 *
 * Revision 1.7  2005/03/08 17:59:48  ssikorsk
 * Fixed GCC warnings
 *
 * Revision 1.6  2005/02/16 21:46:40  ssikorsk
 * Improved CVariant test
 *
 * Revision 1.5  2005/02/16 20:01:20  ssikorsk
 * Added CVariant test
 *
 * Revision 1.4  2005/02/15 17:32:29  ssikorsk
 * Added  TDS "version" parameter with database connection
 *
 * Revision 1.3  2005/02/15 16:06:24  ssikorsk
 * Added driver and server parameters to the test-suite (handled via CNcbiApplication)
 *
 * Revision 1.2  2005/02/11 16:12:02  ssikorsk
 * Improved GetRowCount test
 *
 * Revision 1.1  2005/02/04 17:25:02  ssikorsk
 * Renamed dbapi-unit-test to dbapi_unit_test.
 * Added dbapi_unit_test to the test suite.
 *
 * Revision 1.1  2005/02/03 16:06:46  ssikorsk
 * Added: initial version of a cppunit test for the DBAPI
 *
 * ===========================================================================
 */
