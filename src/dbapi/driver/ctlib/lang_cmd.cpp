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
#include <dbapi/error_codes.hpp>

#include "ctlib_utils.hpp"


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
//  CTL_Cmd::
//

CTL_CmdBase::CTL_CmdBase(CTL_Connection& conn, const string& query)
: CBaseCmd(conn, query)
, m_RowCount(-1)
, m_IsActive(true)
{
    if (conn.m_ActiveCmd) {
        conn.m_ActiveCmd->m_IsActive = false;
    }
    conn.m_ActiveCmd = this;
}


CTL_CmdBase::CTL_CmdBase(CTL_Connection& conn, const string& cursor_name,
                         const string& query)
: CBaseCmd(conn, cursor_name, query)
, m_RowCount(-1)
, m_IsActive(true)
{
    if (conn.m_ActiveCmd) {
        conn.m_ActiveCmd->m_IsActive = false;
    }
    conn.m_ActiveCmd = this;
}


CTL_CmdBase::~CTL_CmdBase(void)
{
    if (m_IsActive) {
        GetConnection().m_ActiveCmd = NULL;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Cmd::
//

CTL_Cmd::CTL_Cmd(CTL_Connection& conn, const string& query)
: CTL_CmdBase(conn, query)
, m_Cmd(NULL)
, m_Res(NULL)
{
    x_Init();
}

CTL_Cmd::CTL_Cmd(CTL_Connection& conn, const string& cursor_name,
                 const string& query)
: CTL_CmdBase(conn, cursor_name, query)
, m_Cmd(NULL)
, m_Res(NULL)
{
    x_Init();
}

void CTL_Cmd::x_Init(void)
{
    CHECK_DRIVER_ERROR(!GetConnection().IsAlive() || !GetConnection().IsOpen(),
                       "Connection is not open or already dead." + GetDbgInfo(),
                       110003 );

    CheckSFB_Internal(
        ct_cmd_alloc(
            GetConnection().GetNativeConnection().GetNativeHandle(),
            &m_Cmd
            ),
        "ct_cmd_alloc failed", 110001
        );
}


CTL_Cmd::~CTL_Cmd(void)
{
    GetCTLExceptionStorage().SetClosingConnect(true);

    try {
        if (!IsDead()) {
            // Check(ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_CURRENT));
            // Check(ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_ALL));
            Check(ct_cmd_drop(x_GetSybaseCmd()));
        }
    }
    NCBI_CATCH_ALL_X( 4, NCBI_CURRENT_FUNCTION )

    GetCTLExceptionStorage().SetClosingConnect(false);
}


CS_RETCODE
CTL_Cmd::CheckSFB_Internal(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    switch (Check(rc)) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        DATABASE_DRIVER_ERROR( msg, msg_num );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    }

    return rc;
}



void CTL_Cmd::DropSybaseCmd(void)
{
    if (!IsDead()) {
#if 0
    if (Check(ct_cmd_drop(x_GetSybaseCmd())) != CS_SUCCEED) {
        DATABASE_DRIVER_FATAL("ct_cmd_drop failed", 122060);
    }
#else
    Check(ct_cmd_drop(x_GetSybaseCmd()));
#endif
    }

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

bool CTL_Cmd::AssignCmdParam(CDB_Object&   param,
                             const string& param_name,
                             CS_DATAFMT&   param_fmt,
                             bool          declare_only)
{
    CS_SMALLINT   indicator  = param.IsNULL() ? -1 : 0;

    {{
        size_t l = param_name.copy(param_fmt.name, CS_MAX_NAME-1);
        param_fmt.name[l] = '\0';
        param_fmt.namelen = l;
    }}


    CS_RETCODE ret_code = CS_FAIL;

    // We HAVE to pass correct data type even in case of NULL value.
    switch ( param.GetType() ) {
    case eDB_Int: {
        CDB_Int& par = dynamic_cast<CDB_Int&> (param);
        param_fmt.datatype = CS_INT_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_INT value = (CS_INT) par.Value();
        ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt,
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
        ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt,
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
        ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt,
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
        ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt,
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
        value.precision = 19;
        if (longlong_to_numeric(v8, 19, value.array) == 0) {
            return false;
        }
        param_fmt.scale     = 0;
        param_fmt.precision = 19;

        ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt,
                            (CS_VOID*) &value, sizeof(value), indicator));
        break;
    }
    case eDB_Char: {
        CDB_Char& par = dynamic_cast<CDB_Char&> (param);
        param_fmt.datatype = CS_CHAR_TYPE;
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) par.Data(),
                                  (CS_INT) par.Size(),
                                  indicator)
                         );
        break;
    }
    case eDB_LongChar: {
        CDB_LongChar& par = dynamic_cast<CDB_LongChar&> (param);
        param_fmt.datatype = CS_LONGCHAR_TYPE;
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) par.Data(),
                                  (CS_INT) par.Size(),
                                  indicator)
                         );
        break;
    }
    case eDB_VarChar: {
        CDB_VarChar& par = dynamic_cast<CDB_VarChar&> (param);

#ifdef FTDS_IN_USE
        param_fmt.datatype = CS_VARCHAR_TYPE;
        if (GetConnection().GetCTLibContext().GetClientEncoding() == eEncoding_UTF8
            &&  !par.IsNULL())
        {
            CStringUTF8 str_check(CUtf8::AsUTF8(par.AsString(),
                                  eEncoding_UTF8, CUtf8::eValidate));
            CUtf8::AsBasicString<TCharUCS2>(str_check);
        }
#else
        param_fmt.datatype = CS_CHAR_TYPE;
#endif

        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) par.Data(),
                                  (CS_INT) par.Size(),
                                  indicator)
                         );
        break;
    }
    case eDB_Binary: {
        CDB_Binary& par = dynamic_cast<CDB_Binary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) par.Value(),
                                  (CS_INT) par.Size(),
                                  indicator)
                         );
        break;
    }
    case eDB_LongBinary: {
        CDB_LongBinary& par = dynamic_cast<CDB_LongBinary&> (param);
#ifdef FTDS_IN_USE
        if (GetConnection().GetServerType() == CDBConnParams::eMSSqlServer)
            param_fmt.datatype = CS_BINARY_TYPE;
        else
#endif
            param_fmt.datatype = CS_LONGBINARY_TYPE;

        if ( declare_only ) {
            break;
        }
        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) par.Value(),
                                  (CS_INT) par.Size(),
                                  indicator)
                         );
        break;
    }
    case eDB_VarBinary: {
        CDB_VarBinary& par = dynamic_cast<CDB_VarBinary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( declare_only ) {
            break;
        }

        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) par.Value(),
                                  (CS_INT) par.Size(),
                                  indicator)
                         );
        break;
    }
    case eDB_Float: {
        CDB_Float& par = dynamic_cast<CDB_Float&> (param);
        param_fmt.datatype = CS_REAL_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_REAL value = (CS_REAL) par.Value();
        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) &value,
                                  CS_UNUSED,
                                  indicator)
                         );
        break;
    }
    case eDB_Double: {
        CDB_Double& par = dynamic_cast<CDB_Double&> (param);
        param_fmt.datatype = CS_FLOAT_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_FLOAT value = (CS_FLOAT) par.Value();
        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) &value,
                                  CS_UNUSED,
                                  indicator)
                         );
        break;
    }
    case eDB_SmallDateTime: {
        CDB_SmallDateTime& par = dynamic_cast<CDB_SmallDateTime&> (param);
        param_fmt.datatype = CS_DATETIME4_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_DATETIME4 dt;
        if (param.IsNULL()) {
            dt.days    = 0;
            dt.minutes = 0;
        } else {
            dt.days    = par.GetDays();
            dt.minutes = par.GetMinutes();
        }

        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) &dt,
                                  CS_UNUSED,
                                  indicator)
                         );
        break;
    }
    case eDB_DateTime: {
        CDB_DateTime& par = dynamic_cast<CDB_DateTime&> (param);
        param_fmt.datatype = CS_DATETIME_TYPE;
        if ( declare_only ) {
            break;
        }

        CS_DATETIME dt;
        if (param.IsNULL()) {
            dt.dtdays = 0;
            dt.dttime = 0;
        } else {
            dt.dtdays = par.GetDays();
            dt.dttime = par.Get300Secs();
        }

        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) &dt,
                                  CS_UNUSED,
                                  indicator)
                         );
        break;
    }
	case eDB_Text:
	case eDB_Image:
	case eDB_Numeric:
	case eDB_UnsupportedType:
		ret_code = CS_FAIL;
        return false;
    }

    if ( declare_only ) {
        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  0,
                                  CS_UNUSED,
                                  0)
                         );
    }

    return (ret_code == CS_SUCCEED);
}


