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
*
*/

#include <ncbi_pch.hpp>
#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>

#include "conn_impl.hpp"
#include "ds_impl.hpp"
#include "stmt_impl.hpp"
#include "cstmt_impl.hpp"
#include "cursor_impl.hpp"
#include "bulkinsert.hpp"
#include "err_handler.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_ObjImpls

BEGIN_NCBI_SCOPE

// Implementation
CConnection::CConnection(CDataSource* ds, EOwnership ownership)
    : m_ds(ds), m_connection(0), m_connCounter(1), m_connUsed(false),
      m_modeMask(0), m_forceSingle(false), m_multiExH(0),
      m_msgToEx(false), m_ownership(ownership)
{
    _TRACE("Default connection " << (void *)this << " created...");
    SetIdent("CConnection");
}

CConnection::CConnection(CDB_Connection *conn, CDataSource* ds)
    : m_ds(ds), m_connection(conn), m_connCounter(-1), m_connUsed(false),
      m_modeMask(0), m_forceSingle(false), m_multiExH(0),
      m_msgToEx(false)
{
    _TRACE("Auxiliary connection " << (void *)this << " created...");
    SetIdent("CConnection");
}


void CConnection::SetMode(EConnMode mode)
{
    m_modeMask |= mode;
}

void CConnection::ResetMode(EConnMode mode)
{
    m_modeMask &= ~mode;
}

IDataSource* CConnection::GetDataSource()
{
    return m_ds;
}

unsigned int CConnection::GetModeMask()
{
    return m_modeMask;
}

void CConnection::ForceSingle(bool enable)
{
    m_forceSingle = enable;
}

CDB_Connection*
CConnection::GetCDB_Connection()
{
    CHECK_NCBI_DBAPI(m_connection == 0, "Database connection has not been initialized");
    return m_connection;
}

void CConnection::Connect(const string& user,
                          const string& password,
                          const string& server,
                          const string& database)
{
    CDBDefaultConnParams def_params(
            server,
            user,
            password,
            GetModeMask(),
            m_ds->IsPoolUsed()
            );
    const CCPPToolkitConnParams params(def_params);

    def_params.SetDatabaseName(database);

    Connect(params);
}


void CConnection::Connect(const CDBConnParams& params)
{
    CHECK_NCBI_DBAPI(m_connection != 0, "Connection is already open");
    CHECK_NCBI_DBAPI(m_ds == NULL, "m_ds is not initialized");

    m_connection = m_ds->GetDriverContext()->MakeConnection(params);
    // Explicitly set member value ...
    m_database = m_connection? m_connection->DatabaseName(): string();
}


void CConnection::ConnectValidated(IConnValidator& validator,
                                   const string& user,
                                   const string& password,
                                   const string& server,
                                   const string& database)
{
    CDBDefaultConnParams def_params(
            server,
            user,
            password,
            GetModeMask(),
            m_ds->IsPoolUsed()
            );
    const CCPPToolkitConnParams params(def_params);

    def_params.SetDatabaseName(database);
    def_params.SetConnValidator(CRef<IConnValidator>(&validator));

    Connect(params);
}

CConnection::~CConnection()
{
    try {
        if( IsAux() ) {
            _TRACE("Auxiliary connection " << (void*)this << " is being deleted...");
        } else {
            _TRACE("Default connection " << (void*)this << " is being deleted...");
        }
        FreeResources();
        Notify(CDbapiDeletedEvent(this));
        _TRACE(GetIdent() << " " << (void*)this << " deleted.");
    }
    NCBI_CATCH_ALL_X( 1, kEmptyStr )
}


void CConnection::SetDatabase(const string& name)
{
    SetDbName(name);
}

string CConnection::GetDatabase()
{
    return m_database;
}

bool CConnection::IsAlive()
{
    return m_connection == 0 ? false : m_connection->IsAlive();
}

void CConnection::SetDbName(const string& name, CDB_Connection* conn)
{
    m_database = name;

    if( GetDatabase().empty() )
        return;

    CDB_Connection* work = (conn == 0 ? GetCDB_Connection() : conn);
    work->SetDatabaseName(name);
}

CDB_Connection* CConnection::CloneCDB_Conn()
{
    CHECK_NCBI_DBAPI(m_ds == NULL, "m_ds is not initialized");

    CDBDefaultConnParams def_params(
            GetCDB_Connection()->ServerName(),
            GetCDB_Connection()->UserName(),
            GetCDB_Connection()->Password(),
            GetModeMask(),
            true
            );
    const CCPPToolkitConnParams params(def_params);

    def_params.SetHost(GetCDB_Connection()->Host());
    def_params.SetPort(GetCDB_Connection()->Port());
    def_params.SetDatabaseName(GetDatabase());
    def_params.SetParam("do_not_dispatch", "true");
    def_params.SetParam("do_not_read_conf", "true");

    CDB_Connection* tmp_conn(
            m_ds->GetDriverContext()->MakeConnection(params)
        );

    _TRACE("CDB_Connection " << (void*)GetCDB_Connection()
        << " cloned, new CDB_Connection: " << (void*)tmp_conn);

    return tmp_conn;
}

