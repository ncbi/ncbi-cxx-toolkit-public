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
* File Description:  CTLib bcp-in command
*
*
*/


#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include <string.h>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_BCPInCmd::
//

CTL_BCPInCmd::CTL_BCPInCmd(CTL_Connection* con, CS_BLKDESC* cmd,
                           const string& table_name, unsigned int nof_columns)
    : m_Query(table_name), m_Params(nof_columns)
{
    m_Connect    = con;
    m_Cmd        = cmd;
    m_WasSent    = false;
    m_HasFailed  = false;
    m_Bind       = new SBcpBind[nof_columns];
}


bool CTL_BCPInCmd::Bind(unsigned int column_num, CDB_Object* pVal)
{
    return m_Params.BindParam(column_num, kEmptyStr, pVal);
}


bool CTL_BCPInCmd::x_AssignParams()
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.format = CS_FMT_UNUSED;
    param_fmt.count  = 1;

    for (unsigned int i = 0;  i < m_Params.NofParams();  i++) {

        if (m_Params.GetParamStatus(i) == 0)
            continue;

        CDB_Object& param = *m_Params.GetParam(i);
        m_Bind[i].indicator = param.IsNULL() ? -1 : 0;
        m_Bind[i].datalen   = (m_Bind[i].indicator == 0) ? CS_UNUSED : 0;

        CS_RETCODE ret_code;

        switch ( param.GetType() ) {
        case eDB_Int: {
            CDB_Int& par = dynamic_cast<CDB_Int&> (param);
            param_fmt.datatype = CS_INT_TYPE;
            CS_INT value = (CS_INT) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_INT));
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) m_Bind[i].buffer, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_SmallInt: {
            CDB_SmallInt& par = dynamic_cast<CDB_SmallInt&> (param);
            param_fmt.datatype = CS_SMALLINT_TYPE;
            CS_SMALLINT value = (CS_SMALLINT) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_SMALLINT));
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) m_Bind[i].buffer, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_TinyInt: {
            CDB_TinyInt& par = dynamic_cast<CDB_TinyInt&> (param);
            param_fmt.datatype = CS_TINYINT_TYPE;
            CS_TINYINT value = (CS_TINYINT) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_TINYINT));
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) m_Bind[i].buffer, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_BigInt: {
            CDB_BigInt& par = dynamic_cast<CDB_BigInt&> (param);
            param_fmt.datatype = CS_NUMERIC_TYPE;
            CS_NUMERIC value;
            Int8 val8 = par.Value();
	    memset(&value, 0, sizeof(value));
	    value.precision= 18;
            if (longlong_to_numeric(val8, 18, value.array) == 0)
                return false;
            param_fmt.scale     = 0;
            param_fmt.precision = 18;
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_NUMERIC));
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) m_Bind[i].buffer, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_Char: {
            CDB_Char& par = dynamic_cast<CDB_Char&> (param);
            param_fmt.datatype  = CS_CHAR_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size() + 1;
            m_Bind[i].datalen   =
                (m_Bind[i].indicator == -1) ? 0 : (CS_INT) par.Size();
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) par.Value(), &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_VarChar: {
            CDB_VarChar& par = dynamic_cast<CDB_VarChar&> (param);
            param_fmt.datatype  = CS_CHAR_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size() + 1;
            m_Bind[i].datalen   = (CS_INT) par.Size();
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) par.Value(), &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_Binary: {
            CDB_Binary& par = dynamic_cast<CDB_Binary&> (param);
            param_fmt.datatype  = CS_BINARY_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size() + 1;
            m_Bind[i].datalen   =
                (m_Bind[i].indicator == -1) ? 0 : (CS_INT) par.Size();
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) par.Value(), &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_VarBinary: {
            CDB_VarBinary& par = dynamic_cast<CDB_VarBinary&> (param);
            param_fmt.datatype  = CS_BINARY_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size() + 1;
            m_Bind[i].datalen   = (CS_INT) par.Size();
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) par.Value(), &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_Float: {
            CDB_Float& par = dynamic_cast<CDB_Float&> (param);
            param_fmt.datatype = CS_REAL_TYPE;
            CS_REAL value = (CS_REAL) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_REAL));
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) m_Bind[i].buffer, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_Double: {
            CDB_Double& par = dynamic_cast<CDB_Double&> (param);
            param_fmt.datatype = CS_FLOAT_TYPE;
            CS_FLOAT value = (CS_FLOAT) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_FLOAT));
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) m_Bind[i].buffer, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_SmallDateTime: {
            CDB_SmallDateTime& par = dynamic_cast<CDB_SmallDateTime&> (param);
            param_fmt.datatype = CS_DATETIME4_TYPE;
            CS_DATETIME4 dt;
            dt.days    = par.GetDays();
            dt.minutes = par.GetMinutes();
            memcpy(m_Bind[i].buffer, &dt, sizeof(CS_DATETIME4));
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) m_Bind[i].buffer, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_DateTime: {
            CDB_DateTime& par = dynamic_cast<CDB_DateTime&> (param);
            param_fmt.datatype = CS_DATETIME_TYPE;
            CS_DATETIME dt;
            dt.dtdays = par.GetDays();
            dt.dttime = par.Get300Secs();
            memcpy(m_Bind[i].buffer, &dt, sizeof(CS_DATETIME));
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                (CS_VOID*) m_Bind[i].buffer, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_Text: {
            CDB_Text& par = dynamic_cast<CDB_Text&> (param);
            param_fmt.datatype  = CS_TEXT_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size();
            m_Bind[i].datalen   = (CS_INT) par.Size();
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                0, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        case eDB_Image: {
            CDB_Image& par = dynamic_cast<CDB_Image&> (param);
            param_fmt.datatype  = CS_IMAGE_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size();
            m_Bind[i].datalen   = (CS_INT) par.Size();
            ret_code = blk_bind(m_Cmd, i + 1, &param_fmt,
                                0, &m_Bind[i].datalen,
                                &m_Bind[i].indicator);
            break;
        }
        default: {
            return false;
        }
        }

        if (ret_code != CS_SUCCEED) {
            return false;
        }
    }

    return true;
}


