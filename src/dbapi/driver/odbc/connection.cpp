/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * File Description:  ODBC connection
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/types.hpp>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_ODBCSS_H
#include <odbcss.h>
#endif

#include "odbc_utils.hpp"

BEGIN_NCBI_SCOPE

static bool ODBC_xSendDataPrepare(CStatementBase& stmt,
                                  CDB_ITDescriptor& descr_in,
                                  SQLLEN size,
                                  bool is_text,
                                  bool logit,
                                  SQLPOINTER id,
                                  SQLLEN* ph);

static bool ODBC_xSendDataGetId(CStatementBase& stmt,
                                SQLPOINTER* id);

CODBC_Connection::CODBC_Connection(CODBCContext& cntx,
                                   SQLHDBC conn,
                                   bool reusable,
                                   const string& pool_name) :
    impl::CConnection(cntx, false, reusable, pool_name),
    m_Link(conn),
    m_Reporter(0, SQL_HANDLE_DBC, conn, &cntx.GetReporter())
{
    m_Reporter.SetHandlerStack(GetMsgHandlers());
}


bool CODBC_Connection::IsAlive()
{
    SQLINTEGER status;
    SQLRETURN r= SQLGetConnectAttr(m_Link, SQL_ATTR_CONNECTION_DEAD, &status, SQL_IS_INTEGER, 0);

    return ((r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) && (status == SQL_CD_FALSE));
}


CODBC_LangCmd* CODBC_Connection::xLangCmd(const string& lang_query,
                                          unsigned int  nof_params)
{
    string extra_msg = "SQL Command: \"" + lang_query + "\"";
    m_Reporter.SetExtraMsg( extra_msg );

    CODBC_LangCmd* lcmd = new CODBC_LangCmd(this, lang_query, nof_params);
    return lcmd;
}

CDB_LangCmd* CODBC_Connection::LangCmd(const string& lang_query,
                                     unsigned int  nof_params)
{
    return Create_LangCmd(*(xLangCmd(lang_query, nof_params)));
}


CDB_RPCCmd* CODBC_Connection::RPC(const string& rpc_name,
                                unsigned int  nof_args)
{
    string extra_msg = "RPC Command: " + rpc_name;
    m_Reporter.SetExtraMsg( extra_msg );

    CODBC_RPCCmd* rcmd = new CODBC_RPCCmd(this, rpc_name, nof_args);
    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CODBC_Connection::BCPIn(const string& table_name,
                                    unsigned int  nof_columns)
{
#ifdef FTDS_IN_USE
    return NULL; // not implemented yet
#else
    if ( !IsBCPable() ) {
        string err_message = "No bcp on this connection" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 410003 );
    }

    CODBC_BCPInCmd* bcmd = new CODBC_BCPInCmd(this, m_Link, table_name, nof_columns);
    return Create_BCPInCmd(*bcmd);
#endif
}


CDB_CursorCmd* CODBC_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  nof_params,
                                      unsigned int  batch_size)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    m_Reporter.SetExtraMsg( extra_msg );

#if 1 // defined(FTDS_IN_USE)
    CODBC_CursorCmdExpl* ccmd = new CODBC_CursorCmdExpl(this,
                                                cursor_name,
                                                query,
                                                nof_params);
#else
    CODBC_CursorCmd* ccmd = new CODBC_CursorCmd(this,
                                                cursor_name,
                                                query,
                                                nof_params);
#endif

    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CODBC_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                               size_t data_size, bool log_it)
{
    CODBC_SendDataCmd* sd_cmd =
        new CODBC_SendDataCmd(this,
                              (CDB_ITDescriptor&)descr_in,
                              data_size,
                              log_it);
    return Create_SendDataCmd(*sd_cmd);
}


bool CODBC_Connection::SendData(I_ITDescriptor& desc, CDB_Image& img, bool log_it)
{
    CStatementBase stmt(*this);

    SQLPOINTER  p = (SQLPOINTER)2;
    SQLLEN      s = img.Size();
    SQLLEN      ph;

    if((!ODBC_xSendDataPrepare(stmt, (CDB_ITDescriptor&)desc, s, false, log_it, p, &ph)) ||
       (!ODBC_xSendDataGetId(stmt, &p ))) {
        string err_message = "Cannot prepare a command" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 410035 );
    }

    return x_SendData(CDB_ITDescriptor::eBinary, stmt, img);

}


bool CODBC_Connection::SendData(I_ITDescriptor& desc, CDB_Text& txt, bool log_it)
{
    CStatementBase stmt(*this);

    SQLPOINTER  p = (SQLPOINTER)2;
    SQLLEN      s = txt.Size();
    SQLLEN      ph;

    if((!ODBC_xSendDataPrepare(stmt, (CDB_ITDescriptor&)desc, s, true, log_it, p, &ph)) ||
       (!ODBC_xSendDataGetId(stmt, &p))) {
        string err_message = "Cannot prepare a command" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 410035 );
    }

    return x_SendData(CDB_ITDescriptor::eText, stmt, txt);
}


bool CODBC_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    return IsAlive();
}


I_DriverContext::TConnectionMode CODBC_Connection::ConnectMode() const
{
    I_DriverContext::TConnectionMode mode = 0;
    if ( IsBCPable() ) {
        mode |= I_DriverContext::fBcpIn;
    }
    if ( HasSecureLogin() ) {
        mode |= I_DriverContext::fPasswordEncrypted;
    }
    return mode;
}


CODBC_Connection::~CODBC_Connection()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CODBC_Connection::Abort()
{
    SQLDisconnect(m_Link);
    return false;
}

bool CODBC_Connection::Close(void)
{
    if (Refresh()) {
        switch(SQLDisconnect(m_Link)) {
        case SQL_SUCCESS_WITH_INFO:
        case SQL_ERROR:
            ReportErrors();
        case SQL_SUCCESS:
            break;
        default:
            {
                string err_message = "SQLDisconnect failed (memory corruption suspected)" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410009 );
            }
        }

        if(SQLFreeHandle(SQL_HANDLE_DBC, m_Link) == SQL_ERROR) {
            ReportErrors();
        }

        m_Link = NULL;

        return true;
    }

    return false;
}

static
bool
ODBC_xCheckSIE(int rc, CStatementBase& stmt)
{
    switch(rc) {
    case SQL_SUCCESS_WITH_INFO:
        stmt.ReportErrors();
    case SQL_SUCCESS: break;
    case SQL_ERROR:
    default:
        stmt.ReportErrors();
        return false;
    }

    return true;
}

static bool ODBC_xSendDataPrepare(CStatementBase& stmt,
                                  CDB_ITDescriptor& descr_in,
                                  SQLLEN size,
                                  bool is_text,
                                  bool logit,
                                  SQLPOINTER id,
                                  SQLLEN* ph)
{
    string q = "update ";
    q += descr_in.TableName();
    q += " set ";
    q += descr_in.ColumnName();
    q += "= ? where ";
    q += descr_in.SearchConditions();
    //q+= " ;\nset rowcount 0";

#if defined(SQL_TEXTPTR_LOGGING) && !defined(FTDS_IN_USE)
    if(!logit) {
        switch(SQLSetStmtAttr(stmt.GetHandle(), SQL_TEXTPTR_LOGGING, /*SQL_SOPT_SS_TEXTPTR_LOGGING,*/
            (SQLPOINTER)SQL_TL_OFF, SQL_IS_INTEGER)) {
        case SQL_SUCCESS_WITH_INFO:
        case SQL_ERROR:
            stmt.ReportErrors();
        default:
            break;
        }
    }
#endif


    CDB_ITDescriptor::ETDescriptorType descr_type = descr_in.GetColumnType();
    if (descr_type == CDB_ITDescriptor::eUnknown) {
        if (is_text) {
            descr_type = CDB_ITDescriptor::eText;
        } else {
            descr_type = CDB_ITDescriptor::eBinary;
        }
    }

    *ph = SQL_LEN_DATA_AT_EXEC(size);

    int c_type = 0;
    int sql_type = 0;

    if (descr_type == CDB_ITDescriptor::eText) {
#ifdef UNICODE
        c_type = SQL_C_WCHAR;
        sql_type = SQL_WLONGVARCHAR;
#else
        c_type = SQL_C_CHAR;
        sql_type = SQL_LONGVARCHAR;
#endif
    } else {
        c_type = SQL_C_BINARY;
        sql_type = SQL_LONGVARBINARY;
    }

    // Do not use SQLDescribeParam. It is not implemented with the odbc driver
    // from FreeTDS.

    if (!ODBC_xCheckSIE(SQLBindParameter(
            stmt.GetHandle(),
            1,
            SQL_PARAM_INPUT,
            c_type,
            sql_type,
            size,
            0,
            id,
            0,
            ph),
        stmt)) {
        return false;
    }

    if (!ODBC_xCheckSIE(SQLPrepare(stmt.GetHandle(),
                                   CODBCString(q, odbc::DefStrEncoding),
                                   SQL_NTS),
                        stmt)) {
        return false;
    }

    switch(SQLExecute(stmt.GetHandle())) {
    case SQL_NEED_DATA:
        return true;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        stmt.ReportErrors();
    default:
        return false;
    }
}

