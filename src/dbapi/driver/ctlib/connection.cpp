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
 * File Description:  CTLib connection
 *
 */

#include <ncbi_pch.hpp>

#include "ctlib_utils.hpp"

#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/error_codes.hpp>

#include <string.h>
#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#  include <io.h>
inline int close(int fd)
{
    return _close(fd);
}
#else
#  include <unistd.h>
#endif

#ifdef FTDS_IN_USE
#  include <ctlib.h>
#endif

#include "../dbapi_driver_exception_storage.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_CTlib_Conn


BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
namespace ftds64_ctlib
{


CDBConnParams::EServerType 
GetTDSServerType(CS_CONNECTION* conn)
{
    if (conn != NULL && conn->tds_socket != NULL) {
        const char* product = conn->tds_socket->product_name;

        if (product != NULL && strlen(product) != 0) {
            if (strcmp(product, "sql server") == 0) {
                return CDBConnParams::eSybaseSQLServer;
            } else if (strcmp(product, "ASE") == 0) {
                return CDBConnParams::eSybaseSQLServer;
            } else if (strcmp(product, "Microsoft SQL Server") == 0) {
                return CDBConnParams::eMSSqlServer;
            } else if (strcmp(product, "OpenServer") == 0
                || strcmp(product, "NcbiTdsServer")) {
                return CDBConnParams::eSybaseOpenServer;
            }
        }
    }

    return CDBConnParams::eUnknown;
}

#endif

////////////////////////////////////////////////////////////////////////////
CTL_Connection::CTL_Connection(CTLibContext& cntx,
                               const CDBConnParams& params)
: impl::CConnection(cntx, params, true)
, m_Cntx(&cntx)
, m_ActiveCmd(NULL)
, m_Handle(cntx, *this)
{
#ifdef FTDS_IN_USE
    int tds_version = 0;

    if (params.GetProtocolVersion()) {
        // Use current TDS version if available...
        tds_version = params.GetProtocolVersion();
    } else {
        tds_version = GetCTLibContext().GetTDSVersion();
    }
    
    switch (tds_version) {
        case 50:
            tds_version = CS_TDS_50;
            break;
        case 70:
            tds_version = CS_TDS_70;
            break;
        case 80:
            tds_version = CS_TDS_80;
            break;
        case 125:
            tds_version = CS_TDS_50;
            break;
        case CS_VERSION_110:
            tds_version = CS_TDS_50;
            break;
        case 40:
        case 42:
        case 46:
        case CS_VERSION_100:
            DATABASE_DRIVER_ERROR("FTDS driver does not support TDS protocol "
                                  "version other than 5.0 or 7.0." + GetDbgInfo(),
                                  300011 );
            break;
    }
#endif

    string extra_msg = " SERVER: " + params.GetServerName() + 
        "; USER: " + params.GetUserName();
    SetExtraMsg(extra_msg);

    CheckWhileOpening(ct_callback(NULL,
                           x_GetSybaseConn(),
                           CS_SET,
                           CS_CLIENTMSG_CB,
                           (CS_VOID*) CTLibContext::CTLIB_cterr_handler));

    CheckWhileOpening(ct_callback(NULL,
                           x_GetSybaseConn(),
                           CS_SET,
                           CS_SERVERMSG_CB,
                           (CS_VOID*) CTLibContext::CTLIB_srverr_handler));

    char hostname[256];

    if(gethostname(hostname, 256)) {
      strcpy(hostname, "UNKNOWN");
    } else {
        hostname[255] = '\0';
    }


    if (CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_USERNAME,
                                (void*) params.GetUserName().data(),
                                params.GetUserName().size(),
                                NULL)) != CS_SUCCEED
        || CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                                   CS_SET,
                                   CS_PASSWORD,
                                   (void*) params.GetPassword().data(),
                                   params.GetPassword().size(),
                                   NULL)) != CS_SUCCEED
        || CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                                   CS_SET,
                                   CS_APPNAME,
                                   (void*) GetCDriverContext().GetApplicationName().data(),
                                   GetCDriverContext().GetApplicationName().size(),
                                   NULL)) != CS_SUCCEED
        || CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                                   CS_SET,
                                   CS_HOSTNAME,
                                   (void*) hostname,
                                   CS_NULLTERM,
                                   NULL)) != CS_SUCCEED
