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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *    Driver for SQLite v3 embedded database server
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/sqlite3/interfaces.hpp>

#include "sqlite3_utils.hpp"


BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
static
EDB_Type
s_GetDataType(const char* type)
{
    if (NStr::CompareNocase(type, "INTEGER") == 0) {
        return eDB_Int;
    } else if (NStr::CompareNocase(type, "FLOAT") == 0) {
        return eDB_Double;
    } else if (NStr::CompareNocase(type, "TEXT") == 0) {
        return eDB_Text;
    } else if (NStr::CompareNocase(type, "BLOB") == 0) {
        return eDB_Image;
    }

    return eDB_UnsupportedType;
}

////////////////////////////////////////////////////////////////////////////////
static
size_t
s_GetMaxDataSize(sqlite3_stmt* stmt, unsigned int pos)
{
    size_t max_size = 0;

    switch (sqlite3_column_type(stmt, pos)) {
        case SQLITE_INTEGER:
            max_size = sizeof(int);
            break;
        case SQLITE_FLOAT:
            max_size = sizeof(double);
            break;
        case SQLITE_TEXT:
        case SQLITE_BLOB:
            max_size = 1000000000;
            break;
        default:
            max_size = 0;
    }

    return max_size;
}

////////////////////////////////////////////////////////////////////////////////
/* Shouldn't be used till design changes ...
CSL3_RowResult::CRowInfo::CRowInfo(sqlite3_stmt* stmt)
: m_SQLite3stmt(stmt)
, m_NofCols(sqlite3_column_count(x_GetSQLite3stmt()))
{
}

CSL3_RowResult::CRowInfo::~CRowInfo(void)
{
}

unsigned int
CSL3_RowResult::CRowInfo::GetNum(void) const
{
    return m_NofCols;
}

string
CSL3_RowResult::CRowInfo::GetName(
        const CDBParamVariant& param, 
        CDBParamVariant::ENameFormat format) const
{
    if (param.IsPositional()) {
        unsigned int num = param.GetPosition();
        
        if (num < m_NofCols) {
            return sqlite3_column_name(x_GetSQLite3stmt(), num);
        }
    }

    return NULL;
}


unsigned int CSL3_RowResult::CRowInfo::GetIndex(const CDBParamVariant& param) const
{
    if (param.IsPositional()) {
        return param.GetPosition();
    } else {
        for (unsigned int i = 0; i < GetNum(); ++i) {
            if (param.GetName().compare(sqlite3_column_name(x_GetSQLite3stmt(), i)) == 0) {
                return i;
            }
        }
    }

    DATABASE_DRIVER_ERROR("Parameter name not found.", 1);

    return 0;
}


size_t
CSL3_RowResult::CRowInfo::GetMaxSize(const CDBParamVariant& param) const
{
    if (param.IsPositional()) {
        unsigned int num = param.GetPosition();
        
        if (num < m_NofCols) {
            switch (sqlite3_column_type(x_GetSQLite3stmt(), num)) {
            case SQLITE_INTEGER:
                return sizeof(int);
            case SQLITE_FLOAT:
                return sizeof(double);
            case SQLITE_TEXT:
            case SQLITE_BLOB:
                return 1000000000;
            default:
                return 0;
            }
        }
    }

    return 0;
}

EDB_Type
CSL3_RowResult::CRowInfo::GetDataType(const CDBParamVariant& param) const
{
    if (param.IsPositional()) {
        unsigned int num = param.GetPosition();
        
        if (num < m_NofCols) {
            return s_GetDataType(sqlite3_column_decltype(x_GetSQLite3stmt(), num));
        }
    }

    return eDB_UnsupportedType;
}

CDBParams::EDirection
CSL3_RowResult::CRowInfo::GetDirection(const CDBParamVariant& param) const
{
    return eOut;
}

*/