void CTL_Cmd::GetRowCount(int* cnt)
{
    CS_INT n;
    CS_INT outlen;
    if (cnt  &&
        ct_res_info(x_GetSybaseCmd(),
                    CS_ROW_COUNT,
                    &n,
                    CS_UNUSED,
                    &outlen
                    ) == CS_SUCCEED
        && n >= 0  &&  n != CS_NO_COUNT) {
        *cnt = (int) n;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_LRCmd::
//

CTL_LRCmd::CTL_LRCmd(CTL_Connection& conn,
                     const string& query)
: CTL_Cmd(conn, query)
{
}


CTL_LRCmd::~CTL_LRCmd(void)
{
    try {
        // In test mode ... Mon Jul  9 15:20:21 EDT 2007
        // This call is supposed to cancel brocken resultset.
        Cancel();
    }
    NCBI_CATCH_ALL_X( 5, NCBI_CURRENT_FUNCTION )
}


CS_RETCODE
CTL_LRCmd::CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    try {
        switch (Check(rc)) {
        case CS_SUCCEED:
            break;
        case CS_FAIL:
            SetHasFailed();
            DATABASE_DRIVER_ERROR( msg, msg_num );
    #ifdef CS_BUSY
        case CS_BUSY:
            DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
    #endif
        }
    } catch (...) {
        SetHasFailed();
        throw;
    }

    return rc;
}

CTL_RowResult*
CTL_LRCmd::MakeResultInternal(void)
{
    DeleteResult();

    CHECK_DRIVER_ERROR(
        !WasSent(),
        "You need to send a command first." + GetDbgInfo(),
        120010 );

    if (IsDead()) {
        SetHasFailed();
        SetWasSent(false);
        return NULL;
    }

    for (;;) {
        CS_INT res_type;
        CS_RETCODE rc = 0;

        rc = Check(ct_results(x_GetSybaseCmd(), &res_type));

        /* This code causes problems with Test_DropConnection.
        try {
            rc = Check(ct_results(x_GetSybaseCmd(), &res_type));
        } catch (...) {
            // We have to fech out all pending  results ...
            // while (ct_results(x_GetSybaseCmd(), &res_type) == CS_SUCCEED) {
            //     continue;
            // }

            SetHasFailed();
            Cancel();
            SetWasSent(false);

            throw;
        }
        */

        switch (rc) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            SetWasSent(false);
            return NULL;
        case CS_FAIL:
            SetHasFailed();
            Cancel();
            SetWasSent(false);
            DATABASE_DRIVER_ERROR( "ct_result failed." + GetDbgInfo(), 120013 );
        case CS_CANCELED:
            SetWasSent(false);
            DATABASE_DRIVER_ERROR( "Your command has been canceled." + GetDbgInfo(), 120011 );
#ifdef CS_BUSY
        case CS_BUSY:
            DATABASE_DRIVER_ERROR( "Connection has another request pending." + GetDbgInfo(), 120014 );
#endif
        default:
            DATABASE_DRIVER_ERROR( "Your request is pending." + GetDbgInfo(), 120015 );
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
            SetHasFailed();
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

        return &GetResult();
    }
}


CDB_Result*
CTL_LRCmd::MakeResult(void)
{
    CTL_RowResult* result = MakeResultInternal();

    return result ? Create_Result(*result) : NULL;
}


bool
CTL_LRCmd::Cancel(void)
{
    if (WasSent()) {
        DeleteResultInternal();

        if (!IsDead()  &&  GetConnection().IsAlive()) {
            size_t was_timeout = GetConnection().PrepareToCancel();
            try {
                CS_RETCODE retcode = Check(ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_ALL));
                GetConnection().CancelFinished(was_timeout);

                switch (retcode) {
                case CS_SUCCEED:
                    SetWasSent(false);
                    return true;
                case CS_FAIL:
                    DATABASE_DRIVER_ERROR( "ct_cancel failed." + GetDbgInfo(), 120008 );
#ifdef CS_BUSY
                case CS_BUSY:
                    DATABASE_DRIVER_ERROR( "Connection has another request pending." + GetDbgInfo(), 120009 );
#endif
                default:
                    return false;
                }
            }
            catch (CDB_Exception&) {
                GetConnection().CancelFinished(was_timeout);
                throw;
            }
        } else {
            return false;
        }
    }

    return true;
}

