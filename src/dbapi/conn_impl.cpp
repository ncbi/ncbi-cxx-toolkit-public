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
* File Description:   Connection implementation
*
*
* $Log$
* Revision 1.7  2002/04/15 19:08:55  kholodov
* Changed GetContext() -> GetDriverContext
*
* Revision 1.6  2002/02/08 22:43:10  kholodov
* Set/GetDataBase() renamed to Set/GetDatabase() respectively
*
* Revision 1.5  2002/02/08 21:29:54  kholodov
* SetDataBase() restored, connection cloning algorithm changed
*
* Revision 1.4  2002/02/08 17:47:34  kholodov
* Removed SetDataBase() method
*
* Revision 1.3  2002/02/08 17:38:26  kholodov
* Moved listener registration to parent objects
*
* Revision 1.2  2002/02/06 22:20:09  kholodov
* Connections are cloned for CallableStatement and Cursor
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include <dbapi/driver/public.hpp>
//#include <dbapi/driver/interfaces.hpp>
//#include <dbapi/driver/exception.hpp>

#include "conn_impl.hpp"
#include "ds_impl.hpp"
#include "stmt_impl.hpp"
#include "cstmt_impl.hpp"
#include "cursor_impl.hpp"

BEGIN_NCBI_SCOPE

// Implementation
CConnection::CConnection(CDataSource* ds)
    : m_ds(ds), m_connection(0), m_cmdUsed(false)
{

}

CConnection::CConnection(CDB_Connection *conn, CDataSource* ds)
    : m_ds(ds), m_connection(conn), m_cmdUsed(false)
{

}


void CConnection::Connect(const string& user, 
                          const string& password,
                          const string& server,
                          const string& database)
{

    m_connection = m_ds->
        GetDriverContext()->Connect(server.c_str(),
                                    user.c_str(),
                                    password.c_str(),
                                    0,
                                    m_ds->IsPoolUsed());
    SetDbName(database);
					   
}

CConnection::~CConnection()
{
    Close();
    Notify(CDbapiDeletedEvent(this));
}


void CConnection::SetDatabase(const string& name)
{
  SetDbName(name);
}

string CConnection::GetDatabase()
{
    return m_database;
}

void CConnection::SetDbName(const string& name, 
                            CDB_Connection* conn)
{

    m_database = name;

    if( m_database.empty() )
        return;


    CDB_Connection* work = (conn == 0 ? m_connection : conn);
    string sql = "use " + m_database;
    CDB_LangCmd* cmd = work->LangCmd(sql.c_str());
    cmd->Send();
    while( cmd->HasMoreResults() ) {
        cmd->Result();
    }
    delete cmd;
}

CDB_Connection* CConnection::GetConnAux()
{
    CDB_Connection *temp = m_ds->
   GetDriverContext()->Connect(GetConnection()->ServerName(),
                              GetConnection()->UserName(),
                              GetConnection()->Password(),
                              0,
                              true);
    SetDbName(m_database, temp);
    return temp;
}

void CConnection::Close()
{
    delete m_connection;
    m_connection = 0;
}
  
CConnection* CConnection::Clone()
{
    CConnection *conn = new CConnection(GetConnAux(), m_ds);
    conn->AddListener(m_ds);
    m_ds->AddListener(conn);
    return conn;
}

IStatement* CConnection::CreateStatement()
{

    CStatement *stmt = new CStatement(GetFreeConn());
    AddListener(stmt);
    stmt->AddListener(this);
    return stmt;
}

ICallableStatement*
CConnection::PrepareCall(const string& proc,
                         int nofArgs)
{
    CCallableStatement *cstmt = new CCallableStatement(proc, nofArgs, GetFreeConn());
    AddListener(cstmt);
    cstmt->AddListener(this);
    return cstmt;
}

ICursor* CConnection::CreateCursor(const string& name,
                                   const string& sql,
                                   int nofArgs,
                                   int batchSize)
{
    CCursor *cur = new CCursor(name, sql, nofArgs, batchSize, GetFreeConn());
    AddListener(cur);
    cur->AddListener(this);
    return cur;
}

void CConnection::Action(const CDbapiEvent& e) 
{
    if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(dynamic_cast<IEventListener*>(e.GetSource()));
        if(dynamic_cast<CDataSource*>(e.GetSource()) != 0 ) {
            delete this;
            //SetValid(false);
        }
    }
}

CConnection* CConnection::GetFreeConn()
{
    CConnection *conn = this;
    if( !m_cmdUsed ) 
        m_cmdUsed = true;
    else
        conn = Clone();

    return conn;
}

END_NCBI_SCOPE
