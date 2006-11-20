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
 */


#include <ncbi_pch.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include <string.h>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_BCPInCmd::
//

CTL_BCPInCmd::CTL_BCPInCmd(CTL_Connection* conn,
                           CS_BLKDESC* cmd,
                           const string& table_name,
                           unsigned int nof_columns) :
    CTL_Cmd(conn, NULL),
    impl::CBaseCmd(table_name, nof_columns),
    m_Cmd(cmd),
    m_Bind(nof_columns),
    m_RowCount(0)
{
}

bool CTL_BCPInCmd::Bind(unsigned int column_num, CDB_Object* pVal)
{
    return GetParams().BindParam(column_num, kEmptyStr, pVal);
}


bool CTL_BCPInCmd::x_IsUnicodeClientAPI(void) const
{
    switch (GetConnection().GetCTLibContext().GetTDSVersion()) {
    case 70:
    case 80:
        return true;
    };

    return false;
}


CS_VOID*
CTL_BCPInCmd::x_GetValue(const CDB_Char& value) const
{
#if defined(HAVE_WSTRING)
    if (x_IsUnicodeClientAPI()) {
        return const_cast<CS_VOID*>(
            static_cast<const CS_VOID*>(value.AsUnicode(eEncoding_UTF8)));
    }
#endif

    return const_cast<CS_VOID*>(
        static_cast<const CS_VOID*>(value.Value()));
}


CS_VOID*
CTL_BCPInCmd::x_GetValue(const CDB_VarChar& value) const
{
#if defined(HAVE_WSTRING)
    if (x_IsUnicodeClientAPI()) {
        return const_cast<CS_VOID*>(
            static_cast<const CS_VOID*>(value.AsUnicode(eEncoding_UTF8)));
    }
#endif

    return const_cast<CS_VOID*>(
        static_cast<const CS_VOID*>(value.Value()));
}


