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
 * Author:  Anton Butanayev
 *
 * File Description
 *    Driver for MySQL server
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>

#undef NCBI_DATABASE_THROW
#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), GetConnection(), GetBindParams())
// No use of NCBI_DATABASE_RETHROW or DATABASE_DRIVER_*_EX here.

BEGIN_NCBI_SCOPE


static EDB_Type s_GetDataType(enum_field_types type)
{
    switch ( type ) {
    case FIELD_TYPE_TINY:        return eDB_TinyInt;
    case FIELD_TYPE_SHORT:       return eDB_SmallInt;
    case FIELD_TYPE_LONG:        return eDB_Int;
    case FIELD_TYPE_INT24:       return eDB_Int;
    case FIELD_TYPE_LONGLONG:    return eDB_Int;
    case FIELD_TYPE_DECIMAL:     return eDB_Numeric;
    case FIELD_TYPE_FLOAT:       return eDB_Float;
    case FIELD_TYPE_DOUBLE:      return eDB_Double;
    case FIELD_TYPE_TIMESTAMP:   return eDB_DateTime;
    case FIELD_TYPE_DATE:        return eDB_SmallDateTime;
    case FIELD_TYPE_TIME:        return eDB_UnsupportedType;
    case FIELD_TYPE_DATETIME:    return eDB_DateTime;
    case FIELD_TYPE_YEAR:        return eDB_UnsupportedType;
    case FIELD_TYPE_STRING:
    case FIELD_TYPE_VAR_STRING:  return eDB_VarChar;
    case FIELD_TYPE_BLOB:        return eDB_Image;
    case FIELD_TYPE_SET:         return eDB_UnsupportedType;
    case FIELD_TYPE_ENUM:        return eDB_UnsupportedType;
    case FIELD_TYPE_NULL:        return eDB_UnsupportedType;
    default:
        return eDB_UnsupportedType;
    }
}


CMySQL_RowResult::CMySQL_RowResult(CMySQL_Connection& conn)
    : m_Connect(&conn),
      m_CurrItem(-1)
{
    m_Result = mysql_use_result(&m_Connect->m_MySQL);
    if ( !m_Result ) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_use_result", 800004 );
    }

    unsigned int col_num = mysql_num_fields(m_Result);

    MYSQL_FIELD* fields = mysql_fetch_fields(m_Result);

    for (unsigned int n = 0;  n < col_num;  n++) {
        m_CachedRowInfo.Add(
            fields[n].name,
            fields[n].max_length,
            s_GetDataType(fields[n].type)
            );
    }
}


CMySQL_RowResult::~CMySQL_RowResult()
{
}


EDB_ResType CMySQL_RowResult::ResultType() const
{
    return eDB_RowResult;
}


bool CMySQL_RowResult::Fetch()
{
    m_CurrItem = -1;
    m_Row = mysql_fetch_row(m_Result);
    if ( m_Row ) {
        m_Lengths = mysql_fetch_lengths(m_Result);
        if ( !m_Lengths )
            DATABASE_DRIVER_WARNING( "Failed: mysql_fetch_lengths", 800006 );
    }
    m_CurrItem = 0;
    return m_Row != 0;
}


int CMySQL_RowResult::CurrentItemNo() const
{
    return m_CurrItem;
}


int CMySQL_RowResult::GetColumnNum(void) const
{
    return static_cast<int>(GetDefineParams().GetNum());
}


