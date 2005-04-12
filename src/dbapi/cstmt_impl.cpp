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
* Revision 1.17  2005/04/12 18:12:10  ssikorsk
* Added SetAutoClearInParams and IsAutoClearInParams functions to IStatement
*
* Revision 1.16  2004/09/22 14:27:57  kholodov
* Modified: reference to unused basetmpl.hpp removed
*
* Revision 1.15  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.14  2004/04/26 14:16:56  kholodov
* Modified: recreate the command objects each time the Get...() is called
*
* Revision 1.13  2004/04/12 14:25:33  kholodov
* Modified: resultset caching scheme, fixed single connection handling
*
* Revision 1.12  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.11  2004/03/12 16:27:09  sponomar
* correct nested querys
*
* Revision 1.9  2004/02/26 18:52:34  kholodov
* Added: more trace messages
*
* Revision 1.8  2002/12/05 17:37:23  kholodov
* Fixed: potential memory leak in CStatement::HasMoreResults() method
* Modified: getter and setter name for the internal CDB_Result pointer.
*
* Revision 1.7  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.6  2002/09/09 20:48:57  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.5  2002/05/16 22:11:11  kholodov
* Improved: using minimum connections possible
*
* Revision 1.4  2002/04/05 19:33:08  kholodov
* Added: ExecuteUpdate() to skip all resultsets returned (if any)
*
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

#include <ncbi_pch.hpp>
#include "conn_impl.hpp"
#include "cstmt_impl.hpp"
#include "rs_impl.hpp"
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

// implementation
CCallableStatement::CCallableStatement(const string& proc,
                       int nofArgs,
                       CConnection* conn)
  : CStatement(conn), m_status(0), m_nofParams(nofArgs)
{
    SetIdent("CCallableStatement");
  SetBaseCmd(conn->GetCDB_Connection()->RPC(proc.c_str(), nofArgs));
}

CCallableStatement::~CCallableStatement()
{
    Notify(CDbapiClosedEvent(this));
}

CDB_RPCCmd* CCallableStatement::GetRpcCmd() 
{
  return (CDB_RPCCmd*)GetBaseCmd();
}

bool CCallableStatement::HasMoreResults()
{
    _TRACE("CCallableStatement::HasMoreResults(): Calling parent method");
    bool more = CStatement::HasMoreResults();
    if( more 
        && GetCDB_Result() != 0 
        && GetCDB_Result()->ResultType() == eDB_StatusResult ) {
      
        _TRACE("CCallableStatement::HasMoreResults(): Status result received");
        CDB_Int *res = 0;
        while( GetCDB_Result()->Fetch() ) {
            res = dynamic_cast<CDB_Int*>(GetCDB_Result()->GetItem());
        }
        if( res != 0 ) {
            m_status = res->Value();
            _TRACE("CCallableStatement::HasMoreResults(): Return status " 
                   << m_status );
            delete res;
        }

        more = CStatement::HasMoreResults();
    }
    return more;
}
  
void CCallableStatement::SetParam(const CVariant& v, 
                  const string& name)
{

  GetRpcCmd()->SetParam(name, v.GetData(), false);
}
        
void CCallableStatement::SetOutputParam(const CVariant& v, 
                    const string& name)
{
  GetRpcCmd()->SetParam(name, v.GetData(), true);
}
        
        
void CCallableStatement::Execute()
{
  SetFailed(false);
  GetRpcCmd()->Send();

  if ( IsAutoClearInParams() ) {
      // Implicitely clear all parameters.
      ClearParamList();
  }
}

void CCallableStatement::ExecuteUpdate()
{
  Execute();
  while( HasMoreResults() );
}

int CCallableStatement::GetReturnStatus()
{
  return m_status;
}

void CCallableStatement::Close()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
}    

END_NCBI_SCOPE
