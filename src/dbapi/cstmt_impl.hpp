#ifndef _CSTMT_IMPL_HPP_
#define _CSTMT_IMPL_HPP_

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
*/

#include "stmt_impl.hpp"
#include <map>

BEGIN_NCBI_SCOPE

class CCallableStatement : public CStatement,
                           public ICallableStatement
{
public:
    CCallableStatement(const string& proc, CConnection* conn);

    virtual ~CCallableStatement();

    virtual bool HasMoreResults();

    virtual int GetReturnStatus();
    virtual void SetParam(const CVariant& v, 
                          const CDBParamVariant& param);

    virtual void SetOutputParam(const CVariant& v, 
            const CDBParamVariant& param);

    virtual void Execute();
    virtual void ExecuteUpdate();
    virtual void Close();

protected:

    CDB_RPCCmd* GetRpcCmd();

    virtual void SendSql(const string& /*sql*/) {}
    virtual void Execute(const string& /*sql*/) {}
    virtual void ExecuteUpdate(const string& /*sql*/) {}
    virtual IResultSet* ExecuteQuery(const string& /*sql*/) { return 0; }

private:
    int  m_status;
	bool m_StatusIsAvailable;
};

//====================================================================
END_NCBI_SCOPE

#endif // _CSTMT_IMPL_HPP_
