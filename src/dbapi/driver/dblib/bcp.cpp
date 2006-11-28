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
 * File Description:  DBLib bcp-in command
 *
 */
#include <ncbi_pch.hpp>

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>

#include <dbapi/driver/util/numeric_convert.hpp>
#include <string.h>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_BCPInCmd::
//

CDBL_BCPInCmd::CDBL_BCPInCmd(CDBL_Connection* conn,
                             DBPROCESS*       cmd,
                             const string&    table_name,
                             unsigned int     nof_columns) :
    CDBL_Cmd( conn, cmd ),
    impl::CBaseCmd(table_name, nof_columns),
    m_HasTextImage(false),
    m_WasBound(false)
{
    if (Check(bcp_init(cmd, (char*) table_name.c_str(), 0, 0, DB_IN)) != SUCCEED) {
        DATABASE_DRIVER_ERROR( "bcp_init failed", 223001 );
    }

    ++m_RowCount;
}

bool CDBL_BCPInCmd::Bind(unsigned int column_num, CDB_Object* param_ptr)
{
    m_WasBound = false;
    return GetParams().BindParam(column_num,  kEmptyStr, param_ptr);
}


bool CDBL_BCPInCmd::x_AssignParams(void* pb)
{
    RETCODE r;

    if (!m_WasBound) {
        for (unsigned int i = 0; i < GetParams().NofParams(); i++) {
            if (GetParams().GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *GetParams().GetParam(i);

            switch ( param.GetType() ) {
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                // DBINT v = (DBINT) val.Value();
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT4, i + 1));
            }
            break;
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                // DBSMALLINT v = (DBSMALLINT) val.Value();
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT2, i + 1));
            }
            break;
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                // DBTINYINT v = (DBTINYINT) val.Value();
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT1, i + 1));
            }
            break;
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
#ifndef FTDS_IN_USE
                DBNUMERIC* v = reinterpret_cast<DBNUMERIC*> (pb);
                Int8 v8 = val.Value();
                v->precision= 18;
                v->scale= 0;
                if (longlong_to_numeric(v8, 18, DBNUMERIC_val(v)) == 0)
                    return false;
                r = Check(bcp_bind(GetCmd(), (BYTE*) v, 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBNUMERIC, i + 1));
                pb = (void*) (v + 1);
