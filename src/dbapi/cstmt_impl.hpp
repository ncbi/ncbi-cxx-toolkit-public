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
* $Log$
* Revision 1.7  2004/04/26 14:16:56  kholodov
* Modified: recreate the command objects each time the Get...() is called
*
* Revision 1.6  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.5  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.4  2002/04/05 19:33:08  kholodov
* Added: ExecuteUpdate() to skip all resultsets returned (if any)
*
* Revision 1.3  2002/02/08 21:29:55  kholodov
* SetDataBase() restored, connection cloning algorithm changed
*
* Revision 1.2  2002/02/05 17:24:02  kholodov
* Put into NCBI scope
*
* Revision 1.1  2002/01/30 14:51:22  kholodov
* User DBAPI implementation, first commit
*
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
    CCallableStatement(const string& proc,
                       int nofArgs,
                       CConnection* conn);

    virtual ~CCallableStatement();

    virtual bool HasMoreResults();

    virtual int GetReturnStatus();
    virtual void SetParam(const CVariant& v, 
                          const string& name);

    virtual void SetOutputParam(const CVariant& v, const string& name);

    virtual void Execute();
    virtual void ExecuteUpdate();
    virtual void Close();

protected:

    CDB_RPCCmd* GetRpcCmd();

    virtual void Execute(const string& /*sql*/) {}
    virtual void ExecuteUpdate(const string& /*sql*/) {}
    virtual IResultSet* ExecuteQuery(const string& /*sql*/) { return 0; }

private:
    int m_status;
    int m_nofParams;
    //typedef map<string, CVariant> ParamList;
    //ParamList m_paramList;
  
};

//====================================================================
END_NCBI_SCOPE

#endif // _CSTMT_IMPL_HPP_
