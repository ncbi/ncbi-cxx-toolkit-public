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
#include <dbapi/driver/public.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

CTL_RPCCmd::CTL_RPCCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                       const string& proc_name, unsigned int nof_params) :
    CTL_Cmd(conn, cmd),
    impl::CBaseCmd(proc_name, nof_params)
{
    SetExecCntxInfo("RPC Command: " + proc_name);
}


bool CTL_RPCCmd::Send()
{
    Cancel();

    m_HasFailed = false;

    CheckSFB(ct_command(x_GetSybaseCmd(), CS_RPC_CMD,
                        const_cast<char*> (GetQuery().c_str()), CS_NULLTERM,
                              NeedToRecompile() ? CS_RECOMPILE : CS_UNUSED),
             "ct_command failed", 121001);

    m_HasFailed = !x_AssignParams();
    CHECK_DRIVER_ERROR( m_HasFailed, "cannot assign the params", 121003 );

    switch ( Check(ct_send(x_GetSybaseCmd())) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        if (Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL)) != CS_SUCCEED) {
            // we need to close this connection
            DATABASE_DRIVER_ERROR( "Unrecoverable crash of ct_send. "
                               "Connection must be closed", 121004 );
        }
        DATABASE_DRIVER_ERROR( "ct_send failed", 121005 );
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "command was canceled", 121006 );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "connection has another request pending", 121007 );
#endif
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
    return CTL_Cmd::Cancel();
}


bool CTL_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CTL_RPCCmd::Result()
{
    return MakeResult();
}

void CTL_RPCCmd::DumpResults()
{
    auto_ptr<CDB_Result> res(Result());

    while (res.get()) {
        if (!ProcessResultInternal(*res)) {
            while(res->Fetch());
        }

        DeleteResult();

        res.reset(Result());
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


CDB_Result*
CTL_RPCCmd::CreateResult(impl::CResult& result)
{
    return Create_Result(result);
}


CTL_RPCCmd::~CTL_RPCCmd()
{
    try {
        DetachInterface();

        DropCmd(*this);

        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


void
CTL_RPCCmd::Close(void)
{
    if (x_GetSybaseCmd()) {

        // ????
        DetachInterface();

        Cancel();

        DropSybaseCmd();
    }
}


bool CTL_RPCCmd::x_AssignParams()
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.namelen = CS_NULLTERM;

    for (unsigned int i = 0;  i < GetParams().NofParams();  i++) {

        if(GetParams().GetParamStatus(i) == 0) continue;
        CDB_Object&   param      = *GetParams().GetParam(i);
        const string& param_name = GetParams().GetParamName(i);
        CS_SMALLINT   indicator  = param.IsNULL() ? -1 : 0;

        param_fmt.status =
            ((GetParams().GetParamStatus(i) & CDB_Params::fOutput) == 0)
            ? CS_INPUTVALUE : CS_RETURN;

        if ( !AssignCmdParam(param,
                             param_name,
                             param_fmt,
                             indicator,
                             false/*!declare_only*/) ) {
            return false;
        }
    }

    return true;
}


END_NCBI_SCOPE


