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
* Revision 1.12  2004/02/10 18:50:44  kholodov
* Modified: made Move() method const
*
* Revision 1.11  2003/06/16 19:43:58  kholodov
* Added: SQL command logging
*
* Revision 1.10  2002/12/16 18:56:50  kholodov
* Fixed: memory leak in CStatement object
*
* Revision 1.9  2002/12/06 23:00:21  kholodov
* Memory leak fix rolled back
*
* Revision 1.8  2002/12/05 17:37:23  kholodov
* Fixed: potential memory leak in CStatement::HasMoreResults() method
* Modified: getter and setter name for the internal CDB_Result pointer.
*
* Revision 1.7  2002/10/21 20:38:08  kholodov
* Added: GetParentConn() method to get the parent connection from IStatement,
* ICallableStatement and ICursor objects.
* Fixed: Minor fixes
*
* Revision 1.6  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.5  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.4  2002/09/09 20:48:57  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.3  2002/05/16 22:11:12  kholodov
* Improved: using minimum connections possible
*
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


BEGIN_NCBI_SCOPE

// implementation
CStatement::CStatement(CConnection* conn)
    : m_conn(conn), m_cmd(0), m_rs(0), m_rowCount(-1), m_failed(false),
      m_irs(0)
{
    SetIdent("CStatement");
}

CStatement::~CStatement()
{
    Close();
    Notify(CDbapiDeletedEvent(this));
    _TRACE(GetIdent() << " " << (void*)this << " deleted."); 
}

IConnection* CStatement::GetParentConn() 
{
    return m_conn;
}

void CStatement::SetCDB_Result(CDB_Result *rs) 
{ 
/*
    RequestedRsList::iterator i = m_requestedRsList.find(m_rs);
    if( i == m_requestedRsList.end() ) {
        _TRACE(GetIdent() << ": deleting unrequested CDB_Result " 
               << (void*)m_rs);
        delete m_rs;
    }
*/
    if( m_rs != 0 && (m_irs == 0 || (m_irs != 0 && m_irs->GetCDB_Result() != m_rs)) ) {
        _TRACE(GetIdent() << ": deleting unrequested CDB_Result " 
               << (void*)m_rs);
        
        delete m_rs;
    }

    m_rs = rs;
}

IResultSet* CStatement::GetResultSet()
{
    if( m_rs == 0 )
        return 0;

    if( m_irs == 0 || (m_irs != 0 && m_irs->GetCDB_Result() != m_rs) ) {
        _TRACE(GetIdent() << ": CDB_Result " << (void*)m_rs << " requested");
        CResultSet *ri = new CResultSet(m_conn, m_rs);
        ri->AddListener(this);
        AddListener(ri);
        //m_requestedRsList.insert(m_rs);
        m_irs = ri;
        return ri;
    }
    else
        return m_irs;
}

bool CStatement::HasMoreResults() 
{

    bool more = GetBaseCmd()->HasMoreResults();
    if( more ) {
        if( GetBaseCmd()->HasFailed() ) {
            SetFailed(true);
            return false;
        }
        SetCDB_Result(GetBaseCmd()->Result()); 
        if( m_rs == 0 )
            m_rowCount = GetBaseCmd()->RowCount();
    }
    return more;
}

void CStatement::SetParam(const CVariant& v, 
                          const string& name)
{

    ParamList::iterator i = m_params.find(name);
    if( i != m_params.end() ) {
        *((*i).second) = v;
    }
    else {
        m_params.insert(make_pair(name, new CVariant(v)));
    }


}
void CStatement::ClearParamList()
{
    ParamList::iterator i = m_params.begin();
    for( ; i != m_params.end(); ++i ) {
        delete (*i).second;
    }

    m_params.clear();
}

void CStatement::Execute(const string& sql)
{
    if( m_cmd != 0 ) {
        delete m_cmd;
        m_cmd = 0;
        m_rowCount = -1;
    }

    SetFailed(false);

    _TRACE("Sending SQL: " + sql);
    m_cmd = m_conn->GetCDB_Connection()->LangCmd(sql, m_params.size());

    ExecuteLast();
}

void CStatement::ExecuteUpdate(const string& sql)
{
    Execute(sql);
    while( HasMoreResults() );
}

void CStatement::ExecuteLast()
{
    ParamList::iterator i = m_params.begin();
    for( ; i != m_params.end(); ++i ) {
        GetLangCmd()->SetParam((*i).first, (*i).second->GetData());
    }
    m_cmd->Send();
}

bool CStatement::HasRows() 
{
    return m_rs != 0;
}

bool CStatement::Failed() 
{
    return m_failed;
}

int CStatement::GetRowCount() 
{
    return m_rowCount;
}

void CStatement::Close()
{
    delete m_cmd;
    m_cmd = 0;
    m_rowCount = -1;
    if( m_conn != 0 ) {
        if( m_conn->IsAux() ) {
            delete m_conn;
            m_conn = 0;
        }
        Notify(CDbapiClosedEvent(this));
    }

    ClearParamList();
}
  
void CStatement::Cancel()
{
    GetBaseCmd()->Cancel();
    m_rowCount = -1;
}

CDB_LangCmd* CStatement::GetLangCmd() 
{
    //if( m_cmd == 0 )
    //throw CDbException("CStatementImpl::GetLangCmd(): no cmd structure");
    return dynamic_cast<CDB_LangCmd*>(m_cmd);
}

void CStatement::Action(const CDbapiEvent& e) 
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName() 
           << "' received from " << e.GetSource()->GetIdent());

    if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(e.GetSource());
        if(dynamic_cast<CConnection*>(e.GetSource()) != 0 ) {
            _TRACE("Deleting " << GetIdent() << " " << (void*)this); 
            delete this;
        }
    }
}

END_NCBI_SCOPE
