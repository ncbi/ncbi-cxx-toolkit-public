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
#include <dbapi/error_codes.hpp>
#include <string.h>


#define NCBI_USE_ERRCODE_X   Dbapi_Dblib_Cmds

#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), GetConnection(), &GetBindParams())
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_BCPInCmd::
//

CDBL_BCPInCmd::CDBL_BCPInCmd(CDBL_Connection& conn,
                             DBPROCESS*       cmd,
                             const string&    table_name) :
    CDBL_Cmd(conn, cmd, table_name),
    m_HasTextImage(false),
    m_WasBound(false)
{
    SetExecCntxInfo("BCP table name: " + table_name);

    if (Check(bcp_init(cmd, (char*) table_name.c_str(), 0, 0, DB_IN)) != SUCCEED) {
        DATABASE_DRIVER_ERROR( "bcp_init failed." + GetDbgInfo(), 223001 );
    }

    ++m_RowCount;
}

bool CDBL_BCPInCmd::Bind(unsigned int column_num, CDB_Object* param_ptr)
{
    m_WasBound = false;
    return GetBindParamsImpl().BindParam(column_num,  kEmptyStr, param_ptr);
}


bool CDBL_BCPInCmd::x_AssignParams(void* pb)
{
    RETCODE r;

    if (!m_WasBound) {
        for (unsigned int i = 0; i < GetBindParamsImpl().NofParams(); i++) {
            if (GetBindParamsImpl().GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *GetBindParamsImpl().GetParam(i);

            switch ( param.GetType() ) {
            case eDB_Bit:
                DATABASE_DRIVER_ERROR("Bit data type is not supported", 10005);
                break;
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
                DBNUMERIC* v = reinterpret_cast<DBNUMERIC*> (pb);
                Int8 v8 = val.Value();
                v->precision= 18;
                v->scale= 0;
                if (longlong_to_numeric(v8, 18, DBNUMERIC_val(v)) == 0)
                    return false;
                r = Check(bcp_bind(GetCmd(), (BYTE*) v, 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBNUMERIC, i + 1));
                pb = (void*) (v + 1);
            }
            break;
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);

                if (val.IsNULL()) {
                    r = Check(bcp_bind(GetCmd(), (BYTE*) "", 0, 0,
                                       NULL, 0, SYBCHAR, i + 1));
                } else {
                    // r = Check(bcp_bind(GetCmd(), (BYTE*) val.Data(), 0,
                    //                   val.Size(), NULL, 0, SYBCHAR, i + 1));
                    r = Check(bcp_bind(GetCmd(), (BYTE*) val.AsCString(), 0,
                                -1,
                                (BYTE*) "", 1, SYBCHAR, i + 1));
                }
            }
            break;
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);

                if (val.IsNULL()) {
                    r = Check(bcp_bind(GetCmd(), (BYTE*) "", 0, 0,
                                       NULL, 0, SYBVARCHAR, i + 1));
                } else {
                    r = Check(bcp_bind(GetCmd(), (BYTE*) val.AsCString(), 0,
                                -1,
                                (BYTE*) "", 1, SYBVARCHAR, i + 1));
                }
            }
            break;
            case eDB_LongChar: {
                CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);

                if (val.IsNULL()) {
                    r = Check(bcp_bind(GetCmd(), (BYTE*) "", 0, 0,
                                       NULL, 0, SYBCHAR, i + 1));
                } else {
                    r = Check(bcp_bind(GetCmd(), (BYTE*) val.AsCString(), 0,
                                -1,
                                (BYTE*) "", 1, SYBCHAR, i + 1));
                }
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
                if (param.IsNULL()) {
                    DBDATETIME4_days(dt) = 0;
                    DBDATETIME4_mins(dt) = 0;
                } else {
                    DBDATETIME4_days(dt) = val.GetDays();
                    DBDATETIME4_mins(dt) = val.GetMinutes();
                }

                r = Check(bcp_bind(GetCmd(), (BYTE*) dt, 0, val.IsNULL() ? 0 : -1,
                             0, 0, SYBDATETIME4,  i + 1));
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_DateTime: {
                CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);

                DBDATETIME* dt = (DBDATETIME*) pb;
                if (param.IsNULL()) {
                    dt->dtdays = 0;
                    dt->dttime = 0;
                } else {
                    dt->dtdays = val.GetDays();
                    dt->dttime = val.Get300Secs();
                }

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

        GetBindParamsImpl().LockBinding();
        m_WasBound = true;
    } else {
        for (unsigned int i = 0; i < GetBindParamsImpl().NofParams(); i++) {
            if (GetBindParamsImpl().GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *GetBindParamsImpl().GetParam(i);

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
            }
            break;
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);

                if (val.IsNULL()) {
                    r = Check(bcp_colptr(GetCmd(), (BYTE*) "", i + 1))
                        == SUCCEED &&
                        Check(bcp_collen(GetCmd(), 0, i + 1))
                        == SUCCEED ? SUCCEED : FAIL;
                } else {
                    r = Check(bcp_colptr(GetCmd(), (BYTE*) val.AsCString(),
                                         i + 1))
                        == SUCCEED &&
                        Check(bcp_collen(GetCmd(), -1 /* val.Size() */, i + 1))
                        == SUCCEED ? SUCCEED : FAIL;
                }
            }
            break;
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);

                if (val.IsNULL()) {
                    r = Check(bcp_colptr(GetCmd(), (BYTE*) "", i + 1))
                        == SUCCEED &&
                        Check(bcp_collen(GetCmd(), 0, i + 1))
                        == SUCCEED ? SUCCEED : FAIL;
                } else {
                    r = Check(bcp_colptr(GetCmd(), (BYTE*) val.AsCString(),
                                         i + 1))
                        == SUCCEED &&
                        Check(bcp_collen(GetCmd(), -1, i + 1))
                        == SUCCEED ? SUCCEED : FAIL;
                }
            }
            break;
            case eDB_LongChar: {
                CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);

                if (val.IsNULL()) {
                    r = Check(bcp_colptr(GetCmd(), (BYTE*) "", i + 1))
                        == SUCCEED &&
                        Check(bcp_collen(GetCmd(), 0, i + 1))
                        == SUCCEED ? SUCCEED : FAIL;
                } else {
                    r = Check(bcp_colptr(GetCmd(), (BYTE*) val.AsCString(),
                                         i + 1))
                        == SUCCEED &&
                        Check(bcp_collen(GetCmd(), -1, i + 1))
                        == SUCCEED ? SUCCEED : FAIL;
                }
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
                if (param.IsNULL()) {
                    DBDATETIME4_days(dt) = 0;
                    DBDATETIME4_mins(dt) = 0;
                } else {
                    DBDATETIME4_days(dt) = val.GetDays();
                    DBDATETIME4_mins(dt) = val.GetMinutes();
                }

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
                if (param.IsNULL()) {
                    dt->dtdays = 0;
                    dt->dttime = 0;
                } else {
                    dt->dtdays = val.GetDays();
                    dt->dttime = val.Get300Secs();
                }

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

    if (!x_AssignParams(param_buff)) {
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "Cannot assign params." + GetDbgInfo(), 223004 );
    }

    SetWasSent();

    if (Check(bcp_sendrow(GetCmd())) != SUCCEED) {
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "bcp_sendrow failed." + GetDbgInfo(), 223005 );
    }

    if (m_HasTextImage) { // send text/image data
        char buff[1800]; // text/image page size

        for (unsigned int i = 0; i < GetBindParamsImpl().NofParams(); i++) {
            if (GetBindParamsImpl().GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *GetBindParamsImpl().GetParam(i);

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
                    SetHasFailed();
                    string error;

                    if (param.GetType() == eDB_Text) {
                        error = "bcp_moretext for text failed.";
                    } else {
                        error = "bcp_moretext for image failed.";
                    }
                    DATABASE_DRIVER_ERROR( error + GetDbgInfo(), 223006 );
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


bool CDBL_BCPInCmd::Cancel()
{
    if (WasSent()) {
        DBINT outrow = Check(bcp_done(GetCmd()));
        SetWasSent(false);
        return outrow == 0;
    }

    return true;
}


bool CDBL_BCPInCmd::CommitBCPTrans(void)
{
    if(WasSent()) {
        DBINT outrow = Check(bcp_batch(GetCmd()));
        if(outrow < 0) {
            SetHasFailed();
            DATABASE_DRIVER_ERROR( "bcp_batch failed." + GetDbgInfo(), 223020 );
        }
        return outrow > 0;
    }
    return false;
}


bool CDBL_BCPInCmd::EndBCP(void)
{
    if(WasSent()) {
        DBINT outrow = Check(bcp_done(GetCmd()));
        if(outrow < 0) {
            SetHasFailed();
            DATABASE_DRIVER_ERROR( "bcp_done failed." + GetDbgInfo(), 223020 );
        }
        SetWasSent(false);
        return outrow > 0;
    }
    return false;
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
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}


END_NCBI_SCOPE



