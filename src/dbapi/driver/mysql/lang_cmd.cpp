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
 * File Description:  Driver for MySQL server
 *
 */

#include <corelib/ncbimtx.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>

BEGIN_NCBI_SCOPE

CMySQL_LangCmd::CMySQL_LangCmd(CMySQL_Connection* conn,
                               const string& lang_query,
                               unsigned int nof_params) :
m_Connect(conn), m_Query(lang_query), m_HasResults(false)
{
}

CMySQL_LangCmd::~CMySQL_LangCmd()
{}

bool CMySQL_LangCmd::More(const string& query_text)
{
  return false;
}

bool CMySQL_LangCmd::BindParam(const string& param_name, CDB_Object* param_ptr)
{
  return false;
}

bool CMySQL_LangCmd::SetParam(const string& param_name, CDB_Object* param_ptr)
{
  return false;
}

bool CMySQL_LangCmd::Send()
{
  if(0 != mysql_real_query(&m_Connect->m_MySQL, m_Query.c_str(), m_Query.length()))
    throw CDB_ClientEx(eDB_Warning, 800003,
                       "CMySQL_LangCmd::Send",
                       "Failed: mysql_real_query");
  m_HasResults = true;
  return false;
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

bool CMySQL_LangCmd::HasFailed() const
{
  return false;
}

int CMySQL_LangCmd::RowCount() const
{
  return false;
}

void CMySQL_LangCmd::Release()
{
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/08/28 17:18:20  butanaev
 * Improved error handling, demo app.
 *
 * Revision 1.1  2002/08/13 20:23:14  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
