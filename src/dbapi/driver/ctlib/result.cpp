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
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_CTlib_Results

#undef NCBI_DATABASE_THROW
#undef NCBI_DATABASE_RETHROW

#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), GetConnection(), GetLastParams())
#define NCBI_DATABASE_RETHROW(prev_ex, ex_class, message, err_code, severity) \
    NCBI_DATABASE_RETHROW_ANNOTATED(prev_ex, ex_class, message, err_code, \
        severity, GetDbgInfo(), GetConnection(), GetLastParams())

BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
namespace NCBI_NS_FTDS_CTLIB
{
#define MAX_VARCHAR_SIZE 8000
#else
#define MAX_VARCHAR_SIZE 1900
#endif

/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RowResult::
//


CTL_RowResult::CTL_RowResult(CS_COMMAND* cmd, CTL_Connection& conn) :
    m_Connect(&conn),
    m_Cmd(cmd),
    m_CurrItem(-1),
    m_EOR(false),
    m_BindedCols(0)
{
    CheckIsDead();

    CS_INT outlen;

    CS_INT nof_cols;
    bool rc = (Check(ct_res_info(x_GetSybaseCmd(),
                                 CS_NUMDATA,
                                 &nof_cols,
                                 CS_UNUSED,
                                 &outlen))
               != CS_SUCCEED);
    CHECK_DRIVER_ERROR( rc, "ct_res_info(CS_NUMDATA) failed." + GetDbgInfo(), 130001 );

    CS_INT bind_len = 0;
    m_BindedCols = 0;
    bool buff_is_full = false;

    m_ColFmt = AutoArray<CS_DATAFMT>(nof_cols);
    m_NullValue = AutoArray<ENullValue>(nof_cols);

    for (unsigned int nof_item = 0;  nof_item < (unsigned int) nof_cols;  nof_item++) {
        rc = (Check(ct_describe(x_GetSybaseCmd(),
                                (CS_INT) nof_item + 1,
                                &m_ColFmt[nof_item]))
            != CS_SUCCEED);
        CHECK_DRIVER_ERROR( rc, "ct_describe failed." + GetDbgInfo(), 130002 );

        m_NullValue[nof_item] = eNullUnknown;

#ifdef FTDS_IN_USE
        // Seems like FreeTDS reports wrong maxlength in
        // ct_describe() - fix this when binding to a buffer.
        if (m_ColFmt[nof_item].datatype == CS_NUMERIC_TYPE
            || m_ColFmt[nof_item].datatype == CS_DECIMAL_TYPE
            ) {
            m_ColFmt[nof_item].maxlength = sizeof(CS_NUMERIC);
        }
#endif

        m_CachedRowInfo.Add(
                string(m_ColFmt[nof_item].name, m_ColFmt[nof_item].namelen),
                m_ColFmt[nof_item].maxlength,
                ConvDataType_Ctlib2DBAPI(m_ColFmt[nof_item])
                );

        if (!buff_is_full) {
            if (m_ColFmt[nof_item].maxlength > 2048
                ||  m_ColFmt[nof_item].datatype == CS_IMAGE_TYPE)
            {
                buff_is_full = true;
            } else {
                bind_len += m_ColFmt[nof_item].maxlength;
                if (bind_len <= 2048) {
                    m_BindedCols++;
                } else {
                    buff_is_full = true;
                }
            }
        }
    }



    if(m_BindedCols) {
        m_BindItem = AutoArray<CS_VOID*>(m_BindedCols);
        m_Copied = AutoArray<CS_INT>(m_BindedCols);
        m_Indicator = AutoArray<CS_SMALLINT>(m_BindedCols);

        for(int i= 0; i < m_BindedCols; i++) {
          m_BindItem[i] = (i ? ((unsigned char*)(m_BindItem[i-1])) + m_ColFmt[i-1].maxlength : m_BindBuff);
          rc = (Check(ct_bind(x_GetSybaseCmd(),
                              i+1,
                              &m_ColFmt[i],
                              m_BindItem[i],
                              &m_Copied[i],
                              &m_Indicator[i]) )
                    != CS_SUCCEED);

            CHECK_DRIVER_ERROR( rc, "ct_bind failed." + GetDbgInfo(), 130042 );
        }
    }
}


EDB_ResType CTL_RowResult::ResultType() const
{
    return eDB_RowResult;
}

EDB_Type
CTL_RowResult::ConvDataType_Ctlib2DBAPI(const CS_DATAFMT& fmt)
{
    switch ( fmt.datatype ) {
    case CS_VARBINARY_TYPE:
    case CS_BINARY_TYPE:        return eDB_VarBinary;
    case CS_BIT_TYPE:           return eDB_Bit;
    case CS_LONGCHAR_TYPE:
    case CS_VARCHAR_TYPE:
    case CS_CHAR_TYPE:          return eDB_VarChar;
    case CS_DATETIME_TYPE:      return eDB_DateTime;
    case CS_DATETIME4_TYPE:     return eDB_SmallDateTime;
    case CS_TINYINT_TYPE:       return eDB_TinyInt;
    case CS_SMALLINT_TYPE:      return eDB_SmallInt;
    case CS_INT_TYPE:           return eDB_Int;
    case CS_LONG_TYPE:          return eDB_BigInt;
    case CS_DECIMAL_TYPE:
    case CS_NUMERIC_TYPE:
        // 2**63 ~= 9.2e18; however, Sybase returns (U)BIGINT as
        // NUMERIC(20,0), albeit with different usertype values.
        if (fmt.scale > 0  ||  fmt.precision > 20) {
            return eDB_Numeric;
        } else if (fmt.precision < 20
                   ||  fmt.usertype == 43  ||  fmt.usertype == 46) {
            return eDB_BigInt;
        } else {
            return eDB_Numeric;
        }
    case CS_FLOAT_TYPE:         return eDB_Double;
    case CS_REAL_TYPE:          return eDB_Float;
    case CS_TEXT_TYPE:          return eDB_Text;
    case CS_IMAGE_TYPE:         return eDB_Image;
    case CS_LONGBINARY_TYPE:    return eDB_LongBinary;

#ifdef FTDS_IN_USE
    case CS_UNIQUE_TYPE:        return eDB_VarBinary;
#endif

//     case CS_MONEY_TYPE:
//     case CS_MONEY4_TYPE:
//     case CS_SENSITIVITY_TYPE:
//     case CS_BOUNDARY_TYPE:
//     case CS_VOID_TYPE:
//     case CS_USHORT_TYPE:
//     case CS_UNICHAR_TYPE:
    }

    return eDB_UnsupportedType;
}

bool CTL_RowResult::Fetch()
{
    SetCurrentItemNum(-1);
    if ( m_EOR ) {
        return false;
    }

    // Reset NullValue flags ...
    for (unsigned int nof_items = 0;  nof_items < GetDefineParams().GetNum();  ++nof_items) {
        m_NullValue[nof_items] = eNullUnknown;
    }

    CheckIsDead();

    switch ( Check(ct_fetch(x_GetSybaseCmd(), CS_UNUSED, CS_UNUSED, CS_UNUSED, 0)) ) {
    case CS_SUCCEED:
        SetCurrentItemNum(0);
        return true;
    case CS_END_DATA:
        m_EOR = true;
        return false;
    case CS_ROW_FAIL:
        DATABASE_DRIVER_ERROR( "Error while fetching the row." + GetDbgInfo(), 130003 );
    case CS_FAIL:
        DATABASE_DRIVER_ERROR("ct_fetch has failed. "
                              "You need to cancel the command." +
                              GetDbgInfo(),
                              130006 );
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "The command has been canceled." + GetDbgInfo(), 130004 );
    default:
        DATABASE_DRIVER_ERROR( "The connection is busy." + GetDbgInfo(), 130005 );
    }
}


