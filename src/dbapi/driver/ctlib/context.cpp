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
 * File Description:  Driver for CTLib server
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <winsock.h>
#else
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContext::
//

CS_START_EXTERN_C
    CS_RETCODE CS_PUBLIC s_CTLIB_cserr_callback(CS_CONTEXT* context, CS_CLIENTMSG* msg)
    {
        return CTLibContext::CTLIB_cserr_handler(context, msg)
            ? CS_SUCCEED : CS_FAIL;
    }

    CS_RETCODE CS_PUBLIC s_CTLIB_cterr_callback(CS_CONTEXT* context, CS_CONNECTION* con,
                                      CS_CLIENTMSG* msg)
    {
        return CTLibContext::CTLIB_cterr_handler(context, con, msg)
            ? CS_SUCCEED : CS_FAIL;
    }

    CS_RETCODE CS_PUBLIC s_CTLIB_srverr_callback(CS_CONTEXT* context, CS_CONNECTION* con,
                                       CS_SERVERMSG* msg)
    {
        return CTLibContext::CTLIB_srverr_handler(context, con, msg)
            ? CS_SUCCEED : CS_FAIL;
    }
CS_END_EXTERN_C

CTLibContext::CTLibContext(bool reuse_context, CS_INT version)
{
    DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(xMutex);

    m_Context         = 0;
    m_AppName         = "CTLibDriver";
    m_LoginRetryCount = 0;
    m_LoginLoopDelay  = 0;
    m_PacketSize      = 2048;

    CS_RETCODE r = reuse_context ? cs_ctx_global(version, &m_Context) :
        cs_ctx_alloc(version, &m_Context);
    if (r != CS_SUCCEED) {
        m_Context = 0;
        throw CDB_ClientEx(eDB_Fatal, 100001, "CTLibContext::CTLibContext",
                           "Can not allocate a context");
    }

    CS_VOID*     cb;
    CS_INT       outlen;
    CPointerPot* p_pot = 0;

    // check if cs message callback is already installed
    r = cs_config(m_Context, CS_GET, CS_MESSAGE_CB, &cb, CS_UNUSED, &outlen);
    if (r != CS_SUCCEED) {
        m_Context = 0;
        throw CDB_ClientEx(eDB_Error, 100006, "CTLibContext::CTLibContext",
                           "cs_config failed");
    }

    if (cb == (CS_VOID*)  s_CTLIB_cserr_callback) {
        // we did use this context already
        r = cs_config(m_Context, CS_GET, CS_USERDATA,
                      (CS_VOID*) &p_pot, (CS_INT) sizeof(p_pot), &outlen);
        if (r != CS_SUCCEED) {
            m_Context = 0;
            throw CDB_ClientEx(eDB_Error, 100006, "CTLibContext::CTLibContext",
                               "cs_config failed");
        }
    }
    else {
        // this is a brand new context
        r = cs_config(m_Context, CS_SET, CS_MESSAGE_CB,
                      (CS_VOID*) s_CTLIB_cserr_callback, CS_UNUSED, NULL);
        if (r != CS_SUCCEED) {
            cs_ctx_drop(m_Context);
            m_Context = 0;
            throw CDB_ClientEx(eDB_Error, 100005, "CTLibContext::CTLibContext",
                               "Can not install the cslib message callback");
        }

        p_pot = new CPointerPot;
        r = cs_config(m_Context, CS_SET, CS_USERDATA,
                      (CS_VOID*) &p_pot, (CS_INT) sizeof(p_pot), NULL);
        if (r != CS_SUCCEED) {
            cs_ctx_drop(m_Context);
            m_Context = 0;
            delete p_pot;
            throw CDB_ClientEx(eDB_Error, 100007, "CTLibContext::CTLibContext",
                               "Can not install the user data");
        }

        r = ct_init(m_Context, version);
        if (r != CS_SUCCEED) {
            cs_ctx_drop(m_Context);
            m_Context = 0;
            delete p_pot;
            throw CDB_ClientEx(eDB_Error, 100002, "CTLibContext::CTLibContext",
                               "ct_init failed");
        }

        r = ct_callback(m_Context, NULL, CS_SET, CS_CLIENTMSG_CB,
                        (CS_VOID*) s_CTLIB_cterr_callback);
        if (r != CS_SUCCEED) {
            ct_exit(m_Context, CS_FORCE_EXIT);
            cs_ctx_drop(m_Context);
            m_Context = 0;
            delete p_pot;
            throw CDB_ClientEx(eDB_Error, 100003, "CTLibContext::CTLibContext",
                               "Can not install the client message callback");
        }

        r = ct_callback(m_Context, NULL, CS_SET, CS_SERVERMSG_CB,
                        (CS_VOID*) s_CTLIB_srverr_callback);
        if (r != CS_SUCCEED) {
            ct_exit(m_Context, CS_FORCE_EXIT);
            cs_ctx_drop(m_Context);
            m_Context = 0;
            delete p_pot;
            throw CDB_ClientEx(eDB_Error, 100004, "CTLibContext::CTLibContext",
                               "Can not install the server message callback");
        }
    }

    if ( p_pot ) {
        p_pot->Add((TPotItem) this);
    }
}