#ifdef FTDS_IN_USE
        || CheckWhileOpening(ct_con_props(
                                   x_GetSybaseConn(), 
                                   CS_SET, 
                                   CS_TDS_VERSION, 
                                   &tds_version,
                                   CS_UNUSED, 
                                   NULL)
                                  ) != CS_SUCCEED
#endif
        )
    {
        DATABASE_DRIVER_ERROR("Cannot set connection's properties."
                              + GetDbgInfo(), 100011);
    }

    if (cntx.GetLocale()) {
        if (Check(ct_con_props(x_GetSybaseConn(),
                               CS_SET,
                               CS_LOC_PROP,
                               (void*) cntx.GetLocale(),
                               CS_UNUSED,
                               NULL)) != CS_SUCCEED
            ) {
            DATABASE_DRIVER_ERROR( "Cannot set a connection locale." + GetDbgInfo(), 100011 );
        }
    }
//     if ( !GetCDriverContext().GetHostName().empty() ) {
//         CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
//                                 CS_SET,
//                                 CS_HOSTNAME,
//                                 (void*) GetCDriverContext().GetHostName().data(),
//                                 GetCDriverContext().GetHostName().size(),
//                                 NULL));
//     }

    if (cntx.GetPacketSize() > 0) {
        CS_INT packet_size = cntx.GetPacketSize();

        CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_PACKETSIZE,
                                (void*) &packet_size,
                                CS_UNUSED,
                                NULL));
    }

#if defined(CS_RETRY_COUNT)
    if (cntx.GetLoginRetryCount() > 0) {
        CS_INT login_retry_count = cntx.GetLoginRetryCount();

        CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_RETRY_COUNT,
                                (void*) &login_retry_count,
                                CS_UNUSED,
                                NULL));
    }
#endif

#if defined (CS_LOOP_DELAY)
    if (cntx.GetLoginLoopDelay() > 0) {
        CS_INT login_loop_delay = cntx.GetLoginLoopDelay();

        CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_LOOP_DELAY,
                                (void*) &login_loop_delay,
                                CS_UNUSED,
                                NULL));
    }
#endif

    CS_BOOL flag = CS_TRUE;
    CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                            CS_SET,
                            CS_BULK_LOGIN,
                            &flag,
                            CS_UNUSED,
                            NULL));

    if (params.GetParam("secure_login") == "true") {
        CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_SEC_ENCRYPTION,
                                &flag,
                                CS_UNUSED,
                                NULL));
    }

    CTL_Connection* link = this;
    CheckWhileOpening(ct_con_props(x_GetSybaseConn(),
                            CS_SET,
                            CS_USERDATA,
                            &link,
                            (CS_INT) sizeof(link),
                            NULL));

    if (!m_Handle.Open(params)) {
        string err;

        err += "Cannot connect to the server '" + params.GetServerName();
        err += "' as user '" + params.GetUserName() + "'";
        DATABASE_DRIVER_ERROR( err, 100011 );
    }

    /* Future development ...
    if (!params.GetDatabaseName().empty()) {
        const string sql = "use " + params.GetDatabaseName();

        auto_ptr<CDB_LangCmd> auto_stmt(LangCmd(sql));
        auto_stmt->Send();
        auto_stmt->DumpResults();
    }
    */

#ifdef FTDS_IN_USE
    SetServerType(GetTDSServerType(m_Handle.GetNativeHandle()));
#endif
}


#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), *this, GetBindParams())
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.


