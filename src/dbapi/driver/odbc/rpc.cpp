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
 * File Description:  ODBC RPC command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>

#include <stdio.h>

#include "odbc_utils.hpp"

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RPCCmd::
//

CODBC_RPCCmd::CODBC_RPCCmd(CODBC_Connection* conn,
                           const string& proc_name,
                           unsigned int nof_params) :
    CStatementBase(*conn),
    impl::CBaseCmd(proc_name, nof_params),
    m_Res(0)
{
    string extra_msg = "Procedure Name: " + proc_name;
    SetDiagnosticInfo( extra_msg );

    return;
}


bool CODBC_RPCCmd::Send()
{
    Cancel();

    m_HasFailed = false;
    m_HasStatus = false;

    // make a language command
    string main_exec_query("declare @STpROCrETURNsTATUS int;\nexec @STpROCrETURNsTATUS=");
    main_exec_query += m_Query;
    string param_result_query;

    CMemPot bindGuard;
    string q_str;

    if(m_Params.NofParams() > 0) {
        SQLLEN* indicator = (SQLLEN*)
                bindGuard.Alloc(m_Params.NofParams() * sizeof(SQLLEN));

        if (!x_AssignParams(q_str, main_exec_query, param_result_query,
                          bindGuard, indicator)) {
            ResetParams();
            m_HasFailed = true;

            string err_message = "cannot assign params" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420003 );
        }
    }

   if(NeedToRecompile()) main_exec_query += " with recompile";

   q_str += main_exec_query + ";\nselect STpROCrETURNsTATUS=@STpROCrETURNsTATUS";
   if(!param_result_query.empty()) {
       q_str += ";\nselect " + param_result_query;
   }

    switch(SQLExecDirect(GetHandle(), CODBCString(q_str, odbc::DefStrEncoding), SQL_NTS)) {
    case SQL_SUCCESS:
        m_hasResults = true;
        break;

    case SQL_NO_DATA:
        m_hasResults = true; /* this is a bug in SQLExecDirect it returns SQL_NO_DATA if
                               status result is the only result of RPC */
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
            string err_message = "Some other query is executing on this connection" +
                GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420002 );
        }

    case SQL_INVALID_HANDLE:
        m_HasFailed = true;
        {
            string err_message = "The statement handler is invalid (memory corruption suspected)" +
                GetDiagnosticInfo();
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


bool CODBC_RPCCmd::WasSent() const
{
    return m_WasSent;
}


bool CODBC_RPCCmd::Cancel()
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
        m_Query.erase();
    }

    return true;
}


bool CODBC_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CODBC_RPCCmd::Result()
{
    enum {eNameStrLen = 64};

    if (m_Res) {
        delete m_Res;
        m_Res = 0;
        m_hasResults = xCheck4MoreResults();
    }

    if ( !m_WasSent ) {
        string err_message = "a command has to be sent first" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 420010 );
    }

    if(!m_hasResults) {
        m_WasSent = false;
        return 0;
    }

    SQLSMALLINT nof_cols = 0;
    odbc::TChar buffer[eNameStrLen];

    while(m_hasResults) {
        CheckSIE(SQLNumResultCols(GetHandle(), &nof_cols),
                 "SQLNumResultCols failed", 420011);

        if(nof_cols < 1) { // no data in this result set
            SQLLEN rc;

            CheckSIE(SQLRowCount(GetHandle(), &rc),
                     "SQLRowCount failed", 420013);

            m_RowCount = rc;
            m_hasResults = xCheck4MoreResults();
            continue;
        }

        if(nof_cols == 1) { // it could be a status result
            SQLSMALLINT l;

            CheckSIE(SQLColAttribute(GetHandle(),
                                     1,
                                     SQL_DESC_LABEL,
                                     buffer,
                                     sizeof(buffer),
                                     &l,
                                     0),
                     "SQLColAttribute failed", 420015);

            if(util::strcmp(buffer, _T("STpROCrETURNsTATUS")) == 0) {//this is a status result
                m_HasStatus = true;
                m_Res = new CODBC_StatusResult(*this);
            }
        }
        if(!m_Res) {
            if(m_HasStatus) {
                m_HasStatus = false;
                m_Res = new CODBC_ParamResult(*this, nof_cols);
            }
            else {
                m_Res = new CODBC_RowResult(*this, nof_cols, &m_RowCount);
            }
        }
        return Create_Result(*m_Res);
    }

    m_WasSent = false;
    return 0;
}


bool CODBC_RPCCmd::HasMoreResults() const
{
    return m_hasResults;
}

