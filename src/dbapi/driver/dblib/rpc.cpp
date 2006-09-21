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


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_RPCCmd::
//

CDBL_RPCCmd::CDBL_RPCCmd(CDBL_Connection* conn, DBPROCESS* cmd,
                         const string& proc_name, unsigned int nof_params) :
    CDBL_Cmd( conn, cmd ),
    impl::CBaseCmd(proc_name, nof_params),
    m_Res(0),
    m_Status(0)
{
    return;
}


bool CDBL_RPCCmd::Send()
{
    if (m_WasSent) {
        Cancel();
    } else {
#if 1 && defined(FTDS_IN_USE)
        if ( GetResultSet() ) {
            while (GetResultSet()->Fetch())
                continue;
        }
#endif
    }

    m_HasFailed = false;

    if (Check(dbrpcinit(GetCmd(), (char*) m_Query.c_str(),
                  NeedToRecompile() ? DBRPCRECOMPILE : 0)) != SUCCEED) {
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "dbrpcinit failed", 221001 );
    }

    char param_buff[2048]; // maximal page size
    if (!x_AssignParams(param_buff)) {
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "Cannot assign the params", 221003 );
    }
    if (Check(dbrpcsend(GetCmd())) != SUCCEED) {
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "dbrpcsend failed", 221005 );
    }

    m_WasSent = true;
    m_Status = 0;
    return true;
}


bool CDBL_RPCCmd::WasSent() const
{
    return m_WasSent;
}


bool CDBL_RPCCmd::Cancel()
{
    if (m_WasSent) {
        if (GetResultSet()) {
#if 1 && defined(FTDS_IN_USE)
            while (GetResultSet()->Fetch())
                continue;
#endif

            ClearResultSet();
        }
        m_WasSent = false;
        return Check(dbcancel(GetCmd())) == SUCCEED;
    }
    // m_Query.erase();
    return true;
}


