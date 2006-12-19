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
#include <stdio.h>
#include <dbapi/driver/odbc/interfaces.hpp>

#include "odbc_utils.hpp"

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_LangCmd::
//

CODBC_LangCmd::CODBC_LangCmd(
    CODBC_Connection* conn,
    const string& lang_query,
    unsigned int nof_params
    ) :
    CStatementBase(*conn),
    impl::CBaseCmd(lang_query, nof_params),
    m_Res(NULL)
{
    _ASSERT( conn );

/* This logic is not working for some reason
    if ( SQLSetStmtAttr(m_Cmd, SQL_ATTR_ROWS_FETCHED_PTR, &m_RowCount, sizeof(m_RowCount)) != SQL_SUCCESS ) {
        DATABASE_DRIVER_ERROR( "SQLSetStmtAttr failed (memory corruption suspected)", 420014 );
    }
*/
    // string extra_msg = "SQL Command: \"" + lang_query + "\"";
    // m_Reporter.SetExtraMsg( extra_msg );
}


bool CODBC_LangCmd::Send(void)
{
    Cancel();

    m_HasFailed = false;

    CMemPot bindGuard;
    string q_str;

    if(GetParams().NofParams() > 0) {
        SQLLEN* indicator = (SQLLEN*)
                bindGuard.Alloc(GetParams().NofParams() * sizeof(SQLLEN));

        if (!x_AssignParams(q_str, bindGuard, indicator)) {
            ResetParams();
            m_HasFailed = true;

            string err_message = "cannot assign params" + GetDiagnosticInfo();
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

    switch(SQLExecDirect(GetHandle(), CODBCString(*real_query, GetClientEncoding()), SQL_NTS)) {
    case SQL_SUCCESS:
        m_hasResults = true;
        break;

    case SQL_NO_DATA:
        m_hasResults = false;
        m_RowCount = 0;
        break;

    case SQL_ERROR:
        ReportErrors();
        ResetParams();
        m_HasFailed = true;
        {
            string err_message = "SQLExecDirect failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420001 );
        }

    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
        m_hasResults = true;
        break;

    case SQL_STILL_EXECUTING:
        ReportErrors();
        ResetParams();
        m_HasFailed = true;
        {
            string err_message = "Some other query is executing on this connection" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420002 );
        }

    case SQL_INVALID_HANDLE:
        m_HasFailed = true;
        {
            string err_message = "The statement handler is invalid (memory corruption suspected)" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420004 );
        }

    default:
        ReportErrors();
        ResetParams();
        m_HasFailed = true;
        {
            string err_message = "Unexpected error" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420005 );
        }

    }

    m_WasSent = true;
    return true;
}


bool CODBC_LangCmd::WasSent() const
{
    return m_WasSent;
}


bool CODBC_LangCmd::Cancel()
{
    if (m_WasSent) {
        if (m_Res) {
            delete m_Res;
            m_Res = 0;
        }

        m_WasSent = false;

        if ( !Close() ) {
            return false;
        }

        ResetParams();
        // GetQuery().erase();
    }

    return true;
}


bool CODBC_LangCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CODBC_LangCmd::Result()
{
    if (m_Res) {
        delete m_Res;
        m_Res = 0;
        m_hasResults= xCheck4MoreResults();
    }

    if ( !m_WasSent ) {
        string err_message = "a command has to be sent first" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 420010 );
    }

    if(!m_hasResults) {
        m_WasSent = false;
        return 0;
    }

    SQLSMALLINT nof_cols= 0;

    while(m_hasResults) {
        CheckSIE(SQLNumResultCols(GetHandle(), &nof_cols),
                 "SQLNumResultCols failed", 420011);

        if(nof_cols < 1) { // no data in this result set
            SQLLEN rc;

            CheckSIE(SQLRowCount(GetHandle(), &rc),
                     "SQLRowCount failed", 420013);

            m_RowCount = rc;
            m_hasResults= xCheck4MoreResults();
            continue;
        }

        m_Res = new CODBC_RowResult(*this, nof_cols, &m_RowCount);
        return Create_Result(*m_Res);
    }

    m_WasSent = false;
    return 0;
}


bool CODBC_LangCmd::HasMoreResults() const
{
    return m_hasResults;
}

void CODBC_LangCmd::DumpResults()
{
    CDB_Result* dbres;
    while(m_WasSent) {
        dbres= Result();
        if(dbres) {
            m_RowCount = 0;

            if(GetConnection().GetResultProcessor()) {
                GetConnection().GetResultProcessor()->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch());
            }
            delete dbres;
        }
    }
}