bool CTL_BCPInCmd::SendRow()
{
    unsigned int i;
    CS_INT       datalen = 0;
    CS_SMALLINT  indicator = 0;

    if ( !m_WasSent ) {
        // we need to init the bcp
        if (blk_init(m_Cmd, CS_BLK_IN, (CS_CHAR*) m_Query.c_str(), CS_NULLTERM)
            != CS_SUCCEED) {
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Fatal, 123001, "CTL_BCPInCmd::SendRow",
                               "blk_init failed");
        }
        m_WasSent = true;

        // check what needs to be default
        CS_DATAFMT fmt;
        for (i = 0;  i < m_Params.NofParams();  i++) {
            if (m_Params.GetParamStatus(i) != 0)
                continue;

            if (blk_describe(m_Cmd, i + 1, &fmt) != CS_SUCCEED) {
                m_HasFailed = true;
                throw CDB_ClientEx(eDB_Error, 123002, "CTL_BCPInCmd::SendRow",
                                   "blk_describe failed (check the number of "
                                   "columns in a table)");
            }

            if (blk_bind(m_Cmd, i + 1, &fmt, (void*) &m_Params,
                         &datalen, &indicator) != CS_SUCCEED) {
                m_HasFailed = true;
                throw CDB_ClientEx(eDB_Error, 123003, "CTL_BCPInCmd::SendRow",
                                   "blk_bind failed for default value");
            }
        }
    }


    if ( !x_AssignParams() ) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 123004, "CTL_BCPInCmd::SendRow",
                           "cannot assign the params");
    }

    switch ( blk_rowxfer(m_Cmd) ) {
    case CS_BLK_HAS_TEXT:  {
        char buff[2048];
        size_t n;
	
        for (i = 0;  i < m_Params.NofParams();  i++) {
            if (m_Params.GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *m_Params.GetParam(i);

            switch ( param.GetType() ) {
            case eDB_Text:  {
                CDB_Text& par = dynamic_cast<CDB_Text&> (param);
                for (datalen = (CS_INT) par.Size();  datalen > 0;
                     datalen -= (CS_INT) n) {
                    n = par.Read(buff, 2048);
                    if (blk_textxfer(m_Cmd, (CS_BYTE*) buff, (CS_INT) n, 0)
                        == CS_FAIL) {
                        m_HasFailed = true;
                        throw CDB_ClientEx
                            (eDB_Error, 123005, "CTL_BCPInCmd::SendRow",
                             "blk_textxfer failed for the text field");
                    }
                }
                break;
            }
            case eDB_Image:  {
                CDB_Image& par = dynamic_cast<CDB_Image&> (param);
                for (datalen = (CS_INT) par.Size();  datalen > 0;
                     datalen -= (CS_INT) n) {
                    n = par.Read(buff, 2048);
                    if (blk_textxfer(m_Cmd, (CS_BYTE*) buff, (CS_INT) n, 0)
                        == CS_FAIL) {
                        m_HasFailed = true;
                        throw CDB_ClientEx
                            (eDB_Error, 123006, "CTL_BCPInCmd::SendRow",
                             "blk_textxfer failed for the image field");
                    }
                }
                break;
            }
            default:
                continue;
            }
        }
    }
    case CS_SUCCEED:
        return true;
    default:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 123007,
                           "CTL_BCPInCmd::SendRow", "blk_rowxfer failed");
    }

    return false;
}


bool CTL_BCPInCmd::Cancel()
{
    CS_INT outrow = 0;

    bool result =
        m_WasSent  &&  blk_done(m_Cmd, CS_BLK_CANCEL, &outrow) == CS_SUCCEED;

    m_WasSent = false;
    return result;
}


bool CTL_BCPInCmd::CompleteBatch()
{
    CS_INT outrow = 0;

    return m_WasSent  &&  blk_done(m_Cmd, CS_BLK_BATCH, &outrow) == CS_SUCCEED;
}


bool CTL_BCPInCmd::CompleteBCP()
{
    CS_INT outrow = 0;

    return m_WasSent  &&   blk_done(m_Cmd, CS_BLK_ALL, &outrow) == CS_SUCCEED;
}


void CTL_BCPInCmd::Release()
{
    m_BR = 0;
    if (m_WasSent) {
        Cancel();
        m_WasSent = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CTL_BCPInCmd::~CTL_BCPInCmd()
{
    if ( m_BR ) {
        *m_BR = 0;
    }

    if ( m_WasSent ) {
        Cancel();
    }

    delete[] m_Bind;

    if (blk_drop(m_Cmd) != CS_SUCCEED) {
        throw CDB_ClientEx(eDB_Fatal, 123021,
                           "CTL_BCPInCmd::~CTL_BCPInCmd", "blk_drop failed");
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2001/10/11 16:30:44  soussov
 * excludes ctlib dependences fron numeric conversions calls
 *
 * Revision 1.1  2001/09/21 23:40:02  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
