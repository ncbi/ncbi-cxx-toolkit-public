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


BEGIN_NCBI_SCOPE


CMySQL_LangCmd::CMySQL_LangCmd(CMySQL_Connection* conn,
                               const string&      lang_query,
                               unsigned int       /*nof_params*/)
    : m_Connect(conn),
      m_Query(lang_query),
      m_HasResults(false)
{
}


CMySQL_LangCmd::~CMySQL_LangCmd()
{
}


bool CMySQL_LangCmd::More(const string& query_text)
{
    m_Query += query_text;
    return true;
}


bool CMySQL_LangCmd::BindParam(const string& /*param_name*/,
                               CDB_Object*   /*param_ptr*/)
{
    return false;
}


bool CMySQL_LangCmd::SetParam(const string& /*param_name*/,
                              CDB_Object*   /*param_ptr*/)
{
    return false;
}


bool CMySQL_LangCmd::Send()
{
    if (mysql_real_query
        (&m_Connect->m_MySQL, m_Query.c_str(), m_Query.length()) != 0) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_real_query", 800003 );
    }
    
    int nof_Rows = mysql_affected_rows(&this->m_Connect->m_MySQL);
    m_HasResults = nof_Rows == -1 || nof_Rows > 0;
    return true;
}


bool CMySQL_LangCmd::WasSent() const
{
    return false;
}


bool CMySQL_LangCmd::Cancel()
{
    return false;
}


bool CMySQL_LangCmd::WasCanceled() const
{
    return false;
}


CDB_Result *CMySQL_LangCmd::Result()
{
    m_HasResults = false;
    return Create_Result(*new CMySQL_RowResult(m_Connect));
}


bool CMySQL_LangCmd::HasMoreResults() const
{
    return m_HasResults;
}

void CMySQL_LangCmd::DumpResults()
{
    CDB_Result* dbres;
    while(m_HasResults) {
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


bool CMySQL_LangCmd::HasFailed() const
{
    if(mysql_errno(&m_Connect->m_MySQL) == 0) 
      return false;
    return true;
}

void CMySQL_LangCmd::Release()
{
}

int CMySQL_LangCmd::LastInsertId() const
{
  return mysql_insert_id(&this->m_Connect->m_MySQL);
}

int CMySQL_LangCmd::RowCount() const
{
  return mysql_affected_rows(&this->m_Connect->m_MySQL);
}

string CMySQL_LangCmd::EscapeString(const char* str, unsigned long len)
{
    std::auto_ptr<char> buff( new char[len*2 + 1]);
    mysql_real_escape_string(&this->m_Connect->m_MySQL, buff.get(), str, len);
    return buff.get();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.9  2004/05/17 21:15:34  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2004/03/24 19:46:53  vysokolo
 * addaed support of blob
 *
 * Revision 1.7  2003/06/06 16:02:53  butanaev
 * Fixed return value of Send - it should return true.
 *
 * Revision 1.6  2003/06/05 16:02:48  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.5  2003/05/29 21:42:25  butanaev
 * Fixed Send, HasFailed functions.
 *
 * Revision 1.4  2003/05/29 21:25:47  butanaev
 * Added function to return last insert id, fixed RowCount, Send,
 * added call to RowCount in sample app.
 *
 * Revision 1.3  2003/01/06 20:30:26  vakatov
 * Get rid of some redundant header(s).
 * Formally reformatted to closer meet C++ Toolkit/DBAPI style.
 *
 * Revision 1.2  2002/08/28 17:18:20  butanaev
 * Improved error handling, demo app.
 *
 * Revision 1.1  2002/08/13 20:23:14  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
