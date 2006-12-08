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

#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>

#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#  include "../ncbi_win_hook.hpp"
#else
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE


static const CDiagCompileInfo kBlankCompileInfo;


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

    friend class CSafeStaticPtr<CTLibContextRegistry>;
};


/////////////////////////////////////////////////////////////////////////////
CTLibContextRegistry::CTLibContextRegistry(void) :
m_ExitProcessPatched(false)
{
#if defined(NCBI_OS_MSWIN)

    try {
        NWinHook::COnExitProcess::Instance().Add(CTLibContextRegistry::StaticClearAll);
        m_ExitProcessPatched = true;
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
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

CTLibContextRegistry&
CTLibContextRegistry::Instance(void)
{
    static CSafeStaticPtr<CTLibContextRegistry> instance;

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
BEGIN_SCOPE(ctlib)

Connection::Connection(CTLibContext& context,
                       CTL_Connection* ctl_conn) :
    m_CTL_Context(&context),
    m_CTL_Conn(ctl_conn),
    m_Handle(NULL),
    m_IsAllocated(false),
    m_IsOpen(false)
{
    if (GetCTLContext().Check(ct_con_alloc(GetCTLContext().CTLIB_GetContext(),
                                   &m_Handle)) != CS_SUCCEED) {
        DATABASE_DRIVER_ERROR( "Cannot allocate a connection handle.", 100011 );
    }
    m_IsAllocated = true;
}


Connection::~Connection(void) throw()
{
    try {
        Drop();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool Connection::Drop(void)
{
    if (m_IsAllocated) {
        GetCTLConn().Check(ct_con_drop(m_Handle));
        m_IsAllocated = false;
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


bool Connection::Open(const string& srv_name)
{
    if (!IsOpen() || Close()) {
        CS_RETCODE rc;

        rc = GetCTLContext().Check(ct_connect(GetNativeHandle(),
                                const_cast<char*> (srv_name.c_str()),
                                CS_NULLTERM));

        if (rc == CS_SUCCEED) {
            m_IsOpen = true;
        }
    }

    return IsOpen();
}


bool Connection::Close(void)
{
    if (IsOpen()) {
        if (GetCTLConn().Check(ct_close(GetNativeHandle(), CS_UNUSED)) == CS_SUCCEED ||
            GetCTLConn().Check(ct_close(GetNativeHandle(), CS_FORCE_CLOSE)) == CS_SUCCEED) {
            m_IsOpen = false;
        }
    }

    return !IsOpen();
}


END_SCOPE(ctlib)


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
    DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(xMutex);

    SetApplicationName("CTLibDriver");

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
    try {
        x_Close();

        if (m_Locale) {
            cs_loc_drop(CTLIB_GetContext(), m_Locale);
            m_Locale = NULL;
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


CS_RETCODE
CTLibContext::Check(CS_RETCODE rc) const
{
    // We need const_cast here because of someone's idea of non-const argument
    // of Handle()
    GetCTLExceptionStorage().Handle(const_cast<CDBHandlerStack&>(GetCtxHandlerStack()));

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
    CFastMutexGuard mg(m_Mtx);

    I_DriverContext::SetLoginTimeout(nof_secs);

    CS_INT t_out = (CS_INT) GetLoginTimeout();
    t_out = (t_out == 0 ? CS_NO_LIMIT : t_out);

    return Check(ct_config(CTLIB_GetContext(),
                           CS_SET,
                           CS_LOGIN_TIMEOUT,
                           &t_out,
                           CS_UNUSED,
                           NULL)) == CS_SUCCEED;
}


bool CTLibContext::SetTimeout(unsigned int nof_secs)
{
    if (impl::CDriverContext::SetTimeout(nof_secs)) {
        CFastMutexGuard mg(m_Mtx);

        CS_INT t_out = (CS_INT) GetTimeout();
        t_out = (t_out == 0 ? CS_NO_LIMIT : t_out);

        return Check(ct_config(CTLIB_GetContext(),
                               CS_SET,
                               CS_TIMEOUT,
                               &t_out,
                               CS_UNUSED,
                               NULL)) == CS_SUCCEED;
    }

    return false;
}


bool CTLibContext::SetMaxTextImageSize(size_t nof_bytes)
{
    impl::CDriverContext::SetMaxTextImageSize(nof_bytes);

    CFastMutexGuard mg(m_Mtx);

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
    CS_INT t_out = 0;

    if (Check(ct_config(CTLIB_GetContext(),
                           CS_GET,
                           CS_LOGIN_TIMEOUT,
                           &t_out,
                           CS_UNUSED,
                           NULL)) == CS_SUCCEED) {
        return t_out;
    }

    return I_DriverContext::GetLoginTimeout();
}


unsigned int CTLibContext::GetTimeout(void) const
{
    CS_INT t_out = 0;

    if (Check(ct_config(CTLIB_GetContext(),
                        CS_GET,
                        CS_TIMEOUT,
                        &t_out,
                        CS_UNUSED,
                        NULL)) == CS_SUCCEED) {
        return t_out;
    }

    return I_DriverContext::GetTimeout();
}


impl::CConnection*
CTLibContext::MakeIConnection(const SConnAttr& conn_attr)
{
    return new CTL_Connection(*this, conn_attr);
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


void
CTLibContext::x_Close(bool delete_conn)
{
    if ( CTLIB_GetContext() ) {
        CFastMutexGuard mg(m_Mtx);
        if (CTLIB_GetContext()) {

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
                    // this is a last driver for this context
                    delete p_pot;

                    if (x_SafeToFinalize()) {
                        if (Check(ct_exit(CTLIB_GetContext(),
                                          CS_UNUSED)) != CS_SUCCEED) {
                            Check(ct_exit(CTLIB_GetContext(),
                                          CS_FORCE_EXIT));
                        }
                        Check(cs_ctx_drop(CTLIB_GetContext()));
                    }
                }
            }

            m_Context = NULL;
            x_RemoveFromRegistry();
        }
    } else {
        if (delete_conn && x_SafeToFinalize()) {
            DeleteAllConn();
        }
    }
}

bool CTLibContext::x_SafeToFinalize(void) const
{
    if (m_Registry) {
#if defined(NCBI_OS_MSWIN)
        return m_Registry->ExitProcessIsPatched();
#endif
    }

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
    EDiagSev sev = eDiag_Error;

    if (msg->severity == CS_SV_INFORM) {
        sev = eDiag_Info;
    }
    else if (msg->severity == CS_SV_FATAL) {
        sev = eDiag_Critical;
    }

    CDB_ClientEx ex(
        kBlankCompileInfo,
        0, msg->msgstring,
        sev,
        msg->msgnumber
        );

    ex.SetSybaseSeverity(msg->severity);

    GetCTLExceptionStorage().Accept(ex);

    return CS_SUCCEED;
}


CS_RETCODE CTLibContext::CTLIB_cterr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                       CS_CLIENTMSG* msg)
{
    CS_INT          outlen;
    CPointerPot*    p_pot = 0;
    CTL_Connection* link = 0;
    string          message;
    string          server_name;
    string          user_name;

    if ( msg->msgstring ) {
        message.append( msg->msgstring );
    }

    // Retrieve CDBHandlerStack ...
    if (con != 0  &&
        ct_con_props(con,
                     CS_GET,
                     CS_USERDATA,
                     (void*) &link,
                     (CS_INT) sizeof(link),
                     &outlen ) == CS_SUCCEED  &&  link != 0) {
        server_name = link->ServerName();
        user_name = link->UserName();
    }
    else if (cs_config(context,
                       CS_GET,
                       CS_USERDATA,
                       (void*) &p_pot,
                       (CS_INT) sizeof(p_pot),
                       &outlen ) == CS_SUCCEED  &&
             p_pot != 0  &&  p_pot->NofItems() > 0) {
        // CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
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
                (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
                err_str << "SQL: " << msg->sqlstate << endl;
            }

            ERR_POST((string)CNcbiOstrstreamToString(err_str));
        }

        return CS_SUCCEED;
    }

    // Process the message ...
    switch (msg->severity) {
    case CS_SV_INFORM: {
        CDB_ClientEx ex( kBlankCompileInfo,
                           0,
                           message,
                           eDiag_Info,
                           msg->msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);

        break;
    }
    case CS_SV_RETRY_FAIL: {
        CDB_TimeoutEx ex(
            kBlankCompileInfo,
            0,
            message,
            msg->msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);

        if( con ) {
            CS_INT status;
            if((ct_con_props(con,
                             CS_GET,
                             CS_LOGIN_STATUS,
                             (CS_VOID*)&status,
                             CS_UNUSED,
                             NULL) != CS_SUCCEED) ||
                (!status)) {
                return CS_FAIL;
            }

            if(ct_cancel(con, (CS_COMMAND*)0, CS_CANCEL_ATTN) != CS_SUCCEED) {
                return CS_FAIL;
            }
        }
        else {
            return CS_FAIL;
        }

        break;
    }
    case CS_SV_CONFIG_FAIL:
    case CS_SV_API_FAIL:
    case CS_SV_INTERNAL_FAIL: {
        CDB_ClientEx ex( kBlankCompileInfo,
                          0,
                          message,
                          eDiag_Error,
                          msg->msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);

        break;
    }
    default: {
        CDB_ClientEx ex(
            kBlankCompileInfo,
            0,
            message,
            eDiag_Critical,
            msg->msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);

        break;
    }
    }

    return CS_SUCCEED;
}


CS_RETCODE CTLibContext::CTLIB_srverr_handler(CS_CONTEXT* context,
                                        CS_CONNECTION* con,
                                        CS_SERVERMSG* msg)
{
    if (
        /* (msg->severity == 0  &&  msg->msgnumber == 0)  ||*/
        // commented out because nobody remember why it is there and PubSeqOS does
        // send messages with 0 0 that need to be processed
        msg->msgnumber == 5701 ||
        msg->msgnumber == 5703 ||
        msg->msgnumber == 5704) {
        return CS_SUCCEED;
    }

    CS_INT          outlen;
    CPointerPot*    p_pot = 0;
    CTL_Connection* link = 0;
    string          message;
    string          server_name;
    string          user_name;

    if (con != 0  &&  ct_con_props(con, CS_GET, CS_USERDATA,
                                   (void*) &link, (CS_INT) sizeof(link),
                                   &outlen) == CS_SUCCEED  &&
        link != 0) {
        server_name = link->ServerName();
        user_name = link->UserName();
    }
    else if (cs_config(context, CS_GET,
                       CS_USERDATA,
                       (void*) &p_pot,
                       (CS_INT) sizeof(p_pot),
                       &outlen) == CS_SUCCEED  &&
             p_pot != 0  &&  p_pot->NofItems() > 0) {

        // CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
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

        ERR_POST((string)CNcbiOstrstreamToString(err_str));

        return CS_SUCCEED;
    }

    if ( msg->text ) {
        message += msg->text;
    }

    if (msg->msgnumber == 1205 /*DEADLOCK*/) {
        CDB_DeadlockEx ex(kBlankCompileInfo,
                          0,
                          message);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);
    }
    else {
        EDiagSev sev =
            msg->severity <  10 ? eDiag_Info :
            msg->severity == 10 ? (msg->msgnumber == 0 ? eDiag_Info : eDiag_Warning) :
            msg->severity <  16 ? eDiag_Error : eDiag_Critical;

        if (msg->proclen > 0) {
            CDB_RPCEx ex(kBlankCompileInfo,
                          0,
                          message,
                          sev,
                          (int) msg->msgnumber,
                          msg->proc,
                          (int) msg->line);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(msg->severity);

            GetCTLExceptionStorage().Accept(ex);
        }
        else if (msg->sqlstatelen > 1  &&
                 (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
            CDB_SQLEx ex(kBlankCompileInfo,
                          0,
                          message,
                          sev,
                          (int) msg->msgnumber,
                          (const char*) msg->sqlstate,
                          (int) msg->line);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(msg->severity);

            GetCTLExceptionStorage().Accept(ex);
        }
        else {
            CDB_DSEx ex(kBlankCompileInfo,
                        0,
                        message,
                        sev,
                        (int) msg->msgnumber);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(msg->severity);

            GetCTLExceptionStorage().Accept(ex);
        }
    }

    return CS_SUCCEED;
}


void CTLibContext::SetClientCharset(const string& charset)
{
    impl::CDriverContext::SetClientCharset(charset);

    if ( !GetClientCharset().empty() ) {
        cs_locale(CTLIB_GetContext(),
                  CS_SET,
                  m_Locale,
                  CS_SYB_CHARSET,
                  (CS_CHAR*) GetClientCharset().c_str(),
                  CS_UNUSED,
                  NULL);
    }
}

// Tunable version of TDS protocol to use

#if !defined(NCBI_CTLIB_TDS_VERSION)
#    define NCBI_CTLIB_TDS_VERSION 125
#endif

#define NCBI_CTLIB_TDS_FALLBACK_VERSION 110


NCBI_PARAM_DECL  (int, ctlib, TDS_VERSION);
NCBI_PARAM_DEF_EX(int, ctlib, TDS_VERSION,
                  NCBI_CTLIB_TDS_VERSION,  // default TDS version
                  eParam_NoThread,
                  CTLIB_TDS_VERSION);
typedef NCBI_PARAM_TYPE(ctlib, TDS_VERSION) TCtlibTdsVersion;


CS_INT GetCtlibTdsVersion(int version)
{
    if (version == 0) {
        version = TCtlibTdsVersion::GetDefault();
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

    ERR_POST(Warning <<
             "The version " << version << " of TDS protocol for "
             "the DBAPI CTLib driver is not supported. Falling back to "
             "the TDS protocol version " << fallback_version << ".");

    return GetCtlibTdsVersion(fallback_version);
}



///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

I_DriverContext* CTLIB_CreateContext(const map<string,string>* attr = 0)
{
    bool reuse_context = true;
    int  tds_version   = NCBI_CTLIB_TDS_VERSION;

    if ( attr ) {
        map<string,string>::const_iterator citer = attr->find("reuse_context");
        if ( citer != attr->end() ) {
            reuse_context = (citer->second != "false");
        }

        citer = attr->find("version");
        if (citer != attr->end())
            tds_version = NStr::StringToInt(citer->second);
    }

    CTLibContext* cntx = new CTLibContext(reuse_context,
                                          GetCtlibTdsVersion(tds_version));

    if ( cntx && attr ) {
        string page_size;
        map<string,string>::const_iterator citer = attr->find("packet");
        if ( citer != attr->end() ) {
            page_size = citer->second;
        }
        if ( !page_size.empty() ) {
            CS_INT s= atoi(page_size.c_str());
            cntx->CTLIB_SetPacketSize(s);
        }
        string prog_name;
        citer = attr->find("prog_name");
        if ( citer != attr->end() ) {
            prog_name = citer->second;
        }
        if ( !prog_name.empty() ) {
            cntx->CTLIB_SetApplicationName(prog_name);
        }
        string host_name;
        citer = attr->find("host_name");
        if ( citer != attr->end() ) {
            host_name = citer->second;
        }
        if ( !host_name.empty() ) {
            cntx->CTLIB_SetHostName(host_name);
        }
    }
    return cntx;
}

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
    TImplementation* drv = 0;
    if ( !driver.empty()  &&  driver != m_DriverName ) {
        return 0;
    }
    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        bool reuse_context = true;
        int  tds_version   = NCBI_CTLIB_TDS_VERSION;

        // Optional parameters ...
        CS_INT page_size = 0;
        string prog_name;
        string host_name;
        string client_charset;

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
                    tds_version = NStr::StringToInt( v.value );
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                } else if ( v.id == "prog_name" ) {
                    prog_name = v.value;
                } else if ( v.id == "host_name" ) {
                    host_name = v.value;
                } else if ( v.id == "client_charset" ) {
                    client_charset = v.value;
                }
            }
        }

        // Create a driver ...
        drv = new CTLibContext(reuse_context,
                               GetCtlibTdsVersion(tds_version));

        // Set parameters ...
        if ( page_size ) {
            drv->CTLIB_SetPacketSize( page_size );
        }

        if ( !prog_name.empty() ) {
            drv->CTLIB_SetApplicationName( prog_name );
        }

        if ( !host_name.empty() ) {
            drv->CTLIB_SetHostName( host_name );
        }

        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset );
        }
    }
    return drv;
}