int CTL_RowResult::CurrentItemNo() const
{
    return GetCurrentItemNum();
}

int CTL_RowResult::GetColumnNum(void) const
{
    return static_cast<int>(GetDefineParams().GetNum());
}

CS_RETCODE CTL_RowResult::my_ct_get_data(CS_COMMAND* cmd,
                                         CS_INT item,
                                         CS_VOID* buffer,
                                         CS_INT buflen,
                                         CS_INT *outlen,
                                         bool& is_null)
{
    CheckIsDead();

    is_null = false;

    if(item > m_BindedCols) {
        // Not bound ...
        CS_RETCODE rc = Check(ct_get_data(cmd, item, buffer, buflen, outlen));
        if ((rc == CS_END_ITEM || rc == CS_END_DATA)) {
            if (outlen) {
#ifdef FTDS_IN_USE
                if (*outlen == -1) {
                    is_null = true;
                    *outlen = 0;
                }
                else
                    is_null = false;
#else
                is_null = (*outlen == 0);
#endif
            }
        } else if (rc == CS_CANCELED) {
            DATABASE_DRIVER_ERROR("The command has been canceled.", 130004);
        } else if (rc != CS_SUCCEED) {
            DATABASE_DRIVER_ERROR("ct_get_data failed.", 130000);
        }
        return rc;
    }

    // Bound ...
    // Move data ...

    --item;

    CS_SMALLINT   indicator = m_Indicator[item];
    CS_INT        copied = m_Copied[item];

    //   if((m_Indicator[item] < 0) || ((CS_INT)m_Indicator[item] >= m_Copied[item])) {
    if(indicator < 0) {
        // Value is NULL ...
        is_null = true;

        if(outlen) {
            *outlen = 0;
        }

        return CS_END_ITEM;
    }

    if(!buffer || (buflen < 1)) {
        return CS_SUCCEED;
    }

    CS_INT n = copied - indicator;

    if(buflen > n) {
        buflen = n;
    }

    memcpy(buffer, (char*)(m_BindItem[item]) + indicator, buflen);

    if(outlen) {
        *outlen = buflen;
    }

    m_Indicator[item] += static_cast<CS_SMALLINT>(buflen);

    return (n == buflen) ? CS_END_ITEM : CS_SUCCEED;
}


