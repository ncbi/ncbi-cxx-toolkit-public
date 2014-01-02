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
 * Author:  Vladimir Soussov
 *
 * File Description:  Driver for CTLib client library
 *
 */

#include <ncbi_pch.hpp>

#include "ctlib_utils.hpp"

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_param.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>
#include <dbapi/driver/impl/handle_stack.hpp>
#include <dbapi/error_codes.hpp>

#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#  include "../ncbi_win_hook.hpp"
#else
#  include <unistd.h>
#endif

#ifdef FTDS_IN_USE
#  include "config.h"
#  include "tds.h"
#endif

#define NCBI_USE_ERRCODE_X   Dbapi_CTLib_Context


BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
namespace ftds64_ctlib
{
#endif

/////////////////////////////////////////////////////////////////////////////
inline
CDiagCompileInfo GetBlankCompileInfo(void)
{
    return CDiagCompileInfo();
}


#ifdef FTDS_IN_USE
#   define s_CTLCtxMtx  s_TDSCtxMtx
#endif

/// Static mutex which will guard all thread-unsafe operations on all ctlib
/// contexts. It is added because several CTLibContext classes can share one
/// global underlying context handle, so there is no other way to synchronize
/// them but some global mutex. Use of non-global context handles considered
/// to be very rare so the impact on using it through global mutex can be
/// treated as insignificant.
DEFINE_STATIC_MUTEX(s_CTLCtxMtx);


/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContextRegistry (Singleton)
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTLibContextRegistry
{
public:
    static CTLibContextRegistry& Instance(void);

    void Add(CTLibContext* ctx);
    void Remove(CTLibContext* ctx);
    void ClearAll(void);
    static void StaticClearAll(void);

    bool ExitProcessIsPatched(void) const
    {
        return m_ExitProcessPatched;
    }

private:
    CTLibContextRegistry(void);
    ~CTLibContextRegistry(void) throw();

    mutable CMutex          m_Mutex;
    vector<CTLibContext*>   m_Registry;
    bool                    m_ExitProcessPatched;

    friend class CSafeStatic_Allocator<CTLibContextRegistry>;
};


/////////////////////////////////////////////////////////////////////////////
CTLibContextRegistry::CTLibContextRegistry(void) :
m_ExitProcessPatched(false)
{
#if defined(NCBI_OS_MSWIN) && defined(NCBI_DLL_BUILD)

    try {
        m_ExitProcessPatched = 
            NWinHook::COnExitProcess::Instance().Add(CTLibContextRegistry::StaticClearAll);
    } catch (const NWinHook::CWinHookException&) {
        // Just in case ...
        m_ExitProcessPatched = false;
    }

#endif
}

CTLibContextRegistry::~CTLibContextRegistry(void) throw()
{
    try {
        ClearAll();
    }
    NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
}

CTLibContextRegistry&
CTLibContextRegistry::Instance(void)
{
    static CSafeStatic<CTLibContextRegistry> instance;

    return instance.Get();
}

void
CTLibContextRegistry::Add(CTLibContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CTLibContext*>::iterator it = find(m_Registry.begin(),
                                              m_Registry.end(),
                                              ctx);
    if (it == m_Registry.end()) {
        m_Registry.push_back(ctx);
    }
}

void
CTLibContextRegistry::Remove(CTLibContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CTLibContext*>::iterator it = find(m_Registry.begin(),
                                              m_Registry.end(),
                                              ctx);

    if (it != m_Registry.end()) {
        m_Registry.erase(it);
        ctx->x_SetRegistry(NULL);
    }
}


void
CTLibContextRegistry::ClearAll(void)
{
    if (!m_Registry.empty())
    {
        CMutexGuard mg(m_Mutex);
        CMutexGuard ctx_mg(s_CTLCtxMtx);

        while ( !m_Registry.empty() ) {
            // x_Close will unregister and remove handler from the registry.
            m_Registry.back()->x_Close(false);
        }
    }
}

void
CTLibContextRegistry::StaticClearAll(void)
{
    CTLibContextRegistry::Instance().ClearAll();
}


/////////////////////////////////////////////////////////////////////////////
namespace ctlib
{

Connection::Connection(CTLibContext& context,
                       CTL_Connection& ctl_conn)
: m_CTL_Context(&context)
, m_CTL_Conn(&ctl_conn)
, m_Handle(NULL)
, m_IsAllocated(false)
, m_IsOpen(false)
, m_IsDead(false)
{
    if (CheckWhileOpening(ct_con_alloc(GetCTLContext().CTLIB_GetContext(),
                                   &m_Handle)) != CS_SUCCEED) {
        DATABASE_DRIVER_ERROR( "Cannot allocate a connection handle.", 100011 );
    }
    m_IsAllocated = true;
}


Connection::~Connection(void) throw()
{
    try {
        if (m_IsAllocated) {
            // Connection must be closed before it is allowed to be dropped.
            Close();
            Drop();
        }
    }
    NCBI_CATCH_ALL_X( 7, NCBI_CURRENT_FUNCTION )
}


bool Connection::Drop(void)
{
    // Connection must be dropped always, even if it is dead.
    if (m_IsAllocated) {
        GetCTLConn().Check(ct_con_drop(m_Handle));
        m_IsAllocated = false;
        m_IsOpen = false;
    }

    return !m_IsAllocated;
}


const CTL_Connection&
Connection::GetCTLConn(void) const
{
    if (!m_CTL_Conn) {
        DATABASE_DRIVER_ERROR( "CTL_Connection wasn't assigned.", 100011 );
    }

    return *m_CTL_Conn;
}


CTL_Connection&
Connection::GetCTLConn(void)
{
    if (!m_CTL_Conn) {
        DATABASE_DRIVER_ERROR( "CTL_Connection wasn't assigned.", 100011 );
    }

    return *m_CTL_Conn;
}


bool Connection::Open(const CDBConnParams& params)
{
    if (!IsOpen() || Close()) {
        CS_RETCODE rc;

        string server_name;

#if defined(FTDS_IN_USE)
        if (params.GetHost()) {
            if (params.GetUserName().empty()) {
                // Kerberos authentication needs a hostname to get appropriate
                // service tickets
                server_name = params.GetServerName();
            } else {
                server_name = impl::ConvertN2A(params.GetHost());
            }
            if (params.GetPort()) {
                server_name += ":" + NStr::IntToString(params.GetPort());
            }
        } else {
            server_name = params.GetServerName();
        }

        rc = CheckWhileOpening(ct_connect(GetNativeHandle(),
                                const_cast<char*>(server_name.data()),
                                server_name.size()));
#else
#if defined(CS_SERVERADDR)
        if (params.GetHost()) {
            server_name = impl::ConvertN2A(params.GetHost());
            if (params.GetPort()) {
                server_name += " " + NStr::IntToString(params.GetPort());
            }

            CheckWhileOpening(ct_con_props(GetNativeHandle(),
                                CS_SET,
                                CS_SERVERADDR,
                                (CS_VOID*)server_name.data(),
                                server_name.size(),
                                NULL));

            // It's strange but at least after one type of error inside
            // ct_connect (when client encoding is unrecognized) and thus
            // after throwing an exception from Check one should make
            // mandatory call to ct_close().
            rc = CheckWhileOpening(ct_connect(GetNativeHandle(),
                                    NULL,
                                    CS_UNUSED));
        } else {
            server_name = params.GetServerName();

            // See comment above
            rc = CheckWhileOpening(ct_connect(GetNativeHandle(),
                                    const_cast<char*>(server_name.data()),
                                    server_name.size()));
        }
#else
        server_name = params.GetServerName();

        // See comment above
        rc = CheckWhileOpening(ct_connect(GetNativeHandle(),
                                const_cast<char*>(server_name.data()),
                                server_name.size()));

#endif
#endif

        if (rc == CS_SUCCEED) {
            m_IsOpen = true;
        }
        else {
            m_IsOpen = false;
        }
    }

    return IsOpen();
}


bool Connection::Close(void)
{
    if (IsOpen()) {
        if (IsDead()  ||  !IsAlive()) {
            if (GetCTLConn().Check(ct_close(GetNativeHandle(), CS_FORCE_CLOSE)) == CS_SUCCEED) {
                m_IsOpen = false;
            }
        } else {
            if (GetCTLConn().Check(ct_close(GetNativeHandle(), CS_UNUSED)) == CS_SUCCEED) {
                m_IsOpen = false;
            }
        }
    }

    return !IsOpen();
}


bool Connection::Cancel(void)
{
    if (IsOpen()) {
        if (!IsAlive()) {
            return false;
        }

        if (GetCTLConn().Check(ct_cancel(
            GetNativeHandle(),
            NULL,
            CS_CANCEL_ALL) != CS_SUCCEED)) {
            return false;
        }
    }

    return true;
}


bool Connection::IsAlive(void)
{
    CS_INT status;
    if (GetCTLConn().Check(ct_con_props(
        GetNativeHandle(),
        CS_GET,
        CS_CON_STATUS,
        &status,
        CS_UNUSED,
        0)) != CS_SUCCEED) {
        return false;
    }

    return
        (status & CS_CONSTAT_CONNECTED) != 0  &&
        (status & CS_CONSTAT_DEAD     ) == 0;
}


bool Connection::IsOpen_native(void)
{
    CS_INT is_logged = CS_TRUE;

#if !defined(FTDS_IN_USE)
    GetCTLConn().Check(ct_con_props(
        GetNativeHandle(),
        CS_GET,
        CS_LOGIN_STATUS,
        (CS_VOID*)&is_logged,
        CS_UNUSED,
        NULL)
    );
#endif

    return is_logged == CS_TRUE;
}


////////////////////////////////////////////////////////////////////////////////
Command::Command(CTL_Connection& ctl_conn)
: m_CTL_Conn(&ctl_conn)
, m_Handle(NULL)
, m_IsAllocated(false)
, m_IsOpen(false)
{
    if (GetCTLConn().Check(ct_cmd_alloc(
                           GetCTLConn().GetNativeConnection().GetNativeHandle(),
                           &m_Handle
                           )) != CS_SUCCEED) {
        DATABASE_DRIVER_ERROR("Cannot allocate a command handle.", 100011);
    }

    m_IsAllocated = true;
}


Command::~Command(void)
{
    try {
        Close();
        Drop();
    }
    NCBI_CATCH_ALL_X( 8, NCBI_CURRENT_FUNCTION )
}


bool
Command::Open(CS_INT type, CS_INT option, const string& arg)
{
    _ASSERT(!m_IsOpen);

    if (!m_IsOpen) {
        m_IsOpen = (GetCTLConn().Check(ct_command(m_Handle,
                                                  type,
                                                  (CS_CHAR*)arg.data(),
                                                  arg.size(),
                                                  option)) == CS_SUCCEED);
    }

    return m_IsOpen;
}


bool
Command::GetDataInfo(CS_IODESC& desc)
{
    return (GetCTLConn().Check(ct_data_info(
        m_Handle,
        CS_GET,
        CS_UNUSED,
        &desc)) == CS_SUCCEED);
}


bool
Command::SendData(CS_VOID* buff, CS_INT buff_len)
{
    return (GetCTLConn().Check(ct_send_data(
        m_Handle,
        buff,
        buff_len)) == CS_SUCCEED);
}


bool
Command::Send(void)
{
    return (GetCTLConn().Check(ct_send(
        m_Handle)) == CS_SUCCEED);
}


CS_RETCODE
Command::GetResults(CS_INT& res_type)
{
    return GetCTLConn().Check(ct_results(m_Handle, &res_type));
}


CS_RETCODE
Command::Fetch(void)
{
    return GetCTLConn().Check(ct_fetch(
        m_Handle,
        CS_UNUSED,
        CS_UNUSED,
        CS_UNUSED,
        0));
}


void
Command::Drop(void)
{
    if (m_IsAllocated) {
        GetCTLConn().Check(ct_cmd_drop(m_Handle));
        m_Handle = NULL;
        m_IsAllocated = false;
    }
}


void
Command::Close(void)
{
    if (m_IsOpen) {
        GetCTLConn().Check(ct_cancel(NULL, m_Handle, CS_CANCEL_ALL));
        m_IsOpen = false;
    }
}


} // namespace ctlib


/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContext::
//

CTLibContext::CTLibContext(bool reuse_context, CS_INT version) :
    m_Context(NULL),
    m_Locale(NULL),
    m_PacketSize(2048),
    m_LoginRetryCount(0),
    m_LoginLoopDelay(0),
    m_TDSVersion(version),
    m_Registry(NULL)
{
#ifdef FTDS_IN_USE
    switch (version) {
        case 40:
        case 42:
        case 46:
        case CS_VERSION_100:
            DATABASE_DRIVER_ERROR("FTDS driver does not support TDS protocol "
                                  "version other than 5.0 or 7.0.",
                                  300011 );
            break;
    }
#endif

    CMutexGuard mg(s_CTLCtxMtx);

    ResetEnvSybase();

    string app_name = GetDiagContext().GetAppName();
    if (app_name.empty()) {
#ifdef FTDS_IN_USE
        app_name = "DBAPI-ftds64";
#else
        app_name = "DBAPI-ctlib";
#endif
    }
    SetApplicationName(NStr::PrintableString(app_name));

    CS_RETCODE r = reuse_context ? Check(cs_ctx_global(version, &m_Context)) :
        Check(cs_ctx_alloc(version, &m_Context));
    if (r != CS_SUCCEED) {
        m_Context = 0;
        DATABASE_DRIVER_ERROR( "Cannot allocate a context", 100001 );
    }


    r = cs_loc_alloc(CTLIB_GetContext(), &m_Locale);
    if (r != CS_SUCCEED) {
        m_Locale = NULL;
    }

    CS_VOID*     cb;
    CS_INT       outlen;
    CPointerPot* p_pot = 0;

    // check if cs message callback is already installed
    r = Check(cs_config(CTLIB_GetContext(), CS_GET, CS_MESSAGE_CB, &cb, CS_UNUSED, &outlen));
    if (r != CS_SUCCEED) {
        m_Context = 0;
        DATABASE_DRIVER_ERROR( "cs_config failed", 100006 );
    }

    if (cb == (CS_VOID*)  CTLIB_cserr_handler) {
        // we did use this context already
        r = Check(cs_config(CTLIB_GetContext(), CS_GET, CS_USERDATA,
                      (CS_VOID*) &p_pot, (CS_INT) sizeof(p_pot), &outlen));
        if (r != CS_SUCCEED) {
            m_Context = 0;
            DATABASE_DRIVER_ERROR( "cs_config failed", 100006 );
        }
    }
    else {
        // this is a brand new context
        r = Check(cs_config(CTLIB_GetContext(), CS_SET, CS_MESSAGE_CB,
                      (CS_VOID*) CTLIB_cserr_handler, CS_UNUSED, NULL));
        if (r != CS_SUCCEED) {
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            DATABASE_DRIVER_ERROR( "Cannot install the cslib message callback", 100005 );
        }

        p_pot = new CPointerPot;
        r = Check(cs_config(CTLIB_GetContext(), CS_SET, CS_USERDATA,
                      (CS_VOID*) &p_pot, (CS_INT) sizeof(p_pot), NULL));
        if (r != CS_SUCCEED) {
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            delete p_pot;
            DATABASE_DRIVER_ERROR( "Cannot install the user data", 100007 );
        }

        r = Check(ct_init(CTLIB_GetContext(), version));
        if (r != CS_SUCCEED) {
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            delete p_pot;
            DATABASE_DRIVER_ERROR( "ct_init failed", 100002 );
        }

        r = Check(ct_callback(CTLIB_GetContext(), NULL, CS_SET, CS_CLIENTMSG_CB,
                        (CS_VOID*) CTLIB_cterr_handler));
        if (r != CS_SUCCEED) {
            Check(ct_exit(CTLIB_GetContext(), CS_FORCE_EXIT));
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            delete p_pot;
            DATABASE_DRIVER_ERROR( "Cannot install the client message callback", 100003 );
        }

        r = Check(ct_callback(CTLIB_GetContext(), NULL, CS_SET, CS_SERVERMSG_CB,
                        (CS_VOID*) CTLIB_srverr_handler));
        if (r != CS_SUCCEED) {
            Check(ct_exit(CTLIB_GetContext(), CS_FORCE_EXIT));
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            delete p_pot;
            DATABASE_DRIVER_ERROR( "Cannot install the server message callback", 100004 );
        }
        // PushCntxMsgHandler();
    }

    if ( p_pot ) {
        p_pot->Add((TPotItem) this);
    }

    m_Registry = &CTLibContextRegistry::Instance();
    x_AddToRegistry();
}


CTLibContext::~CTLibContext()
{
    CMutexGuard mg(s_CTLCtxMtx);

    try {
        x_Close();

        if (m_Locale) {
            cs_loc_drop(CTLIB_GetContext(), m_Locale);
            m_Locale = NULL;
        }
    }
    NCBI_CATCH_ALL_X( 9, NCBI_CURRENT_FUNCTION )
}


CS_RETCODE
CTLibContext::Check(CS_RETCODE rc) const
{
    GetCTLExceptionStorage().Handle(GetCtxHandlerStack(), GetExtraMsg());

    return rc;
}


void
CTLibContext::x_AddToRegistry(void)
{
    if (m_Registry) {
        m_Registry->Add(this);
    }
}

void
CTLibContext::x_RemoveFromRegistry(void)
{
    if (m_Registry) {
        m_Registry->Remove(this);
    }
}

void
CTLibContext::x_SetRegistry(CTLibContextRegistry* registry)
{
    m_Registry = registry;
}

bool CTLibContext::SetLoginTimeout(unsigned int nof_secs)
{
    impl::CDriverContext::SetLoginTimeout(nof_secs);

    CMutexGuard mg(s_CTLCtxMtx);

    int sec = (nof_secs == 0 ? CS_NO_LIMIT : static_cast<int>(nof_secs));

    return Check(ct_config(CTLIB_GetContext(),
                           CS_SET,
                           CS_LOGIN_TIMEOUT,
                           &sec,
                           CS_UNUSED,
                           NULL)) == CS_SUCCEED;
}


bool CTLibContext::SetTimeout(unsigned int nof_secs)
{
    bool success = impl::CDriverContext::SetTimeout(nof_secs);

    CMutexGuard mg(s_CTLCtxMtx);

    int sec = (nof_secs == 0 ? CS_NO_LIMIT : static_cast<int>(nof_secs));

    if (Check(ct_config(CTLIB_GetContext(),
                        CS_SET,
                        CS_TIMEOUT,
                        &sec,
                        CS_UNUSED,
                        NULL)) == CS_SUCCEED
        ) {
        return success;
    }

    return false;
}


bool CTLibContext::SetMaxTextImageSize(size_t nof_bytes)
{
    impl::CDriverContext::SetMaxTextImageSize(nof_bytes);

    CMutexGuard mg(s_CTLCtxMtx);

    CS_INT ti_size = (CS_INT) GetMaxTextImageSize();
    return Check(ct_config(CTLIB_GetContext(),
                           CS_SET,
                           CS_TEXTLIMIT,
                           &ti_size,
                           CS_UNUSED,
                           NULL)) == CS_SUCCEED;
}


unsigned int
CTLibContext::GetLoginTimeout(void) const
{
    {
        // For the sake of Check() and HandlerStack()
        CMutexGuard mg(s_CTLCtxMtx);

        CS_INT t_out = 0;

        if (Check(ct_config(CTLIB_GetContext(),
                            CS_GET,
                            CS_LOGIN_TIMEOUT,
                            &t_out,
                            CS_UNUSED,
                            NULL)) == CS_SUCCEED) {
            return t_out;
        }
    }

    return impl::CDriverContext::GetLoginTimeout();
}


unsigned int CTLibContext::GetTimeout(void) const
{
    {
        // For the sake of Check() and HandlerStack()
        CMutexGuard mg(s_CTLCtxMtx);

        CS_INT t_out = 0;

        if (Check(ct_config(CTLIB_GetContext(),
                            CS_GET,
                            CS_TIMEOUT,
                            &t_out,
                            CS_UNUSED,
                            NULL)) == CS_SUCCEED)
        {
            if (t_out == -1  ||  t_out == CS_NO_LIMIT)
                return 0;
            else
                return t_out;
        }
    }

    return impl::CDriverContext::GetTimeout();
}


string CTLibContext::GetDriverName(void) const
{
#if FTDS_IN_USE
    return "ftds";
#else
    return "ctlib";
#endif
}


impl::CConnection*
CTLibContext::MakeIConnection(const CDBConnParams& params)
{
    CMutexGuard mg(s_CTLCtxMtx);

    return new CTL_Connection(*this, params);
}


bool CTLibContext::IsAbleTo(ECapability cpb) const
{
    switch(cpb) {
    case eBcp:
    case eReturnITDescriptors:
    case eReturnComputeResults:
        return true;
    default:
        break;
    }

    return false;
}


bool
CTLibContext::SetMaxConnect(unsigned int num)
{
    CMutexGuard mg(s_CTLCtxMtx);

    return Check(ct_config(CTLIB_GetContext(),
                           CS_SET,
                           CS_MAX_CONNECT,
                           (CS_VOID*)&num,
                           CS_UNUSED,
                           NULL)) == CS_SUCCEED;
}


unsigned int
CTLibContext::GetMaxConnect(void)
{
    // For the sake of Check() and HandlerStack()
    CMutexGuard mg(s_CTLCtxMtx);

    unsigned int num = 0;

    if (Check(ct_config(CTLIB_GetContext(),
                        CS_GET,
                        CS_MAX_CONNECT,
                        (CS_VOID*)&num,
                        CS_UNUSED,
                        NULL)) != CS_SUCCEED) {
        return 0;
    }

    return num;
}


void
CTLibContext::x_Close(bool delete_conn)
{
    if ( CTLIB_GetContext() ) {
        if (x_SafeToFinalize()) {
            if (delete_conn) {
                DeleteAllConn();
            } else {
                CloseAllConn();
            }
        }

        CS_INT       outlen;
        CPointerPot* p_pot = 0;

        if (Check(cs_config(CTLIB_GetContext(),
                        CS_GET,
                        CS_USERDATA,
                        (void*) &p_pot,
                        (CS_INT) sizeof(p_pot),
                        &outlen)) == CS_SUCCEED
            &&  p_pot != 0) {
            p_pot->Remove(this);
            if (p_pot->NofItems() == 0) {
                if (x_SafeToFinalize()) {
                    if (Check(ct_exit(CTLIB_GetContext(),
                                        CS_UNUSED)) != CS_SUCCEED) {
                        Check(ct_exit(CTLIB_GetContext(),
                                        CS_FORCE_EXIT));
                    }

                    // This is a last driver for this context
                    // Clean context user data ...
                    {
                        CPointerPot* p_pot_tmp = NULL;
                        Check(cs_config(CTLIB_GetContext(),
                                        CS_SET,
                                        CS_USERDATA,
                                        (CS_VOID*) &p_pot_tmp,
                                        (CS_INT) sizeof(p_pot_tmp),
                                        NULL
                                        )
                              );

                        delete p_pot;
                    }

                    Check(cs_ctx_drop(CTLIB_GetContext()));
                }
            }
        }

        m_Context = NULL;
        x_RemoveFromRegistry();
    } else {
        if (delete_conn && x_SafeToFinalize()) {
            DeleteAllConn();
        }
    }
}

bool CTLibContext::x_SafeToFinalize(void) const
{
#if defined(NCBI_OS_MSWIN) && defined(NCBI_DLL_BUILD)
    if (m_Registry) {
        return m_Registry->ExitProcessIsPatched();
    }
#endif

    return true;
}

void CTLibContext::CTLIB_SetApplicationName(const string& a_name)
{
    SetApplicationName( a_name );
}


void CTLibContext::CTLIB_SetHostName(const string& host_name)
{
    SetHostName( host_name );
}


void CTLibContext::CTLIB_SetPacketSize(CS_INT packet_size)
{
    m_PacketSize = packet_size;
}


void CTLibContext::CTLIB_SetLoginRetryCount(CS_INT n)
{
    m_LoginRetryCount = n;
}


void CTLibContext::CTLIB_SetLoginLoopDelay(CS_INT nof_sec)
{
    m_LoginLoopDelay = nof_sec;
}


CS_CONTEXT* CTLibContext::CTLIB_GetContext() const
{
    return m_Context;
}


CS_RETCODE CTLibContext::CTLIB_cserr_handler(CS_CONTEXT* context, CS_CLIENTMSG* msg)
{
    CPointerPot*    p_pot = NULL;
    CTLibContext*   ctl_ctx = NULL;
    CS_INT          outlen;

    try {
        CMutexGuard mg(s_CTLCtxMtx);

        if (cs_config(context,
                      CS_GET,
                      CS_USERDATA,
                      (void*) &p_pot,
                      (CS_INT) sizeof(p_pot),
                      &outlen ) == CS_SUCCEED  &&
            p_pot != 0  &&  p_pot->NofItems() > 0)
        {
            ctl_ctx = (CTLibContext*) p_pot->Get(0);
        }
        if (ctl_ctx
            &&  ctl_ctx->GetCtxHandlerStack().HandleMessage(
                                    msg->severity, msg->msgnumber, msg->msgstring))
        {
            return CS_SUCCEED;
        }

        EDiagSev sev = eDiag_Error;

        if (msg->severity == CS_SV_INFORM) {
            sev = eDiag_Info;
        }
        else if (msg->severity == CS_SV_FATAL) {
            sev = eDiag_Critical;
        }


#ifdef FTDS_IN_USE
        if ((msg->msgnumber & 0xFF) == 25) {
            CDB_TruncateEx ex(
                DIAG_COMPILE_INFO,
                0,
                msg->msgstring,
                msg->msgnumber);

            ex.SetSybaseSeverity(msg->severity);
            GetCTLExceptionStorage().Accept(ex);
            return CS_SUCCEED;
        }
#endif

        CDB_ClientEx ex(
            DIAG_COMPILE_INFO,
            0, msg->msgstring,
            sev,
            msg->msgnumber
            );

        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);
    } catch (...) {
        return CS_FAIL;
    }

