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
*
*
*/

#include <ncbi_pch.hpp>
#include "conn_impl.hpp"
#include "stmt_impl.hpp"
#include "rs_impl.hpp"
#include "rw_impl.hpp"
#include <dbapi/driver/public.hpp>


BEGIN_NCBI_SCOPE

// implementation
CStatement::CStatement(CConnection* conn)
    : m_conn(conn)
    , m_cmd(0)
    , m_rowCount(-1)
    , m_failed(false)
    , m_irs(0)
    , m_wr(0)
    , m_AutoClearInParams(true)
{
    SetIdent("CStatement");
}

CStatement::~CStatement()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
    Notify(CDbapiDeletedEvent(this));
    _TRACE(GetIdent() << " " << (void*)this << " deleted."); 
}

IConnection* CStatement::GetParentConn() 
{
    return m_conn;
}

void CStatement::CacheResultSet(CDB_Result *rs) 
{ 
    if( m_irs != 0 ) {
        _TRACE("CStatement::CacheResultSet(): Invalidating cached CResultSet " << (void*)m_irs);
        m_irs->Invalidate();
    }
    if( rs != 0 ) {
        m_irs = new CResultSet(m_conn, rs);
        m_irs->AddListener(this);
        AddListener(m_irs);
        _TRACE("CStatement::CacheResultSet(): Created new CResultSet " << (void*)m_irs
            << " with CDB_Result " << (void*)rs);
    }
    else
        m_irs = 0;
}

IResultSet* CStatement::GetResultSet()
{
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
        //Notify(CDbapiNewResultEvent(this));
        CDB_Result *rs = GetBaseCmd()->Result();
        CacheResultSet(rs); 
#if 0
        if( rs == 0 ) {
            m_rowCount = GetBaseCmd()->RowCount();
        }
#endif
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

    if ( IsAutoClearInParams() ) {
        // Implicitely clear all parameters.
        ClearParamList();
    }
}

IResultSet* CStatement::ExecuteQuery(const string& sql)
{
    Execute(sql);
    while( HasMoreResults() ) {
        if( HasRows() ) {
            return GetResultSet();
        }
    }
    return 0;
}
void CStatement::ExecuteUpdate(const string& sql)
{
    Execute(sql);
    //while( HasMoreResults() );
    GetBaseCmd()->DumpResults();
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
    return m_irs != 0;
}

IWriter* CStatement::GetBlobWriter(CDB_ITDescriptor &d, size_t blob_size, EAllowLog log_it)
{
    delete m_wr;
    m_wr = new CBlobWriter(GetConnection()->GetCDB_Connection(), d, blob_size, log_it == eEnableLog);
    return m_wr;
}

CDB_Result* CStatement::GetCDB_Result() {
    return m_irs == 0 ? 0 : m_irs->GetCDB_Result();
}

bool CStatement::Failed() 
{
    return m_failed;
}

int CStatement::GetRowCount() 
{
    int v;
    if( (v = GetBaseCmd()->RowCount()) >= 0 ) {
        m_rowCount = v;
    }
    return m_rowCount;
}

void CStatement::Close()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
}

void CStatement::FreeResources() 
{
    delete m_cmd;
    m_cmd = 0;
    m_rowCount = -1;
    if( m_conn != 0 && m_conn->IsAux() ) {
        delete m_conn;
        m_conn = 0;
        Notify(CDbapiAuxDeletedEvent(this));
    }

    delete m_wr;
    m_wr = 0;

    ClearParamList();
}
  
void CStatement::PurgeResults()
{
    if( GetBaseCmd() != 0 )
        GetBaseCmd()->DumpResults();
}

void CStatement::Cancel()
{
    if( GetBaseCmd() != 0 )
        GetBaseCmd()->Cancel();
    m_rowCount = -1;
}

CDB_LangCmd* CStatement::GetLangCmd() 
{
    //if( m_cmd == 0 )
    //throw CDbException("CStatementImpl::GetLangCmd(): no cmd structure");
    return (CDB_LangCmd*)m_cmd;
}

void CStatement::Action(const CDbapiEvent& e) 
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName() 
           << "' received from " << e.GetSource()->GetIdent());

    CResultSet *rs;

    if(dynamic_cast<const CDbapiFetchCompletedEvent*>(&e) != 0 ) {
        if( m_irs != 0 && (rs = dynamic_cast<CResultSet*>(e.GetSource())) != 0 ) {
            if( rs == m_irs ) {
                m_rowCount = rs->GetTotalRows();
                _TRACE("Rowcount from the last resultset: " << m_rowCount); 
            }
        } 
    }

    if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(e.GetSource());
        if(dynamic_cast<CConnection*>(e.GetSource()) != 0 ) {
            _TRACE("Deleting " << GetIdent() << " " << (void*)this); 
            delete this;
        } 
        else if( m_irs != 0 && (rs = dynamic_cast<CResultSet*>(e.GetSource())) != 0 ) {
            if( rs == m_irs ) {
                _TRACE("Clearing cached CResultSet " << (void*)m_irs); 
                m_irs = 0;
            }
        } 
    }
}

END_NCBI_SCOPE
/*
* $Log$
* Revision 1.29  2005/04/12 18:12:10  ssikorsk
* Added SetAutoClearInParams and IsAutoClearInParams functions to IStatement
*
* Revision 1.28  2005/04/11 14:13:15  ssikorsk
* Explicitly clean a parameter list after Execute (because of the ctlib driver)
*
* Revision 1.27  2005/01/31 14:21:46  kholodov
* Added: use of CDB_ITDescriptor for writing BLOBs
*
* Revision 1.26  2004/11/16 19:59:46  kholodov
* Added: GetBlobOStream() with explicit connection
*
* Revision 1.25  2004/07/21 18:43:58  kholodov
* Added: separate row counter for resultsets
*
* Revision 1.24  2004/07/20 17:49:17  kholodov
* Added: IReader/IWriter support for BLOB I/O
*
* Revision 1.23  2004/07/19 15:51:57  kholodov
* Fixed: unwanted resultset cleanup while calling ExecuteUpdate()
*
* Revision 1.22  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.21  2004/04/26 14:15:28  kholodov
* Added: ExecuteQuery() method
*
* Revision 1.20  2004/04/22 15:14:53  kholodov
* Added: PurgeResults()
*
* Revision 1.19  2004/04/12 14:25:33  kholodov
* Modified: resultset caching scheme, fixed single connection handling
*
* Revision 1.18  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.17  2004/03/02 19:37:56  kholodov
* Added: process close event from CStatement to CResultSet
*
* Revision 1.16  2004/03/01 16:21:55  kholodov
* Fixed: double deletion in calling subsequently CResultset::Close() and delete
*
* Revision 1.15  2004/02/26 18:52:34  kholodov
* Added: more trace messages
*
* Revision 1.14  2004/02/26 16:56:01  kholodov
* Added: Trace message to HasMoreResults()
*
* Revision 1.13  2004/02/19 15:23:21  kholodov
* Fixed: attempt to delete cached CDB_Result when it was already deleted by the CResultSet object
*
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
*/
