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
 * Author:  Pavel Ivanov
 *
 */

#include <ncbi_pch.hpp>

#include <connect/ncbi_core_cxx.hpp>

#include <dbapi/driver/dbapi_driver_conn_params.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <dbapi/driver/drivers.hpp>

#include "sdbapi_impl.hpp"

BEGIN_NCBI_SCOPE


#define SDBAPI_CATCH_LOWLEVEL()                             \
    catch (CDB_Exception& ex) {                             \
        NCBI_RETHROW(ex, CSDB_Exception, eLowLevel, "");    \
    }



const char*
CSDB_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eEmptyDBName:  return "eEmptyDBName";
    case eClosed:       return "eClosed";
    case eStarted:      return "eStarted";
    case eNotInOrder:   return "eNotInOrder";
    case eInconsistent: return "eInconsistent";
    case eUnsupported:  return "eUnsupported";
    case eNotExist:     return "eNotExist";
    case eOutOfBounds:  return "eOutOfBounds";
    case eLowLevel:     return "eLowLevel";
    default:            return CException::GetErrCodeString();
    }
}


static EDB_Type
s_ConvertType(ESDB_Type type)
{
    switch (type)
    {
    case eSDB_Byte:
        return eDB_TinyInt;
    case eSDB_Short:
        return eDB_SmallInt;
    case eSDB_Int4:
        return eDB_Int;
    case eSDB_Int8:
        return eDB_BigInt;
    case eSDB_Float:
        return eDB_Float;
    case eSDB_Double:
        return eDB_Double;
    case eSDB_String:
        return eDB_VarChar;
    case eSDB_Binary:
        return eDB_VarBinary;
    case eSDB_DateTime:
        return eDB_DateTime;
    case eSDB_Text:
        return eDB_Text;
    case eSDB_Image:
        return eDB_Image;
    }
    return eDB_UnsupportedType;
}

static ESDB_Type
s_ConvertType(EDB_Type type)
{
    switch (type) {
    case eDB_Int:
        return eSDB_Int4;
    case eDB_SmallInt:
        return eSDB_Short;
    case eDB_TinyInt:
    case eDB_Bit:
        return eSDB_Byte;
    case eDB_BigInt:
        return eSDB_Int8;
    case eDB_Numeric:
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        return eSDB_String;
    case eDB_VarBinary:
    case eDB_Binary:
    case eDB_LongBinary:
        return eSDB_Binary;
    case eDB_Float:
        return eSDB_Float;
    case eDB_Double:
        return eSDB_Double;
    case eDB_DateTime:
    case eDB_SmallDateTime:
        return eSDB_DateTime;
    case eDB_Text:
        return eSDB_Text;
    case eDB_Image:
        return eSDB_Image;
    case eDB_UnsupportedType:
        break;
    }
    _ASSERT(false);
    return eSDB_String;
}

static IBulkInsert::EHints
s_ConvertHints(CBulkInsert::EHintsWithValue hints)
{
    switch (hints) {
    case CBulkInsert::eRowsPerBatch:      return IBulkInsert::eRowsPerBatch;
    case CBulkInsert::eKilobytesPerBatch: return IBulkInsert::eKilobytesPerBatch;
    }
    _ASSERT(false);
    return IBulkInsert::EHints(hints);
}

static IBulkInsert::EHints
s_ConvertHints(CBulkInsert::EHints hints)
{
    switch (hints) {
    case CBulkInsert::eTabLock:          return IBulkInsert::eTabLock;
    case CBulkInsert::eCheckConstraints: return IBulkInsert::eCheckConstraints;
    case CBulkInsert::eFireTriggers:     return IBulkInsert::eFireTriggers;
    }
    _ASSERT(false);
    return IBulkInsert::EHints(hints);
}

static void
s_ConvertionNotSupported(const char* one_type, EDB_Type other_type)
{
    NCBI_THROW(CSDB_Exception, eUnsupported,
               "Conversion between " + string(one_type) + " and "
               + CDB_Object::GetTypeName(other_type) + " is not supported");
}

#ifdef NCBI_COMPILER_WORKSHOP
#define CONVERTVALUE_STATIC
#else
#define CONVERTVALUE_STATIC static
#endif