static bool ODBC_xSendDataGetId(CStatementBase& stmt,
                                SQLPOINTER* id)
{
    switch(SQLParamData(stmt.GetHandle(), id)) {
    case SQL_NEED_DATA:
        return true;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        stmt.ReportErrors();
    default:
        return false;
    }
}

bool CODBC_Connection::x_SendData(CDB_ITDescriptor::ETDescriptorType descr_type,
                                  CStatementBase& stmt,
                                  CDB_Stream& stream)
{
    char buff[1800];

    int rc;

    size_t len = 0;
    size_t invalid_len = 0;

    while(( len = stream.Read(buff + invalid_len, sizeof(buff) - invalid_len - 1)) != 0 ) {
#ifdef UNICODE
        if (descr_type == CDB_ITDescriptor::eText) {
            // Convert string.

            size_t valid_len = CStringUTF8::GetValidBytesCount(buff, len);
            invalid_len = len - valid_len;

            CODBCString odbc_str(buff, valid_len, odbc::DefStrEncoding);
            // Force odbc_str to make conversion to wchar_t*.
            wchar_t* wchar_str = odbc_str;

            rc = SQLPutData(stmt.GetHandle(),
                            (SQLPOINTER)wchar_str,
                            (SQLINTEGER)odbc_str.GetSymbolNum() * sizeof(wchar_t) // Number of bytes ...
                            );

            if (valid_len < len) {
                memmove(buff, buff + valid_len, invalid_len);
            }
        } else {
            rc = SQLPutData(stmt.GetHandle(),
                            (SQLPOINTER)buff,
                            (SQLINTEGER)len // Number of bytes ...
                            );
        }
#else
        rc = SQLPutData(stmt.GetHandle(),
                        (SQLPOINTER)buff,
                        (SQLINTEGER)len // Number of bytes ...
                        );
#endif

        switch( rc ) {
        case SQL_SUCCESS_WITH_INFO:
            stmt.ReportErrors();
        case SQL_NEED_DATA:
            continue;
        case SQL_NO_DATA:
            return true;
        case SQL_SUCCESS:
            break;
        case SQL_ERROR:
            stmt.ReportErrors();
        default:
            return false;
        }
    }

    switch(SQLParamData(stmt.GetHandle(), (SQLPOINTER*)&len)) {
    case SQL_SUCCESS_WITH_INFO: stmt.ReportErrors();
    case SQL_SUCCESS:           break;
    case SQL_NO_DATA:           return true;
    case SQL_NEED_DATA:
        {
            string err_message = "Not all the data were sent" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 410044 );
        }
    case SQL_ERROR:             stmt.ReportErrors();
    default:
        {
            string err_message = "SQLParamData failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 410045 );
        }
    }

    for(;;) {
        switch(SQLMoreResults( stmt.GetHandle() )) {
        case SQL_SUCCESS_WITH_INFO: stmt.ReportErrors();
        case SQL_SUCCESS:           continue;
        case SQL_NO_DATA:           break;
        case SQL_ERROR:
            {
                stmt.ReportErrors();
                string err_message = "SQLMoreResults failed" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410014 );
            }
        default:
            {
                string err_message = "SQLMoreResults failed (memory corruption suspected)" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410015 );
            }
        }
        break;
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
CStatementBase::CStatementBase(CODBC_Connection& conn) :
    m_RowCount(-1),
    m_WasSent(false),
    m_HasFailed(false),
    m_ConnectPtr(&conn),
    m_Reporter(&conn.GetMsgHandlers(), SQL_HANDLE_STMT, NULL, &conn.m_Reporter)
{
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, conn.m_Link, &m_Cmd);

    if(rc == SQL_ERROR) {
        conn.ReportErrors();
    }
    m_Reporter.SetHandle(m_Cmd);
}

