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
#include <odbcss.h>


BEGIN_NCBI_SCOPE

#define DBDATETIME4_days(x) ((x)->numdays)
#define DBDATETIME4_mins(x) ((x)->nummins)
#define DBNUMERIC_val(x) ((x)->val)
#define SQL_VARLEN_DATA (-10)

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_BCPInCmd::
//

CODBC_BCPInCmd::CODBC_BCPInCmd(CODBC_Connection* con,
                               SQLHDBC       cmd,
                               const string&    table_name,
                               unsigned int     nof_columns) :
    m_Connect(con), m_Cmd(cmd), m_Params(nof_columns),
    m_WasSent(false), m_HasFailed(false),
    m_HasTextImage(false), m_WasBound(false), 
    m_Reporter(&con->m_MsgHandlers, SQL_HANDLE_DBC, cmd)
{
    if (bcp_init(cmd, (char*) table_name.c_str(), 0, 0, DB_IN) != SUCCEED) {
        m_Reporter.ReportErrors();
        DATABASE_DRIVER_FATAL( "bcp_init failed", 423001 );
    }
}


bool CODBC_BCPInCmd::Bind(unsigned int column_num, CDB_Object* param_ptr)
{
    return m_Params.BindParam(column_num,  kEmptyStr, param_ptr);
}


bool CODBC_BCPInCmd::x_AssignParams(void* pb)
{
    RETCODE r;
    
    if (!m_WasBound) {
        for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
            if (m_Params.GetParamStatus(i) == 0) {
                bcp_bind(m_Cmd, (BYTE*) pb, 0, SQL_NULL_DATA, 0, 0, 0, i+1);
                continue;
            }
            
            CDB_Object& param = *m_Params.GetParam(i);
            
            switch ( param.GetType() ) {
            case eDB_Int:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA, 0, 0, SQLINT4, i + 1);
                break;
            case eDB_SmallInt:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA, 0, 0, SQLINT2, i + 1);
                break;
            case eDB_TinyInt: 
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA, 0, 0, SQLINT1, i + 1);
                break;
            case eDB_BigInt: 
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA, 0, 0, SQLNUMERIC, i + 1);
                break;
            case eDB_Char:
            case eDB_VarChar:
            case eDB_LongChar:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA,(BYTE*) "", 1, SQLCHARACTER, i + 1);
                break;
#if 0
            case eDB_VarChar:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA,(BYTE*) "", 1, SQLCHARACTER, i + 1);
                break;
            case eDB_Binary:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA, 0, 0, SQLBINARY, i + 1);
                break;
#endif
            case eDB_Binary:
            case eDB_VarBinary:
            case eDB_LongBinary:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA, 0, 0, SQLBINARY, i + 1);
                break;
            case eDB_Float:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA, 0, 0, SQLFLT4, i + 1);
                break;
            case eDB_Double:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0,
                             SQL_VARLEN_DATA, 0, 0, SQLFLT8, i + 1);
                break;
            case eDB_SmallDateTime:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0, SQL_VARLEN_DATA,
                             0, 0, SQLDATETIM4,  i + 1);
                break;
            case eDB_DateTime:
                r = bcp_bind(m_Cmd, (BYTE*) pb, 0, SQL_VARLEN_DATA,
                             0, 0, SQLDATETIME, i + 1);
                break;
            case eDB_Text:
                r = bcp_bind(m_Cmd, 0, 0,
                             SQL_VARLEN_DATA, (BYTE*) "", 1, 
                             SQLTEXT, i + 1);
                m_HasTextImage = true;
                break;
            case eDB_Image:
                r = bcp_bind(m_Cmd, 0, 0,
                             1, 0, 0, SQLIMAGE, i + 1);
                m_HasTextImage = true;
                break;
            default:
                return false;
            }
            if (r != SUCCEED) {
                m_Reporter.ReportErrors();
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
            r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd,  val.IsNULL() ? SQL_NULL_DATA : sizeof(Int4), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_SmallInt: {
            CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
            // DBSMALLINT v = (DBSMALLINT) val.Value();
            r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd,  val.IsNULL() ? SQL_NULL_DATA : sizeof(Int2), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_TinyInt: {
            CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
            // DBTINYINT v = (DBTINYINT) val.Value();
            r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd, val.IsNULL() ? SQL_NULL_DATA : sizeof(Uint1), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_BigInt: {
            CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
            DBNUMERIC* v = (DBNUMERIC*) pb;
            Int8 v8 = val.Value();
            if (longlong_to_numeric(v8, 18, DBNUMERIC_val(v)) == 0)
                return false;
            r = bcp_colptr(m_Cmd, (BYTE*) v, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd,  val.IsNULL() ? SQL_NULL_DATA : sizeof(DBNUMERIC), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
            pb = (void*) (v + 1);
        }
        break;
        case eDB_Char: {
            CDB_Char& val = dynamic_cast<CDB_Char&> (param);
            r = bcp_colptr(m_Cmd, (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd, val.IsNULL() ? SQL_NULL_DATA : SQL_VARLEN_DATA, i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_VarChar: {
            CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
            r = bcp_colptr(m_Cmd, (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd, val.IsNULL() ? SQL_NULL_DATA : SQL_VARLEN_DATA, i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_LongChar: {
            CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
            r = bcp_colptr(m_Cmd, (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd, val.IsNULL() ? SQL_NULL_DATA : SQL_VARLEN_DATA, i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_Binary: {
            CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
            r = bcp_colptr(m_Cmd, (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd,
                           val.IsNULL() ? SQL_NULL_DATA : (Int4) val.Size(), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_VarBinary: {
            CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
            r = bcp_colptr(m_Cmd, (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd,
                           val.IsNULL() ? SQL_NULL_DATA : (Int4) val.Size(), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_LongBinary: {
            CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
            r = bcp_colptr(m_Cmd, (!val.IsNULL())? ((BYTE*) val.Value()) : (BYTE*)pb, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd,
                           val.IsNULL() ? SQL_NULL_DATA : (Int4) val.DataSize(), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_Float: {
            CDB_Float& val = dynamic_cast<CDB_Float&> (param);
            //DBREAL v = (DBREAL) val.Value();
            r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd,  val.IsNULL() ? SQL_NULL_DATA : sizeof(float), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_Double: {
            CDB_Double& val = dynamic_cast<CDB_Double&> (param);
            //DBFLT8 v = (DBFLT8) val.Value();
            r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd,  val.IsNULL() ? SQL_NULL_DATA : sizeof(double), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
        }
        break;
        case eDB_SmallDateTime: {
            CDB_SmallDateTime& val =
                dynamic_cast<CDB_SmallDateTime&> (param);
            DBDATETIM4* dt = (DBDATETIM4*) pb;
            DBDATETIME4_days(dt)        = val.GetDays();
            DBDATETIME4_mins(dt)     = val.GetMinutes();
            r = bcp_colptr(m_Cmd, (BYTE*) dt, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd, val.IsNULL() ? SQL_NULL_DATA : sizeof(DBDATETIM4), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
            pb = (void*) (dt + 1);
        }
        break;
        case eDB_DateTime: {
            CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
            DBDATETIME* dt = (DBDATETIME*) pb;
            dt->dtdays     = val.GetDays();
            dt->dttime     = val.Get300Secs();
            r = bcp_colptr(m_Cmd, (BYTE*) dt, i + 1)
                == SUCCEED &&
                bcp_collen(m_Cmd, val.IsNULL() ? SQL_NULL_DATA : sizeof(DBDATETIME), i + 1)
                == SUCCEED ? SUCCEED : FAIL;
            pb = (void*) (dt + 1);
        }
        break;
        case eDB_Text: {
            CDB_Text& val = dynamic_cast<CDB_Text&> (param);
            r = bcp_collen(m_Cmd, (DBINT) val.Size(), i + 1);
        }
        break;
        case eDB_Image: {
            CDB_Image& val = dynamic_cast<CDB_Image&> (param);
            r = bcp_collen(m_Cmd, (DBINT) val.Size(), i + 1);
        }
        break;
        default:
            return false;
        }
        if (r != SUCCEED) {
            m_Reporter.ReportErrors();
            return false;
        }
    }
    return true;
}


bool CODBC_BCPInCmd::SendRow()
{
    char param_buff[2048]; // maximal row size, assured of buffer overruns
    
    if (!x_AssignParams(param_buff)) {
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "cannot assign params", 423004 );
    }

    if (bcp_sendrow(m_Cmd) != SUCCEED) {
        m_HasFailed = true;
        m_Reporter.ReportErrors();
        DATABASE_DRIVER_ERROR( "bcp_sendrow failed", 423005 );
    }
    m_WasSent = true;

    if (m_HasTextImage) { // send text/image data
        char buff[1800]; // text/image page size

        for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
            if (m_Params.GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *m_Params.GetParam(i);

            if (param.GetType() != eDB_Text &&
                param.GetType() != eDB_Image)
                continue;

            CDB_Stream& val = dynamic_cast<CDB_Stream&> (param);
            size_t s = val.Size();
            do {
                size_t l = val.Read(buff, sizeof(buff));
                if (l > s)
                    l = s;
                if (bcp_moretext(m_Cmd, (DBINT) l, (BYTE*) buff) != SUCCEED) {
                    m_HasFailed = true;
                    m_Reporter.ReportErrors();

                    string err_text;
                    if (param.GetType() == eDB_Text) {
                        err_text = "bcp_moretext for text failed";
                    } else {
                        err_text = "bcp_moretext for image failed";
                    }
                    DATABASE_DRIVER_ERROR( "bcp_sendrow failed", 423006 );
                }
                if (!l)
                    break;
                s -= l;
            } while (s);
        }
    }

    return true;
}


bool CODBC_BCPInCmd::Cancel()
{
    if(m_WasSent) {
        DBINT outrow = bcp_done(m_Cmd);
        m_WasSent= false;
        return outrow == 0;
    }
    return true;
}


bool CODBC_BCPInCmd::CompleteBatch()
{
    if(m_WasSent) {
        Int4 outrow = bcp_batch(m_Cmd);
        if(outrow == -1) {
            m_Reporter.ReportErrors();
            return false;
        }
        return true;
    }
    return false;
}


bool CODBC_BCPInCmd::CompleteBCP()
{
    if(m_WasSent) {
        Int4 outrow = bcp_done(m_Cmd);
        m_WasSent= false;
        if(outrow == -1) {
            m_Reporter.ReportErrors();
            return false;
        }
        return true;
    }
    return false;
}


void CODBC_BCPInCmd::Release()
{
    m_BR = 0;
    if (m_WasSent) {
        Cancel();
        m_WasSent = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CODBC_BCPInCmd::~CODBC_BCPInCmd()
{
    if (m_BR)
        *m_BR = 0;
    if (m_WasSent)
        Cancel();
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