CONVERTVALUE_STATIC void
s_ConvertValue(const CTime& from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_DateTime:
    case eDB_SmallDateTime:
        to_var = from_val;
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        to_var = from_val.AsString();
        break;
    case eDB_Text:
        {
            string str_val = from_val.AsString();
            to_var.Truncate();
            to_var.Append(str_val.data(), str_val.size());
            break;
        }
    default:
        s_ConvertionNotSupported("CTime", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(Int8 from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_BigInt:
        to_var = Int8(from_val);
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        to_var = NStr::Int8ToString(from_val);
        break;
    case eDB_Text:
        {
            string str_val = NStr::Int8ToString(from_val);
            to_var.Truncate();
            to_var.Append(str_val.data(), str_val.size());
            break;
        }
    default:
        s_ConvertionNotSupported("Int8", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(Int4 from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_BigInt:
        to_var = Int8(from_val);
        break;
    case eDB_Int:
        to_var = Int4(from_val);
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        to_var = NStr::IntToString(from_val);
        break;
    case eDB_Text:
        {
            string str_val = NStr::IntToString(from_val);
            to_var.Truncate();
            to_var.Append(str_val.data(), str_val.size());
            break;
        }
    default:
        s_ConvertionNotSupported("Int4", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(short from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_BigInt:
        to_var = Int8(from_val);
        break;
    case eDB_Int:
        to_var = Int4(from_val);
        break;
    case eDB_SmallInt:
        to_var = Int2(from_val);
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        to_var = NStr::IntToString(from_val);
        break;
    case eDB_Text:
        {
            string str_val = NStr::IntToString(from_val);
            to_var.Truncate();
            to_var.Append(str_val.data(), str_val.size());
            break;
        }
    default:
        s_ConvertionNotSupported("short", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(unsigned char from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_BigInt:
        to_var = Int8(from_val);
        break;
    case eDB_Int:
        to_var = Int4(from_val);
        break;
    case eDB_SmallInt:
        to_var = Int2(from_val);
        break;
    case eDB_TinyInt:
        to_var = Uint1(from_val);
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        to_var = NStr::IntToString(from_val);
        break;
    case eDB_Text:
        {
            string str_val = NStr::IntToString(from_val);
            to_var.Truncate();
            to_var.Append(str_val.data(), str_val.size());
            break;
        }
    default:
        s_ConvertionNotSupported("unsigned char", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(bool from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_BigInt:
        to_var = Int8(from_val);
        break;
    case eDB_Int:
        to_var = Int4(from_val);
        break;
    case eDB_SmallInt:
        to_var = Int2(from_val);
        break;
    case eDB_TinyInt:
        to_var = Uint1(from_val);
        break;
    case eDB_Bit:
        to_var = bool(from_val);
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        to_var = NStr::BoolToString(from_val);
        break;
    case eDB_Text:
        {
            string str_val = NStr::BoolToString(from_val);
            to_var.Truncate();
            to_var.Append(str_val.data(), str_val.size());
            break;
        }
    default:
        s_ConvertionNotSupported("bool", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(const float& from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_Float:
        to_var = float(from_val);
        break;
    case eDB_Double:
        to_var = double(from_val);
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        to_var = NStr::DoubleToString(from_val);
        break;
    case eDB_Text:
        {
            string str_val = NStr::DoubleToString(from_val);
            to_var.Truncate();
            to_var.Append(str_val.data(), str_val.size());
            break;
        }
    default:
        s_ConvertionNotSupported("float", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(const double& from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_Double:
        to_var = double(from_val);
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
        to_var = NStr::DoubleToString(from_val);
        break;
    case eDB_Text:
        {
            string str_val = NStr::DoubleToString(from_val);
            to_var.Truncate();
            to_var.Append(str_val.data(), str_val.size());
            break;
        }
    default:
        s_ConvertionNotSupported("double", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(const string& from_val, CVariant& to_var)
{
    switch (to_var.GetType()) {
    case eDB_Char:
    case eDB_VarChar:
    case eDB_LongChar:
        to_var = from_val;
        break;
    case eDB_Binary:
    case eDB_LongBinary:
    case eDB_VarBinary:
        to_var = CVariant::VarBinary(from_val.data(), from_val.size());
        break;
    case eDB_Text:
    case eDB_Image:
        to_var.Truncate();
        to_var.Append(from_val.data(), from_val.size());
        break;
    case eDB_Bit:
        to_var = NStr::StringToBool(from_val);
        break;
    case eDB_Int:
        to_var = NStr::StringToInt(from_val);
        break;
    case eDB_BigInt:
        to_var = NStr::StringToInt8(from_val);
        break;
    case eDB_Double:
        to_var = NStr::StringToDouble(from_val);
        break;
    case eDB_DateTime:
    case eDB_SmallDateTime:
        to_var = CTime(from_val);
        break;
    default:
        s_ConvertionNotSupported("string", to_var.GetType());
    }
}

CONVERTVALUE_STATIC void
s_ConvertValue(const char* from_val, CVariant& to_var)
{
    s_ConvertValue(string(from_val), to_var);
}

#undef CONVERTVALUE_STATIC

static void
s_ConvertValue(const CVariant& from_var, CTime& to_val)
{
    switch (from_var.GetType()) {
    case eDB_DateTime:
    case eDB_SmallDateTime:
        to_val = from_var.GetCTime();
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
        to_val = CTime(from_var.GetString());
        break;
    default:
        s_ConvertionNotSupported("CTime", from_var.GetType());
    }
}

static void
s_ConvertValue(const CVariant& from_var, Int8& to_val)
{
    switch (from_var.GetType()) {
    case eDB_BigInt:
    case eDB_Int:
    case eDB_SmallInt:
    case eDB_TinyInt:
        to_val = from_var.GetInt8();
        break;
    case eDB_Bit:
        to_val = Int8(from_var.GetBit());
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
        to_val = NStr::StringToInt8(from_var.GetString());
        break;
    default:
        s_ConvertionNotSupported("Int8", from_var.GetType());
    }
}

static void
s_ConvertValue(const CVariant& from_var, Int4& to_val)
{
    Int8 temp_val;
    switch (from_var.GetType()) {
    case eDB_BigInt:
        temp_val = from_var.GetInt8();
        break;
    case eDB_Int:
    case eDB_SmallInt:
    case eDB_TinyInt:
        to_val = from_var.GetInt4();
        return;
    case eDB_Bit:
        to_val = Int4(from_var.GetBit());
        return;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
        temp_val = NStr::StringToInt8(from_var.GetString());
        break;
    default:
        s_ConvertionNotSupported("Int4", from_var.GetType());
    }
    if (temp_val < numeric_limits<Int4>::min()
        ||  temp_val > numeric_limits<Int4>::max())
    {
        NCBI_THROW(CSDB_Exception, eOutOfBounds,
                   "Value for Int4 is out of bounds");
    }
    to_val = Int4(temp_val);
}

static void
s_ConvertValue(const CVariant& from_var, short& to_val)
{
    Int8 temp_val;
    switch (from_var.GetType()) {
    case eDB_BigInt:
    case eDB_Int:
        temp_val = from_var.GetInt8();
        break;
    case eDB_SmallInt:
    case eDB_TinyInt:
        to_val = from_var.GetInt2();
        return;
    case eDB_Bit:
        to_val = Int2(from_var.GetBit());
        return;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
        temp_val = NStr::StringToInt8(from_var.GetString());
        break;
    default:
        s_ConvertionNotSupported("short", from_var.GetType());
    }
    if (temp_val < numeric_limits<short>::min()
        ||  temp_val > numeric_limits<short>::max())
    {
        NCBI_THROW(CSDB_Exception, eOutOfBounds,
                   "Value for short is out of bounds");
    }
    to_val = short(temp_val);
}

static void
s_ConvertValue(const CVariant& from_var, unsigned char& to_val)
{
    Int8 temp_val;
    switch (from_var.GetType()) {
    case eDB_BigInt:
    case eDB_Int:
    case eDB_SmallInt:
        temp_val = from_var.GetInt8();
        break;
    case eDB_TinyInt:
        to_val = from_var.GetByte();
        return;
    case eDB_Bit:
        to_val = static_cast<unsigned char>(from_var.GetBit());
        return;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
        temp_val = NStr::StringToInt8(from_var.GetString());
        break;
    default:
        s_ConvertionNotSupported("unsigned char", from_var.GetType());
    }
    if (temp_val < numeric_limits<unsigned char>::min()
        ||  temp_val > numeric_limits<unsigned char>::max())
    {
        NCBI_THROW(CSDB_Exception, eOutOfBounds,
                   "Value for unsigned char is out of bounds");
    }
    to_val = static_cast<unsigned char>(temp_val);
}

static void
s_ConvertValue(const CVariant& from_var, bool& to_val)
{
    Int8 temp_val;
    switch (from_var.GetType()) {
    case eDB_BigInt:
    case eDB_Int:
    case eDB_SmallInt:
    case eDB_TinyInt:
        temp_val = from_var.GetInt8();
        break;
    case eDB_Bit:
        to_val = from_var.GetBit();
        return;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
        temp_val = NStr::StringToInt8(from_var.GetString());
        break;
    default:
        s_ConvertionNotSupported("bool", from_var.GetType());
    }
    if (temp_val != 0  &&  temp_val != 1)
    {
        NCBI_THROW(CSDB_Exception, eOutOfBounds,
                   "Value for bool is out of bounds");
    }
    to_val = temp_val == 1;
}

static void
s_ConvertValue(const CVariant& from_var, float& to_val)
{
    switch (from_var.GetType()) {
    case eDB_Float:
        to_val = from_var.GetFloat();
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
        to_val = float(NStr::StringToDouble(from_var.GetString()));
        break;
    default:
        s_ConvertionNotSupported("float", from_var.GetType());
    }
}

static void
s_ConvertValue(const CVariant& from_var, double& to_val)
{
    switch (from_var.GetType()) {
    case eDB_Float:
    case eDB_Double:
        to_val = from_var.GetDouble();
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
        to_val = NStr::StringToDouble(from_var.GetString());
        break;
    default:
        s_ConvertionNotSupported("double", from_var.GetType());
    }
}

static void
s_ConvertValue(const CVariant& from_var, string& to_val)
{
    switch (from_var.GetType()) {
    case eDB_BigInt:
    case eDB_Int:
    case eDB_SmallInt:
    case eDB_TinyInt:
        to_val = NStr::Int8ToString(from_var.GetInt8());
        break;
    case eDB_Bit:
        to_val = NStr::BoolToString(from_var.GetBit());
        break;
    case eDB_Float:
    case eDB_Double:
        to_val = NStr::DoubleToString(from_var.GetDouble());
        break;
    case eDB_DateTime:
    case eDB_SmallDateTime:
        to_val = from_var.GetCTime().AsString();
        break;
    case eDB_VarChar:
    case eDB_Char:
    case eDB_LongChar:
    case eDB_Text:
    case eDB_VarBinary:
    case eDB_Binary:
    case eDB_LongBinary:
    case eDB_Image:
    case eDB_Numeric:
        to_val = from_var.GetString();
        break;
    default:
        s_ConvertionNotSupported("string", from_var.GetType());
    }
}


class CDataSourceInitializer
{
public:
    CDataSourceInitializer(const CDBConnParams& params)
    {
        CNcbiRegistry* reg = NULL;
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app)
            reg = &app->GetConfig();
        CONNECT_Init(reg);
        DBLB_INSTALL_DEFAULT();
        DBAPI_RegisterDriver_FTDS();

        IDataSource* ds = CDriverManager::GetInstance().MakeDs(params);
        I_DriverContext* ctx = ds->GetDriverContext();
        ctx->PushCntxMsgHandler(new CDB_UserHandler_Exception, eTakeOwnership);
        ctx->PushDefConnMsgHandler(new CDB_UserHandler_Exception, eTakeOwnership);
    }
};


inline
CDatabaseImpl::CDatabaseImpl(const string&             db_name,
                             const string&             user,
                             const string&             password,
                             const string&             server,
                             const map<string, string> params)
{
    CDBDefaultConnParams conn_params(server, user, password);
    conn_params.SetDatabaseName(db_name);
    conn_params.SetDriverName("ftds");
    conn_params.SetEncoding(eEncoding_UTF8);

    typedef map<string, string>  TStrMap;
    ITERATE(TStrMap, it, params) {
        conn_params.SetParam(it->first, it->second);
    }

    static CDataSourceInitializer ds_init(conn_params);
    IDataSource* ds = CDriverManager::GetInstance().MakeDs(conn_params);

    AutoPtr<IConnection> conn = ds->CreateConnection();
    conn->Connect(conn_params);
    m_Conn.Reset(new TConnHolder(conn.release()));
}

inline
CDatabaseImpl::CDatabaseImpl(const CDatabaseImpl& other)
    : m_Conn(other.m_Conn)
{}

inline void
CDatabaseImpl::Close(void)
{
    if (m_Conn  &&  m_Conn->ReferencedOnlyOnce()) {
        m_Conn->GetData()->Close();
        delete m_Conn->GetData();
    }
    m_Conn.Reset();
}

inline
CDatabaseImpl::~CDatabaseImpl(void)
{
    Close();
}

inline bool
CDatabaseImpl::IsOpen(void) const
{
    return m_Conn.NotNull();
}

inline IConnection*
CDatabaseImpl::GetConnection(void)
{
    return m_Conn->GetData();
}


CDatabase::CDatabase(void)
{}

CDatabase::CDatabase(const string& db_name)
    : m_DBName(db_name)
{
    if (db_name.empty()) {
        NCBI_THROW(CSDB_Exception, eEmptyDBName,
                   "Database name cannot be empty");
    }
}

CDatabase::CDatabase(const CDatabase& db)
{
    operator= (db);
}

CDatabase&
CDatabase::operator= (const CDatabase& db)
{
    m_DBName = db.m_DBName;
    m_Props  = db.m_Props;
    if (db.m_Impl.NotNull()) {
        m_User     = db.m_User;
        m_Password = db.m_Password;
        m_Server   = db.m_Server;
        m_Impl.Reset(new CDatabaseImpl(*db.m_Impl));
    }
    else {
        m_Impl.Reset();
    }
    return *this;
}

CDatabase::~CDatabase(void)
{
    if (m_Impl)
        m_Impl->Close();
}

void
CDatabase::Connect(const string &user,
                   const string &password,
                   const string &server)
{
    if (m_Impl) {
        m_Impl->Close();
        m_Impl.Reset();
    }
    if (m_DBName.empty()) {
        NCBI_THROW(CSDB_Exception, eEmptyDBName,
                   "Database name cannot be empty");
    }
    m_User     = user;
    m_Password = password;
    m_Server   = server;
    try {
        m_Impl.Reset(new CDatabaseImpl(m_DBName, user, password, server, m_Props));
    }
    SDBAPI_CATCH_LOWLEVEL()
}

void
CDatabase::Close(void)
{
    if (!m_Impl)
        return;
    m_Impl->Close();
    m_Impl.Reset();
    m_User = m_Password = m_Server = "";
}

CDatabase
CDatabase::Clone(void)
{
    if (!m_Impl) {
        NCBI_THROW(CSDB_Exception, eClosed,
                   "Database cannot be cloned if it's not connected");
    }

    CDatabase result(m_DBName);
    result.m_Props = m_Props;
    result.Connect(m_User, m_Password, m_Server);
    return result;
}

CQuery
CDatabase::NewQuery(void)
{
    if (!m_Impl) {
        NCBI_THROW(CSDB_Exception, eClosed,
                   "Cannot create query when not connected");
    }
    return CQuery(m_Impl);
}

CBulkInsert
CDatabase::NewBulkInsert(const string& table_name, int autoflush)
{
    if (!m_Impl) {
        NCBI_THROW(CSDB_Exception, eClosed,
                   "Cannot create query when not connected");
    }
    return CBulkInsert(m_Impl, table_name, autoflush);
}


inline
CBulkInsertImpl::CBulkInsertImpl(CDatabaseImpl* db_impl,
                                 const string&  table_name,
                                 int            autoflush)
    : m_DBImpl(db_impl),
      m_BI(NULL),
      m_Autoflush(autoflush),
      m_RowsWritten(0),
      m_ColsWritten(0),
      m_WriteStarted(false)
{
    m_BI = db_impl->GetConnection()->GetBulkInsert(table_name);
}

CBulkInsertImpl::~CBulkInsertImpl(void)
{
    if (m_BI  &&  m_RowsWritten != 0) {
        m_BI->Complete();
    }
    delete m_BI;
}

void
CBulkInsertImpl::x_CheckCanWrite(int col)
{
    if (!m_BI) {
        NCBI_THROW(CSDB_Exception, eClosed,
                   "Cannot write into completed CBulkInsert");
    }
    if (!m_DBImpl->IsOpen()) {
        m_BI->Cancel();
        delete m_BI;
        m_BI = NULL;
        NCBI_THROW(CSDB_Exception, eClosed,
                   "Cannot write into CBulkInsert when CDatabase was closed");
    }
    if (col != 0  &&  col > int(m_Cols.size())) {
        NCBI_THROW(CSDB_Exception, eInconsistent,
                   "Too many values were written to CBulkInsert");
    }
}

void
CBulkInsertImpl::SetHints(CTempString hints)
{
    x_CheckCanWrite(0);
    m_BI->SetHints(hints);
}

void
CBulkInsertImpl::AddHint(IBulkInsert::EHints hint, unsigned int value)
{
    x_CheckCanWrite(0);
    m_BI->AddHint(hint, value);
}

void
CBulkInsertImpl::AddOrderHint(CTempString columns)
{
    x_CheckCanWrite(0);
    m_BI->AddOrderHint(columns);
}

inline void
CBulkInsertImpl::Bind(int col, ESDB_Type type)
{
    x_CheckCanWrite(0);
    if (m_WriteStarted) {
        NCBI_THROW(CSDB_Exception, eStarted,
                   "Cannot bind columns when already started to insert");
    }
    if (col - 1 != int(m_Cols.size())) {
        NCBI_THROW(CSDB_Exception, eNotInOrder,
                   "Cannot bind columns in CBulkInsert randomly");
    }
    m_Cols.push_back(CVariant(s_ConvertType(type)));
}

inline void
CBulkInsertImpl::EndRow(void)
{
    x_CheckCanWrite(0);
    if (m_ColsWritten != int(m_Cols.size())) {
        NCBI_THROW(CSDB_Exception, eInconsistent,
                   "Not enough values were written to CBulkInsert");
    }
    try {
        m_BI->AddRow();
        if (++m_RowsWritten == m_Autoflush) {
            m_BI->StoreBatch();
            m_RowsWritten = 0;
        }
        m_ColsWritten = 0;
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline void
CBulkInsertImpl::Complete(void)
{
    x_CheckCanWrite(0);
    try {
        m_BI->Complete();
    }
    SDBAPI_CATCH_LOWLEVEL()
    delete m_BI;
    m_BI = NULL;
}

inline void
CBulkInsertImpl::x_CheckWriteStarted(void)
{
    x_CheckCanWrite(m_ColsWritten + 1);
    if (!m_WriteStarted) {
        m_WriteStarted = true;
        try {
            for (unsigned int i = 0; i < m_Cols.size(); ++i) {
                m_BI->Bind(i + 1, &m_Cols[i]);
            }
        }
        SDBAPI_CATCH_LOWLEVEL()
    }
}

inline void
CBulkInsertImpl::WriteNull(void)
{
    x_CheckWriteStarted();
    m_Cols[m_ColsWritten++].SetNull();
}

template <class T>
inline void
CBulkInsertImpl::WriteVal(const T& val)
{
    x_CheckWriteStarted();
    s_ConvertValue(val, m_Cols[m_ColsWritten++]);
}


CBulkInsert::CBulkInsert(void)
{}

CBulkInsert::CBulkInsert(CDatabaseImpl* db_impl,
                         const string&  table_name,
                         int            autoflush)
{
    m_Impl.Reset(new CBulkInsertImpl(db_impl, table_name, autoflush));
}

CBulkInsert::CBulkInsert(const CBulkInsert& bi)
    : m_Impl(bi.m_Impl)
{}

CBulkInsert::~CBulkInsert(void)
{}

CBulkInsert&
CBulkInsert::operator= (const CBulkInsert& bi)
{
    m_Impl = bi.m_Impl;
    return *this;
}

void
CBulkInsert::SetHints(CTempString hints)
{
    m_Impl->SetHints(hints);
}

void
CBulkInsert::AddHint(EHints hint)
{
    try {
        m_Impl->AddHint(s_ConvertHints(hint), 0);
    }
    SDBAPI_CATCH_LOWLEVEL()
}

void
CBulkInsert::AddHint(EHintsWithValue hint, unsigned int value)
{
    try {
        m_Impl->AddHint(s_ConvertHints(hint), value);
    }
    SDBAPI_CATCH_LOWLEVEL()
}

void
CBulkInsert::AddOrderHint(CTempString columns)
{
    m_Impl->AddOrderHint(columns);
}

void
CBulkInsert::Bind(int col, ESDB_Type type)
{
    m_Impl->Bind(col, type);
}

void
CBulkInsert::Complete(void)
{
    m_Impl->Complete();
}

CBulkInsert&
CBulkInsert::operator<<(const string& val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(const char* val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(unsigned char val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(short val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(Int4 val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(Int8 val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(float val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(double val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(bool val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(const CTime& val)
{
    m_Impl->WriteVal(val);
    return *this;
}

CBulkInsert&
CBulkInsert::operator<<(CBulkInsert& (*f)(CBulkInsert&))
{
    return f(*this);
}

CBulkInsert&
NullValue(CBulkInsert& bi)
{
    bi.m_Impl->WriteNull();
    return bi;
}

CBulkInsert&
EndRow(CBulkInsert& bi)
{
    bi.m_Impl->EndRow();
    return bi;
}


inline
SQueryParamInfo::SQueryParamInfo(void)
    : type(eSP_In),
      value(NULL),
      field(NULL)
{}


inline
CQuery::CField::CField(CQueryImpl* q, SQueryParamInfo* param_info)
    : m_IsParam(true),
      m_Query(q),
      m_ParamInfo(param_info)
{}

inline
CQuery::CField::CField(CQueryImpl* q, unsigned int col_num)
    : m_IsParam(false),
      m_Query(q),
      m_ColNum(col_num)
{}

inline
CQuery::CField::~CField(void)
{}


inline
CQueryImpl::CQueryImpl(CDatabaseImpl* db_impl)
    : m_DBImpl(db_impl),
      m_Stmt(NULL),
      m_CallStmt(NULL),
      m_IgnoreBounds(false),
      m_CurRS(NULL),
      m_RSBeginned(false),
      m_RSFinished(true),
      m_CurRSNo(0),
      m_CurRowNo(0),
      m_RowCount(-1),
      m_Status(-1)
{
    m_Stmt = db_impl->GetConnection()->GetStatement();
}

CQueryImpl::~CQueryImpl(void)
{
    x_Close();
    delete m_Stmt;
}

void
CQueryImpl::x_CheckCanWork(bool need_rs /* = false */) const
{
    if (!m_DBImpl->IsOpen()) {
        NCBI_THROW(CSDB_Exception, eClosed,
                   "CQuery is not operational because CDatabase was closed");
    }
    if (need_rs  &&  !m_CurRS
        &&  !const_cast<CQueryImpl*>(this)->HasMoreResultSets())
    {
        NCBI_THROW(CSDB_Exception, eClosed,
                   "CQuery is closed or never executed");
    }
}

template <class T>
inline void
CQueryImpl::SetParameter(CTempString   name,
                         const T&      value,
                         ESDB_Type     type,
                         ESP_ParamType param_type)
{
    x_CheckCanWork();

    SQueryParamInfo& info = m_Params[name];
    info.type = param_type;
    EDB_Type var_type = s_ConvertType(type);
    if (!info.value  ||  info.value->GetType() != var_type) {
        delete info.value;
        info.value = new CVariant(var_type);
    }
    s_ConvertValue(value, *info.value);
}

inline void
CQueryImpl::SetNullParameter(CTempString   name,
                             ESDB_Type     type,
                             ESP_ParamType param_type)
{
    x_CheckCanWork();

    SQueryParamInfo& info = m_Params[name];
    info.type = param_type;
    EDB_Type var_type = s_ConvertType(type);
    if (!info.value  ||  info.value->GetType() != var_type) {
        delete info.value;
        info.value = new CVariant(var_type);
    }
    info.value->SetNull();
}

inline void
CQueryImpl::x_SetOutParameter(const string& name, const CVariant& value)
{
    SQueryParamInfo& info = m_Params[name];
    if (info.value) {
        *info.value = value;
    }
    else {
        info.value = new CVariant(value);
        info.type = eSP_InOut;
    }
}

inline const CVariant&
CQueryImpl::GetFieldValue(const CQuery::CField& field)
{
    return field.m_IsParam? *field.m_ParamInfo->value
                          :  m_CurRS->GetVariant(field.m_ColNum);
}

inline const CQuery::CField&
CQueryImpl::GetParameter(CTempString name)
{
    x_CheckCanWork();

    TParamsMap::iterator it = m_Params.find(name);
    if (it == m_Params.end()) {
        NCBI_THROW(CSDB_Exception, eNotExist,
                   "Parameter '" + string(name) + "' doesn't exist");
    }
    SQueryParamInfo& info = it->second;
    if (!info.field)
        info.field = new CQuery::CField(this, &info);
    return *info.field;
}

inline void
CQueryImpl::ClearParameter(CTempString name)
{
    x_CheckCanWork();

    TParamsMap::iterator it = m_Params.find(name);
    if (it != m_Params.end()) {
        SQueryParamInfo& info = it->second;
        delete info.field;
        delete info.value;
        m_Params.erase(it);
    }
}

inline void
CQueryImpl::ClearParameters(void)
{
    x_CheckCanWork();

    ITERATE(TParamsMap, it, m_Params) {
        const SQueryParamInfo& info = it->second;
        delete info.field;
        delete info.value;
    }
    m_Params.clear();
}

inline void
CQueryImpl::SetSql(CTempString sql)
{
    x_CheckCanWork();
    m_Sql = sql;
}

void
CQueryImpl::x_Close(void)
{
    delete m_CurRS;
    m_CurRS = NULL;
    if (m_CallStmt) {
        m_CallStmt->PurgeResults();
        delete m_CallStmt;
        m_CallStmt = NULL;
    }
    m_Stmt->PurgeResults();
    m_Stmt->Close();
}

inline void
CQueryImpl::x_InitBeforeExec(void)
{
    m_IgnoreBounds = false;
    m_RSBeginned = false;
    m_RSFinished = true;
    m_CurRSNo = 0;
    m_CurRowNo = 0;
    m_RowCount = 0;
    m_Status = -1;
}

inline void
CQueryImpl::Execute(void)
{
    x_CheckCanWork();

    x_Close();
    x_InitBeforeExec();
    try {
        m_Stmt->ClearParamList();
        ITERATE(TParamsMap, it, m_Params) {
            const SQueryParamInfo& info = it->second;
            m_Stmt->SetParam(*info.value, it->first);
        }
        m_Stmt->SendSql(m_Sql);
        HasMoreResultSets();
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline void
CQueryImpl::ExecuteSP(CTempString sp)
{
    x_CheckCanWork();

    x_Close();
    x_InitBeforeExec();
    try {
        m_CallStmt = m_DBImpl->GetConnection()->GetCallableStatement(sp);
        ITERATE(TParamsMap, it, m_Params) {
            const SQueryParamInfo& info = it->second;
            if (info.type == eSP_In)
                m_CallStmt->SetParam(*info.value, it->first);
            else
                m_CallStmt->SetOutputParam(*info.value, it->first);
        }
        m_CallStmt->Execute();
        HasMoreResultSets();
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline void
CQueryImpl::SetIgnoreBounds(bool is_ignore)
{
    x_CheckCanWork();
    m_IgnoreBounds = is_ignore;
}

inline unsigned int
CQueryImpl::GetResultSetNo(void)
{
    x_CheckCanWork();
    return m_CurRSNo;
}

inline unsigned int
CQueryImpl::GetRowNo(void)
{
    x_CheckCanWork();
    return m_CurRowNo;
}

inline int
CQueryImpl::GetRowCount(void)
{
    x_CheckCanWork();
    if (m_CurRS  ||  m_Stmt->HasMoreResults())
        return -1;
    else
        return m_RowCount;
}

inline int
CQueryImpl::GetStatus(void)
{
    x_CheckCanWork();
    return m_Status;
}

inline bool
CQueryImpl::x_Fetch(void)
{
    try {
        if (m_CurRS->Next()) {
            ++m_CurRowNo;
            return true;
        }
        else {
            m_RSFinished = true;
            return false;
        }
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline void
CQueryImpl::x_InitRSFields(void)
{
    m_Fields.clear();
    m_ColNums.clear();
    unsigned int cols_cnt = m_CurRS->GetTotalColumns();
    const IResultSetMetaData* meta = m_CurRS->GetMetaData();
    m_Fields.resize(cols_cnt);
    for (unsigned int i = 1; i <= cols_cnt; ++i) {
        m_Fields[i - 1] = new CQuery::CField(this, i);
        m_ColNums[meta->GetName(i)] = i;
    }
}

bool
CQueryImpl::HasMoreResultSets(void)
{
    x_CheckCanWork();
    if (m_CurRS  &&  !m_RSBeginned)
        return true;
    while (m_CurRS  &&  !m_RSFinished) {
        x_Fetch();
    }
    delete m_CurRS;
    m_CurRS = NULL;
    m_RSBeginned = m_RSFinished = false;

    IStatement* stmt = (m_CallStmt? m_CallStmt: m_Stmt);
    try {
        while (stmt->HasMoreResults()) {
            m_CurRS = stmt->GetResultSet();
            if (!m_CurRS)
                continue;
            switch (m_CurRS->GetResultType()) {
            case eDB_StatusResult:
                if (m_CallStmt) {
                    m_CurRS->Next();
                    m_Status = m_CurRS->GetVariant(1).GetInt4();
                }
                delete m_CurRS;
                break;
            case eDB_ParamResult:
                if (m_CallStmt) {
                    m_CurRS->Next();
                    unsigned int col_cnt = m_CurRS->GetTotalColumns();
                    const IResultSetMetaData* meta = m_CurRS->GetMetaData();
                    for (unsigned int i = 1; i <= col_cnt; ++i) {
                        x_SetOutParameter(meta->GetName(i),
                                          m_CurRS->GetVariant(i));
                    }
                }
                delete m_CurRS;
                break;
            case eDB_RowResult:
                ++m_CurRSNo;
                x_InitRSFields();
                return true;
            case eDB_ComputeResult:
            case eDB_CursorResult:
                _ASSERT(false);
            }
        }
    }
    SDBAPI_CATCH_LOWLEVEL()

    if (m_RowCount == 0) {
        if (m_CurRowNo != 0) {
            m_RowCount = m_CurRowNo;
            m_CurRowNo = 0;
        }
        else {
            m_RowCount = stmt->GetRowCount();
        }
    }
    m_CurRSNo = 0;
    m_RSFinished = true;
    return false;
}

inline void
CQueryImpl::BeginNewRS(void)
{
    while (HasMoreResultSets()  &&  !x_Fetch()  &&  m_IgnoreBounds)
        m_RSBeginned = true;
    m_RSBeginned = true;
}

inline void
CQueryImpl::PurgeResults(void)
{
    x_CheckCanWork();
    while (HasMoreResultSets())
        BeginNewRS();
}

inline int
CQueryImpl::GetTotalColumns(void)
{
    x_CheckCanWork(true);
    try {
        return m_CurRS->GetTotalColumns();
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline string
CQueryImpl::GetColumnName(unsigned int col)
{
    x_CheckCanWork(true);
    try {
        return m_CurRS->GetMetaData()->GetName(col);
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline ESDB_Type
CQueryImpl::GetColumnType(unsigned int col)
{
    x_CheckCanWork(true);
    try {
        return s_ConvertType(m_CurRS->GetMetaData()->GetType(col));
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline void
CQueryImpl::Next(void)
{
    while (!x_Fetch()  &&  m_IgnoreBounds  &&  HasMoreResultSets())
        m_RSBeginned = true;
    m_RSBeginned = true;
}

inline bool
CQueryImpl::IsFinished(void) const
{
    return m_RSFinished;
}

inline const CQuery::CField&
CQueryImpl::GetColumn(const CDBParamVariant& col) const
{
    x_CheckCanWork(true);
    int pos = -1;
    if (col.IsPositional())
        pos = col.GetPosition();
    else {
        TColNumsMap::const_iterator it = m_ColNums.find(col.GetName());
        if (it != m_ColNums.end())
            pos = it->second;
    }
    if (pos <= 0  ||  pos > int(m_Fields.size())) {
        NCBI_THROW(CSDB_Exception, eNotExist,
                   "No such column in the result set");
    }
    return *m_Fields[pos - 1];
}


CQuery::CRowIterator::CRowIterator(void)
    : m_IsEnd(false)
{}

inline
CQuery::CRowIterator::CRowIterator(CQueryImpl* q, bool is_end)
    : m_Query(q),
      m_IsEnd(is_end)
{}

CQuery::CRowIterator::CRowIterator(const CRowIterator& ri)
    : m_Query(ri.m_Query),
      m_IsEnd(ri.m_IsEnd)
{}

CQuery::CRowIterator&
CQuery::CRowIterator::operator= (const CRowIterator& ri)
{
    m_Query = ri.m_Query;
    m_IsEnd = ri.m_IsEnd;
    return *this;
}

CQuery::CRowIterator::~CRowIterator(void)
{}

unsigned int
CQuery::CRowIterator::GetResultSetNo(void)
{
    return m_Query->GetResultSetNo();
}

unsigned int
CQuery::CRowIterator::GetRowNo(void)
{
    return m_Query->GetRowNo();
}

int
CQuery::CRowIterator::GetTotalColumns(void)
{
    return m_Query->GetTotalColumns();
}

string
CQuery::CRowIterator::GetColumnName(unsigned int col)
{
    return m_Query->GetColumnName(col);
}

ESDB_Type
CQuery::CRowIterator::GetColumnType(unsigned int col)
{
    return m_Query->GetColumnType(col);
}

bool
CQuery::CRowIterator::operator== (const CRowIterator& ri) const
{
    if (m_Query != ri.m_Query)
        return false;
    else if (m_IsEnd ^ ri.m_IsEnd) {
        return m_Query->IsFinished();
    }
    else
        return true;
}

CQuery::CRowIterator&
CQuery::CRowIterator::operator++ (void)
{
    if (m_IsEnd  ||  m_Query->IsFinished()) {
        NCBI_THROW(CSDB_Exception, eInconsistent,
                   "Cannot increase end() iterator");
    }
    m_Query->Next();
    return *this;
}

const CQuery::CField&
CQuery::CRowIterator::operator[](int col) const
{
    return m_Query->GetColumn(col);
}

const CQuery::CField&
CQuery::CRowIterator::operator[](CTempString col) const
{
    return m_Query->GetColumn(string(col));
}


string
CQuery::CField::AsString(void) const
{
    string value;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

unsigned char
CQuery::CField::AsByte(void) const
{
    unsigned char value = 0;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

short
CQuery::CField::AsShort(void) const
{
    short value;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

Int4
CQuery::CField::AsInt4(void) const
{
    Int4 value;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

Int8
CQuery::CField::AsInt8(void) const
{
    Int8 value;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

float
CQuery::CField::AsFloat(void) const
{
    float value;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

double
CQuery::CField::AsDouble(void) const
{
    double value;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

bool
CQuery::CField::AsBool(void) const
{
    bool value = false;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

CTime
CQuery::CField::AsDateTime(void) const
{
    CTime value;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

const vector<unsigned char>&
CQuery::CField::AsVector(void) const
{
    const CVariant& var_val = m_Query->GetFieldValue(*this);
    EDB_Type var_type = var_val.GetType();
    if (var_type != eDB_Image  &&  var_type != eDB_Text) {
        NCBI_THROW(CSDB_Exception, eUnsupported,
                   "Method is unsupported for this type of data");
    }
    string value = var_val.GetString();
    // WorkShop cannot eat string::iterators in vector<>::insert (although due
    // to STL he has to eat any iterator-like type) but is okay with pointers
    // to unsigned char here.
    const unsigned char* data
                       = reinterpret_cast<const unsigned char*>(value.data());
    m_Vector.clear();
    m_Vector.insert(m_Vector.begin(), data, data + value.size());
    return m_Vector;
}

CNcbiIstream&
CQuery::CField::AsIStream(void) const
{
    const CVariant& var_val = m_Query->GetFieldValue(*this);
    EDB_Type var_type = var_val.GetType();
    if (var_type != eDB_Image  &&  var_type != eDB_Text) {
        NCBI_THROW(CSDB_Exception, eUnsupported,
                   "Method is unsupported for this type of data");
    }
    m_ValueForStream = var_val.GetString();
    m_Stream.reset(new CNcbiIstrstream(m_ValueForStream.c_str()));
    return *m_Stream;
}


CQuery::CQuery(void)
{}

CQuery::CQuery(CDatabaseImpl* db_impl)
{
    m_Impl.Reset(new CQueryImpl(db_impl));
}

CQuery::CQuery(const CQuery& q)
    : m_Impl(q.m_Impl)
{}

CQuery::~CQuery(void)
{}

CQuery &
CQuery::operator= (const CQuery& q)
{
    m_Impl = q.m_Impl;
    return *this;
}

void
CQuery::SetParameter(CTempString   name,
                     const string& value,
                     ESDB_Type     type /* = eSDB_String */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     const char*   value,
                     ESDB_Type     type /* = eSDB_String */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     Int8          value,
                     ESDB_Type     type /* = eSDB_Int8 */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     Int4          value,
                     ESDB_Type     type /* = eSDB_Int4 */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     short         value,
                     ESDB_Type     type /* = eSDB_Short */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     unsigned char value,
                     ESDB_Type     type /* = eSDB_Byte */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     float         value,
                     ESDB_Type     type /* = eSDB_Float */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     double        value,
                     ESDB_Type     type /* = eSDB_Double */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     const CTime&  value,
                     ESDB_Type     type /* = eSDB_DateTime */,
                     ESP_ParamType param_type /*= eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetParameter(CTempString   name,
                     bool          value,
                     ESDB_Type     type /* = eSDB_Byte */,
                     ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetParameter(name, value, type, param_type);
}

void
CQuery::SetNullParameter(CTempString   name,
                         ESDB_Type     type,
                         ESP_ParamType param_type /* = eSP_In */)
{
    m_Impl->SetNullParameter(name, type, param_type);
}

const CQuery::CField&
CQuery::GetParameter(CTempString name)
{
    return m_Impl->GetParameter(name);
}

void
CQuery::ClearParameter(CTempString name)
{
    m_Impl->ClearParameter(name);
}

void
CQuery::ClearParameters(void)
{
    m_Impl->ClearParameters();
}

void
CQuery::SetSql(CTempString sql)
{
    m_Impl->SetSql(sql);
}

void
CQuery::Execute(void)
{
    m_Impl->Execute();
}

void
CQuery::ExecuteSP(CTempString sp)
{
    m_Impl->ExecuteSP(sp);
}

CQuery&
CQuery::SingleSet(void)
{
    m_Impl->SetIgnoreBounds(true);
    return *this;
}

CQuery&
CQuery::MultiSet(void)
{
    m_Impl->SetIgnoreBounds(false);
    return *this;
}

unsigned int
CQuery::GetResultSetNo(void)
{
    return m_Impl->GetResultSetNo();
}

unsigned int
CQuery::GetRowNo(void)
{
    return m_Impl->GetRowNo();
}

int
CQuery::GetRowCount(void)
{
    return m_Impl->GetRowCount();
}

int
CQuery::GetStatus(void)
{
    return m_Impl->GetStatus();
}

bool
CQuery::HasMoreResultSets(void)
{
    return m_Impl->HasMoreResultSets();
}

void
CQuery::PurgeResults(void)
{
    m_Impl->PurgeResults();
}

int
CQuery::GetTotalColumns(void)
{
    return m_Impl->GetTotalColumns();
}

string
CQuery::GetColumnName(unsigned int col)
{
    return m_Impl->GetColumnName(col);
}

ESDB_Type
CQuery::GetColumnType(unsigned int col)
{
    return m_Impl->GetColumnType(col);
}

CQuery::CRowIterator
CQuery::begin(void)
{
    m_Impl->BeginNewRS();
    return CRowIterator(m_Impl, false);
}

CQuery::CRowIterator
CQuery::end(void)
{
    return CRowIterator(m_Impl, true);
}

END_NCBI_SCOPE