    return CS_SUCCEED;
}


static
void PassException(CDB_Exception& ex,
                   const string&  server_name,
                   const string&  user_name,
                   CS_INT         severity,
                   const CDBParams* params
                   )
{
    ex.SetServerName(server_name);
    ex.SetUserName(user_name);
    ex.SetSybaseSeverity(severity);
    ex.SetParams(params);

    /* if (ex.GetSeverity() != eDiag_Info) */ {
        string msg =
            " SERVER: '" + server_name +
            "' USER: '" + user_name + "'" +
            (ex.GetExtraMsg().empty() ? "" : " CONTEXT: '" +
             ex.GetExtraMsg() + "'")
            ;
        ex.AddToMessage(msg);
    }

    GetCTLExceptionStorage().Accept(ex);
}


static
CS_RETCODE
HandleConnStatus(CS_CONNECTION* conn,
                 CS_CLIENTMSG*  msg,
                 const string&  server_name,
                 const string&  user_name
                 )
{
    if(conn) {
        CS_INT login_status = 0;

        if( ct_con_props(conn,
                         CS_GET,
                         CS_LOGIN_STATUS,
                         (CS_VOID*)&login_status,
                         CS_UNUSED,
                         NULL) != CS_SUCCEED) {
            return CS_FAIL;
        }

        if (login_status) {
            CS_RETCODE rc = ct_cancel(conn, NULL, CS_CANCEL_ATTN);

            switch(rc){
            case CS_SUCCEED:
                return CS_SUCCEED;
#if !defined(FTDS_IN_USE)
            case CS_TRYING: {
                CDB_TimeoutEx ex(
                    DIAG_COMPILE_INFO,
                    0,
                    "Got timeout on ct_cancel(CS_CANCEL_ALL)",
                    msg->msgnumber);

                PassException(ex, server_name, user_name, msg->severity, NULL);
            }
#endif
            default:
                return CS_FAIL;
            }
        }
    }

    return CS_FAIL;
}


