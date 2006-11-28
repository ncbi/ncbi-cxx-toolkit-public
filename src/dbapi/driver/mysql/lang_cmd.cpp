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
                               unsigned int       nof_params) :
    impl::CBaseCmd(lang_query, nof_params),
    m_Connect(conn),
    m_HasResults(false)
{
}


bool CMySQL_LangCmd::Send()
{
    if (mysql_real_query
        (&m_Connect->m_MySQL, GetQuery().c_str(), GetQuery().length()) != 0) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_real_query", 800003 );
    }

    my_ulonglong nof_Rows = mysql_affected_rows(&this->m_Connect->m_MySQL);
    // There is not too much sence in comparing unsigned value with -1.
    // m_HasResults = nof_Rows == -1 || nof_Rows > 0;
    m_HasResults = nof_Rows > 0;
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
            if(m_Connect->GetResultProcessor()) {
                m_Connect->GetResultProcessor()->ProcessResult(*dbres);
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

CMySQL_LangCmd::~CMySQL_LangCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.20  2006/11/28 20:08:07  ssikorsk
 * Replaced NCBI_CATCH_ALL(kEmptyStr) with NCBI_CATCH_ALL(NCBI_CURRENT_FUNCTION)
 *
 * Revision 1.19  2006/11/20 18:15:58  ssikorsk
 * Revamp code to use GetQuery() and GetParams() methods.
 *
 * Revision 1.18  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.17  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.16  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.15  2006/06/05 21:09:22  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.14  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.13  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.12  2006/02/16 19:37:42  ssikorsk
 * Get rid of compilation warnings
 *
 * Revision 1.11  2005/10/31 12:27:38  ssikorsk
 * Get rid of warnings.
 *
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