// Traditional conversion policy, when given a target object.
static bool s_CanStore(CS_INT src, EDB_Type dst)
{
    switch (src) {
    case CS_CHAR_TYPE:
    case CS_BINARY_TYPE:
    case CS_VARCHAR_TYPE:
    case CS_VARBINARY_TYPE:
#ifdef FTDS_IN_USE
    case CS_UNIQUE_TYPE:
#endif
        if (dst == eDB_Char  ||  dst == eDB_VarBinary  ||  dst == eDB_Binary) {
            return true;
        }
        // else fall through
    case CS_LONGCHAR_TYPE:
        if (
#ifndef FTDS_IN_USE
            dst == eDB_Char  ||
#endif
            dst == eDB_VarChar) {
            return true;
        }
        // else fall through
    case CS_LONGBINARY_TYPE:
        return dst == eDB_LongChar  ||  dst == eDB_LongBinary;

    case CS_TEXT_TYPE:
    case CS_IMAGE_TYPE:
        return dst == eDB_Text  ||  dst == eDB_Image;

    case CS_BIT_TYPE:      if (dst == eDB_Bit)      { return true; }
    case CS_TINYINT_TYPE:  if (dst == eDB_TinyInt)  { return true; }
    case CS_SMALLINT_TYPE: if (dst == eDB_SmallInt) { return true; }
    case CS_INT_TYPE:      if (dst == eDB_Int)      { return true; }
    case CS_LONG_TYPE:     return dst == eDB_BigInt;

    case CS_REAL_TYPE:  return dst == eDB_Float;

    case CS_FLOAT_TYPE: return dst == eDB_Double;

    case CS_DATETIME4_TYPE: if (dst == eDB_SmallDateTime) { return true; }
    case CS_DATETIME_TYPE:  return dst == eDB_DateTime;

    case CS_NUMERIC_TYPE:
    case CS_DECIMAL_TYPE:
        return dst == eDB_BigInt  ||  dst == eDB_Numeric;
    }

    // Other types (MONEY, etc.) unhandled.
    return false;
}


