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
        GetDbgInfo(), GetConnection(), GetLastParams())
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
namespace NCBI_NS_FTDS_CTLIB
{
#endif

/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CmdBase::
//

CTL_CmdBase::CTL_CmdBase(CTL_Connection& conn, const string& query)
: CBaseCmd(conn, query)
, m_RowCount(-1)
, m_DbgInfo(new TDbgInfo(conn.GetDbgInfo()))
, m_IsActive(true)
, m_TimedOut(false)
, m_Retriable(eRetriable_Unknown)
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
, m_DbgInfo(new TDbgInfo(conn.GetDbgInfo()))
, m_IsActive(true)
, m_TimedOut(false)
, m_Retriable(eRetriable_Unknown)
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


CS_RETCODE
CTL_CmdBase::Check(CS_RETCODE rc)
{
    // The connection::Check() may or may not throw an exception.
    // If it did not then an exception may be thrown later (e.g. in case of
    // a timeout). Two attributes: retriability and the fact that it was a
    // timeout needs to be memorized in the command.
    SetTimedOut(GetCTLExceptionStorage().GetHasTimeout());
    SetRetriable(GetCTLExceptionStorage().GetRetriable());
    return GetConnection().Check(rc, GetDbgInfo());
}


void CTL_CmdBase::EnsureActiveStatus(void)
{
    if ( !m_IsActive ) {
        CTL_Connection& conn = GetConnection();
        if (conn.m_ActiveCmd) {
            conn.m_ActiveCmd->m_IsActive = false;
        }
        conn.m_ActiveCmd = this;
        m_IsActive = true;
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
        }
        Check(ct_cmd_drop(x_GetSybaseCmd()));
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
        if (GetConnection().IsAlive()) {
            DATABASE_DRIVER_ERROR(msg, msg_num);
        } else {
            DATABASE_DRIVER_ERROR("Connection has died.", 122010);
        }
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
        CS_INT l = (CS_INT) param_name.copy(param_fmt.name, CS_MAX_NAME-1);
        param_fmt.name[l] = '\0';
        param_fmt.namelen = l;
    }}
    param_fmt.maxlength = 0;


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
        if ( (param_fmt.status & CS_RETURN) != 0) {
            param_fmt.maxlength = 8000;
        }
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
        if ( (param_fmt.status & CS_RETURN) != 0) {
            param_fmt.maxlength = 8000;
        }
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

        param_fmt.datatype = NCBI_CS_STRING_TYPE;
        if ( (param_fmt.status & CS_RETURN) != 0) {
            param_fmt.maxlength = 8000;
        }
#ifdef FTDS_IN_USE
        if (GetConnection().GetCTLibContext().GetClientEncoding() == eEncoding_UTF8
            &&  !par.IsNULL())
        {
            CStringUTF8 str_check(CUtf8::AsUTF8(par.AsString(),
                                  eEncoding_UTF8, CUtf8::eValidate));
            CUtf8::AsBasicString<TCharUCS2>(str_check);
        }
#endif

        if ( declare_only ) {
            break;
        }

        CTempString  ts;
        CTempVarChar varchar;
        if (par.Size() <= kMax_UI1) {
            varchar.SetValue(par.Value());
            ts = varchar.GetValue();
        } else {
            param_fmt.datatype = CS_LONGCHAR_TYPE;
            ts.assign(par.Data(), par.Size());
        }

        ret_code = Check(ct_param(x_GetSybaseCmd(),
                                  &param_fmt,
                                  (CS_VOID*) ts.data(),
                                  (CS_INT) ts.size(),
                                  indicator)
                         );
        break;
    }
    case eDB_Binary: {
        CDB_Binary& par = dynamic_cast<CDB_Binary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( (param_fmt.status & CS_RETURN) != 0) {
            param_fmt.maxlength = 8000;
        }
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
        if (GetConnection().GetServerType() == CDBConnParams::eMSSqlServer) {
#  ifdef CS_TDS_72
            if (GetConnection().m_TDSVersion >= CS_TDS_72)
                param_fmt.datatype = CS_IMAGE_TYPE;
            else
#  endif
            {
                param_fmt.datatype = CS_BINARY_TYPE;
                if ( (param_fmt.status & CS_RETURN) != 0) {
                    param_fmt.maxlength = 8000;
                }
            }
        } else
#endif
        {
            param_fmt.datatype = CS_LONGBINARY_TYPE;
            if ( (param_fmt.status & CS_RETURN) != 0) {
                param_fmt.maxlength = 8000;
            }
        }

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
#if defined(FTDS_IN_USE)  &&  defined(CS_TDS_72)
        if (GetConnection().m_TDSVersion >= CS_TDS_72)
            param_fmt.datatype = CS_IMAGE_TYPE;
        else
#endif
        {
            param_fmt.datatype = CS_BINARY_TYPE;
            if ( (param_fmt.status & CS_RETURN) != 0) {
                param_fmt.maxlength = 8000;
            }
        }
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
    case eDB_BigDateTime: {
        CDB_BigDateTime& par = dynamic_cast<CDB_BigDateTime&> (param);
        const CTime&     t   = par.GetCTime();
        CS_DATETIME      dt  = { 0, 0 };
        param_fmt.datatype = CS_DATETIME_TYPE;
        if ( !param.IsNULL() ) {
            TDBTimeI dbt = t.GetTimeDBI();
            if (CTime().SetTimeDBI(dbt) == t) {
                dt.dtdays = dbt.days;
                dt.dttime = dbt.time;
            } else {
                param_fmt.datatype = CS_CHAR_TYPE;
            }
        }
        if (declare_only) {
            break;
        }

        if (param_fmt.datatype == CS_CHAR_TYPE) {
            CTime lt = t.GetLocalTime();
            lt.SetNanoSecond(lt.NanoSecond() / 100 * 100);
            string s = lt.AsString(CDB_BigDateTime::GetTimeFormat
                                   (GetConnection().GetDateTimeSyntax(),
                                    par.GetSQLType()));
            ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt,
                                      (CS_VOID*) s.data(), (CS_INT) s.size(),
                                      indicator));
        } else {
            ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt,
                                      (CS_VOID*) &dt, CS_UNUSED, indicator));
        }
        break;
    }
	case eDB_Text:
	case eDB_Image:
    case eDB_VarCharMax:
    case eDB_VarBinaryMax:
    {
        CDB_Stream& par = dynamic_cast<CDB_Stream&> (param);
        switch (CDB_Object::GetBlobType(param.GetType())) {
        case eBlobType_Text:    param_fmt.datatype = CS_TEXT_TYPE;   break;
        case eBlobType_Binary:  param_fmt.datatype = CS_IMAGE_TYPE;  break;
        default:                _TROUBLE;
        }
        if (declare_only) {
            break;
        } else if (par.IsNULL()) {
            ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt, NULL, 0,
                                      indicator));
        } else {
            size_t n = par.Size();
            AutoArray<char> buf(n);
            par.MoveTo(0);
            size_t n2 = par.Read(buf.get(), n);
            _ASSERT(n2 == n);
            ret_code = Check(ct_param(x_GetSybaseCmd(), &param_fmt,
                                      buf.get(), n2, indicator));
        }
        break;
    }
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
        DeleteResultInternal();
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
        DATABASE_DRIVER_ERROR( "Connection has died." + GetDbgInfo(), 121009 );
        // return NULL;
    }

#ifdef FTDS_IN_USE
    unique_ptr<CTL_Connection::CAsyncCancelGuard> GUARD
        (new CTL_Connection::CAsyncCancelGuard(GetConnection()));