CS_RETCODE CTLibContext::CTLIB_cterr_handler(CS_CONTEXT* context,
                                             CS_CONNECTION* con,
                                             CS_CLIENTMSG* msg
                                             )
{
    CS_INT          outlen;
    CPointerPot*    p_pot = NULL;
    CTL_Connection* ctl_conn = NULL;
    CTLibContext*   ctl_ctx = NULL;
    string          message;
    string          server_name;
    string          user_name;

    try {
        CMutexGuard mg(s_CTLCtxMtx);

        // Ignoring "The connection has been marked dead" from connection's
        // Close() method.
        if (msg->msgnumber == 16843058
            &&  GetCTLExceptionStorage().IsClosingConnect())
        {
            return CS_SUCCEED;
        }
        if (msg->msgstring) {
            message.append(msg->msgstring);
        }

        // Retrieve CDBHandlerStack ...
        if (con != NULL  &&
            ct_con_props(con,
                         CS_GET,
                         CS_USERDATA,
                         (void*) &ctl_conn,
                         (CS_INT) sizeof(ctl_conn),
                         &outlen ) == CS_SUCCEED  &&  ctl_conn != 0)
        {
            if (ctl_conn->ServerName().size() < 127 && ctl_conn->UserName().size() < 127) {
                server_name = ctl_conn->ServerName();
                user_name = ctl_conn->UserName();
            } else {
                ERR_POST_X(1, Error << "Invalid value of ServerName." << CStackTrace());
            }
        }
        else if (cs_config(context,
                           CS_GET,
                           CS_USERDATA,
                           (void*) &p_pot,
                           (CS_INT) sizeof(p_pot),
                           &outlen ) == CS_SUCCEED  &&
                 p_pot != 0  &&  p_pot->NofItems() > 0)
        {
            ctl_ctx = (CTLibContext*) p_pot->Get(0);
        }
        else {
            if (msg->severity != CS_SV_INFORM) {
                ostrstream err_str;

                // nobody can be informed, let's put it in stderr
                err_str << "CTLIB error handler detects the following error" << endl
                        << "Severity:" << msg->severity << err_str << " Msg # "
                        << msg->msgnumber << endl;
                err_str << msg->msgstring << endl;

                if (msg->osstringlen > 1) {
                    err_str << "OS # "    << msg->osnumber
                            << " OS msg " << msg->osstring << endl;
                }

                if (msg->sqlstatelen > 1  &&
                    (msg->sqlstate[0] != 'Z' || msg->sqlstate[1] != 'Z')) {
                    err_str << "SQL: " << msg->sqlstate << endl;
                }

                ERR_POST_X(2, (string)CNcbiOstrstreamToString(err_str));
            }

            return CS_SUCCEED;
        }

        impl::CDBHandlerStack* handlers;
        const CDBParams*       params = NULL;
        if (ctl_conn) {
            handlers = &ctl_conn->GetMsgHandlers();
            message += ctl_conn->GetDbgInfo();
            params = ctl_conn->GetBindParams();
        } else
            handlers = &ctl_ctx->GetCtxHandlerStack();
        if (handlers->HandleMessage(msg->severity, msg->msgnumber, msg->msgstring))
            return CS_SUCCEED;

        // In case of timeout ...
        /* Experimental. Based on C-Toolkit and code developed by Eugene
         * Yaschenko.
        if (msg->msgnumber == 16908863) {
            return HandleConnStatus(con, msg, server_name, user_name);
        }
        */

#ifdef FTDS_IN_USE
        if ((msg->msgnumber & 0xFF) == 25) {
            CDB_TruncateEx ex(
                DIAG_COMPILE_INFO,
                0,
                message,
                msg->msgnumber);

            PassException(ex, server_name, user_name, msg->severity, params);
            return CS_SUCCEED;
        }
#endif

        // Process the message ...
        switch (msg->severity) {
        case CS_SV_INFORM: {
            CDB_ClientEx ex( DIAG_COMPILE_INFO,
                               0,
                               message,
                               eDiag_Info,
                               msg->msgnumber);

            PassException(ex, server_name, user_name, msg->severity, params);

            break;
        }
        case CS_SV_RETRY_FAIL: {
            CDB_TimeoutEx ex(
                DIAG_COMPILE_INFO,
                0,
                message,
                msg->msgnumber);

            PassException(ex, server_name, user_name, msg->severity, params);

            return HandleConnStatus(con, msg, server_name, user_name);

            break;
        }
        case CS_SV_CONFIG_FAIL:
        case CS_SV_API_FAIL:
        case CS_SV_INTERNAL_FAIL: {
            CDB_ClientEx ex( DIAG_COMPILE_INFO,
                              0,
                              message,
                              eDiag_Error,
                              msg->msgnumber);

            PassException(ex, server_name, user_name, msg->severity, params);

            break;
        }
        default: {
            CDB_ClientEx ex(
                DIAG_COMPILE_INFO,
                0,
                message,
                eDiag_Critical,
                msg->msgnumber);

            PassException(ex, server_name, user_name, msg->severity, params);

            break;
        }
        }
    } catch (...) {
        return CS_FAIL;
    }

    return CS_SUCCEED;
}


