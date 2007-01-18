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
 * File Description:  CTLib language command
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Cmd::
//

CTL_Cmd::CTL_Cmd(CTL_Connection* conn,
                 CS_COMMAND* cmd) :
    m_HasFailed(false),
    m_WasSent(false),
    m_RowCount(-1),
    m_Connect(conn),
    m_Cmd(cmd),
    m_Res(NULL)
{
}


CTL_Cmd::~CTL_Cmd(void)
{
}


CS_RETCODE CTL_Cmd::CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    switch (Check(rc)) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( msg, msg_num );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    }

    return rc;
}


CS_RETCODE CTL_Cmd::CheckSentSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    switch (Check(rc)) {
    case CS_SUCCEED:
        m_WasSent = false;
        break;
    case CS_FAIL:
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( msg, msg_num );
#ifdef CS_BUSY
    case CS_BUSY:
        m_WasSent = false;
        // DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    }

    return rc;
}


CS_RETCODE CTL_Cmd::CheckSFBCP(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    switch (Check(rc)) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( msg, msg_num );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "command was canceled", 122008 );
    case CS_PENDING:
        DATABASE_DRIVER_ERROR( "connection has another request pending", 122007 );
    }

    return rc;
}


void CTL_Cmd::DropSybaseCmd(void)
{
#if 0
    if (Check(ct_cmd_drop(x_GetSybaseCmd())) != CS_SUCCEED) {
        throw CDB_ClientEx(eDiag_Fatal, 122060, "::~CTL_CursorCmd",
                          "ct_cmd_drop failed");
    }
#else
    Check(ct_cmd_drop(x_GetSybaseCmd()));
#endif

    m_Cmd = NULL;
}

bool
CTL_Cmd::ProcessResultInternal(CDB_Result& res)
{
    if(GetConnection().GetResultProcessor()) {
        GetConnection().GetResultProcessor()->ProcessResult(res);
        return true;
    }

    return false;
}

bool
CTL_Cmd::ProcessResults(void)
{
    // process the results
    for (;;) {
        CS_INT res_type;

        if (CheckSFBCP(ct_results(x_GetSybaseCmd(), &res_type),
                       "ct_result failed", 122045) == CS_END_RESULTS) {
            return true;
        }

        if (ProcessResultInternal(res_type)) {
            continue;
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            continue;
        case CS_CMD_FAIL: // the command has failed
            m_HasFailed = true;
            while(Check(ct_results(x_GetSybaseCmd(), &res_type)) == CS_SUCCEED);
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 122049 );
        default:
            continue;
        }
    }

    return false;
}

bool
CTL_Cmd::Cancel(void)
{
    if ( m_WasSent ) {
        if ( HaveResult() ) {
            // to prevent ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_CURRENT) call:
            ((CTL_RowResult*)m_Res)->m_EOR = true;

            DeleteResult();
        }

        switch ( Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL)) ) {
        case CS_SUCCEED:
            m_WasSent = false;
            return true;
        case CS_FAIL:
            DATABASE_DRIVER_ERROR( "ct_cancel failed", 120008 );
#ifdef CS_BUSY
        case CS_BUSY:
            DATABASE_DRIVER_ERROR( "connection has another request pending", 120009 );
#endif
        default:
            return false;
        }
    }

    return true;
}