bool
CTL_LRCmd::SendInternal(void)
{
    CS_RETCODE rc;

    if (IsDead()) {
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "Connection has died." + GetDbgInfo(), 121008 );
    }

    try {
        rc = Check(ct_send(x_GetSybaseCmd()));
    } catch (...) {
        SetHasFailed();
        Cancel();
        throw;
    }

    switch (rc) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        SetHasFailed();
        Cancel();
        DATABASE_DRIVER_ERROR( "ct_send failed." + GetDbgInfo(), 121005 );
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "Command was canceled." + GetDbgInfo(), 121006 );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "Connection has another request pending." + GetDbgInfo(), 121007 );
#endif
    case CS_PENDING:
    default:
        SetWasSent();
        return false;
    }

    SetWasSent();
    return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_LangCmd::
//

CTL_LangCmd::CTL_LangCmd(CTL_Connection& conn,
                         const string& lang_query
                         )
: CTL_LRCmd(conn, lang_query)
{
    SetExecCntxInfo("SQL Command: \"" + lang_query + "\"");
}


bool CTL_LangCmd::Send()
{
    Cancel();

    SetHasFailed(false);

    CheckSFB(ct_command(x_GetSybaseCmd(), CS_LANG_CMD,
                        const_cast<char*>(GetQuery().data()),
                        GetQuery().size(), CS_END),
             "ct_command failed", 120001);


    SetHasFailed(!x_AssignParams());
    CHECK_DRIVER_ERROR( HasFailed(), "Cannot assign the params." + GetDbgInfo(), 120003 );

    return SendInternal();
}


