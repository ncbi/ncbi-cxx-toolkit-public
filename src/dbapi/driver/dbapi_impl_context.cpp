/*  $Id$
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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>

#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>

#include <corelib/ncbifile.hpp>

#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#endif


BEGIN_NCBI_SCOPE


namespace impl
{

///////////////////////////////////////////////////////////////////////////
//  CDriverContext::
//

CDriverContext::CDriverContext(void) :
    m_MaxTextImageSize(0),
    m_ClientEncoding(eEncoding_ISO8859_1)
{
    PushCntxMsgHandler    ( &CDB_UserHandler::GetDefault(), eTakeOwnership );
    PushDefConnMsgHandler ( &CDB_UserHandler::GetDefault(), eTakeOwnership );
}

CDriverContext::~CDriverContext(void)
{
    return;
}

bool CDriverContext::SetTimeout(unsigned int nof_secs)
{
    bool success = true;
    CMutexGuard mg(m_CtxMtx);

    try {
        if (I_DriverContext::SetTimeout(nof_secs)) {
            // We do not have to update query/connection timeout in context
            // any more. Each connection can be updated explicitly now.
            // UpdateConnTimeout();
        }
    } catch (...) {
        success = false;
    }

    return success;
}

bool CDriverContext::SetMaxTextImageSize(size_t nof_bytes)
{
    CMutexGuard mg(m_CtxMtx);

    m_MaxTextImageSize = nof_bytes;

    UpdateConnMaxTextImageSize();

    return true;
}

void CDriverContext::PushCntxMsgHandler(CDB_UserHandler* h,
                                         EOwnership ownership)
{
    CMutexGuard mg(m_CtxMtx);
    m_CntxHandlers.Push(h, ownership);
}

void CDriverContext::PopCntxMsgHandler(CDB_UserHandler* h)
{
    CMutexGuard mg(m_CtxMtx);
    m_CntxHandlers.Pop(h);
}

void CDriverContext::PushDefConnMsgHandler(CDB_UserHandler* h,
                                            EOwnership ownership)
{
    CMutexGuard mg(m_CtxMtx);
    m_ConnHandlers.Push(h, ownership);
}

void CDriverContext::PopDefConnMsgHandler(CDB_UserHandler* h)
{
    CMutexGuard mg(m_CtxMtx);
    m_ConnHandlers.Pop(h);

    // Remove this handler from all connections
    TConnPool::value_type con = NULL;
    ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;
        con->PopMsgHandler(h);
    }

    ITERATE(TConnPool, it, m_InUse) {
        con = *it;
        con->PopMsgHandler(h);
    }
}


void CDriverContext::SetExtraMsg(const string& msg)
{
    GetCtxHandlerStack().SetExtraMsg(msg);
}


void CDriverContext::ResetEnvSybase(void) const
{
    CMutexGuard mg(m_CtxMtx);
    CNcbiEnvironment env;

    // If user forces his own Sybase client path using $RESET_SYBASE
    // and $SYBASE -- use that unconditionally.
    try {
        if (!env.Get("SYBASE").empty()  &&
            NStr::StringToBool(env.Get("RESET_SYBASE"))) {
            return;
        }
        // ...else try hardcoded paths 
    } catch (const CStringException&) {
        // Conversion error -- try hardcoded paths too
    }

    // User-set or default hardcoded path
    if ( CDir(NCBI_GetSybasePath()).CheckAccess(CDirEntry::fRead) ) {
        env.Set("SYBASE", NCBI_GetSybasePath());
        return;
    }

    // If NCBI_SetSybasePath() was used to set up the Sybase path, and it is
    // not right, then use the very Sybase client against which the code was
    // compiled
    if ( !NStr::Equal(NCBI_GetSybasePath(), NCBI_GetDefaultSybasePath())  &&
         CDir(NCBI_GetDefaultSybasePath()).CheckAccess(CDirEntry::fRead) ) {
        env.Set("SYBASE", NCBI_GetDefaultSybasePath());
    }

    // Else, well, use whatever $SYBASE there is
}


void CDriverContext::x_Recycle(CConnection* conn, bool conn_reusable)
{
    CMutexGuard mg(m_CtxMtx);

    TConnPool::iterator it = find(m_InUse.begin(), m_InUse.end(), conn);

    if (it != m_InUse.end()) {
        m_InUse.erase(it);
    }

    if ( conn_reusable ) {
        m_NotInUse.push_back(conn);
    } else {
        delete conn;
    }
}

void CDriverContext::CloseUnusedConnections(const string&   srv_name,
                                             const string&   pool_name)
{
    CMutexGuard mg(m_CtxMtx);

    TConnPool::value_type con;

    // close all connections first
    NON_CONST_ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;

        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;

        it = --m_NotInUse.erase(it);
        delete con;
    }
}

unsigned int CDriverContext::NofConnections(const string& srv_name,
                                          const string& pool_name) const
{
    CMutexGuard mg(m_CtxMtx);

    if ( srv_name.empty() && pool_name.empty()) {
        return static_cast<unsigned int>(m_InUse.size() + m_NotInUse.size());
    }

    int n = 0;
    TConnPool::value_type con;

    ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;
        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;
        ++n;
    }

    ITERATE(TConnPool, it, m_InUse) {
        con = *it;
        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;
        ++n;
    }

    return n;
}

CDB_Connection* CDriverContext::MakeCDBConnection(CConnection* connection)
{
    m_InUse.push_back(connection);

    return new CDB_Connection(connection);
}

CDB_Connection*
CDriverContext::MakePooledConnection(const CDBConnParams& params)
{
    CMutexGuard mg(m_CtxMtx);

    if (params.IsPooled() && !m_NotInUse.empty()) {
        // try to get a connection from the pot
        if (!params.GetPoolName().empty()) {
            // use a pool name
            for (TConnPool::iterator it = m_NotInUse.begin(); it != m_NotInUse.end(); ) {
                CConnection* t_con(*it);

                // There is no pool name check here. We assume that a connection
                // pool contains connections with appropriate server names only.
                if (params.GetPoolName().compare(t_con->PoolName()) == 0) {
                    it = m_NotInUse.erase(it);
                    if(t_con->Refresh()) {
                        return MakeCDBConnection(t_con);
                    }
                    else {
                        delete t_con;
                    }
                }
                else {
                    ++it;
                }
            }
        }
        else {

            if ( params.GetServerName().empty() ) {
                return NULL;
            }

            // try to use a server name
            for (TConnPool::iterator it = m_NotInUse.begin(); it != m_NotInUse.end(); ) {
                CConnection* t_con(*it);

                if (params.GetServerName().compare(t_con->ServerName()) == 0) {
                    it = m_NotInUse.erase(it);
                    if(t_con->Refresh()) {
                        return MakeCDBConnection(t_con);
                    }
                    else {
                        delete t_con;
                    }
                }
                else {
                    ++it;
                }
            }
        }
    }

    if (params.IsDoNotConnect()) {
        return NULL;
    }

    // Precondition check.
    if (params.GetServerName().empty() ||
        params.GetUserName().empty() ||
        params.GetPassword().empty()) {
        string err_msg("Insufficient info/credentials to connect.");

        if (params.GetServerName().empty()) {
            err_msg += " Server name has not been set.";
        }
        if (params.GetUserName().empty()) {
            err_msg += " User name has not been set.";
        }
        if (params.GetPassword().empty()) {
            err_msg += " Password has not been set.";
        }

        DATABASE_DRIVER_ERROR( err_msg, 200010 );
    }

    CConnection* t_con = MakeIConnection(params);

    return MakeCDBConnection(t_con);
}

void
CDriverContext::CloseAllConn(void)
{
    // close all connections first
    ITERATE(TConnPool, it, m_NotInUse) {
        delete *it;
    }
    m_NotInUse.clear();

    ITERATE(TConnPool, it, m_InUse) {
        (*it)->Close();
    }
}

void
CDriverContext::DeleteAllConn(void)
{
    // close all connections first
    ITERATE(TConnPool, it, m_NotInUse) {
        delete *it;
    }
    m_NotInUse.clear();

    ITERATE(TConnPool, it, m_InUse) {
        delete *it;
    }
    m_InUse.clear();
}

CDB_Connection* 
CDriverContext::MakeConnection(const CDBConnParams& params)
{
    CDB_Connection* t_con =
        CDbapiConnMgr::Instance().GetConnectionFactory()->MakeDBConnection(
            *this,
            params);

    if((!t_con && params.IsDoNotConnect())) {
        return NULL;
    }

    if (!t_con) {
        string err;

        err += "Cannot connect to the server '" + params.GetServerName();
        err += "' as user '" + params.GetUserName() + "'";
        DATABASE_DRIVER_ERROR( err, 100011 );
    }

    return t_con;
}

void CDriverContext::DestroyConnImpl(CConnection* impl)
{
    if (impl) {
        impl->ReleaseInterface();
        x_Recycle(impl, impl->IsReusable());
    }
}

void CDriverContext::SetClientCharset(const string& charset)
{
    CMutexGuard mg(m_CtxMtx);

    m_ClientCharset = charset;
    m_ClientEncoding = eEncoding_Unknown;

    if (NStr::CompareNocase(charset.c_str(), "UTF-8") == 0 ||
        NStr::CompareNocase(charset.c_str(), "UTF8") == 0) {
        m_ClientEncoding = eEncoding_UTF8;
    } else if (NStr::CompareNocase(charset.c_str(), "Ascii") == 0) {
        m_ClientEncoding = eEncoding_Ascii;
    } else if (NStr::CompareNocase(charset.c_str(), "ISO8859_1") == 0 ||
               NStr::CompareNocase(charset.c_str(), "ISO8859-1") == 0
               ) {
        m_ClientEncoding = eEncoding_ISO8859_1;
    } else if (NStr::CompareNocase(charset.c_str(), "Windows_1252") == 0 ||
               NStr::CompareNocase(charset.c_str(), "Windows-1252") == 0) {
        m_ClientEncoding = eEncoding_Windows_1252;
    }
}

void CDriverContext::UpdateConnTimeout(void) const
{
    // Do not protect this method. It is already protected.

    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTimeout(GetTimeout());
    }

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTimeout(GetTimeout());
    }
}


void CDriverContext::UpdateConnMaxTextImageSize(void) const
{
    // Do not protect this method. It is protected.

    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTextImageSize(GetMaxTextImageSize());
    }

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTextImageSize(GetMaxTextImageSize());
    }
}


///////////////////////////////////////////////////////////////////////////
CWinSock::CWinSock(void)
{
#if defined(NCBI_OS_MSWIN)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
    {
        DATABASE_DRIVER_ERROR( "winsock initialization failed", 200001 );
    }
#endif
}

CWinSock::~CWinSock(void)
{
#if defined(NCBI_OS_MSWIN)
        WSACleanup();
#endif
}

} // namespace impl

END_NCBI_SCOPE