CS_RETCODE
CTL_Connection::Check(CS_RETCODE rc)
{
    GetCTLExceptionStorage().Handle(GetMsgHandlers(), GetExtraMsg(), this,
                                    GetBindParams());

    return rc;
}


CS_RETCODE
CTL_Connection::Check(CS_RETCODE rc, const string& extra_msg)
{
    GetCTLExceptionStorage().Handle(GetMsgHandlers(), extra_msg, this,
                                    GetBindParams());

    return rc;
}

CS_RETCODE
CTL_Connection::CheckWhileOpening(CS_RETCODE rc)
{
    const impl::CDBHandlerStack& handlers = GetOpeningMsgHandlers();
    if (handlers.GetSize() > 0) {
        GetCTLExceptionStorage().Handle(handlers, GetExtraMsg(), this,
                                        GetBindParams());
        return rc;
    } else {
        return GetCTLibContext().Check(rc);
    }
}

CS_RETCODE CTL_Connection::CheckSFB(CS_RETCODE rc,
                                    const char* msg,
                                    unsigned int msg_num)
{
    switch (Check(rc)) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        DATABASE_DRIVER_ERROR(msg, msg_num);
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    }

    return rc;
}


CS_INT
CTL_Connection::GetBLKVersion(void) const
{
    CS_INT blk_version = BLK_VERSION_100;

    _ASSERT(m_Cntx);
    switch (m_Cntx->GetTDSVersion()) {
    case CS_VERSION_100:
        blk_version = BLK_VERSION_100;
        break;
    case CS_VERSION_110: // Same as CS_VERSION_120
        blk_version = BLK_VERSION_110;
        break;
#ifdef CS_VERSION_125
    case CS_VERSION_125:
        blk_version = BLK_VERSION_125;
        break;
#endif
    }

    return blk_version;
}

bool CTL_Connection::IsAlive()
{
    return m_Handle.IsAlive();
}


void 
CTL_Connection::SetTimeout(size_t nof_secs)
{
    // DATABASE_DRIVER_ERROR( "SetTimeout is not supported.", 100011 );
#ifdef FTDS_IN_USE
    x_GetSybaseConn()->tds_socket->query_timeout = nof_secs;
#else
    _TRACE("SetTimeout is not supported.");
#endif
}


void
CTL_Connection::SetCancelTimeout(size_t nof_secs)
{
    GetCTLibContext().SetCancelTimeout(nof_secs);
}


size_t
CTL_Connection::PrepareToCancel(void)
{
#ifdef FTDS_IN_USE
    size_t was_timeout = x_GetSybaseConn()->tds_socket->query_timeout;
    x_GetSybaseConn()->tds_socket->query_timeout = GetCTLibContext().GetCancelTimeout();
    return was_timeout;
#else
    return 0;
#endif
}


void
CTL_Connection::CancelFinished(size_t was_timeout)
{
#ifdef FTDS_IN_USE
    x_GetSybaseConn()->tds_socket->query_timeout = was_timeout;
#endif
}


I_ConnectionExtra::TSockHandle
CTL_Connection::GetLowLevelHandle(void) const
{
#ifdef FTDS_IN_USE
    return x_GetSybaseConn()->tds_socket->s;
#else
    return impl::CConnection::GetLowLevelHandle();
#endif
}


