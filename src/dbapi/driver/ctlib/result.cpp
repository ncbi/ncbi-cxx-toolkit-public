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
 * File Description:  CTLib Results
 *
 */


#include <ncbi_pch.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RowResult::
//


CTL_RowResult::CTL_RowResult(CS_COMMAND* cmd)
    : m_Cmd(cmd),
      m_CurrItem(-1),
      m_EOR(false)
{
    CS_INT outlen;

    CS_INT nof_cols;
    bool rc = (ct_res_info(m_Cmd, CS_NUMDATA, &nof_cols, CS_UNUSED, &outlen)
               != CS_SUCCEED);
    CHECK_DRIVER_ERROR( rc, "ct_res_info(CS_NUMDATA) failed", 130001 );

    m_NofCols = nof_cols;

    CS_INT cmd_num;
    rc = (ct_res_info(m_Cmd, CS_CMD_NUMBER, &cmd_num, CS_UNUSED, &outlen)
        != CS_SUCCEED);
    CHECK_DRIVER_ERROR( rc, "ct_res_info(CS_CMD_NUMBER) failed", 130001 );
    m_CmdNum = cmd_num;

    CS_INT bind_len= 0;
    m_BindedCols= 0;

    m_ColFmt = new CS_DATAFMT[m_NofCols];
    for (unsigned int nof_items = 0;  nof_items < m_NofCols;  nof_items++) {
        rc = (ct_describe(m_Cmd, (CS_INT) nof_items + 1, &m_ColFmt[nof_items])
            != CS_SUCCEED);
        CHECK_DRIVER_ERROR( rc, "ct_describe failed", 130002 );
        bind_len+= m_ColFmt[nof_items].maxlength;
        if(bind_len <= 2048) m_BindedCols++;
    }
    if(m_BindedCols) {
        m_BindItem= new CS_VOID*[m_BindedCols];
        m_Copied= new CS_INT[m_BindedCols];
        m_Indicator= new CS_SMALLINT[m_BindedCols];
        for(int i= 0; i < m_BindedCols; i++) {
          m_BindItem[i]= i? ((unsigned char*)(m_BindItem[i-1])) + m_ColFmt[i-1].maxlength : m_BindBuff;
          rc = (ct_bind(m_Cmd, i+1, &m_ColFmt[i], m_BindItem[i], &m_Copied[i], &m_Indicator[i]) 
             != CS_SUCCEED);
          CHECK_DRIVER_ERROR( rc, "ct_bind failed", 130042 );
        }
    }
}


EDB_ResType CTL_RowResult::ResultType() const
{
    return eDB_RowResult;
}


unsigned int CTL_RowResult::NofItems() const
{
    return m_NofCols;
}


const char* CTL_RowResult::ItemName(unsigned int item_num) const
{
    return (item_num < m_NofCols  &&  m_ColFmt[item_num].namelen > 0)
        ? m_ColFmt[item_num].name : 0;
}


size_t CTL_RowResult::ItemMaxSize(unsigned int item_num) const
{
    return (item_num < m_NofCols) ? m_ColFmt[item_num].maxlength : 0;
}


EDB_Type CTL_RowResult::ItemDataType(unsigned int item_num) const
{
    if (item_num >= m_NofCols) {
        return eDB_UnsupportedType;
    }

    const CS_DATAFMT& fmt = m_ColFmt[item_num];
    switch ( fmt.datatype ) {
    case CS_BINARY_TYPE:    return eDB_VarBinary;
    case CS_BIT_TYPE:       return eDB_Bit;
    case CS_CHAR_TYPE:      return eDB_VarChar;
    case CS_DATETIME_TYPE:  return eDB_DateTime;
    case CS_DATETIME4_TYPE: return eDB_SmallDateTime;
    case CS_TINYINT_TYPE:   return eDB_TinyInt;
    case CS_SMALLINT_TYPE:  return eDB_SmallInt;
    case CS_INT_TYPE:       return eDB_Int;
    case CS_DECIMAL_TYPE:
    case CS_NUMERIC_TYPE:   return (fmt.scale == 0  &&  fmt.precision < 20)
                                ? eDB_BigInt : eDB_Numeric;
    case CS_FLOAT_TYPE:     return eDB_Double;
    case CS_REAL_TYPE:      return eDB_Float;
    case CS_TEXT_TYPE:      return eDB_Text;
    case CS_IMAGE_TYPE:     return eDB_Image;
    case CS_LONGCHAR_TYPE:  return eDB_LongChar;
    case CS_LONGBINARY_TYPE: return eDB_LongBinary;
    }

    return eDB_UnsupportedType;
}