CStatementBase::~CStatementBase(void)
{
    try {
        SQLRETURN rc = SQLFreeHandle(SQL_HANDLE_STMT, m_Cmd);
        if(rc != SQL_SUCCESS) {
            ReportErrors();
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

bool
CStatementBase::CheckRC(int rc) const
{
    switch (rc)
    {
    case SQL_SUCCESS:
        return true;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        ReportErrors();
        break;
    case SQL_INVALID_HANDLE:
        {
            string err_message = "Invalid handle" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 0 );
        }
        break;
    }

    return false;
}

int
CStatementBase::CheckSIE(int rc, const char* msg, unsigned int msg_num) const
{
    switch( rc ) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();

    case SQL_SUCCESS:
        break;

    case SQL_ERROR:
        ReportErrors();
        {
            string err_message = msg + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, msg_num );
        }
    default:
        {
            string err_message;

            err_message.append(msg);
            err_message.append(" (memory corruption suspected)");
            err_message.append(GetDiagnosticInfo());

            DATABASE_DRIVER_ERROR( err_message, 420001 );
        }
    }

    return rc;
}

int
CStatementBase::CheckSIENd(int rc, const char* msg, unsigned int msg_num) const
{
    switch( rc ) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();

    case SQL_SUCCESS:
        break;

    case SQL_ERROR:
        ReportErrors();
        {
            string err_message = msg + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, msg_num );
        }
    default:
        {
            string err_message;

            err_message.append(msg);
            err_message.append(" (memory corruption suspected)");
            err_message.append(GetDiagnosticInfo());

            DATABASE_DRIVER_ERROR( err_message, 420001 );
        }
    }

    return rc;
}


string
CStatementBase::Type2String(const CDB_Object& param) const
{
    string type_str;

    switch (param.GetType()) {
    case eDB_Int:
        type_str = "int";
        break;
    case eDB_SmallInt:
        type_str = "smallint";
        break;
    case eDB_TinyInt:
        type_str = "tinyint";
        break;
    case eDB_BigInt:
        type_str = "numeric";
        break;
    case eDB_Char:
    case eDB_VarChar:
#ifdef UNICODE
        type_str = "nvarchar(255)";
#else
        type_str = "varchar(255)";
#endif
        break;
    case eDB_LongChar:
#ifdef UNICODE
        type_str = "nvarchar(4000)";
#else
        type_str = "varchar(8000)";
#endif
        break;
    case eDB_Binary:
    case eDB_VarBinary:
        type_str = "varbinary(255)";
        break;
    case eDB_LongBinary:
        type_str = "varbinary(8000)";
        break;
    case eDB_Float:
        type_str = "real";
        break;
    case eDB_Double:
        type_str = "float";
        break;
    case eDB_SmallDateTime:
        type_str = "smalldatetime";
        break;
    case eDB_DateTime:
        type_str = "datetime";
        break;
    }

    return type_str;
}

