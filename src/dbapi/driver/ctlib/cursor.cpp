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
 * File Description:  CTLib cursor command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmd::
//

CTL_CursorCmd::CTL_CursorCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                             const string& cursor_name, const string& query,
                             unsigned int nof_params, unsigned int fetch_size)
    : m_Connect(conn),
      m_Cmd(cmd),
      m_Name(cursor_name),
      m_Query(query),
      m_Params(nof_params),
      m_FetchSize(fetch_size)
{
    m_IsOpen      = false;
    m_HasFailed   = false;
    m_Used        = false;
    m_Res         = 0;
    m_RowCount    = -1;
}


bool CTL_CursorCmd::BindParam(const string& param_name, CDB_Object* param_ptr)
{
    return
        m_Params.BindParam(CDB_Params::kNoParamNumber, param_name, param_ptr);
}


CDB_Result* CTL_CursorCmd::Open()
{
    if ( m_IsOpen ) {
        // need to close it first
        Close();
    }

    if ( !m_Used ) {
        m_HasFailed = false;

        switch ( ct_cursor(m_Cmd, CS_CURSOR_DECLARE,
                           const_cast<char*> (m_Name.c_str()), CS_NULLTERM,
                           const_cast<char*> (m_Query.c_str()), CS_NULLTERM,
                           CS_UNUSED) ) {
        case CS_SUCCEED:
            break;
        case CS_FAIL:
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Fatal, 122001, "CTL_CursorCmd::Open",
                               "ct_cursor(DECLARE) failed");
        case CS_BUSY:
            throw CDB_ClientEx(eDB_Error, 122002, "CTL_CursorCmd::Open",
                               "the connection is busy");
        }

        if (m_Params.NofParams() > 0) {
            // we do have the parameters
            // check if query is a select statement or a function call
            if (m_Query.find("select") != string::npos  ||
                m_Query.find("SELECT") != string::npos) {
                // this is a select
                if ( !x_AssignParams(true) ) {
                    m_HasFailed = true;
                    throw CDB_ClientEx(eDB_Error, 122003,"CTL_CursorCmd::Open",
                                       "cannot declare the params");
                }
            }
        }

        if (m_FetchSize > 1) {
            switch ( ct_cursor(m_Cmd, CS_CURSOR_ROWS, 0, CS_UNUSED,
                               0, CS_UNUSED, (CS_INT) m_FetchSize) ) {
            case CS_SUCCEED:
                break;
            case CS_FAIL:
                m_HasFailed = true;
                throw CDB_ClientEx(eDB_Fatal, 122004, "CTL_CursorCmd::Open",
                                   "ct_cursor(ROWS) failed");
            case CS_BUSY:
                throw CDB_ClientEx(eDB_Error, 122002, "CTL_CursorCmd::Open",
                                   "the connection is busy");
            }
        }
    }

    m_HasFailed = false;

    // open the cursor
    switch ( ct_cursor(m_Cmd, CS_CURSOR_OPEN, 0, CS_UNUSED, 0, CS_UNUSED,
                       m_Used ? CS_RESTORE_OPEN : CS_UNUSED) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 122005, "CTL_CursorCmd::Open",
                           "ct_cursor(open) failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Error, 122002, "CTL_CursorCmd::Open",
                           "the connection is busy");
    }

    m_IsOpen = true;

    if (m_Params.NofParams() > 0) {
        // we do have the parameters
        if ( !x_AssignParams(false) ) {
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Error, 122003, "CTL_CursorCmd::Open",
                               "cannot assign the params");
        }
    }

    // send this command
    switch ( ct_send(m_Cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 122006, "CTL_CursorCmd::Open",
                           "ct_send failed");
    case CS_CANCELED:
        throw CDB_ClientEx(eDB_Error, 122008, "CTL_CursorCmd::Open",
                           "command was canceled");
    case CS_BUSY:
    case CS_PENDING:
        throw CDB_ClientEx(eDB_Error, 122007, "CTL_CursorCmd::Open",
                           "connection has another request pending");
    }

    m_Used = true;

    for (;;) {
        CS_INT res_type;

        switch ( ct_results(m_Cmd, &res_type) ) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            return 0;
        case CS_FAIL:
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Error, 122013, "CTL_CursorCmd::Open",
                               "ct_result failed");
        case CS_CANCELED:
            throw CDB_ClientEx(eDB_Error, 122011, "CTL_CursorCmd::Open",
                               "your command has been canceled");
        case CS_BUSY:
            throw CDB_ClientEx(eDB_Error, 122014, "CTL_CursorCmd::Open",
                               "connection has another request pending");
        default:
            throw CDB_ClientEx(eDB_Error, 122015, "CTL_CursorCmd::Open",
                               "your request is pending");
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE:
            // done with this command -- check the number of affected rows
            g_CTLIB_GetRowCount(m_Cmd, &m_RowCount);
            continue;
        case CS_CMD_FAIL:
            // the command has failed -- check the number of affected rows
            g_CTLIB_GetRowCount(m_Cmd, &m_RowCount);
            m_HasFailed = true;
            while (ct_results(m_Cmd, &res_type) == CS_SUCCEED) {
                continue;
            }
            throw CDB_ClientEx(eDB_Warning, 122016, "CTL_CursorCmd::Open",
                               "The server encountered an error while "
                               "executing a command");
        case CS_CURSOR_RESULT:
            m_Res = new CTL_CursorResult(m_Cmd);
            break;
        default:
            continue;
        }

        return Create_Result(*m_Res);
    }
}