///////////////////////////////////////////////////////////////////////////////
class CDbapiCtlibCF_Sybase : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_Sybase(void)
    : CDbapiCtlibCFBase("ctlib")
    {
    }
};

class CDbapiCtlibCF_ftds64_ctlib : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_ftds64_ctlib(void)
    : CDbapiCtlibCFBase("ftds64_ctlib")
    {
    }
};


///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiCtlibCF_Sybase>::NCBI_EntryPointImpl( info_list, method );
}

void
NCBI_EntryPoint_xdbapi_ftds64_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiCtlibCF_ftds64_ctlib>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
DBAPI_RegisterDriver_CTLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ctlib );
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds64_ctlib );
}

///////////////////////////////////////////////////////////////////////////////
NCBI_DBAPIDRIVER_CTLIB_EXPORT
void DBAPI_RegisterDriver_CTLIB(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("ctlib", CTLIB_CreateContext);
    DBAPI_RegisterDriver_CTLIB();
}

void DBAPI_RegisterDriver_CTLIB_old(I_DriverMgr& mgr)
{
    DBAPI_RegisterDriver_CTLIB( mgr );
}

extern "C" {
    NCBI_DBAPIDRIVER_CTLIB_EXPORT
    void* DBAPI_E_ctlib()
    {
    return (void*)DBAPI_RegisterDriver_CTLIB_old;
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.102  2006/12/08 19:40:06  ssikorsk
 * Error message about TDS protocol version was downgraded to a warning.
 *
 * Revision 1.101  2006/11/28 20:08:09  ssikorsk
 * Replaced NCBI_CATCH_ALL(kEmptyStr) with NCBI_CATCH_ALL(NCBI_CURRENT_FUNCTION)
 *
 * Revision 1.100  2006/11/24 20:18:38  ssikorsk
 * Implemented methods Drop, IsOpen, Open, Close of the ctlib::GetCTLContext.
 *
 * Revision 1.99  2006/11/22 20:50:58  ssikorsk
 * Implemented class ctlib::Connection.
 *
 * Revision 1.98  2006/10/05 19:51:50  ssikorsk
 * Moved connection logic from CTLibContext to CTL_Connection.
 *
 * Revision 1.97  2006/10/04 19:27:22  ssikorsk
 * Revamp code to use AutoArray where it is possible.
 *
 * Revision 1.96  2006/10/02 19:59:01  ssikorsk
 * Handle TDS version 46.
 *
 * Revision 1.95  2006/09/18 15:22:33  ssikorsk
 * Implemented methods GetLoginTimeout and GetTimeout with CTLibContext.
 *
 * Revision 1.94  2006/09/13 22:49:42  ssikorsk
 * CDriverContext --> impl::CDriverContext
 *
 * Revision 1.93  2006/09/13 19:53:21  ssikorsk
 * Revamp code to use CWinSock.
 *
 * Revision 1.92  2006/08/31 15:02:05  ssikorsk
 * Handle ClientCharset and locale.
 *
 * Revision 1.91  2006/08/23 20:45:38  ssikorsk
 * Initiolize/finalyze winsock on Windows in case of FreeTDS.
 *
 * Revision 1.90  2006/08/22 20:15:55  ssikorsk
 * Minor fixes in order to compile with FreeTDS ctlib.
 *
 * Revision 1.89  2006/08/17 06:33:06  vakatov
 * Switch default version of TDS protocol to 12.5 (from 11.0)
 *
 * Revision 1.88  2006/08/02 18:49:43  ssikorsk
 * winsock.h --> winsock2.h
 *
 * Revision 1.87  2006/08/02 15:17:41  ssikorsk
 * Implemented NCBI_EntryPoint_xdbapi_ftds64_ctlib.
 *
 * Revision 1.86  2006/07/28 15:00:45  ssikorsk
 * Revamp code to use CDB_Exception::SetSybaseSeverity.
 *
 * Revision 1.85  2006/07/20 19:55:07  ssikorsk
 * Added CTLibContext.m_TDSVersion.
 *
 * Revision 1.84  2006/07/20 16:31:07  vakatov
 * NCBI_CTLIB_TDS_VERSION -- preprocessor var to allow altering the
 * default version of TDS protocol (from the current default of 110)
 *
 * Revision 1.83  2006/07/13 16:08:12  ssikorsk
 * Timeout == 0 --> CS_NO_LIMIT.
 *
 * Revision 1.82  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.81  2006/07/05 16:08:09  ssikorsk
 * Revamp code to use GetCtlibTdsVersion function.
 *
 * Revision 1.80  2006/06/07 22:19:37  ssikorsk
 * Context finalization improvements.
 *
 * Revision 1.79  2006/06/05 14:49:31  ssikorsk
 * Set value of m_MsgHandlers in I_DriverContext::Create_Connection.
 *
 * Revision 1.78  2006/05/30 19:01:14  ssikorsk
 * Removed outdated comments.
 *
 * Revision 1.77  2006/05/30 18:48:13  ssikorsk
 * Revamp code to use server and user names with database exceptions;
 *
 * Revision 1.76  2006/05/18 16:57:41  ssikorsk
 * Assign values to m_LoginTimeout and m_Timeout.
 *
 * Revision 1.75  2006/05/15 19:41:00  ssikorsk
 * Fixed CTLibContext::x_SafeToFinalize in case of Unix.
 *
 * Revision 1.74  2006/05/11 18:13:43  ssikorsk
 * Fixed compilation issues
 *
 * Revision 1.73  2006/05/11 18:07:58  ssikorsk
 * Utilized new exception storage
 *
 * Revision 1.72  2006/05/10 14:46:38  ssikorsk
 * Implemented CCTLExceptions::CGuard;
 * Improved CCTLExceptions::Handle;
 *
 * Revision 1.71  2006/05/04 15:24:03  ucko
 * Modify CCTLExceptions to store pointers, as our exception classes don't
 * support assignment.
 * Take advantage of CNcbiOstrstreamToString rather than duplicating its logic.
 *
 * Revision 1.70  2006/05/04 14:34:56  ssikorsk
 * Call freeze(false) for ostrstream after getting a string value.
 *
 * Revision 1.69  2006/05/03 15:10:36  ssikorsk
 * Implemented classs CTL_Cmd and CCTLExceptions;
 * Surrounded each native ctlib call with Check;
 *
 * Revision 1.68  2006/04/13 15:11:56  ssikorsk
 * Fixed erasing of an element from a std::vector.
 *
 * Revision 1.67  2006/04/05 14:27:56  ssikorsk
 * Implemented CTLibContext::Close
 *
 * Revision 1.66  2006/03/15 19:57:09  ssikorsk
 * Replaced "static auto_ptr" with "static CSafeStaticPtr".
 *
 * Revision 1.65  2006/03/09 19:03:36  ssikorsk
 * Utilized method I_DriverContext:: CloseAllConn.
 *
 * Revision 1.64  2006/03/09 17:21:41  ssikorsk
 * Replaced database error severity eDiag_Fatal with eDiag_Critical.
 *
 * Revision 1.63  2006/03/07 17:24:21  ucko
 * +<algorithm> for find()
 *
 * Revision 1.62  2006/03/06 20:16:04  ssikorsk
 * Fixed singleton initialisation in CTLibContextRegistry::Instance.
 *
 * Revision 1.61  2006/03/06 19:51:38  ssikorsk
 *
 * Added method Close/CloseForever to all context/command-aware classes.
 * Use getters to access Sybase's context and command handles.
 *
 * Revision 1.60  2006/02/22 15:15:50  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.59  2006/02/15 22:53:49  soussov
 * removes filter for messages that have msgnum == 0 and severity == 0
 *
 * Revision 1.58  2006/02/08 17:25:07  ssikorsk
 * Treat messages with msg->severity == 10 && msg->msgnumber == 0 as
 * having informational severity.
 *
 * Revision 1.57  2006/02/07 17:24:24  ssikorsk
 * Added an extra space prior server name in the regular exception string.
 *
 * Revision 1.56  2006/02/06 16:21:11  ssikorsk
 *     Use ERR_POST instead of cerr to report error messages.
 *
 * Revision 1.55  2006/02/01 13:58:18  ssikorsk
 * Report server and user names in case of a failed connection attempt.
 *
 * Revision 1.54  2006/01/23 13:37:56  ssikorsk
 * Removed connection attribute check from CTLibContext::MakeIConnection;
 *
 * Revision 1.53  2006/01/03 19:01:25  ssikorsk
 * Implement method MakeConnection.
 *
 * Revision 1.52  2005/12/28 13:41:09  ssikorsk
 * Disable ct_exit/cs_ctx_drop on Windows
 *
 * Revision 1.51  2005/12/02 14:16:14  ssikorsk
 *  Log server and user names with error message.
 *
 * Revision 1.50  2005/11/02 15:59:31  ucko
 * Revert previous change in favor of supplying empty compilation info
 * via a static constant.
 *
 * Revision 1.49  2005/11/02 15:37:52  ucko
 * Replace CDiagCompileInfo() with DIAG_COMPILE_INFO, as the latter
 * automatically fills in some useful information and is less likely to
 * confuse compilers into thinking they're looking at function prototypes.
 *
 * Revision 1.48  2005/11/02 13:30:33  ssikorsk
 * Do not report function name, file name and line number in case of SQL Server errors.
 *
 * Revision 1.47  2005/10/27 16:48:49  grichenk
 * Redesigned CTreeNode (added search methods),
 * removed CPairTreeNode.
 *
 * Revision 1.46  2005/10/20 13:04:49  ssikorsk
 * Fixed:
 * CTLibContext::CTLIB_SetApplicationName
 * CTLibContext::CTLIB_SetHostName
 *
 * Revision 1.45  2005/10/04 13:44:35  ucko
 * Conditionalize uses of CS_VERSION_12x, as some configurations may be
 * using older client libraries that don't define them.
 *
 * Revision 1.44  2005/10/03 12:17:53  ssikorsk
 * Handle versions 11.0, 12.0 and 12.5 of the TDS protocol.
 *
 * Revision 1.43  2005/09/19 14:19:02  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.42  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.41  2005/07/18 17:01:10  ssikorsk
 * Export DBAPI_RegisterDriver_CTLIB(I_DriverMgr& mgr)
 *
 * Revision 1.40  2005/06/07 16:22:51  ssikorsk
 * Included <dbapi/driver/driver_mgr.hpp> to make CDllResolver_Getter<I_DriverContext> explicitly visible.
 *
 * Revision 1.39  2005/06/03 16:44:03  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.38  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.37  2005/03/21 14:08:30  ssikorsk
 * Fixed the 'version' of a databases protocol parameter handling
 *
 * Revision 1.36  2005/03/02 21:19:20  ssikorsk
 * Explicitly call a new RegisterDriver function from the old one
 *
 * Revision 1.35  2005/03/02 19:29:54  ssikorsk
 * Export new RegisterDriver function on Windows
 *
 * Revision 1.34  2005/03/01 15:22:55  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.33  2004/12/20 16:20:29  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.32  2004/05/17 21:12:03  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.31  2004/04/07 13:41:47  gorelenk
 * Added export prefix to implementations of DBAPI_E_* functions.
 *
 * Revision 1.30  2003/11/19 22:47:20  soussov
 * adds code to setup client/server msg callbacks for each connection; adds 'prog_name' and 'host_name' attributes for create context
 *
 * Revision 1.29  2003/11/14 20:46:13  soussov
 * implements DoNotConnect mode
 *
 * Revision 1.28  2003/10/29 21:45:54  soussov
 * adds code which prevents repeated timeouts if server is hanging
 *
 * Revision 1.27  2003/10/27 17:00:20  soussov
 * adds code to prevent the return of broken connection from the pool
 *
 * Revision 1.26  2003/07/21 22:00:36  soussov
 * fixes bug whith pool name mismatch in Connect()
 *
 * Revision 1.25  2003/07/17 20:45:44  soussov
 * connections pool improvements
 *
 * Revision 1.24  2003/05/27 21:12:03  soussov
 * bit type param added
 *
 * Revision 1.23  2003/05/15 21:54:30  soussov
 * fixed bug in timeout handling
 *
 * Revision 1.22  2003/04/29 21:15:35  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.21  2003/04/01 21:49:55  soussov
 * new attribute 'packet=XXX' (where XXX is a packet size) added to CTLIB_CreateContext
 *
 * Revision 1.20  2003/03/18 14:34:20  ivanov
 * Fix for Rev 1.18-1.19 -- #include's for gethostname() on NCBI_OS_MSWIN platform
 *
 * Revision 1.19  2003/03/17 20:57:09  ivanov
 * #include <unistd.h> everywhere except NCBI_OS_MSWIN platform
 *
 * Revision 1.18  2003/03/17 19:37:31  ucko
 * #include <unistd.h> for gethostname()
 *
 * Revision 1.17  2003/03/17 15:29:02  soussov
 * sets the default host name using gethostname()
 *
 * Revision 1.16  2002/12/20 17:53:56  soussov
 * renames the members of ECapability enum
 *
 * Revision 1.15  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.14  2002/06/28 22:49:02  garrett
 * CTLIB now connects under Windows.
 *
 * Revision 1.13  2002/04/19 15:01:28  soussov
 * adds static mutex to Connect
 *
 * Revision 1.12  2002/04/12 18:50:25  soussov
 * static mutex in context constructor added
 *
 * Revision 1.11  2002/03/26 15:34:38  soussov
 * new image/text operations added
 *
 * Revision 1.10  2002/01/17 22:07:10  soussov
 * changes driver registration
 *
 * Revision 1.9  2002/01/11 20:24:33  soussov
 * driver manager support added
 *
 * Revision 1.8  2001/11/06 18:02:00  lavr
 * Added methods formely inline (moved from header files)
 *
 * Revision 1.7  2001/10/12 21:21:55  lavr
 * Bugfix: Changed '=' -> '==' in severity parsing
 *
 * Revision 1.6  2001/10/11 16:30:44  soussov
 * exclude ctlib dependencies from numeric conversion calls
 *
 * Revision 1.5  2001/10/01 20:09:30  vakatov
 * Introduced a generic default user error handler and the means to
 * alternate it. Added an auxiliary error handler class
 * "CDB_UserHandler_Stream".
 * Moved "{Push/Pop}{Cntx/Conn}MsgHandler()" to the generic code
 * (in I_DriverContext).
 *
 * Revision 1.4  2001/09/27 20:08:33  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.3  2001/09/27 16:46:34  vakatov
 * Non-const (was const) exception object to pass to the user handler
 *
 * Revision 1.2  2001/09/26 23:23:31  vakatov
 * Moved the err.message handlers' stack functionality (generic storage
 * and methods) to the "abstract interface" level.
 *
 * Revision 1.1  2001/09/21 23:40:02  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