static CDB_Object* s_GetItem(I_Result::EGetItem policy,
                             EDB_Type    data_type,
                             CDB_Object* item_buff,
                             EDB_Type    b_type,
                             const char* d_ptr,
                             size_t      d_len)
{
    if ( !d_ptr )
        d_ptr = "";

    if (b_type == eDB_VarChar) {
        if ( !item_buff ) {
            item_buff = new CDB_VarChar;
        }
        if ( d_len ) {
            ((CDB_VarChar*) item_buff)->SetValue(d_ptr, d_len);
        } else {
            item_buff->AssignNULL();
        }
        return item_buff;
    }

    if (b_type == eDB_Image) {
        if ( !item_buff ) {
            item_buff = new CDB_Image;
        } else if (policy == I_Result::eAssignLOB) {
            // Explicitly truncate previous value ...
            static_cast<CDB_Image*>(item_buff)->Truncate();
        }

        if ( d_len ) {
            ((CDB_Image*) item_buff)->Append(d_ptr, d_len);
        } else {
            item_buff->AssignNULL();
        }

        return item_buff;
    }

    if (b_type == eDB_Text) {
        if ( !item_buff ) {
            item_buff = new CDB_Text;
        } else if (policy == I_Result::eAssignLOB) {
            // Explicitly truncate previous value ...
            static_cast<CDB_Text*>(item_buff)->Truncate();
        }

        if ( d_len ) {
            ((CDB_Text*) item_buff)->Append(d_ptr, d_len);
        } else {
            item_buff->AssignNULL();
        }

        return item_buff;
    }

    long   int_val = 0;
    Int8   int8_val;
    double double_val;

    switch ( data_type ) {
    case eDB_Bit:
    case eDB_TinyInt:
    case eDB_SmallInt:
    case eDB_Int:
        int_val = NStr::StringToLong(d_ptr);
        break;

    case eDB_BigInt:
        int8_val = NStr::StringToLong(d_ptr);
        break;

    case eDB_Float:
    case eDB_Double:
        double_val = NStr::StringToDouble(d_ptr);
        break;
    default:
        break;
    }

    switch ( b_type ) {
    case eDB_TinyInt: {
        if ( item_buff )
            *((CDB_TinyInt*) item_buff) = Uint1(int_val);
        else
            item_buff = new CDB_TinyInt(Uint1(int_val));
        break;
    }
    case eDB_Bit: {
        if ( item_buff )
            *((CDB_Bit*) item_buff) = int(int_val);
        else
            item_buff = new CDB_Bit(int(int_val));
        break;
    }
    case eDB_SmallInt: {
        if ( item_buff )
            *((CDB_SmallInt*) item_buff) = Int2(int_val);
        else
            item_buff = new CDB_SmallInt(Int2(int_val));
        break;
    }
    case eDB_Int: {
        if ( item_buff )
            *((CDB_Int*) item_buff) = Int4(int_val);
        else
            item_buff = new CDB_Int(Int4(int_val));
        break;
    }
    case eDB_BigInt: {
        if ( item_buff )
            *((CDB_BigInt*) item_buff) = int8_val;
        else
            item_buff = new CDB_BigInt(int8_val);
        break;
    }
    case eDB_Float: {
        if ( item_buff )
            *((CDB_Float*) item_buff) = float(double_val);
        else
            item_buff = new CDB_Float(float(double_val));
        break;
    }
    case eDB_Double: {
        if ( item_buff )
            *((CDB_Double*) item_buff) = double_val;
        else
            item_buff = new CDB_Double(double_val);
        break;
    }
    case eDB_DateTime: {
        CTime time;
        if ( d_len )
            time = CTime(d_ptr, "Y-M-D h:m:s");

        if ( item_buff )
            *(CDB_DateTime*) item_buff = time;
        else
            item_buff = new CDB_DateTime(time);
        break;
    }
    case eDB_SmallDateTime: {
        CTime time;
        if (d_len)
            time = CTime(d_ptr, "Y-M-D");

        if (item_buff)
            *(CDB_SmallDateTime*) item_buff = time;
        else
            item_buff = new CDB_SmallDateTime(time);
        break;
    }
    default:
        break;
    }

    if (d_len == 0  &&  item_buff) {
        item_buff->AssignNULL();
    }

    return item_buff;
}


CDB_Object* CMySQL_RowResult::GetItem(CDB_Object* item_buf, I_Result::EGetItem policy)
{
    if ((unsigned int) m_CurrItem >= GetDefineParams().GetNum()) {
        return 0;
    }

    CDB_Object* r =
        s_GetItem(policy, GetDefineParams().GetDataType(m_CurrItem), item_buf,
                  item_buf ? item_buf->GetType() : eDB_UnsupportedType,
                  m_Row[m_CurrItem], m_Lengths[m_CurrItem]);
    ++m_CurrItem;
    return r;
}

size_t CMySQL_RowResult::ReadItem(void*  /*buffer*/,
                                  size_t /*buffer_size*/,
                                  bool*  /*is_null*/)
{
    return 0;
}


I_ITDescriptor* CMySQL_RowResult::GetImageOrTextDescriptor()
{
    return 0;
}


bool CMySQL_RowResult::SkipItem()
{
    return false;
}


END_NCBI_SCOPE


