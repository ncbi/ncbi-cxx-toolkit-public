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
*/

#include <ncbi_pch.hpp>
#include "conn_impl.hpp"
#include "cstmt_impl.hpp"
#include "rs_impl.hpp"
#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_ObjImpls

BEGIN_NCBI_SCOPE

// implementation
CCallableStatement::CCallableStatement(const string& proc,
                       CConnection* conn)
: CStatement(conn)
, m_status(0)
, m_StatusIsAvailable(false)
{
    SetBaseCmd(conn->GetCDB_Connection()->RPC(proc.c_str()));
    SetIdent("CCallableStatement");
}

CCallableStatement::~CCallableStatement()
{
    try {
        Notify(CDbapiClosedEvent(this));
    }
    NCBI_CATCH_ALL_X( 2, kEmptyStr )
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
			m_StatusIsAvailable = true;
            _TRACE("CCallableStatement::HasMoreResults(): Return status "
                   << m_status );
            delete res;
        }

        more = CStatement::HasMoreResults();
    }
    return more;
}

void CCallableStatement::SetParam(const CVariant& v,
                  const CDBParamVariant& param)
{
    if (param.IsPositional()) {
        // Decrement position by ONE.
        GetRpcCmd()->GetBindParams().Set(param.GetPosition() - 1, v.GetData());
    } else {
        GetRpcCmd()->GetBindParams().Set(param, v.GetData());
    }
}

void CCallableStatement::SetOutputParam(const CVariant& v,
                    const CDBParamVariant& param)
{
    if (param.IsPositional()) {
        // Decrement position by ONE.
        GetRpcCmd()->GetBindParams().Set(param.GetPosition() - 1, v.GetData(), true);
    } else {
        GetRpcCmd()->GetBindParams().Set(param, v.GetData(), true);
    }
}


void CCallableStatement::Execute()
{
  SetFailed(false);

  // Reset status value ...
  m_status = 0;
  m_StatusIsAvailable = false;

  _TRACE("Executing stored procedure: " + GetRpcCmd()->GetProcName());
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
  CHECK_NCBI_DBAPI(!m_StatusIsAvailable, "Return status is not available yet.");

  return m_status;
}

void CCallableStatement::Close()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
}


END_NCBI_SCOPE
