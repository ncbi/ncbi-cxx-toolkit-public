#ifndef _STMT_IMPL_HPP_
#define _STMT_IMPL_HPP_

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
* Revision 1.1  2002/01/30 14:51:23  kholodov
* User DBAPI implementation, first commit
*
* Revision 1.1  2001/11/30 16:30:03  kholodov
* Snapshot of the first draft of dbapi lib
*
*
*/

#include <dbapi/dbapi.hpp>
#include "active_obj.hpp"
#include <corelib/ncbistre.hpp>
#include "dbexception.hpp"


USING_NCBI_SCOPE;

class CStatement : public CActiveObject, 
		   public IEventListener,
		   public virtual IStatement
{
public:
  CStatement(class CConnection* conn);

  virtual ~CStatement();
  
  virtual IResultSet* GetResultSet();

  virtual bool HasMoreResults();

  virtual bool HasRows() {
    return m_rs != 0;
  }
  
    
  virtual void Execute(const string& sql);
  virtual void ExecuteUpdate(const string& sql);
  virtual void Cancel();
  virtual void Close();

  virtual void SetParam(const CVariant& v, 
			const string& name);

  virtual int GetRowCount() {
    return m_rowCount;
  }

  CDB_Result* GetResult() {
    if( m_rs == 0 )
      throw CDbapiException("CStatementImpl::GetResult(): no resultset returned");
    return m_rs;
  }

  CDB_LangCmd* GetLangCmd();

  class CConnection* GetConnection() {
    return m_conn;
  }

  // Interface IEventListener implementation
  virtual void Action(const CDbapiEvent& e);

protected:    
  void SetBaseCmd(I_BaseCmd *cmd) { m_cmd = cmd; }
  I_BaseCmd* GetBaseCmd() { return m_cmd; }

  void SetRs(CDB_Result *rs);

private:
  class CConnection* m_conn;
  I_BaseCmd *m_cmd;
  CDB_Result *m_rs;
  int m_rowCount;

};

//====================================================================

#endif // _STMT_IMPL_HPP_
