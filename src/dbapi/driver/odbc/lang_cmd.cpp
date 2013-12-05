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
 * File Description:  ODBC language command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/error_codes.hpp>

#include "odbc_utils.hpp"

#include <stdio.h>


#define NCBI_USE_ERRCODE_X   Dbapi_Odbc_Cmds

#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_ODBC_THROW(ex_class, message, err_code, severity)
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_LangCmd::
//

CODBC_LangCmd::CODBC_LangCmd(
    CODBC_Connection& conn,
    const string& lang_query
    ) :
    CStatementBase(conn, lang_query),
    m_Res(NULL)
{
/* This logic is not working for some reason
    if ( SQLSetStmtAttr(m_Cmd, SQL_ATTR_ROWS_FETCHED_PTR, &m_RowCount, sizeof(m_RowCount)) != SQL_SUCCESS ) {
        DATABASE_DRIVER_ERROR( "SQLSetStmtAttr failed (memory corruption suspected)", 420014 );
    }
*/
    SetDbgInfo("SQL Command: \"" + lang_query + "\"");

}


bool CODBC_LangCmd::Send(void)
{
    Cancel();

    SetHasFailed(false);

    CMemPot bindGuard;
    string q_str;

    if(GetBindParamsImpl().NofParams() > 0) {
        SQLLEN* indicator = (SQLLEN*)
                bindGuard.Alloc(GetBindParamsImpl().NofParams() * sizeof(SQLLEN));

        if (!x_AssignParams(q_str, bindGuard, indicator)) {
            ResetParams();
            SetHasFailed();

            string err_message = "Cannot assign params." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420003 );
        }
    }

    const string* real_query;
    if(!q_str.empty()) {
        q_str.append(GetQuery());
        real_query = &q_str;
    }
    else {
        real_query = &GetQuery();
    }

    TSqlString ss = x_MakeTSqlString(*real_query, GetClientEncoding());
    switch (SQLExecDirect(GetHandle(), const_cast<TSqlChar*>(ss.data()),
                          ss.size())) {
    // switch(SQLExecDirect(GetHandle(), tchar_str, odbc_str.GetSymbolNum())) {
    case SQL_SUCCESS:
        m_HasMoreResults = true;
        break;

    case SQL_NO_DATA:
        m_HasMoreResults = false;
        m_RowCount = 0;
        break;

    case SQL_ERROR:
        ReportErrors();
        ResetParams();
        SetHasFailed();
        {
            string err_message = "SQLExecDirect failed." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420001 );
        }

    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
        m_HasMoreResults = true;
        break;

    case SQL_STILL_EXECUTING:
        ReportErrors();
        ResetParams();
        SetHasFailed();
        {
            string err_message = "Some other query is executing on this connection." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420002 );
        }

    case SQL_INVALID_HANDLE:
        SetHasFailed();
        {
            string err_message = "The statement handler is invalid (memory corruption suspected)." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420004 );
        }

    default:
        ReportErrors();
        ResetParams();
        SetHasFailed();
        {
            string err_message = "Unexpected error." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420005 );
        }

    }

    SetWasSent(true);
    return true;
}


bool CODBC_LangCmd::Cancel()
{
    if (WasSent()) {
        if (m_Res) {
            delete m_Res;
            m_Res = 0;
        }

        SetWasSent(false);

        if ( !Close() ) {
            return false;
        }

        ResetParams();
        // GetQuery().erase();
    }

    return true;
}


CDB_Result* CODBC_LangCmd::Result()
{
    if (m_Res) {
        delete m_Res;
        m_Res = 0;
        m_HasMoreResults = xCheck4MoreResults();
    }

    if ( !WasSent() ) {
        string err_message = "A command has to be sent first." + GetDbgInfo();
        DATABASE_DRIVER_ERROR( err_message, 420010 );
    }

    if(!m_HasMoreResults) {
        SetWasSent(false);
        return 0;
    }

    SQLSMALLINT nof_cols= 0;

    while(m_HasMoreResults) {
        CheckSIE(SQLNumResultCols(GetHandle(), &nof_cols),
                 "SQLNumResultCols failed", 420011);

        if(nof_cols < 1) { // no data in this result set
            SQLLEN rc;

            CheckSIE(SQLRowCount(GetHandle(), &rc),
                     "SQLRowCount failed", 420013);

            m_RowCount = rc;
            m_HasMoreResults = xCheck4MoreResults();
            continue;
        }

        m_Res = new CODBC_RowResult(*this, nof_cols, &m_RowCount);
        return Create_Result(*m_Res);
    }

    SetWasSent(false);
    return 0;
}