bool CTL_Cmd::AssignCmdParam(CDB_Object&   param,
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
        if ( declare_only ) {
            break;
        }

        CS_INT value = (CS_INT) par.Value();
        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                                  (CS_VOID*) &value, CS_UNUSED, indicator));
        break;
    }
    case eDB_SmallInt: {
        CDB_SmallInt& par = dynamic_cast<CDB_SmallInt&> (param);
        param_fmt.datatype = CS_SMALLINT_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_SMALLINT value = (CS_SMALLINT) par.Value();
        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator));
        break;
    }
    case eDB_TinyInt: {
        CDB_TinyInt& par = dynamic_cast<CDB_TinyInt&> (param);
        param_fmt.datatype = CS_TINYINT_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_TINYINT value = (CS_TINYINT) par.Value();
        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator));
        break;
    }
    case eDB_Bit: {
        CDB_Bit& par = dynamic_cast<CDB_Bit&> (param);
        param_fmt.datatype = CS_BIT_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_BIT value = (CS_BIT) par.Value();
        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator));
        break;
    }
    case eDB_BigInt: {
        CDB_BigInt& par = dynamic_cast<CDB_BigInt&> (param);
        param_fmt.datatype = CS_NUMERIC_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_NUMERIC value;
        Int8 v8 = par.Value();
        memset(&value, 0, sizeof(value));
        value.precision= 18;
        if (longlong_to_numeric(v8, 18, value.array) == 0)
            return false;

        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator));
        break;
    }
    case eDB_Char: {
        CDB_Char& par = dynamic_cast<CDB_Char&> (param);
        param_fmt.datatype = CS_CHAR_TYPE;
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(m_Cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator));
        break;
    }
    case eDB_LongChar: {
        CDB_LongChar& par = dynamic_cast<CDB_LongChar&> (param);
#ifndef FTDS_IN_USE
        param_fmt.datatype = CS_LONGCHAR_TYPE;
#else
        // CS_LONGCHAR_TYPE is not supported with the FreeTDS ctlib library ...
        // param_fmt.datatype = CS_UNICHAR_TYPE; - Unicode datatype ...
        param_fmt.datatype = CS_CHAR_TYPE;
#endif
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(m_Cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator));
        break;
    }
    case eDB_VarChar: {
        CDB_VarChar& par = dynamic_cast<CDB_VarChar&> (param);

// #if defined(CS_NCHAR_TYPE)
//         param_fmt.datatype = IsMultibyteClientEncoding() ? CS_NCHAR_TYPE : CS_CHAR_TYPE;
// #else
//         param_fmt.datatype = CS_CHAR_TYPE;
// #endif

        param_fmt.datatype = CS_CHAR_TYPE;

        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(m_Cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator));
        break;
    }
    case eDB_Binary: {
        CDB_Binary& par = dynamic_cast<CDB_Binary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(m_Cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator));
        break;
    }
    case eDB_LongBinary: {
        CDB_LongBinary& par = dynamic_cast<CDB_LongBinary&> (param);
        param_fmt.datatype = CS_LONGBINARY_TYPE;
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(m_Cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator));
        break;
    }
    case eDB_VarBinary: {
        CDB_VarBinary& par = dynamic_cast<CDB_VarBinary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(m_Cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator));
        break;
    }
    case eDB_Float: {
        CDB_Float& par = dynamic_cast<CDB_Float&> (param);
        param_fmt.datatype = CS_REAL_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_REAL value = (CS_REAL) par.Value();
        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator));
        break;
    }
    case eDB_Double: {
        CDB_Double& par = dynamic_cast<CDB_Double&> (param);
        param_fmt.datatype = CS_FLOAT_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_FLOAT value = (CS_FLOAT) par.Value();
        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator));
        break;
    }
    case eDB_SmallDateTime: {
        CDB_SmallDateTime& par = dynamic_cast<CDB_SmallDateTime&> (param);
        param_fmt.datatype = CS_DATETIME4_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_DATETIME4 dt;
        dt.days    = par.GetDays();
        dt.minutes = par.GetMinutes();
        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                            (CS_VOID*) &dt, CS_UNUSED, indicator));
        break;
    }
    case eDB_DateTime: {
        CDB_DateTime& par = dynamic_cast<CDB_DateTime&> (param);
        param_fmt.datatype = CS_DATETIME_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_DATETIME dt;
        dt.dtdays = par.GetDays();
        dt.dttime = par.Get300Secs();
        ret_code = Check(ct_param(m_Cmd, &param_fmt,
                            (CS_VOID*) &dt, CS_UNUSED, indicator));
        break;
    }
    default:
        return false;
    }

    if ( declare_only ) {
        ret_code = Check(ct_param(m_Cmd, &param_fmt, 0, CS_UNUSED, 0));
    }

    return (ret_code == CS_SUCCEED);
}


void CTL_Cmd::GetRowCount(int* cnt)
{
    CS_INT n;
    CS_INT outlen;
    if (cnt  &&
        ct_res_info(m_Cmd, CS_ROW_COUNT, &n, CS_UNUSED, &outlen) == CS_SUCCEED
        && n >= 0  &&  n != CS_NO_COUNT) {
        *cnt = (int) n;
    }
}

CDB_Result* CTL_Cmd::MakeResult(void)
{
    DeleteResult();

    CHECK_DRIVER_ERROR(
        !m_WasSent,
        "you need to send a command first",
        120010 );

    for (;;) {
        CS_INT res_type;

        switch ( Check(ct_results(x_GetSybaseCmd(), &res_type)) ) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            m_WasSent = false;
            return 0;
        case CS_FAIL:
            m_HasFailed = true;
            if (Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL)) != CS_SUCCEED) {
                // we need to close this connection
                DATABASE_DRIVER_ERROR(
                    "Unrecoverable crash of ct_result. "
                    "Connection must be closed",
                    120012 );
            }
            m_WasSent = false;
            DATABASE_DRIVER_ERROR( "ct_result failed", 120013 );
        case CS_CANCELED:
            m_WasSent = false;
            DATABASE_DRIVER_ERROR( "your command has been canceled", 120011 );
