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

#include "dbapi_driver_ctlib_utils.hpp"

#include <dbapi/driver/ctlib/interfaces.hpp>

#include <string.h>
#include <algorithm>


#if defined(NCBI_OS_MSWIN)
#include <io.h>
inline int close(int fd)
{
    return _close(fd);
}
#endif


BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////
CTL_Connection::CTL_Connection(CTLibContext* cntx, CS_CONNECTION* con,
                               bool reusable, const string& pool_name)
{
    m_Link     = con;
    m_Context  = cntx;
    m_Reusable = reusable;
    m_Pool     = pool_name;

    CTL_Connection* link = this;
    Check(ct_con_props(x_GetSybaseConn(), CS_SET, CS_USERDATA,
                 &link, (CS_INT) sizeof(link), NULL));

    // retrieve the connection attributes
    CS_INT outlen(0);
    char   buf[512];

    Check(ct_con_props(x_GetSybaseConn(), CS_GET, CS_SERVERNAME,
                 buf, (CS_INT) (sizeof(buf) - 1), &outlen));
    if((outlen > 0) && (buf[outlen-1] == '\0')) --outlen;
    m_Server.append(buf, (size_t) outlen);

    Check(ct_con_props(x_GetSybaseConn(), CS_GET, CS_USERNAME,
                 buf, (CS_INT) (sizeof(buf) - 1), &outlen));
    if((outlen > 0) && (buf[outlen-1] == '\0')) --outlen;
    m_User.append(buf, (size_t) outlen);

    Check(ct_con_props(x_GetSybaseConn(), CS_GET, CS_PASSWORD,
                 buf, (CS_INT) (sizeof(buf) - 1), &outlen));
    if((outlen > 0) && (buf[outlen-1] == '\0')) --outlen;
    m_Passwd.append(buf, (size_t) outlen);

    CS_BOOL flag;
    Check(ct_con_props(x_GetSybaseConn(), 
                       CS_GET, 
                       CS_BULK_LOGIN, 
                       &flag, 
                       CS_UNUSED, 
                       &outlen));
    m_BCPable = (flag == CS_TRUE);

    Check(ct_con_props(x_GetSybaseConn(), 
                       CS_GET, 
                       CS_SEC_ENCRYPTION, 
                       &flag, 
                       CS_UNUSED, 
                       &outlen));
    m_SecureLogin = (flag == CS_TRUE);

    m_ResProc= 0;
}


CS_RETCODE 
CTL_Connection::Check(CS_RETCODE rc)
{
    GetCTLExceptionStorage().Handle(m_MsgHandlers);
    
    return rc;
}

void 
CTL_Connection::x_CmdAlloc(CS_COMMAND** cmd)
{
    switch ( Check(ct_cmd_alloc(m_Link, cmd)) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        DATABASE_DRIVER_ERROR( "ct_cmd_alloc failed", 110001 );
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 110002 );
    }
}


bool CTL_Connection::IsAlive()
{
    CS_INT status;
    if (Check(ct_con_props(x_GetSybaseConn(), 
                           CS_GET, 
                           CS_CON_STATUS, 
                           &status, 
                           CS_UNUSED, 
                           0))
        != CS_SUCCEED)
        return false;

    return
        (status & CS_CONSTAT_CONNECTED) != 0  &&
        (status & CS_CONSTAT_DEAD     ) == 0;
}


CDB_LangCmd* CTL_Connection::LangCmd(const string& lang_query,
                                     unsigned int  nof_params)
{
    CS_COMMAND* cmd;

    x_CmdAlloc(&cmd);

    CTL_LangCmd* lcmd = new CTL_LangCmd(this, cmd, lang_query, nof_params);
    m_CMDs.push_back(lcmd);
    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd* CTL_Connection::RPC(const string& rpc_name,
                                unsigned int  nof_args)
{
    CS_COMMAND* cmd;

    x_CmdAlloc(&cmd);

    CTL_RPCCmd* rcmd = new CTL_RPCCmd(this, cmd, rpc_name, nof_args);
    m_CMDs.push_back(rcmd);
    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CTL_Connection::BCPIn(const string& table_name,
                                    unsigned int  nof_columns)
{
    CHECK_DRIVER_ERROR( !m_BCPable, "No bcp on this connection", 110003 );

    CS_BLKDESC* cmd;
    if (blk_alloc(x_GetSybaseConn(), BLK_VERSION_100, &cmd) != CS_SUCCEED) {
        DATABASE_DRIVER_ERROR( "blk_alloc failed", 110004 );
    }

    CTL_BCPInCmd* bcmd = new CTL_BCPInCmd(this, cmd, table_name, nof_columns);
    m_CMDs.push_back(bcmd);
    return Create_BCPInCmd(*bcmd);
}


CDB_CursorCmd* CTL_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  nof_params,
                                      unsigned int  batch_size)
{
    CS_COMMAND* cmd;

    x_CmdAlloc(&cmd);

    CTL_CursorCmd* ccmd = new CTL_CursorCmd(this, cmd, cursor_name, query,
                                            nof_params, batch_size);
    m_CMDs.push_back(ccmd);
    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CTL_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                             size_t data_size, bool log_it)
{
    CHECK_DRIVER_ERROR( !data_size, "wrong (zero) data size", 110092 );

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CTL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc= x_GetNativeITDescriptor
            (dynamic_cast<CDB_ITDescriptor&> (descr_in));
        if(p_desc == 0) return 0;
    }

    C_ITDescriptorGuard d_guard(p_desc);
    CS_COMMAND* cmd;

    x_CmdAlloc(&cmd);

    if (Check(ct_command(cmd, CS_SEND_DATA_CMD, 0, CS_UNUSED, CS_COLUMN_DATA))
        != CS_SUCCEED) {
        Check(ct_cmd_drop(cmd));
        DATABASE_DRIVER_ERROR( "ct_command failed", 110093 );
    }

    CTL_ITDescriptor& desc = p_desc? dynamic_cast<CTL_ITDescriptor&>(*p_desc) :
        dynamic_cast<CTL_ITDescriptor&> (descr_in);
    // desc.m_Desc.datatype   = CS_TEXT_TYPE;
    desc.m_Desc.total_txtlen  = (CS_INT)data_size;
    desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;

    if (Check(ct_data_info(cmd, CS_SET, CS_UNUSED, &desc.m_Desc)) != CS_SUCCEED) {
        Check(ct_cancel(0, cmd, CS_CANCEL_ALL));
        Check(ct_cmd_drop(cmd));
        DATABASE_DRIVER_ERROR( "ct_data_info failed", 110093 );
    }   

    CTL_SendDataCmd* sd_cmd = new CTL_SendDataCmd(this, cmd, data_size);
    m_CMDs.push_back(sd_cmd);
    return Create_SendDataCmd(*sd_cmd);
}


bool CTL_Connection::SendData(I_ITDescriptor& desc, CDB_Image& img,
                              bool log_it)
{
    return x_SendData(desc, img, log_it);
}


bool CTL_Connection::SendData(I_ITDescriptor& desc, CDB_Text& txt,
                              bool log_it)
{
    return x_SendData(desc, txt, log_it);
}


bool CTL_Connection::Refresh()
{
    // close all commands first
    ITERATE(deque<CDB_BaseEnt*>, it, m_CMDs) {
        try {
            delete *it;
        } catch (CDB_Exception& ) {
            _ASSERT(false);
        }
    }
    m_CMDs.clear();

    // cancel all pending commands
    if (Check(ct_cancel(x_GetSybaseConn(), 0, CS_CANCEL_ALL) != CS_SUCCEED))
        return false;

    // check the connection status
    CS_INT status;
    if (Check(ct_con_props(x_GetSybaseConn(), 
                           CS_GET, 
                           CS_CON_STATUS, 
                           &status, 
                           CS_UNUSED, 
                           0))
        != CS_SUCCEED)
        return false;

    return
        (status & CS_CONSTAT_CONNECTED) != 0  &&
        (status & CS_CONSTAT_DEAD     ) == 0;
}


const string& CTL_Connection::ServerName() const
{
    return m_Server;
}


const string& CTL_Connection::UserName() const
{
    return m_User;
}


const string& CTL_Connection::Password() const
{
    return m_Passwd;
}


I_DriverContext::TConnectionMode CTL_Connection::ConnectMode() const
{
    I_DriverContext::TConnectionMode mode = 0;
    if ( m_BCPable ) {
        mode |= I_DriverContext::fBcpIn;
    }
    if ( m_SecureLogin ) {
        mode |= I_DriverContext::fPasswordEncrypted;
    }
    return mode;
}