////////////////////////////////////////////////////////////////////////////////
CSL3_RowResult::CSL3_RowResult(CSL3_LangCmd* cmd, bool fetch_done) :
    m_Cmd(cmd),
    m_CurrItem(-1),
    m_SQLite3stmt(cmd->x_GetSQLite3stmt()),
    m_RC(SQLITE_DONE),
    m_FetchDone(fetch_done)
{
    const unsigned int col_num = sqlite3_column_count(x_GetSQLite3stmt());

    for (unsigned int i = 0; i < col_num; ++i) {
        const char* name = sqlite3_column_name(x_GetSQLite3stmt(), i);
        const string col_name = (name ? name : kEmptyStr);

        m_CachedRowInfo.Add(
                col_name,
                s_GetMaxDataSize(x_GetSQLite3stmt(), i),
                s_GetDataType(sqlite3_column_decltype(x_GetSQLite3stmt(), i))
                );
    }
}


CSL3_RowResult::~CSL3_RowResult()
{
}


EDB_ResType CSL3_RowResult::ResultType() const
{
    return eDB_RowResult;
}

bool CSL3_RowResult::Fetch()
{
    m_CurrItem = 0;

    if (m_FetchDone) {
        m_FetchDone = false;
        m_RC = SQLITE_ROW;
    } else if (m_RC == SQLITE_ROW) {
        m_RC = sqlite3_step(x_GetSQLite3stmt());

        switch (m_RC) {
        case SQLITE_DONE:
        case SQLITE_ROW:
            break;
        case SQLITE_BUSY:
        case SQLITE_ERROR:
        case SQLITE_MISUSE:
            m_CurrItem = -1;
            Check(m_RC);
        default:
            _ASSERT(false);
        }
    }

    return m_RC == SQLITE_ROW;
}


int CSL3_RowResult::CurrentItemNo() const
{
    return m_CurrItem;
}


int CSL3_RowResult::GetColumnNum(void) const
{
    return static_cast<int>(GetDefineParams().GetNum());
}


