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
 * File Description:  CTLib RPC command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

CTL_RPCCmd::CTL_RPCCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                       const string& proc_name, unsigned int nof_params)
    : m_Connect(conn),
      m_Cmd(cmd),
      m_Query(proc_name),
      m_Params(nof_params)
{
    m_WasSent     = false;
    m_HasFailed   = false;
    m_Recompile   = false;
    m_Res         = 0;
    m_RowCount    = -1;
}


bool CTL_RPCCmd::BindParam(const string& param_name, CDB_Object* param_ptr,
                           bool out_param)
{
    return m_Params.BindParam(CDB_Params::kNoParamNumber, param_name,
                              param_ptr, out_param);
}


bool CTL_RPCCmd::SetParam(const string& param_name, CDB_Object* param_ptr,
                          bool out_param)
{
    return m_Params.SetParam(CDB_Params::kNoParamNumber, param_name,
                             param_ptr, out_param);
}


bool CTL_RPCCmd::Send()
{
    if ( m_WasSent ) {
        Cancel();
    }

    m_HasFailed = false;

    switch ( ct_command(m_Cmd, CS_RPC_CMD,
                        const_cast<char*> (m_Query.c_str()), CS_NULLTERM,
                        m_Recompile ? CS_RECOMPILE : CS_UNUSED) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        DATABASE_DRIVER_FATAL( "ct_command failed", 121001 );
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 121002 );
    }

    m_HasFailed = !x_AssignParams();
    CHECK_DRIVER_ERROR( m_HasFailed, "cannot assign the params", 121003 );

    switch ( ct_send(m_Cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        if (ct_cancel(0, m_Cmd, CS_CANCEL_ALL) != CS_SUCCEED) {
            // we need to close this connection
            DATABASE_DRIVER_FATAL( "Unrecoverable crash of ct_send. "
                               "Connection must be closed", 121004 );
        }
        DATABASE_DRIVER_ERROR( "ct_send failed", 121005 );
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "command was canceled", 121006 );
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "connection has another request pending", 121007 );
    case CS_PENDING:
    default:
        m_WasSent = true;
        return false;
    }

    m_WasSent = true;
    return true;
}


bool CTL_RPCCmd::WasSent() const
{
    return m_WasSent;
}


bool CTL_RPCCmd::Cancel()
{
    if ( !m_WasSent ) {
        return true;
    }

    if ( m_Res ) {
        // to prevent ct_cancel(NULL, m_Cmd, CS_CANCEL_CURRENT) call:
        ((CTL_RowResult*)m_Res)->m_EOR= true; 
        delete m_Res;
        m_Res = 0;
    }

    switch ( ct_cancel(0, m_Cmd, CS_CANCEL_ALL) ) {
    case CS_SUCCEED:
        m_WasSent = false;
        return true;
    case CS_FAIL:
        DATABASE_DRIVER_ERROR( "ct_cancel failed", 121008 );
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "connection has another request pending", 121009 );
    default:
        return false;
    }
}


bool CTL_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CTL_RPCCmd::Result()
{
    if ( m_Res ) {
        delete m_Res;
        m_Res = 0;
    }

    CHECK_DRIVER_ERROR( 
        !m_WasSent, 
        "you need to send a command first", 
        121010 );

    for (;;) {
        CS_INT res_type;

        switch ( ct_results(m_Cmd, &res_type) ) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            m_WasSent = false;
            return 0;
        case CS_FAIL:
            m_HasFailed = true;
            if (ct_cancel(0, m_Cmd, CS_CANCEL_ALL) != CS_SUCCEED) {
                // we need to close this connection
                DATABASE_DRIVER_FATAL( "Unrecoverable crash of ct_result. "
                                   "Connection must be closed", 121012 );
            }
            m_WasSent = false;
            DATABASE_DRIVER_ERROR( "ct_result failed", 121013 );
        case CS_CANCELED:
            m_WasSent = false;
            DATABASE_DRIVER_ERROR( "your command has been canceled", 121011 );
        case CS_BUSY:
            DATABASE_DRIVER_ERROR( "connection has another request pending", 121014 );
        default:
            DATABASE_DRIVER_ERROR( "your request is pending", 121015 );
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            // check the number of affected rows
            g_CTLIB_GetRowCount(m_Cmd, &m_RowCount);
            continue;
        case CS_CMD_FAIL: // the command has failed
            g_CTLIB_GetRowCount(m_Cmd, &m_RowCount);
            m_HasFailed = true;
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 121016 );
        case CS_ROW_RESULT:
            m_Res = new CTL_RowResult(m_Cmd);
            break;
        case CS_PARAM_RESULT:
            m_Res = new CTL_ParamResult(m_Cmd);
            break;
        case CS_COMPUTE_RESULT:
            m_Res = new CTL_ComputeResult(m_Cmd);
            break;
        case CS_STATUS_RESULT:
            m_Res = new CTL_StatusResult(m_Cmd);
            break;
        case CS_COMPUTEFMT_RESULT:
            DATABASE_DRIVER_INFO( "CS_COMPUTEFMT_RESULT has arrived", 121017 );
        case CS_ROWFMT_RESULT:
            DATABASE_DRIVER_INFO( "CS_ROWFMT_RESULT has arrived", 121018 );
        case CS_MSG_RESULT:
            DATABASE_DRIVER_INFO( "CS_MSG_RESULT has arrived", 121019 );
        default:
            DATABASE_DRIVER_WARNING( "Unexpected result type has arrived", 121020 );
        }

        return Create_Result(*m_Res);
    }
}

void CTL_RPCCmd::DumpResults()
{
    if ( m_Res ) {
        delete m_Res;
        m_Res = 0;
    }

    CHECK_DRIVER_ERROR( 
        !m_WasSent, 
        "you need to send a command first", 
        121010 );

    for (;;) {
        CS_INT res_type;

        switch ( ct_results(m_Cmd, &res_type) ) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            m_WasSent = false;
            return;
        case CS_FAIL:
            m_HasFailed = true;
            if (ct_cancel(0, m_Cmd, CS_CANCEL_ALL) != CS_SUCCEED) {
                // we need to close this connection
                DATABASE_DRIVER_FATAL( "Unrecoverable crash of ct_result. "
                                   "Connection must be closed", 121012 );
            }
            m_WasSent = false;
            DATABASE_DRIVER_ERROR( "ct_result failed", 121013 );
        case CS_CANCELED:
            m_WasSent = false;
            DATABASE_DRIVER_ERROR( "your command has been canceled", 121011 );
        case CS_BUSY:
            DATABASE_DRIVER_ERROR( "connection has another request pending", 121014 );
        default:
            DATABASE_DRIVER_ERROR( "your request is pending", 121015 );
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            // check the number of affected rows
            g_CTLIB_GetRowCount(m_Cmd, &m_RowCount);
            continue;
        case CS_CMD_FAIL: // the command has failed
            g_CTLIB_GetRowCount(m_Cmd, &m_RowCount);
            m_HasFailed = true;
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 121016 );
        case CS_ROW_RESULT:
            m_Res = new CTL_RowResult(m_Cmd);
            break;
        case CS_PARAM_RESULT:
            m_Res = new CTL_ParamResult(m_Cmd);
            break;
        case CS_COMPUTE_RESULT:
            m_Res = new CTL_ComputeResult(m_Cmd);
            break;
        case CS_STATUS_RESULT:
            m_Res = new CTL_StatusResult(m_Cmd);
            break;
        case CS_COMPUTEFMT_RESULT:
            DATABASE_DRIVER_INFO( "CS_COMPUTEFMT_RESULT has arrived", 121017 );
        case CS_ROWFMT_RESULT:
            DATABASE_DRIVER_INFO( "CS_ROWFMT_RESULT has arrived", 121018 );
        case CS_MSG_RESULT:
            DATABASE_DRIVER_INFO( "CS_MSG_RESULT has arrived", 121019 );
        default:
            DATABASE_DRIVER_WARNING( "Unexpected result type has arrived", 121020 );
        }

        if(m_Res) {
            if(m_Connect->m_ResProc) {
                CDB_Result* r= Create_Result(*m_Res);
                m_Connect->m_ResProc->ProcessResult(*r);
                delete r;
            }
            else {
                while(m_Res->Fetch());
            }
            delete m_Res;
            m_Res= 0;
        }
    }
}

bool CTL_RPCCmd::HasMoreResults() const
{
    return m_WasSent;
}


bool CTL_RPCCmd::HasFailed() const
{
    return m_HasFailed;
}


int CTL_RPCCmd::RowCount() const
{
    return m_RowCount;
}


void CTL_RPCCmd::SetRecompile(bool recompile)
{
    m_Recompile = recompile;
}


void CTL_RPCCmd::Release()
{
    m_BR = 0;
    if ( m_WasSent) {
        try {
            Cancel();
        } catch (CDB_Exception& ) {}
        m_WasSent = false;
    }

    m_Connect->DropCmd(*this);
    delete this;
}


CTL_RPCCmd::~CTL_RPCCmd()
{
    if ( m_BR ) {
        *m_BR = 0;
    }

    if ( m_WasSent ) {
        try {
            Cancel();
        } catch (CDB_Exception& ) {}
    }

    ct_cmd_drop(m_Cmd);
}


bool CTL_RPCCmd::x_AssignParams()
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.namelen = CS_NULLTERM;

    for (unsigned int i = 0;  i < m_Params.NofParams();  i++) {

        if(m_Params.GetParamStatus(i) == 0) continue;
        CDB_Object&   param      = *m_Params.GetParam(i);
        const string& param_name = m_Params.GetParamName(i);
        CS_SMALLINT   indicator  = param.IsNULL() ? -1 : 0;

        param_fmt.status =
            ((m_Params.GetParamStatus(i) & CDB_Params::fOutput) == 0)
            ? CS_INPUTVALUE : CS_RETURN;

        if ( !g_CTLIB_AssignCmdParam(m_Cmd, param, param_name, param_fmt,
                                     indicator, false/*!declare_only*/) ) {
            return false;
        }
    }

    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.9  2004/05/17 21:12:03  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2003/10/07 16:09:48  soussov
 * adds code which prevents ct_cancel(NULL, m_Cmd, CS_CANCEL_CURRENT) call if command is canceled while retrieving results
 *
 * Revision 1.7  2003/06/24 18:42:26  soussov
 * removes throwing exception from destructor
 *
 * Revision 1.6  2003/06/05 16:00:31  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.5  2003/05/16 20:24:24  soussov
 * adds code to skip parameters if it was not set
 *
 * Revision 1.4  2003/01/31 16:49:38  lavr
 * Remove unused variable "e" from catch() clause
 *
 * Revision 1.3  2002/09/16 19:40:03  soussov
 * add try catch when canceling in Release method
 *
 * Revision 1.2  2001/11/06 17:59:55  lavr
 * Formatted uniformly as the rest of the library
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
