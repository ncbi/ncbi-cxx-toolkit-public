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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*   
* File Description:  Statement implementation
*
*
* $Log$
* Revision 1.2  2002/02/08 17:38:26  kholodov
* Moved listener registration to parent objects
*
* Revision 1.1  2002/01/30 14:51:22  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include "conn_impl.hpp"
#include "stmt_impl.hpp"
#include "rs_impl.hpp"
#include <dbapi/driver/public.hpp>


// implementation
CStatement::CStatement(CConnection* conn)
  : m_conn(conn), m_cmd(0), m_rs(0), m_rowCount(-1)
{
}

CStatement::~CStatement()
{
  Close();
  Notify(CDbapiDeletedEvent(this));
}

void CStatement::SetRs(CDB_Result *rs) 
{ 
  delete m_rs;
  m_rs = rs;
}

bool CStatement::HasMoreResults() 
{
  CheckValid();

  bool more = GetBaseCmd()->HasMoreResults();
  if( more ) {
    //delete m_rs;
    m_rs = GetBaseCmd()->Result(); 
    if( m_rs == 0 )
      m_rowCount = GetBaseCmd()->RowCount();
  }
  return more;
}

void CStatement::SetParam(const CVariant& v, 
			  const string& name)
{
  CheckValid();

  GetLangCmd()->SetParam(name.empty() ? 0 : name.c_str(),
			v.GetData());
}

void CStatement::Execute(const string& sql)
{
  CheckValid();
  if( m_cmd != 0 ) {
    delete m_cmd;
    m_cmd = 0;
    m_rowCount = -1;
  }

  SetBaseCmd(m_conn->GetConnection()->LangCmd(sql.c_str()));
  GetBaseCmd()->Send();
}

void CStatement::ExecuteUpdate(const string& sql)
{
  Execute(sql);
  while( HasMoreResults() );
}

void CStatement::Close()
{
  delete m_cmd;
  m_cmd = 0;
  m_rowCount = -1;
  SetValid(false);
}
  
void CStatement::Cancel()
{
  CheckValid();
  GetBaseCmd()->Cancel();
  m_rowCount = -1;
}

IResultSet* CStatement::GetResultSet()
{
  CheckValid();
  if( m_rs != 0 ) {
    CResultSet *ri = new CResultSet(m_conn, m_rs);
    ri->AddListener(this);
    AddListener(ri);
    return ri;
  }
  else
    return 0;
}

CDB_LangCmd* CStatement::GetLangCmd() 
{
  //if( m_cmd == 0 )
  //throw CDbException("CStatementImpl::GetLangCmd(): no cmd structure");
  return dynamic_cast<CDB_LangCmd*>(m_cmd);
}

void CStatement::Action(const CDbapiEvent& e) 
{
  if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
    RemoveListener(dynamic_cast<IEventListener*>(e.GetSource()));
    if(dynamic_cast<CConnection*>(e.GetSource()) != 0 ) {
      delete this;
      //SetValid(false);
    }
  }
}