bool CTLibContext::SetLoginTimeout(unsigned int nof_secs)
{
    CS_INT t_out = (CS_INT) nof_secs;
    return ct_config(m_Context, CS_SET,
                     CS_LOGIN_TIMEOUT, &t_out, CS_UNUSED, NULL) == CS_SUCCEED;
}


bool CTLibContext::SetTimeout(unsigned int nof_secs)
{
    CS_INT t_out = (CS_INT) nof_secs;
    return ct_config(m_Context, CS_SET,
                     CS_TIMEOUT, &t_out, CS_UNUSED, NULL) == CS_SUCCEED;
}


bool CTLibContext::SetMaxTextImageSize(size_t nof_bytes)
{
    CS_INT ti_size = (CS_INT) nof_bytes;
    return ct_config(m_Context, CS_SET,
                     CS_TEXTLIMIT, &ti_size, CS_UNUSED, NULL) == CS_SUCCEED;
}


CDB_Connection* CTLibContext::Connect(const string&   srv_name,
                                      const string&   user_name,
                                      const string&   passwd,
                                      TConnectionMode mode,
                                      bool            reusable,
                                      const string&   pool_name)
{
    //DEFINE_STATIC_FAST_MUTEX(xMutex);
    // CFastMutexGuard mg(xMutex);
    CFastMutexGuard mg(m_Mtx);

    if (reusable  &&  m_NotInUse.NofItems() > 0) {
        // try to get a connection from the pot
        if ( !pool_name.empty() ) {
            // use a pool name
            for (int i = m_NotInUse.NofItems();  i--; ) {
                CTL_Connection* t_con
                    = static_cast<CTL_Connection*> (m_NotInUse.Get(i));

                if (pool_name.compare(t_con->PoolName()) == 0) {
                    m_NotInUse.Remove(i);
                    if(t_con->Refresh()) {
                        m_InUse.Add((TPotItem) t_con);
                        return Create_Connection(*t_con);
                    }
                    delete t_con;
                }
            }
        }
        else { // using server name as a pool name

            if ( srv_name.empty() )
                return 0;

            // try to use a server name
            for (int i = m_NotInUse.NofItems();  i--; ) {
                CTL_Connection* t_con
                    = static_cast<CTL_Connection*> (m_NotInUse.Get(i));

                if (srv_name.compare(t_con->ServerName()) == 0) {
                    m_NotInUse.Remove(i);
                    if(t_con->Refresh()) {
                        m_InUse.Add((TPotItem) t_con);
                        return Create_Connection(*t_con);
                    }
                    delete t_con;
                }
            }
        }
    }

    if((mode & fDoNotConnect) != 0) return 0;

    // new connection needed
    if (srv_name.empty()  ||  user_name.empty()  ||  passwd.empty()) {
        throw CDB_ClientEx(eDB_Error, 100010, "CTLibContext::Connect",
                           "You have to provide server name, user name and "
                           "password to connect to the server");
    }

    CS_CONNECTION* con = x_ConnectToServer(srv_name, user_name, passwd, mode);
    if (con == 0) {
        throw CDB_ClientEx(eDB_Error, 100011, "CTLibContext::Connect",
                           "Can not connect to the server");
    }

    CTL_Connection* t_con = new CTL_Connection(this, con, reusable, pool_name);
    t_con->m_MsgHandlers = m_ConnHandlers;

    m_InUse.Add((TPotItem) t_con);

    return Create_Connection(*t_con);
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

CTLibContext::~CTLibContext()
{
    CFastMutexGuard mg(m_Mtx);

    if ( !m_Context ) {
        return;
    }

    // close all connections first
    for (int i = m_NotInUse.NofItems();  i--; ) {
        CTL_Connection* t_con = static_cast<CTL_Connection*>(m_NotInUse.Get(i));
        delete t_con;
    }

    for (int i = m_InUse.NofItems();  i--; ) {
        CTL_Connection* t_con = static_cast<CTL_Connection*> (m_InUse.Get(i));
        delete t_con;
    }

    CS_INT       outlen;
    CPointerPot* p_pot = 0;

    if (cs_config(m_Context, CS_GET, CS_USERDATA,
                  (void*) &p_pot, (CS_INT) sizeof(p_pot), &outlen) == CS_SUCCEED
        &&  p_pot != 0) {
        p_pot->Remove(this);
        if (p_pot->NofItems() == 0) { // this is a last driver for this context
            delete p_pot;
            if (ct_exit(m_Context, CS_UNUSED) != CS_SUCCEED) {
                ct_exit(m_Context, CS_FORCE_EXIT);
            }
            cs_ctx_drop(m_Context);
        }
    }
}


void CTLibContext::CTLIB_SetApplicationName(const string& a_name)
{
    m_AppName = a_name;
}


void CTLibContext::CTLIB_SetHostName(const string& host_name)
{
    m_HostName = host_name;
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


bool CTLibContext::CTLIB_cserr_handler(CS_CONTEXT* context, CS_CLIENTMSG* msg)
{
    CS_INT       outlen;
    CPointerPot* p_pot = 0;
    CS_RETCODE   r;

    r = cs_config(context, CS_GET, CS_USERDATA,
                  (void*) &p_pot, (CS_INT) sizeof(p_pot), &outlen);

    if (r == CS_SUCCEED  &&  p_pot != 0  &&  p_pot->NofItems() > 0) {
        CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
        EDB_Severity sev = eDB_Error;
        if (msg->severity == CS_SV_INFORM) {
            sev = eDB_Info;
        }
        else if (msg->severity == CS_SV_FATAL) {
            sev = eDB_Fatal;
        }

        CDB_ClientEx ex(sev, msg->msgnumber, "cslib", msg->msgstring);
        drv->m_CntxHandlers.PostMsg(&ex);
    }
    else if (msg->severity != CS_SV_INFORM) {
        // nobody can be informed, so put it to stderr
        cerr << "CSLIB error handler detects the following error" << endl
             << msg->msgstring << endl;
    }

    return true;
}


bool CTLibContext::CTLIB_cterr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                       CS_CLIENTMSG* msg)
{
    CS_INT           outlen;
    CPointerPot*        p_pot = 0;
    CTL_Connection*  link = 0;
    CDBHandlerStack* hs;

    if (con != 0  &&
        ct_con_props(con, CS_GET, CS_USERDATA,
                     (void*) &link, (CS_INT) sizeof(link),
                     &outlen) == CS_SUCCEED  &&  link != 0) {
        hs = &link->m_MsgHandlers;
    }
    else if (cs_config(context, CS_GET, CS_USERDATA,
                       (void*) &p_pot, (CS_INT) sizeof(p_pot),
                       &outlen) == CS_SUCCEED  &&
             p_pot != 0  &&  p_pot->NofItems() > 0) {
        CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
        hs = &drv->m_CntxHandlers;
    }
    else {
        if (msg->severity != CS_SV_INFORM) {
            // nobody can be informed, let's put it in stderr
            cerr << "CTLIB error handler detects the following error" << endl
                 << "Severity:" << msg->severity
                 << " Msg # " << msg->msgnumber << endl
                 << msg->msgstring << endl;
            if (msg->osstringlen > 1) {
                cerr << "OS # "    << msg->osnumber
                     << " OS msg " << msg->osstring << endl;
            }
            if (msg->sqlstatelen > 1  &&
                (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
                cerr << "SQL: " << msg->sqlstate << endl;
            }
        }
        return true;
    }


    switch (msg->severity) {
    case CS_SV_INFORM: {
        CDB_ClientEx info(eDB_Info,
                          (int) msg->msgnumber, "ctlib", msg->msgstring);
        hs->PostMsg(&info);
        break;
    }
    case CS_SV_RETRY_FAIL: {
        CDB_TimeoutEx to((int) msg->msgnumber, "ctlib", msg->msgstring);
        hs->PostMsg(&to);
    if(con) {
      CS_INT status;
      if((ct_con_props(con, CS_GET, CS_LOGIN_STATUS, (CS_VOID*)&status, CS_UNUSED, NULL) != CS_SUCCEED) ||
       (!status)) return false;

      if(ct_cancel(con, (CS_COMMAND*)0, CS_CANCEL_ATTN) != CS_SUCCEED) return false;
    }
    else return false;
    break;
    }
    case CS_SV_CONFIG_FAIL:
    case CS_SV_API_FAIL:
    case CS_SV_INTERNAL_FAIL: {
        CDB_ClientEx err(eDB_Error,
                         (int) msg->msgnumber, "ctlib", msg->msgstring);
        hs->PostMsg(&err);
        break;
    }
    default: {
        CDB_ClientEx ftl(eDB_Fatal,
                         (int) msg->msgnumber, "ctlib", msg->msgstring);
        hs->PostMsg(&ftl);
        break;
    }
    }

    return true;
}


bool CTLibContext::CTLIB_srverr_handler(CS_CONTEXT* context,
                                        CS_CONNECTION* con,
                                        CS_SERVERMSG* msg)
{
    if ((msg->severity == 0  &&  msg->msgnumber == 0)  ||
        msg->msgnumber == 5701  ||  msg->msgnumber == 5703) {
        return true;
    }

    CS_INT           outlen;
    CPointerPot*     p_pot = 0;
    CTL_Connection*  link = 0;
    CDBHandlerStack* hs;

    if (con != 0  &&  ct_con_props(con, CS_GET, CS_USERDATA,
                                   (void*) &link, (CS_INT) sizeof(link),
                                   &outlen) == CS_SUCCEED  &&
        link != 0) {
        hs = &link->m_MsgHandlers;
    }
    else if (cs_config(context, CS_GET, CS_USERDATA,
                       (void*) &p_pot, (CS_INT) sizeof(p_pot),
                       &outlen) == CS_SUCCEED  &&
             p_pot != 0  &&  p_pot->NofItems() > 0) {
        CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
        hs = &drv->m_CntxHandlers;
    }
    else {
        cerr << "Message from the server ";
        if (msg->svrnlen > 0) {
            cerr << "<" << msg->svrname << "> ";
        }
        cerr << "msg # " << msg->msgnumber
             << " severity: " << msg->severity << endl;
        if (msg->proclen > 0) {
            cerr << "Proc: " << msg->proc << " line: " << msg->line << endl;
        }
        if (msg->sqlstatelen > 1  &&
            (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
            cerr << "SQL: " << msg->sqlstate << endl;
        }
        cerr << msg->text << endl;
        return true;
    }

    if (msg->msgnumber == 1205 /*DEADLOCK*/) {
        CDB_DeadlockEx dl(msg->svrname, msg->text);
        hs->PostMsg(&dl);
    }
    else {
        EDB_Severity sev =
            msg->severity <  10 ? eDB_Info :
            msg->severity == 10 ? eDB_Warning :
            msg->severity <  16 ? eDB_Error :
            msg->severity >  16 ? eDB_Fatal :
            eDB_Unknown;

        if (msg->proclen > 0) {
            CDB_RPCEx rpc(sev, (int) msg->msgnumber, msg->svrname, msg->text,
                          msg->proc, (int) msg->line);
            hs->PostMsg(&rpc);
        }
        else if (msg->sqlstatelen > 1  &&
                 (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
            CDB_SQLEx sql(sev, (int) msg->msgnumber, msg->svrname, msg->text,
                          (const char*) msg->sqlstate, (int) msg->line);
            hs->PostMsg(&sql);
        }
        else {
            CDB_DSEx m(sev, (int) msg->msgnumber, msg->svrname, msg->text);
            hs->PostMsg(&m);
        }
    }

    return true;
}



CS_CONNECTION* CTLibContext::x_ConnectToServer(const string&   srv_name,
                                               const string&   user_name,
                                               const string&   passwd,
                                               TConnectionMode mode)
{
    CS_CONNECTION* con;
    if (ct_con_alloc(m_Context, &con) != CS_SUCCEED)
        return 0;

    ct_callback(NULL, con, CS_SET, CS_CLIENTMSG_CB,
                (CS_VOID*) s_CTLIB_cterr_callback);

    ct_callback(NULL, con, CS_SET, CS_SERVERMSG_CB,
                (CS_VOID*) s_CTLIB_srverr_callback);

    char hostname[256];
    if(gethostname(hostname, 256)) {
      strcpy(hostname, "UNKNOWN");
    }
    else hostname[255]= '\0';


    if (ct_con_props(con, CS_SET, CS_USERNAME, (void*) user_name.c_str(),
                     CS_NULLTERM, NULL) != CS_SUCCEED  ||
        ct_con_props(con, CS_SET, CS_PASSWORD, (void*) passwd.c_str(),
                     CS_NULLTERM, NULL) != CS_SUCCEED  ||
        ct_con_props(con, CS_SET, CS_APPNAME, (void*) m_AppName.c_str(),
                     CS_NULLTERM, NULL) != CS_SUCCEED ||
        ct_con_props(con, CS_SET, CS_HOSTNAME, (void*) hostname,
                     CS_NULLTERM, NULL) != CS_SUCCEED) {
        ct_con_drop(con);
        return 0;
    }

    if ( !m_HostName.empty() ) {
        ct_con_props(con, CS_SET, CS_HOSTNAME,
                     (void*) m_HostName.c_str(), CS_NULLTERM, NULL);
    }

    if (m_PacketSize > 0) {
        ct_con_props(con, CS_SET, CS_PACKETSIZE,
                     (void*) &m_PacketSize, CS_UNUSED, NULL);
    }

    if (m_LoginRetryCount > 0) {
        ct_con_props(con, CS_SET, CS_RETRY_COUNT,
                     (void*) &m_LoginRetryCount, CS_UNUSED, NULL);
    }

    if (m_LoginLoopDelay > 0) {
        ct_con_props(con, CS_SET, CS_LOOP_DELAY,
                     (void*) &m_LoginLoopDelay, CS_UNUSED, NULL);
    }

    CS_BOOL flag = CS_TRUE;
    if ((mode & fBcpIn) != 0) {
        ct_con_props(con, CS_SET, CS_BULK_LOGIN, &flag, CS_UNUSED, NULL);
    }
    if ((mode & fPasswordEncrypted) != 0) {
        ct_con_props(con, CS_SET, CS_SEC_ENCRYPTION, &flag, CS_UNUSED, NULL);
    }

    if (ct_connect(con, const_cast<char*> (srv_name.c_str()), CS_NULLTERM)
        != CS_SUCCEED) {
        ct_con_drop(con);
        return 0;
    }

    return con;
}


/////////////////////////////////////////////////////////////////////////////
//
//  Miscellaneous
//

void g_CTLIB_GetRowCount(CS_COMMAND* cmd, int* cnt)
{
    CS_INT n;
    CS_INT outlen;
    if (cnt  &&
        ct_res_info(cmd, CS_ROW_COUNT, &n, CS_UNUSED, &outlen) == CS_SUCCEED
        && n >= 0  &&  n != CS_NO_COUNT) {
        *cnt = (int) n;
    }
}


bool g_CTLIB_AssignCmdParam(CS_COMMAND*   cmd,
                            CDB_Object&   param,
                            const string& param_name,
                            CS_DATAFMT&   param_fmt,
                            CS_SMALLINT   indicator,
                            bool          declare_only)
{
    {{
        size_t l = param_name.copy(param_fmt.name, CS_MAX_NAME-1);
        param_fmt.name[l] = '\0';
    }}


    CS_RETCODE ret_code;

    switch ( param.GetType() ) {
    case eDB_Int: {
        CDB_Int& par = dynamic_cast<CDB_Int&> (param);
        param_fmt.datatype = CS_INT_TYPE;
        if ( declare_only )
            break;

        CS_INT value = (CS_INT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_SmallInt: {
        CDB_SmallInt& par = dynamic_cast<CDB_SmallInt&> (param);
        param_fmt.datatype = CS_SMALLINT_TYPE;
        if ( declare_only )
            break;

        CS_SMALLINT value = (CS_SMALLINT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_TinyInt: {
        CDB_TinyInt& par = dynamic_cast<CDB_TinyInt&> (param);
        param_fmt.datatype = CS_TINYINT_TYPE;
        if ( declare_only )
            break;

        CS_TINYINT value = (CS_TINYINT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_Bit: {
        CDB_Bit& par = dynamic_cast<CDB_Bit&> (param);
        param_fmt.datatype = CS_BIT_TYPE;
        if ( declare_only )
            break;

        CS_BIT value = (CS_BIT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_BigInt: {
        CDB_BigInt& par = dynamic_cast<CDB_BigInt&> (param);
        param_fmt.datatype = CS_NUMERIC_TYPE;
        if ( declare_only )
            break;

        CS_NUMERIC value;
        Int8 v8 = par.Value();
        memset(&value, 0, sizeof(value));
        value.precision= 18;
        if (longlong_to_numeric(v8, 18, value.array) == 0)
            return false;

        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_Char: {
        CDB_Char& par = dynamic_cast<CDB_Char&> (param);
        param_fmt.datatype = CS_CHAR_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_LongChar: {
        CDB_LongChar& par = dynamic_cast<CDB_LongChar&> (param);
        param_fmt.datatype = CS_LONGCHAR_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_VarChar: {
        CDB_VarChar& par = dynamic_cast<CDB_VarChar&> (param);
        param_fmt.datatype = CS_CHAR_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_Binary: {
        CDB_Binary& par = dynamic_cast<CDB_Binary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_LongBinary: {
        CDB_LongBinary& par = dynamic_cast<CDB_LongBinary&> (param);
        param_fmt.datatype = CS_LONGBINARY_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_VarBinary: {
        CDB_VarBinary& par = dynamic_cast<CDB_VarBinary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_Float: {
        CDB_Float& par = dynamic_cast<CDB_Float&> (param);
        param_fmt.datatype = CS_REAL_TYPE;
        if ( declare_only )
            break;

        CS_REAL value = (CS_REAL) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_Double: {
        CDB_Double& par = dynamic_cast<CDB_Double&> (param);
        param_fmt.datatype = CS_FLOAT_TYPE;
        if ( declare_only )
            break;

        CS_FLOAT value = (CS_FLOAT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_SmallDateTime: {
        CDB_SmallDateTime& par = dynamic_cast<CDB_SmallDateTime&> (param);
        param_fmt.datatype = CS_DATETIME4_TYPE;
        if ( declare_only )
            break;

        CS_DATETIME4 dt;
        dt.days    = par.GetDays();
        dt.minutes = par.GetMinutes();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &dt, CS_UNUSED, indicator);
        break;
    }
    case eDB_DateTime: {
        CDB_DateTime& par = dynamic_cast<CDB_DateTime&> (param);
        param_fmt.datatype = CS_DATETIME_TYPE;
        if ( declare_only )
            break;

        CS_DATETIME dt;
        dt.dtdays = par.GetDays();
        dt.dttime = par.Get300Secs();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &dt, CS_UNUSED, indicator);
        break;
    }
    default: {
        return false;
    }
    }

    if ( declare_only ) {
        ret_code = ct_param(cmd, &param_fmt, 0, CS_UNUSED, 0);
    }

    return (ret_code == CS_SUCCEED);
}



///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

I_DriverContext* CTLIB_CreateContext(const map<string,string>* attr = 0)
{
    bool reuse_context= true;
    CS_INT version= CS_VERSION_110;

    if ( attr ) {
        map<string,string>::const_iterator citer = attr->find("reuse_context");
        if ( citer != attr->end() ) {
            reuse_context = (citer->second != "false");
        }
        string vers;
        citer = attr->find("version");
        if ( citer != attr->end() ) {
            vers = citer->second;
        }

        if ( vers.find("100") != string::npos ) {
            version= CS_VERSION_100;
        } else {
            char* e;
            long v= strtol(vers.c_str(), &e, 10);
            if ( v > 0 && (e == 0 || (!isalpha(*e))) ) version= v;
        }
    }
    CTLibContext* cntx= new CTLibContext(reuse_context, version);
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

void DBAPI_RegisterDriver_CTLIB(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("ctlib", CTLIB_CreateContext);
}

extern "C" {
    NCBI_DBAPIDRIVER_CTLIB_EXPORT
    void* DBAPI_E_ctlib()
    {
    return (void*)DBAPI_RegisterDriver_CTLIB;
    }
}

///////////////////////////////////////////////////////////////////////////////
const string kDBAPI_CTLIB_DriverName("ctlib");

class CDbapiCtlibCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CTLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CTLibContext> TParent;

public:
    CDbapiCtlibCF2(void);
    ~CDbapiCtlibCF2(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiCtlibCF2::CDbapiCtlibCF2(void)
    : TParent( kDBAPI_CTLIB_DriverName, 0 )
{
    return ;
}

CDbapiCtlibCF2::~CDbapiCtlibCF2(void)
{
    return ;
}

CDbapiCtlibCF2::TInterface*
CDbapiCtlibCF2::CreateInstance(
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
        CS_INT version = CS_VERSION_110;

        // Optional parameters ...
        CS_INT page_size = 0;
        string prog_name;
        string host_name;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TTreeValueType TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "reuse_context" ) {
                    reuse_context = (v.value != "false");
                } else if ( v.id == "version" ) {
                    version = NStr::StringToInt( v.value );
                    if ( version == 100 ) {
                        version = CS_VERSION_100;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                } else if ( v.id == "prog_name" ) {
                    prog_name = v.value;
                } else if ( v.id == "host_name" ) {
                    host_name = v.value;
                }
            }
        }

        // Create a driver ...
        drv = new CTLibContext(reuse_context, version);

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
    }
    return drv;
}

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiCtlibCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
DBAPI_RegisterDriver_CTLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ctlib );
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
