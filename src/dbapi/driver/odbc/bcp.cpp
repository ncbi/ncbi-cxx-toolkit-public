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
 * File Description:  ODBC bcp-in command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include <dbapi/error_codes.hpp>
#include <string.h>

#include <odbcss.h>

#include "odbc_utils.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_Odbc_Cmds

#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_ODBC_THROW(ex_class, message, err_code, severity)
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE

#define DBDATETIME4_days(x) ((x)->numdays)
#define DBDATETIME4_mins(x) ((x)->nummins)
#define DBNUMERIC_val(x) ((x)->val)
#define SQL_VARLEN_DATA (-10)

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_BCPInCmd::
//

CODBC_BCPInCmd::CODBC_BCPInCmd(CODBC_Connection& conn,
                               SQLHDBC           cmd,
                               const string&     table_name) :
    CStatementBase(conn, table_name),
    m_Cmd(cmd),
    m_HasTextImage(false),
    m_WasBound(false)
{
    string extra_msg = "Table Name: " + table_name;
    SetDbgInfo( extra_msg );

    if (bcp_init(cmd,
                 CODBCString(table_name, GetClientEncoding()).AsCWString(),
                 0, 0, DB_IN) != SUCCEED) {
        ReportErrors();
        string err_message = "bcp_init failed." + GetDbgInfo();
        DATABASE_DRIVER_ERROR( err_message, 423001 );
    }

    ++m_RowCount;
}


bool CODBC_BCPInCmd::Bind(unsigned int column_num, CDB_Object* param_ptr)
{
    return GetBindParamsImpl().BindParam(column_num,  kEmptyStr, param_ptr);
}


int
CODBC_BCPInCmd::x_GetBCPDataType(EDB_Type type)
{
    int bcp_datatype = 0;

    switch (type) {
    case eDB_Int:
        bcp_datatype = SQLINT4;
        break;
    case eDB_SmallInt:
        bcp_datatype = SQLINT2;
        break;
    case eDB_TinyInt:
        bcp_datatype = SQLINT1;
        break;
    case eDB_BigInt:
        bcp_datatype = SQLINT8;
        break;
    case eDB_Char:
    case eDB_VarChar:
    case eDB_LongChar:
#ifdef UNICODE
        bcp_datatype = SQLNCHAR;
#else
        bcp_datatype = SQLCHARACTER;
#endif
        break;
    case eDB_Binary:
    case eDB_VarBinary:
    case eDB_LongBinary:
        bcp_datatype = SQLBINARY;
        break;
        /*
    case eDB_Binary:
        bcp_datatype = SQLBINARY;
        break;
    case eDB_VarBinary:
        bcp_datatype = SQLVARBINARY;
        break;
    case eDB_LongBinary:
        bcp_datatype = SQLBIGBINARY;
        break;
        */
    case eDB_Float:
        bcp_datatype = SQLFLT4;
        break;
    case eDB_Double:
        bcp_datatype = SQLFLT8;
        break;
    case eDB_SmallDateTime:
        bcp_datatype = SQLDATETIM4;
        break;
    case eDB_DateTime:
        bcp_datatype = SQLDATETIME;
        break;
    case eDB_Text:
//TODO: Make different type depending on type of underlying column
/*#ifdef UNICODE
        bcp_datatype = SQLNTEXT;
#else*/
        bcp_datatype = SQLTEXT;
//#endif
        break;
    case eDB_Image:
        bcp_datatype = SQLIMAGE;
        break;
    default:
        break;
    }

    return bcp_datatype;
}


size_t
CODBC_BCPInCmd::x_GetDataTermSize(EDB_Type type)
{
    switch (type) {
    case eDB_Char:
    case eDB_VarChar:
    case eDB_LongChar:
    case eDB_Text:
        return sizeof(odbc::TChar);
    default:
        break;
    }

    return 0;
}