bool CTL_RowResult::Fetch()
{
    m_CurrItem = -1;
    if ( m_EOR ) {
        return false;
    }

    switch ( ct_fetch(m_Cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, 0) ) {
    case CS_SUCCEED:
        m_CurrItem = 0;
        return true;
    case CS_END_DATA:
        m_EOR = true;
        return false;
    case CS_ROW_FAIL:
        DATABASE_DRIVER_ERROR( "error while fetching the row", 130003 );
    case CS_FAIL:
        DATABASE_DRIVER_ERROR( "ct_fetch has failed. "
                           "You need to cancel the command", 130006 );
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
    default:
        DATABASE_DRIVER_ERROR( "the connection is busy", 130005 );
    }
}


int CTL_RowResult::CurrentItemNo() const
{
    return m_CurrItem;
}

CS_RETCODE CTL_RowResult::my_ct_get_data(CS_COMMAND* cmd, CS_INT item, 
                                         CS_VOID* buffer, 
                                         CS_INT buflen, CS_INT *outlen)
{
  if(item > m_BindedCols)
    return ct_get_data(cmd, item, buffer, buflen, outlen);

  --item;

  if((m_Indicator[item] < 0) || ((CS_INT)m_Indicator[item] >= m_Copied[item])) {
    if(outlen) *outlen= 0;
    return CS_END_ITEM;
  }

  if(!buffer || (buflen < 1)) return CS_SUCCEED;

  CS_INT n= m_Copied[item] - m_Indicator[item];
  if(buflen > n) buflen= n;
  memcpy(buffer, (char*)(m_BindItem[item]) + m_Indicator[item], buflen);
  if(outlen) *outlen= buflen;
  m_Indicator[item]+= buflen;
  return (n == buflen)? CS_END_ITEM : CS_SUCCEED;
}

