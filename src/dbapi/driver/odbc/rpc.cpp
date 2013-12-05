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
#include <dbapi/error_codes.hpp>

#include <stdio.h>

#include "odbc_utils.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_Odbc_Cmds

#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_ODBC_THROW(ex_class, message, err_code, severity)
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RPCCmd::
//

CODBC_RPCCmd::CODBC_RPCCmd(CODBC_Connection& conn,
                           const string& proc_name) :
    CStatementBase(conn, proc_name),
    m_Res(0)
{
    string extra_msg = "Procedure Name: " + proc_name;
    SetDbgInfo( extra_msg );

    return;
}


CDBParams& 
CODBC_RPCCmd::GetBindParams(void)
{
    if (m_InParams.get() == NULL) {
        m_InParams.reset(new impl::CRowInfo_SP_SQL_Server(
                    GetQuery(), 
                    GetConnImpl(), 
                    GetBindParamsImpl()
                    )
                );
    }

    return *m_InParams;
}


bool CODBC_RPCCmd::Send()
{
    Cancel();

    SetHasFailed(false);
    m_HasStatus = false;

    // make a language command
    string main_exec_query("declare @STpROCrETURNsTATUS int;\nexec @STpROCrETURNsTATUS=");
    main_exec_query += GetQuery();
    string param_result_query;

    CMemPot bindGuard;
    string q_str;

    if(GetBindParamsImpl().NofParams() > 0) {
        SQLLEN* indicator = (SQLLEN*)
                bindGuard.Alloc(GetBindParamsImpl().NofParams() * sizeof(SQLLEN));

        if (!x_AssignParams(q_str, main_exec_query, param_result_query,
                          bindGuard, indicator)) {
            ResetParams();
            SetHasFailed();

            string err_message = "Cannot assign params." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420003 );
        }
    }

   if(NeedToRecompile()) main_exec_query += " with recompile";

   q_str += main_exec_query + ";\nselect STpROCrETURNsTATUS=@STpROCrETURNsTATUS";
   if(!param_result_query.empty()) {
       q_str += ";\nselect " + param_result_query;
   }

    TSqlString ss = x_MakeTSqlString(q_str, GetClientEncoding());
    switch (SQLExecDirect(GetHandle(), const_cast<TSqlChar*>(ss.data()),
                          ss.size())) {
    case SQL_SUCCESS:
        m_HasMoreResults = true;
        break;

    case SQL_NO_DATA:
        m_HasMoreResults = true; /* this is a bug in SQLExecDirect it returns SQL_NO_DATA if
                               status result is the only result of RPC */
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
            string err_message = "Some other query is executing on this connection." +
                GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420002 );
        }

    case SQL_INVALID_HANDLE:
        SetHasFailed();
        {
            string err_message = "The statement handler is invalid (memory corruption suspected)." +
                GetDbgInfo();
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

    SetWasSent();
    return true;
}


bool CODBC_RPCCmd::Cancel()
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


CDB_Result* CODBC_RPCCmd::Result()
{
    enum {eNameStrLen = 64};

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

    SQLSMALLINT nof_cols = 0;
    odbc::TChar buffer[eNameStrLen];

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

            if(util::strcmp(buffer, _T_NCBI_ODBC("STpROCrETURNsTATUS")) == 0) {
				//this is a status result
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

    SetWasSent(false);
    return 0;
}


bool CODBC_RPCCmd::HasMoreResults() const
{
    return m_HasMoreResults;
}


int CODBC_RPCCmd::RowCount() const
{
    return static_cast<int>(m_RowCount);
}


CODBC_RPCCmd::~CODBC_RPCCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL_X( 5, NCBI_CURRENT_FUNCTION )
}


bool CODBC_RPCCmd::x_AssignParams(string& cmd, string& q_exec, string& q_select,
                                   CMemPot& bind_guard, SQLLEN* indicator)
{
    char p_nm[16];
    // check if we do have a named parameters (first named - all named)
    bool param_named = !GetBindParamsImpl().GetParamName(0).empty();

    for (unsigned int n = 0; n < GetBindParamsImpl().NofParams(); n++) {
        if(GetBindParamsImpl().GetParamStatus(n) == 0) continue;
        const string& name  =  GetBindParamsImpl().GetParamName(n);
        CDB_Object&   param = *GetBindParamsImpl().GetParam(n);

        if (!x_BindParam_ODBC(param, bind_guard, indicator, n)) {
            return false;
        }

        q_exec += n ? ',':' ';

        const string type = Type2String(param);
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

        if ((GetBindParamsImpl().GetParamStatus(n) & impl::CDB_Params::fOutput) != 0) {
            q_exec += " output";
            const char* p_name = param_named? name.c_str() : p_nm;
            if(!q_select.empty()) q_select += ',';
            q_select.append(p_name+1);
            q_select += '=';
            q_select += p_name;
        }
    }
    GetBindParamsImpl().LockBinding();
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
            string err_message = "SQLMoreResults failed." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420014 );
        }
    default:
        {
            string err_message = "SQLMoreResults failed (memory corruption suspected)." +
                GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 420015 );
        }
    }
}

END_NCBI_SCOPE


