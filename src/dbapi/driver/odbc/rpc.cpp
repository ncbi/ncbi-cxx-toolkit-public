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

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RPCCmd::
//

CODBC_RPCCmd::CODBC_RPCCmd(CODBC_Connection* con, SQLHSTMT cmd,
                         const string& proc_name, unsigned int nof_params) :
    m_Connect(con), m_Cmd(cmd), m_Query(proc_name), m_Params(nof_params),
    m_WasSent(false), m_HasFailed(false), m_Recompile(false), m_Res(0),
    m_RowCount(-1), m_Reporter(&con->m_MsgHandlers, SQL_HANDLE_STMT, cmd)
{
    return;
}


bool CODBC_RPCCmd::BindParam(const string& param_name,
                            CDB_Object* param_ptr, bool out_param)
{
    return m_Params.BindParam(CDB_Params::kNoParamNumber, param_name,
                              param_ptr, out_param);
}


bool CODBC_RPCCmd::SetParam(const string& param_name,
                           CDB_Object* param_ptr, bool out_param)
{
    return m_Params.SetParam(CDB_Params::kNoParamNumber, param_name,
                             param_ptr, out_param);
}


bool CODBC_RPCCmd::Send()
{
    if (m_WasSent)
        Cancel();

    m_HasFailed = false;
    m_HasStatus = false;

    // make a language command
    string main_exec_query("declare @STpROCrETURNsTATUS int;\nexec @STpROCrETURNsTATUS=");
    main_exec_query+= m_Query;
    string param_result_query;

    CMemPot bindGuard;
    string q_str;

	if(m_Params.NofParams() > 0) {
		SQLINTEGER* indicator= (SQLINTEGER*)
				bindGuard.Alloc(m_Params.NofParams()*sizeof(SQLINTEGER));

		if (!x_AssignParams(q_str, main_exec_query, param_result_query, 
                          bindGuard, indicator)) {
			SQLFreeStmt(m_Cmd, SQL_RESET_PARAMS);
			m_HasFailed = true;
			throw CDB_ClientEx(eDB_Error, 420003, "CODBC_RPCCmd::Send",
                           "cannot assign params");
		}
	}

   if(m_Recompile) main_exec_query+= " with recompile";

   q_str+= main_exec_query + ";\nselect STpROCrETURNsTATUS=@STpROCrETURNsTATUS";
   if(!param_result_query.empty()) {
       q_str+= ";\nselect " + param_result_query;
   }

    switch(SQLExecDirect(m_Cmd, (SQLCHAR*)q_str.c_str(), SQL_NTS)) {
    case SQL_SUCCESS:
        m_hasResults= true;
        break;

    case SQL_NO_DATA:
        m_hasResults= true; /* this is a bug in SQLExecDirect it returns SQL_NO_DATA if
                               status result is the only result of RPC */
        m_RowCount= 0;
        break;

    case SQL_ERROR:
        m_Reporter.ReportErrors();
        SQLFreeStmt(m_Cmd, SQL_RESET_PARAMS);
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 420001, "CODBC_RPCCmd::Send",
                           "SQLExecDirect failed");

    case SQL_SUCCESS_WITH_INFO:
        m_Reporter.ReportErrors();
        m_hasResults= true;
        break;

    case SQL_STILL_EXECUTING:
        m_Reporter.ReportErrors();
        SQLFreeStmt(m_Cmd, SQL_RESET_PARAMS);
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 420002, "CODBC_RPCCmd::Send",
                           "Some other query is executing on this connection");
        
    case SQL_INVALID_HANDLE:
        m_HasFailed= true;
        throw CDB_ClientEx(eDB_Fatal, 420004, "CODBC_RPCCmd::Send",
                           "The statement handler is invalid (memory corruption suspected)");
        
    default:
        m_Reporter.ReportErrors();
        SQLFreeStmt(m_Cmd, SQL_RESET_PARAMS);
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 420005, "CODBC_RPCCmd::Send",
                           "Unexpected error");
        
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
        switch(SQLFreeStmt(m_Cmd, SQL_CLOSE)) {
        case SQL_SUCCESS_WITH_INFO: m_Reporter.ReportErrors();
        case SQL_SUCCESS:           break;
        case SQL_ERROR:             m_Reporter.ReportErrors();
        default:                    return false;
        }
    }

    SQLFreeStmt(m_Cmd, SQL_RESET_PARAMS);
    m_Query.erase();
    return true;
}