#ifdef CS_BUSY
        case CS_BUSY:
            DATABASE_DRIVER_ERROR( "connection has another request pending", 120014 );
#endif
        default:
            DATABASE_DRIVER_ERROR( "your request is pending", 120015 );
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            // check the number of affected rows
            GetRowCount(&m_RowCount);
            continue;
        case CS_CMD_FAIL: // the command has failed
            // check the number of affected rows
            GetRowCount(&m_RowCount);
            m_HasFailed = true;
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 120016 );
        case CS_ROW_RESULT:
            SetResult(MakeRowResult());
            break;
        case CS_PARAM_RESULT:
            SetResult(MakeParamResult());
            break;
        case CS_COMPUTE_RESULT:
            SetResult(MakeComputeResult());
            break;
        case CS_STATUS_RESULT:
            SetResult(MakeStatusResult());
            break;
        case CS_COMPUTEFMT_RESULT:
            DATABASE_DRIVER_INFO( "CS_COMPUTEFMT_RESULT has arrived", 120017 );
        case CS_ROWFMT_RESULT:
            DATABASE_DRIVER_INFO( "CS_ROWFMT_RESULT has arrived", 120018 );
        case CS_MSG_RESULT:
            DATABASE_DRIVER_INFO( "CS_MSG_RESULT has arrived", 120019 );
        default:
            DATABASE_DRIVER_WARNING( "Unexpected result type has arrived", 120020 );
        }

        return CTL_Cmd::CreateResult();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
//  CTL_LangCmd::
//

CTL_LangCmd::CTL_LangCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                         const string& lang_query, unsigned int nof_params) :
    CTL_Cmd(conn, cmd),
    impl::CBaseCmd(lang_query, nof_params)
{
    SetExecCntxInfo("SQL Command: \"" + lang_query + "\"");
}


bool CTL_LangCmd::Send()
{
    Cancel();

    m_HasFailed = false;

    CheckSFB(ct_command(x_GetSybaseCmd(), CS_LANG_CMD,
                        const_cast<char*> (GetQuery().c_str()), CS_NULLTERM,
                        CS_END),
             "ct_command failed", 120001);


    m_HasFailed = !x_AssignParams();
    CHECK_DRIVER_ERROR( m_HasFailed, "cannot assign the params", 120003 );

    switch ( Check(ct_send(x_GetSybaseCmd())) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        if (Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL)) != CS_SUCCEED) {
            // we need to close this connection
            DATABASE_DRIVER_ERROR( "Unrecoverable crash of ct_send. "
                               "Connection must be closed", 120004 );
        }
        DATABASE_DRIVER_ERROR( "ct_send failed", 120005 );
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "command was canceled", 120006 );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "connection has another request pending", 120007 );
#endif
    case CS_PENDING:
    default:
        m_WasSent = true;
        return false;
    }

    m_WasSent = true;
    return true;
}


bool CTL_LangCmd::WasSent() const
{
    return m_WasSent;
}


bool CTL_LangCmd::Cancel()
{
    // m_Query.erase();
    return CTL_Cmd::Cancel();
}


bool CTL_LangCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CTL_LangCmd::Result()
{
    return MakeResult();
}

void CTL_LangCmd::DumpResults()
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


bool CTL_LangCmd::HasMoreResults() const
{
    return m_WasSent;
}


bool CTL_LangCmd::HasFailed() const
{
    return m_HasFailed;
}


int CTL_LangCmd::RowCount() const
{
    return m_RowCount;
}


CDB_Result*
CTL_LangCmd::CreateResult(impl::CResult& result)
{
    return Create_Result(result);
}

CTL_LangCmd::~CTL_LangCmd()
{
    try {
        DropCmd(*this);

        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


void
CTL_LangCmd::Close(void)
{
    // ????
    DetachInterface();

    Cancel();

    Check(ct_cmd_drop(x_GetSybaseCmd()));

    SetSybaseCmd(NULL);
}


bool CTL_LangCmd::x_AssignParams()
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.namelen = CS_NULLTERM;
    param_fmt.status  = CS_INPUTVALUE;

    for (unsigned int i = 0;  i < GetParams().NofParams();  i++) {
        if(GetParams().GetParamStatus(i) == 0) continue;

        CDB_Object&   param      = *GetParams().GetParam(i);
        const string& param_name = GetParams().GetParamName(i);
        CS_SMALLINT   indicator  = param.IsNULL() ? -1 : 0;

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


