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
 * File Description:  TDS bcp-in command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include <string.h>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_BCPInCmd::
//

CTDS_BCPInCmd::CTDS_BCPInCmd(CTDS_Connection* con,
                             DBPROCESS*       cmd,
                             const string&    table_name,
                             unsigned int     nof_columns) :
    m_Connect(con), m_Cmd(cmd), m_Params(nof_columns),
    m_WasSent(false), m_HasFailed(false),
    m_HasTextImage(false), m_WasBound(false)
{
    if (bcp_init(cmd, (char*) table_name.c_str(), 0, 0, DB_IN) != SUCCEED) {
        DATABASE_DRIVER_FATAL( "bcp_init failed", 223001 );
    }
}


bool CTDS_BCPInCmd::Bind(unsigned int column_num, CDB_Object* param_ptr)
{
    return m_Params.BindParam(column_num,  kEmptyStr, param_ptr);
}


bool CTDS_BCPInCmd::x_AssignParams(void* pb)
{
    RETCODE r;

    if (!m_WasBound) {
        for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
            if (m_Params.GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *m_Params.GetParam(i);

            switch ( param.GetType() ) {
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                // DBINT v = (DBINT) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT4, i + 1);
            }
            break;
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                // DBSMALLINT v = (DBSMALLINT) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT2, i + 1);
            }
            break;
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                // DBTINYINT v = (DBTINYINT) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT1, i + 1);
            }
            break;
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
                DBNUMERIC* v = reinterpret_cast<DBNUMERIC*> (pb);
                Int8 v8 = val.Value();
                v->precision= 18;
                v->scale= 0;
                if (longlong_to_numeric(v8, 18, v->array) == 0)
                    return false;
                r = bcp_bind(m_Cmd, (BYTE*) v, 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBNUMERIC, i + 1);
                pb = (void*) (v + 1);
            }
            break;
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : -1,
                             (BYTE*) "", 1, SYBCHAR, i + 1);
            }
            break;
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : -1,
                             (BYTE*) "", 1, SYBCHAR, i + 1);
            }
            break;
            case eDB_LongChar: {
                CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : -1,
                             (BYTE*) "", 1, SYBCHAR, i + 1);
            }
            break;
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : val.Size(),
                             0, 0, SYBBINARY, i + 1);
            }
            break;
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : val.Size(),
                             0, 0, SYBBINARY, i + 1);
            }
            break;
            case eDB_LongBinary: {
                CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : val.DataSize(),
                             0, 0, SYBBINARY, i + 1);
            }
            break;
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                // DBREAL v = (DBREAL) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBREAL, i + 1);
            }
            break;
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                // DBFLT8 v = (DBFLT8) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBFLT8, i + 1);
            }
            break;
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                DBDATETIME4* dt = (DBDATETIME4*) pb;
                dt->days        = val.GetDays();
                dt->minutes     = val.GetMinutes();
                r = bcp_bind(m_Cmd, (BYTE*) dt, 0, val.IsNULL() ? 0 : -1,
                             0, 0, SYBDATETIME4,  i + 1);
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_DateTime: {
                CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
                DBDATETIME* dt = (DBDATETIME*) pb;
                dt->dtdays     = val.GetDays();
                dt->dttime     = val.Get300Secs();
                r = bcp_bind(m_Cmd, (BYTE*) dt, 0, val.IsNULL() ? 0 : -1,
                             0, 0, SYBDATETIME, i + 1);
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_Text: {
                CDB_Text& val = dynamic_cast<CDB_Text&> (param);
                r = bcp_bind(m_Cmd, 0, 0, val.IsNULL() ? 0 : val.Size(),
                             0, 0, SYBTEXT, i + 1);
                m_HasTextImage = true;
            }
            break;
            case eDB_Image: {
                CDB_Image& val = dynamic_cast<CDB_Image&> (param);
                r = bcp_bind(m_Cmd, 0, 0, val.IsNULL() ? 0 : val.Size(),
                             0, 0, SYBIMAGE, i + 1);
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
                    bcp_collen(m_Cmd,  val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                // DBSMALLINT v = (DBSMALLINT) val.Value();
                r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,  val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                // DBTINYINT v = (DBTINYINT) val.Value();
                r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
                DBNUMERIC* v = (DBNUMERIC*) pb;
                v->precision= 18;
                v->scale= 0;
                Int8 v8 = val.Value();
                if (longlong_to_numeric(v8, 18, v->array) == 0)
                    return false;
                r = bcp_colptr(m_Cmd, (BYTE*) v, i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,  val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (v + 1);
            }
            break;
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_LongChar: {
                CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : val.Size(), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : val.Size(), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_LongBinary: {
                CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : val.DataSize(), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                //DBREAL v = (DBREAL) val.Value();
                r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,  val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                //DBFLT8 v = (DBFLT8) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBFLT8, i + 1);
            }
            break;
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                DBDATETIME4* dt = (DBDATETIME4*) pb;
                dt->days        = val.GetDays();
                dt->minutes     = val.GetMinutes();
                r = bcp_colptr(m_Cmd, (BYTE*) dt, i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
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
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_Text: {
                CDB_Text& val = dynamic_cast<CDB_Text&> (param);
                r = bcp_collen(m_Cmd, val.Size(), i + 1);
            }
            break;
            case eDB_Image: {
                CDB_Image& val = dynamic_cast<CDB_Image&> (param);
                r = bcp_collen(m_Cmd, val.Size(), i + 1);
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


bool CTDS_BCPInCmd::SendRow()
{
    char param_buff[2048]; // maximal row size, assured of buffer overruns

    if (!x_AssignParams(param_buff)) {
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "cannot assign params", 223004 );
    }

    if (bcp_sendrow(m_Cmd) != SUCCEED) {
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "bcp_sendrow failed", 223005 );
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
                if (bcp_moretext(m_Cmd, l, (BYTE*) buff) != SUCCEED) {
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

    return true;
}


bool CTDS_BCPInCmd::Cancel()
{
    if(m_WasSent) {
        DBINT outrow = bcp_done(m_Cmd);
        m_WasSent= false;
        return outrow == 0;
    }
    return false;
}


bool CTDS_BCPInCmd::CompleteBatch()
{
    if(m_WasSent) {
        DBINT outrow = bcp_batch(m_Cmd);
        if(outrow < 0) {
            m_HasFailed= true;
            DATABASE_DRIVER_ERROR( "bcp_batch failed", 223020 );
        }
        return outrow > 0;
    }
    return false;
}


bool CTDS_BCPInCmd::CompleteBCP()
{
    if(m_WasSent) {
        DBINT outrow = bcp_done(m_Cmd);
        if(outrow < 0) {
            m_HasFailed = true;
            DATABASE_DRIVER_ERROR( "bcp_done failed", 223020 );
        }
        m_WasSent= false;
        return outrow > 0;
    }
    return false;
}


void CTDS_BCPInCmd::Release()
{
    m_BR = 0;
    if (m_WasSent) {
        Cancel();
        m_WasSent = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CTDS_BCPInCmd::~CTDS_BCPInCmd()
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
 * Revision 1.7  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.6  2004/05/17 21:13:37  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.5  2003/04/29 21:15:03  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.4  2002/12/03 19:20:40  soussov
 * some minor fixes in bcp_batch, bcp_done
 *
 * Revision 1.3  2002/03/04 19:09:27  soussov
 * fixed bug in m_WasSent flag setting
 *
 * Revision 1.2  2001/11/06 18:00:02  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