bool CODBC_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CODBC_RPCCmd::Result()
{
    if (m_Res) {
        delete m_Res;
        m_Res = 0;
        m_hasResults= xCheck4MoreResults();
    }

    if (!m_WasSent) {
        throw CDB_ClientEx(eDB_Error, 420010, "CODBC_RPCCmd::Result",
                           "a command has to be sent first");
    }

    if(!m_hasResults) {
		m_WasSent= false;
        return 0;
    }

    SQLSMALLINT nof_cols= 0;
    char n_buff[64];

    while(m_hasResults) {
        switch(SQLNumResultCols(m_Cmd, &nof_cols)) {
        case SQL_SUCCESS_WITH_INFO:
            m_Reporter.ReportErrors();

        case SQL_SUCCESS:
            break;

        case SQL_ERROR:
            m_Reporter.ReportErrors();
            throw CDB_ClientEx(eDB_Error, 420011, "CODBC_RPCCmd::Result",
                               "SQLNumResultCols failed");
        default:
            throw CDB_ClientEx(eDB_Error, 420012, "CODBC_RPCCmd::Result",
                               "SQLNumResultCols failed (memory corruption suspected)");
        }

        if(nof_cols < 1) { // no data in this result set
			SQLINTEGER rc;
			switch(SQLRowCount(m_Cmd, &rc)) {
				case SQL_SUCCESS_WITH_INFO:
					m_Reporter.ReportErrors(); 
				case SQL_SUCCESS: break;
				case SQL_ERROR:
						m_Reporter.ReportErrors();
						throw CDB_ClientEx(eDB_Error, 420013, "CODBC_RPCCmd::Result",
                               "SQLRowCount failed");
				default:
					throw CDB_ClientEx(eDB_Error, 420014, "CODBC_RPCCmd::Result",
						"SQLRowCount failed (memory corruption suspected)");
			}

            m_RowCount = rc;
            m_hasResults= xCheck4MoreResults();
            continue;
        }

        if(nof_cols == 1) { // it could be a status result
            SQLSMALLINT l;
            switch(SQLColAttribute(m_Cmd, 1, SQL_DESC_LABEL, n_buff, 64, &l, 0)) {
            case SQL_SUCCESS_WITH_INFO: m_Reporter.ReportErrors();
            case SQL_SUCCESS:           break;
            case SQL_ERROR:
                m_Reporter.ReportErrors();
                throw CDB_ClientEx(eDB_Error, 420015, "CODBC_RPCCmd::Result",
                                   "SQLColAttribute failed");
            default:
                throw CDB_ClientEx(eDB_Error, 420016, "CODBC_RPCCmd::Result",
                                   "SQLColAttribute failed (memory corruption suspected)");
            }
            
            if(strcmp(n_buff, "STpROCrETURNsTATUS") == 0) {//this is a status result
                m_HasStatus= true;
                m_Res= new CODBC_StatusResult(m_Cmd, m_Reporter);
            }
        }
		if(!m_Res) {
			if(m_HasStatus) {
				m_HasStatus= false;
				m_Res= new CODBC_ParamResult(nof_cols, m_Cmd, m_Reporter);
			}	
			else {
				m_Res = new CODBC_RowResult(nof_cols, m_Cmd, m_Reporter, &m_RowCount);
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
        dbres= Result();
        if(dbres) {
            if(m_Connect->m_ResProc) {
                m_Connect->m_ResProc->ProcessResult(*dbres);
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


void CODBC_RPCCmd::SetRecompile(bool recompile)
{
    m_Recompile = recompile;
}


void CODBC_RPCCmd::Release()
{
    m_BR = 0;
    if (m_WasSent) {
        Cancel();
        m_WasSent = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CODBC_RPCCmd::~CODBC_RPCCmd()
{
    if (m_BR)
        *m_BR = 0;
    if (m_WasSent)
        Cancel();
    SQLFreeHandle(SQL_HANDLE_STMT, m_Cmd);
}


bool CODBC_RPCCmd::x_AssignParams(string& cmd, string& q_exec, string& q_select,
                                   CMemPot& bind_guard, SQLINTEGER* indicator)
{
    char p_nm[16], tbuf[32];
    // check if we do have a named parameters (first named - all named)
    bool param_named= !m_Params.GetParamName(0).empty();

    for (unsigned int n = 0; n < m_Params.NofParams(); n++) {
        if(m_Params.GetParamStatus(n) == 0) continue;
        const string& name  =  m_Params.GetParamName(n);
        CDB_Object&   param = *m_Params.GetParam(n);
        const char*   type;

        switch (param.GetType()) {
        case eDB_Int: {
            CDB_Int& val = dynamic_cast<CDB_Int&> (param);
            type = "int";
            indicator[n]= 4;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_SLONG, 
                             SQL_INTEGER, 4, 0, val.BindVal(), 4, indicator+n);
            break;
        }
        case eDB_SmallInt: {
            CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
            type = "smallint";
            indicator[n]= 2;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_SSHORT, 
                             SQL_SMALLINT, 2, 0, val.BindVal(), 2, indicator+n);
            break;
        }
        case eDB_TinyInt: {
            CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
            type = "tinyint";
            indicator[n]= 1;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_UTINYINT, 
                             SQL_TINYINT, 1, 0, val.BindVal(), 1, indicator+n);
            break;
        }
        case eDB_BigInt: {
            CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
            type = "numeric";
            indicator[n]= 8;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 18, 0, val.BindVal(), 18, indicator+n);
            
            break;
        }
        case eDB_Char: {
            CDB_Char& val = dynamic_cast<CDB_Char&> (param);
            type= "varchar(255)";
            indicator[n]= SQL_NTS;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_CHAR, 
                             SQL_VARCHAR, 255, 0, (void*)val.Value(), 256, indicator+n);
            break;
        }
        case eDB_VarChar: {
            CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
            type = "varchar(255)";
            indicator[n]= SQL_NTS;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_CHAR, 
                             SQL_VARCHAR, 255, 0, (void*)val.Value(), 256, indicator+n);
            break;
        }
        case eDB_LongChar: {
            CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
			sprintf(tbuf,"varchar(%d)", val.Size());
            type= tbuf;
            indicator[n]= SQL_NTS;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_CHAR, 
                             SQL_VARCHAR, val.Size(), 0, (void*)val.Value(), val.Size(), indicator+n);
            break;
        }
        case eDB_Binary: {
            CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
            type = "varbinary(255)";
            indicator[n]= val.Size();
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_BINARY, 
                             SQL_VARBINARY, 255, 0, (void*)val.Value(), 255, indicator+n);
            break;
        }
        case eDB_VarBinary: {
            CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
            type = "varbinary(255)";
            indicator[n]= val.Size();
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_BINARY, 
                             SQL_VARBINARY, 255, 0, (void*)val.Value(), 255, indicator+n);
            break;
        }
        case eDB_LongBinary: {
            CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
			sprintf(tbuf,"varbinary(%d)", val.Size());
            type= tbuf;
            indicator[n]= val.DataSize();
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_BINARY, 
                             SQL_VARBINARY, val.Size(), 0, (void*)val.Value(), val.Size(), indicator+n);
            break;
        }
        case eDB_Float: {
            CDB_Float& val = dynamic_cast<CDB_Float&> (param);
            type = "real";
            indicator[n]= 4;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_FLOAT, 
                             SQL_REAL, 4, 0, val.BindVal(), 4, indicator+n);
            break;
        }
        case eDB_Double: {
            CDB_Double& val = dynamic_cast<CDB_Double&> (param);
            type = "float";
            indicator[n]= 8;
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_DOUBLE, 
                             SQL_FLOAT, 8, 0, val.BindVal(), 8, indicator+n);
            break;
        }
        case eDB_SmallDateTime: {
            CDB_SmallDateTime& val = dynamic_cast<CDB_SmallDateTime&> (param);
            type = "smalldatetime";
            SQL_TIMESTAMP_STRUCT* ts= 0;
            if(!val.IsNULL()) {
                ts= (SQL_TIMESTAMP_STRUCT*)bind_guard.Alloc(sizeof(SQL_TIMESTAMP_STRUCT));
                const CTime& t= val.Value();
                ts->year= t.Year();
                ts->month= t.Month();
                ts->day= t.Day();
                ts->hour= t.Hour();
                ts->minute= t.Minute();
                ts->second= 0;
                ts->fraction= 0;
                indicator[n]= sizeof(SQL_TIMESTAMP_STRUCT);
            }
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, 
                             SQL_TYPE_TIMESTAMP, 16, 0, (void*)ts, sizeof(SQL_TIMESTAMP_STRUCT), 
                             indicator+n);
            break;
        }
        case eDB_DateTime: {
            CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
            type = "datetime";
            SQL_TIMESTAMP_STRUCT* ts= 0;
            if(!val.IsNULL()) {
                ts= (SQL_TIMESTAMP_STRUCT*)bind_guard.Alloc(sizeof(SQL_TIMESTAMP_STRUCT));
                const CTime& t= val.Value();
                ts->year= t.Year();
                ts->month= t.Month();
                ts->day= t.Day();
                ts->hour= t.Hour();
                ts->minute= t.Minute();
                ts->second= t.Second();
                ts->fraction= t.NanoSecond()/1000000;
				ts->fraction*= 1000000; /* MSSQL has a bug - it can not handle fraction of msecs */
                indicator[n]= sizeof(SQL_TIMESTAMP_STRUCT);
            }
            SQLBindParameter(m_Cmd, n+1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, 
                             SQL_TYPE_TIMESTAMP, 23, 3, ts, sizeof(SQL_TIMESTAMP_STRUCT), 
                             indicator+n);
            break;
        }
        default:
            return false;
        }

        q_exec+= n? ',':' ';

        if(!param_named) {
            sprintf(p_nm, "@pR%d", n);
            q_exec+= p_nm;
            cmd+= "declare ";
			cmd+= p_nm;
			cmd+= ' ';
			cmd+= type;
			cmd+= ";select ";
			cmd+= p_nm;
			cmd+= " = ?;";
        }
        else {
            q_exec+= name+'='+name;
            cmd += "declare " + name + ' ' + type + ";select " + name + " = ?;";
        }

        if(param.IsNULL()) {
            indicator[n]= SQL_NULL_DATA;
        }

        if ((m_Params.GetParamStatus(n) & CDB_Params::fOutput) != 0) {
            q_exec+= " output";
            const char* p_name= param_named? name.c_str() : p_nm;
            if(!q_select.empty()) q_select+= ',';
            q_select.append(p_name+1);
            q_select+= '=';
			q_select+= p_name;
        }
            
    }
    return true;
}

bool CODBC_RPCCmd::xCheck4MoreResults()
{
    switch(SQLMoreResults(m_Cmd)) {
    case SQL_SUCCESS_WITH_INFO: m_Reporter.ReportErrors();
    case SQL_SUCCESS:           return true;
    case SQL_NO_DATA:           return false;
    case SQL_ERROR:             
        m_Reporter.ReportErrors();
        throw CDB_ClientEx(eDB_Error, 420014, "CODBC_RPCCmd::xCheck4MoreResults",
                           "SQLMoreResults failed");
    default:
        throw CDB_ClientEx(eDB_Error, 420015, "CODBC_RPCCmd::xCheck4MoreResults",
                           "SQLMoreResults failed (memory corruption suspected)");
    }
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
