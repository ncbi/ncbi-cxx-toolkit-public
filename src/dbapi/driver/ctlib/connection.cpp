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
#include <dbapi/driver/ctlib/interfaces.hpp>
#include <string.h>


BEGIN_NCBI_SCOPE


CTL_Connection::CTL_Connection(CTLibContext* cntx, CS_CONNECTION* con,
                               bool reusable, const string& pool_name)
{
    m_Link     = con;
    m_Context  = cntx;
    m_Reusable = reusable;
    m_Pool     = pool_name;

    CTL_Connection* link = this;
    ct_con_props(m_Link, CS_SET, CS_USERDATA,
                 &link, (CS_INT) sizeof(link), NULL);

    // retrieve the connection attributes
    CS_INT outlen(0);
    char   buf[512];

    ct_con_props(m_Link, CS_GET, CS_SERVERNAME,
                 buf, (CS_INT) (sizeof(buf) - 1), &outlen);
    if((outlen > 0) && (buf[outlen-1] == '\0')) --outlen;
    m_Server.append(buf, (size_t) outlen);

    ct_con_props(m_Link, CS_GET, CS_USERNAME,
                 buf, (CS_INT) (sizeof(buf) - 1), &outlen);
    if((outlen > 0) && (buf[outlen-1] == '\0')) --outlen;
    m_User.append(buf, (size_t) outlen);

    ct_con_props(m_Link, CS_GET, CS_PASSWORD,
                 buf, (CS_INT) (sizeof(buf) - 1), &outlen);
    if((outlen > 0) && (buf[outlen-1] == '\0')) --outlen;
    m_Passwd.append(buf, (size_t) outlen);

    CS_BOOL flag;
    ct_con_props(m_Link, CS_GET, CS_BULK_LOGIN, &flag, CS_UNUSED, &outlen);
    m_BCPable = (flag == CS_TRUE);

    ct_con_props(m_Link, CS_GET, CS_SEC_ENCRYPTION, &flag, CS_UNUSED, &outlen);
    m_SecureLogin = (flag == CS_TRUE);

    m_ResProc= 0;
}