#else
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT8, i + 1));
#endif
            }
            break;
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : -1,
                             (BYTE*) "", 1, SYBCHAR, i + 1));
            }
            break;
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : -1,
                             (BYTE*) "", 1, SYBVARCHAR, i + 1));
            }
            break;
            case eDB_LongChar: {
                CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : -1,
                             (BYTE*) "", 1, SYBCHAR, i + 1));
            }
            break;
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : (DBINT) val.Size(),
                             0, 0, SYBBINARY, i + 1));
            }
            break;
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : (DBINT) val.Size(),
                             0, 0, SYBBINARY, i + 1));
            }
            break;
            case eDB_LongBinary: {
                CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : val.DataSize(),
                             0, 0, SYBBINARY, i + 1));
            }
            break;
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                // DBREAL v = (DBREAL) val.Value();
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBREAL, i + 1));
            }
            break;
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                // DBFLT8 v = (DBFLT8) val.Value();
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBFLT8, i + 1));
            }
            break;
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                DBDATETIME4* dt = (DBDATETIME4*) pb;
                DBDATETIME4_days(dt)        = val.GetDays();
                DBDATETIME4_mins(dt)     = val.GetMinutes();
                r = Check(bcp_bind(GetCmd(), (BYTE*) dt, 0, val.IsNULL() ? 0 : -1,
                             0, 0, SYBDATETIME4,  i + 1));
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_DateTime: {
                CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
                DBDATETIME* dt = (DBDATETIME*) pb;
                dt->dtdays     = val.GetDays();
                dt->dttime     = val.Get300Secs();
                r = Check(bcp_bind(GetCmd(), (BYTE*) dt, 0, val.IsNULL() ? 0 : -1,
                             0, 0, SYBDATETIME, i + 1));
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_Text: {
                CDB_Text& val = dynamic_cast<CDB_Text&> (param);
                r = Check(bcp_bind(GetCmd(), 0, 0,
                             val.IsNULL() ? 0 : (DBINT) val.Size(),
                             0, 0, SYBTEXT, i + 1));
                m_HasTextImage = true;
            }
            break;
            case eDB_Image: {
                CDB_Image& val = dynamic_cast<CDB_Image&> (param);
                r = Check(bcp_bind(GetCmd(), 0, 0,
                             val.IsNULL() ? 0 : (DBINT) val.Size(),
                             0, 0, SYBIMAGE, i + 1));
                m_HasTextImage = true;
            }
            break;
            default:
                return false;
            }
            if (r != CS_SUCCEED)
                return false;
        }
        m_WasBound = true;
    } else {
        for (unsigned int i = 0; i < GetParams().NofParams(); i++) {
            if (GetParams().GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *GetParams().GetParam(i);

            switch ( param.GetType() ) {
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                // DBINT v = (DBINT) val.Value();
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.BindVal(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(),  val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                // DBSMALLINT v = (DBSMALLINT) val.Value();
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.BindVal(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(),  val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                // DBTINYINT v = (DBTINYINT) val.Value();
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.BindVal(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
#ifndef FTDS_IN_USE
                DBNUMERIC* v = (DBNUMERIC*) pb;
                v->precision= 18;
                v->scale= 0;
                Int8 v8 = val.Value();
                if (longlong_to_numeric(v8, 18, DBNUMERIC_val(v)) == 0)
                    return false;
                r = Check(bcp_colptr(GetCmd(), (BYTE*) v, i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(),  val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (v + 1);
#else
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.BindVal(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
#endif
            }
            break;
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.Value(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.Value(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_LongChar: {
                CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.Value(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.Value(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(),
                               val.IsNULL() ? 0 : (DBINT) val.Size(), i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.Value(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : (DBINT)val.Size(), i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_LongBinary: {
                CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.Value(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : val.DataSize(), i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                //DBREAL v = (DBREAL) val.Value();
                r = Check(bcp_colptr(GetCmd(), (BYTE*) val.BindVal(), i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(),  val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                //DBFLT8 v = (DBFLT8) val.Value();
                r = Check(bcp_bind(GetCmd(), (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBFLT8, i + 1));
            }
            break;
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                DBDATETIME4* dt = (DBDATETIME4*) pb;
                DBDATETIME4_days(dt)        = val.GetDays();
                DBDATETIME4_mins(dt)     = val.GetMinutes();
                r = Check(bcp_colptr(GetCmd(), (BYTE*) dt, i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_DateTime: {
                CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
                DBDATETIME* dt = (DBDATETIME*) pb;
                dt->dtdays     = val.GetDays();
                dt->dttime     = val.Get300Secs();
                r = Check(bcp_colptr(GetCmd(), (BYTE*) dt, i + 1))
                    == SUCCEED &&
                    Check(bcp_collen(GetCmd(), val.IsNULL() ? 0 : -1, i + 1))
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_Text: {
                CDB_Text& val = dynamic_cast<CDB_Text&> (param);
                r = Check(bcp_collen(GetCmd(), (DBINT) val.Size(), i + 1));
            }
            break;
            case eDB_Image: {
                CDB_Image& val = dynamic_cast<CDB_Image&> (param);
                r = Check(bcp_collen(GetCmd(), (DBINT) val.Size(), i + 1));
            }
            break;
            default:
                return false;
            }
            if (r != CS_SUCCEED)
                return false;
        }
    }
    return true;
}


bool CDBL_BCPInCmd::Send(void)
{
    char param_buff[2048]; // maximal row size, assured of buffer overruns

    m_HasFailed = !x_AssignParams(param_buff);
    CHECK_DRIVER_ERROR( m_HasFailed, "cannot assign params", 223004 );

    m_HasFailed = (Check(bcp_sendrow(GetCmd())) != SUCCEED);
    CHECK_DRIVER_ERROR( m_HasFailed, "bcp_sendrow failed", 223005 );

    m_WasSent = true;

    if (m_HasTextImage) { // send text/image data
        char buff[1800]; // text/image page size

        for (unsigned int i = 0; i < GetParams().NofParams(); i++) {
            if (GetParams().GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *GetParams().GetParam(i);

            if (param.GetType() != eDB_Text &&
                param.GetType() != eDB_Image)
                continue;

            CDB_Stream& val = dynamic_cast<CDB_Stream&> (param);
            size_t s = val.Size();
            do {
                size_t l = val.Read(buff, sizeof(buff));
                if (l > s)
                    l = s;
                if (Check(bcp_moretext(GetCmd(), (DBINT) l, (BYTE*) buff)) != SUCCEED) {
                    m_HasFailed = true;
                    string error;

                    if (param.GetType() == eDB_Text) {
                        error = "bcp_moretext for text failed";
                    } else {
                        error = "bcp_moretext for image failed";
                    }
                    DATABASE_DRIVER_ERROR( error, 223006 );
                }
                if (!l)
                    break;
                s -= l;
            } while (s);
        }
    }

    ++m_RowCount;

    return true;
}


bool CDBL_BCPInCmd::WasSent(void) const
{
    return m_WasSent;
}


bool CDBL_BCPInCmd::Cancel()
{
    if (m_WasSent) {
        DBINT outrow = Check(bcp_done(GetCmd()));
        m_WasSent = false;
        return outrow == 0;
    }

    return true;
}


bool CDBL_BCPInCmd::WasCanceled(void) const
{
    return !m_WasSent;
}


bool CDBL_BCPInCmd::CommitBCPTrans(void)
{
    if(m_WasSent) {
        DBINT outrow = Check(bcp_batch(GetCmd()));
        if(outrow < 0) {
            m_HasFailed= true;
            DATABASE_DRIVER_ERROR( "bcp_batch failed", 223020 );
        }
        return outrow > 0;
    }
    return false;
}


bool CDBL_BCPInCmd::EndBCP(void)
{
    if(m_WasSent) {
        DBINT outrow = Check(bcp_done(GetCmd()));
        if(outrow < 0) {
            m_HasFailed = true;
            DATABASE_DRIVER_ERROR( "bcp_done failed", 223020 );
        }
        m_WasSent= false;
        return outrow > 0;
    }
    return false;
}


bool CDBL_BCPInCmd::HasFailed(void) const
{
    return m_HasFailed;
}


int CDBL_BCPInCmd::RowCount(void) const
{
    return m_RowCount;
}


CDBL_BCPInCmd::~CDBL_BCPInCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.29  2006/11/28 20:08:09  ssikorsk
 * Replaced NCBI_CATCH_ALL(kEmptyStr) with NCBI_CATCH_ALL(NCBI_CURRENT_FUNCTION)
 *
 * Revision 1.28  2006/11/20 18:15:58  ssikorsk
 * Revamp code to use GetQuery() and GetParams() methods.
 *
 * Revision 1.27  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.26  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.25  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.24  2006/06/05 21:09:21  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.23  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.22  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.21  2006/05/04 20:12:17  ssikorsk
 * Implemented classs CDBL_Cmd, CDBL_Result and CDBLExceptions;
 * Surrounded each native dblib call with Check;
 *
 * Revision 1.20  2006/02/22 15:15:50  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.19  2005/10/31 12:18:55  ssikorsk
 * Do not use separate include files for msdblib.
 *
 * Revision 1.18  2005/09/26 14:53:52  ssikorsk
 * CompleteBatch should always throw an exception if bcp_batch fails.
 * This was not done in case of the DBLIB driver.
 *
 * Revision 1.17  2005/09/26 14:40:49  ssikorsk
 * CompleteBCP will always throw an exception if bcp_done failed.
 * This was not done in case of the DBLIB driver.
 *
 * Revision 1.16  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.15  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.14  2005/08/22 15:16:48  ssikorsk
 * Treat eDB_BigInt as SYBINT8 in case of FreeTDS and as SYBNUMERIC in case of Sybase dblib
 *
 * Revision 1.13  2005/08/15 18:46:03  ssikorsk
 * Rebind variables after each call of Bind.
 *
 * Revision 1.12  2005/07/07 19:12:54  ssikorsk
 * Improved to support a ftds driver
 *
 * Revision 1.11  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.10  2004/05/18 18:30:36  gorelenk
 * PCH <ncbi_pch.hpp> moved to correct place .
 *
 * Revision 1.9  2004/05/17 21:12:41  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2002/07/02 16:05:49  soussov
 * splitting Sybase dblib and MS dblib
 *
 * Revision 1.7  2002/03/04 19:07:21  soussov
 * fixed bug in m_WasSent flag setting
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
 * Revision 1.3  2001/10/24 16:38:53  lavr
 * Explicit casts (where necessary) to eliminate 64->32 bit compiler warnings
 *
 * Revision 1.2  2001/10/22 18:38:49  soussov
 * sending NULL instead of emty string fixed
 *
 * Revision 1.1  2001/10/22 15:19:55  lavr
 * This is a major revamp (by Anton Lavrentiev, with help from Vladimir
 * Soussov and Denis Vakatov) of the DBAPI "driver" libs originally
 * written by Vladimir Soussov. The revamp follows the one of CTLib
 * driver, and it involved massive code shuffling and grooming, numerous
 * local API redesigns, adding comments and incorporating DBAPI to
 * the C++ Toolkit.
 *
 * ===========================================================================
 */