CDB_Result* CTL_LangCmd::Result()
{
    return MakeResult();
}

bool CTL_LangCmd::HasMoreResults() const
{
    return WasSent();
}


int CTL_LangCmd::RowCount() const
{
    return m_RowCount;
}


CTL_LangCmd::~CTL_LangCmd()
{
    try {
        DropCmd(*this);

        Close();
    }
    NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
}


void
CTL_LangCmd::Close(void)
{
    // ????
    DetachInterface();

    try {
        SetDead(!Cancel());
    } catch (...) {
        SetDead();
        throw;
    }
}


bool CTL_LangCmd::x_AssignParams()
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.status  = CS_INPUTVALUE;

    for (unsigned int i = 0;  i < GetBindParamsImpl().NofParams();  i++) {
        if(GetBindParamsImpl().GetParamStatus(i) == 0) continue;

        CDB_Object&   param      = *GetBindParamsImpl().GetParam(i);
        const string& param_name = GetBindParamsImpl().GetParamName(i);

        if ( !AssignCmdParam(param,
                             param_name,
                             param_fmt,
                             false/*!declare_only*/) ) 
        {
            return false;
        }
    }

    GetBindParamsImpl().LockBinding();
    return true;
}


#ifdef FTDS_IN_USE
} // namespace ftds64_ctlib
#endif

END_NCBI_SCOPE