bool CTL_Connection::IsAlive()
{
    CS_INT status;
    if (ct_con_props(m_Link, CS_GET, CS_CON_STATUS, &status, CS_UNUSED, 0)
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

    switch ( ct_cmd_alloc(m_Link, &cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        throw CDB_ClientEx(eDB_Fatal, 110001, "CTL_Connection::LangCmd",
                           "ct_cmd_alloc failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Fatal, 110002, "CTL_Connection::LangCmd",
                           "the connection is busy");
    }

    CTL_LangCmd* lcmd = new CTL_LangCmd(this, cmd, lang_query, nof_params);
    m_CMDs.Add(lcmd);
    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd* CTL_Connection::RPC(const string& rpc_name,
                                unsigned int  nof_args)
{
    CS_COMMAND* cmd;

    switch ( ct_cmd_alloc(m_Link, &cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        throw CDB_ClientEx(eDB_Fatal, 110001, "CTL_Connection::RPC",
                           "ct_cmd_alloc failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Fatal, 110002, "CTL_Connection::RPC",
                           "the connection is busy");
    }

    CTL_RPCCmd* rcmd = new CTL_RPCCmd(this, cmd, rpc_name, nof_args);
    m_CMDs.Add(rcmd);
    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CTL_Connection::BCPIn(const string& table_name,
                                    unsigned int  nof_columns)
{
    if (!m_BCPable) {
        throw CDB_ClientEx(eDB_Error, 110003, "CTL_Connection::BCPIn",
                           "No bcp on this connection");
    }

    CS_BLKDESC* cmd;
    if (blk_alloc(m_Link, BLK_VERSION_100, &cmd) != CS_SUCCEED) {
        throw CDB_ClientEx(eDB_Fatal, 110004, "CTL_Connection::BCPIn",
                           "blk_alloc failed");
    }

    CTL_BCPInCmd* bcmd = new CTL_BCPInCmd(this, cmd, table_name, nof_columns);
    m_CMDs.Add(bcmd);
    return Create_BCPInCmd(*bcmd);
}


CDB_CursorCmd* CTL_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  nof_params,
                                      unsigned int  batch_size)
{
    CS_COMMAND* cmd;

    switch ( ct_cmd_alloc(m_Link, &cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        throw CDB_ClientEx(eDB_Fatal, 110001, "CTL_Connection::Cursor",
                           "ct_cmd_alloc failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Fatal, 110002, "CTL_Connection::Cursor",
                           "the connection is busy");
    }

    CTL_CursorCmd* ccmd = new CTL_CursorCmd(this, cmd, cursor_name, query,
                                            nof_params, batch_size);
    m_CMDs.Add(ccmd);
    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CTL_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                             size_t data_size, bool log_it)
{
    if ( !data_size ) {
        throw CDB_ClientEx(eDB_Fatal, 110092, "CTL_Connection::SendDataCmd",
                           "wrong (zero) data size");
    }

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

    switch ( ct_cmd_alloc(m_Link, &cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        throw CDB_ClientEx(eDB_Fatal, 110001, "CTL_Connection::SendDataCmd",
                           "ct_cmd_alloc failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Fatal, 110002, "CTL_Connection::SendDataCmd",
                           "the connection is busy");
    }

    if (ct_command(cmd, CS_SEND_DATA_CMD, 0, CS_UNUSED, CS_COLUMN_DATA)
        != CS_SUCCEED) {
        ct_cmd_drop(cmd);
        throw CDB_ClientEx(eDB_Fatal, 110093, "CTL_Connection::SendDataCmd",
                           "ct_command failed");
    }

    CTL_ITDescriptor& desc = p_desc? dynamic_cast<CTL_ITDescriptor&>(*p_desc) :
        dynamic_cast<CTL_ITDescriptor&> (descr_in);
    // desc.m_Desc.datatype   = CS_TEXT_TYPE;
    desc.m_Desc.total_txtlen  = (CS_INT)data_size;
    desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;

    if (ct_data_info(cmd, CS_SET, CS_UNUSED, &desc.m_Desc) != CS_SUCCEED) {
        ct_cancel(0, cmd, CS_CANCEL_ALL);
        ct_cmd_drop(cmd);
        throw CDB_ClientEx(eDB_Fatal, 110093, "CTL_Connection::SendDataCmd",
                           "ct_data_info failed");
    }	

    CTL_SendDataCmd* sd_cmd = new CTL_SendDataCmd(this, cmd, data_size);
    m_CMDs.Add(sd_cmd);
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
    while(m_CMDs.NofItems() > 0) {
        CDB_BaseEnt* pCmd = static_cast<CDB_BaseEnt*> (m_CMDs.Get(0));
        try {
            delete pCmd;
        } catch (CDB_Exception& ) {
        }
        m_CMDs.Remove((int) 0);
    }

    // cancel all pending commands
    if (ct_cancel(m_Link, 0, CS_CANCEL_ALL) != CS_SUCCEED)
        return false;

    // check the connection status
    CS_INT status;
    if (ct_con_props(m_Link, CS_GET, CS_CON_STATUS, &status, CS_UNUSED, 0)
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
    while(m_CMDs.NofItems() > 0) {
        CDB_BaseEnt* pCmd = static_cast<CDB_BaseEnt*> (m_CMDs.Get(0));
        try {
            delete pCmd;
        } catch (CDB_Exception& ) {
        }
        m_CMDs.Remove((int) 0);
    }
}


CTL_Connection::~CTL_Connection()
{
    if (!Refresh()  ||  ct_close(m_Link, CS_UNUSED) != CS_SUCCEED) {
        ct_close(m_Link, CS_FORCE_CLOSE);
    }

    ct_con_drop(m_Link);
}


void CTL_Connection::DropCmd(CDB_BaseEnt& cmd)
{
    m_CMDs.Remove(static_cast<TPotItem> (&cmd));
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

    switch ( ct_cmd_alloc(m_Link, &cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        throw CDB_ClientEx(eDB_Fatal, 110001, "CTL_Connection::SendData",
                           "ct_cmd_alloc failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Fatal, 110002, "CTL_Connection::SendData",
                           "the connection is busy");
    }

    if (ct_command(cmd, CS_SEND_DATA_CMD, 0, CS_UNUSED, CS_COLUMN_DATA)
        != CS_SUCCEED) {
        ct_cmd_drop(cmd);
        throw CDB_ClientEx(eDB_Fatal, 110031, "CTL_Connection::SendData",
                           "ct_command failed");
    }

    CTL_ITDescriptor& desc = p_desc ?
        dynamic_cast<CTL_ITDescriptor&> (*p_desc) : 
            dynamic_cast<CTL_ITDescriptor&> (descr_in);
    // desc->m_Desc.datatype = CS_TEXT_TYPE;
    desc.m_Desc.total_txtlen  = size;
    desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;

    if (ct_data_info(cmd, CS_SET, CS_UNUSED, &desc.m_Desc) != CS_SUCCEED) {
        ct_cancel(0, cmd, CS_CANCEL_ALL);
        ct_cmd_drop(cmd);
        return false;
    }	

    while (size > 0) {
        char   buff[1800];
        CS_INT n_read = (CS_INT) img.Read(buff, sizeof(buff));
        if ( !n_read ) {
            ct_cancel(0, cmd, CS_CANCEL_ALL);
            ct_cmd_drop(cmd);
            throw CDB_ClientEx(eDB_Fatal, 110032, "CTL_Connection::SendData",
                               "Text/Image data corrupted");
        }
        if (ct_send_data(cmd, buff, n_read) != CS_SUCCEED) {
            ct_cancel(0, cmd, CS_CANCEL_CURRENT);
            ct_cmd_drop(cmd);
            throw CDB_ClientEx(eDB_Fatal, 110033, "CTL_Connection::SendData",
                               "ct_send_data failed");
        }
        size -= n_read;
    }

    if (ct_send(cmd) != CS_SUCCEED) {
        ct_cancel(0, cmd, CS_CANCEL_CURRENT);
        ct_cmd_drop(cmd);
        throw CDB_ClientEx(eDB_Fatal, 110034, "CTL_Connection::SendData",
                           "ct_send failed");
    }

    for (;;) {
        CS_INT res_type;
        switch ( ct_results(cmd, &res_type) ) {
        case CS_SUCCEED: {
            if(m_ResProc) {
                I_Result* res= 0;
                switch (res_type) {
                case CS_ROW_RESULT:
                    res = new CTL_RowResult(cmd);
                    break;
                case CS_PARAM_RESULT:
                    res = new CTL_ParamResult(cmd);
                    break;
                case CS_COMPUTE_RESULT:
                    res = new CTL_ComputeResult(cmd);
                    break;
                case CS_STATUS_RESULT:
                    res = new CTL_StatusResult(cmd);
                    break;
                }
                if(res) {
                    CDB_Result* dbres= Create_Result(*res);
                    m_ResProc->ProcessResult(*dbres);
                    delete dbres;
                    delete res;
                    continue;
                }
            }
                    
            switch (res_type) {
            case CS_COMPUTE_RESULT:
            case CS_CURSOR_RESULT:
            case CS_PARAM_RESULT:
            case CS_ROW_RESULT:
            case CS_STATUS_RESULT: {
                CS_RETCODE ret_code;
                while ((ret_code = ct_fetch(cmd, CS_UNUSED, CS_UNUSED,
                                            CS_UNUSED, 0)) == CS_SUCCEED) {
                    continue;
                }
                if (ret_code != CS_END_DATA) {
                    ct_cmd_drop(cmd);
                    throw CDB_ClientEx(eDB_Fatal, 110036,
                                       "CTL_Connection::SendData",
                                       "ct_fetch failed");
                }
                break;
            }
            case CS_CMD_FAIL:
                ct_cmd_drop(cmd);
                throw CDB_ClientEx(eDB_Error, 110037,
                                   "CTL_Connection::SendData",
                                   "command failed");
            default:
                break;
            }
            continue;
        }
        case CS_END_RESULTS: {
            ct_cmd_drop(cmd);
            return true;
        }
        default: {
            if (ct_cancel(0, cmd, CS_CANCEL_ALL) != CS_SUCCEED) {
                // we need to close this connection
                ct_cmd_drop(cmd);
                throw CDB_ClientEx(eDB_Fatal, 110033,
                                   "CTL_Connection::SendData",
                                   "Unrecoverable crash of ct_result. "
                                   "Connection must be closed");
            }
            ct_cmd_drop(cmd);
            throw CDB_ClientEx(eDB_Error, 110034, "CTL_Connection::SendData",
                               "ct_result failed");
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
    if(!lcmd->Send()) {
        throw CDB_ClientEx(eDB_Error, 110035, "CTL_Connection::SendData",
                           "can not send the language command");
    }

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
    int fd= -1;

    if(ct_con_props(m_Link, CS_GET, CS_ENDPOINT, &fd, CS_UNUSED, 0) == CS_SUCCEED) {
        if(fd >= 0) {
            close(fd);
            return true;
        }
    }
    return false;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_SendDataCmd::
//


CTL_SendDataCmd::CTL_SendDataCmd(CTL_Connection* con, CS_COMMAND* cmd,
                                 size_t nof_bytes)
{
    m_Connect  = con;
    m_Cmd      = cmd;
    m_Bytes2go = nof_bytes;
}


size_t CTL_SendDataCmd::SendChunk(const void* pChunk, size_t nof_bytes)
{
    if (!pChunk  ||  !nof_bytes) {
        throw CDB_ClientEx(eDB_Fatal, 190000, "CTL_SendDataCmd::SendChunk",
                           "wrong (zero) arguments");
    }

    if ( !m_Bytes2go )
        return 0;

    if (nof_bytes > m_Bytes2go)
        nof_bytes = m_Bytes2go;

    if (ct_send_data(m_Cmd, (void*) pChunk, (CS_INT) nof_bytes) != CS_SUCCEED){
        throw CDB_ClientEx(eDB_Fatal, 190001, "CTL_SendDataCmd::SendChunk",
                           "ct_send_data failed");
    }

    m_Bytes2go -= nof_bytes;
    if ( m_Bytes2go )
        return nof_bytes;

    if (ct_send(m_Cmd) != CS_SUCCEED) {
        ct_cancel(0, m_Cmd, CS_CANCEL_CURRENT);
        throw CDB_ClientEx(eDB_Fatal, 190004, "CTL_SendDataCmd::SendChunk",
                           "ct_send failed");
    }

    for (;;) {
        CS_INT res_type;
        switch ( ct_results(m_Cmd, &res_type) ) {
        case CS_SUCCEED: {
            if(m_Connect->m_ResProc) {
                I_Result* res= 0;
                switch (res_type) {
                case CS_ROW_RESULT:
                    res = new CTL_RowResult(m_Cmd);
                    break;
                case CS_PARAM_RESULT:
                    res = new CTL_ParamResult(m_Cmd);
                    break;
                case CS_COMPUTE_RESULT:
                    res = new CTL_ComputeResult(m_Cmd);
                    break;
                case CS_STATUS_RESULT:
                    res = new CTL_StatusResult(m_Cmd);
                    break;
                }
                if(res) {
                    CDB_Result* dbres= Create_Result(*res);
                    m_Connect->m_ResProc->ProcessResult(*dbres);
                    delete dbres;
                    delete res;
                    continue;
                }
            }
            switch ( res_type ) {
            case CS_COMPUTE_RESULT:
            case CS_CURSOR_RESULT:
            case CS_PARAM_RESULT:
            case CS_ROW_RESULT:
            case CS_STATUS_RESULT: {
                CS_RETCODE ret_code;
                while ((ret_code = ct_fetch(m_Cmd, CS_UNUSED, CS_UNUSED,
                                            CS_UNUSED, 0)) == CS_SUCCEED);
                if (ret_code != CS_END_DATA) {
                    throw CDB_ClientEx(eDB_Fatal, 190006,
                                       "CTL_SendDataCmd::SendChunk",
                                       "ct_fetch failed");
                }
                break;
            }
            case CS_CMD_FAIL: {
                ct_cancel(NULL, m_Cmd, CS_CANCEL_ALL);
                throw CDB_ClientEx(eDB_Error, 190007,
                                   "CTL_SendDataCmd::SendChunk",
                                   "command failed");
            }
            default: {
                break;
            }
            }
            continue;
        }
        case CS_END_RESULTS: {
            return nof_bytes;
        }
        default: {
            if (ct_cancel(0, m_Cmd, CS_CANCEL_ALL) != CS_SUCCEED) {
                throw CDB_ClientEx(eDB_Fatal, 190002,
                                   "CTL_SendDataCmd::SendChunk",
                                   "Unrecoverable crash of ct_result. "
                                   "Connection must be closed");
            }
            throw CDB_ClientEx(eDB_Error, 190003, "CTL_SendDataCmd::SendChunk",
                               "ct_result failed");
        }
        }
    }
}


void CTL_SendDataCmd::Release()
{
    m_BR = 0;
    if ( m_Bytes2go ) {
        ct_cancel(0, m_Cmd, CS_CANCEL_ALL);
        m_Bytes2go = 0;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CTL_SendDataCmd::~CTL_SendDataCmd()
{
    if ( m_Bytes2go )
        ct_cancel(0, m_Cmd, CS_CANCEL_ALL);

    if ( m_BR )
        *m_BR = 0;

    ct_cmd_drop(m_Cmd);
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