// Aux. for CTL_RowResult::GetItem()
CDB_Object* CTL_RowResult::GetItemInternal(
	I_Result::EGetItem policy, 
	CS_COMMAND* cmd, 
	CS_INT item_no, 
	CS_DATAFMT& fmt,
	CDB_Object* item_buf
	)
{
    CS_INT outlen = 0, maxlength = fmt.maxlength;
    char buffer[2048];
    EDB_Type b_type = ConvDataType_Ctlib2DBAPI(fmt);
    bool is_null = false;

    CHECK_DRIVER_ERROR(b_type == eDB_UnsupportedType,
                       "Unsupported result type "
                       + NStr::NumericToString(fmt.datatype),
                       130004);

    if (item_buf == NULL) {
        _ASSERT(s_CanStore(fmt.datatype, b_type));
    } else {
        b_type = item_buf->GetType();
        CHECK_DRIVER_ERROR(!s_CanStore(fmt.datatype, b_type),
                           "Wrong type of CDB_Object.", 130020);
    }

    AutoPtr<CDB_Object> auto_item(item_buf, eNoOwnership);
#define ENSURE_ITEM() \
        if (item_buf == NULL) { \
            auto_item.reset(CDB_Object::Create(b_type, maxlength)); \
            item_buf = auto_item.get(); \
            _ASSERT(item_buf->IsNULL()); \
        } else if (is_null) { \
            item_buf->AssignNULL(); \
        } \
        if (is_null) { \
            break; \
        }

    switch ( fmt.datatype ) {
    case CS_CHAR_TYPE:
    case CS_BINARY_TYPE:
    case CS_VARCHAR_TYPE:
    case CS_VARBINARY_TYPE:
#ifdef FTDS_IN_USE
    case CS_UNIQUE_TYPE:
#endif
    case CS_LONGCHAR_TYPE:
    case CS_LONGBINARY_TYPE:
    {
        AutoPtr<char, ArrayDeleter<char> > auto_buf;
        char *v = buffer;
        if (maxlength >= (CS_INT) sizeof(buffer)) {
            auto_buf.reset(new char[maxlength + 1]);
            v = auto_buf.get();
        }

        my_ct_get_data(cmd, item_no, v, maxlength, &outlen, is_null);

        ENSURE_ITEM();

        CHECK_DRIVER_ERROR(fmt.datatype == CS_LONGCHAR_TYPE
                           &&  b_type == eDB_VarChar
                           &&  outlen > MAX_VARCHAR_SIZE,
                           "Invalid conversion to CDB_VarChar type.", 230021);

#ifndef FTDS_IN_USE
        CHECK_DRIVER_ERROR(fmt.datatype == CS_LONGCHAR_TYPE
                           &&  b_type == eDB_Char
                           &&  outlen > MAX_VARCHAR_SIZE,
                           "Invalid conversion to CDB_VarChar type.", 230022);
#endif

        switch (b_type) {
        case eDB_VarChar:
        case eDB_Char:
        case eDB_LongChar:
            v[outlen] = '\0';
            static_cast<CDB_String*>(item_buf)->Assign(v /* , outlen */);
            break;
        case eDB_VarBinary:
            static_cast<CDB_VarBinary*>(item_buf)->SetValue(v, outlen);
            break;
        case eDB_Binary:
            static_cast<CDB_Binary*>(item_buf)->SetValue(v, outlen);
            break;
        case eDB_LongBinary:
            static_cast<CDB_LongBinary*>(item_buf)->SetValue(v, outlen);
            break;
        default:
            _TROUBLE;
        }
        break;
    }

    case CS_TINYINT_TYPE:
    case CS_SMALLINT_TYPE:
    case CS_INT_TYPE:
    case CS_BIT_TYPE:
    case CS_LONG_TYPE:
    {
        Int8 v = 0;
        CS_INT sz;
        switch (fmt.datatype) {
        case CS_TINYINT_TYPE:  sz = sizeof(CS_TINYINT);  break;
        case CS_SMALLINT_TYPE: sz = sizeof(CS_SMALLINT); break;
        case CS_INT_TYPE:      sz = sizeof(CS_INT);      break;
        case CS_BIT_TYPE:      sz = sizeof(CS_BIT);      break;
        case CS_LONG_TYPE:     sz = sizeof(v);           break;
        default:               sz = 0;                   _TROUBLE;
        }
        my_ct_get_data(cmd, item_no, &v, sz, &outlen, is_null);
#ifdef WORDS_BIGENDIAN
        v >>= (sizeof(v) - sz) * CHAR_BIT;
#endif

        ENSURE_ITEM();

        switch (b_type) {
        case eDB_Int:      *(CDB_Int     *) item_buf = (Int4) v;  break;
        case eDB_SmallInt: *(CDB_SmallInt*) item_buf = (Int2) v;  break;
        case eDB_TinyInt:  *(CDB_TinyInt *) item_buf = (Int1) v;  break;
        case eDB_BigInt:   *(CDB_BigInt  *) item_buf =        v;  break;
        case eDB_Bit:      *(CDB_Bit     *) item_buf = (v != 0);  break;
        default:           _TROUBLE;
        }
        break;
    }

    case CS_DATETIME_TYPE:
    {
        CS_DATETIME v;
        my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen, is_null);
        ENSURE_ITEM();
        static_cast<CDB_DateTime*>(item_buf)->Assign(v.dtdays, v.dttime);
        break;
    }

    case CS_DATETIME4_TYPE:
    {
        CS_DATETIME4 v;
        my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen, is_null);

        ENSURE_ITEM();

        switch (b_type) {
        case eDB_SmallDateTime:
            ((CDB_SmallDateTime*)item_buf)->Assign(v.days, v.minutes);
            break;
        case eDB_DateTime:
            ((CDB_DateTime*)item_buf)->Assign(v.days, v.minutes * 60 * 300);
            break;
        default:
            _TROUBLE;
        }
        break;
    }

    case CS_DECIMAL_TYPE:
    case CS_NUMERIC_TYPE:
    {
        CS_NUMERIC v;
        my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen, is_null);

        if (outlen < 3) {
            /* ctlib on windows returns 2 even for NULL numeric */
            is_null = true;
        }

        ENSURE_ITEM();

        switch (b_type) {
        case eDB_BigInt:
            *static_cast<CDB_BigInt*>(item_buf) =
                numeric_to_longlong((unsigned int)v.precision, v.array);
            break;
        case eDB_Numeric:
            static_cast<CDB_Numeric*>(item_buf)->Assign
                            ((unsigned int)         v.precision,
                             (unsigned int)         v.scale,
                             (const unsigned char*) v.array);
            break;
        default:
            _TROUBLE;
        }
        break;
    }

    case CS_FLOAT_TYPE:
    {
        CS_FLOAT v;
        my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen, is_null);
        ENSURE_ITEM();
        *static_cast<CDB_Double*>(item_buf) = v;
        break;
    }

    case CS_REAL_TYPE:
    {
        CS_REAL v;
        my_ct_get_data(cmd, item_no, &v, (CS_INT) sizeof(v), &outlen, is_null);
        ENSURE_ITEM();
        *static_cast<CDB_Float*>(item_buf) = v;
        break;
    }

    case CS_TEXT_TYPE:
    case CS_IMAGE_TYPE:
    {
        CDB_Stream* val = static_cast<CDB_Stream*>(item_buf);

        if (item_buf == NULL) {
            auto_item.reset(CDB_Object::Create(b_type, maxlength));
            item_buf = auto_item.get();
            val = static_cast<CDB_Stream*>(item_buf);
        } else if (policy == I_Result::eAssignLOB) {
            // Explicitly truncate previous value ...
            val->Truncate();
        }

		_ASSERT(val);

        if (m_NullValue[GetCurrentItemNum()] == eIsNull) {
            is_null = true;
            break;
        }

        bool done = false;
        while (!done) {
            switch ( my_ct_get_data(cmd, item_no, buffer, sizeof(buffer), &outlen, is_null) ) {
            case CS_SUCCEED:
                // For historic reasons, Append with a length of 0 calls
                // strlen; avoid passing it inappropriate data.
                val->Append(outlen ? buffer : kEmptyCStr, outlen);
                continue;
            case CS_END_ITEM:
            case CS_END_DATA:
                val->Append(outlen ? buffer : kEmptyCStr, outlen);
                done = true;
                break;
            default:
                _TROUBLE;
            }

            if (is_null) {
                _ASSERT(outlen == 0);
                _ASSERT(m_NullValue[GetCurrentItemNum()] != eIsNotNull);
                // Conditional, so as to preserve existing values in
                // eAppendLOB mode.  (No explicit mode check needed, per
                // the conditional call to Truncate above.)
                if (val->Size() == 0) {
                    val->AssignNULL();
                }
                break;
            }
        }
        break;
    }

    default:
        _TROUBLE;
    }