bool
CStatementBase::BindParam_ODBC(const CDB_Object& param,
                               CMemPot& bind_guard,
                               SQLLEN* indicator_base,
                               unsigned int pos) const
{
    SQLRETURN rc = 0;

    switch (param.GetType()) {
    case eDB_Int: {
        const CDB_Int& val = dynamic_cast<const CDB_Int&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : 4);
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 4, 0, val.BindVal(), 4, indicator_base + pos);
        break;
    }
    case eDB_SmallInt: {
        const CDB_SmallInt& val = dynamic_cast<const CDB_SmallInt&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : 2);
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_SSHORT,
                         SQL_SMALLINT, 2, 0, val.BindVal(), 2, indicator_base + pos);
        break;
    }
    case eDB_TinyInt: {
        const CDB_TinyInt& val = dynamic_cast<const CDB_TinyInt&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : 1);
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_UTINYINT,
                         SQL_TINYINT, 1, 0, val.BindVal(), 1, indicator_base + pos);
        break;
    }
    case eDB_BigInt: {
        const CDB_BigInt& val = dynamic_cast<const CDB_BigInt&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : 8);
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                         SQL_NUMERIC, 18, 0, val.BindVal(), 18, indicator_base + pos);

        break;
    }
    case eDB_Char: {
        const CDB_Char& val = dynamic_cast<const CDB_Char&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : SQL_NTS);
#ifdef UNICODE
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
                         SQL_WVARCHAR, 255, 0, (void*)val.AsUnicode(odbc::DefStrEncoding), 256 * sizeof(wchar_t), indicator_base + pos);
#else
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_VARCHAR, 255, 0, (void*)val.Value(), 256, indicator_base + pos);
#endif
        break;
    }
    case eDB_VarChar: {
        const CDB_VarChar& val = dynamic_cast<const CDB_VarChar&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : SQL_NTS);
#ifdef UNICODE
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
                         SQL_WVARCHAR, 255, 0, (void*)val.AsUnicode(odbc::DefStrEncoding), 256 * sizeof(wchar_t), indicator_base + pos);
#else
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                         SQL_VARCHAR, 255, 0, (void*)val.Value(), 256, indicator_base + pos);
#endif
        break;
    }
    case eDB_LongChar: {
        const CDB_LongChar& val = dynamic_cast<const CDB_LongChar&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : SQL_NTS);
#ifdef UNICODE
        rc = SQLBindParameter(GetHandle(),
                              pos + 1,
                              SQL_PARAM_INPUT,
                              SQL_C_WCHAR,
                              SQL_WLONGVARCHAR,
                              4000,
                              0,
                              (void*)val.AsUnicode(odbc::DefStrEncoding),
                              val.DataSize() * sizeof(wchar_t),
                              indicator_base + pos);
#else
        rc = SQLBindParameter(GetHandle(),
                              pos + 1,
                              SQL_PARAM_INPUT,
                              SQL_C_CHAR,
                              SQL_LONGVARCHAR,
                              8000,
                              0, (void*)val.Value(),
                              val.DataSize(),
                              indicator_base + pos);