bool CDBL_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CDBL_RPCCmd::Result()
{
    if ( GetResultSet() ) {
        if(m_RowCount < 0) {
            m_RowCount = DBCOUNT(GetCmd());
        }

#if 1 && defined(FTDS_IN_USE)
        while (GetResultSet()->Fetch())
            continue;
#endif

        ClearResultSet();
    }

    if (!m_WasSent) {
        DATABASE_DRIVER_ERROR( "you have to send a command first", 221010 );
    }

    if (m_Status == 0) {
        m_Status = 0x1;
        if (Check(dbsqlok(GetCmd())) != SUCCEED) {
            m_WasSent = false;
            m_HasFailed = true;
            DATABASE_DRIVER_ERROR( "dbsqlok failed", 221011 );
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
// This optimization is currently unavailable for MS dblib...
#ifndef MS_DBLIB_IN_USE /*Text,Image*/
                if (Check(dbnumcols(GetCmd())) == 1) {
                    int ct = Check(dbcoltype(GetCmd(), 1));
                    if ((ct == SYBTEXT) || (ct == SYBIMAGE)) {
                        SetResultSet( new CDBL_BlobResult(GetConnection(), GetCmd()) );
                    }
                }
#endif
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
            m_HasFailed = true;
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

    m_WasSent = false;
    return 0;
}


bool CDBL_RPCCmd::HasMoreResults() const
{
    return m_WasSent;
}

void CDBL_RPCCmd::DumpResults()
{
    while(m_WasSent) {
        auto_ptr<CDB_Result> dbres( Result() );
        if( dbres.get() ) {
            if(GetConnection().GetResultProcessor()) {
                GetConnection().GetResultProcessor()->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch())
                    continue;
            }
        }
    }
}

bool CDBL_RPCCmd::HasFailed() const
{
    return m_HasFailed;
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
    NCBI_CATCH_ALL( kEmptyStr )
}


bool CDBL_RPCCmd::x_AssignParams(char* param_buff)
{
    RETCODE r;

    for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
        if(m_Params.GetParamStatus(i) == 0) continue;
        CDB_Object& param = *m_Params.GetParam(i);
        BYTE status =
            (m_Params.GetParamStatus(i) & CDB_Params::fOutput)
            ? DBRPCRETURN : 0;
        bool is_null = param.IsNULL();

        switch (param.GetType()) {
        case eDB_Int: {
            CDB_Int& val = dynamic_cast<CDB_Int&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBINT4, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_SmallInt: {
            CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBINT2, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_TinyInt: {
            CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBINT1, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_Bit: {
            CDB_Bit& val = dynamic_cast<CDB_Bit&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
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
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBNUMERIC, -1,
                           is_null ? 0 : -1, (BYTE*) v));
            param_buff = (char*) (v + 1);
            break;
        }
        case eDB_Char: {
            CDB_Char& val = dynamic_cast<CDB_Char&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBCHAR, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value()));
            break;
        }
        case eDB_LongChar: {
            CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBCHAR, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value()));
            break;
        }
        case eDB_VarChar: {
            CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBCHAR, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value()));
        }
        break;
        case eDB_Binary: {
            CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBBINARY, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value()));
            break;
        }
        case eDB_VarBinary: {
            CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBBINARY, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value()));
        }
        break;
        case eDB_Float: {
            CDB_Float& val = dynamic_cast<CDB_Float&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBREAL, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_Double: {
            CDB_Double& val = dynamic_cast<CDB_Double&> (param);
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBFLT8, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal()));
            break;
        }
        case eDB_SmallDateTime: {
            CDB_SmallDateTime& val = dynamic_cast<CDB_SmallDateTime&> (param);
            DBDATETIME4* dt        = (DBDATETIME4*) param_buff;
            DBDATETIME4_days(dt)   = val.GetDays();
            DBDATETIME4_mins(dt)   = val.GetMinutes();
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBDATETIME4, -1,
                           is_null ? 0 : -1, (BYTE*) dt));
            param_buff = (char*) (dt + 1);
            break;
        }
        case eDB_DateTime: {
            CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
            DBDATETIME* dt = (DBDATETIME*) param_buff;
            dt->dtdays     = val.GetDays();
            dt->dttime     = val.Get300Secs();
            r = Check(dbrpcparam(GetCmd(), (char*) m_Params.GetParamName(i).c_str(),
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


#ifdef FTDS_IN_USE
bool CDBL_RPCCmd::x_AddParamValue(string& cmd, const CDB_Object& param)
{
    return false;
}

bool CDBL_RPCCmd::x_AssignOutputParams(void)
{
    return false;
}

bool CDBL_RPCCmd::x_AssignParams(void)
{
    return false;
}

#endif

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.31  2006/09/21 16:23:02  ssikorsk
 * CDBL_Connection::Check --> CheckDead.
 *
 * Revision 1.30  2006/08/22 18:21:04  ssikorsk
 * Added handling of CDB_LongChar to CDBL_RPCCmd::x_AssignParams.
 *
 * Revision 1.29  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.28  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.27  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.26  2006/06/05 21:09:21  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.25  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.24  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.23  2006/05/04 20:12:17  ssikorsk
 * Implemented classs CDBL_Cmd, CDBL_Result and CDBLExceptions;
 * Surrounded each native dblib call with Check;
 *
 * Revision 1.22  2005/12/06 19:31:42  ssikorsk
 * Revamp code to use GetResultSet/SetResultSet/ClearResultSet
 * methods instead of raw data access.
 *
 * Revision 1.21  2005/10/31 12:20:42  ssikorsk
 * Do not use separate include files for msdblib.
 *
 * Revision 1.20  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.19  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.18  2005/08/09 14:54:10  ssikorsk
 * Removed redudant comments. Stylistic changes.
 *
 * Revision 1.17  2005/07/20 12:33:05  ssikorsk
 * Merged ftds/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.16  2005/07/14 19:12:36  ssikorsk
 * Eliminated usage of ProcessResult
 *
 * Revision 1.15  2005/07/07 19:12:55  ssikorsk
 * Improved to support a ftds driver
 *
 * Revision 1.14  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.13  2004/05/18 18:30:37  gorelenk
 * PCH <ncbi_pch.hpp> moved to correct place .
 *
 * Revision 1.12  2004/05/17 21:12:41  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.11  2003/06/05 16:01:13  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.10  2003/05/16 20:25:20  soussov
 * adds code to skip parameter if it was not set
 *
 * Revision 1.9  2002/07/22 20:02:59  soussov
 * fixes the RowCount calculations
 *
 * Revision 1.8  2002/07/02 16:05:50  soussov
 * splitting Sybase dblib and MS dblib
 *
 * Revision 1.7  2002/02/22 22:12:33  soussov
 * fixes bug with return params result
 *
 * Revision 1.6  2002/01/08 18:10:18  sapojnik
 * Syabse to MSSQL name translations moved to interface_p.hpp
 *
 * Revision 1.5  2002/01/03 17:01:56  sapojnik
 * fixing CR/LF mixup
 *
 * Revision 1.4  2002/01/03 15:46:23  sapojnik
 * ported to MS SQL (about 12 'ifdef NCBI_OS_MSWIN' in 6 files)
 *
 * Revision 1.3  2001/10/24 16:39:32  lavr
 * Explicit casts (where necessary) to eliminate 64->32 bit compiler warnings
 *
 * Revision 1.2  2001/10/22 16:28:02  lavr
 * Default argument values removed
 * (mistakenly left while moving code from header files)
 *
 * Revision 1.1  2001/10/22 15:19:56  lavr
 * This is a major revamp (by Anton Lavrentiev, with help from Vladimir
 * Soussov and Denis Vakatov) of the DBAPI "driver" libs originally
 * written by Vladimir Soussov. The revamp follows the one of CTLib
 * driver, and it involved massive code shuffling and grooming, numerous
 * local API redesigns, adding comments and incorporating DBAPI to
 * the C++ Toolkit.
 *
 * ===========================================================================
 */