CDB_LangCmd* CTL_Connection::LangCmd(const string& lang_query)
{
    string extra_msg = "SQL Command: \"" + lang_query + "\"";
    SetExtraMsg(extra_msg);

    CTL_LangCmd* lcmd = new CTL_LangCmd(
        *this,
        lang_query
        );

    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd* CTL_Connection::RPC(const string& rpc_name)
{
    string extra_msg = "RPC Command: " + rpc_name;
    SetExtraMsg(extra_msg);

    CTL_RPCCmd* rcmd = new CTL_RPCCmd(
        *this,
        rpc_name
        );

    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CTL_Connection::BCPIn(const string& table_name)
{
    string extra_msg = "BCP Table: " + table_name;
    SetExtraMsg(extra_msg);

    CTL_BCPInCmd* bcmd = new CTL_BCPInCmd(
        *this,
        table_name
        );

    return Create_BCPInCmd(*bcmd);
}


CDB_CursorCmd* CTL_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  batch_size)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExtraMsg(extra_msg);

#ifdef FTDS_IN_USE
    CTL_CursorCmdExpl* ccmd = new CTL_CursorCmdExpl(
        *this,
        cursor_name,
        query,
        batch_size
        );
#else
    CTL_CursorCmd* ccmd = new CTL_CursorCmd(
        *this,
        cursor_name,
        query,
        batch_size
        );
#endif

    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CTL_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                             size_t data_size,
                                             bool log_it,
                                             bool dump_results)
{
    CTL_SendDataCmd* sd_cmd = new CTL_SendDataCmd(*this,
                                                  descr_in,
                                                  data_size,
                                                  log_it,
                                                  dump_results
                                                  );
    return Create_SendDataCmd(*sd_cmd);
}


bool CTL_Connection::SendData(I_ITDescriptor& desc, CDB_Stream& lob,
                              bool log_it)
{
    return x_SendData(desc, lob, log_it);
}

bool CTL_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    // cancel all pending commands
    if (!m_Handle.Cancel()) {
        return false;
    }

    // check the connection status
    return m_Handle.IsAlive();
}


I_DriverContext::TConnectionMode CTL_Connection::ConnectMode() const
{
    I_DriverContext::TConnectionMode mode = I_DriverContext::fBcpIn;

    if ( HasSecureLogin() ) {
        mode |= I_DriverContext::fPasswordEncrypted;
    }
    return mode;
}


CTL_Connection::~CTL_Connection()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
    if (m_ActiveCmd) {
        m_ActiveCmd->m_IsActive = false;
    }
}


CTL_LangCmd* 
CTL_Connection::xLangCmd(const string& lang_query)
{
    string extra_msg = "SQL Command: \"" + lang_query + "\"";
    SetExtraMsg( extra_msg );

    CTL_LangCmd* lcmd = new CTL_LangCmd(*this, lang_query);
    return lcmd;
}


// bool CTL_Connection::x_SendData(I_ITDescriptor& descr_in,
//                                 CDB_Stream& img,
//                                 bool log_it)
// {
//     CS_INT size = (CS_INT) img.Size();
//     if ( !size )
//         return false;
//
//     I_ITDescriptor* p_desc = 0;
//
//     // check what type of descriptor we've got
//     if(descr_in.DescriptorType() != CTL_ITDESCRIPTOR_TYPE_MAGNUM) {
//         // this is not a native descriptor
//         p_desc = x_GetNativeITDescriptor
//             (dynamic_cast<CDB_ITDescriptor&> (descr_in));
//         if(p_desc == 0)
//             return false;
//     }
//
//     auto_ptr<I_ITDescriptor> d_guard(p_desc);
//     ctlib::Command cmd(*this);
//
//     cmd.Open(CS_SEND_DATA_CMD, CS_COLUMN_DATA);
//
//     CTL_ITDescriptor& desc = p_desc ?
//         dynamic_cast<CTL_ITDescriptor&> (*p_desc) :
//             dynamic_cast<CTL_ITDescriptor&> (descr_in);
//     // desc->m_Desc.datatype = CS_TEXT_TYPE;
//     desc.m_Desc.total_txtlen  = size;
//     desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;
//
//     if (!cmd.GetDataInfo(desc.m_Desc)) {
//         return false;
//     }
//
//     while (size > 0) {
//         char   buff[1800];
//         CS_INT n_read = (CS_INT) img.Read(buff, sizeof(buff));
//         if (!n_read) {
//             DATABASE_DRIVER_ERROR( "Text/Image data corrupted." + GetDbgInfo(), 110032 );
//         }
//
//         if (!cmd.SendData(buff, n_read)) {
//             DATABASE_DRIVER_ERROR( "ct_send_data failed." + GetDbgInfo(), 110033 );
//         }
//
//         size -= n_read;
//     }
//
//     if (!cmd.Send()) {
//         DATABASE_DRIVER_ERROR( "ct_send failed." + GetDbgInfo(), 110034 );
//     }
//
//     for (;;) {
//         CS_INT res_type;
//
//         switch (cmd.GetResults(res_type)) {
//         case CS_SUCCEED:
//             if (x_ProcessResultInternal(cmd.GetNativeHandle(), res_type)) {
//                 continue;
//             }
//
//             switch (res_type) {
//             case CS_COMPUTE_RESULT:
//             case CS_CURSOR_RESULT:
//             case CS_PARAM_RESULT:
//             case CS_ROW_RESULT:
//             case CS_STATUS_RESULT: {
//                 CS_RETCODE ret_code;
//                 while ((ret_code = cmd.Fetch()) == CS_SUCCEED) {
//                     continue;
//                 }
//                 if (ret_code != CS_END_DATA) {
//                     DATABASE_DRIVER_ERROR( "ct_fetch failed." + GetDbgInfo(), 110036 );
//                 }
//                 break;
//             }
//             case CS_CMD_FAIL:
//                 DATABASE_DRIVER_ERROR( "command failed." + GetDbgInfo(), 110037 );
//             default:
//                 break;
//             }
//             continue;
//         case CS_END_RESULTS:
//             return true;
//         default:
//             DATABASE_DRIVER_ERROR( "ct_result failed." + GetDbgInfo(), 110034 );
//         }
//     }
// }