#endif
        break;
    }
    case eDB_Binary: {
        const CDB_Binary& val = dynamic_cast<const CDB_Binary&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : val.Size());
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                         SQL_VARBINARY, 255, 0, (void*)val.Value(), 255, indicator_base + pos);
        break;
    }
    case eDB_VarBinary: {
        const CDB_VarBinary& val = dynamic_cast<const CDB_VarBinary&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : val.Size());
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                         SQL_VARBINARY, 255, 0, (void*)val.Value(), 255, indicator_base + pos);
        break;
    }
    case eDB_LongBinary: {
        const CDB_LongBinary& val = dynamic_cast<const CDB_LongBinary&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : val.DataSize());
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                         SQL_VARBINARY, 8000, 0, (void*)val.Value(), val.Size(), indicator_base + pos);
        break;
    }
    case eDB_Float: {
        const CDB_Float& val = dynamic_cast<const CDB_Float&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : 4);
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_FLOAT,
                         SQL_REAL, 4, 0, val.BindVal(), 4, indicator_base + pos);
        break;
    }
    case eDB_Double: {
        const CDB_Double& val = dynamic_cast<const CDB_Double&> (param);
        indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : 8);
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                         SQL_FLOAT, 8, 0, val.BindVal(), 8, indicator_base + pos);
        break;
    }
    case eDB_SmallDateTime: {
        const CDB_SmallDateTime& val = dynamic_cast<const CDB_SmallDateTime&> (param);
        SQL_TIMESTAMP_STRUCT* ts = 0;
        if(!val.IsNULL()) {
            ts= (SQL_TIMESTAMP_STRUCT*)bind_guard.Alloc(sizeof(SQL_TIMESTAMP_STRUCT));
            const CTime& t = val.Value();
            ts->year = t.Year();
            ts->month = t.Month();
            ts->day = t.Day();
            ts->hour = t.Hour();
            ts->minute = t.Minute();
            ts->second = 0;
            ts->fraction = 0;
            indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : sizeof(SQL_TIMESTAMP_STRUCT));
        }
        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                         SQL_TYPE_TIMESTAMP, 16, 0, (void*)ts, sizeof(SQL_TIMESTAMP_STRUCT),
                         indicator_base + pos);
        break;
    }
    case eDB_DateTime: {
        const CDB_DateTime& val = dynamic_cast<const CDB_DateTime&> (param);
        SQL_TIMESTAMP_STRUCT* ts = 0;

        if(!val.IsNULL()) {
            ts= (SQL_TIMESTAMP_STRUCT*)bind_guard.Alloc(sizeof(SQL_TIMESTAMP_STRUCT));
            const CTime& t = val.Value();
            ts->year = t.Year();
            ts->month = t.Month();
            ts->day = t.Day();
            ts->hour = t.Hour();
            ts->minute = t.Minute();
            ts->second = t.Second();
            ts->fraction = t.NanoSecond()/1000000;
            ts->fraction *= 1000000; /* MSSQL has a bug - it cannot handle fraction of msecs */
            indicator_base[pos] = (param.IsNULL() ? SQL_NULL_DATA : sizeof(SQL_TIMESTAMP_STRUCT));
        }

        rc = SQLBindParameter(GetHandle(), pos + 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                         SQL_TYPE_TIMESTAMP, 23, 3, ts, sizeof(SQL_TIMESTAMP_STRUCT),
                         indicator_base + pos);
        break;
    }
    default:
        return false;
    }

    CheckSIE(rc, "SQLBindParameter failed", 420066);

    return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_SendDataCmd::
//

CODBC_SendDataCmd::CODBC_SendDataCmd(CODBC_Connection* conn,
                                     CDB_ITDescriptor& descr,
                                     size_t nof_bytes,
                                     bool logit) :
    CStatementBase(*conn),
    impl::CSendDataCmd(nof_bytes),
    m_DescrType(descr.GetColumnType() == CDB_ITDescriptor::eText ?
                CDB_ITDescriptor::eText : CDB_ITDescriptor::eBinary)
{
    SQLPOINTER p = (SQLPOINTER)1;
    if((!ODBC_xSendDataPrepare(*this, descr, (SQLINTEGER)nof_bytes,
                              false, logit, p, &m_ParamPH)) ||
       (!ODBC_xSendDataGetId(*this, &p))) {

        string err_message = "Cannot prepare a command" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 410035 );
    }
}

