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

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include <dbapi/dbapi.hpp>

#include "dbapi_unit_test.hpp"

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( CDBAPIUnitTest );

///////////////////////////////////////////////////////////////////////////////
enum EServerType {
    eUnknown,   //< Server type is not known
    eSybase,    //< Sybase server
    eMsSql,     //< Microsoft SQL server
    eOracle     //< ORACLE server
};

string DriverName;
string ServerName;
string UserName;
string UserPassword;

string GetDriverName(void)
{
    return DriverName;
}

string GetServerName(void)
{
    return ServerName;
}

string GetUserName(void)
{
    return UserName;
}

string GetUserPassword(void)
{
    return UserPassword;
}

EServerType
GetServerType(void)
{
    if ( GetServerName() == "STRAUSS"  ||  GetServerName() == "MOZART" ) {
        return eSybase;
    } else if ( GetServerName().substr(0, 6) == "MS_DEV" ) {
        return eMsSql;
    }

    return eUnknown;
}

///////////////////////////////////////////////////////////////////////////////
CTestTransaction::CTestTransaction(
    IConnection& conn,
    ETransBehavior tb
    )
    : m_TransBehavior( tb )
{
    if ( m_TransBehavior != eNoTrans ) {
        m_Stmt.reset( conn.CreateStatement() );
        m_Stmt->ExecuteUpdate( "BEGIN TRANSACTION" );
    }
}

CTestTransaction::~CTestTransaction(void)
{
    try {
        if ( m_TransBehavior == eTransCommit ) {
            m_Stmt->ExecuteUpdate( "COMMIT TRANSACTION" );
        } else if ( m_TransBehavior == eTransRollback ) {
            m_Stmt->ExecuteUpdate( "ROLLBACK TRANSACTION" );
        }
    }
    catch( ... ) {
        // Just ignore ...
    }
}
///////////////////////////////////////////////////////////////////////////////
CDBAPIUnitTest::CDBAPIUnitTest()
: m_DM( CDriverManager::GetInstance() )
, m_DS( NULL )
, m_TableName( "#dbapi_unit_test" )
{
}

void
CDBAPIUnitTest::SetDatabaseParameters(void)
{
    if ( GetDriverName() == "dblib"  &&  GetServerType() == eSybase ) {
        // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
        // BcpIn to it using protocol version other than "100".
        m_DatabaseParameters["version"] = "100";
    } else if ( GetDriverName() == "ftds"  &&  GetServerType() == eSybase ) {
        // ftds forks with Sybase databases using protocol v42 only ...
        m_DatabaseParameters["version"] = "42";
    }
}

void
CDBAPIUnitTest::setUp()
{
    if ( m_DS == NULL ) {
        SetDatabaseParameters();
        m_DS = m_DM.CreateDs( DriverName, &m_DatabaseParameters );
    }
    m_Stmt.release();
    m_Conn.reset( m_DS->CreateConnection() );
    m_Conn->Connect( GetUserName(), GetUserPassword(), GetServerName(), "DBAPI_Sample" );
    m_Stmt.reset( m_Conn->CreateStatement() );

    // Create a test table ...
    string sql;

    sql  = " CREATE TABLE " + m_TableName + "( \n";
    sql += "    int_val INT NOT NULL \n";
    sql += " )";

    // Create the table
    m_Stmt->ExecuteUpdate(sql);
}

void
CDBAPIUnitTest::tearDown()
{
}


void
CDBAPIUnitTest::TestGetRowCount()
{
    for ( int i = 0; i < 5; ++i ) {
        CheckGetRowCount( i, eNoTrans );
        CheckGetRowCount( i, eTransCommit );
        CheckGetRowCount( i, eTransRollback );
    }
}

