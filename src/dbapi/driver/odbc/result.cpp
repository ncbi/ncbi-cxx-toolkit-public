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
 * File Description:  DBLib Results
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////

static EDB_Type s_GetDataType(SQLSMALLINT t, SQLSMALLINT dec_digits,
                              SQLUINTEGER prec)
{
    switch (t) {
    case SQL_WCHAR:
    case SQL_CHAR:         return (prec < 256)? eDB_Char : eDB_LongChar;
    case SQL_WVARCHAR:
    case SQL_VARCHAR:      return (prec < 256)? eDB_VarChar : eDB_LongChar;
    case SQL_LONGVARCHAR:  return eDB_Text;
    case SQL_LONGVARBINARY:
    case SQL_WLONGVARCHAR: return eDB_Image;
    case SQL_DECIMAL:
    case SQL_NUMERIC:      if(prec > 20 || dec_digits > 0) return eDB_Numeric;
    case SQL_BIGINT:       return eDB_BigInt;
    case SQL_SMALLINT:     return eDB_SmallInt;
    case SQL_INTEGER:      return eDB_Int;
    case SQL_REAL:         return eDB_Float;
    case SQL_FLOAT:        return eDB_Double;
    case SQL_BINARY:       return (prec < 256)? eDB_Binary : eDB_LongBinary;
    case SQL_BIT:          return eDB_Bit;
    case SQL_TINYINT:      return eDB_TinyInt;
    case SQL_VARBINARY:    return (prec < 256)? eDB_VarBinary : eDB_LongBinary;
    case SQL_TYPE_TIMESTAMP:
        return (prec > 16 || dec_digits > 0)? eDB_DateTime : eDB_SmallDateTime;
    default:               return eDB_UnsupportedType;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RowResult::
//


CODBC_RowResult::CODBC_RowResult(SQLSMALLINT nof_cols, SQLHSTMT cmd,
                                 CODBC_Reporter& r) :
    m_Cmd(cmd), m_Reporter(r), m_CurrItem(-1), m_EOR(false)
{
    m_NofCols = nof_cols;

    SQLSMALLINT actual_name_size, nullable;

    m_ColFmt = new SODBC_ColDescr[m_NofCols];
    for (unsigned int n = 0; n < m_NofCols; n++) {
        switch(SQLDescribeCol(m_Cmd, n+1, m_ColFmt[n].ColumnName,
                              ODBC_COLUMN_NAME_SIZE, &actual_name_size,
                              &m_ColFmt[n].DataType, &m_ColFmt[n].ColumnSize,
                              &m_ColFmt[n].DecimalDigits, &nullable)) {
        case SQL_SUCCESS_WITH_INFO:
            m_Reporter.ReportErrors();
        case SQL_SUCCESS:
            continue;
        case SQL_ERROR:
            m_Reporter.ReportErrors();
            throw CDB_ClientEx(eDB_Error, 420020, "CODBC_RowResult::CODBC_RowResult",
                               "SQLDescribeCol failed");
        default:
            throw CDB_ClientEx(eDB_Error, 420021, "CODBC_RowResult::CODBC_RowResult",
                               "SQLDescribeCol failed (memory corruption suspected)");
	   }
	}
}


EDB_ResType CODBC_RowResult::ResultType() const
{
    return eDB_RowResult;
}


unsigned int CODBC_RowResult::NofItems() const
{
    return m_NofCols;
}


const char* CODBC_RowResult::ItemName(unsigned int item_num) const
{
    return item_num < m_NofCols ? (char*)m_ColFmt[item_num].ColumnName : 0;
}


size_t CODBC_RowResult::ItemMaxSize(unsigned int item_num) const
{
    return item_num < m_NofCols ? m_ColFmt[item_num].ColumnSize : 0;
}


EDB_Type CODBC_RowResult::ItemDataType(unsigned int item_num) const
{
    return item_num < m_NofCols ?
        s_GetDataType(m_ColFmt[item_num].DataType, m_ColFmt[item_num].DecimalDigits,
			m_ColFmt[item_num].ColumnSize) :  eDB_UnsupportedType;
}


bool CODBC_RowResult::Fetch()
{
    m_CurrItem= -1;
    if (!m_EOR) {
        switch (SQLFetch(m_Cmd)) {
        case SQL_SUCCESS_WITH_INFO:
            m_Reporter.ReportErrors();
        case SQL_SUCCESS:
            m_CurrItem = 0;
            return true;
        case SQL_NO_DATA:
            m_EOR = true;
            break;
        case SQL_ERROR:
            m_Reporter.ReportErrors();
            throw CDB_ClientEx(eDB_Error, 430003, "CODBC_RowResult::Fetch",
                               "SQLFetch failed");
        default:
            throw CDB_ClientEx(eDB_Error, 430004, "CODBC_RowResult::Fetch",
                               "SQLFetch failed (memory corruption suspected)");
        }
    }
    return false;
}


int CODBC_RowResult::CurrentItemNo() const
{
    return m_CurrItem;
}

int CODBC_RowResult::xGetData(SQLSMALLINT target_type, SQLPOINTER buffer,
                              SQLINTEGER buffer_size)
{
    SQLINTEGER f;

    switch(SQLGetData(m_Cmd, m_CurrItem+1, target_type, buffer, buffer_size, &f)) {
    case SQL_SUCCESS_WITH_INFO:
        switch(f) {
        case SQL_NO_TOTAL:
            return buffer_size;
        case SQL_NULL_DATA:
            return 0;
        default:
			if(f < 0)
				m_Reporter.ReportErrors();
            return (int)f;
        }
    case SQL_SUCCESS:
        if(target_type==SQL_C_CHAR) buffer_size--;
        return (f > buffer_size)? buffer_size : (int)f;
    case SQL_NO_DATA:
        return 0;
    case SQL_ERROR:
        m_Reporter.ReportErrors();
    default:
        throw CDB_ClientEx(eDB_Error, 430027, "CODBC_*Result::xGetData",
                                   "SQLGetData failed ");
    }
}

static void xConvert2CDB_Numeric(CDB_Numeric* d, SQL_NUMERIC_STRUCT& s)
{
    swap_numeric_endian((unsigned int)s.precision, s.val);
    d->Assign((unsigned int)s.precision, (unsigned int)s.scale,
             s.sign == 0, s.val);
}

CDB_Object* CODBC_RowResult::xLoadItem(CDB_Object* item_buf)
{
    char buffer[8*1024];
    int outlen;

    switch(m_ColFmt[m_CurrItem].DataType) {
    case SQL_WCHAR:
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_WVARCHAR: {
        switch (item_buf->GetType()) {
        case eDB_VarBinary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_VarBinary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_Binary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_Binary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_LongBinary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_LongBinary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_VarChar:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_VarChar*)  item_buf) = buffer;
            break;
        case eDB_Char:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Char*)     item_buf) = buffer;
            break;
        case eDB_LongChar:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_LongChar*)     item_buf) = buffer;
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_BINARY:
    case SQL_VARBINARY: {
        switch ( item_buf->GetType() ) {
        case eDB_VarBinary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_VarBinary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_Binary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_Binary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_LongBinary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_LongBinary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_VarChar:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_VarChar*)  item_buf) = buffer;
            break;
        case eDB_Char:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Char*) item_buf) = buffer;
            break;
        case eDB_LongChar:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_LongChar*) item_buf) = buffer;
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }

        break;
    }

    case SQL_BIT: {
        SQLCHAR v;
        switch (  item_buf->GetType()  ) {
        case eDB_Bit:
            outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Bit*) item_buf) = (int) v;
            break;
        case eDB_TinyInt:
            outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_TinyInt*)  item_buf) = v ? 1 : 0;
            break;
        case eDB_SmallInt:
            outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = v ? 1 : 0;
            break;
        case eDB_Int:
            outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*)      item_buf) = v ? 1 : 0;
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT v;
        switch ( item_buf->GetType() ) {
        case eDB_SmallDateTime: {
            outlen= xGetData(SQL_C_TYPE_TIMESTAMP, &v, sizeof(SQL_TIMESTAMP_STRUCT));
            if (outlen <= 0) item_buf->AssignNULL();
            else {
                CTime t((int)v.year, (int)v.month, (int)v.day,
                        (int)v.hour, (int)v.minute, (int)v.second,
                        (long)v.fraction);

                *((CDB_SmallDateTime*) item_buf)= t;
            }
            break;
        }
        case eDB_DateTime: {
            outlen= xGetData(SQL_C_TYPE_TIMESTAMP, &v, sizeof(SQL_TIMESTAMP_STRUCT));
            if (outlen <= 0) item_buf->AssignNULL();
            else {
                CTime t((int)v.year, (int)v.month, (int)v.day,
                        (int)v.hour, (int)v.minute, (int)v.second,
                        (long)v.fraction);

                *((CDB_DateTime*) item_buf)= t;
            }
            break;
        }
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_TINYINT: {
        SQLCHAR v;
        switch (  item_buf->GetType()  ) {
        case eDB_TinyInt:
            outlen= xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_TinyInt*)  item_buf) = (Uint1) v;
            break;
        case eDB_SmallInt:
            outlen= xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = (Int2) v;
            break;
        case eDB_Int:
            outlen= xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*)      item_buf) = (Int4) v;
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_SMALLINT: {
        SQLSMALLINT v;
        switch (  item_buf->GetType()  ) {
        case eDB_SmallInt:
            outlen= xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = (Int2) v;
            break;
        case eDB_Int:
            outlen= xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*) item_buf) = (Int4) v;
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_INTEGER: {
        SQLINTEGER v;
        switch (  item_buf->GetType()  ) {
        case eDB_Int:
            outlen= xGetData(SQL_C_SLONG, &v, sizeof(SQLINTEGER));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*) item_buf) = (Int4) v;
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_DOUBLE:
    case SQL_FLOAT: {
        SQLDOUBLE v;
        switch (  item_buf->GetType()  ) {
        case eDB_Double:
            outlen= xGetData(SQL_C_DOUBLE, &v, sizeof(SQLDOUBLE));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Double*)      item_buf) = v;
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_REAL: {
        SQLREAL v;
        switch (  item_buf->GetType()  ) {
        case eDB_Float:
            outlen= xGetData(SQL_C_FLOAT, &v, sizeof(SQLREAL));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Float*)      item_buf) = v;
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_BIGINT:
    case SQL_DECIMAL:
    case SQL_NUMERIC: {
        switch (  item_buf->GetType()  ) {
        case eDB_Numeric: {
            SQL_NUMERIC_STRUCT v;
			SQLHDESC hdesc;
			SQLGetStmtAttr(m_Cmd, SQL_ATTR_APP_ROW_DESC,&hdesc, 0, NULL);
			SQLSetDescField(hdesc,m_CurrItem+1,SQL_DESC_TYPE,(VOID*)SQL_C_NUMERIC,0);
			SQLSetDescField(hdesc,m_CurrItem+1,SQL_DESC_PRECISION,
					(VOID*)(m_ColFmt[m_CurrItem].ColumnSize),0);
			SQLSetDescField(hdesc,m_CurrItem+1,SQL_DESC_SCALE,
					(VOID*)(m_ColFmt[m_CurrItem].DecimalDigits),0);

            outlen= xGetData(SQL_ARD_TYPE, &v, sizeof(SQL_NUMERIC_STRUCT));
            if (outlen <= 0) item_buf->AssignNULL();
            else xConvert2CDB_Numeric((CDB_Numeric*)item_buf, v);
            break;
        }
        case eDB_BigInt: {
            SQLBIGINT v;
            outlen= xGetData(SQL_C_SBIGINT, &v, sizeof(SQLBIGINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_BigInt*) item_buf) = (Int8) v;
            break;
        }
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }

    case SQL_WLONGVARCHAR:
    case SQL_LONGVARBINARY:
    case SQL_LONGVARCHAR: {
        SQLINTEGER f;
        switch(item_buf->GetType()) {
        case eDB_Text: {
            CDB_Stream* val = (CDB_Stream*) item_buf;
            for(;;) {
                switch(SQLGetData(m_Cmd, m_CurrItem+1, SQL_C_CHAR, buffer, sizeof(buffer), &f)) {
                case SQL_SUCCESS_WITH_INFO:
                    if(f == SQL_NO_TOTAL) f= sizeof(buffer) - 1;
                    else if(f < 0) m_Reporter.ReportErrors();
                case SQL_SUCCESS:
                    if(f > 0) {
                        if(f > sizeof(buffer)-1) f= sizeof(buffer)-1;
                        val->Append(buffer, f);
                    }
                    continue;
                case SQL_NO_DATA:
                    break;
                case SQL_ERROR:
                    m_Reporter.ReportErrors();
                default:
                    throw CDB_ClientEx(eDB_Error, 430021, "CODBC_*Result::GetItem",
                               "SQLGetData failed while retrieving text/image into CDB_Text");
                }
				break;
            }
            break;
        }
        case eDB_Image: {
            CDB_Stream* val = (CDB_Stream*) item_buf;
            for(;;) {
                switch(SQLGetData(m_Cmd, m_CurrItem+1, SQL_C_BINARY, buffer, sizeof(buffer), &f)) {
                case SQL_SUCCESS_WITH_INFO:
                    if(f == SQL_NO_TOTAL || f > sizeof(buffer)) f= sizeof(buffer);
                    else m_Reporter.ReportErrors();
                case SQL_SUCCESS:
                    if(f > 0) {
                        if(f > sizeof(buffer)) f= sizeof(buffer);
                        val->Append(buffer, f);
                    }
                    continue;
                case SQL_NO_DATA:
                    break;
                case SQL_ERROR:
                    m_Reporter.ReportErrors();
                default:
                    throw CDB_ClientEx(eDB_Error, 430022, "CODBC_*Result::GetItem",
                               "SQLGetData failed while retrieving text/image into CDB_Image");
                }
				break;
            }
            break;
        }
        default:
            throw CDB_ClientEx(eDB_Error, 430020, "CODBC_*Result::GetItem",
                               "Wrong type of CDB_Object");
        }
        break;
    }
    default:
        throw CDB_ClientEx(eDB_Error, 430025, "CODBC_*Result::GetItem",
                               "Unsupported column type");

    }
    return item_buf;
}

