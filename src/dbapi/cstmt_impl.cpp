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
* File Description:  Callable statement implementation
*
*
* $Log$
* Revision 1.3  2002/02/08 21:29:54  kholodov
* SetDataBase() restored, connection cloning algorithm changed
*
* Revision 1.2  2002/02/05 17:24:02  kholodov
* Put into NCBI scope
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include "conn_impl.hpp"
#include "cstmt_impl.hpp"
#include "rs_impl.hpp"
#include "basetmpl.hpp"
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

// implementation
CCallableStatement::CCallableStatement(const string& proc,
				       int nofArgs,
				       CConnection* conn)
  : CStatement(conn), m_status(0), m_nofParams(nofArgs)
{
  SetBaseCmd(conn->GetConnection()->RPC(proc.c_str(), nofArgs));
}

CCallableStatement::~CCallableStatement()
{
  Close();
}

CDB_RPCCmd* CCallableStatement::GetRpcCmd() 
{
  return dynamic_cast<CDB_RPCCmd*>(GetBaseCmd());
}

bool CCallableStatement::HasMoreResults()
{
  CheckValid();

  bool more = CStatement::HasMoreResults();
  if( more 
      && GetResult() != 0 
      && GetResult()->ResultType() == eDB_StatusResult ) {

    CDB_Int *res = 0;
    while( GetResult()->Fetch() ) {
      res = dynamic_cast<CDB_Int*>(GetResult()->GetItem());
    }
    if( res != 0 )
      m_status = res->Value();
    delete res;

    more = CStatement::HasMoreResults();
  }
  return more;
}
  
void CCallableStatement::SetParam(const CVariant& v, 
				  const string& name)
{
  CheckValid();

  GetRpcCmd()->SetParam(name.empty() ? 0 : name.c_str(),
			v.GetData(), false);
}
		
void CCallableStatement::SetOutputParam(const CVariant& v, 
					const string& name)
{
  GetRpcCmd()->SetParam(name.c_str(), v.GetData(), true);
}
		
		
void CCallableStatement::Execute()
{
  CheckValid();
  GetRpcCmd()->Send();
}

int CCallableStatement::GetReturnStatus()
{
  CheckValid();
  return m_status;
}

END_NCBI_SCOPE
