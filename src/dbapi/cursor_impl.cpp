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
* File Description:  Base class for database access
*
*
* $Log$
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
* Revision 1.1  2001/11/30 16:30:05  kholodov
* Snapshot of the first draft of dbapi lib
*
*
*
*/

#include "conn_impl.hpp"
#include "cursor_impl.hpp"
#include "rs_impl.hpp"
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

// implementation
CCursor::CCursor(const string& name,
		 const string& sql,
		 int nofArgs,
		 int batchSize,
		 CConnection* conn)
  : m_nofArgs(nofArgs), m_cmd(0), m_conn(conn)
{
  AddListener(conn);
  m_cmd = m_conn->GetConnection()->Cursor(name.c_str(), sql.c_str(),
					  nofArgs, batchSize);
}

CCursor::~CCursor()
{
  Close();
  Notify(CDbapiDeletedEvent(this));
  delete m_cmd;
}

  
void CCursor::SetParam(const CVariant& v, 
		       const string& name)
{

  CheckValid();

  Parameters::iterator cur = m_params.find(name);
  if( cur != m_params.end() ) {
    *((*cur).second) = v;
  }
  else {
    pair<Parameters::iterator, bool> 
      ins = m_params.insert(make_pair(name, new CVariant(v)));

    cur = ins.first;
  }


  GetCursorCmd()->BindParam((*cur).first.c_str(),
			    (*cur).second->GetData());
}
		
		
IResultSet* CCursor::Open()
{
  CheckValid();
  CResultSet *ri = new CResultSet(m_conn, GetCursorCmd()->Open());
  ri->AddListener(this);
  AddListener(ri);
  return ri;
}

void CCursor::Update(const string& table, const string& updateSql) 
{
  CheckValid();
  GetCursorCmd()->Update(table.c_str(), updateSql.c_str());
}

void CCursor::Delete(const string& table)
{
  CheckValid();
  GetCursorCmd()->Delete(table.c_str());
}

void CCursor::Close()
{

  Parameters::iterator i = m_params.begin();
  for( ; i != m_params.end(); ++i ) {
    delete (*i).second;
  }

  m_params.clear();

}

void CCursor::Action(const CDbapiEvent& e) 
{
  if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
    RemoveListener(dynamic_cast<IEventListener*>(e.GetSource()));
    if(dynamic_cast<CConnection*>(e.GetSource()) != 0 ) {
      delete this;
      //SetValid(false);
    }
  }
}

END_NCBI_SCOPE