bool CODBC_LangCmd::HasFailed() const
{
    return m_HasFailed;
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
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CODBC_LangCmd::x_AssignParams(string& cmd, CMemPot& bind_guard, SQLLEN* indicator)
{
    for (unsigned int n = 0; n < GetParams().NofParams(); ++n) {
        if(GetParams().GetParamStatus(n) == 0) continue;
        const string& name  =  GetParams().GetParamName(n);
        const CDB_Object& param = *GetParams().GetParam(n);

        const string type = Type2String(param);
        if (!x_BindParam_ODBC(param, bind_guard, indicator, n)) {
            return false;
        }

        cmd += "declare " + name + ' ' + type + ";select " + name + " = ?;";

        if(param.IsNULL()) {
            indicator[n] = SQL_NULL_DATA;
        }
    }
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

            string err_message = "SQLMoreResults failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420014 );
        }
    default:
        {
            string err_message = "SQLMoreResults failed (memory corruption suspected)" + GetDiagnosticInfo();
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

    CheckSIE(SQLSetCursorName(GetHandle(), CODBCString(name, GetClientEncoding()), static_cast<SQLSMALLINT>(name.size())),
             "SQLSetCursorName failed", 420016);
}


void
CODBC_LangCmd::CloseCursor(void) const
{
    CheckSIE(SQLCloseCursor(GetHandle()),
             "SQLCloseCursor failed", 420017);
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.37  2006/12/19 20:45:56  ssikorsk
 * Get rid of compilation warnings on vc8 x64.
 *
 * Revision 1.36  2006/11/20 18:15:59  ssikorsk
 * Revamp code to use GetQuery() and GetParams() methods.
 *
 * Revision 1.35  2006/10/26 15:07:57  ssikorsk
 * Use a charset provided by a client instead of a default one.
 *
 * Revision 1.34  2006/09/18 15:32:43  ssikorsk
 * Redesigned CODBC_LangCmd::x_AssignParams using BindParam_ODBC.
 *
 * Revision 1.33  2006/09/13 20:07:32  ssikorsk
 * Revamp code to support  unicode version of ODBC API.
 *
 * Revision 1.32  2006/08/18 15:18:09  ssikorsk
 * Improved the CODBC_LangCmd::SetCursorName method in order to open
 * server-side cursor.
 *
 * Revision 1.31  2006/08/17 14:40:23  ssikorsk
 * Implemented SetCursorName and CloseCursor.
 *
 * Revision 1.30  2006/08/10 15:24:22  ssikorsk
 * Revamp code to use new CheckXXX methods.
 *
 * Revision 1.29  2006/07/18 15:47:59  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.28  2006/07/12 17:11:11  ssikorsk
 * Fixed compilation isssues.
 *
 * Revision 1.27  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.26  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.25  2006/06/06 14:46:31  ssikorsk
 * Fixed CODBC_LangCmd::Cancel.
 *
 * Revision 1.24  2006/06/05 21:09:22  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.23  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.22  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.21  2006/06/02 19:37:40  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.20  2006/02/28 14:27:30  ssikorsk
 * Replaced int/SQLINTEGER variables with SQLLEN where needed.
 *
 * Revision 1.19  2006/02/28 14:00:47  ssikorsk
 * Fixed argument type misuse (like SQLINTEGER and SQLLEN) for vc8-x64 sake.
 *
 * Revision 1.18  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.17  2005/12/28 13:15:00  ssikorsk
 * Roll back an accidental commit
 *
 * Revision 1.16  2005/12/28 13:10:03  ssikorsk
 * Restore CSafeStaticPtr-based singleton
 *
 * Revision 1.15  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.14  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.13  2005/11/02 12:59:34  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.12  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.11  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.10  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.9  2005/02/15 16:07:51  ssikorsk
 * Fixed a bug with GetRowCount plus SELECT statement
 *
 * Revision 1.8  2004/05/17 21:16:06  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.7  2003/11/07 17:14:20  soussov
 * work around the odbc bug. It can not handle properly the fractions of msecs
 *
 * Revision 1.6  2003/11/06 20:33:32  soussov
 * fixed bug in DateTime bindings
 *
 * Revision 1.5  2003/06/05 16:02:04  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.4  2003/05/16 20:26:44  soussov
 * adds code to skip parameter if it was not set
 *
 * Revision 1.3  2003/05/08 20:40:08  soussov
 * adds stdio.h for sprintf
 *
 * Revision 1.2  2003/05/08 20:30:24  soussov
 * CDB_LongChar CDB_LongBinary added
 *
 * Revision 1.1  2002/06/18 22:06:24  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */
