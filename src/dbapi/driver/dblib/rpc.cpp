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
 * File Description:  DBLib RPC command
 *
 */
#include <ncbi_pch.hpp>

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>

#include <dbapi/driver/util/numeric_convert.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_Dblib_Cmds

#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), GetConnection(), &GetBindParams())
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_RPCCmd::
//

CDBL_RPCCmd::CDBL_RPCCmd(CDBL_Connection& conn,
                         DBPROCESS* cmd,
                         const string& proc_name) :
    CDBL_Cmd(conn, cmd, proc_name),
    m_Res(0),
    m_Status(0)
{
    SetExecCntxInfo("RPC Command: " + proc_name);
}


CDBParams&
CDBL_RPCCmd::GetBindParams(void)
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


bool CDBL_RPCCmd::Send()
{
    if (WasSent()) {
        Cancel();
    }

    SetHasFailed(false);

    if (Check(dbrpcinit(GetCmd(), (char*) GetQuery().c_str(),
                  NeedToRecompile() ? DBRPCRECOMPILE : 0)) != SUCCEED) {
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "dbrpcinit failed." + GetDbgInfo(), 221001 );
    }

    char param_buff[2048]; // maximal page size
    if (!x_AssignParams(param_buff)) {
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "Cannot assign the params." + GetDbgInfo(), 221003 );
    }
    if (Check(dbrpcsend(GetCmd())) != SUCCEED) {
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "dbrpcsend failed." + GetDbgInfo(), 221005 );
    }

    SetWasSent();
    m_Status = 0;
    return true;
}


bool CDBL_RPCCmd::Cancel()
{
    if (WasSent()) {
        if (GetResultSet()) {
            ClearResultSet();
        }
        SetWasSent(false);
        return Check(dbcancel(GetCmd())) == SUCCEED;
    }
    // m_Query.erase();
    return true;
}


CDB_Result* CDBL_RPCCmd::Result()
{
    if ( GetResultSet() ) {
        if(m_RowCount < 0) {
            m_RowCount = DBCOUNT(GetCmd());
        }

        ClearResultSet();
    }

    if (!WasSent()) {
        DATABASE_DRIVER_ERROR( "You have to send a command first." + GetDbgInfo(), 221010 );
    }

    if (m_Status == 0) {
        m_Status = 0x1;
        if (Check(dbsqlok(GetCmd())) != SUCCEED) {
            SetWasSent(false);
            SetHasFailed();
            DATABASE_DRIVER_ERROR( "dbsqlok failed." + GetDbgInfo(), 221011 );
        }
    }

    if ((m_Status & 0x10) != 0) { // we do have a compute result
        SetResultSet( new CDBL_ComputeResult(GetConnection(), GetCmd(), &m_Status) );
        m_RowCount= 1;
        return Create_Result(*GetResultSet());
    }

    while ((m_Status & 0x1) != 0) {
        RETCODE rc = Check(dbresults(GetCmd()));

        switch (rc) {
        case SUCCEED:
            if (DBCMDROW(GetCmd()) == SUCCEED) { // we may get rows in this result
                if (Check(dbnumcols(GetCmd())) == 1) {
                    int ct = Check(dbcoltype(GetCmd(), 1));
                    if ((ct == SYBTEXT) || (ct == SYBIMAGE)) {
                        SetResultSet( new CDBL_BlobResult(GetConnection(), GetCmd()) );
                    }
                }
                if (!GetResultSet())
                    SetResultSet( new CDBL_RowResult(GetConnection(), GetCmd(), &m_Status) );
                m_RowCount= -1;
                return Create_Result(*GetResultSet());
            } else {
                m_RowCount = DBCOUNT(GetCmd());
                continue;
            }
        case NO_MORE_RESULTS:
            m_Status = 2;
            break;
        default:
            SetHasFailed();
            DATABASE_DRIVER_WARNING( "error encountered in command execution", 221016 );
        }
        break;
    }

    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (m_Status == 2) {
        m_Status = 4;
        int n = Check(dbnumrets(GetCmd()));
        if (n > 0) {
            SetResultSet( new CDBL_ParamResult(GetConnection(), GetCmd(), n) );
            m_RowCount= 1;
            return Create_Result(*GetResultSet());
        }
    }

    if (m_Status == 4) {
        m_Status = 6;
        if (Check(dbhasretstat(GetCmd()))) {
            SetResultSet( new CDBL_StatusResult(GetConnection(), GetCmd()) );
            m_RowCount= 1;
            return Create_Result(*GetResultSet());
        }
    }

    SetWasSent(false);
    return 0;
}