bool CTL_CursorCmd::Update(const string& table_name, const string& upd_query)
{
    if ( !m_IsOpen ) {
        return false;
    }

    switch ( ct_cursor(m_Cmd, CS_CURSOR_UPDATE,
                       const_cast<char*> (table_name.c_str()), CS_NULLTERM,
                       const_cast<char*> (upd_query.c_str()),  CS_NULLTERM,
                       CS_UNUSED) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 122030, "CTL_CursorCmd::Update",
                           "ct_cursor(update) failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Error, 122031, "CTL_CursorCmd::Update",
                           "the connection is busy");
    }


    // send this command
    switch ( ct_send(m_Cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 122032, "CTL_CursorCmd::Update",
                           "ct_send failed");
    case CS_CANCELED:
        throw CDB_ClientEx(eDB_Error, 122033, "CTL_CursorCmd::Update",
                           "command was canceled");
    case CS_BUSY:
    case CS_PENDING:
        throw CDB_ClientEx(eDB_Error, 122034, "CTL_CursorCmd::Update",
                           "connection has another request pending");
    }

    // process the results
    for (;;) {
        CS_INT res_type;

        switch ( ct_results(m_Cmd, &res_type) ) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            return true;
        case CS_FAIL:
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Error, 122035, "CTL_CursorCmd::Update",
                               "ct_result failed");
        case CS_CANCELED:
            throw CDB_ClientEx(eDB_Error, 122036, "CTL_CursorCmd::Update",
                               "your command has been canceled");
        case CS_BUSY:
            throw CDB_ClientEx(eDB_Error, 122037, "CTL_CursorCmd::Update",
                               "connection has another request pending");
        default:
            throw CDB_ClientEx(eDB_Error, 122038, "CTL_CursorCmd::Update",
                               "your request is pending");
        }

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
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            continue;
        case CS_CMD_FAIL: // the command has failed
            m_HasFailed = true;
            while (ct_results(m_Cmd, &res_type) == CS_SUCCEED) {
                continue;
            }
            throw CDB_ClientEx(eDB_Warning, 122039, "CTL_CursorCmd::Update",
                               "The server encountered an error while "
                               "executing a command");
        default:
            continue;
        }
    }
}

I_ITDescriptor* CTL_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!m_IsOpen || (m_Res == 0)) {
        return 0;
    }
    while(m_Res->CurrentItemNo() < item_num) {
        if(!m_Res->SkipItem()) return 0;
    }
    
    I_ITDescriptor* desc= 0;
    if(m_Res->CurrentItemNo() == item_num) {
        desc= m_Res->GetImageOrTextDescriptor();
    }
    else {
        CTL_ITDescriptor* dsc = new CTL_ITDescriptor;
        
        if (ct_data_info(m_Cmd, CS_GET, item_num+1, &dsc->m_Desc)
            != CS_SUCCEED) {
            delete dsc;
            throw CDB_ClientEx(eDB_Error, 130010,
                               "CTL_CursorCmd::UpdateTextImage",
                               "ct_data_info failed");
        }
        desc= dsc;
    }
    return desc;
}

bool CTL_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data, 
				    bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    C_ITDescriptorGuard d_guard(desc);

    return (desc) ? m_Connect->x_SendData(*desc, data, log_it) : false;
}

CDB_SendDataCmd* CTL_CursorCmd::SendDataCmd(unsigned int item_num, size_t size, 
					    bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    C_ITDescriptorGuard d_guard(desc);

    return (desc) ? m_Connect->SendDataCmd(*desc, size, log_it) : 0;
}					    