#undef ENSURE_ITEM

    _ASSERT(item_buf->IsNULL() == is_null);
    return auto_item.release();
}


CDB_Object* CTL_RowResult::GetItem(CDB_Object* item_buf, I_Result::EGetItem policy)
{
    if ((unsigned int) CurrentItemNo() >= GetDefineParams().GetNum()  ||  CurrentItemNo() == -1) {
        return 0;
    }

    CDB_Object* item = GetItemInternal(
        policy, 
        x_GetSybaseCmd(), 
        CurrentItemNo() + 1, 
        m_ColFmt[CurrentItemNo()],
        item_buf);

    IncCurrentItemNum();

    return item;
}


size_t CTL_RowResult::ReadItem(void* buffer, size_t buffer_size,
                               bool* is_null)
{
    if ((unsigned int) CurrentItemNo() >= GetDefineParams().GetNum()  ||  CurrentItemNo() == -1) {
        return 0;
    }

    if (m_NullValue[GetCurrentItemNum()] == eIsNull) {
        if (is_null)
            *is_null = true;
        IncCurrentItemNum();
        return 0;
    }

    CS_INT outlen = 0;

    if((buffer == 0) && (buffer_size == 0)) {
        buffer= (void*)(&buffer_size);
    }

    bool is_null_tmp;
    const CS_RETCODE rc = my_ct_get_data(
            x_GetSybaseCmd(),
            GetCurrentItemNum() + 1,
            buffer,
            (CS_INT) buffer_size,
            &outlen,
            is_null_tmp
            );

    switch (rc) {
    case CS_END_ITEM:
        // This is not the last column in the row.
    case CS_END_DATA:
        // This is the last column in the row.
        if (m_NullValue[GetCurrentItemNum()] == eNullUnknown) {
            m_NullValue[GetCurrentItemNum()] = (is_null_tmp ? eIsNull : eIsNotNull);
        }

        if (is_null) {
            if (m_NullValue[GetCurrentItemNum()] != eNullUnknown) {
                *is_null = (m_NullValue[GetCurrentItemNum()] == eIsNull);
            } else {
                *is_null = (outlen == 0);
            }
        }

#ifdef FTDS_IN_USE
        if (rc == CS_END_ITEM) {
            IncCurrentItemNum();
        }
#else
        IncCurrentItemNum(); // That won't work with the ftds driver
#endif

        break;
    case CS_SUCCEED:
        // ct_get_data successfully retrieved a chunk of data that is
        // not the last chunk of data for this column.
        break;
    default:
        _TROUBLE;
    }

    return (size_t) outlen;
}


I_ITDescriptor* CTL_RowResult::GetImageOrTextDescriptor()
{
    return GetImageOrTextDescriptor(GetCurrentItemNum());
}