CDB_Object* CSL3_RowResult::GetItem(CDB_Object* item_buf)
{
    if ((unsigned int) m_CurrItem >= GetDefineParams().GetNum()) {
        ++m_CurrItem;
        return 0;
    }

    int sqlite_type = sqlite3_column_type(x_GetSQLite3stmt(), m_CurrItem);

    if (sqlite_type == SQLITE_NULL) {
        if (!item_buf) {
            switch (GetDefineParams().GetDataType(m_CurrItem)) {
            case eDB_Int:
                item_buf = new CDB_BigInt;
                break;
            case eDB_Double:
                item_buf = new CDB_Double;
                break;
            case eDB_Text:
                item_buf = new CDB_LongChar;
                break;
            case eDB_Image:
                item_buf = new CDB_LongBinary;
                break;
            default:
                _ASSERT(false);
            }
        } else {
            item_buf->AssignNULL();
        }
    } else {
        if (item_buf) {
            switch (item_buf->GetType()) {
            case eDB_Bit:
                *(static_cast<CDB_Bit*>(item_buf)) =
                    sqlite3_column_int(x_GetSQLite3stmt(), m_CurrItem);
                break;
            case eDB_Int:
                *(static_cast<CDB_Int*>(item_buf)) =
                    sqlite3_column_int(x_GetSQLite3stmt(), m_CurrItem);
                break;
            case eDB_SmallInt:
                *(static_cast<CDB_SmallInt*>(item_buf)) =
                    sqlite3_column_int(x_GetSQLite3stmt(), m_CurrItem);
                break;
            case eDB_TinyInt:
                *(static_cast<CDB_TinyInt*>(item_buf)) =
                    sqlite3_column_int(x_GetSQLite3stmt(), m_CurrItem);
                break;
            case eDB_BigInt:
                *(static_cast<CDB_BigInt*>(item_buf)) =
                    sqlite3_column_int64(x_GetSQLite3stmt(), m_CurrItem);
                break;

            //
            case eDB_VarChar:
                static_cast<CDB_VarChar*>(item_buf)->SetValue(
                    reinterpret_cast<const char*>(
                        sqlite3_column_text(x_GetSQLite3stmt(), m_CurrItem)));
                break;
            case eDB_Char:
                static_cast<CDB_Char*>(item_buf)->SetValue(
                    reinterpret_cast<const char*>
                        (sqlite3_column_text(x_GetSQLite3stmt(), m_CurrItem)),
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem));
                break;
            case eDB_LongChar:
                static_cast<CDB_LongChar*>(item_buf)->SetValue(
                    reinterpret_cast<const char*>
                        (sqlite3_column_text(x_GetSQLite3stmt(), m_CurrItem)),
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem));
                break;
            case eDB_Text:
                static_cast<CDB_Text*>(item_buf)->Append(
                    sqlite3_column_text(x_GetSQLite3stmt(), m_CurrItem),
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem));
                break;

            //
            case eDB_VarBinary:
                static_cast<CDB_VarBinary*>(item_buf)->SetValue(
                    sqlite3_column_blob(x_GetSQLite3stmt(), m_CurrItem),
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem));
                break;
            case eDB_Binary:
                static_cast<CDB_Binary*>(item_buf)->SetValue(
                    sqlite3_column_blob(x_GetSQLite3stmt(), m_CurrItem),
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem));
                break;
            case eDB_LongBinary:
                static_cast<CDB_LongBinary*>(item_buf)->SetValue(
                    sqlite3_column_blob(x_GetSQLite3stmt(), m_CurrItem),
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem));
                break;
            case eDB_Image:
                static_cast<CDB_Image*>(item_buf)->Append(
                    sqlite3_column_blob(x_GetSQLite3stmt(), m_CurrItem),
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem));
                break;

            //
            case eDB_Float:
                *(static_cast<CDB_Float*>(item_buf)) = static_cast<float>(
                    sqlite3_column_double(x_GetSQLite3stmt(), m_CurrItem));
                break;
            case eDB_Double:
                *(static_cast<CDB_Double*>(item_buf)) =
                    sqlite3_column_double(x_GetSQLite3stmt(), m_CurrItem);
                break;
            case eDB_Numeric:
                *(static_cast<CDB_Numeric*>(item_buf)) =
                    reinterpret_cast<const char*>(
                        sqlite3_column_text(x_GetSQLite3stmt(), m_CurrItem));
                break;

            //
            case eDB_DateTime:
                *(static_cast<CDB_DateTime*>(item_buf)) = CTime(
                    static_cast<time_t>(
                        sqlite3_column_int(x_GetSQLite3stmt(), m_CurrItem)));
                break;
            case eDB_SmallDateTime:
                *(static_cast<CDB_SmallDateTime*>(item_buf)) = CTime(
                    static_cast<time_t>(
                        sqlite3_column_int(x_GetSQLite3stmt(), m_CurrItem)));

            //
            case eDB_UnsupportedType:
                _ASSERT(false);
            }
        } else {
            switch (sqlite_type) {
            case SQLITE_INTEGER:
                item_buf = new CDB_BigInt(
                    sqlite3_column_int64(x_GetSQLite3stmt(), m_CurrItem));
                break;
            case SQLITE_FLOAT:
                item_buf = new CDB_Double(
                    sqlite3_column_double(x_GetSQLite3stmt(), m_CurrItem));
                break;
            case SQLITE_TEXT: {
                CDB_LongChar* value = new CDB_LongChar(
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem),
                    reinterpret_cast<const char*>(
                        sqlite3_column_text(x_GetSQLite3stmt(), m_CurrItem))
                    );

                item_buf = value;
                break;
            }
            case SQLITE_BLOB: {
                CDB_LongBinary* value = new CDB_LongBinary;

                value->SetValue(
                    sqlite3_column_blob(x_GetSQLite3stmt(), m_CurrItem),
                    sqlite3_column_bytes(x_GetSQLite3stmt(), m_CurrItem)
                    );

                item_buf = value;
                break;
            }
            }
        }
    }

    ++m_CurrItem;
    return item_buf;
}

size_t CSL3_RowResult::ReadItem(void*  /*buffer*/,
                                  size_t /*buffer_size*/,
                                  bool*  /*is_null*/)
{
    return 0;
}


I_ITDescriptor* CSL3_RowResult::GetImageOrTextDescriptor()
{
    return 0;
}


bool CSL3_RowResult::SkipItem()
{
    return false;
}


END_NCBI_SCOPE