bool CODBC_LangCmd::HasMoreResults() const
{
    return m_HasMoreResults;
}


int CODBC_LangCmd::RowCount() const
{
    return static_cast<int>(m_RowCount);
}


CODBC_LangCmd::~CODBC_LangCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL_X( 4, NCBI_CURRENT_FUNCTION )
}


bool CODBC_LangCmd::x_AssignParams(string& cmd, CMemPot& bind_guard, SQLLEN* indicator)
{
    for (unsigned int n = 0; n < GetBindParamsImpl().NofParams(); ++n) {
        if(GetBindParamsImpl().GetParamStatus(n) == 0) continue;
        const string& name  =  GetBindParamsImpl().GetParamName(n);
        if (name.empty()) {
            DATABASE_DRIVER_ERROR( "Binding by position is not supported." + GetDbgInfo(), 420110 );
        }
        const CDB_Object& param = *GetBindParamsImpl().GetParam(n);

        const string type = Type2String(param);
        if (!x_BindParam_ODBC(param, bind_guard, indicator, n)) {
            return false;
        }

        cmd += "declare " + name + ' ' + type + ";select " + name + " = ?;";

        if(param.IsNULL()) {
            indicator[n] = SQL_NULL_DATA;
        }
    }

    GetBindParamsImpl().LockBinding();
    return true;
}


bool CODBC_LangCmd::xCheck4MoreResults()
{
//     int rc = CheckSIE(SQLMoreResults(GetHandle()), "SQLBindParameter failed", 420066);
//
//     return (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_SUCCESS);

    switch(SQLMoreResults(GetHandle())) {
    case SQL_SUCCESS_WITH_INFO: ReportErrors();
    case SQL_SUCCESS:           return true;
    case SQL_NO_DATA:           return false;
    case SQL_ERROR:
        {
            ReportErrors();

            string err_message = "SQLMoreResults failed." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420014 );
        }
    default:
        {
            string err_message = "SQLMoreResults failed (memory corruption suspected)." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420015 );
        }
    }
}


void CODBC_LangCmd::SetCursorName(const string& name) const
{
    // Set statement attributes so server-side cursor is generated

    // The default ODBC cursor attributes are:
    // SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, SQL_CURSOR_FORWARD_ONLY);
    // SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, SQL_CONCUR_READ_ONLY);
    // SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, 1);

    // CheckSIE(SQLSetStmtAttr(GetHandle(), SQL_ROWSET_SIZE, (void*)2, SQL_NTS),
    //          "SQLSetStmtAttr(SQL_ROWSET_SIZE) failed", 420015);
    CheckSIE(SQLSetStmtAttr(GetHandle(), SQL_ATTR_CONCURRENCY, (void*)SQL_CONCUR_VALUES, SQL_NTS),
             "SQLSetStmtAttr(SQL_ATTR_CONCURRENCY) failed", 420017);
    CheckSIE(SQLSetStmtAttr(GetHandle(), SQL_ATTR_CURSOR_TYPE, (void*)SQL_CURSOR_FORWARD_ONLY, SQL_NTS),
             "SQLSetStmtAttr(SQL_ATTR_CURSOR_TYPE) failed", 420018);


    TSqlString ss = x_MakeTSqlString(name, GetClientEncoding());
    CheckSIE(SQLSetCursorName(GetHandle(), const_cast<TSqlChar*>(ss.data()),
                              static_cast<SQLSMALLINT>(ss.size())),
             "SQLSetCursorName failed", 420016);
}


bool
CODBC_LangCmd::CloseCursor(void)
{
	bool result = true;

	try {
		CheckSIE(SQLCloseCursor(GetHandle()),
				 "SQLCloseCursor failed", 420017);
	}
	catch (...)
	{
		result = false;
	}

	return result;
}

END_NCBI_SCOPE


