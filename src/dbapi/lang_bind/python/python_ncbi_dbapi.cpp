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
* File Description:
* Status: *Initial*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "python_ncbi_dbapi.hpp"
#include "pythonpp/pythonpp_pdt.hpp"
#include "pythonpp/pythonpp_date.hpp"

//////////////////////////////////////////////////////////////////////////////
// Compatibility macros
//
// From Python 2.2 to 2.3, the way to export the module init function
// has changed. These macros keep the code compatible to both ways.
//
#if PY_VERSION_HEX >= 0x02030000
// #  define PYDBAPI_DECLARE_MODINIT_FUNC(name) PyMODINIT_FUNC name(void)
#  define PYDBAPI_MODINIT_FUNC(name)         PyMODINIT_FUNC name(void)
#else
// #  define PYDBAPI_DECLARE_MODINIT_FUNC(name) void name(void)
#  define PYDBAPI_MODINIT_FUNC(name)         DL_EXPORT(void) name(void)
#endif

BEGIN_NCBI_SCOPE

// ??? Temporary ...
static PyObject *ErrorObject = NULL;
static PyObject *ProgrammingErrorObject = NULL;

namespace python
{

//////////////////////////////////////////////////////////////////////////////
CBinary::CBinary(void)
{
    PrepareForPython(this);
}

CBinary::~CBinary(void)
{
}

//////////////////////////////////////////////////////////////////////////////
CNumber::CNumber(void)
{
    PrepareForPython(this);
}

CNumber::~CNumber(void)
{
}

//////////////////////////////////////////////////////////////////////////////
CRowID::CRowID(void)
{
    PrepareForPython(this);
}

CRowID::~CRowID(void)
{
}

//////////////////////////////////////////////////////////////////////////////
CConnection::CConnection(
    const string& driver_name,
    const string& db_type,
    const string& server_name,
    const string& db_name,
    const string& user_name,
    const string& user_pswd
    )
: m_driver_name(driver_name)
, m_db_type(db_type)
, m_server_name(server_name)
, m_db_name(db_name)
, m_user_name(user_name)
, m_user_pswd(user_pswd)
, m_ServerType(eUnknown)
, m_DM(CDriverManager::GetInstance())
, m_DS(NULL)
{
    // Setup a server type ...
    string db_type_uc = db_type;
    NStr::ToUpper(db_type_uc);

    if ( db_type_uc == "SYBASE" ) {
        m_ServerType = eSybase;
    } else if ( db_type_uc == "ORACLE" ) {
        m_ServerType = eOracle;
    } else if ( db_type_uc == "MYSQL" ) {
        m_ServerType = eMySql;
    } else if ( db_type_uc == "SQLITE" ) {
        m_ServerType = eSqlite;
    } else if ( db_type_uc == "MSSQL" ||
            db_type_uc == "MS_SQL" ||
            db_type_uc == "MS SQL") {
        m_ServerType = eMsSql;
    }

    CreateConnection();

    PrepareForPython(this);
}

CConnection::~CConnection(void)
{
}

void
CConnection::CreateConnection(void)
{
    try {
        m_DS = m_DM.CreateDs( GetDriverName() );
        m_Conn.reset( m_DS->CreateConnection() );
        m_Conn->Connect( m_user_name, m_user_pswd, m_server_name, m_db_name );
        m_Stmt.reset( m_Conn->CreateStatement() );

        GetStmt().ExecuteUpdate("BEGIN TRANSACTION");
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }
}

pythonpp::CObject
CConnection::close(const pythonpp::CTuple& args)
{
    m_Stmt.release();
    m_Conn.release();

    return pythonpp::CNone();
}

pythonpp::CObject
CConnection::cursor(const pythonpp::CTuple& args)
{
    return pythonpp::CObject(new CCursor(this), pythonpp::eTakeOwnership);
}

pythonpp::CObject
CConnection::commit(const pythonpp::CTuple& args)
{
    try {
        GetStmt().ExecuteUpdate("COMMIT TRANSACTION");
        GetStmt().ExecuteUpdate("BEGIN TRANSACTION");
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CConnection::rollback(const pythonpp::CTuple& args)
{
    try {
        GetStmt().ExecuteUpdate("ROLLBACK TRANSACTION");
        GetStmt().ExecuteUpdate("BEGIN TRANSACTION");
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }

    return pythonpp::CNone();
}

IConnection*
CConnection::GetDBConnection(void) const
{
    if ( m_Conn.get() == NULL ) {
        throw pythonpp::CError("Connection to a database is closed");
    }
    return m_Conn.get();
}

//////////////////////////////////////////////////////////////////////////////
CCursor::CCursor(CConnection* conn)
: m_PythonConnection(conn)
, m_ParentConnection(conn)
, m_RowsNum(0)
, m_StatementType(estNone)
, m_ArraySize(1)
{
    if ( conn == NULL ) {
        throw pythonpp::CError("Invalid CConnection object");
    }
    m_Stmt.reset(GetDBConnection()->CreateStatement());
    PrepareForPython(this);
}

CCursor::~CCursor(void)
{
}

IStatement&
CCursor::GetStmt(void)
{
    if ( m_Stmt.get() == NULL ) {
        throw pythonpp::CError("Cursor is closed");
    }
    return *m_Stmt;
}

ICallableStatement&
CCursor::GetCallableStmt(void)
{
    if ( m_CallableStmt.get() == NULL ) {
        throw pythonpp::CError("Cursor is closed");
    }
    return *m_CallableStmt;
}

pythonpp::CObject
CCursor::callproc(const pythonpp::CTuple& args)
{
    int num_of_arguments = 0;

    try {
        m_StmtStr = pythonpp::CString(args[0]);

    } catch (const pythonpp::CError&) {
        throw pythonpp::CError("Invalid parameters within 'CCursor::callproc' function");
    }

    try {
        m_CallableStmt.reset(GetDBConnection()->PrepareCall( m_StmtStr, num_of_arguments ));
        GetCallableStmt().Execute();
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::close(const pythonpp::CTuple& args)
{
    try {
        m_RS.release();
        m_Stmt.release();
        m_CallableStmt.release();
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::execute(const pythonpp::CTuple& args)
{
    size_t args_size = args.size();

    // Prepare to execute ...
    m_CallableStmt.release();
    GetStmt().ClearParamList();

    // Process function's arguments ...
    if ( args_size == 0 ) {
        throw pythonpp::CError("A SQL statement string is expected as a parameter");
    } else if ( args_size > 0 ) {
        pythonpp::CObject obj(args[0]);

        if ( pythonpp::CString::HasSameType(obj) ) {
            m_StmtStr = pythonpp::CString(args[0]);
        } else {
            throw pythonpp::CError("A SQL statement string is expected as a parameter");
        }
        if ( args_size > 1 ) {
            pythonpp::CObject obj(args[1]);

            if ( pythonpp::CDict::HasSameType(obj) ) {
                SetUpParameters(obj);
            } else  {
                // Curently, NCBI DBAPI supports pameter binding by name only ...
//            pythonpp::CSequence sequence;
//            if ( pythonpp::CList::HasSameType(obj) ) {
//            } else if ( pythonpp::CTuple::HasSameType(obj) ) {
//            } else if ( pythonpp::CSet::HasSameType(obj) ) {
//            }
                throw pythonpp::CError("NCBI DBAPI supports pameter binding by name only");
            }
        }
    }

    // Execute ...
    ExecuteCurrStatement();

    return pythonpp::CNone();
}

void
CCursor::SetUpParameters(const pythonpp::CDict& dict)
{
    // Iterate over a dict.
    int i = 0;
    PyObject* key;
    PyObject* value;
    while ( PyDict_Next(dict, &i, &key, &value) ) {
        // Refer to borrowed references in key and value.
        const pythonpp::CObject key_obj(key);
        const pythonpp::CObject value_obj(value);
        string param_name = pythonpp::CString(key_obj);

        if ( param_name.size() > 0) {
            if ( param_name[0] != '@') {
                param_name = "@" + param_name;
            }
        } else {
            throw pythonpp::CError("Invalid SQL parameter name");
        }
        try {
            GetStmt().SetParam( GetCVariant(value_obj), param_name );
        }
        catch(const CDB_Exception& e) {
            throw pythonpp::CError(e.Message());
        }
    }
}

CVariant
CCursor::GetCVariant(const pythonpp::CObject& obj) const
{
    if ( pythonpp::CNone::HasSameType(obj) ) {
        return CVariant(eDB_UnsupportedType);
    } else if ( pythonpp::CBool::HasSameType(obj) ) {
        return CVariant( pythonpp::CBool(obj) );
    } else if ( pythonpp::CInt::HasSameType(obj) ) {
        return CVariant( static_cast<int>(pythonpp::CInt(obj)) );
    } else if ( pythonpp::CLong::HasSameType(obj) ) {
        return CVariant( static_cast<Int8>(pythonpp::CLong(obj)) );
    } else if ( pythonpp::CFloat::HasSameType(obj) ) {
        return CVariant( pythonpp::CFloat(obj) );
    } else if ( pythonpp::CString::HasSameType(obj) ) {
        return CVariant( static_cast<string>(pythonpp::CString(obj)) );
    }

    return CVariant(eDB_UnsupportedType);
}

pythonpp::CObject
CCursor::executemany(const pythonpp::CTuple& args)
{
    size_t args_size = args.size();

    // Prepare to execute ...
    m_CallableStmt.release();
    GetStmt().ClearParamList();

    // Process function's arguments ...
    if ( args_size == 0 ) {
        throw pythonpp::CError("A SQL statement string is expected as a parameter");
    } else if ( args_size > 0 ) {
        pythonpp::CObject obj(args[0]);

        if ( pythonpp::CString::HasSameType(obj) ) {
            m_StmtStr = pythonpp::CString(args[0]);
        } else {
            throw pythonpp::CError("A SQL statement string is expected as a parameter");
        }

        if ( args_size > 1 ) {
            pythonpp::CObject obj(args[1]);

            if ( pythonpp::CList::HasSameType(obj) || pythonpp::CTuple::HasSameType(obj) ) {
                const pythonpp::CSequence params(obj);
                pythonpp::CList::const_iterator citer;
                pythonpp::CList::const_iterator cend = params.end();

                for ( citer = params.begin(); citer != cend; ++citer ) {
                    SetUpParameters(*citer);
                    ExecuteCurrStatement();
                }
            } else {
                throw pythonpp::CError("Sequence of parameters should be provided either as a list or as a tuple data type");
            }
        } else {
            throw pythonpp::CError("A sequence of parameters is expected with the 'executemany' function");
        }
    }

    return pythonpp::CNone();
}

void
CCursor::ExecuteCurrStatement(void)
{
    try {
        // Execute ...
        if ( RetrieveStatementType( GetStmtStr() ) == estSelect ) {
            m_RS.reset(GetStmt().ExecuteQuery ( GetStmtStr() ) );
        } else {
            m_RS.release();
            GetStmt().ExecuteUpdate ( GetStmtStr() );
        }
    }
    catch(const bad_cast&) {
        throw pythonpp::CError("std::bad_cast exception within 'CCursor::ExecuteCurrStatement'");
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }    
    catch(const exception&) {
        throw pythonpp::CError("std::exception exception within 'CCursor::ExecuteCurrStatement'");
    }

}

const pythonpp::CTuple
CCursor::MakeResultTuple(const IResultSet& rs) const
{
    // Set data. Make a sequence (tuple) ...
    int col_num = m_RS->GetColumnNo();
    col_num = (col_num > 0 ? col_num - 1 : col_num);
    pythonpp::CTuple tuple(col_num);

    for ( int i = 0; i < col_num; ++i) {
        const CVariant& value = m_RS->GetVariant (i + 1);

        if ( value.IsNull() ) {
            continue;
        }

        switch ( value.GetType() ) {
        case eDB_Int :
            tuple[i] = pythonpp::CInt( value.GetInt4() );
            break;
        case eDB_SmallInt :
            tuple[i] = pythonpp::CInt( value.GetInt2() );
            break;
        case eDB_TinyInt :
            tuple[i] = pythonpp::CInt( value.GetByte() );
            break;
        case eDB_BigInt :
            tuple[i] = pythonpp::CLong( value.GetInt8() );
            break;
        case eDB_Float :
            tuple[i] = pythonpp::CFloat( value.GetFloat() );
            break;
        case eDB_Double :
            tuple[i] = pythonpp::CFloat( value.GetDouble() );
            break;
        /*
        case eDB_Numeric :
            tuple[i] = pythonpp::CFloat( value.GetDouble() );
            break;
        */
        case eDB_Bit :
            // BIT --> BOOL ...
            tuple[i] = pythonpp::CBool( value.GetBit() );
            break;
        case eDB_DateTime :
        case eDB_SmallDateTime :
            {
                const CTime& cur_time = value.GetCTime();
                tuple[i] = pythonpp::CDateTime(
                    cur_time.Year(),
                    cur_time.Month(),
                    cur_time.Day(),
                    cur_time.Hour(),
                    cur_time.Minute(),
                    cur_time.Second(),
                    cur_time.NanoSecond() / 1000
                    );
            }
            break;
        case eDB_VarChar :
        case eDB_Char :
        case eDB_LongChar :
        case eDB_Text :
            tuple[i] = pythonpp::CString( value.GetString() );
            break;
        case eDB_Image :
        case eDB_LongBinary :
        case eDB_VarBinary :
        case eDB_Binary :
        case eDB_Numeric :
            tuple[i] = pythonpp::CString( value.GetString() );
            break;
        case eDB_UnsupportedType :
            break;
        }
    }

    return tuple;
}

pythonpp::CObject
CCursor::fetchone(const pythonpp::CTuple& args)
{
    if ( m_RS.get() == NULL ) {
        throw pythonpp::CError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    try {
        if ( m_RS->Next() ) {
            return MakeResultTuple(*m_RS);
        }
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::fetchmany(const pythonpp::CTuple& args)
{
    size_t array_size = m_ArraySize;

    if ( m_RS.get() == NULL ) {
        throw pythonpp::CError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    try {
        if ( args.size() > 0 ) {
            array_size = static_cast<unsigned long>(pythonpp::CLong(args[0]));
        }
    } catch (const pythonpp::CError&) {
        throw pythonpp::CError("Invalid parameters within 'CCursor::fetchmany' function");
    }

    pythonpp::CList py_list;
    for ( size_t i = 0; i < array_size && m_RS->Next(); ++i ) {
        py_list.Append(MakeResultTuple(*m_RS));
    }

    return py_list;
}

pythonpp::CObject
CCursor::fetchall(const pythonpp::CTuple& args)
{
    if ( m_RS.get() == NULL ) {
        throw pythonpp::CError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    pythonpp::CList py_list;
    try {
        while ( m_RS->Next() ) {
            py_list.Append(MakeResultTuple(*m_RS));
        }
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }

    return py_list;
}

pythonpp::CObject
CCursor::nextset(const pythonpp::CTuple& args)
{
    try {
        while ( GetStmt().HasMoreResults() ) {
            if ( GetStmt().HasRows() ) {
                m_RS.reset(GetStmt().GetResultSet() );
                return pythonpp::CBool(true);
            }
        }
    }
    catch(const CDB_Exception& e) {
        throw pythonpp::CError(e.Message());
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::setinputsizes(const pythonpp::CTuple& args)
{
    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::setoutputsize(const pythonpp::CTuple& args)
{
    return pythonpp::CNone();
}

CCursor::EStatementType
CCursor::RetrieveStatementType(const string& stmt)
{
    string::size_type pos;
    EStatementType stmtType = estNone;

    string stmt_uc = stmt;
    NStr::ToUpper(stmt_uc);
    pos = stmt_uc.find_first_not_of(" \t\n");
    if (pos != string::npos)
    {
        // "CREATE" should be before DML ...
        if (stmt_uc.substr(pos, sizeof("CREATE") - 1) == "CREATE")
        {
            stmtType = estCreate;
        } else if (stmt_uc.substr(pos, sizeof("SELECT") - 1) == "SELECT")
        {
            stmtType = estSelect;
        } else if (stmt_uc.substr(pos, sizeof("UPDATE") - 1) == "UPDATE")
        {
            stmtType = estUpdate;
        } else if (stmt_uc.substr(pos, sizeof("DELETE") - 1) == "DELETE")
        {
            stmtType = estDelete;
        } else if (stmt_uc.substr(pos, sizeof("INSERT") - 1) == "INSERT")
        {
            stmtType = estInsert;
        } else if (stmt_uc.substr(pos, sizeof("DROP") - 1) == "DROP")
        {
            stmtType = estDrop;
        } else if (stmt_uc.substr(pos, sizeof("ALTER") - 1) == "ALTER")
        {
            stmtType = estAlter;
        }
    }

    return stmtType;
}

//////////////////////////////////////////////////////////////////////////////
// Future development ...
class CModuleDBAPI : public pythonpp::CExtModule<CModuleDBAPI>
{
public:
    CModuleDBAPI(const char* name, const char* descr = 0)
    : pythonpp::CExtModule<CModuleDBAPI>(name, descr)
    {
    }

public:
    pythonpp::CObject connect(const pythonpp::CTuple& args)
    {
        // return pythonpp::CObject(new CConnection(), false);
        return pythonpp::CNone();
    }
};

//////////////////////////////////////////////////////////////////////////////
// connect(driver_name, db_type, db_name, user_name, user_pswd)
static
PyObject*
Connect(PyObject *self, PyObject *args)
{
    CConnection* conn = NULL;

    try {
        string driver_name;
        string db_type;
        string server_name;
        string db_name;
        string user_name;
        string user_pswd;

        try {
            const pythonpp::CTuple func_args(args);

            driver_name = pythonpp::CString(func_args[0]);
            db_type = pythonpp::CString(func_args[1]);
            server_name = pythonpp::CString(func_args[2]);
            db_name = pythonpp::CString(func_args[3]);
            user_name = pythonpp::CString(func_args[4]);
            user_pswd = pythonpp::CString(func_args[5]);
        } catch (const pythonpp::CError&) {
            throw pythonpp::CError("Invalid parameters within 'connect' function");
        }

        conn = new CConnection(
            driver_name,
            db_type,
            server_name,
            db_name,
            user_name,
            user_pswd
            );
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.Message());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Connect");
    }

    return conn;
}

// This function constructs an object holding a date value.
// Date(year,month,day)
static
PyObject*
Date(PyObject *self, PyObject *args)
{
    try {
        int year;
        int month;
        int day;

        try {
            const pythonpp::CTuple func_args(args);

            year = pythonpp::CInt(func_args[0]);
            month = pythonpp::CInt(func_args[1]);
            day = pythonpp::CInt(func_args[2]);
        } catch (const pythonpp::CError&) {
            throw pythonpp::CError("Invalid parameters within 'Date' function");
        }

        return IncRefCount(pythonpp::CDate(year, month, day));
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.Message());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Date");
    }

    return NULL;
}

// This function constructs an object holding a time value.
// Time(hour,minute,second)
static
PyObject*
Time(PyObject *self, PyObject *args)
{
    try {
        int hour;
        int minute;
        int second;

        try {
            const pythonpp::CTuple func_args(args);

            hour = pythonpp::CInt(func_args[0]);
            minute = pythonpp::CInt(func_args[1]);
            second = pythonpp::CInt(func_args[2]);
        } catch (const pythonpp::CError&) {
            throw pythonpp::CError("Invalid parameters within 'Time' function");
        }

        return IncRefCount(pythonpp::CTime(hour, minute, second, 0));
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.Message());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Time");
    }

    return NULL;
}

// This function constructs an object holding a time stamp
// value.
// Timestamp(year,month,day,hour,minute,second)
static
PyObject*
Timestamp(PyObject *self, PyObject *args)
{
    try {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;

        try {
            const pythonpp::CTuple func_args(args);

            year = pythonpp::CInt(func_args[0]);
            month = pythonpp::CInt(func_args[1]);
            day = pythonpp::CInt(func_args[2]);
            hour = pythonpp::CInt(func_args[3]);
            minute = pythonpp::CInt(func_args[4]);
            second = pythonpp::CInt(func_args[5]);
        } catch (const pythonpp::CError&) {
            throw pythonpp::CError("Invalid parameters within 'Timestamp' function");
        }

        return IncRefCount(pythonpp::CDateTime(year, month, day, hour, minute, second, 0));
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.Message());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Timestamp");
    }

    return NULL;
}

// This function constructs an object holding a date value
// from the given ticks value (number of seconds since the
// epoch; see the documentation of the standard Python time
// module for details).
// DateFromTicks(ticks)
static
PyObject*
DateFromTicks(PyObject *self, PyObject *args)
{
    try {
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.Message());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::DateFromTicks");
    }

    return NULL;
}

// This function constructs an object holding a time value
// from the given ticks value (number of seconds since the
// epoch; see the documentation of the standard Python time
// module for details).
// TimeFromTicks(ticks)
static
PyObject*
TimeFromTicks(PyObject *self, PyObject *args)
{
    try {
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.Message());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::TimeFromTicks");
    }

    return NULL;
}

// This function constructs an object holding a time stamp
// value from the given ticks value (number of seconds since
// the epoch; see the documentation of the standard Python
// time module for details).
// TimestampFromTicks(ticks)
static
PyObject*
TimestampFromTicks(PyObject *self, PyObject *args)
{
    try {
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.Message());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::TimestampFromTicks");
    }

    return NULL;
}

// This function constructs an object capable of holding a
// binary (long) string value.
// Binary(string)
static
PyObject*
Binary(PyObject *self, PyObject *args)
{
    CBinary* obj = NULL;

    try {
        string value;

        try {
            const pythonpp::CTuple func_args(args);

            value = pythonpp::CString(func_args[0]);
        } catch (const pythonpp::CError&) {
            throw pythonpp::CError("Invalid parameters within 'Binary' function");
        }

        obj = new CBinary(
            );
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.Message());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Binary");
    }

    return obj;
}

}

static struct PyMethodDef python_ncbi_dbapi_methods[] = {
    {"connect", (PyCFunction) python::Connect, METH_VARARGS,
        "connect(driver_name, server_name, database_name, userid, password) "
        "-- connect to the "
        "driver_name; server_name; database_name; userid; password;"
    },
    {"Date", (PyCFunction) python::Date, METH_VARARGS, "Date"},
    {"Time", (PyCFunction) python::Time, METH_VARARGS, "Time"},
    {"Timestamp", (PyCFunction) python::Timestamp, METH_VARARGS, "Timestamp"},
    {"DateFromTicks", (PyCFunction) python::DateFromTicks, METH_VARARGS, "DateFromTicks"},
    {"TimeFromTicks", (PyCFunction) python::TimeFromTicks, METH_VARARGS, "TimeFromTicks"},
    {"TimestampFromTicks", (PyCFunction) python::TimestampFromTicks, METH_VARARGS, "TimestampFromTicks"},
    {"Binary", (PyCFunction) python::Binary, METH_VARARGS, "Binary"},
	{ NULL, NULL }
};

// Module initialization
PYDBAPI_MODINIT_FUNC(initpython_ncbi_dbapi)
{
    PyObject *module;
    // char *rev = "$Revision$";
    PyObject *dict;
    PyObject *eo;

    // Initialize DateTime module ...
    PyDateTime_IMPORT;

    // Declare CModuleDBAPI
//    python::CModuleDBAPI::
//        def("close",    &python::CModuleDBAPI::connect, "connect");
//    python::CModuleDBAPI module_("python_ncbi_dbapi");

    // Declare CBinary
    python::CBinary::Declare("BINARY");

    // Declare CNumber
    python::CNumber::Declare("NUMBER");

    // Declare CRowID
    python::CRowID::Declare("ROWID");

    // Declare CConnection
    python::CConnection::
        Def("close",    &python::CConnection::close,    "close").
        Def("commit",   &python::CConnection::commit,   "commit").
        Def("rollback", &python::CConnection::rollback, "rollback").
        Def("cursor",   &python::CConnection::cursor,   "cursor");
    python::CConnection::Declare("Connection");

    // Declare CCursor
    python::CCursor::
        Def("callproc",     &python::CCursor::callproc,     "callproc").
        Def("close",        &python::CCursor::close,        "close").
        Def("execute",      &python::CCursor::execute,      "execute").
        Def("executemany",  &python::CCursor::executemany,  "executemany").
        Def("fetchone",     &python::CCursor::fetchone,     "fetchone").
        Def("fetchmany",    &python::CCursor::fetchmany,    "fetchmany").
        Def("fetchall",     &python::CCursor::fetchall,     "fetchall").
        Def("nextset",      &python::CCursor::nextset,      "nextset").
        Def("setinputsizes", &python::CCursor::setinputsizes, "setinputsizes").
        Def("setoutputsize", &python::CCursor::setoutputsize, "setoutputsize");
    python::CCursor::Declare("Cursor");


    module = Py_InitModule("python_ncbi_dbapi", python_ncbi_dbapi_methods);

    dict = PyModule_GetDict(module);

    eo = PyErr_NewException("python_ncbi_dbapi.Warning", PyExc_StandardError,
        NULL);
    PyDict_SetItemString(dict, "Warning", eo);
    Py_DECREF(eo);

    if (ErrorObject == NULL) {
        ErrorObject = PyErr_NewException("python_ncbi_dbapi.Error",
            PyExc_StandardError, NULL);
    }
    PyDict_SetItemString(dict, "Error", ErrorObject);

    eo = PyErr_NewException("python_ncbi_dbapi.InterfaceError", ErrorObject,
        NULL);
    PyDict_SetItemString(dict, "InterfaceError", eo);
    Py_DECREF(eo);

    eo = PyErr_NewException("python_ncbi_dbapi.DatabaseError", ErrorObject,
        NULL);
    PyDict_SetItemString(dict, "DatabaseError", eo);

    Py_DECREF(ErrorObject);
    ErrorObject = eo;

    eo = PyErr_NewException("python_ncbi_dbapi.InternalError", ErrorObject,
        NULL);
    PyDict_SetItemString(dict, "InternalError", eo);
    Py_DECREF(eo);

    eo = PyErr_NewException("python_ncbi_dbapi.OperationalError", ErrorObject,
        NULL);
    PyDict_SetItemString(dict, "OperationalError", eo);
    Py_DECREF(eo);

    if (ProgrammingErrorObject == NULL) {
        ProgrammingErrorObject =
            PyErr_NewException("python_ncbi_dbapi.ProgrammingError",
            ErrorObject, NULL);
    }
    PyDict_SetItemString(dict, "ProgrammingError", ProgrammingErrorObject);

    eo = PyErr_NewException("python_ncbi_dbapi.IntegrityError", ErrorObject,
        NULL);
    PyDict_SetItemString(dict, "IntegrityError", eo);
    Py_DECREF(eo);

    eo = PyErr_NewException("python_ncbi_dbapi.DataError", ErrorObject,
        NULL);
    PyDict_SetItemString(dict, "DataError", eo);
    Py_DECREF(eo);

    eo = PyErr_NewException("python_ncbi_dbapi.NotSupportedError", ErrorObject,
        NULL);
    PyDict_SetItemString(dict, "NotSupportedError", eo);
    Py_DECREF(eo);

    /*
	PyDict_SetItemString(dict, "__version__",
		PyString_FromStringAndSize(rev+11,strlen(rev+11)-2));

    */

}

END_NCBI_SCOPE

/* ===========================================================================
*
* $Log$
* Revision 1.2  2005/01/21 15:50:18  ssikorsk
* Fixed: build errors with GCC 2.95.
*
* Revision 1.1  2005/01/18 19:26:07  ssikorsk
* Initial version of a Python DBAPI module
*
*
* ===========================================================================
*/