bool CDBL_RPCCmd::HasMoreResults() const
{
    return WasSent();
}


int CDBL_RPCCmd::RowCount() const
{
    return (m_RowCount < 0)? DBCOUNT(GetCmd()) : m_RowCount;
}


CDBL_RPCCmd::~CDBL_RPCCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL_X( 4, NCBI_CURRENT_FUNCTION )
}


bool CDBL_RPCCmd::x_AssignParams(char* param_buff)
{
    RETCODE r;

    for (unsigned int i = 0; i < GetBindParamsImpl().NofParams(); i++) {
        if(GetBindParamsImpl().GetParamStatus(i) == 0) continue;
        CDB_Object& param = *GetBindParamsImpl().GetParam(i);
        BYTE status =
            (GetBindParamsImpl().GetParamStatus(i) & impl::CDB_Params::fOutput)
            ? DBRPCRETURN : 0;
        bool is_null = param.IsNULL();

        switch (param.GetType()) {
        case eDB_Int: {
            CDB_Int& val = dynamic_cast<CDB_Int&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBINT4, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_SmallInt: {
            CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBINT2, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_TinyInt: {
            CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBINT1, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_Bit: {
            CDB_Bit& val = dynamic_cast<CDB_Bit&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBBIT, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_BigInt: {
            CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
            DBNUMERIC* v = (DBNUMERIC*) param_buff;
            Int8 v8 = val.Value();
            if (longlong_to_numeric(v8, 18, DBNUMERIC_val(v)) == 0)
                return false;
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBNUMERIC, -1,
                           is_null ? 0 : -1, (BYTE*) v));
            param_buff = (char*) (v + 1);
            break;
        }
        case eDB_Char:
        case eDB_LongChar:
        case eDB_VarChar: {
            CDB_String& val = dynamic_cast<CDB_String&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBCHAR, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Data()));
        }
        break;
        case eDB_Binary: {
            CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBBINARY, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value()));
            break;
        }
        case eDB_VarBinary: {
            CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBBINARY, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value()));
        }
        break;
        case eDB_Float: {
            CDB_Float& val = dynamic_cast<CDB_Float&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBREAL, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_Double: {
            CDB_Double& val = dynamic_cast<CDB_Double&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBFLT8, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_SmallDateTime: {
            CDB_SmallDateTime& val = dynamic_cast<CDB_SmallDateTime&> (param);

            DBDATETIME4* dt        = (DBDATETIME4*) param_buff;
            if (param.IsNULL()) {
                DBDATETIME4_days(dt) = 0;
                DBDATETIME4_mins(dt) = 0;
            } else {
                DBDATETIME4_days(dt) = val.GetDays();
                DBDATETIME4_mins(dt) = val.GetMinutes();
            }

            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBDATETIME4, -1,
                           is_null ? 0 : -1, (BYTE*) dt));
            param_buff = (char*) (dt + 1);
            break;
        }
        case eDB_DateTime: {
            CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);

            DBDATETIME* dt = (DBDATETIME*) param_buff;
            if (param.IsNULL()) {
                dt->dtdays = 0;
                dt->dttime = 0;
            } else {
                dt->dtdays = val.GetDays();
                dt->dttime = val.Get300Secs();
            }

            r = Check(dbrpcparam(GetCmd(), (char*) GetBindParamsImpl().GetParamName(i).c_str(),
                           status, SYBDATETIME, -1,
                           is_null ? 0 : -1, (BYTE*) dt));
            param_buff = (char*) (dt + 1);
            break;
        }
        default:
            return false;
        }
        if (r != SUCCEED)
            return false;
    }
    return true;
}


END_NCBI_SCOPE