bool CTL_CursorCmd::Delete(const string& table_name)
{
    if ( !m_IsOpen ) {
        return false;
    }

    switch ( ct_cursor(m_Cmd, CS_CURSOR_DELETE,
                       const_cast<char*> (table_name.c_str()), CS_NULLTERM,
                       0, CS_UNUSED, CS_UNUSED) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 122040, "CTL_CursorCmd::Delete",
                           "ct_cursor(delete) failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Error, 122041, "CTL_CursorCmd::Delete",
                           "the connection is busy");
    }

    // send this command
    switch ( ct_send(m_Cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 122042, "CTL_CursorCmd::Delete",
                           "ct_send failed");
    case CS_CANCELED:
        throw CDB_ClientEx(eDB_Error, 122043, "CTL_CursorCmd::Delete",
                           "command was canceled");
    case CS_BUSY:
    case CS_PENDING:
        throw CDB_ClientEx(eDB_Error, 122044, "CTL_CursorCmd::Delete",
                           "connection has another request pending");
    }

    // process the results
    for (;;) {
        CS_INT res_type;

        switch ( ct_results(m_Cmd, &res_type) ) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            return true;
        case CS_FAIL:
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Error, 122045, "CTL_CursorCmd::Delete",
                               "ct_result failed");
        case CS_CANCELED:
            throw CDB_ClientEx(eDB_Error, 122046, "CTL_CursorCmd::Delete",
                               "your command has been canceled");
        case CS_BUSY:
            throw CDB_ClientEx(eDB_Error, 122047, "CTL_CursorCmd::Delete",
                               "connection has another request pending");
        default:
            throw CDB_ClientEx(eDB_Error, 122048, "CTL_CursorCmd::Delete",
                               "your request is pending");
        }

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
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            continue;
        case CS_CMD_FAIL: // the command has failed
            m_HasFailed = true;
            while(ct_results(m_Cmd, &res_type) == CS_SUCCEED);
            throw CDB_ClientEx(eDB_Warning, 122049, "CTL_CursorCmd::Delete",
                               "The server encountered an error while "
                               "executing a command");
        default:
            continue;
        }
    }
}


int CTL_CursorCmd::RowCount() const
{
    return m_RowCount;
}


bool CTL_CursorCmd::Close()
{
    if ( !m_IsOpen ) {
        return false;
    }

    if (m_Res) {
        delete m_Res;
        m_Res = 0;
    }

    switch ( ct_cursor(m_Cmd, CS_CURSOR_CLOSE, 0, CS_UNUSED, 0, CS_UNUSED,
                       CS_UNUSED) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 122020, "CTL_CursorCmd::Close",
                           "ct_cursor(close) failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Error, 122021, "CTL_CursorCmd::Close",
                           "the connection is busy");
    }

    // send this command
    switch ( ct_send(m_Cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 122022, "CTL_CursorCmd::Close",
                           "ct_send failed");
    case CS_CANCELED:
        throw CDB_ClientEx(eDB_Error, 122023, "CTL_CursorCmd::Close",
                           "command was canceled");
    case CS_BUSY:
    case CS_PENDING:
        throw CDB_ClientEx(eDB_Error, 122024, "CTL_CursorCmd::Close",
                           "connection has another request pending");
    }

    m_IsOpen = false;

    CS_INT res_type;
    for (;;) {
        switch ( ct_results(m_Cmd, &res_type) ) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            return true;
        case CS_FAIL:
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Error, 122025, "CTL_CursorCmd::Close",
                               "ct_result failed");
        case CS_CANCELED:
            throw CDB_ClientEx(eDB_Error, 122026, "CTL_CursorCmd::Close",
                               "your command has been canceled");
        case CS_BUSY:
            throw CDB_ClientEx(eDB_Error, 122027, "CTL_CursorCmd::Close",
                               "connection has another request pending");
        default:
            throw CDB_ClientEx(eDB_Error, 122028, "CTL_CursorCmd::Close",
                               "your request is pending");
        }

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
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE:
            // done with this command
            continue;
        case CS_CMD_FAIL:
            // the command has failed
            m_HasFailed = true;
            while (ct_results(m_Cmd, &res_type) == CS_SUCCEED) {
                continue;
            }
            throw CDB_ClientEx(eDB_Warning, 122029, "CTL_CursorCmd::Close",
                               "The server encountered an error while "
                               "executing a command");
        }
    }
}


void CTL_CursorCmd::Release()
{
    m_BR = 0;
    if ( m_IsOpen ) {
        Close();
        m_IsOpen = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CTL_CursorCmd::~CTL_CursorCmd()
{
    if ( m_BR ) {
        *m_BR = 0;
    }

    if ( m_IsOpen ) {
        Close();
    }

    if ( m_Used ) {
        // deallocate the cursor
        switch ( ct_cursor(m_Cmd, CS_CURSOR_DEALLOC,
                           0, CS_UNUSED, 0, CS_UNUSED, CS_UNUSED) ) {
        case CS_SUCCEED:
            break;
        case CS_FAIL:
            // m_HasFailed = true;
            //throw CDB_ClientEx(eDB_Fatal, 122050, "::~CTL_CursorCmd",
            //                   "ct_cursor(dealloc) failed");
        case CS_BUSY:
            //throw CDB_ClientEx(eDB_Error, 122051, "::~CTL_CursorCmd",
            //                   "the connection is busy");
            ct_cmd_drop(m_Cmd);
            return;
        }

        // send this command
        switch ( ct_send(m_Cmd) ) {
        case CS_SUCCEED:
            break;
        case CS_FAIL:
            // m_HasFailed = true;
            // throw CDB_ClientEx(eDB_Error, 122052, "::~CTL_CursorCmd",
            //                   "ct_send failed");
        case CS_CANCELED:
            // throw CDB_ClientEx(eDB_Error, 122053, "::~CTL_CursorCmd",
            //                   "command was canceled");
        case CS_BUSY:
        case CS_PENDING:
            // throw CDB_ClientEx(eDB_Error, 122054, "::~CTL_CursorCmd",
            //                   "connection has another request pending");
            ct_cmd_drop(m_Cmd);
            return;
        }

        // process the results
        for (bool need_cont = true;  need_cont; ) {
            CS_INT res_type;

            switch ( ct_results(m_Cmd, &res_type) ) {
            case CS_SUCCEED:
                break;
            case CS_END_RESULTS:
                need_cont = false;
                continue;
            case CS_FAIL:
                // m_HasFailed = true;
                //throw CDB_ClientEx(eDB_Error, 122055, "::~CTL_CursorCmd",
                //                   "ct_result failed");
            case CS_CANCELED:                          
                // throw CDB_ClientEx(eDB_Error, 122056, "::~CTL_CursorCmd",
                //                   "your command has been canceled");
            case CS_BUSY:                              
                // throw CDB_ClientEx(eDB_Error, 122057, "::~CTL_CursorCmd",
                //                   "connection has another request pending");
            default:                                   
                //throw CDB_ClientEx(eDB_Error, 122058, "::~CTL_CursorCmd",
                //                   "your request is pending");
                need_cont = false;
                continue;
            }

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
            case CS_CMD_SUCCEED:
            case CS_CMD_DONE: // done with this command
                continue;
            case CS_CMD_FAIL: // the command has failed
                // m_HasFailed = true;
                while (ct_results(m_Cmd, &res_type) == CS_SUCCEED);
                // throw CDB_ClientEx(eDB_Warning, 122059, "::~CTL_CursorCmd",
                //                   "The server encountered an error while "
                //                   "executing a command");
                need_cont = false;
            default:
                continue;
            }
        }
    }

#if 0
    if (ct_cmd_drop(m_Cmd) != CS_SUCCEED) {
        // throw CDB_ClientEx(eDB_Fatal, 122060, "::~CTL_CursorCmd",
        //                   "ct_cmd_drop failed");
    }
#else
    ct_cmd_drop(m_Cmd);
#endif
}


bool CTL_CursorCmd::x_AssignParams(bool declare_only)
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.namelen = CS_NULLTERM;
    param_fmt.status  = CS_INPUTVALUE;

    for (unsigned int i = 0;  i < m_Params.NofParams();  i++) {
        if(m_Params.GetParamStatus(i) == 0) continue;

        CDB_Object&   param = *m_Params.GetParam(i);
        const string& param_name = m_Params.GetParamName(i);
        CS_SMALLINT   indicator = (!declare_only  &&  param.IsNULL()) ? -1 : 0;

        if ( !g_CTLIB_AssignCmdParam(m_Cmd, param, param_name, param_fmt,
                                     indicator, declare_only) ) {
            return false;
        }
    }

    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/05/17 21:12:03  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2003/06/05 16:00:31  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.7  2003/05/16 20:24:24  soussov
 * adds code to skip parameters if it was not set
 *
 * Revision 1.6  2002/09/16 16:34:16  soussov
 * add try catch when canceling in Release method
 *
 * Revision 1.5  2002/05/16 21:35:22  soussov
 * fixes the memory leak in text/image processing
 *
 * Revision 1.4  2002/03/26 15:34:38  soussov
 * new image/text operations added
 *
 * Revision 1.3  2001/11/06 17:59:55  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.2  2001/09/25 16:29:57  soussov
 * fixed typo in CTL_CursorCmd::x_AssignParams
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