CDB_Object* CODBC_RowResult::xMakeItem()
{
    char buffer[8*1024];
    int outlen;

    switch(m_ColFmt[m_CurrItem].DataType) {
    case SQL_WCHAR:
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_WVARCHAR: {
        outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
		if(m_ColFmt[m_CurrItem].ColumnSize < 256) {
			CDB_VarChar* val = (outlen < 0)
				? new CDB_VarChar() : new CDB_VarChar(buffer, (size_t) outlen);

			return val;
		}
		else {
			CDB_LongChar* val = (outlen < 0)
				? new CDB_LongChar(m_ColFmt[m_CurrItem].ColumnSize) :
				new CDB_LongChar(m_ColFmt[m_CurrItem].ColumnSize,
						buffer);

			return val;
		}

    }

    case SQL_BINARY:
    case SQL_VARBINARY: {
        outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
		if(m_ColFmt[m_CurrItem].ColumnSize < 256) {
	        CDB_VarBinary* val = (outlen <= 0)
		        ? new CDB_VarBinary() : new CDB_VarBinary(buffer, (size_t)outlen);

			return val;
		}
		else {
			CDB_LongBinary* val = (outlen < 0)
				? new CDB_LongBinary(m_ColFmt[m_CurrItem].ColumnSize) :
				new CDB_LongBinary(m_ColFmt[m_CurrItem].ColumnSize,
						buffer, (size_t) outlen);

			return val;
		}
    }

    case SQL_BIT: {
        SQLCHAR v;
        outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
        return (outlen <= 0) ? new CDB_Bit() : new CDB_Bit((int) v);
    }

    case SQL_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT v;
        outlen= xGetData(SQL_C_TYPE_TIMESTAMP, &v, sizeof(SQL_TIMESTAMP_STRUCT));
        if (outlen <= 0) {
            return new CDB_SmallInt();
        }
        else {
            CTime t((int)v.year, (int)v.month, (int)v.day,
                    (int)v.hour, (int)v.minute, (int)v.second,
                    (long)v.fraction);
            return (m_ColFmt[m_CurrItem].ColumnSize > 16 ||
				m_ColFmt[m_CurrItem].DecimalDigits > 0)? (CDB_Object*)(new CDB_DateTime(t)) :
				(CDB_Object*)(new CDB_SmallDateTime(t));
        }
    }

    case SQL_TINYINT: {
        SQLCHAR v;
        outlen= xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
        return (outlen <= 0) ? new CDB_TinyInt() : new CDB_TinyInt((Uint1) v);
    }

    case SQL_SMALLINT: {
        SQLSMALLINT v;
        outlen= xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
        return (outlen <= 0) ? new CDB_SmallInt() : new CDB_SmallInt((Int2) v);
    }

    case SQL_INTEGER: {
        SQLINTEGER v;
        outlen= xGetData(SQL_C_SLONG, &v, sizeof(SQLINTEGER));
        return (outlen <= 0) ? new CDB_Int() : new CDB_Int((Int4) v);
    }

    case SQL_DOUBLE:
    case SQL_FLOAT: {
        SQLDOUBLE v;
        outlen= xGetData(SQL_C_DOUBLE, &v, sizeof(SQLDOUBLE));
        return (outlen <= 0) ? new CDB_Double() : new CDB_Double(v);
    }
    case SQL_REAL: {
        SQLREAL v;
        outlen= xGetData(SQL_C_FLOAT, &v, sizeof(SQLREAL));
        return (outlen <= 0) ? new CDB_Float() : new CDB_Float(v);
    }

    case SQL_DECIMAL:
    case SQL_NUMERIC: {
        if((m_ColFmt[m_CurrItem].DecimalDigits > 0) ||
           (m_ColFmt[m_CurrItem].ColumnSize > 20)) { // It should be numeric
            SQL_NUMERIC_STRUCT v;
            outlen= xGetData(SQL_C_NUMERIC, &v, sizeof(SQL_NUMERIC_STRUCT));
            CDB_Numeric* r= new CDB_Numeric;
            if(outlen > 0) {
                xConvert2CDB_Numeric(r, v);
            }
                return r;
        }
        else { // It should be bigint
            SQLBIGINT v;
            outlen= xGetData(SQL_C_SBIGINT, &v, sizeof(SQLBIGINT));
            return (outlen <= 0) ? new CDB_BigInt() : new CDB_BigInt((Int8) v);
        }
    }

    case SQL_WLONGVARCHAR:
    case SQL_LONGVARCHAR: {
        CDB_Text* val = new CDB_Text;
        SQLINTEGER f;

        for(;;) {
            switch(SQLGetData(m_Cmd, m_CurrItem+1, SQL_C_CHAR, buffer, sizeof(buffer), &f)) {
            case SQL_SUCCESS_WITH_INFO:
                if(f == SQL_NO_TOTAL) f= sizeof(buffer) - 1;
                else if(f < 0) m_Reporter.ReportErrors();
            case SQL_SUCCESS:
                if(f > 0) {
                    if(f > sizeof(buffer)-1) f= sizeof(buffer)-1;
                    val->Append(buffer, f);
                }
                continue;
            case SQL_NO_DATA:
                break;
            case SQL_ERROR:
                m_Reporter.ReportErrors();
            default:
                throw CDB_ClientEx(eDB_Error, 430023, "CODBC_*Result::GetItem",
                               "SQLGetData failed while retrieving text into CDB_Text");
            }
        }
        return val;
    }

    case SQL_LONGVARBINARY: {
        CDB_Image* val = new CDB_Image;
        SQLINTEGER f;
        for(;;) {
            switch(SQLGetData(m_Cmd, m_CurrItem+1, SQL_C_BINARY, buffer, sizeof(buffer), &f)) {
            case SQL_SUCCESS_WITH_INFO:
                if(f == SQL_NO_TOTAL) f= sizeof(buffer);
                else if(f < 0) m_Reporter.ReportErrors();
            case SQL_SUCCESS:
                if(f > 0) {
                    if(f > sizeof(buffer)) f= sizeof(buffer);
                    val->Append(buffer, f);
                }
                continue;
            case SQL_NO_DATA:
                break;
            case SQL_ERROR:
                m_Reporter.ReportErrors();
            default:
                throw CDB_ClientEx(eDB_Error, 430024, "CODBC_*Result::GetItem",
                                   "SQLGetData failed while retrieving image into CDB_Image");
            }
        }
        return val;
    }
    default:
        throw CDB_ClientEx(eDB_Error, 430025, "CODBC_*Result::GetItem",
                               "Unsupported column type");

    }
}