size_t CODBC_SendDataCmd::SendChunk(const void* chunk_ptr, size_t nof_bytes)
{
    if(nof_bytes > m_Bytes2go) nof_bytes= m_Bytes2go;
    if(nof_bytes < 1) return 0;

    int rc;

#ifdef UNICODE
    if (m_DescrType == CDB_ITDescriptor::eText) {
        // Convert string.

        size_t valid_len = 0;

        valid_len = CStringUTF8::GetValidBytesCount(static_cast<const char*>(chunk_ptr),
                                                    nof_bytes);

        if (valid_len < nof_bytes) {
            DATABASE_DRIVER_ERROR( "Invalid encoding of a text string.", 410055 );
        }

        CODBCString odbc_str(static_cast<const char*>(chunk_ptr), nof_bytes, odbc::DefStrEncoding);
        // Force odbc_str to make conversion to wchar_t*.
        wchar_t* wchar_str = odbc_str;

        rc = SQLPutData(GetHandle(),
                        (SQLPOINTER)wchar_str,
                        (SQLINTEGER)odbc_str.GetSymbolNum() * sizeof(wchar_t) // Number of bytes ...
                        );
    } else {
        rc = SQLPutData(GetHandle(),
                        (SQLPOINTER)chunk_ptr,
                        (SQLINTEGER)nof_bytes // Number of bytes ...
                        );
    }
#else
    rc = SQLPutData(GetHandle(),
                    (SQLPOINTER)chunk_ptr,
                    (SQLINTEGER)nof_bytes // Number of bytes ...
                    );
#endif

    switch( rc ) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
    case SQL_NEED_DATA:
    case SQL_NO_DATA:
    case SQL_SUCCESS:
        m_Bytes2go-= nof_bytes;
        if(m_Bytes2go == 0) break;
        return nof_bytes;
    case SQL_ERROR:
        ReportErrors();
    default:
        return 0;
    }

    SQLPOINTER s= (SQLPOINTER)1;
    switch(SQLParamData(GetHandle(), (SQLPOINTER*)&s)) {
    case SQL_SUCCESS_WITH_INFO: ReportErrors();
    case SQL_SUCCESS:           break;
    case SQL_NO_DATA:           break;
    case SQL_NEED_DATA:
        {
            string err_message = "Not all the data were sent" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 410044 );
        }
    case SQL_ERROR:             ReportErrors();
    default:
        {
            string err_message = "SQLParamData failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 410045 );
        }
    }

    for(;;) {
        switch(SQLMoreResults(GetHandle())) {
        case SQL_SUCCESS_WITH_INFO: ReportErrors();
        case SQL_SUCCESS:           continue;
        case SQL_NO_DATA:           break;
        case SQL_ERROR:
            {
                ReportErrors();
                string err_message = "SQLMoreResults failed" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410014 );
            }
        default:
            {
                string err_message = "SQLMoreResults failed (memory corruption suspected)" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410015 );
            }
        }
        break;
    }
    return nof_bytes;
}

bool CODBC_SendDataCmd::Cancel(void)
{
    if (m_Bytes2go > 0) {
        xCancel();
        m_Bytes2go = 0;
        return true;
    }

    return false;
}

CODBC_SendDataCmd::~CODBC_SendDataCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

