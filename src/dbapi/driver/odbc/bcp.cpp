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
#include <string.h>

#ifdef FTDS_IN_USE
    #include <sybdb.h>
#else
    #include <odbcss.h>
#endif

#include "odbc_utils.hpp"


BEGIN_NCBI_SCOPE

#define DBDATETIME4_days(x) ((x)->numdays)
#define DBDATETIME4_mins(x) ((x)->nummins)
#define DBNUMERIC_val(x) ((x)->val)
#define SQL_VARLEN_DATA (-10)

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_BCPInCmd::
//

CODBC_BCPInCmd::CODBC_BCPInCmd(CODBC_Connection* conn,
                               SQLHDBC          cmd,
                               const string&    table_name,
                               unsigned int     nof_columns) :
    CStatementBase(*conn),
    impl::CBaseCmd(table_name, nof_columns),
    m_Cmd(cmd),
    m_HasTextImage(false),
    m_WasBound(false)
{
    string extra_msg = "Table Name: " + table_name;
    SetDiagnosticInfo( extra_msg );

    if (bcp_init(cmd, CODBCString(table_name, GetClientEncoding()), 0, 0, DB_IN) != SUCCEED) {
        ReportErrors();
        string err_message = "bcp_init failed" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 423001 );
    }

    ++m_RowCount;
}


bool CODBC_BCPInCmd::Bind(unsigned int column_num, CDB_Object* param_ptr)
{
    return m_Params.BindParam(column_num,  kEmptyStr, param_ptr);
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
        bcp_datatype = SQLNUMERIC;
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
#ifdef UNICODE
        bcp_datatype = SQLNTEXT;
#else
        bcp_datatype = SQLTEXT;
#endif
        break;
    case eDB_Image:
        bcp_datatype = SQLIMAGE;
        break;
    }

    return bcp_datatype;
}


size_t
CODBC_BCPInCmd::x_GetBCPDataSize(EDB_Type type)
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
        return _T("");
    default:
        break;
    }

    return NULL;
}


void*
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