CDB_Object* CODBC_RowResult::GetItem(CDB_Object* item_buf)
{
    if ((unsigned int) m_CurrItem >= m_NofCols  ||  m_CurrItem == -1) {
        return 0;
    }

    CDB_Object* item = item_buf? xLoadItem(item_buf) : xMakeItem();

    ++m_CurrItem;
    return item;
}


size_t CODBC_RowResult::ReadItem(void* buffer,size_t buffer_size,bool* is_null)
{
    if ((unsigned int) m_CurrItem >= m_NofCols  ||  m_CurrItem == -1 ||
        buffer == 0 || buffer_size == 0) {
        return 0;
    }

    SQLINTEGER f= 0;

    if(is_null) *is_null= false;

    switch(SQLGetData(m_Cmd, m_CurrItem+1, SQL_C_BINARY, buffer, buffer_size, &f)) {
    case SQL_SUCCESS_WITH_INFO:
        switch(f) {
        case SQL_NO_TOTAL:
            return buffer_size;
        case SQL_NULL_DATA:
            ++m_CurrItem;
            if(is_null) *is_null= true;
            return 0;
        default:
			   if(f >= 0) return (f <= buffer_size)? (size_t)f : buffer_size;
            m_Reporter.ReportErrors();
            return 0;
        }
    case SQL_SUCCESS:
        ++m_CurrItem;
        if(f == SQL_NULL_DATA) {
            if(is_null) *is_null= true;
            return 0;
        }
        return (f >= 0)? ((size_t)f) : 0;
    case SQL_NO_DATA:
        ++m_CurrItem;
        if(f == SQL_NULL_DATA) {
            if(is_null) *is_null= true;
        }
        return 0;
    case SQL_ERROR:
        m_Reporter.ReportErrors();
    default:
        throw CDB_ClientEx(eDB_Error, 430026, "CODBC_*Result::ReadItem",
                                   "SQLGetData failed ");
    }
}