void CODBC_SendDataCmd::xCancel()
{
    if ( !Close() ) {
        return;
    }
    ResetParams();
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.50  2006/10/03 20:13:25  ssikorsk
 * strncpy --> memmove;
 * Get rid of warnings;
 *
 * Revision 1.49  2006/09/22 19:49:29  ssikorsk
 * Use fixed LongBinary size with SQLBindParameter. Set it to 8000.
 *
 * Revision 1.48  2006/09/22 18:51:47  ssikorsk
 * Use 8000 as a max datasize for varchar, varbinary and 4000 for nvarchar.
 *
 * Revision 1.47  2006/09/22 16:01:44  ssikorsk
 * Use SQL_LONGVARCHAR/SQL_WLONGVARCHAR with CDB_LongChar.
 *
 * Revision 1.46  2006/09/22 15:30:29  ssikorsk
 * Use CDB_LongChar::DataSize to retrieve an actual data size.
 *
 * Revision 1.45  2006/09/21 21:30:45  ssikorsk
 * Handle NULL values correctly with SQLBindParameter.
 *
 * Revision 1.44  2006/09/19 00:34:06  ucko
 * +<stdio.h> for sprintf()
 *
 * Revision 1.43  2006/09/18 15:31:04  ssikorsk
 * Improved reading data from streams in case of using of UTF8 in CODBC_Connection::x_SendData.
 *
 * Revision 1.42  2006/09/13 20:06:12  ssikorsk
 * Revamp code to support  unicode version of ODBC API.
 *
 * Revision 1.41  2006/08/22 18:08:45  ssikorsk
 * Improved calculation of CDB_ITDescriptor::ETDescriptorType in ODBC_xSendDataPrepare.
 *
 * Revision 1.40  2006/08/21 18:14:57  ssikorsk
 * Use explicit ursors with the MS Win odbc driver temporarily;
 * Revamp code to use CDB_ITDescriptor:: GetColumnType();
 *
 * Revision 1.39  2006/08/18 15:15:10  ssikorsk
 * Use CODBC_CursorCmdExpl with the FreeTDS odbc driver (because implicit cursors
 * are still not implemented) and CODBC_CursorCmd with the Windows SQL Server driver.
 *
 * Revision 1.38  2006/08/17 14:37:16  ssikorsk
 * Improved ODBC_xSendDataPrepare to behave correctly when HAS_DEFERRED_PREPARE is not defined (FreeTDS).
 *
 * Revision 1.37  2006/08/16 15:38:47  ssikorsk
 * Fixed a bug introduced during refactoring.
 *
 * Revision 1.36  2006/08/10 15:24:22  ssikorsk
 * Revamp code to use new CheckXXX methods.
 *
 * Revision 1.35  2006/08/04 20:39:48  ssikorsk
 * Do not use SQLBindParameter in case of FreeTDS. It is not implemented.
 *
 * Revision 1.34  2006/07/31 21:35:21  ssikorsk
 * Disable BCP for the ftds64_odbc driver temporarily.
 *
 * Revision 1.33  2006/07/18 15:47:59  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.32  2006/07/12 17:11:11  ssikorsk
 * Fixed compilation isssues.
 *
 * Revision 1.31  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.30  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.29  2006/06/05 21:07:25  ssikorsk
 * Deleted CODBC_Connection::Release;
 * Replaced "m_BR = 0" with "CDB_BaseEnt::Release()";
 *
 * Revision 1.28  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.27  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.26  2006/06/05 15:05:50  ssikorsk
 * Revamp code to use GetMsgHandlers() and DeleteAllCommands() instead of
 * m_MsgHandlers and m_CMDs.
 *
 * Revision 1.25  2006/06/05 14:44:32  ssikorsk
 * Moved methods PushMsgHandler, PopMsgHandler and DropCmd into I_Connection.
 *
 * Revision 1.24  2006/06/02 19:37:40  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.23  2006/05/31 16:56:11  ssikorsk
 * Replaced CPointerPot with deque<CDB_BaseEnt*>
 *
 * Revision 1.22  2006/05/15 19:39:30  ssikorsk
 * Added EOwnership argument to method PushMsgHandler.
 *
 * Revision 1.21  2006/04/05 14:32:51  ssikorsk
 * Implemented CODBC_Connection::Close
 *
 * Revision 1.20  2006/02/28 15:14:30  ssikorsk
 * Replaced argument type SQLINTEGER on SQLLEN where needed.
 *
 * Revision 1.19  2006/02/28 15:00:45  ssikorsk
 * Use larger type (SQLLEN) instead of SQLINTEGER where it needs to be converted to a pointer.
 *
 * Revision 1.18  2006/02/28 14:27:30  ssikorsk
 * Replaced int/SQLINTEGER variables with SQLLEN where needed.
 *
 * Revision 1.17  2006/02/28 14:00:47  ssikorsk
 * Fixed argument type misuse (like SQLINTEGER and SQLLEN) for vc8-x64 sake.
 *
 * Revision 1.16  2006/02/17 18:00:29  ssikorsk
 * Set CODBC_Connection::m_BCPable to false in ctor.
 *
 * Revision 1.15  2005/12/01 14:40:32  ssikorsk
 * Staticfunctions ODBC_xSendDataPrepare and ODBC_xSendDataGetId take
 * statement instead of connection as a parameter now.
 *
 * Revision 1.14  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.13  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.12  2005/11/02 12:58:38  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.11  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.10  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.9  2005/08/09 20:41:28  soussov
 * adds SQLDisconnect to Abort
 *
 * Revision 1.8  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.7  2005/02/23 21:40:55  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.6  2004/12/21 22:17:16  soussov
 * fixes bug in SendDataCmd
 *
 * Revision 1.5  2004/05/17 21:16:05  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.4  2003/06/05 16:02:04  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.3  2003/05/05 20:47:45  ucko
 * Check HAVE_ODBCSS_H; regardless, disable BCP on Unix, since even
 * DataDirect's implementation lacks the relevant bcp_* functions.
 *
 * Revision 1.2  2002/07/05 20:47:56  soussov
 * fixes bug in SendData
 *
 *
 * ===========================================================================
 */