bool CTL_Connection::IsReusable() const
{
    return m_Reusable;
}


const string& CTL_Connection::PoolName() const
{
    return m_Pool;
}


I_DriverContext* CTL_Connection::Context() const
{
    return const_cast<CTLibContext*> (m_Context);
}


void CTL_Connection::PushMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Push(h);
}


void CTL_Connection::PopMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Pop(h);
}

CDB_ResultProcessor* CTL_Connection::SetResultProcessor(CDB_ResultProcessor* rp)
{
    CDB_ResultProcessor* r= m_ResProc;
    m_ResProc= rp;
    return r;
}

void CTL_Connection::Release()
{
    m_BR = 0;
    // close all commands first

    ITERATE(deque<CDB_BaseEnt*>, it, m_CMDs) {
        try {
            delete *it;
        } catch (CDB_Exception& ) {
            _ASSERT(false);
        }
    }
    m_CMDs.clear();
}


CTL_Connection::~CTL_Connection()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


void CTL_Connection::DropCmd(CDB_BaseEnt& cmd)
{
    deque<CDB_BaseEnt*>::iterator it = find(m_CMDs.begin(), m_CMDs.end(), &cmd);

    if (it != m_CMDs.end()) {
        m_CMDs.erase(it);
    }
}


bool CTL_Connection::x_SendData(I_ITDescriptor& descr_in, CDB_Stream& img,
                                bool log_it)
{
    CS_INT size = (CS_INT) img.Size();
    if ( !size )
        return false;

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CTL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc= x_GetNativeITDescriptor
            (dynamic_cast<CDB_ITDescriptor&> (descr_in));
        if(p_desc == 0)
            return false;
    }
    

    C_ITDescriptorGuard d_guard(p_desc);
    CS_COMMAND* cmd;

    x_CmdAlloc(&cmd);

    if (Check(ct_command(cmd, CS_SEND_DATA_CMD, 0, CS_UNUSED, CS_COLUMN_DATA))
        != CS_SUCCEED) {
        Check(ct_cmd_drop(cmd));
        DATABASE_DRIVER_ERROR( "ct_command failed", 110031 );
    }

    CTL_ITDescriptor& desc = p_desc ?
        dynamic_cast<CTL_ITDescriptor&> (*p_desc) : 
            dynamic_cast<CTL_ITDescriptor&> (descr_in);
    // desc->m_Desc.datatype = CS_TEXT_TYPE;
    desc.m_Desc.total_txtlen  = size;
    desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;

    if (Check(ct_data_info(cmd, CS_SET, CS_UNUSED, &desc.m_Desc)) != CS_SUCCEED) {
        Check(ct_cancel(0, cmd, CS_CANCEL_ALL));
        Check(ct_cmd_drop(cmd));
        return false;
    }   

    while (size > 0) {
        char   buff[1800];
        CS_INT n_read = (CS_INT) img.Read(buff, sizeof(buff));
        if ( !n_read ) {
            Check(ct_cancel(0, cmd, CS_CANCEL_ALL));
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "Text/Image data corrupted", 110032 );
        }
        if (Check(ct_send_data(cmd, buff, n_read)) != CS_SUCCEED) {
            Check(ct_cancel(0, cmd, CS_CANCEL_CURRENT));
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "ct_send_data failed", 110033 );
        }
        size -= n_read;
    }

    if (Check(ct_send(cmd)) != CS_SUCCEED) {
        Check(ct_cancel(0, cmd, CS_CANCEL_CURRENT));
        Check(ct_cmd_drop(cmd));
        DATABASE_DRIVER_ERROR( "ct_send failed", 110034 );
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
                    DATABASE_DRIVER_ERROR( "ct_fetch failed", 110036 );
                }
                break;
            }
            case CS_CMD_FAIL:
                Check(ct_cmd_drop(cmd));
                DATABASE_DRIVER_ERROR( "command failed", 110037 );
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
                DATABASE_DRIVER_ERROR( "Unrecoverable crash of ct_result. "
                                   "Connection must be closed", 110033 );
            }
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "ct_result failed", 110034 );
        }
        }
    }
}