void*
CODBC_BCPInCmd::x_GetDataTerminator(EDB_Type type)
{
    switch (type) {
    case eDB_Char:
    case eDB_VarChar:
    case eDB_LongChar:
    case eDB_Text:
        return _T_NCBI_ODBC("");
    default:
        break;
    }

    return NULL;
}


const void*
CODBC_BCPInCmd::x_GetDataPtr(EDB_Type type, void* pb)
{
    switch (type) {
    case eDB_Text:
    case eDB_Image:
        return NULL;
    default:
        break;
    }

    return pb;
}


size_t
CODBC_BCPInCmd::x_GetBCPDataSize(EDB_Type type)
{
    switch (type) {
    case eDB_Image:
    case eDB_Binary:
    case eDB_VarBinary:
    case eDB_LongBinary:
        return 1;
    default:
        break;
    }

    return SQL_VARLEN_DATA;
}


bool CODBC_BCPInCmd::x_AssignParams(void* pb)
{
    RETCODE r;

    if (!m_WasBound) {
        for (unsigned int i = 0; i < GetBindParamsImpl().NofParams(); ++i) {
            if (GetBindParamsImpl().GetParamStatus(i) == 0) {
                r = bcp_bind(GetHandle(), (BYTE*) pb, 0, SQL_VARLEN_DATA, 0, 0, 0, i + 1);
            }
            else {
                CDB_Object& param = *GetBindParamsImpl().GetParam(i);

                EDB_Type data_type = param.GetType();
                r = bcp_bind(GetHandle(),
                             static_cast<LPCBYTE>(const_cast<void*>(x_GetDataPtr(data_type, pb))),
                             0,
                             static_cast<DBINT>(x_GetBCPDataSize(data_type)),
                             static_cast<LPCBYTE>(x_GetDataTerminator(data_type)),
                             static_cast<INT>(x_GetDataTermSize(data_type)),
                             x_GetBCPDataType(data_type),
                             i + 1);

                m_HasTextImage = m_HasTextImage || (data_type == eDB_Image || data_type == eDB_Text);
            }

            if (r != SUCCEED) {
                ReportErrors();
                return false;
            }
        }

    GetBindParamsImpl().LockBinding();
        m_WasBound = true;
    }

    for (unsigned int i = 0; i < GetBindParamsImpl().NofParams(); i++) {
        if (GetBindParamsImpl().GetParamStatus(i) == 0) {
            r = bcp_collen(GetHandle(), SQL_NULL_DATA, i + 1);
        }
        else {
            CDB_Object& param = *GetBindParamsImpl().GetParam(i);

            switch ( param.GetType() ) {
            case eDB_Bit:
                DATABASE_DRIVER_ERROR("Bit data type is not supported", 10005);
                break;
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                r = bcp_colptr(GetHandle(), (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),  val.IsNULL() ? SQL_NULL_DATA : sizeof(Int4), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                // DBSMALLINT v = (DBSMALLINT) val.Value();
                r = bcp_colptr(GetHandle(), (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),  val.IsNULL() ? SQL_NULL_DATA : sizeof(Int2), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                // DBTINYINT v = (DBTINYINT) val.Value();
                r = bcp_colptr(GetHandle(), (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(), val.IsNULL() ? SQL_NULL_DATA : sizeof(Uint1), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
                r = bcp_colptr(GetHandle(), (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),  val.IsNULL() ? SQL_NULL_DATA : sizeof(Int8), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Char:
            case eDB_VarChar:
            case eDB_LongChar: {
                CDB_String& val = dynamic_cast<CDB_String&> (param);
                CTempString data;

                if (val.IsNULL()) {
                    data.assign(static_cast<char*>(pb), 0);
                } else {
                    val.GetBulkInsertionData
                        (&data, IsMultibyteClientEncoding());
                }

                DBINT length;
                INT   dummy; // Can't just supply NULL
                if (val.IsNULL()) {
                    length = SQL_NULL_DATA;
                } else if (bcp_getcolfmt(GetHandle(), i + 1, BCP_FMT_DATA_LEN,
                                         &length, sizeof(length), &dummy)
                           == FAIL  ||  length < 0) {
                    length = SQL_VARLEN_DATA; // Be conservative.
                } else if (static_cast<SIZE_TYPE>(length) > data.size()) {
                    // Retain automatic truncation, which blindly supplying
                    // data.size() could lose.
                    length = data.size();
                }

                r = bcp_colptr(GetHandle(),
                               (BYTE*) data.data(),
                               i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),
                               length,
                               i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                r = bcp_colptr(GetHandle(), (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),
                               val.IsNULL() ? SQL_NULL_DATA : (Int4) val.Size(), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                r = bcp_colptr(GetHandle(), (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),
                               val.IsNULL() ? SQL_NULL_DATA : (Int4) val.Size(), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_LongBinary: {
                CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
                r = bcp_colptr(GetHandle(), (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),
                               val.IsNULL() ? SQL_NULL_DATA : (Int4) val.DataSize(), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                //DBREAL v = (DBREAL) val.Value();
                r = bcp_colptr(GetHandle(), (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),  val.IsNULL() ? SQL_NULL_DATA : sizeof(float), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                //DBFLT8 v = (DBFLT8) val.Value();
                r = bcp_colptr(GetHandle(), (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(),  val.IsNULL() ? SQL_NULL_DATA : sizeof(double), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                DBDATETIM4* dt = (DBDATETIM4*) pb;
                DBDATETIME4_days(dt)     = val.GetDays();
                DBDATETIME4_mins(dt)     = val.GetMinutes();
                r = bcp_colptr(GetHandle(), (BYTE*) dt, i + 1)
                    == SUCCEED &&
                    bcp_collen(GetHandle(), val.IsNULL() ? SQL_NULL_DATA : sizeof(DBDATETIM4), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_DateTime: {
                CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
                DBDATETIME* dt = (DBDATETIME*) pb;
                if (val.IsNULL()) {
                    r = bcp_colptr(GetHandle(), (BYTE*) dt, i + 1)
                        == SUCCEED &&
                        bcp_collen(GetHandle(), SQL_NULL_DATA, i + 1)
                        == SUCCEED ? SUCCEED : FAIL;
                }
                else {
                    dt->dtdays     = val.GetDays();
                    dt->dttime     = val.Get300Secs();
                    r = bcp_colptr(GetHandle(), (BYTE*) dt, i + 1)
                        == SUCCEED &&
                        bcp_collen(GetHandle(), sizeof(DBDATETIME), i + 1)
                        == SUCCEED ? SUCCEED : FAIL;
                }
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_Text: {
                CDB_Text& val = dynamic_cast<CDB_Text&> (param);
                if (val.IsNULL()) {
                    r = bcp_colptr(GetHandle(), (BYTE*) pb, i + 1)
                        == SUCCEED &&
                        bcp_collen(GetHandle(),  SQL_NULL_DATA, i + 1)
                        == SUCCEED ? SUCCEED : FAIL;
                }
                else {
                    r = bcp_bind(GetHandle(), (BYTE*) NULL, 0, (DBINT) val.Size(),
                                 static_cast<LPCBYTE>(x_GetDataTerminator(eDB_Text)),
                                 static_cast<INT>(x_GetDataTermSize(eDB_Text)),
                                 x_GetBCPDataType(eDB_Text),
                                 i + 1);
                }
            }
            break;
            case eDB_Image: {
                CDB_Image& val = dynamic_cast<CDB_Image&> (param);
                // Null images doesn't work in odbc
                // (at least in those tests that exists in dbapi_unit_test)
                r = bcp_collen(GetHandle(),  (DBINT) val.Size(), i + 1);
            }
            break;
            default:
                return false;
            }
        }

        if (r != SUCCEED) {
            ReportErrors();
            return false;
        }
    }

    return true;
}


bool CODBC_BCPInCmd::Send(void)
{
    char param_buff[2048]; // maximal row size, assured of buffer overruns

    if (!x_AssignParams(param_buff)) {
        SetHasFailed();
        string err_message = "Cannot assign params." + GetDbgInfo();
        DATABASE_DRIVER_ERROR( err_message, 423004 );
    }

    SetWasSent();

    if (bcp_sendrow(GetHandle()) != SUCCEED) {
        SetHasFailed();
        ReportErrors();
        string err_message = "bcp_sendrow failed." + GetDbgInfo();
        DATABASE_DRIVER_ERROR( err_message, 423005 );
    }

    if (m_HasTextImage) { // send text/image data
        char buff[1800]; // text/image page size

        for (unsigned int i = 0; i < GetBindParamsImpl().NofParams(); ++i) {
            if (GetBindParamsImpl().GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *GetBindParamsImpl().GetParam(i);

            if (param.GetType() != eDB_Image &&
                (param.GetType() != eDB_Text  ||  param.IsNULL()))
                continue;

            CDB_Stream& val = dynamic_cast<CDB_Stream&> (param);

            size_t left_bytes = val.Size();
            size_t len = 0;
            size_t valid_len = 0;
            size_t invalid_len = 0;

            do {
                invalid_len = len - valid_len;

                if (valid_len < len) {
                    memmove(buff, buff + valid_len, invalid_len);
                }

                len = val.Read(buff + invalid_len, sizeof(buff) - invalid_len);
                if (len > left_bytes) {
                    len = left_bytes;
                }

                valid_len = CUtf8::GetValidBytesCount( CTempString(buff, len));

                CODBCString odbc_str(buff, len);

                // !!! TODO: Decode to UCS2 if needed !!!!
                if (bcp_moretext(GetHandle(),
                                 (DBINT) valid_len,
                                 (LPCBYTE)static_cast<const char*>(odbc_str)
                                 ) != SUCCEED) {
                    SetHasFailed();
                    ReportErrors();

                    string err_text;
                    if (param.GetType() == eDB_Text) {
                        err_text = "bcp_moretext for text failed.";
                    } else {
                        err_text = "bcp_moretext for image failed.";
                    }
                    err_text += GetDbgInfo();
                    DATABASE_DRIVER_ERROR( err_text, 423006 );
                }

                if (!valid_len) {
                    break;
                }

                left_bytes -= valid_len;
            } while (left_bytes);
        }
    }

    return true;
}


bool CODBC_BCPInCmd::Cancel()
{
    if (WasSent()) {
        bcp_control(GetHandle(), BCPABORT, NULL);
        DBINT outrow = bcp_done(GetHandle());
        SetWasSent(false);
        return outrow == 0;
    }

    return true;
}


bool CODBC_BCPInCmd::CommitBCPTrans(void)
{
    if(WasSent()) {
        Int4 outrow = bcp_batch(GetHandle());
        if(outrow == -1) {
            SetHasFailed();
            ReportErrors();
            DATABASE_DRIVER_ERROR( "bcp_batch failed." + GetDbgInfo(), 423006 );
            return false;
        }
        return true;
    }
    return false;
}


bool CODBC_BCPInCmd::EndBCP(void)
{
    if(WasSent()) {
        Int4 outrow = bcp_done(GetHandle());
        if(outrow == -1) {
            SetHasFailed();
            ReportErrors();
            DATABASE_DRIVER_ERROR( "bcp_done failed." + GetDbgInfo(), 423007 );
            return false;
        }
        SetWasSent(false);
        return true;
    }
    return false;
}


int CODBC_BCPInCmd::RowCount(void) const
{
    return static_cast<int>(m_RowCount);
}

CODBC_BCPInCmd::~CODBC_BCPInCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}


END_NCBI_SCOPE