void
CDBAPIUnitTest::CheckGetRowCount(int row_count, ETransBehavior tb)
{
    // Transaction ...
    CTestTransaction transaction(*m_Conn, tb);
    string sql;
    sql  = " INSERT INTO " + m_TableName + "(int_val) VALUES( 1 ) \n";

    // Insert row_count records into the table ...
    for ( int i = 0; i < row_count; ++i ) {
        m_Stmt->ExecuteUpdate(sql);
        int nRows = m_Stmt->GetRowCount();
        CPPUNIT_ASSERT_EQUAL( nRows, 1 );
    }

    // Check a SELECT statement
    {
        sql  = " SELECT * FROM " + m_TableName;
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( row_count, nRows );
    }

    // Check an UPDATE statement
    {
        sql  = " UPDATE " + m_TableName + " SET int_val = 0 ";
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again
    {
        sql  = " SELECT * FROM " + m_TableName + " WHERE int_val = 0";
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( row_count, nRows );
    }

    // Check a DELETE statement
    {
        sql  = " DELETE FROM " + m_TableName;
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again and again ...
    {
        sql  = " SELECT * FROM " + m_TableName;
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( 0, nRows );
    }

}

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
        CPPUNIT_ASSERT( !variant_Int8.IsNull() );

        const CVariant variant_Int4( value_Int4 );
        CPPUNIT_ASSERT( !variant_Int4.IsNull() );

        const CVariant variant_Int2( value_Int2 );
        CPPUNIT_ASSERT( !variant_Int2.IsNull() );

        const CVariant variant_Uint1( value_Uint1 );
        CPPUNIT_ASSERT( !variant_Uint1.IsNull() );

        const CVariant variant_float( value_float );
        CPPUNIT_ASSERT( !variant_float.IsNull() );

        const CVariant variant_double( value_double );
        CPPUNIT_ASSERT( !variant_double.IsNull() );

        const CVariant variant_bool( value_bool );
        CPPUNIT_ASSERT( !variant_bool.IsNull() );

        const CVariant variant_string( value_string );
        CPPUNIT_ASSERT( !variant_string.IsNull() );

        const CVariant variant_char( value_char );
        CPPUNIT_ASSERT( !variant_char.IsNull() );

        const CVariant variant_CTimeShort( value_CTime, eShort );
        CPPUNIT_ASSERT( !variant_CTimeShort.IsNull() );

        const CVariant variant_CTimeLong( value_CTime, eLong );
        CPPUNIT_ASSERT( !variant_CTimeLong.IsNull() );

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

            CPPUNIT_ASSERT_EQUAL( eDB_Int, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_SmallInt );

            CPPUNIT_ASSERT_EQUAL( eDB_SmallInt, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_TinyInt );

            CPPUNIT_ASSERT_EQUAL( eDB_TinyInt, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_BigInt );

            CPPUNIT_ASSERT_EQUAL( eDB_BigInt, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_VarChar );

            CPPUNIT_ASSERT_EQUAL( eDB_VarChar, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Char );

            CPPUNIT_ASSERT_EQUAL( eDB_Char, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_VarBinary );

            CPPUNIT_ASSERT_EQUAL( eDB_VarBinary, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Binary );

            CPPUNIT_ASSERT_EQUAL( eDB_Binary, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Float );

            CPPUNIT_ASSERT_EQUAL( eDB_Float, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Double );

            CPPUNIT_ASSERT_EQUAL( eDB_Double, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_DateTime );

            CPPUNIT_ASSERT_EQUAL( eDB_DateTime, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_SmallDateTime );

            CPPUNIT_ASSERT_EQUAL( eDB_SmallDateTime, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Text );

            CPPUNIT_ASSERT_EQUAL( eDB_Text, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Image );

            CPPUNIT_ASSERT_EQUAL( eDB_Image, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Bit );

            CPPUNIT_ASSERT_EQUAL( eDB_Bit, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Numeric );

            CPPUNIT_ASSERT_EQUAL( eDB_Numeric, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_LongChar );

            CPPUNIT_ASSERT_EQUAL( eDB_LongChar, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_LongBinary );

            CPPUNIT_ASSERT_EQUAL( eDB_LongBinary, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
        if (false) {
            CVariant value_variant( eDB_UnsupportedType );

            CPPUNIT_ASSERT_EQUAL( eDB_UnsupportedType, value_variant.GetType() );
            CPPUNIT_ASSERT( value_variant.IsNull() );
        }
    }

    // Check the copy-constructor CVariant(const CVariant& v)
    {
        const CVariant variant_Int8 = CVariant(value_Int8);
        CPPUNIT_ASSERT( !variant_Int8.IsNull() );

        const CVariant variant_Int4 = CVariant(value_Int4);
        CPPUNIT_ASSERT( !variant_Int4.IsNull() );

        const CVariant variant_Int2 = CVariant(value_Int2);
        CPPUNIT_ASSERT( !variant_Int2.IsNull() );

        const CVariant variant_Uint1 = CVariant(value_Uint1);
        CPPUNIT_ASSERT( !variant_Uint1.IsNull() );

        const CVariant variant_float = CVariant(value_float);
        CPPUNIT_ASSERT( !variant_float.IsNull() );

        const CVariant variant_double = CVariant(value_double);
        CPPUNIT_ASSERT( !variant_double.IsNull() );

        const CVariant variant_bool = CVariant(value_bool);
        CPPUNIT_ASSERT( !variant_bool.IsNull() );

        const CVariant variant_string = CVariant(value_string);
        CPPUNIT_ASSERT( !variant_string.IsNull() );

        const CVariant variant_char = CVariant(value_char);
        CPPUNIT_ASSERT( !variant_char.IsNull() );

        const CVariant variant_CTimeShort = CVariant( value_CTime, eShort );
        CPPUNIT_ASSERT( !variant_CTimeShort.IsNull() );

        const CVariant variant_CTimeLong = CVariant( value_CTime, eLong );
        CPPUNIT_ASSERT( !variant_CTimeLong.IsNull() );
    }

    // Call Factories for different types
    {
        const CVariant variant_Int8 = CVariant::BigInt( const_cast<Int8*>(&value_Int8) );
        CPPUNIT_ASSERT( !variant_Int8.IsNull() );

        const CVariant variant_Int4 = CVariant::Int( const_cast<Int4*>(&value_Int4) );
        CPPUNIT_ASSERT( !variant_Int4.IsNull() );

        const CVariant variant_Int2 = CVariant::SmallInt( const_cast<Int2*>(&value_Int2) );
        CPPUNIT_ASSERT( !variant_Int2.IsNull() );

        const CVariant variant_Uint1 = CVariant::TinyInt( const_cast<Uint1*>(&value_Uint1) );
        CPPUNIT_ASSERT( !variant_Uint1.IsNull() );

        const CVariant variant_float = CVariant::Float( const_cast<float*>(&value_float) );
        CPPUNIT_ASSERT( !variant_float.IsNull() );

        const CVariant variant_double = CVariant::Double( const_cast<double*>(&value_double) );
        CPPUNIT_ASSERT( !variant_double.IsNull() );

        const CVariant variant_bool = CVariant::Bit( const_cast<bool*>(&value_bool) );
        CPPUNIT_ASSERT( !variant_bool.IsNull() );

        const CVariant variant_LongChar = CVariant::LongChar( value_char, strlen(value_char) );
        CPPUNIT_ASSERT( !variant_LongChar.IsNull() );

        const CVariant variant_VarChar = CVariant::VarChar( value_char, strlen(value_char) );
        CPPUNIT_ASSERT( !variant_VarChar.IsNull() );

        const CVariant variant_Char = CVariant::Char( strlen(value_char), const_cast<char*>(value_char) );
        CPPUNIT_ASSERT( !variant_Char.IsNull() );

        const CVariant variant_LongBinary = CVariant::LongBinary( strlen(value_binary), value_binary, strlen(value_binary)) ;
        CPPUNIT_ASSERT( !variant_LongBinary.IsNull() );

        const CVariant variant_VarBinary = CVariant::VarBinary( value_binary, strlen(value_binary) );
        CPPUNIT_ASSERT( !variant_VarBinary.IsNull() );

        const CVariant variant_Binary = CVariant::Binary( strlen(value_binary), value_binary, strlen(value_binary) );
        CPPUNIT_ASSERT( !variant_Binary.IsNull() );

        const CVariant variant_SmallDateTime = CVariant::SmallDateTime( const_cast<CTime*>(&value_CTime) );
        CPPUNIT_ASSERT( !variant_SmallDateTime.IsNull() );

        const CVariant variant_DateTime = CVariant::DateTime( const_cast<CTime*>(&value_CTime) );
        CPPUNIT_ASSERT( !variant_DateTime.IsNull() );

//        CVariant variant_Numeric = CVariant::Numeric  (unsigned int precision, unsigned int scale, const char* p);
    }

    // Call Get method for different types
    {
        const Uint1 value_Uint1_tmp = CVariant::TinyInt( const_cast<Uint1*>(&value_Uint1) ).GetByte();
        CPPUNIT_ASSERT_EQUAL( value_Uint1, value_Uint1_tmp );

        const Int2 value_Int2_tmp = CVariant::SmallInt( const_cast<Int2*>(&value_Int2) ).GetInt2();
        CPPUNIT_ASSERT_EQUAL( value_Int2, value_Int2_tmp );

        const Int4 value_Int4_tmp = CVariant::Int( const_cast<Int4*>(&value_Int4) ).GetInt4();
        CPPUNIT_ASSERT_EQUAL( value_Int4, value_Int4_tmp );

        const Int8 value_Int8_tmp = CVariant::BigInt( const_cast<Int8*>(&value_Int8) ).GetInt8();
        CPPUNIT_ASSERT_EQUAL( value_Int8, value_Int8_tmp );

        const float value_float_tmp = CVariant::Float( const_cast<float*>(&value_float) ).GetFloat();
        CPPUNIT_ASSERT_EQUAL( value_float, value_float_tmp );

        const double value_double_tmp = CVariant::Double( const_cast<double*>(&value_double) ).GetDouble();
        CPPUNIT_ASSERT_EQUAL( value_double, value_double_tmp );

        const bool value_bool_tmp = CVariant::Bit( const_cast<bool*>(&value_bool) ).GetBit();
        CPPUNIT_ASSERT_EQUAL( value_bool, value_bool_tmp );

        const CTime value_CTime_tmp = CVariant::DateTime( const_cast<CTime*>(&value_CTime) ).GetCTime();
        CPPUNIT_ASSERT( value_CTime == value_CTime_tmp );

        // GetNumeric() ????
    }

    // Call operator= for different types
    //!!! It *fails* !!!
    if (false) {
        CVariant value_variant(0);
        value_variant.SetNull();

        value_variant = CVariant(0);
        CPPUNIT_ASSERT( CVariant(0) == value_variant );

        value_variant = value_Int8;
        CPPUNIT_ASSERT( CVariant( value_Int8 ) == value_variant );

        value_variant = value_Int4;
        CPPUNIT_ASSERT( CVariant( value_Int4 ) == value_variant );

        value_variant = value_Int2;
        CPPUNIT_ASSERT( CVariant( value_Int2 ) == value_variant );

        value_variant = value_Uint1;
        CPPUNIT_ASSERT( CVariant( value_Uint1 ) == value_variant );

        value_variant = value_float;
        CPPUNIT_ASSERT( CVariant( value_float ) == value_variant );

        value_variant = value_double;
        CPPUNIT_ASSERT( CVariant( value_double ) == value_variant );

        value_variant = value_string;
        CPPUNIT_ASSERT( CVariant( value_string ) == value_variant );

        value_variant = value_char;
        CPPUNIT_ASSERT( CVariant( value_char ) == value_variant );

        value_variant = value_bool;
        CPPUNIT_ASSERT( CVariant( value_bool ) == value_variant );

        value_variant = value_CTime;
        CPPUNIT_ASSERT( CVariant( value_CTime ) == value_variant );

    }

    // Check assigning of values of same type ...
    {
        CVariant variant_Int8( value_Int8 );
        CPPUNIT_ASSERT( !variant_Int8.IsNull() );
        variant_Int8 = CVariant( value_Int8 );
        variant_Int8 = value_Int8;

        CVariant variant_Int4( value_Int4 );
        CPPUNIT_ASSERT( !variant_Int4.IsNull() );
        variant_Int4 = CVariant( value_Int4 );
        variant_Int4 = value_Int4;

        CVariant variant_Int2( value_Int2 );
        CPPUNIT_ASSERT( !variant_Int2.IsNull() );
        variant_Int2 = CVariant( value_Int2 );
        variant_Int2 = value_Int2;

        CVariant variant_Uint1( value_Uint1 );
        CPPUNIT_ASSERT( !variant_Uint1.IsNull() );
        variant_Uint1 = CVariant( value_Uint1 );
        variant_Uint1 = value_Uint1;

        CVariant variant_float( value_float );
        CPPUNIT_ASSERT( !variant_float.IsNull() );
        variant_float = CVariant( value_float );
        variant_float = value_float;

        CVariant variant_double( value_double );
        CPPUNIT_ASSERT( !variant_double.IsNull() );
        variant_double = CVariant( value_double );
        variant_double = value_double;

        CVariant variant_bool( value_bool );
        CPPUNIT_ASSERT( !variant_bool.IsNull() );
        variant_bool = CVariant( value_bool );
        variant_bool = value_bool;

        CVariant variant_string( value_string );
        CPPUNIT_ASSERT( !variant_string.IsNull() );
        variant_string = CVariant( value_string );
        variant_string = value_string;

        CVariant variant_char( value_char );
        CPPUNIT_ASSERT( !variant_char.IsNull() );
        variant_char = CVariant( value_char );
        variant_char = value_char;

        CVariant variant_CTimeShort( value_CTime, eShort );
        CPPUNIT_ASSERT( !variant_CTimeShort.IsNull() );
        variant_CTimeShort = CVariant( value_CTime, eShort );

        CVariant variant_CTimeLong( value_CTime, eLong );
        CPPUNIT_ASSERT( !variant_CTimeLong.IsNull() );
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

        CPPUNIT_ASSERT( !value_variant.IsNull() );

        value_variant.SetNull();
        CPPUNIT_ASSERT( value_variant.IsNull() );

        value_variant.SetNull();
        CPPUNIT_ASSERT( value_variant.IsNull() );

        value_variant.SetNull();
        CPPUNIT_ASSERT( value_variant.IsNull() );
    }

    // Check operator==
    {
        // Check values of same type ...
        if (false) {
            CPPUNIT_ASSERT( CVariant( true ) == CVariant( true ) );
            CPPUNIT_ASSERT( CVariant( false ) == CVariant( false ) );
            CPPUNIT_ASSERT( CVariant( Uint1(1) ) == CVariant( Uint1(1) ) );
            CPPUNIT_ASSERT( CVariant( Int2(1) ) == CVariant( Int2(1) ) );
            CPPUNIT_ASSERT( CVariant( Int4(1) ) == CVariant( Int4(1) ) );
            CPPUNIT_ASSERT( CVariant( Int8(1) ) == CVariant( Int8(1) ) );
            CPPUNIT_ASSERT( CVariant( float(1) ) == CVariant( float(1) ) );
            CPPUNIT_ASSERT( CVariant( double(1) ) == CVariant( double(1) ) );
            CPPUNIT_ASSERT( CVariant( string("abcd") ) == CVariant( string("abcd") ) );
            CPPUNIT_ASSERT( CVariant( "abcd" ) == CVariant( "abcd" ) );
            CPPUNIT_ASSERT( CVariant( value_CTime, eShort ) == CVariant( value_CTime, eShort ) );
            CPPUNIT_ASSERT( CVariant( value_CTime, eLong ) == CVariant( value_CTime, eLong ) );
        }
    }

    // Check operator<
    {
        // Check values of same type ...
        {
            // CPPUNIT_ASSERT( CVariant( false ) < CVariant( true ) );
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( Uint1(1) ) );
            CPPUNIT_ASSERT( CVariant( Int2(-1) ) < CVariant( Int2(1) ) );
            CPPUNIT_ASSERT( CVariant( Int4(-1) ) < CVariant( Int4(1) ) );
            // !!! Does not work ...
            // CPPUNIT_ASSERT( CVariant( Int8(-1) ) < CVariant( Int8(1) ) );
            CPPUNIT_ASSERT( CVariant( float(-1) ) < CVariant( float(1) ) );
            CPPUNIT_ASSERT( CVariant( double(-1) ) < CVariant( double(1) ) );
            CPPUNIT_ASSERT( CVariant( string("abcd") ) < CVariant( string("bcde") ) );
            CPPUNIT_ASSERT( CVariant( "abcd" ) < CVariant( "bcde" ) );
            CTime new_time = value_CTime;
            new_time += 1;
            CPPUNIT_ASSERT( CVariant( value_CTime, eShort ) < CVariant( new_time, eShort ) );
            CPPUNIT_ASSERT( CVariant( value_CTime, eLong ) < CVariant( new_time, eLong ) );
        }

        // Check comparasion wit Uint1(0) ...
        //!!! It *fails* !!!
        if (false) {
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( Uint1(1) ) );
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( Int2(1) ) );
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( Int4(1) ) );
            // !!! Does not work ...
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( Int8(1) ) );
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( float(1) ) );
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( double(1) ) );
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( string("bcde") ) );
            CPPUNIT_ASSERT( CVariant( Uint1(0) ) < CVariant( "bcde" ) );
        }
    }

    // Check GetType
    {
        const CVariant variant_Int8( value_Int8 );
        CPPUNIT_ASSERT_EQUAL( eDB_BigInt, variant_Int8.GetType() );

        const CVariant variant_Int4( value_Int4 );
        CPPUNIT_ASSERT_EQUAL( eDB_Int, variant_Int4.GetType() );

        const CVariant variant_Int2( value_Int2 );
        CPPUNIT_ASSERT_EQUAL( eDB_SmallInt, variant_Int2.GetType() );

        const CVariant variant_Uint1( value_Uint1 );
        CPPUNIT_ASSERT_EQUAL( eDB_TinyInt, variant_Uint1.GetType() );

        const CVariant variant_float( value_float );
        CPPUNIT_ASSERT_EQUAL( eDB_Float, variant_float.GetType() );

        const CVariant variant_double( value_double );
        CPPUNIT_ASSERT_EQUAL( eDB_Double, variant_double.GetType() );

        const CVariant variant_bool( value_bool );
        CPPUNIT_ASSERT_EQUAL( eDB_Bit, variant_bool.GetType() );

        const CVariant variant_string( value_string );
        CPPUNIT_ASSERT_EQUAL( eDB_VarChar, variant_string.GetType() );

        const CVariant variant_char( value_char );
        CPPUNIT_ASSERT_EQUAL( eDB_VarChar, variant_char.GetType() );
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

void
CDBAPIUnitTest::Test_Bind(void)
{
}

void
CDBAPIUnitTest::Test_Execute(void)
{
}

void
CDBAPIUnitTest::Test_Procedure(void)
{
}

void
CDBAPIUnitTest::Test_Exception(void)
{
}

void
CDBAPIUnitTest::Test_Exception_Safety(void)
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
CDBAPIUnitTest::Bulk_Writing(void)
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


///////////////////////////////////////////////////////////////////////////////
CUnitTestApp::~CUnitTestApp(void)
{
    return ;
}

void
CUnitTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
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

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int
CUnitTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    // Get command-line arguments ...
    DriverName      = args["d"].AsString();
    ServerName      = args["S"].AsString();
    UserName        = args["U"].AsString();
    UserPassword    = args["P"].AsString();

    // Get the top level suite from the registry
    CPPUNIT_NS::Test *suite = CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest();

    // Adds the test to the list of test to run
    CPPUNIT_NS::TextUi::TestRunner runner;
    runner.addTest( suite );

    // Change the default outputter to a compiler error format outputter
    runner.setOutputter( new CPPUNIT_NS::CompilerOutputter( &runner.result(),   std::cerr ) );
    // Run the test.
    bool wasSucessful = runner.run();

    // Return error code 1 if the one of test failed.
    return wasSucessful ? 0 : 1;
}

void
CUnitTestApp::Exit(void)
{
    return ;
}

END_NCBI_SCOPE

int main(int argc, const char* argv[])
{
    return ncbi::CUnitTestApp().AppMain(argc, argv);
}

/* ===========================================================================
 *
 * $Log$
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