I_ITDescriptor* CTL_Connection::x_GetNativeITDescriptor
(const CDB_ITDescriptor& descr_in)
{
    string q= "set rowcount 1\nupdate ";
    q+= descr_in.TableName();
    q+= " set ";
    q+= descr_in.ColumnName();
    q+= "=NULL where ";
    q+= descr_in.SearchConditions();
    q+= " \nselect ";
    q+= descr_in.ColumnName();
    q+= " from ";
    q+= descr_in.TableName();
    q+= " where ";
    q+= descr_in.SearchConditions();
    q+= " \nset rowcount 0";
    
    CDB_LangCmd* lcmd= LangCmd(q, 0);
    bool rc = !lcmd->Send();
    CHECK_DRIVER_ERROR( rc, "Cannot send the language command", 110035 );

    CDB_Result* res;
    I_ITDescriptor* descr= 0;

    while(lcmd->HasMoreResults()) {
        res= lcmd->Result();
        if(res == 0) continue;
        if((res->ResultType() == eDB_RowResult) && (descr == 0)) {
            while(res->Fetch()) {
                //res->ReadItem(&i, 0);
                descr= res->GetImageOrTextDescriptor();
                if(descr) break;
            }
        }
        delete res;
    }
    delete lcmd;
        
    return descr;
}

bool CTL_Connection::Abort()
{
    int fd = -1;

    if(Check(ct_con_props(x_GetSybaseConn(), 
                          CS_GET, 
                          CS_ENDPOINT, 
                          &fd, 
                          CS_UNUSED, 
                          0)) == CS_SUCCEED) {
        if(fd >= 0) {
            close(fd);
            return true;
        }
    }
    return false;
}

bool CTL_Connection::Close(void)
{
    if (x_GetSybaseConn()) {
        try {
            if (!Refresh()  ||  Check(ct_close(x_GetSybaseConn(), CS_UNUSED)) != CS_SUCCEED) {
                Check(ct_close(x_GetSybaseConn(), CS_FORCE_CLOSE));
            }

            Check(ct_con_drop(x_GetSybaseConn()));

            m_Link = NULL;

            return true;
        }
        NCBI_CATCH_ALL( kEmptyStr )
    }

    return false;
}

bool
CTL_Connection::x_ProcessResultInternal(CS_COMMAND* cmd, CS_INT res_type)
{
    if(m_ResProc) {
        auto_ptr<I_Result> res;
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
            m_ResProc->ProcessResult(*dbres);
            return true;
        }
    }
    
    return false;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_SendDataCmd::
//


CTL_SendDataCmd::CTL_SendDataCmd(CTL_Connection* conn, 
                                 CS_COMMAND* cmd,
                                 size_t nof_bytes) :
CTL_Cmd(conn, cmd)
{
    m_Bytes2go = nof_bytes;
}


size_t CTL_SendDataCmd::SendChunk(const void* pChunk, size_t nof_bytes)
{
    CHECK_DRIVER_ERROR( 
        !pChunk  ||  !nof_bytes, 
        "wrong (zero) arguments", 
        190000 );

    if ( !m_Bytes2go )
        return 0;

    if (nof_bytes > m_Bytes2go)
        nof_bytes = m_Bytes2go;

    if (Check(ct_send_data(x_GetSybaseCmd(), (void*) pChunk, (CS_INT) nof_bytes)) != CS_SUCCEED){
        DATABASE_DRIVER_ERROR( "ct_send_data failed", 190001 );
    }

    m_Bytes2go -= nof_bytes;
    if ( m_Bytes2go )
        return nof_bytes;

    if (Check(ct_send(x_GetSybaseCmd())) != CS_SUCCEED) {
        Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_CURRENT));
        DATABASE_DRIVER_ERROR( "ct_send failed", 190004 );
    }

    for (;;) {
        CS_INT res_type;
        switch ( Check(ct_results(x_GetSybaseCmd(), &res_type)) ) {
        case CS_SUCCEED: {
            if (ProcessResultInternal(res_type)) {
                continue;
            }
            
            switch ( res_type ) {
            case CS_COMPUTE_RESULT:
            case CS_CURSOR_RESULT:
            case CS_PARAM_RESULT:
            case CS_ROW_RESULT:
            case CS_STATUS_RESULT: 
                {
                    CS_RETCODE ret_code;
                    while ((ret_code = Check(ct_fetch(x_GetSybaseCmd(), 
                                                      CS_UNUSED, 
                                                      CS_UNUSED,
                                                      CS_UNUSED, 0))) == 
                           CS_SUCCEED);
                    if (ret_code != CS_END_DATA) {
                        DATABASE_DRIVER_ERROR( "ct_fetch failed", 190006 );
                    }
                    break;
                }
            case CS_CMD_FAIL: 
                Check(ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_ALL));
                DATABASE_DRIVER_ERROR( "command failed", 190007 );
            default:
                break;
            }
            continue;
        }
        case CS_END_RESULTS: 
            return nof_bytes;
        default:
            if (Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL)) != CS_SUCCEED) {
                DATABASE_DRIVER_ERROR( "Unrecoverable crash of ct_result. "
                                   "Connection must be closed", 190002 );
            }
            DATABASE_DRIVER_ERROR( "ct_result failed", 190003 );
        }
    }
}


