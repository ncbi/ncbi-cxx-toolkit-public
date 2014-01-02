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

#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>

#include <dbapi/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Dbapi_ConnFactory


BEGIN_NCBI_SCOPE

namespace impl
{

///////////////////////////////////////////////////////////////////////////
//  CConnection::
//

CDB_LangCmd* CConnection::Create_LangCmd(CBaseCmd& lang_cmd)
{
    m_CMDs.push_back(&lang_cmd);

    return new CDB_LangCmd(&lang_cmd);
}

CDB_RPCCmd* CConnection::Create_RPCCmd(CBaseCmd& rpc_cmd)
{
    m_CMDs.push_back(&rpc_cmd);

    return new CDB_RPCCmd(&rpc_cmd);
}

CDB_BCPInCmd* CConnection::Create_BCPInCmd(CBaseCmd& bcpin_cmd)
{
    m_CMDs.push_back(&bcpin_cmd);

    return new CDB_BCPInCmd(&bcpin_cmd);
}

CDB_CursorCmd* CConnection::Create_CursorCmd(CBaseCmd& cursor_cmd)
{
    m_CMDs.push_back(&cursor_cmd);

    return new CDB_CursorCmd(&cursor_cmd);
}

CDB_SendDataCmd* CConnection::Create_SendDataCmd(CSendDataCmd& senddata_cmd)
{
    m_CMDs.push_back(&senddata_cmd);

    return new CDB_SendDataCmd(&senddata_cmd);
}


CConnection::CConnection(CDriverContext& dc,
                         const CDBConnParams& params,
                         bool isBCPable
                         )
: m_DriverContext(&dc)
, m_MsgHandlers(dc.GetConnHandlerStack())
, m_Interface(NULL)
, m_ResProc(NULL)
, m_ServerType(params.GetServerType())
, m_ServerTypeIsKnown(false)
, m_Server(params.GetServerName())
, m_Host(params.GetHost())
, m_Port(params.GetPort())
, m_User(params.GetUserName())
, m_Passwd(params.GetPassword())
, m_Pool(params.GetParam("pool_name"))
, m_Reusable(params.GetParam("is_pooled") == "true")
, m_OpenFinished(false)
, m_Valid(true)
, m_BCPable(isBCPable)
, m_SecureLogin(params.GetParam("secure_login") == "true")
, m_Opened(false)
{
    _ASSERT(m_MsgHandlers.GetSize() == dc.GetConnHandlerStack().GetSize());
    _ASSERT(m_MsgHandlers.GetSize() > 0);
    m_OpeningMsgHandlers = params.GetOpeningMsgHandlers();

    CheckCanOpen();
}

CConnection::~CConnection(void)
{
    DetachResultProcessor();
//         DetachInterface();
    MarkClosed();
}

void CConnection::CheckCanOpen(void)
{
    MarkClosed();

    // Check for maximum number of connections
    if (!CDbapiConnMgr::Instance().AddConnect()) {
        const string conn_num = NStr::NumericToString(CDbapiConnMgr::Instance().GetMaxConnect());
		const string msg = 
			string("Cannot create new connection: maximum connections amount (")
			+ conn_num
			+ ") is exceeded!!!";

        ERR_POST_X_ONCE(3, msg);
        DATABASE_DRIVER_ERROR(msg, 500000);
    }

    m_Opened = true;
}

void CConnection::MarkClosed(void)
{
    if (m_Opened) {
        CDbapiConnMgr::Instance().DelConnect();
        m_Opened = false;
    }
}


CDBConnParams::EServerType 
CConnection::CalculateServerType(CDBConnParams::EServerType server_type)
{
    if (server_type == CDBConnParams::eUnknown) {
        CMsgHandlerGuard guard(*this);

        try {
            auto_ptr<CDB_LangCmd> cmd(LangCmd("SELECT @@version"));
            cmd->Send();

            while (cmd->HasMoreResults()) {
                auto_ptr<CDB_Result> res(cmd->Result());

                if (res.get() != NULL && res->ResultType() == eDB_RowResult ) {
                    CDB_VarChar version;

                    while (res->Fetch()) {
                        res->GetItem(&version);

                        if (!version.IsNULL()) {
                            if (NStr::Compare(
                                        version.AsString(), 
                                        0, 
                                        15, 
                                        "Adaptive Server"
                                        ) == 0) {
                                server_type = CDBConnParams::eSybaseSQLServer;
                            } else if (NStr::Compare(
                                        version.AsString(), 
                                        0, 
                                        20, 
                                        "Microsoft SQL Server"
                                        ) == 0) {
                                server_type = CDBConnParams::eMSSqlServer;
                            }
                        }
                    }
                }
            }
        }
        catch(const CException&) {
            server_type = CDBConnParams::eSybaseOpenServer;
        }
    }

    return server_type;
}

CDBConnParams::EServerType 
CConnection::GetServerType(void)
{
    if (m_ServerType == CDBConnParams::eUnknown && !m_ServerTypeIsKnown) {
        m_ServerType = CalculateServerType(CDBConnParams::eUnknown);
        m_ServerTypeIsKnown = true;
    }

    return m_ServerType;
}


void CConnection::PushMsgHandler(CDB_UserHandler* h,
                                    EOwnership ownership)
{
    m_MsgHandlers.Push(h, ownership);
    _ASSERT(m_MsgHandlers.GetSize() > 0);
}


void CConnection::PopMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Pop(h, false);
    _ASSERT(m_MsgHandlers.GetSize() > 0);
}