I_ITDescriptor*
CTL_RowResult::GetImageOrTextDescriptor(int item_num)
{
    bool is_null = false;

    if ((unsigned int) item_num >= GetDefineParams().GetNum()  ||  item_num < 0) {
        return 0;
    }

    char dummy[4];
    CS_INT outlen = 0;

    my_ct_get_data(x_GetSybaseCmd(), item_num + 1, dummy, 0, &outlen, is_null);

    if (is_null)
        m_NullValue[item_num] = eIsNull;

    auto_ptr<CTL_ITDescriptor> desc(new CTL_ITDescriptor);

    bool rc = (Check(ct_data_info(x_GetSybaseCmd(),
                                  CS_GET,
                                  item_num + 1,
                                  &desc->m_Desc))
        != CS_SUCCEED);
    CHECK_DRIVER_ERROR( rc, "ct_data_info failed." + GetDbgInfo(), 130010 );

    return desc.release();
}

bool CTL_RowResult::SkipItem()
{
    if (GetCurrentItemNum() < (int) GetDefineParams().GetNum()) {
        IncCurrentItemNum();
        return true;
    }

    return false;
}


CTL_RowResult::~CTL_RowResult()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}


void
CTL_RowResult::Close(void)
{
    if (x_GetSybaseCmd()) {
        if (m_EOR  ||  IsDead()) {
            return;
        }

        switch (Check(ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_CURRENT))) {
        case CS_SUCCEED:
        case CS_CANCELED:
            break;
        default:
            CS_INT err_code = 130007;
            Check(ct_cmd_props(x_GetSybaseCmd(), CS_SET, CS_USERDATA,
                         &err_code, (CS_INT) sizeof(err_code), 0));
        }

        m_Cmd = NULL;
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
    if (GetCurrentItemNum() < (int) GetDefineParams().GetNum()) {
        IncCurrentItemNum();
        char dummy[4];
        bool is_null = false;

        my_ct_get_data(x_GetSybaseCmd(), GetCurrentItemNum(), dummy, 0, 0,
                       is_null);

        return true;
    }

    return false;
}

bool CTL_CursorResult::Fetch(void)
{
    x_InvalidateDescriptors();
    return CTL_RowResult::Fetch();
}

void CTL_CursorResult::x_InvalidateDescriptors(void)
{
    ITERATE (set<CTL_CursorITDescriptor*>, it, m_Descriptors) {
        (*it)->Invalidate();
    }
    m_Descriptors.clear();
}