CS_RETCODE CTLibContext::CTLIB_srverr_handler(CS_CONTEXT* context,
                                              CS_CONNECTION* con,
                                              CS_SERVERMSG* msg
                                              )
{
    if (
        /* (msg->severity == 0  &&  msg->msgnumber == 0)  ||*/
        // commented out because nobody remember why it is there and PubSeqOS does
        // send messages with 0 0 that need to be processed
        msg->msgnumber == 3621 ||  // The statement has been terminated.
        msg->msgnumber == 3980 ||  // The request failed to run because the batch is aborted...
        msg->msgnumber == 5701 ||  // Changed database context to ...
        msg->msgnumber == 5703 ||  // Changed language setting to ...
        msg->msgnumber == 5704 ||  // Changed client character set setting to ...
        msg->msgnumber == 2401 ||  // Character set conversion is not available between client character set and server character set
        msg->msgnumber == 2411     // No conversions will be done
        ) {
        return CS_SUCCEED;
    }

    CS_INT          outlen;
    CPointerPot*    p_pot = NULL;
    CTL_Connection* ctl_conn = NULL;
    CTLibContext*   ctl_ctx = NULL;
    string          message;
    string          server_name;
    string          user_name;

    try {
        CMutexGuard mg(s_CTLCtxMtx);

        if (con != NULL && ct_con_props(con, CS_GET, CS_USERDATA,
                                       (void*) &ctl_conn, (CS_INT) sizeof(ctl_conn),
                                       &outlen) == CS_SUCCEED  &&
            ctl_conn != NULL)
        {
            if (ctl_conn->ServerName().size() < 127 && ctl_conn->UserName().size() < 127) {
                server_name = ctl_conn->ServerName();
                user_name = ctl_conn->UserName();
            } else {
                ERR_POST_X(3, Error << "Invalid value of ServerName." << CStackTrace());
            }
        }
        else if (cs_config(context, CS_GET,
                           CS_USERDATA,
                           (void*) &p_pot,
                           (CS_INT) sizeof(p_pot),
                           &outlen) == CS_SUCCEED  &&
                 p_pot != 0  &&  p_pot->NofItems() > 0)
        {
            ctl_ctx = (CTLibContext*) p_pot->Get(0);
            server_name = string(msg->svrname, msg->svrnlen);
        }
        else {
            ostrstream err_str;

            err_str << "Message from the server ";

            if (msg->svrnlen > 0) {
                err_str << "<" << msg->svrname << "> ";
            }

            err_str << "msg # " << msg->msgnumber
                    << " severity: " << msg->severity << endl;

            if (msg->proclen > 0) {
                err_str << "Proc: " << msg->proc << " line: " << msg->line << endl;
            }

            if (msg->sqlstatelen > 1  &&
                (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
                err_str << "SQL: " << msg->sqlstate << endl;
            }

            err_str << msg->text << endl;

            ERR_POST_X(4, (string)CNcbiOstrstreamToString(err_str));

            return CS_SUCCEED;
        }

        impl::CDBHandlerStack* handlers;
        const CDBParams*       params = NULL;
        if (ctl_conn)
            handlers = &ctl_conn->GetMsgHandlers();
        else
            handlers = &ctl_ctx->GetCtxHandlerStack();
        if (handlers->HandleMessage(msg->severity, msg->msgnumber, msg->text))
            return CS_SUCCEED;

        if ( msg->text ) {
            message += msg->text;
        }
        if (ctl_conn) {
            message += ctl_conn->GetDbgInfo();
            params = ctl_conn->GetBindParams();
        }

        if (msg->msgnumber == 1205 /*DEADLOCK*/) {
            CDB_DeadlockEx ex(DIAG_COMPILE_INFO,
                              0,
                              message);

            PassException(ex, server_name, user_name, msg->severity, params);
        }
        else if (msg->msgnumber == 1771  ||  msg->msgnumber == 1708) {
            // "Maximum row size exceeds allowable width. It is being rounded down to 32767 bytes."
            // "Row size (32767 bytes) could exceed row size limit, which is 1962 bytes."
            // and in ftds
            // "The table has been created but its maximum row size exceeds the maximum number of bytes
            //  per row (8060). INSERT or UPDATE of a row in this table will fail if the resulting row
            //  length exceeds 8060 bytes."
            ERR_POST_X(11, Warning << message);
        }
        else {
            EDiagSev sev =
                msg->severity <  10 ? eDiag_Info :
                msg->severity == 10 ? (msg->msgnumber == 0 ? eDiag_Info : eDiag_Warning) :
                msg->severity <  16 ? eDiag_Error : eDiag_Critical;

            if (msg->proclen > 0) {
                CDB_RPCEx ex(DIAG_COMPILE_INFO,
                              0,
                              message,
                              sev,
                              (int) msg->msgnumber,
                              msg->proc,
                              (int) msg->line);

                PassException(ex, server_name, user_name, msg->severity,
                              params);
            }
            else if (msg->sqlstatelen > 1  &&
                     (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
                CDB_SQLEx ex(DIAG_COMPILE_INFO,
                              0,
                              message,
                              sev,
                              (int) msg->msgnumber,
                              (const char*) msg->sqlstate,
                              (int) msg->line);

                PassException(ex, server_name, user_name, msg->severity,
                              params);
            }
            else {
                CDB_DSEx ex(DIAG_COMPILE_INFO,
                            0,
                            message,
                            sev,
                            (int) msg->msgnumber);

                PassException(ex, server_name, user_name, msg->severity,
                              params);
            }
        }
    } catch (...) {
        return CS_FAIL;
    }

    return CS_SUCCEED;
}


void CTLibContext::SetClientCharset(const string& charset)
{
    impl::CDriverContext::SetClientCharset(charset);

    if ( !GetClientCharset().empty() ) {
        CMutexGuard mg(s_CTLCtxMtx);

        cs_locale(CTLIB_GetContext(),
                  CS_SET,
                  m_Locale,
                  CS_SYB_CHARSET,
                  (CS_CHAR*) GetClientCharset().data(),
                  GetClientCharset().size(),
                  NULL);
    }
}

// Tunable version of TDS protocol to use

#if !defined(NCBI_CTLIB_TDS_VERSION)
#    define NCBI_CTLIB_TDS_VERSION 125
#endif

#define NCBI_CTLIB_TDS_FALLBACK_VERSION 110


#ifdef FTDS_IN_USE

NCBI_PARAM_DECL  (int, ftds, TDS_VERSION);
NCBI_PARAM_DEF_EX(int, ftds, TDS_VERSION,
                  0,                       // default is to auto-detect
                  eParam_NoThread,
                  FTDS_TDS_VERSION);
typedef NCBI_PARAM_TYPE(ftds, TDS_VERSION) TFtdsTdsVersion;

#else

NCBI_PARAM_DECL  (int, ctlib, TDS_VERSION);
NCBI_PARAM_DEF_EX(int, ctlib, TDS_VERSION,
                  NCBI_CTLIB_TDS_VERSION,  // default TDS version
                  eParam_NoThread,
                  CTLIB_TDS_VERSION);
typedef NCBI_PARAM_TYPE(ctlib, TDS_VERSION) TCtlibTdsVersion;

#endif


#ifdef FTDS_IN_USE
# define FTDS_VERSION_ERR_LEVEL Info
#else
# define FTDS_VERSION_ERR_LEVEL Warning
#endif

CS_INT GetCtlibTdsVersion(int version)
{
    if (version == 0) {
#ifdef FTDS_IN_USE
        return TFtdsTdsVersion::GetDefault();
#else
        version = TCtlibTdsVersion::GetDefault();
#endif
    }

    switch ( version ) {
    case 42:
    case 46:
    case 70:
    case 80:
        return version;
    case 100:
        return CS_VERSION_100;
    case 110:
        return CS_VERSION_110;
#ifdef CS_VERSION_120
    case 120:
        return CS_VERSION_120;
#endif
#ifdef CS_VERSION_125
    case 125:
        return CS_VERSION_125;
#endif
    }

    int fallback_version = (version == NCBI_CTLIB_TDS_VERSION) ?
        NCBI_CTLIB_TDS_FALLBACK_VERSION : NCBI_CTLIB_TDS_VERSION;

    ERR_POST_X(5, FTDS_VERSION_ERR_LEVEL
               << "The version " << version << " of TDS protocol for "
               "the DBAPI CTLib driver is not supported. Falling back to "
               "the TDS protocol version " << fallback_version << ".");

    return GetCtlibTdsVersion(fallback_version);
}



///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

///////////////////////////////////////////////////////////////////////////////
class CDbapiCtlibCFBase : public CSimpleClassFactoryImpl<I_DriverContext, CTLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CTLibContext> TParent;

public:
    CDbapiCtlibCFBase(const string& driver_name);
    ~CDbapiCtlibCFBase(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiCtlibCFBase::CDbapiCtlibCFBase(const string& driver_name)
    : TParent( driver_name, 0 )
{
    return ;
}

CDbapiCtlibCFBase::~CDbapiCtlibCFBase(void)
{
    return ;
}

CDbapiCtlibCFBase::TInterface*
CDbapiCtlibCFBase::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    auto_ptr<TImplementation> drv;

    if ( !driver.empty()  &&  driver != m_DriverName ) {
        return 0;
    }

    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {

        // Mandatory parameters ....
#ifdef FTDS_IN_USE
        bool reuse_context = false; // Be careful !!!
        int  tds_version   = 0;
#else
        // Previous behahviour was: reuse_context = true
        bool reuse_context = false;
        int  tds_version   = NCBI_CTLIB_TDS_VERSION;
#endif

        // Optional parameters ...
        CS_INT page_size = 0;
        string prog_name;
        string host_name;
        string client_charset;
        unsigned int max_connect = 0;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TValueType   TValue;

            // Get parameters ...
            TCIter cit  = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "reuse_context" ) {
                    reuse_context = (v.value != "false");
                } else if ( v.id == "version" ) {
                    tds_version = NStr::StringToInt(v.value);
                    _TRACE("WARNING: user manually set TDS version to " << tds_version);
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt(v.value);
                } else if ( v.id == "prog_name" ) {
                    prog_name = v.value;
                } else if ( v.id == "host_name" ) {
                    host_name = v.value;
                } else if ( v.id == "client_charset" ) {
                    client_charset = v.value;
                } else if ( v.id == "max_connect" ) {
                    max_connect = NStr::StringToInt(v.value);;
                }
            }
        }

        // Create a driver ...
        drv.reset(new CTLibContext(reuse_context,
                                   GetCtlibTdsVersion(tds_version)
                                   )
                  );

        // Set parameters ...
        if (page_size) {
            drv->CTLIB_SetPacketSize(page_size);
        }

        if (!prog_name.empty()) {
            drv->SetApplicationName(prog_name);
        }

        if (!host_name.empty()) {
            drv->SetHostName(host_name);
        }

        if (!client_charset.empty()) {
            drv->SetClientCharset(client_charset);
        }

        if (max_connect && CDbapiConnMgr::Instance().GetMaxConnect() < max_connect) {
            CDbapiConnMgr::Instance().SetMaxConnect(max_connect);
        }
        drv->SetMaxConnect(1000);
    }

    return drv.release();
}

