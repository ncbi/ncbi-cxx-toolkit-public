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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *    Driver for SQLite v3 embedded database server
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/sqlite3/interfaces.hpp>
#include <dbapi/error_codes.hpp>

#include "sqlite3_utils.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_SQLt3_Cmds


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
CSL3_LangCmd::CSL3_LangCmd(CSL3_Connection& conn,
                           const string&    lang_query,
                           unsigned int     nof_params)
: impl::CBaseCmd(conn, lang_query, nof_params)
, m_Connect(&conn)
, m_HasMoreResults(false)
, m_RowCount(0)
, m_SQLite3stmt(NULL)
, m_Res(NULL)
, m_RC(SQLITE_ERROR)
{
}


bool CSL3_LangCmd::Send()
{
    const char* sql_tail = NULL;

    Cancel();

    // m_HasFailed = false;

    int rc = Check(sqlite3_prepare(GetConnection().GetSQLite3(),
                                   GetQuery().c_str(),
                                   GetQuery().size(),
                                   &m_SQLite3stmt,
                                   &sql_tail
                                   ));
    CHECK_DRIVER_ERROR(rc != SQLITE_OK,
                       "Failed to prepare a statement." + GetDbgInfo(),
                       100000);

    if (!x_AssignParams()) {
        DATABASE_DRIVER_ERROR( "Cannot assign params." + GetDbgInfo(), 220003 );
    }

    m_RC = sqlite3_step(m_SQLite3stmt);
    switch (m_RC) {
    case SQLITE_DONE:
        m_RowCount = sqlite3_total_changes(m_Connect->GetSQLite3());
        m_HasMoreResults = false;
        break;
    case SQLITE_ROW:
        m_RowCount = 0;
        m_HasMoreResults = true;
        break;
    case SQLITE_BUSY:
    case SQLITE_ERROR:
    case SQLITE_MISUSE:
        m_HasMoreResults = false;
        CHECK_DRIVER_ERROR(rc != SQLITE_OK,
                           "Failed to execute a statement." + GetDbgInfo(),
                           100000);
        break;
    default:
        DATABASE_DRIVER_ERROR("Invalid return code." + GetDbgInfo(), 100000);
    }

    SetWasSent();

    return true;
}


bool CSL3_LangCmd::Cancel()
{
    if (WasSent()) {
        // sqlite3_interrupt(m_Connect->m_SQLite3);
        SetWasSent(false);
        return sqlite3_finalize(m_SQLite3stmt) == SQLITE_OK;
    }

    return true;
}


CDB_Result *CSL3_LangCmd::Result()
{
    delete m_Res;
    m_Res = new CSL3_RowResult(this, (m_RC == SQLITE_ROW));

    m_HasMoreResults = false;

    return Create_Result(static_cast<impl::CResult&>(*m_Res));
}


bool CSL3_LangCmd::HasMoreResults() const
{
    return m_HasMoreResults;
}


bool CSL3_LangCmd::HasFailed() const
{
    int rc = sqlite3_errcode(m_Connect->GetSQLite3());

    return (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE);
}


CSL3_LangCmd::~CSL3_LangCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}

long long int CSL3_LangCmd::LastInsertId() const
{
    return sqlite3_last_insert_rowid(this->m_Connect->GetSQLite3());
}

int CSL3_LangCmd::RowCount() const
{
    return m_RowCount;
}

bool CSL3_LangCmd::x_AssignParams(void)
{
    for (unsigned int i = 0;  i < GetParams().NofParams();  ++i) {
        if(GetParams().GetParamStatus(i) == 0) continue;

        CDB_Object& param = *GetParams().GetParam(i);
        if ( !AssignCmdParam(param, i + 1) ) {
            return false;
        }
    }

    return true;
}