void
CTL_Connection::x_CmdAlloc(CS_COMMAND** cmd)
{
    CheckSFB(ct_cmd_alloc(x_GetSybaseConn(), cmd),
             "ct_cmd_alloc failed", 110001);
}


bool CTL_Connection::x_SendData(I_ITDescriptor& descr_in, CDB_Stream& stream,
                                bool log_it)
{
    CS_INT size = (CS_INT) stream.Size();
    if ( !size )
        return false;

    if (IsDead()) {
        DATABASE_DRIVER_ERROR("Connection has died." + GetDbgInfo(), 122012);
    }

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CTL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc= x_GetNativeITDescriptor
            (dynamic_cast<CDB_ITDescriptor&> (descr_in));
        if(p_desc == 0)
            return false;
    }


    auto_ptr<I_ITDescriptor> d_guard(p_desc);
    CS_COMMAND* cmd;

    x_CmdAlloc(&cmd);

    if (Check(ct_command(cmd, CS_SEND_DATA_CMD, 0, CS_UNUSED, CS_COLUMN_DATA))
        != CS_SUCCEED) {
        Check(ct_cmd_drop(cmd));
        DATABASE_DRIVER_ERROR( "ct_command failed." + GetDbgInfo(), 110031 );
    }

    CTL_ITDescriptor& desc = p_desc ?
        dynamic_cast<CTL_ITDescriptor&> (*p_desc) :
            dynamic_cast<CTL_ITDescriptor&> (descr_in);
    desc.m_Desc.total_txtlen  = size;
    desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;

    if (Check(ct_data_info(cmd, CS_SET, CS_UNUSED, &desc.m_Desc)) != CS_SUCCEED) {
        Check(ct_cancel(0, cmd, CS_CANCEL_ALL));
        Check(ct_cmd_drop(cmd));
        return false;
    }

    char   buff[1800];
    size_t len = 0;
    size_t invalid_len = 0;

    while (size > 0) {
        len = (CS_INT) stream.Read(buff + invalid_len, sizeof(buff) - invalid_len - 1);

        if (!len) {
            Check(ct_cancel(0, cmd, CS_CANCEL_ALL));
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "Text/Image data corrupted." + GetDbgInfo(), 110032 );
        }

        if (Check(ct_send_data(cmd, buff, len)) != CS_SUCCEED) {
            Check(ct_cancel(0, cmd, CS_CANCEL_CURRENT));
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "ct_send_data failed." + GetDbgInfo(), 110033 );
        }

        size -= len;
    }

    if (Check(ct_send(cmd)) != CS_SUCCEED) {
        Check(ct_cancel(0, cmd, CS_CANCEL_CURRENT));
        Check(ct_cmd_drop(cmd));
        DATABASE_DRIVER_ERROR( "ct_send failed." + GetDbgInfo(), 110034 );
    }

    for (;;) {
        CS_INT res_type;
        switch ( Check(ct_results(cmd, &res_type)) ) {
        case CS_SUCCEED: {
            if (x_ProcessResultInternal(cmd, res_type)) {
                continue;
            }

            switch (res_type) {
            case CS_COMPUTE_RESULT:
            case CS_CURSOR_RESULT:
            case CS_PARAM_RESULT:
            case CS_ROW_RESULT:
            case CS_STATUS_RESULT: {
                CS_RETCODE ret_code;
                while ((ret_code = Check(ct_fetch(cmd, CS_UNUSED, CS_UNUSED,
                                            CS_UNUSED, 0))) == CS_SUCCEED) {
                    continue;
                }
                if (ret_code != CS_END_DATA) {
                    Check(ct_cmd_drop(cmd));
                    DATABASE_DRIVER_ERROR( "ct_fetch failed." + GetDbgInfo(), 110036 );
                }
                break;
            }
            case CS_CMD_FAIL:
                Check(ct_cmd_drop(cmd));
                DATABASE_DRIVER_ERROR( "command failed." + GetDbgInfo(), 110037 );
            default:
                break;
            }
            continue;
        }
        case CS_END_RESULTS: {
            Check(ct_cmd_drop(cmd));
            return true;
        }
        default: {
            if (Check(ct_cancel(0, cmd, CS_CANCEL_ALL)) != CS_SUCCEED) {
                // we need to close this connection
                Check(ct_cmd_drop(cmd));
                DATABASE_DRIVER_ERROR("Unrecoverable crash of ct_result. "
                                      "Connection must be closed." +
                                      GetDbgInfo(), 110033 );
            }
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "ct_result failed." + GetDbgInfo(), 110034 );
        }
        }
    }
}