CConnection* CConnection::Clone()
{
    CHECK_NCBI_DBAPI(m_ds == NULL, "m_ds is not initialized");

    CConnection *conn = new CConnection(CloneCDB_Conn(), m_ds);

    if( m_msgToEx )
        conn->MsgToEx(true);

    ++m_connCounter;
    return conn;
}

IConnection* CConnection::CloneConnection(EOwnership ownership)
{
    CHECK_NCBI_DBAPI(m_ds == NULL, "m_ds is not initialized");

    CDB_Connection *cdbConn = CloneCDB_Conn();
    CConnection *conn = new CConnection(m_ds, ownership);

    conn->m_modeMask = this->GetModeMask();
    conn->m_forceSingle = this->m_forceSingle;
    conn->m_database = this->GetDatabase();
    conn->m_connection = cdbConn;
    if( m_msgToEx )
        conn->MsgToEx(true);

    conn->AddListener(m_ds);
    m_ds->AddListener(conn);

    return conn;
}

CConnection* CConnection::GetAuxConn()
{
    if( m_connCounter < 0 )
        return 0;

    CConnection *conn = this;
    CHECK_NCBI_DBAPI( m_connUsed && m_forceSingle, "GetAuxConn(): Extra connections not permitted" );
    if( m_connUsed ) {
        conn = Clone();
        _TRACE("GetAuxConn(): Server: " << GetCDB_Connection()->ServerName()
               << ", open aux connection, total: " << m_connCounter);
    }
    else {
        m_connUsed = true;

        _TRACE("GetAuxconn(): server: " << GetCDB_Connection()->ServerName()
               << ", no aux connections necessary, using default...");
    }

    return conn;

}


void CConnection::Close()
{
    FreeResources();
}

void CConnection::Abort()
{
    GetCDB_Connection()->Abort();
    //FreeResources();
}

void CConnection::SetTimeout(size_t nof_secs)
{
    GetCDB_Connection()->SetTimeout(nof_secs);
}

void CConnection::SetCancelTimeout(size_t nof_secs)
{
    GetCDB_Connection()->SetCancelTimeout(nof_secs);
}

void CConnection::FreeResources()
{
    delete m_connection;
    m_connection = 0;
}

// New part
IStatement* CConnection::GetStatement()
{
    CHECK_NCBI_DBAPI(m_connection == 0, "No connection established");

    CHECK_NCBI_DBAPI(
        m_connUsed,
        "CConnection::GetStatement(): Connection taken, cannot use this method"
        );
    CStatement *stmt = new CStatement(this);
    AddListener(stmt);
    stmt->AddListener(this);
    return stmt;
}

ICallableStatement*
CConnection::GetCallableStatement(const string& proc)
{
    CHECK_NCBI_DBAPI(
        m_connUsed,
        "CConnection::GetCallableStatement(): Connection taken, cannot use this method"
        );
/*
    if( m_cstmt != 0 ) {
        //m_cstmt->PurgeResults();
        delete m_cstmt;
    }
    m_cstmt = new CCallableStatement(proc, this);
    AddListener(m_cstmt);
    m_cstmt->AddListener(this);
    return m_cstmt;
*/
    CCallableStatement *cstmt = new CCallableStatement(proc, this);
    AddListener(cstmt);
    cstmt->AddListener(this);
    return cstmt;
}

ICursor* CConnection::GetCursor(const string& name,
                                const string& sql,
                                int batchSize)
{
//    if( m_connUsed )
//        throw CDbapiException("CConnection::GetCursor(): Connection taken, cannot use this method");
/*
    if( m_cursor != 0 ) {
        delete m_cursor;
    }
    m_cursor = new CCursor(name, sql, batchSize, this);
    AddListener(m_cursor);
    m_cursor->AddListener(this);
    return m_cursor;
*/
    CCursor *cursor = new CCursor(name, sql, batchSize, this);
    AddListener(cursor);
    cursor->AddListener(this);
    return cursor;
}

IBulkInsert* CConnection::GetBulkInsert(const string& table_name)
{
//    if( m_connUsed )
//        throw CDbapiException("CConnection::GetBulkInsert(): Connection taken, cannot use this method");
/*
    if( m_bulkInsert != 0 ) {
        delete m_bulkInsert;
    }
    m_bulkInsert = new CDBAPIBulkInsert(table_name, nof_cols, this);
    AddListener(m_bulkInsert);
    m_bulkInsert->AddListener(this);
    return m_bulkInsert;
*/
    CDBAPIBulkInsert *bulkInsert = new CDBAPIBulkInsert(table_name, this);
    AddListener(bulkInsert);
    bulkInsert->AddListener(this);
    return bulkInsert;
}
// New part end


IStatement* CConnection::CreateStatement()
{
//    if( m_getUsed )
//        throw CDbapiException("CConnection::CreateStatement(): Get...() methods used");

    CStatement *stmt = new CStatement(GetAuxConn());
    AddListener(stmt);
    stmt->AddListener(this);
    return stmt;
}

