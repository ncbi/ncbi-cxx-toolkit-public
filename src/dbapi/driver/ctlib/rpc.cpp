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
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_CTlib_Cmds

#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), GetConnection(), &GetBindParams())
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
namespace ftds64_ctlib
{
#endif

/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

CTL_RPCCmd::CTL_RPCCmd(CTL_Connection& conn,
                       const string& proc_name
                       )
: CTL_LRCmd(conn, proc_name)
{
    SetExecCntxInfo("RPC Command: " + GetQuery());
}


CDBParams& 
CTL_RPCCmd::GetBindParams(void)
{
    if (m_InParams.get() == NULL) {
        m_InParams.reset(new impl::CRowInfo_SP_SQL_Server(
                    GetQuery(), 
                    GetConnImpl(), 
                    GetBindParamsImpl()
                    )
                );
    }

    return *m_InParams;
}


bool CTL_RPCCmd::Send()
{
    Cancel();

    SetHasFailed(false);

    CheckSFB(ct_command(x_GetSybaseCmd(), CS_RPC_CMD,
                        const_cast<char*>(GetQuery().data()),
                        GetQuery().size(),
                              NeedToRecompile() ? CS_RECOMPILE : CS_UNUSED),
             "ct_command failed", 121001);

    SetHasFailed(!x_AssignParams());
    CHECK_DRIVER_ERROR( HasFailed(), "Cannot assign the params." + GetDbgInfo(), 121003 );

    return SendInternal();
}


CDB_Result* CTL_RPCCmd::Result()
{
    return MakeResult();
}

bool CTL_RPCCmd::HasMoreResults() const
{
    return WasSent();
}


int CTL_RPCCmd::RowCount() const
{
    return m_RowCount;
}


CTL_RPCCmd::~CTL_RPCCmd()
{
    try {
        DropCmd(*this);

        x_Close();

        DetachInterface();
    }
    NCBI_CATCH_ALL_X( 7, NCBI_CURRENT_FUNCTION )
}


void
CTL_RPCCmd::x_Close(void)
{
    if (x_GetSybaseCmd()) {

        Cancel();

        DropSybaseCmd();
    }
}


bool CTL_RPCCmd::x_AssignParams()
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.format = CS_FMT_UNUSED;

    for (unsigned int i = 0;  i < GetBindParamsImpl().NofParams();  i++) {

        if(GetBindParamsImpl().GetParamStatus(i) == 0) continue;
        CDB_Object&   param      = *GetBindParamsImpl().GetParam(i);
        const string& param_name = GetBindParamsImpl().GetParamName(i);

        param_fmt.status =
            ((GetBindParamsImpl().GetParamStatus(i) & impl::CDB_Params::fOutput) == 0)
            ? CS_INPUTVALUE : CS_RETURN;

        if ( !AssignCmdParam(param,
                             param_name,
                             param_fmt,
                             false/*!declare_only*/) ) {
            return false;
        }
    }

    return true;
}

#ifdef FTDS_IN_USE
} // namespace ftds64_ctlib
#endif

END_NCBI_SCOPE