///////////////////////////////////////////////////////////////////////////////
#if defined(FTDS_IN_USE)

class CDbapiCtlibCF_ftds64_ctlib : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_ftds64_ctlib(void)
    : CDbapiCtlibCFBase("ftds64")
    {
    }
};


class CDbapiCtlibCF_ftds : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_ftds(void)
    : CDbapiCtlibCFBase("ftds")
    {
    }
};

#else

class CDbapiCtlibCF_Sybase : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_Sybase(void)
    : CDbapiCtlibCFBase("ctlib")
    {
    }
};

#endif


#ifdef FTDS_IN_USE
} // namespace ftds64_ctlib
#endif


///////////////////////////////////////////////////////////////////////////////
#if defined(FTDS_IN_USE)

void
NCBI_EntryPoint_xdbapi_ftds64(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<ftds64_ctlib::CDbapiCtlibCF_ftds64_ctlib>::NCBI_EntryPointImpl( info_list, method );
}

void
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<ftds64_ctlib::CDbapiCtlibCF_ftds>::NCBI_EntryPointImpl(
        info_list,
        method
        );
}

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
DBAPI_RegisterDriver_FTDS(void)
{
    RegisterEntryPoint<I_DriverContext>(NCBI_EntryPoint_xdbapi_ftds);
    RegisterEntryPoint<I_DriverContext>(NCBI_EntryPoint_xdbapi_ftds64);
}


#else // defined(FTDS_IN_USE)

void
NCBI_EntryPoint_xdbapi_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiCtlibCF_Sybase>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
DBAPI_RegisterDriver_CTLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ctlib );
}


#endif // defined(FTDS_IN_USE)

END_NCBI_SCOPE