void CConnection::DropCmd(impl::CCommand& cmd)
{
    TCommandList::iterator it = find(m_CMDs.begin(), m_CMDs.end(), &cmd);

    if (it != m_CMDs.end()) {
        m_CMDs.erase(it);
    }
}

void CConnection::DeleteAllCommands(void)
{
    while (!m_CMDs.empty()) {
        // Destructor will remove an entity from a container ...
        delete m_CMDs.back();
    }
}

void CConnection::Release(void)
{
    // close all commands first
    DeleteAllCommands();
    GetCDriverContext().DestroyConnImpl(this);
}

I_DriverContext* CConnection::Context(void) const
{
    _ASSERT(m_DriverContext);
    return m_DriverContext;
}

void CConnection::DetachResultProcessor(void)
{
    if (m_ResProc) {
        m_ResProc->ReleaseConn();
        m_ResProc = NULL;
    }
}

CDB_ResultProcessor* CConnection::SetResultProcessor(CDB_ResultProcessor* rp)
{
    CDB_ResultProcessor* r = m_ResProc;
    m_ResProc = rp;
    return r;
}

CDB_Result* CConnection::Create_Result(impl::CResult& result)
{
    return new CDB_Result(&result);
}

const string& CConnection::ServerName(void) const
{
    return m_Server;
}

Uint4 CConnection::Host(void) const
{
    return m_Host;
}

Uint2 CConnection::Port(void) const
{
    return m_Port;
}


const string& CConnection::UserName(void) const
{
    return m_User;
}


const string& CConnection::Password(void) const
{
    return m_Passwd;
}

const string& CConnection::PoolName(void) const
{
    return m_Pool;
}

bool CConnection::IsReusable(void) const
{
    return m_Reusable;
}

void CConnection::AttachTo(CDB_Connection* interface)
{
    m_Interface = interface;
}

void CConnection::ReleaseInterface(void)
{
    m_Interface = NULL;
}


void CConnection::SetTextImageSize(size_t /* nof_bytes */)
{
}


bool
CConnection::IsMultibyteClientEncoding(void) const
{
    return GetCDriverContext().IsMultibyteClientEncoding();
}


EEncoding
CConnection::GetClientEncoding(void) const
{
    return GetCDriverContext().GetClientEncoding();
}

void 
CConnection::SetDatabaseName(const string& name)
{
    if (!name.empty()) {
        const string sql = "use " + name;

        auto_ptr<CDB_LangCmd> auto_stmt(LangCmd(sql));
        auto_stmt->Send();
        auto_stmt->DumpResults();

        m_Database = name;
    }
}

const string&
CConnection::GetDatabaseName(void) const
{
    return m_Database;
}

I_ConnectionExtra::TSockHandle
CConnection::GetLowLevelHandle(void) const
{
    DATABASE_DRIVER_ERROR("GetLowLevelHandle is not implemented", 500001);
}

string CConnection::GetDriverName(void) const
{
    return GetCDriverContext().GetDriverName();
}


} // namespace impl

END_NCBI_SCOPE