CTL_CursorResult::~CTL_CursorResult()
{
    x_InvalidateDescriptors();
    try {
        if (m_EOR  &&  !IsDead()) { // this is not a bug
            CS_INT res_type;
            while (Check(ct_results(x_GetSybaseCmd(), &res_type)) == CS_SUCCEED) {
                continue;
            }
        }
        else m_EOR= true; // to prevent ct_cancel call (close cursor will do a job)
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ITDescriptor::
//

CTL_ITDescriptor::CTL_ITDescriptor()
{
    memset(&m_Desc, 0xDB, sizeof(m_Desc));
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


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorITDescriptor::
//

CTL_CursorITDescriptor::CTL_CursorITDescriptor(CTL_CursorResult& cursor_result,
                                               const string& table_name,
                                               const string& column_name,
                                               CS_INT datatype)
    : CDB_ITDescriptor(table_name, column_name,
                       "CURRENT OF " + cursor_result.GetCursorName()),
      m_CursorResult(&cursor_result)
{
    if (datatype == CS_VARBINARY_TYPE  ||  datatype == CS_IMAGE_TYPE) {
        SetColumnType(eBinary);
    } else {
        SetColumnType(eText);
    }
    m_CursorResult->RegisterDescriptor(*this);
}

int CTL_CursorITDescriptor::DescriptorType() const
{
    return CTL_ITDESCRIPTOR_TYPE_CURSOR;
}

CTL_CursorITDescriptor::~CTL_CursorITDescriptor()
{
    m_CursorResult->UnregisterDescriptor(*this);
}


///////////////////////////////////////////////////////////////////////////////
CTL_CursorResultExpl::CTL_CursorResultExpl(CTL_LangCmd* cmd,
                                           const string& cursor_name) :
    CTL_CursorResult(cmd->x_GetSybaseCmd(), cmd->GetConnection(), cursor_name),
    m_Cmd(cmd),
    m_Res(NULL),
    m_CurItemNo(0),
    m_ReadBuffer(NULL)
{
}


void
CTL_CursorResultExpl::ClearFields(void)
{
    ITERATE(vector<CDB_Object*>, it, m_Fields) {
        delete *it;
    }
    ITERATE(vector<I_ITDescriptor*>, it, m_ITDescrs) {
        delete *it;
    }
    m_Fields.clear();
    m_ITDescrs.clear();

    if (m_ReadBuffer) {
        free(m_ReadBuffer);
        m_ReadBuffer = NULL;
    }
}


EDB_ResType CTL_CursorResultExpl::ResultType() const
{
    return eDB_CursorResult;
}


bool CTL_CursorResultExpl::Fetch()
{
    // try to get next cursor result
    try {
        bool fetch_succeeded = false;
        m_CurItemNo = -1;
        ClearFields();

        GetCmd().Send();
        while (GetCmd().HasMoreResults()) {
            m_Res = GetCmd().Result();

            if (m_Res && m_Res->ResultType() == eDB_RowResult) {
                fetch_succeeded = m_Res->Fetch();
                if (fetch_succeeded)
                    break;
            }

            if (m_Res) {
                while (m_Res->Fetch()) {
                    continue;
                }
                delete m_Res;
                m_Res = NULL;
            }
        }

        if (!fetch_succeeded)
            return false;

        m_CachedRowInfo = static_cast<const impl::CCachedRowInfo&>
                                                   (m_Res->GetDefineParams());

        int col_cnt = m_Res->GetColumnNum();
        m_Fields.resize(col_cnt, NULL);
        m_ITDescrs.resize(col_cnt, NULL);
        for (int i = 0; i < col_cnt; ++i) {
            EDB_Type item_type = m_Res->ItemDataType(m_Res->CurrentItemNo());
            if (item_type == eDB_Text || item_type == eDB_Image) {
                m_ITDescrs[i] = m_Res->GetImageOrTextDescriptor();
            }
            m_Fields[i] = m_Res->GetItem();
        }

        m_CurItemNo = 0;
        m_ReadBytes = 0;

        // finish this command
        delete m_Res;
        m_Res = NULL;

        // Looks like DumpResult() to me ...
        while (GetCmd().HasMoreResults()) {
            auto_ptr<CDB_Result> res( GetCmd().Result() );
            if (res.get()) {
                while (res->Fetch())
                    continue;
            }
        }

        return true;
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "Failed to fetch the results." + GetDbgInfo(), 222011 );
    }
    return false;
}


int CTL_CursorResultExpl::CurrentItemNo() const
{
    return m_CurItemNo;
}


int CTL_CursorResultExpl::GetColumnNum(void) const
{
    return int(m_Fields.size());
}


CDB_Object* CTL_CursorResultExpl::GetItem(CDB_Object* item_buff, I_Result::EGetItem policy)
{
    if (m_CurItemNo >= GetColumnNum() || m_CurItemNo == -1)
        return NULL;

    if (item_buff) {
        EDB_Type type = m_Fields[m_CurItemNo]->GetType();
        if (policy == I_Result::eAppendLOB  &&  (type == eDB_Image  ||  type == eDB_Text)) {
            if (item_buff->GetType() != eDB_Text  &&  item_buff->GetType() != eDB_Image) {
                DATABASE_DRIVER_ERROR( "Wrong type of CDB_Object." + GetDbgInfo(), 130120 );
		    }

            CDB_Stream* ostr  = static_cast<CDB_Stream*>(item_buff);
            CDB_Stream* istr  = static_cast<CDB_Stream*>(m_Fields[m_CurItemNo]);
            istr->MoveTo(0);
            size_t total_size = istr->Size();
            size_t total_read = 0;
            while (total_read < total_size) {
                char buffer[2048];

                size_t read_size = istr->Read(buffer, 2048);
                ostr->Append(buffer, read_size);
                total_read += read_size;
            }
        }
        else {
            item_buff->AssignValue(*m_Fields[m_CurItemNo]);
        }
        delete m_Fields[m_CurItemNo];
    }
    else {
        item_buff = m_Fields[m_CurItemNo];
    }
    m_Fields[m_CurItemNo] = NULL;
    ++m_CurItemNo;

    return item_buff;
}


size_t CTL_CursorResultExpl::ReadItem(void* buffer, size_t buffer_size,
                                      bool* is_null)
{
    if (m_CurItemNo >= GetColumnNum() || m_CurItemNo < 0)
        return 0;

    CDB_Object* field = m_Fields[m_CurItemNo];

    if (field->IsNULL()) {
        if (is_null)
            *is_null = true;

        ++m_CurItemNo;

        return 0;
    }

    if (is_null)
        *is_null = false;

    if (!buffer  ||  buffer_size == 0)
        return 0;


    CS_DATETIME  datetime;
    CS_DATETIME4 datetime4;
    CS_NUMERIC   numeric;

    const void* data = NULL;
    size_t max_size  = 0;

    switch(field->GetType()) {
    case eDB_Int:
        max_size = 4;
        data = static_cast<CDB_Int*>(field)->BindVal();
        break;
    case eDB_SmallInt:
        max_size = 2;
        data = static_cast<CDB_SmallInt*>(field)->BindVal();
        break;
    case eDB_TinyInt:
        max_size = 1;
        data = static_cast<CDB_TinyInt*>(field)->BindVal();
        break;
    case eDB_BigInt:
        max_size = 8;
        data = static_cast<CDB_BigInt*>(field)->BindVal();
        break;
    case eDB_VarChar:
    case eDB_Char: {
        CDB_String* c_field = static_cast<CDB_String*>(field);
        max_size = c_field->Size();
        data = static_cast<const void*>(c_field->Data());
        break;
    }
    case eDB_VarBinary: {
        CDB_VarBinary* vb_field = static_cast<CDB_VarBinary*>(field);
        max_size = vb_field->Size();
        data = static_cast<const void*>(vb_field->Value());
        break;
    }
    case eDB_Binary: {
        CDB_Binary* b_field = static_cast<CDB_Binary*>(field);
        max_size = b_field->Size();
        data = static_cast<const void*>(b_field->Value());
        break;
    }
    case eDB_Float:
        max_size = sizeof(float);
        data = static_cast<CDB_Float*>(field)->BindVal();
        break;
    case eDB_Double:
        max_size = sizeof(double);
        data = static_cast<CDB_Double*>(field)->BindVal();
        break;
    case eDB_DateTime: {
        CDB_DateTime* dt_field = static_cast<CDB_DateTime*>(field);
        datetime.dtdays = dt_field->GetDays();
        datetime.dttime = dt_field->Get300Secs();
        max_size = sizeof(datetime);
        data = &datetime;
        break;
    }
    case eDB_SmallDateTime: {
        CDB_SmallDateTime* dt_field = static_cast<CDB_SmallDateTime*>(field);
        datetime4.days = dt_field->GetDays();
        datetime4.minutes = dt_field->GetMinutes();
        max_size = sizeof(datetime4);
        data = &datetime4;
        break;
    }
    case eDB_Text:
    case eDB_Image: {
        CDB_Stream* str_field = static_cast<CDB_Stream*>(field);
        max_size = str_field->Size();
        if (!m_ReadBuffer) {
            m_ReadBuffer = malloc(max_size);
            str_field->MoveTo(0);
            str_field->Read(m_ReadBuffer, max_size);
        }
        data = m_ReadBuffer;
        break;
    }
    case eDB_Bit:
        max_size = 1;
        data = static_cast<CDB_Bit*>(field)->BindVal();
        break;
    case eDB_Numeric: {
        CDB_Numeric* num_field = static_cast<CDB_Numeric*>(field);
        numeric.scale = num_field->Scale();
        numeric.precision = num_field->Precision();
        memcpy(numeric.array, num_field->RawData(), CS_MAX_NUMLEN);
        max_size = sizeof(numeric);
        data = &numeric;
        break;
    }
    case eDB_LongChar: {
        CDB_LongChar* lc_field = static_cast<CDB_LongChar*>(field);
        max_size = lc_field->DataSize();
        data = static_cast<const void*>(lc_field->Data());
        break;
    }
    case eDB_LongBinary: {
        CDB_LongBinary* lb_field = static_cast<CDB_LongBinary*>(field);
        max_size = lb_field->Size();
        data = static_cast<const void*>(lb_field->Value());
        break;
    }
    default:
        break;
    }

    size_t copied = max_size - m_ReadBytes;
    if (copied > buffer_size)
        copied = buffer_size;

    memcpy(buffer, (const char*)data + m_ReadBytes, copied);
    m_ReadBytes += copied;

    if (m_ReadBytes == max_size) {
        ++m_CurItemNo;
        m_ReadBytes = 0;
        if (m_ReadBuffer) {
            free(m_ReadBuffer);
            m_ReadBuffer = NULL;
        }
    }

    return copied;
}


I_ITDescriptor* CTL_CursorResultExpl::GetImageOrTextDescriptor(int item_num)
{
    if (item_num >= GetColumnNum() || item_num < 0)
        return NULL;

    CTL_ITDescriptor* result = new CTL_ITDescriptor;
    *result = *static_cast<CTL_ITDescriptor*>(m_ITDescrs[item_num]);

    return result;
}


bool CTL_CursorResultExpl::SkipItem()
{
    if (m_CurItemNo >= GetColumnNum() || m_CurItemNo == -1)
        return false;

    ++m_CurItemNo;
    return true;
}


CTL_CursorResultExpl::~CTL_CursorResultExpl()
{
    try {
        delete m_Res;
    }
    NCBI_CATCH_ALL_X( 3, NCBI_CURRENT_FUNCTION )

    ClearFields();
}


#ifdef FTDS_IN_USE
} // namespace NCBI_NS_FTDS_CTLIB
#endif

END_NCBI_SCOPE