I_ITDescriptor*
CTL_Connection::x_GetNativeITDescriptor(const CDB_ITDescriptor& descr_in)
{
    string q;
    auto_ptr<CDB_LangCmd> lcmd;
    bool rc = false;

    q  = "set rowcount 1\nupdate ";
    q += descr_in.TableName();
    q += " set ";
    q += descr_in.ColumnName();
#ifdef FTDS_IN_USE
    q += " = '0x0' where ";
#else
    q += " = NULL where ";
#endif
    q += descr_in.SearchConditions();
    q += " \nselect ";
    q += descr_in.ColumnName();
    q += ", TEXTPTR(" + descr_in.ColumnName() + ")";
    q += " from ";
    q += descr_in.TableName();
    q += " where ";
    q += descr_in.SearchConditions();
    q += " \nset rowcount 0";

    lcmd.reset(LangCmd(q));
    rc = !lcmd->Send();
    CHECK_DRIVER_ERROR( rc, "Cannot send the language command." + GetDbgInfo(), 110035 );

    CDB_Result* res;
    I_ITDescriptor* descr = NULL;

    while(lcmd->HasMoreResults()) {
        res = lcmd->Result();
        if(res == 0) continue;
        if((res->ResultType() == eDB_RowResult) && (descr == NULL)) {
            while(res->Fetch()) {
                // res->ReadItem(NULL, 0);
                descr = res->GetImageOrTextDescriptor();
                if(descr) break;
            }
        }
        delete res;
    }

    return descr;
}