bool CTL_BCPInCmd::x_AssignParams()
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.format = CS_FMT_UNUSED;
    param_fmt.count  = 1;

    for (unsigned int i = 0;  i < GetParams().NofParams();  i++) {

        if (GetParams().GetParamStatus(i) == 0)
            continue;

        CDB_Object& param = *GetParams().GetParam(i);
        m_Bind[i].indicator = param.IsNULL() ? -1 : 0;
        m_Bind[i].datalen   = (m_Bind[i].indicator == 0) ? CS_UNUSED : 0;

        CS_RETCODE ret_code;

        switch ( param.GetType() ) {
        case eDB_Int: {
            CDB_Int& par = dynamic_cast<CDB_Int&> (param);
            param_fmt.datatype = CS_INT_TYPE;
            CS_INT value = (CS_INT) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_INT));
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      (CS_VOID*) m_Bind[i].buffer,
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_SmallInt: {
            CDB_SmallInt& par = dynamic_cast<CDB_SmallInt&> (param);
            param_fmt.datatype = CS_SMALLINT_TYPE;
            CS_SMALLINT value = (CS_SMALLINT) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_SMALLINT));
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      (CS_VOID*) m_Bind[i].buffer,
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_TinyInt: {
            CDB_TinyInt& par = dynamic_cast<CDB_TinyInt&> (param);
            param_fmt.datatype = CS_TINYINT_TYPE;
            CS_TINYINT value = (CS_TINYINT) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_TINYINT));
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      (CS_VOID*) m_Bind[i].buffer,
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
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
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      (CS_VOID*) m_Bind[i].buffer,
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_Char: {
            CDB_Char& par = dynamic_cast<CDB_Char&> (param);
            param_fmt.datatype  = CS_CHAR_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size() + 1;
            m_Bind[i].datalen   =
                (m_Bind[i].indicator == -1) ? 0 : (CS_INT) par.Size();
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      par.IsNULL()? (CS_VOID*)m_Bind[i].buffer :
                                          x_GetValue(par),
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_VarChar: {
            CDB_VarChar& par = dynamic_cast<CDB_VarChar&> (param);
            param_fmt.datatype  = CS_CHAR_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size() + 1;
            m_Bind[i].datalen   = (CS_INT) par.Size();
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      par.IsNULL()? (CS_VOID*)m_Bind[i].buffer :
                                          x_GetValue(par),
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_Binary: {
            CDB_Binary& par = dynamic_cast<CDB_Binary&> (param);
            param_fmt.datatype  = CS_BINARY_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size() + 1;
            m_Bind[i].datalen   =
                (m_Bind[i].indicator == -1) ? 0 : (CS_INT) par.Size();
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      par.IsNULL()? (CS_VOID*)m_Bind[i].buffer :
                                        (CS_VOID*) par.Value(),
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_VarBinary: {
            CDB_VarBinary& par = dynamic_cast<CDB_VarBinary&> (param);
            param_fmt.datatype  = CS_BINARY_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size() + 1;
            m_Bind[i].datalen   = (CS_INT) par.Size();
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      par.IsNULL()? (CS_VOID*)m_Bind[i].buffer :
                                        (CS_VOID*) par.Value(),
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_Float: {
            CDB_Float& par = dynamic_cast<CDB_Float&> (param);
            param_fmt.datatype = CS_REAL_TYPE;
            CS_REAL value = (CS_REAL) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_REAL));
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      (CS_VOID*) m_Bind[i].buffer,
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_Double: {
            CDB_Double& par = dynamic_cast<CDB_Double&> (param);
            param_fmt.datatype = CS_FLOAT_TYPE;
            CS_FLOAT value = (CS_FLOAT) par.Value();
            memcpy(m_Bind[i].buffer, &value, sizeof(CS_FLOAT));
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      (CS_VOID*) m_Bind[i].buffer,
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_SmallDateTime: {
            CDB_SmallDateTime& par = dynamic_cast<CDB_SmallDateTime&> (param);
            param_fmt.datatype = CS_DATETIME4_TYPE;
            CS_DATETIME4 dt;
            dt.days    = par.GetDays();
            dt.minutes = par.GetMinutes();
            memcpy(m_Bind[i].buffer, &dt, sizeof(CS_DATETIME4));
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      (CS_VOID*) m_Bind[i].buffer,
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_DateTime: {
            CDB_DateTime& par = dynamic_cast<CDB_DateTime&> (param);
            param_fmt.datatype = CS_DATETIME_TYPE;
            CS_DATETIME dt;
            dt.dtdays = par.GetDays();
            dt.dttime = par.Get300Secs();
            memcpy(m_Bind[i].buffer, &dt, sizeof(CS_DATETIME));
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      (CS_VOID*) m_Bind[i].buffer,
                                      &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_Text: {
            CDB_Text& par = dynamic_cast<CDB_Text&> (param);
            param_fmt.datatype  = CS_TEXT_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size();
            m_Bind[i].datalen   = (CS_INT) par.Size();
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      0, &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        case eDB_Image: {
            CDB_Image& par = dynamic_cast<CDB_Image&> (param);
            param_fmt.datatype  = CS_IMAGE_TYPE;
            param_fmt.maxlength = (CS_INT) par.Size();
            m_Bind[i].datalen   = (CS_INT) par.Size();
            ret_code = Check(blk_bind(x_GetSybaseCmd(), i + 1, &param_fmt,
                                      0, &m_Bind[i].datalen,
                                      &m_Bind[i].indicator));
            break;
        }
        default:
            return false;
        }

        if (ret_code != CS_SUCCEED) {
            return false;
        }
    }

    return true;
}


#if defined(HAVE_WSTRING)
static
string MakeUCS2LE(const wstring& str)
{
    string result;

#if defined(WORDS_BIGENDIAN)
    if (!str.empty()) {
        result.resize(str.size() * 2);
        for(wstring::size_type i = 0; i < str.size(); ++i) {
            wchar_t chracter = str.at(i);

            result.at(i * 2) = (chracter & 0x000000FF);
            result.at(i * 2 + 1) = (chracter & 0x0000FF00);
        }
    }
#else
    result.assign((const char*)str.c_str(), str.size() * sizeof(wchar_t));
#endif

    return result;
}
#endif


bool CTL_BCPInCmd::Send(void)
{
    unsigned int i;
    CS_INT       datalen = 0;
    CS_SMALLINT  indicator = 0;
    size_t       len = 0;
    char         buff[2048];

    if ( !m_WasSent ) {
        // we need to init the bcp
        CheckSFB(blk_init(x_GetSybaseCmd(), CS_BLK_IN, (CS_CHAR*) GetQuery().c_str(), CS_NULLTERM),
                 "blk_init failed", 123001);

        m_WasSent = true;

        // check what needs to be default
        CS_DATAFMT fmt;
        for (i = 0;  i < GetParams().NofParams();  i++) {
            if (GetParams().GetParamStatus(i) != 0) {
                continue;
            }

            m_HasFailed = (Check(blk_describe(x_GetSybaseCmd(),
                                              i + 1,
                                              &fmt)) != CS_SUCCEED);
            CHECK_DRIVER_ERROR(
                m_HasFailed,
                "blk_describe failed (check the number of "
                "columns in a table)",
                123002 );

            m_HasFailed = (Check(blk_bind(x_GetSybaseCmd(),
                                          i + 1,
                                          &fmt,
                                          (void*) &GetParams(),
                                          &datalen,
                                          &indicator)) != CS_SUCCEED);
            CHECK_DRIVER_ERROR(
                m_HasFailed,
                "blk_bind failed for default value",
                123003 );
        }
    }


    m_HasFailed = !x_AssignParams();
    CHECK_DRIVER_ERROR( m_HasFailed, "cannot assign the params", 123004 );

    switch ( Check(blk_rowxfer(x_GetSybaseCmd())) ) {
    case CS_BLK_HAS_TEXT:
        for (i = 0;  i < GetParams().NofParams();  i++) {
            if (GetParams().GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *GetParams().GetParam(i);

            if (param.GetType() == eDB_Text) {
                size_t valid_len = 0;
                size_t invalid_len = 0;
                CDB_Stream& par = dynamic_cast<CDB_Stream&> (param);

                for (datalen = (CS_INT) par.Size();  datalen > 0;
                     datalen -= (CS_INT) len)
                {
                    len = par.Read(buff + invalid_len, sizeof(buff) - invalid_len);

                    valid_len = CStringUTF8::GetValidBytesCount(buff, len);
                    invalid_len = len - valid_len;

#if defined(HAVE_WSTRING)
                    if (x_IsUnicodeClientAPI()) {
                        CWString unicode_str(buff, valid_len, eEncoding_UTF8);

                        string wcharle_str = MakeUCS2LE(unicode_str.AsUnicode(eEncoding_UTF8));

                        m_HasFailed = (Check(
                            blk_textxfer(
                                x_GetSybaseCmd(),
                                (CS_BYTE*) wcharle_str.c_str(),
                                (CS_INT) wcharle_str.size(),
                                0)
                            ) == CS_FAIL);
                    } else {
                        m_HasFailed = (Check(blk_textxfer(x_GetSybaseCmd(),
                                                          (CS_BYTE*) buff,
                                                          (CS_INT) valid_len,
                                                          0)
                                             ) == CS_FAIL);
                    }
#else
                    m_HasFailed = (Check(blk_textxfer(x_GetSybaseCmd(),
                                                      (CS_BYTE*) buff,
                                                      (CS_INT) valid_len,
                                                      0)
                                         ) == CS_FAIL);
#endif

                    CHECK_DRIVER_ERROR(
                        m_HasFailed,
                        "blk_textxfer failed for the text/image field", 123005
                        );

                    if (valid_len < len) {
                        memmove(buff, buff + valid_len, invalid_len);
                    }
                }
            } else if (param.GetType() == eDB_Image) {
                CDB_Stream& par = dynamic_cast<CDB_Stream&> (param);

                for (datalen = (CS_INT) par.Size();  datalen > 0; datalen -= (CS_INT) len) {
                    len = par.Read(buff, sizeof(buff));

                    m_HasFailed = (Check(blk_textxfer(x_GetSybaseCmd(), (CS_BYTE*) buff, (CS_INT) len, 0)) == CS_FAIL);

                    CHECK_DRIVER_ERROR(
                        m_HasFailed,
                        "blk_textxfer failed for the text/image field", 123005
                        );
                }
            } else {
                continue;
            }
        }
    case CS_SUCCEED:
        ++m_RowCount;
        return true;
    default:
        m_HasFailed = true;
        CHECK_DRIVER_ERROR( m_HasFailed, "blk_rowxfer failed", 123007 );
    }

    return false;
}


bool CTL_BCPInCmd::WasSent(void) const
{
    return m_WasSent;
}


bool CTL_BCPInCmd::Cancel()
{
    if(!m_WasSent) {
        return false;
    }

    CS_INT outrow = 0;

    return (CheckSentSFB(blk_done(x_GetSybaseCmd(), CS_BLK_CANCEL, &outrow),
                         "blk_done failed", 123020) == CS_SUCCEED);
}

bool CTL_BCPInCmd::WasCanceled(void) const
{
    return !m_WasSent;
}

bool CTL_BCPInCmd::CommitBCPTrans(void)
{
    if(!m_WasSent) return false;

    CS_INT outrow = 0;

    switch( Check(blk_done(x_GetSybaseCmd(), CS_BLK_BATCH, &outrow)) ) {
    case CS_SUCCEED:
        return (outrow > 0);
    case CS_FAIL:
        m_HasFailed = true;
        DATABASE_DRIVER_ERROR( "blk_done failed", 123020 );
    default:
        return false;
    }
}


bool CTL_BCPInCmd::EndBCP(void)
{
    if(!m_WasSent) return false;

    CS_INT outrow = 0;

    if (CheckSentSFB(blk_done(x_GetSybaseCmd(), CS_BLK_ALL, &outrow),
                     "blk_done failed", 123020) == CS_SUCCEED) {
        return (outrow > 0);
    }

    return false;
}


CDB_Result*
CTL_BCPInCmd::CreateResult(impl::CResult& result)
{
    return Create_Result(result);
}


bool CTL_BCPInCmd::HasFailed(void) const
{
    return m_HasFailed;
}


int CTL_BCPInCmd::RowCount(void) const
{
    return m_RowCount;
}


CTL_BCPInCmd::~CTL_BCPInCmd()
{
    try {
        DetachInterface();

        DropCmd(*this);

        Close();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


void
CTL_BCPInCmd::Close(void)
{
    if (x_GetSybaseCmd()) {
        // ????
        DetachInterface();

        Cancel();

        Check(blk_drop(x_GetSybaseCmd()));

        m_Cmd = NULL;
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.29  2006/11/20 18:15:58  ssikorsk
 * Revamp code to use GetQuery() and GetParams() methods.
 *
 * Revision 1.28  2006/10/06 22:29:48  ssikorsk
 * Explicitly convert text strings into UCS-2LE encoding for blk_textxfer in case of big endian.
 *
 * Revision 1.27  2006/10/04 19:26:57  ssikorsk
 * Revamp code to use AutoArray where it is possible.
 *
 * Revision 1.26  2006/10/04 14:34:09  ssikorsk
 * Minor fix to affect parameter calculation order.
 *
 * Revision 1.25  2006/10/03 17:53:43  ssikorsk
 * Implemented CTL_BCPInCmd::x_GetValue;
 * Translate text into unicode when BCPing text data types in case of TDS v70/80;
 *
 * Revision 1.24  2006/08/10 15:19:27  ssikorsk
 * Revamp code to use new CheckXXX methods.
 *
 * Revision 1.23  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.22  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.21  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.20  2006/06/05 21:09:21  ssikorsk
 * Replaced 'm_BR = 0' with 'CDB_BaseEnt::Release()'.
 *
 * Revision 1.19  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.18  2006/06/05 18:10:06  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.17  2006/05/03 15:10:36  ssikorsk
 * Implemented classs CTL_Cmd and CCTLExceptions;
 * Surrounded each native ctlib call with Check;
 *
 * Revision 1.16  2006/03/06 19:51:37  ssikorsk
 *
 * Added method Close/CloseForever to all context/command-aware classes.
 * Use getters to access Sybase's context and command handles.
 *
 * Revision 1.15  2006/02/22 15:56:39  ssikorsk
 * CHECK_DRIVER_FATAL --> CHECK_DRIVER_ERROR
 *
 * Revision 1.14  2006/02/22 15:15:50  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.13  2005/12/06 19:11:25  ssikorsk
 * Catch exceptions by const ref
 *
 * Revision 1.12  2005/09/19 14:19:02  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.11  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.10  2005/04/04 13:03:56  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.9  2004/10/19 15:32:44  soussov
 * fixes bug in binding NULL valued CDB_Char, CDB_VarChar, CDB_Binary, CDB_VarBinary
 *
 * Revision 1.8  2004/05/17 21:12:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.7  2003/01/31 16:49:38  lavr
 * Remove unused variable "e" from catch() clause
 *
 * Revision 1.6  2002/09/16 16:34:16  soussov
 * add try catch when canceling in Release method
 *
 * Revision 1.5  2002/09/16 15:13:12  soussov
 * throw exceptions in CompleteBCP, CompleteBatch, Cancel if blk_done fails
 *
 * Revision 1.4  2001/11/06 17:59:55  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/10/11 19:10:59  soussov
 * merges the text/image processing in CTL_BCPInCmd::SendRow()]
 *
 * Revision 1.2  2001/10/11 16:30:44  soussov
 * exclude ctlib dependencies from numeric conversion calls
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