// Aux. for CTL_RowResult::GetItem()
CDB_Object* CTL_RowResult::s_GetItem(CS_COMMAND* cmd, CS_INT item_no, CS_DATAFMT& fmt,
                                     CDB_Object* item_buf)
{
    CS_INT outlen = 0;
    char buffer[2048];
    EDB_Type b_type = item_buf ? item_buf->GetType() : eDB_UnsupportedType;

    switch ( fmt.datatype ) {
    case CS_BINARY_TYPE: {
        if (item_buf  &&
            b_type != eDB_VarBinary  &&  b_type != eDB_Binary  &&
            b_type != eDB_VarChar    &&  b_type != eDB_Char &&
            b_type != eDB_LongChar   &&  b_type != eDB_LongBinary) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        char* v = (fmt.maxlength < (CS_INT) sizeof(buffer))
            ? buffer : new char[fmt.maxlength + 1];

        switch ( my_ct_get_data(cmd, item_no, v, fmt.maxlength, &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                switch ( b_type ) {
                case eDB_VarBinary:
                    ((CDB_VarBinary*) item_buf)->SetValue(v, outlen);
                    break;
                case eDB_Binary:
                    ((CDB_Binary*)    item_buf)->SetValue(v, outlen);
                    break;
                case eDB_VarChar:
                    v[outlen] = '\0';
                    *((CDB_VarChar*)  item_buf) = v;
                    break;
                case eDB_Char:
                    v[outlen] = '\0';
                    *((CDB_Char*)     item_buf) = v;
                    break;
                case eDB_LongChar:
                    v[outlen] = '\0';
                    *((CDB_LongChar*)     item_buf) = v;
                    break;
                case eDB_LongBinary:
                    ((CDB_LongBinary*)     item_buf)->SetValue(v, outlen);
                    break;
                default:
                    break;
                }

                if (v != buffer)  delete[] v;
                return item_buf;
            }

            CDB_VarBinary* val = (outlen == 0)
                ? new CDB_VarBinary() : new CDB_VarBinary(v, outlen);

            if ( v != buffer)  delete[] v;
            return val;
        }
        case CS_CANCELED:
            if (v != buffer)  delete[] v;
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        default:
            if (v != buffer)  delete[] v;
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
    }

    case CS_LONGBINARY_TYPE: {
        if (item_buf  &&
            b_type != eDB_LongChar   &&  b_type != eDB_LongBinary) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        char* v = (fmt.maxlength < (CS_INT) sizeof(buffer))
            ? buffer : new char[fmt.maxlength + 1];

        switch ( my_ct_get_data(cmd, item_no, v, fmt.maxlength, &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                switch ( b_type ) {
                case eDB_LongBinary:
                    ((CDB_LongBinary*) item_buf)->SetValue(v, outlen);
                    break;
                case eDB_LongChar:
                    v[outlen] = '\0';
                    *((CDB_LongChar*)     item_buf) = v;
                default:
                    break;
                }

                if (v != buffer)  delete[] v;
                return item_buf;
            }

            CDB_LongBinary* val = (outlen == 0)
                ? new CDB_LongBinary(fmt.maxlength) : 
                  new CDB_LongBinary(fmt.maxlength, v, outlen);

            if ( v != buffer)  delete[] v;
            return val;
        }
        case CS_CANCELED:
            if (v != buffer)  delete[] v;
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        default:
            if (v != buffer)  delete[] v;
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
    }

    case CS_BIT_TYPE: {
        if (item_buf  &&
            b_type != eDB_Bit       &&  b_type != eDB_TinyInt  &&
            b_type != eDB_SmallInt  &&  b_type != eDB_Int) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_BIT v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                if (outlen == 0) {
                    item_buf->AssignNULL();
                }
                else {
                    switch ( b_type ) {
                    case eDB_Bit:
                        *((CDB_Bit*)      item_buf) = (int) v;
                        break;
                    case eDB_TinyInt:
                        *((CDB_TinyInt*)  item_buf) = v ? 1 : 0;
                        break;
                    case eDB_SmallInt:
                        *((CDB_SmallInt*) item_buf) = v ? 1 : 0;
                        break;
                    case eDB_Int:
                        *((CDB_Int*)      item_buf) = v ? 1 : 0;
                        break;
                    default:
                        break;
                    }
                }
                return item_buf;
            }

            return (outlen == 0) ? new CDB_Bit() : new CDB_Bit((int) v);
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_CHAR_TYPE: {
        if (item_buf  &&
            b_type != eDB_VarBinary  &&  b_type != eDB_Binary  &&
            b_type != eDB_VarChar    &&  b_type != eDB_Char &&
            b_type != eDB_LongChar   &&  b_type != eDB_LongBinary) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        char* v = fmt.maxlength < 2048
            ? buffer : new char[fmt.maxlength + 1];
        switch ( my_ct_get_data(cmd, item_no, v, fmt.maxlength, &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            v[outlen] = '\0';
            if ( item_buf) {
                if ( outlen <= 0) {
                    item_buf->AssignNULL();
                }
                else {
                    switch ( b_type ) {
                    case eDB_VarChar:
                        *((CDB_VarChar*)  item_buf) = v;
                        break;
                    case eDB_Char:
                        *((CDB_Char*)     item_buf) = v;
                        break;
                    case eDB_LongChar:
                        *((CDB_LongChar*)     item_buf) = v;
                        break;
                    case eDB_VarBinary:
                        ((CDB_VarBinary*) item_buf)->SetValue(v, outlen);
                        break;
                    case eDB_Binary:
                        ((CDB_Binary*)    item_buf)->SetValue(v, outlen);
                        break;
                    case eDB_LongBinary:
                        ((CDB_LongBinary*)    item_buf)->SetValue(v, outlen);
                        break;
                    default:
                        break;
                    }
                }

                if ( v != buffer) delete[] v;
                return item_buf;
            }

            CDB_VarChar* val = (outlen == 0)
                ? new CDB_VarChar() : new CDB_VarChar(v, (size_t) outlen);

            if (v != buffer) delete[] v;
            return val;
        }
        case CS_CANCELED: {
            if (v != buffer) delete[] v;
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            if (v != buffer) delete[] v;
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_LONGCHAR_TYPE: {
        if (item_buf  &&
            b_type != eDB_LongChar   &&  b_type != eDB_LongBinary) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        char* v = fmt.maxlength < 2048
            ? buffer : new char[fmt.maxlength + 1];
        switch ( my_ct_get_data(cmd, item_no, v, fmt.maxlength, &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            v[outlen] = '\0';
            if ( item_buf) {
                if ( outlen <= 0) {
                    item_buf->AssignNULL();
                }
                else {
                    switch ( b_type ) {
                    case eDB_LongChar:
                        *((CDB_LongChar*)     item_buf) = v;
                        break;
                    case eDB_LongBinary:
                        ((CDB_LongBinary*)    item_buf)->SetValue(v, outlen);
                        break;
                    default:
                        break;
                    }
                }

                if ( v != buffer) delete[] v;
                return item_buf;
            }

            CDB_LongChar* val = (outlen == 0)
                ? new CDB_LongChar(fmt.maxlength) : new CDB_LongChar((size_t) outlen, v);

            if (v != buffer) delete[] v;
            return val;
        }
        case CS_CANCELED: {
            if (v != buffer) delete[] v;
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            if (v != buffer) delete[] v;
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_DATETIME_TYPE: {
        if (item_buf  &&  b_type != eDB_DateTime) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_DATETIME v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf) {
                if ( outlen > 0) {
                    ((CDB_DateTime*)item_buf)->Assign(v.dtdays, v.dttime);
                }
                else {
                    item_buf->AssignNULL();
                }
                return item_buf;
            }

            CDB_DateTime* val;
            if ( outlen > 0) {
                val = new CDB_DateTime(v.dtdays, v.dttime);
            } else {
                val = new CDB_DateTime;
            }

            return val;
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_DATETIME4_TYPE:  {
        if (item_buf  &&
            b_type != eDB_SmallDateTime  &&  b_type != eDB_DateTime) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_DATETIME4 v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf) {
                if (outlen > 0) {
                    switch ( b_type ) {
                    case eDB_SmallDateTime:
                        ((CDB_SmallDateTime*) item_buf)->Assign
                            (v.days, v.minutes);
                        break;
                    case eDB_DateTime:
                        ((CDB_DateTime*)      item_buf)->Assign
                            (v.days, v.minutes * 60 * 300);
                        break;
                    default:
                        break;
                    }
                }
                else {
                    item_buf->AssignNULL();
                }
                return item_buf;
            }

            return (outlen <= 0)
                ? new CDB_SmallDateTime
                : new CDB_SmallDateTime(v.days, v.minutes);
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_TINYINT_TYPE: {
        if (item_buf  &&
            b_type != eDB_TinyInt  &&  b_type != eDB_SmallInt  &&
            b_type != eDB_Int) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_TINYINT v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                if (outlen == 0) {
                    item_buf->AssignNULL();
                }
                else {
                    switch ( b_type ) {
                    case eDB_TinyInt:
                        *((CDB_TinyInt*)  item_buf) = (Uint1) v;
                        break;
                    case eDB_SmallInt:
                        *((CDB_SmallInt*) item_buf) = (Int2) v;
                        break;
                    case eDB_Int:
                        *((CDB_Int*)      item_buf) = (Int4) v;
                        break;
                    default:
                        break;
                    }
                }
                return item_buf;
            }

            return (outlen == 0)
                ? new CDB_TinyInt() : new CDB_TinyInt((Uint1) v);
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_SMALLINT_TYPE: {
        if (item_buf  &&
            b_type != eDB_SmallInt  &&  b_type != eDB_Int) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_SMALLINT v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                if (outlen == 0) {
                    item_buf->AssignNULL();
                }
                else {
                    switch ( b_type ) {
                    case eDB_SmallInt:
                        *((CDB_SmallInt*) item_buf) = (Int2) v;
                        break;
                    case eDB_Int:
                        *((CDB_Int*)      item_buf) = (Int4) v;
                        break;
                    default:
                        break;
                    }
                }
                return item_buf;
            }

            return (outlen == 0)
                ? new CDB_SmallInt() : new CDB_SmallInt((Int2) v);
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_INT_TYPE: {
        if (item_buf  &&  b_type != eDB_Int) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_INT v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                if (outlen == 0) {
                    item_buf->AssignNULL();
                }
                else {
                    *((CDB_Int*) item_buf) = (Int4) v;
                }
                return item_buf;
            }

            return (outlen == 0) ? new CDB_Int() : new CDB_Int((Int4) v);
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_DECIMAL_TYPE:
    case CS_NUMERIC_TYPE: {
        if (item_buf  &&  b_type != eDB_BigInt  &&  b_type != eDB_Numeric) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_NUMERIC v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                if (outlen < 3) { /* it used to be == 0 but ctlib on windows
                     returns 2 even for NULL numeric */
                    item_buf->AssignNULL();
                }
                else {
                    if (b_type == eDB_Numeric) {
                        ((CDB_Numeric*) item_buf)->Assign
                            ((unsigned int)         v.precision,
                             (unsigned int)         v.scale,
                             (const unsigned char*) v.array);
                    }
                    else {
                        *((CDB_BigInt*) item_buf) =
                            numeric_to_longlong((unsigned int)
                                                v.precision, v.array);
                    }
                }
                return item_buf;
            }

            if (fmt.scale == 0  &&  fmt.precision < 20) {
                return (outlen == 0)
                    ? new CDB_BigInt
                    : new CDB_BigInt(numeric_to_longlong((unsigned int)
                                                         v.precision,v.array));
            } else {
                return  (outlen == 0)
                    ? new CDB_Numeric
                    : new CDB_Numeric((unsigned int)         v.precision,
                                      (unsigned int)         v.scale,
                                      (const unsigned char*) v.array);
            }
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_FLOAT_TYPE: {
        if (item_buf  &&  b_type != eDB_Double) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_FLOAT v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                if (outlen == 0) {
                    item_buf->AssignNULL();
                }
                else {
                    *((CDB_Double*) item_buf) = (double) v;
                }
                return item_buf;
            }

            return (outlen == 0)
                ? new CDB_Double() : new CDB_Double((double) v);
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_REAL_TYPE: {
        if (item_buf  &&  b_type != eDB_Float) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CS_REAL v;
        switch ( my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen) ) {
        case CS_SUCCEED:
        case CS_END_ITEM:
        case CS_END_DATA: {
            if ( item_buf ) {
                if (outlen == 0) {
                    item_buf->AssignNULL();
                }
                else {
                    *((CDB_Float*) item_buf) = (float) v;
                }
                return item_buf;
            }

            return (outlen == 0) ? new CDB_Float() : new CDB_Float((float) v);
        }
        case CS_CANCELED: {
            DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
        }
        default: {
            DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
        }
        }
    }

    case CS_TEXT_TYPE:
    case CS_IMAGE_TYPE: {
       if (item_buf  &&  b_type != eDB_Text  &&  b_type != eDB_Image) {
            DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object", 130020 );
        }

        CDB_Stream* val =
            item_buf                         ? (CDB_Stream*) item_buf
            : (fmt.datatype == CS_TEXT_TYPE) ? (CDB_Stream*) new CDB_Text
            :                                  (CDB_Stream*) new CDB_Image;

        for (;;) {
            switch ( my_ct_get_data(cmd, item_no, buffer, 2048, &outlen) ) {
            case CS_SUCCEED:
                if (outlen != 0)
                    val->Append(buffer, outlen);
                continue;
            case CS_END_ITEM:
            case CS_END_DATA:
                if (outlen != 0)
                    val->Append(buffer, outlen);
                return val;
            case CS_CANCELED:
                DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
            default:
                DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
            }
        }
    }

    default: {
        DATABASE_DRIVER_ERROR( "unexpected result type", 130004 );
    }
    }
}


CDB_Object* CTL_RowResult::GetItem(CDB_Object* item_buf)
{
    if ((unsigned int) m_CurrItem >= m_NofCols  ||  m_CurrItem == -1) {
        return 0;
    }

    CDB_Object* item = s_GetItem(m_Cmd, m_CurrItem + 1, m_ColFmt[m_CurrItem],
                                 item_buf);

    ++m_CurrItem;
    return item;
}


size_t CTL_RowResult::ReadItem(void* buffer, size_t buffer_size,
                               bool* is_null)
{
    if ((unsigned int) m_CurrItem >= m_NofCols  ||  m_CurrItem == -1) {
        return 0;
    }

    CS_INT outlen = 0;

    if((buffer == 0) && (buffer_size == 0)) {
    buffer= (void*)(&buffer_size);
    }

    switch ( my_ct_get_data(m_Cmd, m_CurrItem+1, buffer, (CS_INT) buffer_size,
                         &outlen) ) {
    case CS_END_ITEM:
    case CS_END_DATA:
        ++m_CurrItem;
    case CS_SUCCEED:
        break;
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
    default:
        DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
    }

    if ( is_null ) {
        *is_null = (outlen == 0);
    }

    return (size_t) outlen;
}


I_ITDescriptor* CTL_RowResult::GetImageOrTextDescriptor()
{
    if ((unsigned int) m_CurrItem >= m_NofCols  ||  m_CurrItem == -1) {
        return 0;
    }

    char dummy[4];
    switch ( my_ct_get_data(m_Cmd, m_CurrItem+1, dummy, 0, 0) ) {
    case CS_END_ITEM:
    case CS_END_DATA:
    case CS_SUCCEED:
        break;
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
    default:
        DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
    }


    auto_ptr<CTL_ITDescriptor> desc(new CTL_ITDescriptor);

    bool rc = (ct_data_info(m_Cmd, CS_GET, m_CurrItem+1, &desc->m_Desc)
        != CS_SUCCEED);
    CHECK_DRIVER_ERROR( rc, "ct_data_info failed", 130010 );

    return desc.release();
}


bool CTL_RowResult::SkipItem()
{
    if (m_CurrItem < (int) m_NofCols) {
        ++m_CurrItem;
        return true;
    }

    return false;
}


CTL_RowResult::~CTL_RowResult()
{
    if ( m_ColFmt ) {
        delete[] m_ColFmt;
    }
    if(m_BindedCols) {
      delete [] m_BindItem;
      delete [] m_Copied;
      delete [] m_Indicator;
    }

    if ( m_EOR ) {
        return;
    }

    switch (ct_cancel(NULL, m_Cmd, CS_CANCEL_CURRENT)) {
    case CS_SUCCEED:
    case CS_CANCELED:
        break;
    default:
        CS_INT err_code = 130007;
        ct_cmd_props(m_Cmd, CS_SET, CS_USERDATA,
                     &err_code, (CS_INT) sizeof(err_code), 0);
    }
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ParamResult::
//  CTL_ComputeResult::
//  CTL_StatusResult::
//  CTL_CursorResult::
//

EDB_ResType CTL_ParamResult::ResultType() const
{
    return eDB_ParamResult;
}


EDB_ResType CTL_ComputeResult::ResultType() const
{
    return eDB_ComputeResult;
}


EDB_ResType CTL_StatusResult::ResultType() const
{
    return eDB_StatusResult;
}


EDB_ResType CTL_CursorResult::ResultType() const
{
    return eDB_CursorResult;
}


bool CTL_CursorResult::SkipItem()
{
    if (m_CurrItem < (int) m_NofCols) {
        ++m_CurrItem;
    char dummy[4];
    switch ( my_ct_get_data(m_Cmd, m_CurrItem, dummy, 0, 0) ) {
    case CS_END_ITEM:
    case CS_END_DATA:
    case CS_SUCCEED:
        break;
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "the command has been canceled", 130004 );
    default:
        DATABASE_DRIVER_ERROR( "ct_get_data failed", 130000 );
    }
        return true;
    }

    return false;
}

CTL_CursorResult::~CTL_CursorResult()
{
    if (m_EOR) { // this is not a bug
        CS_INT res_type;
        while (ct_results(m_Cmd, &res_type) == CS_SUCCEED) {
            continue;
        }
    }
    else m_EOR= true; // to prevent ct_cancel call (close cursor will do a job)
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ITDescriptor::
//

CTL_ITDescriptor::CTL_ITDescriptor()
{
    return;
}

int CTL_ITDescriptor::DescriptorType() const
{
    return CTL_ITDESCRIPTOR_TYPE_MAGNUM;
}


CTL_ITDescriptor::~CTL_ITDescriptor()
{
    return;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.18  2004/05/17 21:12:03  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.17  2003/04/29 21:15:35  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.16  2003/04/21 20:18:14  soussov
 * Buffering fetched result to avoid ct_get_data performance issue
 *
 * Revision 1.15  2003/01/03 21:47:42  soussov
 * set m_CurrItem = -1 if fetch failes
 *
 * Revision 1.14  2002/11/27 17:09:50  soussov
 * patch to fix NULL numeric related bug in ctlib for Windows
 *
 * Revision 1.13  2002/03/26 15:34:38  soussov
 * new image/text operations added
 *
 * Revision 1.12  2002/02/06 22:24:43  soussov
 * fixes the arguments order in numeric assign
 *
 * Revision 1.11  2002/01/04 19:41:46  soussov
 * allows usage ReadItem(0, 0)
 *
 * Revision 1.10  2001/11/06 18:02:00  lavr
 * Added methods formely inline (moved from header files)
 *
 * Revision 1.9  2001/10/11 16:30:44  soussov
 * exclude ctlib dependencies from numeric conversion calls
 *
 * Revision 1.8  2001/10/03 14:21:01  soussov
 * pevents the ct_cancel call in ~CTL_CursorResult()
 *
 * Revision 1.7  2001/10/02 22:14:50  soussov
 * fixed bug in ~CTL_CursorResult()
 *
 * Revision 1.6  2001/10/01 19:27:40  soussov
 * Fixed typo in ~CTL_CursorResult
 *
 * Revision 1.5  2001/10/01 19:13:56  soussov
 * Eliminates double deletes in results destructor
 *
 * Revision 1.4  2001/09/27 23:01:07  vakatov
 * CTL_***Result::  virtual methods' implementation moved away from the header
 *
 * Revision 1.3  2001/09/27 20:08:33  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.2  2001/09/26 23:23:31  vakatov
 * Moved the err.message handlers' stack functionality (generic storage
 * and methods) to the "abstract interface" level.
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