bool CSL3_LangCmd::AssignCmdParam(CDB_Object&   param,
                                  int           param_num
                                  )
{
    if (param.IsNULL()) {
        Check(sqlite3_bind_null(m_SQLite3stmt, param_num));
        return true;
    }

    switch ( param.GetType() ) {
    case eDB_Int:
        Check(sqlite3_bind_int(m_SQLite3stmt,
                               param_num,
                               dynamic_cast<CDB_Int&>(param).Value()));
        break;
    case eDB_SmallInt:
        Check(sqlite3_bind_int(m_SQLite3stmt,
                               param_num,
                               dynamic_cast<CDB_SmallInt&>(param).Value()));
        break;
    case eDB_TinyInt:
        Check(sqlite3_bind_int(m_SQLite3stmt,
                               param_num,
                               dynamic_cast<CDB_TinyInt&>(param).Value()));
        break;
    case eDB_Bit:
        Check(sqlite3_bind_int(m_SQLite3stmt,
                               param_num,
                               dynamic_cast<CDB_Bit&>(param).Value()));
        break;
    case eDB_BigInt:
        Check(sqlite3_bind_int64(m_SQLite3stmt,
                                 param_num,
                                 dynamic_cast<CDB_BigInt&>(param).Value()));
        break;
    case eDB_Numeric: {
        CDB_Numeric& par = dynamic_cast<CDB_Numeric&> (param);
        string value = par.Value();

        Check(sqlite3_bind_text(m_SQLite3stmt,
                                param_num,
                                value.c_str(),
                                value.size(),
                                SQLITE_TRANSIENT));
        break;
    }
    case eDB_Char: {
        CDB_Char& par = dynamic_cast<CDB_Char&> (param);
        Check(sqlite3_bind_text(m_SQLite3stmt,
                                param_num,
                                par.Value(),
                                par.Size(),
                                SQLITE_STATIC));
        break;
    }
    case eDB_LongChar: {
        CDB_LongChar& par = dynamic_cast<CDB_LongChar&> (param);
        Check(sqlite3_bind_text(m_SQLite3stmt,
                                param_num,
                                par.Value(),
                                par.Size(),
                                SQLITE_STATIC));
        break;
    }
    case eDB_VarChar: {
        CDB_VarChar& par = dynamic_cast<CDB_VarChar&> (param);
        Check(sqlite3_bind_text(m_SQLite3stmt,
                                param_num,
                                par.Value(),
                                par.Size(),
                                SQLITE_STATIC));
        break;
    }
    case eDB_Text: {
        CDB_Text& par = dynamic_cast<CDB_Text&> (param);
        string value;

        value.resize(par.Size());
        par.MoveTo(0);
        par.Read((void*)value.c_str(), value.size());

        Check(sqlite3_bind_text(m_SQLite3stmt,
                                param_num,
                                value.c_str(),
                                value.size(),
                                SQLITE_TRANSIENT));
        break;
    }
    case eDB_Binary: {
        CDB_Binary& par = dynamic_cast<CDB_Binary&> (param);
        Check(sqlite3_bind_blob(m_SQLite3stmt,
                                param_num,
                                par.Value(),
                                par.Size(),
                                SQLITE_STATIC));
        break;
    }
    case eDB_LongBinary: {
        CDB_LongBinary& par = dynamic_cast<CDB_LongBinary&> (param);
        Check(sqlite3_bind_blob(m_SQLite3stmt,
                                param_num,
                                par.Value(),
                                par.Size(),
                                SQLITE_STATIC));
        break;
    }
    case eDB_VarBinary: {
        CDB_VarBinary& par = dynamic_cast<CDB_VarBinary&> (param);
        Check(sqlite3_bind_blob(m_SQLite3stmt,
                                param_num,
                                par.Value(),
                                par.Size(),
                                SQLITE_STATIC));
        break;
    }
    case eDB_Image: {
        CDB_Image& par = dynamic_cast<CDB_Image&> (param);
        string value;

        value.resize(par.Size());
        par.MoveTo(0);
        par.Read((void*)value.c_str(), value.size());

        Check(sqlite3_bind_text(m_SQLite3stmt,
                                param_num,
                                value.c_str(),
                                value.size(),
                                SQLITE_TRANSIENT));
        break;
    }
    case eDB_Float:
        Check(sqlite3_bind_double(m_SQLite3stmt,
                                  param_num,
                                  dynamic_cast<CDB_Float&>(param).Value()));
        break;
    case eDB_Double:
        Check(sqlite3_bind_double(m_SQLite3stmt,
                                  param_num,
                                  dynamic_cast<CDB_Double&>(param).Value()));
        break;
    case eDB_SmallDateTime:
        Check(sqlite3_bind_int64(m_SQLite3stmt,
                                 param_num,
                                 dynamic_cast<CDB_SmallDateTime&>(param).Value().GetTimeT()));
        break;
    case eDB_DateTime:
        Check(sqlite3_bind_int64(m_SQLite3stmt,
                                 param_num,
                                 dynamic_cast<CDB_DateTime&>(param).Value().GetTimeT()));
        break;
    default:
        return false;
    }

    return true;
}



END_NCBI_SCOPE