#endif

    for (;;) {
        CS_INT res_type;
        CS_RETCODE rc = 0;

#ifdef FTDS_IN_USE
        try {
            rc = Check(ct_results(x_GetSybaseCmd(), &res_type));
        } catch (CDB_TimeoutEx&) {
            if (GetConnection().m_AsyncCancelRequested) {
                rc = CS_CANCELED;
            } else {
                throw;
            }
        }
#else
        rc = Check(ct_results(x_GetSybaseCmd(), &res_type));
#endif

        /* This code causes problems with Test_DropConnection.
        try {
            rc = Check(ct_results(x_GetSybaseCmd(), &res_type));
        } catch (...) {
            // We have to fech out all pending  results ...
            // while (ct_results(x_GetSybaseCmd(), &res_type) == CS_SUCCEED) {
            //     continue;
            // }

            SetHasFailed();
            DeleteResultInternal();
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
            DeleteResultInternal();
            Cancel();
            SetWasSent(false);
            DATABASE_DRIVER_ERROR( "ct_result failed." + GetDbgInfo(), 120013 );
        case CS_CANCELED:
#ifdef FTDS_IN_USE
            if (GetConnection().m_AsyncCancelRequested) {
                GUARD.reset();
                DeleteResultInternal();
                Cancel();
            }
#endif
            SetWasSent(false);
            if (GetTimedOut()) {
                // This branch is for a very specific scenario:
                // - the user sets up a message handler which does not throw
                //   any exceptions
                // - a command execution has timed out
                // - a command is used to retrieve data
                // In this case there is no usual timeout exception due to a
                // message handler. However the fact of the timeout is
                // memorized for the command and it is checked here.
                CDB_ClientEx ex(DIAG_COMPILE_INFO, 0,
                                "Your command has been canceled due to timeout",
                                eDiag_Error, 20003);
                ex.SetRetriable(GetRetriable());
                throw ex;
            } else {
                DATABASE_DRIVER_ERROR( "Your command has been canceled." + GetDbgInfo(), 120011 );
            }
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
#ifdef FTDS_IN_USE
        if (GetConnection().AsyncCancel(*this)) {
            return true;
        }
#endif
        return x_Cancel();
    } else {
        return true;
    }
}

bool
CTL_LRCmd::x_Cancel(void)
{
    if (WasSent()) {
        MarkEndOfReply();

        if (!IsDead()  &&  GetConnection().IsAlive()) {
            size_t was_timeout = GetConnection().PrepareToCancel();
            try {
                CS_RETCODE retcode = Check(ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_ALL));
                CS_INT ignored_results_type;
                // Force full cancellation processing in FreeTDS 0.91.
                ct_results(x_GetSybaseCmd(), &ignored_results_type);
                GetConnection().CancelFinished(was_timeout);

                switch (retcode) {
                case CS_SUCCEED:
                    SetWasSent(false);
                    return true;
                case CS_FAIL:
                    if (GetConnection().IsAlive()) {
                        DATABASE_DRIVER_ERROR("ct_cancel failed."
                                              + GetDbgInfo(),
                                              120008);
                    } else {
                        DATABASE_DRIVER_ERROR("Connection has died."
                                              + GetDbgInfo(),
                                              122010);
                    }
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
        DeleteResultInternal();
        Cancel();
        throw;
    }

    switch (rc) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        SetHasFailed();
        DeleteResultInternal();
        Cancel();
        // IsAlive digs deeper than IsDead, and sockets whose peers have
        // disconnected remain nominally open until their owners try to
        // use them, so checking after trying to send is most effective.
        if (GetConnection().IsAlive()) {
            DATABASE_DRIVER_ERROR("ct_send failed." + GetDbgInfo(), 121005);
        } else {
            DATABASE_DRIVER_ERROR("Connection has died." + GetDbgInfo(),
                                  121008);
        }
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
    EnsureActiveStatus();

    DeleteResultInternal();
    Cancel();

    SetHasFailed(false);

    CTempString dyn_id = x_GetDynamicID();
    if (dyn_id.empty()) {
        CheckSFB(ct_command(x_GetSybaseCmd(), CS_LANG_CMD,
                            const_cast<char*>(GetQuery().data()),
                            GetQuery().size(), CS_END),
                 "ct_command failed", 120001);
    } else if (dyn_id == "!") {
        return false;
    } else {
        CheckSFB(ct_dynamic(x_GetSybaseCmd(), CS_EXECUTE,
                            const_cast<char*>(dyn_id.data()), dyn_id.size(),
                            NULL, 0),
                 "ct_dynamic(CS_EXECUTE) failed", 120004);
    }

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
    if ( !m_DynamicID.empty() ) {
        try {
            CheckSFB(ct_dynamic(x_GetSybaseCmd(), CS_DEALLOC,
                                const_cast<char*>(m_DynamicID.data()),
                                m_DynamicID.size(), NULL, 0),
                     "ct_dynamic(CS_DEALLOC) failed", 120005);
            if (SendInternal()) {
                while (HasMoreResults()) {
                    unique_ptr<CDB_Result> r(Result());
                }
            }
        }
        NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
    }
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
        DeleteResultInternal();
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
        auto status = GetBindParamsImpl().GetParamStatus(i);
        if (status == 0) {
            continue;
        } else if ((status & impl::CDB_Params::fOutput) != 0) {
            param_fmt.status |= CS_RETURN;
        } else {
            param_fmt.status &= ~CS_RETURN;
        }

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


CTempString CTL_LangCmd::x_GetDynamicID(void)
{
    CTempString query = GetQuery();
    static const char kValid[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@$#_";
    
    if ( !m_DynamicID.empty() ) {
        // Perhaps double check that we still want to use it?
        return m_DynamicID;
    }

    if (GetBindParamsImpl().NofParams() == 0  ||  query.find('?') == NPOS) {
        return kEmptyStr;
    } else if (query.find('@') != NPOS) {
        if (count(query.begin(), query.end(), '?')
            < GetBindParamsImpl().NofParams()) {
            ERR_POST_X(7, Info << "Query " << NStr::CEncode(query)
                       << " contains both ? and @.  Treating @ as the"
                       " parameter indicator because there are more"
                       " parameters than question marks.");
        }
        set<CTempString> used, found, not_found;
        SIZE_TYPE pos = query.find('@');
        do {
            SIZE_TYPE pos2 = query.find_first_not_of(kValid, pos);
            if (pos2 == NPOS) {
                used.insert(query.substr(pos));
                break;
            } else {
                used.insert(query.substr(pos, pos2 - pos));
                pos = query.find('@', pos2);
            }
        } while (pos != NPOS);
        for (unsigned int i = 0;  i < GetBindParamsImpl().NofParams();  i++) {
            if (GetBindParamsImpl().GetParamStatus(i) == 0) continue;
            const string& name = GetBindParamsImpl().GetParamName(i);
            if (used.find(name) == used.end()) {
                not_found.insert(name);
            } else {
                found.insert(name);
            }
        }
        if (not_found.empty()) {
            ERR_POST_X(7, Info << "Query " << NStr::CEncode(query)
                       << " contains both ? and @.  Treating @ as the"
                       " parameter indicator because all supplied parameter"
                       " names appear: "
                       << NStr::Join(found, ", "));
            return kEmptyStr;
        } else {
            ERR_POST_X(8, Info << "Query " << NStr::CEncode(query)
                       << " contains both ? and @.  Treating ? as the"
                       " parameter indicator because there are enough"
                       " question marks and some or all supplied"
                       " parameter names are absent: "
                       << NStr::Join(not_found, ", "));
        }
    }
    
    m_DynamicID = NStr::NumericToString(reinterpret_cast<uintptr_t>(this),
                                        0, 16);
    CheckSFB(ct_dynamic(x_GetSybaseCmd(), CS_PREPARE,
                        const_cast<char*>(m_DynamicID.data()),
                        m_DynamicID.size(),
                        const_cast<char*>(query.data()), query.size()),
             "ct_dynamic(CS_PREPARE) failed", 120002);
    if ( !SendInternal() ) {
        return "!";
    }
    while (HasMoreResults()) {
        unique_ptr<CDB_Result> r(Result());
    }
    return m_DynamicID;
}

#ifdef FTDS_IN_USE
} // namespace NCBI_NS_FTDS_CTLIB
#endif

END_NCBI_SCOPE