void CTL_SendDataCmd::Release()
{
    m_BR = 0;
    
    if ( m_Bytes2go ) {
        Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL));
        m_Bytes2go = 0;
    }
    
    DropCmd(*this);
    delete this;
}


CDB_Result* 
CTL_SendDataCmd::CreateResult(I_Result& result)
{
    return Create_Result(result);
}


CTL_SendDataCmd::~CTL_SendDataCmd()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


void
CTL_SendDataCmd::Close(void)
{
    if (x_GetSybaseCmd()) {
        if ( m_Bytes2go )
            Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL));

        if ( m_BR )
            *m_BR = 0;

        Check(ct_cmd_drop(x_GetSybaseCmd()));
        
        SetSybaseCmd(NULL);
    }
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.29  2006/05/11 18:07:58  ssikorsk
 * Utilized new exception storage
 *
 * Revision 1.28  2006/05/08 17:46:33  ssikorsk
 * Clear list of commands in CTL_Connection::Release()
 *
 * Revision 1.27  2006/05/03 15:10:36  ssikorsk
 * Implemented classs CTL_Cmd and CCTLExceptions;
 * Surrounded each native ctlib call with Check;
 *
 * Revision 1.26  2006/04/05 14:28:35  ssikorsk
 * Implemented CTL_Connection::Close
 *
 * Revision 1.25  2006/03/06 19:51:37  ssikorsk
 *
 * Added method Close/CloseForever to all context/command-aware classes.
 * Use getters to access Sybase's context and command handles.
 *
 * Revision 1.24  2006/02/22 15:56:40  ssikorsk
 * CHECK_DRIVER_FATAL --> CHECK_DRIVER_ERROR
 *
 * Revision 1.23  2006/02/22 15:15:50  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.22  2005/09/19 14:19:02  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.21  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.20  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.19  2005/02/25 16:09:40  soussov
 * adds wrapper for close to make windows happy
 *
 * Revision 1.18  2005/02/23 21:39:57  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.17  2004/05/17 21:12:03  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.16  2003/09/23 19:38:22  soussov
 * cancels send_data cmd if it failes while processing results
 *
 * Revision 1.15  2003/07/08 18:51:38  soussov
 * fixed bug in constructor
 *
 * Revision 1.14  2003/06/05 16:00:31  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.13  2003/02/05 14:56:22  dicuccio
 * Fixed uninitialized read reported by valgrind.
 *
 * Revision 1.12  2003/01/31 16:49:38  lavr
 * Remove unused variable "e" from catch() clause
 *
 * Revision 1.11  2002/12/16 16:17:25  soussov
 * ct_con_props returns an outlen == strlen(x)+1. Adaptin to this feature
 *
 * Revision 1.10  2002/09/16 15:10:23  soussov
 * add try catch when canceling active commands in Refresh method
 *
 * Revision 1.9  2002/03/27 05:01:58  vakatov
 * Minor formal fixes
 *
 * Revision 1.8  2002/03/26 15:34:37  soussov
 * new image/text operations added
 *
 * Revision 1.7  2002/02/01 21:49:37  soussov
 * ct_cmd_drop for the CTL_Connection::SendData added
 *
 * Revision 1.6  2001/11/06 17:59:55  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.5  2001/10/12 21:21:00  lavr
 * Faster checks against zero for unsigned ints
 *
 * Revision 1.4  2001/09/27 20:08:33  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.3  2001/09/27 15:42:09  soussov
 * CTL_Connection::Release() added
 *
 * Revision 1.2  2001/09/25 15:05:43  soussov
 * fixed typo in CTL_SendDataCmd::SendChunk
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
