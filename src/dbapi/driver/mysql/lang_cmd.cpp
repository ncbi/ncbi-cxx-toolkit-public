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
 * Author:  Anton Butanayev
 *
 * File Description
 *    Driver for MySQL server
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_Mysql_Cmds

#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), GetConnection(), &GetBindParams())
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE


CMySQL_LangCmd::CMySQL_LangCmd(CMySQL_Connection& conn,
                               const string&      lang_query) :
    impl::CBaseCmd(conn, lang_query),
    m_Connect(&conn),
    m_HasMoreResults(false),
    m_IsActive(false)
{
    if (conn.m_ActiveCmd) {
        conn.m_ActiveCmd->m_IsActive = false;
    }
    conn.m_ActiveCmd = this;
}


bool CMySQL_LangCmd::Send()
{
    if (mysql_real_query
        (&m_Connect->m_MySQL, GetQuery().data(), GetQuery().length()) != 0) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_real_query", 800003 );
    }
    GetBindParamsImpl().LockBinding();

    my_ulonglong nof_Rows = mysql_affected_rows(&this->m_Connect->m_MySQL);
    // There is not too much sence in comparing unsigned value with -1.
    // m_HasMoreResults = nof_Rows == -1 || nof_Rows > 0;
    m_HasMoreResults = nof_Rows > 0;
    return true;
}


bool CMySQL_LangCmd::Cancel()
{
    return false;
}


CDB_Result *CMySQL_LangCmd::Result()
{
    m_HasMoreResults = false;
    _ASSERT(m_Connect);
    return Create_Result(*new CMySQL_RowResult(*m_Connect));
}


bool CMySQL_LangCmd::HasMoreResults() const
{
    return m_HasMoreResults;
}


bool CMySQL_LangCmd::HasFailed() const
{
    if(mysql_errno(&m_Connect->m_MySQL) == 0) {
        return false;
    }
    return true;
}

CMySQL_LangCmd::~CMySQL_LangCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
    if (m_IsActive) {
        GetConnection().m_ActiveCmd = NULL;
    }
}

int CMySQL_LangCmd::LastInsertId() const
{
  return static_cast<int>( mysql_insert_id(&this->m_Connect->m_MySQL) );
}

int CMySQL_LangCmd::RowCount() const
{
  return static_cast<int>( mysql_affected_rows(&this->m_Connect->m_MySQL) );
}

string CMySQL_LangCmd::EscapeString(const char* str, unsigned long len)
{
    std::auto_ptr<char> buff( new char[len*2 + 1]);
    mysql_real_escape_string(&this->m_Connect->m_MySQL, buff.get(), str, len);
    return buff.get();
}


END_NCBI_SCOPE