bool CTL_Connection::Abort()
{
#ifdef CS_ENDPOINT
    int fd = -1;

    if(Check(ct_con_props(x_GetSybaseConn(),
                          CS_GET,
                          CS_ENDPOINT,
                          &fd,
                          CS_UNUSED,
                          0)) == CS_SUCCEED) {
        if(fd >= 0) {
            close(fd);
            MarkClosed();
            return true;
        }
    }
#endif

    return false;
}

bool CTL_Connection::Close(void)
{
    if (m_Handle.IsOpen()) {
        // Clean connection user data ...
        {
            CTL_Connection* link = NULL;
            GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                    CS_SET,
                                    CS_USERDATA,
                                    &link,
                                    (CS_INT) sizeof(link),
                                    NULL)
                                    );
        }

        // Finalyze connection ...
        Refresh();
        GetCTLExceptionStorage().SetClosingConnect(true);
        try {
            m_Handle.Close();
            GetCTLExceptionStorage().SetClosingConnect(false);
        }
        catch (...) {
            GetCTLExceptionStorage().SetClosingConnect(false);
            throw;
        }
        // This method is often used as a destructor.
        // So, let's drop the connection handle.
        m_Handle.Drop();

        MarkClosed();

        return true;
    }

    return false;
}

bool
CTL_Connection::x_ProcessResultInternal(CS_COMMAND* cmd, CS_INT res_type)
{
    if(GetResultProcessor()) {
        auto_ptr<impl::CResult> res;
        switch (res_type) {
        case CS_ROW_RESULT:
            res.reset(new CTL_RowResult(cmd, *this));
            break;
        case CS_PARAM_RESULT:
            res.reset(new CTL_ParamResult(cmd, *this));
            break;
        case CS_COMPUTE_RESULT:
            res.reset(new CTL_ComputeResult(cmd, *this));
            break;
        case CS_STATUS_RESULT:
            res.reset(new CTL_StatusResult(cmd, *this));
            break;
        }

        if(res.get()) {
            auto_ptr<CDB_Result> dbres(Create_Result(*res));
            GetResultProcessor()->ProcessResult(*dbres);
            return true;
        }
    }

    return false;
}


#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), GetConnection(), &GetBindParams())

/////////////////////////////////////////////////////////////////////////////
//
//  CTL_SendDataCmd::
//

CTL_SendDataCmd::CTL_SendDataCmd(CTL_Connection& conn,
                                 I_ITDescriptor& descr_in,
                                 size_t nof_bytes,
                                 bool log_it,
                                 bool dump_results)
: CTL_LRCmd(conn, kEmptyStr)
, impl::CSendDataCmd(conn, nof_bytes)
, m_DescrType(CDB_ITDescriptor::eUnknown)
, m_DumpResults(dump_results)
{
    CHECK_DRIVER_ERROR(!nof_bytes, "Wrong (zero) data size." + GetDbgInfo(), 110092);

    I_ITDescriptor* p_desc = NULL;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CTL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc = GetConnection().x_GetNativeITDescriptor(
            dynamic_cast<CDB_ITDescriptor&> (descr_in)
            );
        if(p_desc == 0) {
            DATABASE_DRIVER_ERROR( "ct_command failedCannot retrieve I_ITDescriptor." + GetDbgInfo(), 110093 );
        }
    }

    auto_ptr<I_ITDescriptor> d_guard(p_desc);

    if (Check(ct_command(x_GetSybaseCmd(),
                         CS_SEND_DATA_CMD,
                         0,
                         CS_UNUSED,
                         CS_COLUMN_DATA
                         )
              )
        != CS_SUCCEED) {
        DATABASE_DRIVER_ERROR( "ct_command failed." + GetDbgInfo(), 110093 );
    }

    CTL_ITDescriptor& desc = p_desc ? dynamic_cast<CTL_ITDescriptor&>(*p_desc) :
        dynamic_cast<CTL_ITDescriptor&> (descr_in);
    // desc.m_Desc.datatype   = CS_TEXT_TYPE;
    desc.m_Desc.total_txtlen  = (CS_INT)nof_bytes;
    desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;

    // Calculate descriptor type ...
    if (desc.m_Desc.datatype == CS_TEXT_TYPE) {
        m_DescrType = CDB_ITDescriptor::eText;
    } else if (desc.m_Desc.datatype == CS_IMAGE_TYPE) {
        m_DescrType = CDB_ITDescriptor::eBinary;
    }

    if (Check(ct_data_info(x_GetSybaseCmd(),
                           CS_SET,
                           CS_UNUSED,
                           &desc.m_Desc
                           )
              ) != CS_SUCCEED) {
        Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL));
        DATABASE_DRIVER_ERROR( "ct_data_info failed." + GetDbgInfo(), 110093 );
    }
}