CDB_ITDescriptor* CODBC_RowResult::GetImageOrTextDescriptor(int item_no,
                                                            const string& cond)
{
    char* buffer[128];
    SQLSMALLINT slp;

    switch(SQLColAttribute(m_Cmd, item_no+1,
                           SQL_DESC_BASE_TABLE_NAME,
                           (SQLPOINTER)buffer, sizeof(buffer),
                           &slp, 0)) {
    case SQL_SUCCESS_WITH_INFO:
        m_Reporter.ReportErrors();
    case SQL_SUCCESS:
        break;
    case SQL_ERROR:
        m_Reporter.ReportErrors();
        return 0;
    default:
        throw CDB_ClientEx(eDB_Error, 430027,
                           "CODBC_*Result::GetImageOrTextDescriptor",
                           "SQLColAttribute failed");
    }
    string base_table=(const char*)buffer;

    switch(SQLColAttribute(m_Cmd, item_no+1,
                           SQL_DESC_BASE_COLUMN_NAME,
                           (SQLPOINTER)buffer, sizeof(buffer),
                           &slp, 0)) {
    case SQL_SUCCESS_WITH_INFO:
        m_Reporter.ReportErrors();
    case SQL_SUCCESS:
        break;
    case SQL_ERROR:
        m_Reporter.ReportErrors();
        return 0;
    default:
        throw CDB_ClientEx(eDB_Error, 430027,
                           "CODBC_*Result::GetImageOrTextDescriptor",
                           "SQLColAttribute failed");
    }
    string base_column=(const char*)buffer;

    return new CDB_ITDescriptor(base_table, base_column, cond);
}