ICallableStatement*
CConnection::PrepareCall(const string& proc)
{
//    if( m_getUsed )
//        throw CDbapiException("CConnection::CreateCallableStatement(): Get...() methods used");

    CCallableStatement *cstmt = new CCallableStatement(proc, GetAuxConn());
    AddListener(cstmt);
    cstmt->AddListener(this);
    return cstmt;
}

ICursor* CConnection::CreateCursor(const string& name,
                                   const string& sql,
                                   int batchSize)
{
 //   if( m_getUsed )
 //       throw CDbapiException("CConnection::CreateCursor(): Get...() methods used");

    CCursor *cur = new CCursor(name, sql, batchSize, GetAuxConn());
    AddListener(cur);
    cur->AddListener(this);
    return cur;
}

IBulkInsert* CConnection::CreateBulkInsert(const string& table_name)
{
//    if( m_getUsed )
//        throw CDbapiException("CConnection::CreateBulkInsert(): Get...() methods used");

    CDBAPIBulkInsert *bcp = new CDBAPIBulkInsert(table_name, GetAuxConn());
    AddListener(bcp);
    bcp->AddListener(this);
    return bcp;
}

void CConnection::Action(const CDbapiEvent& e)
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName()
        << "' received from " << e.GetSource()->GetIdent() << " " << (void*)e.GetSource());

    if(dynamic_cast<const CDbapiClosedEvent*>(&e) != 0 ) {
/*
        CStatement *stmt;
        CCallableStatement *cstmt;
        CCursor *cursor;
        CDBAPIBulkInsert *bulkInsert;
        if( (cstmt = dynamic_cast<CCallableStatement*>(e.GetSource())) != 0 ) {
            if( cstmt == m_cstmt ) {
                _TRACE("CConnection: Clearing cached callable statement " << (void*)m_cstmt);
                m_cstmt = 0;
            }
        }
        else if( (stmt = dynamic_cast<CStatement*>(e.GetSource())) != 0 ) {
            if( stmt == m_stmt ) {
                _TRACE("CConnection: Clearing cached statement " << (void*)m_stmt);
                m_stmt = 0;
            }
        }
        else if( (cursor = dynamic_cast<CCursor*>(e.GetSource())) != 0 ) {
            if( cursor == m_cursor ) {
                _TRACE("CConnection: Clearing cached cursor " << (void*)m_cursor);
                m_cursor = 0;
            }
        }
        else if( (bulkInsert = dynamic_cast<CDBAPIBulkInsert*>(e.GetSource())) != 0 ) {
            if( bulkInsert == m_bulkInsert ) {
                _TRACE("CConnection: Clearing cached bulkinsert " << (void*)m_bulkInsert);
                m_bulkInsert = 0;
            }
        }
*/
        if( m_connCounter == 1 )
            m_connUsed = false;
    }
    else if(dynamic_cast<const CDbapiAuxDeletedEvent*>(&e) != 0 ) {
        if( m_connCounter > 1 ) {
            --m_connCounter;
            _TRACE("Server: " << GetCDB_Connection()->ServerName()
                   <<", connections left: " << m_connCounter);
        }
        else
            m_connUsed = false;
    }
    else if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(e.GetSource());
        if(dynamic_cast<CDataSource*>(e.GetSource()) != 0 ) {
            if( m_ownership == eNoOwnership ) {
                delete this;
            }
        }
    }
}

void CConnection::MsgToEx(bool v)
{
    if( !v ) {
        // Clear the previous handlers if present
        GetCDB_Connection()->PopMsgHandler(GetHandler());
        _TRACE("MsqToEx(): connection " << (void*)this
            << ": message handler " << (void*)GetHandler()
            << " removed from CDB_Connection " << (void*)GetCDB_Connection());
        m_msgToEx = false;
    }
    else {
        GetCDB_Connection()->PushMsgHandler(GetHandler());
        _TRACE("MsqToEx(): connection " << (void*)this
            << ": message handler " << (void*)GetHandler()
            << " installed on CDB_Connection " << (void*)GetCDB_Connection());
        m_msgToEx = true;
    }
}

CToMultiExHandler* CConnection::GetHandler()
{
    if(m_multiExH == 0 ) {
        m_multiExH = new CToMultiExHandler;
    }
    return m_multiExH;
}

CDB_MultiEx* CConnection::GetErrorAsEx()
{
    return GetHandler()->GetMultiEx();
}

string CConnection::GetErrorInfo()
{
    CNcbiOstrstream out;
    CDB_UserHandler_Stream h(&out);
    h.HandleIt(GetHandler()->GetMultiEx());
    // Install new handler
    GetHandler()->ReplaceMultiEx();
/*
    GetCDB_Connection()->PopMsgHandler(GetHandler());
    delete m_multiExH;
    m_multiExH = new CToMultiExHandler;
    GetCDB_Connection()->PushMsgHandler(GetHandler());
*/
    return CNcbiOstrstreamToString(out);
}

/*
void CConnection::DeleteConn(CConnection* conn)
{
    if( m_connCounter > 1) {
        delete conn;
        --m_connCounter;
    }

    _TRACE("Connection deleted, total left: " << m_connCounter);
    return;
}
*/
END_NCBI_SCOPE