size_t CTL_SendDataCmd::SendChunk(const void* chunk_ptr, size_t nof_bytes)
{
    CHECK_DRIVER_ERROR(
        !chunk_ptr  ||  !nof_bytes,
        "Wrong (zero) arguments." + GetDbgInfo(),
        190000 );

    CheckIsDead();

    if ( !GetBytes2Go() )
        return 0;

    if (nof_bytes > GetBytes2Go())
        nof_bytes = GetBytes2Go();

    if (Check(ct_send_data(x_GetSybaseCmd(), (void*) chunk_ptr, (CS_INT) nof_bytes)) != CS_SUCCEED){
        DATABASE_DRIVER_ERROR( "ct_send_data failed." + GetDbgInfo(), 190001 );
    }

    // ATTN: if we inline this expression into call to SetBytes2Go and then call GetBytes2Go()
    // in if thinking that the value was already changed and we will check new value, some
    // optimizers (e.g. gcc 4.1.1) can fail to understand this logic, miss this changing and
    // check in if the value that was returned by GetBytes2Go() in first call.
    size_t bytes2go = GetBytes2Go() - nof_bytes;

    SetBytes2Go(bytes2go);

    if ( bytes2go )
        return nof_bytes;

    CTL_LRCmd::SetWasSent();
    if (Check(ct_send(x_GetSybaseCmd())) != CS_SUCCEED) {
        Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_CURRENT));
        CTL_LRCmd::SetWasSent(false);
        DATABASE_DRIVER_ERROR( "ct_send failed." + GetDbgInfo(), 190004 );
    }

    if (m_DumpResults)
        CTL_LRCmd::DumpResults();
    return nof_bytes;
}


bool CTL_SendDataCmd::Cancel(void)
{
    if (!IsDead()  &&  (GetBytes2Go()  ||  CTL_LRCmd::WasSent())) {
        size_t was_timeout = GetConnection().PrepareToCancel();
        try {
            Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL));
            GetConnection().CancelFinished(was_timeout);
            SetBytes2Go(0);
            CTL_LRCmd::SetWasSent(false);
            return true;
        }
        catch (CDB_Exception&) {
            GetConnection().CancelFinished(was_timeout);
            throw;
        }
    }

    return false;
}


CTL_SendDataCmd::~CTL_SendDataCmd()
{
    try {
        DetachSendDataIntf();

        Cancel();

        DropCmd(*(impl::CSendDataCmd*)this);

        Close();
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}


void
CTL_SendDataCmd::Close(void)
{
    if (x_GetSybaseCmd()) {
        CTL_LRCmd::DumpResults();

        DetachSendDataIntf();

        Cancel();

        Check(ct_cmd_drop(x_GetSybaseCmd()));

        SetSybaseCmd(NULL);
    }
}

CDB_Result*
CTL_SendDataCmd::Result()
{
    return CTL_LRCmd::MakeResult();
}

bool
CTL_SendDataCmd::HasMoreResults() const
{
    return CTL_LRCmd::WasSent();
}

int
CTL_SendDataCmd::RowCount() const
{
    return m_RowCount;
}

#ifdef FTDS_IN_USE
} // namespace ftds64_ctlib
#endif

END_NCBI_SCOPE