I_ITDescriptor* CODBC_RowResult::GetImageOrTextDescriptor()
{
    return (I_ITDescriptor*) GetImageOrTextDescriptor(m_CurrItem,
                                                      "don't use me");
}

bool CODBC_RowResult::SkipItem()
{
    if ((unsigned int) m_CurrItem < m_NofCols) {
        ++m_CurrItem;
        return true;
    }
    return false;
}


CODBC_RowResult::~CODBC_RowResult()
{
    if (m_ColFmt) {
        delete[] m_ColFmt;
        m_ColFmt = 0;
    }
    if (!m_EOR)
        SQLFreeStmt(m_Cmd, SQL_CLOSE);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ParamResult::
//  CTL_StatusResult::
//  CTL_CursorResult::
//

EDB_ResType CODBC_StatusResult::ResultType() const
{
    return eDB_StatusResult;
}

CODBC_StatusResult::~CODBC_StatusResult()
{
}

EDB_ResType CODBC_ParamResult::ResultType() const
{
    return eDB_ParamResult;
}

CODBC_ParamResult::~CODBC_ParamResult()
{
}

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorResult::
//

CODBC_CursorResult::CODBC_CursorResult(CODBC_LangCmd* cmd) :
    m_Cmd(cmd), m_Res(0)
{
    try {
        m_Cmd->Send();
		m_EOR= true;
        while (m_Cmd->HasMoreResults()) {
            m_Res = m_Cmd->Result();
            if (m_Res && m_Res->ResultType() == eDB_RowResult) {
				m_EOR= false;
                return;
            }
            if (m_Res) {
                while (m_Res->Fetch())
                    ;
                delete m_Res;
                m_Res = 0;
            }
        }
    } catch (CDB_Exception& ) {
        throw CDB_ClientEx(eDB_Error, 422010,
                           "CODBC_CursorResult::CODBC_CursorResult",
                           "failed to get the results");
    }
}


EDB_ResType CODBC_CursorResult::ResultType() const
{
    return eDB_CursorResult;
}


unsigned int CODBC_CursorResult::NofItems() const
{
    return m_Res? m_Res->NofItems() : 0;
}


const char* CODBC_CursorResult::ItemName(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemName(item_num) : 0;
}


size_t CODBC_CursorResult::ItemMaxSize(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemMaxSize(item_num) : 0;
}


EDB_Type CODBC_CursorResult::ItemDataType(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemDataType(item_num) : eDB_UnsupportedType;
}


bool CODBC_CursorResult::Fetch()
{

	if(m_EOR) return false;
	try {
		if (m_Res && m_Res->Fetch()) return true;
    } catch (CDB_Exception& ) {
		delete m_Res;
		m_Res= 0;
	}

    try {
        // finish this command
		m_EOR= true;
        if(m_Res) {
			delete m_Res;
			while (m_Cmd->HasMoreResults()) {
				m_Res = m_Cmd->Result();
				if (m_Res) {
					while (m_Res->Fetch())
						;
					delete m_Res;
					m_Res = 0;
				}
			}
		}
        // send the another "fetch cursor_name" command
        m_Cmd->Send();
        while (m_Cmd->HasMoreResults()) {
            m_Res = m_Cmd->Result();
            if (m_Res && m_Res->ResultType() == eDB_RowResult) {
				m_EOR= false;
                return m_Res->Fetch();
            }
            if (m_Res) {
                while (m_Res->Fetch())
                    ;
                delete m_Res;
                m_Res = 0;
            }
        }
    } catch (CDB_Exception& ) {
        throw CDB_ClientEx(eDB_Error, 422011, "CODBC_CursorResult::Fetch",
                           "Failed to fetch the results");
    }
    return false;
}


int CODBC_CursorResult::CurrentItemNo() const
{
    return m_Res ? m_Res->CurrentItemNo() : -1;
}


CDB_Object* CODBC_CursorResult::GetItem(CDB_Object* item_buff)
{
    return m_Res ? m_Res->GetItem(item_buff) : 0;
}


size_t CODBC_CursorResult::ReadItem(void* buffer, size_t buffer_size,
                                   bool* is_null)
{
    if (m_Res) {
        return m_Res->ReadItem(buffer, buffer_size, is_null);
    }
    if (is_null)
        *is_null = true;
    return 0;
}


I_ITDescriptor* CODBC_CursorResult::GetImageOrTextDescriptor()
{
    return m_Res ? m_Res->GetImageOrTextDescriptor() : 0;
}


bool CODBC_CursorResult::SkipItem()
{
    return m_Res ? m_Res->SkipItem() : false;
}


CODBC_CursorResult::~CODBC_CursorResult()
{
    if (m_Res)
        delete m_Res;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/01/31 21:48:27  soussov
 * fixes bug in xMakeItem. It should return CDB_SmallDateTime if DecimalDigits == 0
 *
 * Revision 1.12  2004/05/17 21:16:06  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.11  2004/01/30 20:00:39  soussov
 * fixes GetItem for CDB_Numeric
 *
 * Revision 1.10  2003/12/09 17:35:29  sapojnik
 * data sizes returned by SQLGetData() adjusted to fit within the buffer
 *
 * Revision 1.9  2003/12/09 15:42:15  sapojnik
 * CODBC_RowResult::ReadItem(): * was missing in *is_null=false; corrected
 *
 * Revision 1.8  2003/11/25 20:09:06  soussov
 * fixes bug in ReadItem: it did return the text/image size instead of number of bytes in buffer in some cases
 *
 * Revision 1.7  2003/05/08 20:30:24  soussov
 * CDB_LongChar CDB_LongBinary added
 *
 * Revision 1.6  2003/01/31 16:51:03  lavr
 * Remove unused variable "e" from catch() clause
 *
 * Revision 1.5  2003/01/06 16:59:20  soussov
 * sets m_CurrItem = -1 for all result types if no fetch was called
 *
 * Revision 1.4  2003/01/03 21:48:37  soussov
 * set m_CurrItem = -1 if fetch failes
 *
 * Revision 1.3  2003/01/02 21:05:35  soussov
 * SQL_BIGINT added in CODBC_RowResult::xLoadItem
 *
 * Revision 1.2  2002/07/05 15:15:10  soussov
 * fixes bug in ReadItem
 *
 *
 * ===========================================================================
 */