void CODBC_RPCCmd::DumpResults()
{
    CDB_Result* dbres;
    while(m_WasSent) {
        dbres = Result();
        if(dbres) {
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

bool CODBC_RPCCmd::HasFailed() const
{
    return m_HasFailed;
}


int CODBC_RPCCmd::RowCount() const
{
    return m_RowCount;
}


CODBC_RPCCmd::~CODBC_RPCCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CODBC_RPCCmd::x_AssignParams(string& cmd, string& q_exec, string& q_select,
                                   CMemPot& bind_guard, SQLLEN* indicator)
{
    char p_nm[16];
    // check if we do have a named parameters (first named - all named)
    bool param_named = !m_Params.GetParamName(0).empty();

    for (unsigned int n = 0; n < m_Params.NofParams(); n++) {
        if(m_Params.GetParamStatus(n) == 0) continue;
        const string& name  =  m_Params.GetParamName(n);
        CDB_Object&   param = *m_Params.GetParam(n);

        const string type = Type2String(param);
        if (!BindParam_ODBC(param, bind_guard, indicator, n)) {
            return false;
        }

        q_exec += n ? ',':' ';

        if(!param_named) {
            sprintf(p_nm, "@pR%d", n);
            q_exec += p_nm;
            cmd += "declare ";
            cmd += p_nm;
            cmd += ' ';
            cmd += type;
            cmd += ";select ";
            cmd += p_nm;
            cmd += " = ?;";
        }
        else {
            q_exec += name+'='+name;
            cmd += "declare " + name + ' ' + type + ";select " + name + " = ?;";
        }

        if(param.IsNULL()) {
            indicator[n] = SQL_NULL_DATA;
        }

        if ((m_Params.GetParamStatus(n) & CDB_Params::fOutput) != 0) {
            q_exec += " output";
            const char* p_name = param_named? name.c_str() : p_nm;
            if(!q_select.empty()) q_select += ',';
            q_select.append(p_name+1);
            q_select += '=';
            q_select += p_name;
        }
    }
    return true;
}

bool CODBC_RPCCmd::xCheck4MoreResults()
{
//     int rc = CheckSIE(SQLMoreResults(GetHandle()),
//              "SQLMoreResults failed", 420015);
//
//     return (rc == SQL_SUCCESS_WITH_INFO || rc == SQL_SUCCESS);

    switch(SQLMoreResults(GetHandle())) {
    case SQL_SUCCESS_WITH_INFO: ReportErrors();
    case SQL_SUCCESS:           return true;
    case SQL_NO_DATA:           return false;
    case SQL_ERROR:
        ReportErrors();
        {
            string err_message = "SQLMoreResults failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420014 );
        }
    default:
        {
            string err_message = "SQLMoreResults failed (memory corruption suspected)" +
                GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420015 );
        }
    }
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.33  2006/10/19 16:18:06  ssikorsk
 * Fixed handling of unicode strings in CODBC_RPCCmd::Result.
 *
 * Revision 1.32  2006/09/18 15:34:30  ssikorsk
 * Redesigned CODBC_RPCCmd::x_AssignParams using BindParam_ODBC.
 *
 * Revision 1.31  2006/09/13 20:09:25  ssikorsk
 * Revamp code to support  unicode version of ODBC API.
 *
 * Revision 1.30  2006/08/10 15:24:22  ssikorsk
 * Revamp code to use new CheckXXX methods.
 *
 * Revision 1.29  2006/07/18 16:37:21  ssikorsk
 * Fixed compilation issues.
 *
 * Revision 1.28  2006/07/18 15:47:59  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.27  2006/07/12 17:11:11  ssikorsk
 * Fixed compilation isssues.
 *
 * Revision 1.26  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.25  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.24  2006/06/06 14:46:06  ssikorsk
 * Fixed CODBC_RPCCmd::Cancel.
 *
 * Revision 1.23  2006/06/05 21:09:22  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.22  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.21  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.20  2006/06/02 19:37:40  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.19  2006/02/28 14:27:30  ssikorsk
 * Replaced int/SQLINTEGER variables with SQLLEN where needed.
 *
 * Revision 1.18  2006/02/28 14:00:47  ssikorsk
 * Fixed argument type misuse (like SQLINTEGER and SQLLEN) for vc8-x64 sake.
 *
 * Revision 1.17  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.16  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.15  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.14  2005/11/02 12:58:38  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.13  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.12  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.11  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.10  2005/02/15 16:07:51  ssikorsk
 * Fixed a bug with GetRowCount plus SELECT statement
 *
 * Revision 1.9  2004/05/17 21:16:06  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2004/01/27 18:00:07  soussov
 * patches the SQLExecDirect bug
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
 * Revision 1.3  2003/05/08 20:30:24  soussov
 * CDB_LongChar CDB_LongBinary added
 *
 * Revision 1.2  2003/05/05 20:48:47  ucko
 * +<stdio.h> for sprintf
 *
 * Revision 1.1  2002/06/18 22:06:25  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */
