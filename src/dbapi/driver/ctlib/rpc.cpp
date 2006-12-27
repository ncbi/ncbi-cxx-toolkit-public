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



/*
 * ===========================================================================
 * $Log$
 * Revision 1.26  2006/12/27 21:29:37  ssikorsk
 * Call SetExecCntxInfo() in constructor.
 *
 * Revision 1.25  2006/11/28 20:08:09  ssikorsk
 * Replaced NCBI_CATCH_ALL(kEmptyStr) with NCBI_CATCH_ALL(NCBI_CURRENT_FUNCTION)
 *
 * Revision 1.24  2006/11/20 18:15:58  ssikorsk
 * Revamp code to use GetQuery() and GetParams() methods.
 *
 * Revision 1.23  2006/08/02 15:15:58  ssikorsk
 * Revamp code to use CheckSFB and CheckSFBCP;
 * Revamp code to compile with FreeTDS ctlib implementation;
 *
 * Revision 1.22  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.21  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.20  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.19  2006/06/05 21:09:21  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.18  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.17  2006/06/05 18:10:06  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.16  2006/05/03 15:10:36  ssikorsk
 * Implemented classs CTL_Cmd and CCTLExceptions;
 * Surrounded each native ctlib call with Check;
 *
 * Revision 1.15  2006/03/06 19:51:38  ssikorsk
 *
 * Added method Close/CloseForever to all context/command-aware classes.
 * Use getters to access Sybase's context and command handles.
 *
 * Revision 1.14  2006/02/22 15:15:50  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.13  2005/12/06 19:13:08  ssikorsk
 * Catch exceptions by const ref
 *
 * Revision 1.12  2005/09/19 14:19:02  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.11  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
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