bool CODBC_BCPInCmd::x_AssignParams(void* pb)
{
    RETCODE r;

    if (!m_WasBound) {
        for (unsigned int i = 0; i < m_Params.NofParams(); ++i) {
            if (m_Params.GetParamStatus(i) == 0) {
                bcp_bind(GetHandle(), (BYTE*) pb, 0, SQL_NULL_DATA, 0, 0, 0, i + 1);
                continue;
            }

            CDB_Object& param = *m_Params.GetParam(i);

            EDB_Type data_type = param.GetType();
            r = bcp_bind(GetHandle(),
                         static_cast<const LPCBYTE>(x_GetDataPtr(data_type, pb)),
                         0,
                         (data_type == eDB_Image ? 1 : SQL_VARLEN_DATA),
                         static_cast<const LPCBYTE>(x_GetDataTerminator(data_type)),
                         x_GetBCPDataSize(data_type),
                         x_GetBCPDataType(data_type),
                         i + 1);

            m_HasTextImage = (data_type == eDB_Image || data_type == eDB_Text);

            if (r != SUCCEED) {
                ReportErrors();
                return false;
            }
        }
        m_WasBound = true;
    }
    for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
        if (m_Params.GetParamStatus(i) == 0)
            continue;

        CDB_Object& param = *m_Params.GetParam(i);

        switch ( param.GetType() ) {
        case eDB_Int: {
            CDB_Int& val = dynamic_cast<CDB_Int&> (param);
            // DBINT v = (DBINT) val.Value();
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
            DBNUMERIC* v = (DBNUMERIC*) pb;
            Int8 v8 = val.Value();
            if (longlong_to_numeric(v8, 18, DBNUMERIC_val(v)) == 0)
                return false;
            r = bcp_colptr(GetHandle(), (BYTE*) v, i + 1)
                == SUCCEED &&
                bcp_collen(GetHandle(),  val.IsNULL() ? SQL_NULL_DATA : sizeof(DBNUMERIC), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
            pb = (void*) (v + 1);
        }
        break;
        case eDB_Char: {
            CDB_Char& val = dynamic_cast<CDB_Char&> (param);
            BYTE* data = NULL;

            if (val.IsNULL()) {
                data = (BYTE*)pb;
            } else {
                if (IsMultibyteClientEncoding()) {
                    data = (BYTE*)val.AsUnicode(GetClientEncoding());
                } else {
                    data = (BYTE*)val.Value();
                }
            }

            r = bcp_colptr(GetHandle(),
                           data,
                           i + 1)
                == SUCCEED &&
                bcp_collen(GetHandle(),
                           val.IsNULL() ? SQL_NULL_DATA : SQL_VARLEN_DATA,
                           i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_VarChar: {
            CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
            BYTE* data = NULL;

            if (val.IsNULL()) {
                data = (BYTE*)pb;
            } else {
                if (IsMultibyteClientEncoding()) {
                    data = (BYTE*)val.AsUnicode(GetClientEncoding());
                } else {
                    data = (BYTE*)val.Value();
                }
            }

            r = bcp_colptr(GetHandle(),
                           data,
                           i + 1)
                == SUCCEED &&
                bcp_collen(GetHandle(),
                           val.IsNULL() ? SQL_NULL_DATA : SQL_VARLEN_DATA,
                           i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_LongChar: {
            CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
            BYTE* data = NULL;

            if (val.IsNULL()) {
                data = (BYTE*)pb;
            } else {
                if (IsMultibyteClientEncoding()) {
                    data = (BYTE*)val.AsUnicode(GetClientEncoding());
                } else {
                    data = (BYTE*)val.Value();
                }
            }

            r = bcp_colptr(GetHandle(),
                           data,
                           i + 1)
                == SUCCEED &&
                bcp_collen(GetHandle(),
                           val.IsNULL() ? SQL_NULL_DATA : SQL_VARLEN_DATA,
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
            dt->dtdays     = val.GetDays();
            dt->dttime     = val.Get300Secs();
            r = bcp_colptr(GetHandle(), (BYTE*) dt, i + 1)
                == SUCCEED &&
                bcp_collen(GetHandle(), val.IsNULL() ? SQL_NULL_DATA : sizeof(DBDATETIME), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
            pb = (void*) (dt + 1);
        }
        break;
        case eDB_Text: {
            CDB_Text& val = dynamic_cast<CDB_Text&> (param);
            // !!!!! Invalid size ... !!!!
            r = bcp_collen(GetHandle(), (DBINT) val.Size(), i + 1);
        }
        break;
        case eDB_Image: {
            CDB_Image& val = dynamic_cast<CDB_Image&> (param);
            r = bcp_collen(GetHandle(), (DBINT) val.Size(), i + 1);
        }
        break;
        default:
            return false;
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
        m_HasFailed = true;
        string err_message = "cannot assign params" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 423004 );
    }

    if (bcp_sendrow(GetHandle()) != SUCCEED) {
        m_HasFailed = true;
        ReportErrors();
        string err_message = "bcp_sendrow failed" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 423005 );
    }
    m_WasSent = true;

    if (m_HasTextImage) { // send text/image data
        char buff[1800]; // text/image page size

        for (unsigned int i = 0; i < m_Params.NofParams(); ++i) {
            if (m_Params.GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *m_Params.GetParam(i);

            if (param.GetType() != eDB_Text &&
                param.GetType() != eDB_Image)
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

                valid_len = CStringUTF8::GetValidBytesCount(buff, len);

                CODBCString odbc_str(buff, len);

                // !!! Decode to UCS2 !!!!
                if (bcp_moretext(GetHandle(),
                                 (DBINT) valid_len,
                                 (LPCBYTE)static_cast<const odbc::TChar*>(odbc_str)
                                 ) != SUCCEED) {
                    m_HasFailed = true;
                    ReportErrors();

                    string err_text;
                    if (param.GetType() == eDB_Text) {
                        err_text = "bcp_moretext for text failed ";
                    } else {
                        err_text = "bcp_moretext for image failed ";
                    }
                    err_text += GetDiagnosticInfo();
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


bool CODBC_BCPInCmd::WasSent(void) const
{
    return m_WasSent;
}


bool CODBC_BCPInCmd::Cancel()
{
    if (m_WasSent) {
        DBINT outrow = bcp_done(GetHandle());
        m_WasSent = false;
        return outrow == 0;
    }

    return true;
}


bool CODBC_BCPInCmd::WasCanceled(void) const
{
    return !m_WasSent;
}


bool CODBC_BCPInCmd::CommitBCPTrans(void)
{
    if(m_WasSent) {
        Int4 outrow = bcp_batch(GetHandle());
        if(outrow == -1) {
            ReportErrors();
            return false;
        }
        return true;
    }
    return false;
}


bool CODBC_BCPInCmd::EndBCP(void)
{
    if(m_WasSent) {
        Int4 outrow = bcp_done(GetHandle());
        m_WasSent= false;
        if(outrow == -1) {
            ReportErrors();
            return false;
        }
        return true;
    }
    return false;
}


bool CODBC_BCPInCmd::HasFailed(void) const
{
    return m_HasFailed;
}


int CODBC_BCPInCmd::RowCount(void) const
{
    return m_RowCount;
}

CODBC_BCPInCmd::~CODBC_BCPInCmd()
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
 * Revision 1.26  2006/10/26 15:08:39  ssikorsk
 * Use a charset provided by a client instead of a default one.
 *
 * Revision 1.25  2006/10/03 20:12:58  ssikorsk
 * strncpy --> memmove
 *
 * Revision 1.24  2006/09/18 16:15:33  ssikorsk
 * Restored setting of m_HasTextImage in CODBC_BCPInCmd::x_AssignParams.
 *
 * Revision 1.23  2006/09/18 15:29:21  ssikorsk
 * Improved reading data from streams in case of using of UTF8 in CODBC_BCPInCmd::Send.
 *
 * Revision 1.22  2006/09/13 20:05:14  ssikorsk
 * Revamp code to support  unicode version of ODBC API.
 *
 * Revision 1.21  2006/08/10 15:24:22  ssikorsk
 * Revamp code to use new CheckXXX methods.
 *
 * Revision 1.20  2006/07/31 15:50:58  ssikorsk
 * Use <sybdb.h> (from dblib) header in case of the FreeTDS odbc driver.
 *
 * Revision 1.19  2006/07/18 15:47:59  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.18  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.17  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.16  2006/06/05 21:09:22  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.15  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.14  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.13  2006/06/02 19:34:58  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.12  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.11  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.10  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.9  2005/11/02 12:58:38  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.8  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.7  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.6  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.5  2004/05/17 21:16:05  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.4  2003/05/08 20:30:24  soussov
 * CDB_LongChar CDB_LongBinary added
 *
 * Revision 1.3  2002/09/12 14:14:59  soussov
 * fixed typo in binding [var]char/binary with NULL value
 *
 * Revision 1.2  2002/09/11 20:23:54  soussov
 * fixed bug in binding [var]char/binary with NULL value
 *
 * Revision 1.1  2002/06/18 22:06:24  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */
