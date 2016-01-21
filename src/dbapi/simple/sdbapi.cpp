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

#include <corelib/ncbi_safe_static.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <dbapi/driver/dbapi_driver_conn_params.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/error_codes.hpp>

#include "sdbapi_impl.hpp"
#include "../rw_impl.hpp"


#define NCBI_USE_ERRCODE_X  Dbapi_Sdbapi


#ifdef HAVE_LIBCONNEXT
#  include "sdb_decryptor.cpp"
#endif


BEGIN_NCBI_SCOPE


#define SDBAPI_CATCH_LOWLEVEL()                             \
    catch (CDB_DeadlockEx& ex) {                            \
        NCBI_RETHROW(ex, CSDB_DeadlockException, eLowLevel, ""); \
    } catch (CDB_Exception& ex) {                           \
        NCBI_RETHROW(ex, CSDB_Exception, eLowLevel, "");    \
    }

#define SDBAPI_THROW(code, msg) \
    NCBI_THROW(CSDB_Exception, code, \
               CDB_Exception::SMessageInContext(msg, x_GetContext()))
    


const char*
CSDB_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eURLFormat:    return "eURLFormat";
    case eClosed:       return "eClosed";
    case eStarted:      return "eStarted";
    case eNotInOrder:   return "eNotInOrder";
    case eInconsistent: return "eInconsistent";
    case eUnsupported:  return "eUnsupported";
    case eNotExist:     return "eNotExist";
    case eOutOfBounds:  return "eOutOfBounds";
    case eLowLevel:     return "eLowLevel";
    case eWrongParams:  return "eWrongParams";
    default:            return CException::GetErrCodeString();
    }
}

static CSafeStatic<CDB_Exception::SContext> kEmptyContext;

void CSDB_Exception::ReportExtra(ostream& os) const
{
    os << *m_Context;
}

void CSDB_Exception::x_Init(const CDiagCompileInfo&, const string&,
                            const CException* prev, EDiagSev)
{
    const CDB_Exception* dbex = dynamic_cast<const CDB_Exception*>(prev);
    if (dbex == NULL) {
        if (m_Context.Empty()) {
            m_Context.Reset(&kEmptyContext.Get());
        }
    } else if (m_Context.Empty()) {
        m_Context.Reset(&dbex->GetContext());
    } else {
        const_cast<CDB_Exception::SContext&>(*m_Context)
            .UpdateFrom(dbex->GetContext());
    }
}

void CSDB_Exception::x_Assign(const CException& src)
{
    CException::x_Assign(src);
    const CSDB_Exception* sdb_src = dynamic_cast<const CSDB_Exception*>(&src);
    _ASSERT(sdb_src != NULL);
    m_Context = sdb_src->m_Context;
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
    case eSDB_StringUCS2:
        return eDB_VarChar;
    case eSDB_Binary:
        return eDB_VarBinary;
    case eSDB_DateTime:
        return eDB_DateTime;
    case eSDB_Text:
    case eSDB_TextUCS2:
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
               + CDB_Object::GetTypeName(other_type, false)
               + " is not supported");
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
        to_var.Truncate();
        to_var.Append(from_val);
        break;
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
    case eDB_SmallInt:
        to_var = NStr::StringToNumeric<Int2>(from_val);
        break;
    case eDB_TinyInt:
        to_var = NStr::StringToNumeric<Uint1>(from_val);
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

CONVERTVALUE_STATIC void
s_ConvertValue(const TStringUCS2& from_val, CVariant& to_var)
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
        s_ConvertValue(string(reinterpret_cast<const char*>(from_val.data()),
                              from_val.size() * sizeof(TCharUCS2)),
                       to_var);
        break;
    case eDB_Text:
        to_var.Truncate();
        to_var.Append(from_val);
        break;
    case eDB_Image:
        to_var.Truncate();
        to_var.Append(reinterpret_cast<const char*>(from_val.data()),
                      from_val.size() * sizeof(TCharUCS2));
        break;
    case eDB_Bit:
    case eDB_Int:
    case eDB_BigInt:
    case eDB_Double:
    case eDB_DateTime:
    case eDB_SmallDateTime:
        s_ConvertValue(CUtf8::AsUTF8(from_val), to_var);
        break;
    default:
        s_ConvertionNotSupported("UCS2 string", to_var.GetType());
    }
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
        return;
    }
    if (temp_val < numeric_limits<Int4>::min()
        ||  temp_val > numeric_limits<Int4>::max())
    {
        NCBI_THROW(CSDB_Exception, eOutOfBounds,
                   "Value for Int4 is out of bounds: "
                   + NStr::NumericToString(temp_val));
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
        return;
    }
    if (temp_val < numeric_limits<short>::min()
        ||  temp_val > numeric_limits<short>::max())
    {
        NCBI_THROW(CSDB_Exception, eOutOfBounds,
                   "Value for short is out of bounds: "
                   + NStr::NumericToString(temp_val));
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
        return;
    }
    if (temp_val < numeric_limits<unsigned char>::min()
        ||  temp_val > numeric_limits<unsigned char>::max())
    {
        NCBI_THROW(CSDB_Exception, eOutOfBounds,
                   "Value for unsigned char is out of bounds: "
                   + NStr::NumericToString(temp_val));
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
        return;
    }
    if (temp_val != 0  &&  temp_val != 1)
    {
        NCBI_THROW(CSDB_Exception, eOutOfBounds,
                   "Value for bool is out of bounds: "
                   + NStr::NumericToString(temp_val));
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


class CDataSourceInitializer : protected CConnIniter
{
public:
    CDataSourceInitializer(void)
    {
        DBLB_INSTALL_DEFAULT();
        DBAPI_RegisterDriver_FTDS();

        CDBConnParamsBase params;
        params.SetDriverName("ftds");
        params.SetEncoding(eEncoding_UTF8);
        IDataSource* ds
            = CDriverManager::GetInstance().MakeDs(params, ".sdbapi");
        I_DriverContext* ctx = ds->GetDriverContext();
        ctx->PushCntxMsgHandler(new CDB_UserHandler_Exception, eTakeOwnership);
        ctx->PushDefConnMsgHandler(new CDB_UserHandler_Exception, eTakeOwnership);
    }
};


static CSafeStatic<CDataSourceInitializer> ds_init;

inline
static IDataSource* s_GetDataSource(void)
{
    ds_init.Get();
    return CDriverManager::GetInstance().CreateDs("ftds", ".sdbapi");
}

inline
static impl::CDriverContext* s_GetDBContext(void)
{
    IDataSource* ds = s_GetDataSource();
    return static_cast<impl::CDriverContext*>(ds->GetDriverContext());
}


string CSDB_Decryptor::x_GetKey(const CTempString& key_id)
{
    string key;
    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app != NULL) {
        key = app->GetConfig().GetString("DBAPI_key", key_id, kEmptyStr);
    }

#ifdef HAVE_LIBCONNEXT
    if (key.empty()) {
        key = s_GetBuiltInKey(key_id);
    }
#endif
    if (key.empty()) {
        NCBI_THROW(CSDB_Exception, eWrongParams,
                   "Unknown password decryption key ID " + string(key_id));
    }
    return key;
}


DEFINE_STATIC_FAST_MUTEX(s_DecryptorMutex);
static CSafeStatic<CRef<CSDB_Decryptor> > s_Decryptor;
static bool s_DecryptorInitialized = false;

void CSDB_ConnectionParam::SetGlobalDecryptor(CRef<CSDB_Decryptor> decryptor)
{
    CFastMutexGuard GUARD(s_DecryptorMutex);
    s_Decryptor->Reset(decryptor);
    s_DecryptorInitialized = true;
}

CRef<CSDB_Decryptor> CSDB_ConnectionParam::GetGlobalDecryptor(void)
{
    CFastMutexGuard GUARD(s_DecryptorMutex);
#ifdef HAVE_LIBCONNEXT
    if ( !s_DecryptorInitialized ) {
        s_Decryptor->Reset(new CSDB_Decryptor);
        s_DecryptorInitialized = true;
    }
#endif
    return *s_Decryptor;
}


string
CSDB_ConnectionParam::ComposeUrl(TComposeUrlFlags flags) const
{
    if ((flags & fThrowIfIncomplete) != 0
        &&  (m_Url.GetHost().empty()  ||  m_Url.GetUser().empty()
             ||  (m_Url.GetPassword().empty()  &&  Get(ePasswordFile).empty())
             ||  m_Url.GetPath().empty()  ||  m_Url.GetPath() == "/"))
    {
        NCBI_THROW(CSDB_Exception, eURLFormat,
                   "Connection parameters miss at least one essential part"
                   " (host, user, password, or database [as \"path\"]): "
                   + m_Url.ComposeUrl(CUrlArgs::eAmp_Char));
    }

    if ((flags & fHidePassword) != 0  &&  !m_Url.GetPassword().empty()) {
        CUrl redactee = m_Url;
        redactee.SetPassword("(redacted)");
        return redactee.ComposeUrl(CUrlArgs::eAmp_Char);
    }
    return m_Url.ComposeUrl(CUrlArgs::eAmp_Char);
}

bool CSDB_ConnectionParam::IsEmpty(void) const
{
    // Can't use m_Url.IsEmpty(), because the URL will still have a
    // scheme ("dbapi").
    if ( !m_Url.GetUser().empty()  ||  !m_Url.GetPassword().empty()
        ||  !m_Url.GetHost().empty()  ||  !m_Url.GetPort().empty()
        ||  !m_Url.GetPath().empty() ) {
        return false;
    } else if ( !m_Url.HaveArgs() ) {
        return true;
    }

    ITERATE (CUrlArgs::TArgs, it, m_Url.GetArgs().GetArgs()) {
        if ( !it->value.empty() ) {
            return false;
        }
    }
    return true;
}

CSDB_ConnectionParam&
CSDB_ConnectionParam::Set(const CSDB_ConnectionParam& param,
                          TSetFlags flags)
{
    for (int i = eUsername;  i <= eArgsString;  ++i) {
        EParam ii = static_cast<EParam>(i);
        Set(ii, param.Get(ii), flags);
    }
    return *this;
}

void CSDB_ConnectionParam::x_FillParamMap(void)
{
    static const char* kDefault = "default";
    m_ParamMap.clear();

    impl::SDBConfParams conf_params;
    try {
        s_GetDBContext()->ReadDBConfParams(m_Url.GetHost(), &conf_params);
    } SDBAPI_CATCH_LOWLEVEL()

#define COPY_PARAM_EX(en, gn, fn) \
    if (conf_params.Is##gn##Set()) \
        m_ParamMap[e##en] = conf_params.fn
#define COPY_PARAM(en, fn) COPY_PARAM_EX(en, en, fn)
#define COPY_BOOL_PARAM(en, gn, fn) \
    if (conf_params.Is##gn##Set()) \
        m_ParamMap[e##en] = conf_params.fn.empty() ? "default" \
            : NStr::StringToBool(conf_params.fn) ? "true" : "false"
#define COPY_NUM_PARAM(en, gn, fn) \
    if (conf_params.Is##gn##Set()) \
        m_ParamMap[e##en] = conf_params.fn.empty() ? kDefault : conf_params.fn

    COPY_PARAM_EX(Service, Server, server);

    COPY_PARAM(Username, username);
    COPY_PARAM(Password, password);
    COPY_PARAM(Port,     port);
    COPY_PARAM(Database, database);
    COPY_PARAM(PasswordFile, password_file);
    COPY_PARAM_EX(PasswordKeyID, PasswordKey, password_key_id);

    COPY_NUM_PARAM (LoginTimeout,    LoginTimeout, login_timeout);
    COPY_NUM_PARAM (IOTimeout,       IOTimeout,    io_timeout);
    COPY_BOOL_PARAM(ExclusiveServer, SingleServer, single_server);
    COPY_BOOL_PARAM(UseConnPool,     Pooled,       is_pooled);
    COPY_NUM_PARAM (ConnPoolMinSize, PoolMinSize,  pool_minsize);
    COPY_NUM_PARAM (ConnPoolMaxSize, PoolMaxSize,  pool_maxsize);
    COPY_NUM_PARAM (ConnPoolIdleTime, PoolIdleTime, pool_idle_time);
    COPY_NUM_PARAM (ConnPoolWaitTime, PoolWaitTime, pool_wait_time);
    COPY_BOOL_PARAM(ConnPoolAllowTempOverflow, PoolAllowTempOverflow,
                    pool_allow_temp_overflow);

#undef COPY_PARAM_EX
#undef COPY_PARAM
#undef COPY_BOOL_PARAM
#undef COPY_NUM_PARAM

    if ( !conf_params.args.empty() ) {
        m_ParamMap[eArgsString] = conf_params.args;
    }

    // Check for overridden settings
    ITERATE (TParamMap, it, m_ParamMap) {
        string s = Get(it->first, eWithoutOverrides);
        if ( !s.empty() ) {
            x_ReportOverride(x_GetName(it->first), s, it->second);
        }
    }

    if (conf_params.IsPasswordSet()  &&  conf_params.IsPasswordFileSet()) {
        NCBI_THROW(CSDB_Exception, eWrongParams,
                   '[' + m_Url.GetHost() + ".dbservice] password and"
                   " password_file parameters are mutually exclusive.");
    }
}

string CSDB_ConnectionParam::x_GetPassword() const
{
    string password;

    // Account for possible indirection.  Formal search order:
    // 1. Password directly in configuration file.
    // 2. Password filename in configuration file.
    // 3. Password filename in URL.
    // 4. Password directly in URL.
    // However, if either the configuration file or the URL supplies
    // both a direct password and a password filename, SDBAPI will
    // throw an exception, so it doesn't matter that 3 and 4 are
    // backward relative to 1 and 2.
    TParamMap::const_iterator it = m_ParamMap.find(ePassword);
    if (it != m_ParamMap.end()) {
        password = it->second;
    } else {
        string pwfile = Get(ePasswordFile, eWithOverrides);
        if (pwfile.empty()) {
            password = Get(ePassword);
        } else {
            CNcbiIfstream in(pwfile.c_str(), IOS_BASE::in | IOS_BASE::binary);
            if ( !in ) {
                NCBI_THROW(CSDB_Exception, eNotExist, // eWrongParams?
                           "Unable to open password file " + pwfile + ": " +
                           NCBI_ERRNO_STR_WRAPPER(NCBI_ERRNO_CODE_WRAPPER()));
            }
            NcbiGetline(in, password, "\r\n");
        }
    }
    
    // Account for possible encryption.
    string key_id = Get(ePasswordKeyID, eWithOverrides);
    if ( !key_id.empty() ) {
        CRef<CSDB_Decryptor> decryptor = GetGlobalDecryptor();
        if (decryptor.NotEmpty()) {
            password = decryptor->Decrypt(password, key_id);
            ITERATE (string, pit, password) {
                if ( !isprint((unsigned char)*pit) ) {
                    NCBI_THROW(CSDB_Exception, eWrongParams,
                               "Invalid character in supposedly decrypted"
                               " password.");
                }
            }
        }
    }

    return password;
}

void
CSDB_ConnectionParam::x_FillLowerParams(CDBConnParamsBase* params) const
{
    params->SetServerName  (Get(eService,  eWithOverrides));
    params->SetUserName    (Get(eUsername, eWithOverrides));
    params->SetPassword    (x_GetPassword());
    params->SetDatabaseName(Get(eDatabase, eWithOverrides));

    string port = Get(ePort, eWithOverrides);
    if ( !port.empty() ) {
        params->SetPort(NStr::StringToInt(port));
    }

    params->SetParam("login_timeout", Get(eLoginTimeout,    eWithOverrides));
    params->SetParam("io_timeout",    Get(eIOTimeout,       eWithOverrides));
    x_FillBoolParam(params, "single_server", eExclusiveServer);
    x_FillBoolParam(params, "is_pooled",     eUseConnPool);
    params->SetParam("pool_minsize",  Get(eConnPoolMinSize, eWithOverrides));
    params->SetParam("pool_maxsize",  Get(eConnPoolMaxSize, eWithOverrides));
    params->SetParam("pool_idle_time", Get(eConnPoolIdleTime, eWithOverrides));
    params->SetParam("pool_wait_time", Get(eConnPoolWaitTime, eWithOverrides));
    x_FillBoolParam(params, "pool_allow_temp_overflow",
                    eConnPoolAllowTempOverflow);

    // Generic named parameters.  The historic version of this logic
    // had two quirks, which I [AMU] have not carried over:
    // * A setting found in the configuration file counted only when
    //   the original URL mentioned the same parameter.
    // * If a key appeared more than once in either list, the old code
    //   favored the last appearance in the URL but the first one in
    //   the configuration file.  It now always favors the last one.
    typedef vector<CUrlArgs*> TAllArgs;
    TAllArgs all_args;
    all_args.push_back(&m_Url.GetArgs());
    
    auto_ptr<CUrlArgs> conf_args;
    {{
        TParamMap::const_iterator it = m_ParamMap.find(eArgsString);
        if (it != m_ParamMap.end()) {
            conf_args.reset(new CUrlArgs(it->second));
            all_args.push_back(conf_args.get());
        }
    }}

    ITERATE (TAllArgs, ait, all_args) {
        const CUrlArgs::TArgs& args = (*ait)->GetArgs();
        ITERATE (CUrlArgs::TArgs, uit, args) {
            const string& param_name  = uit->name;
            const string& param_value = uit->value;
            if ( !x_IsKnownArg(param_name) ) {
                params->SetParam(param_name, param_value);
            }
        }
    }

    params->SetParam("do_not_read_conf", "true");
}

void
CSDB_ConnectionParam::x_FillBoolParam(CDBConnParamsBase* params,
                                      const string& name, EParam id) const
{
    string value = Get(id, eWithOverrides);
    if ( !value.empty()  &&  value != "default") {
        value = NStr::BoolToString(NStr::StringToBool(value));
    }
    params->SetParam(name, value);
}


void CSDB_ConnectionParam::x_ReportOverride(const CTempString& name,
                                            CTempString code_value,
                                            CTempString reg_value) const
{
    if (code_value == reg_value) {
        return;
    } else if (name == x_GetName(eArgsString)) {
        // Analyze individually
        typedef map<string, string> TConfMap;
        TConfMap conf_map;
        CUrlArgs conf_args(reg_value);
        ITERATE (CUrlArgs::TArgs, it, conf_args.GetArgs()) {
            if ( !x_IsKnownArg(it->name) ) {
                conf_map[it->name] = it->value;
            }
        }
        ITERATE (CUrlArgs::TArgs, uait, m_Url.GetArgs().GetArgs()) {
            TConfMap::const_iterator cmit = conf_map.find(uait->name);
            if (cmit != conf_map.end()) {
                x_ReportOverride(uait->name, uait->value, cmit->second);
            }
        }
    } else if (name == x_GetName(eService)) {
        ERR_POST_X(18, Info << "Using privately configured service alias "
                   << code_value << " -> " << reg_value);
    } else {
        if (name == x_GetName(ePassword)) {
            code_value = "(redacted)";
            reg_value  = "(redacted)";
        }
        ERR_POST_X(17, Warning << "Ignoring program-defined " << name
                   << " parameter value " << code_value << " for "
                   << m_Url.GetHost() << " in favor of configured value "
                   << reg_value);
    }
}


bool
CSDBAPI::Init(void)
{
    CNcbiApplication* app = CNcbiApplication::Instance();
    if (!app)
        return true;
    const IRegistry& reg = app->GetConfig();
    bool result = true;
    list<string> sections;
    reg.EnumerateSections(&sections);
    ITERATE(list<string>, it, sections) {
        const string& name = *it;
        if (name.size() <= 10
            ||  NStr::CompareCase(name, name.size() - 10, 10, ".dbservice") != 0)
        {
            continue;
        }
        impl::CDriverContext* ctx = s_GetDBContext();
        CDBConnParamsBase lower_params;
        CSDB_ConnectionParam conn_params(name.substr(0, name.size() - 10));
        conn_params.x_FillLowerParams(&lower_params);
        if (lower_params.GetParam("is_pooled") == "true") {
            result &= ctx->SatisfyPoolMinimum(lower_params);
        }
    }
    return result;
}


void CSDBAPI::SetApplicationName(const CTempString& name)
{
    s_GetDBContext()->SetApplicationName(name);
}

string CSDBAPI::GetApplicationName(void)
{
    return s_GetDBContext()->GetApplicationName();
}


enum EMirrorConnState {
    eConnInitializing,
    eConnNotConnected,
    eConnEstablished
};

struct SMirrorServInfo
{
    string               server_name;
    AutoPtr<IConnection> conn;
    EMirrorConnState     state;
};

typedef list< AutoPtr<SMirrorServInfo> >  TMirrorServList;

struct SMirrorInfo
{
    AutoPtr<CDBConnParamsBase>  conn_params;
    TMirrorServList             servers;
    string                      master;
};

typedef map<string, SMirrorInfo>  TMirrorsDataMap;
static CSafeStatic<TMirrorsDataMap> s_MirrorsData;


class CUpdMirrorServerParams: public CDBConnParamsBase
{
public:
    CUpdMirrorServerParams(const CDBConnParams& other)
        : m_Other(other)
    {}

    ~CUpdMirrorServerParams(void)
    {}

    virtual Uint4 GetProtocolVersion(void) const
    {
        return m_Other.GetProtocolVersion();
    }

    virtual EEncoding GetEncoding(void) const
    {
        return m_Other.GetEncoding();
    }

    virtual string GetServerName(void) const
    {
        return CDBConnParamsBase::GetServerName();
    }

    virtual string GetDatabaseName(void) const
    {
        return kEmptyStr;
    }

    virtual string GetUserName(void) const
    {
        return m_Other.GetUserName();
    }

    virtual string GetPassword(void) const
    {
        return m_Other.GetPassword();
    }

    virtual EServerType GetServerType(void) const
    {
        return m_Other.GetServerType();
    }

    virtual Uint4 GetHost(void) const
    {
        return m_Other.GetHost();
    }

    virtual Uint2 GetPort(void) const
    {
        return m_Other.GetPort();
    }

    virtual CRef<IConnValidator> GetConnValidator(void) const
    {
        return m_Other.GetConnValidator();
    }

    virtual string GetParam(const string& key) const
    {
        string result(CDBConnParamsBase::GetParam(key));
        if (result.empty())
            return m_Other.GetParam(key);
        else
            return result;
    }

private:
    const CDBConnParams& m_Other;
};


CSDBAPI::EMirrorStatus
CSDBAPI::UpdateMirror(const string& dbservice,
                      list<string>* servers /* = NULL */,
                      string*       error_message /* = NULL */)
{
    if (dbservice.empty()) {
        NCBI_THROW(CSDB_Exception, eWrongParams,
                   "Mirrored database service name cannot be empty");
    }

    bool first_execution = false;
    SMirrorInfo& mir_info = (*s_MirrorsData)[dbservice];
    if (!mir_info.conn_params.get()) {
        mir_info.conn_params.reset(new CDBConnParamsBase);
        first_execution = true;
    }
    CDBConnParamsBase& conn_params = *mir_info.conn_params.get();
    TMirrorServList& serv_list = mir_info.servers;

    ds_init.Get();
    IDBConnectionFactory* i_factory
                = CDbapiConnMgr::Instance().GetConnectionFactory().GetPointer();
    if (conn_params.GetServerName().empty()) {
        first_execution = true;
        CSDB_ConnectionParam sdb_params(dbservice);
        sdb_params.x_FillLowerParams(&conn_params);
        if (conn_params.GetParam("single_server") != "true") {
            NCBI_THROW(CSDB_Exception, eInconsistent,
                       "UpdateMirror cannot be used when configuration file "
                       "doesn't have exclusive_server=true (for " + dbservice
                       + ')');
        }
        CDBConnectionFactory* factory
                              = dynamic_cast<CDBConnectionFactory*>(i_factory);
        if (!factory) {
            NCBI_THROW(CSDB_Exception, eInconsistent,
                       "UpdateMirror cannot work with non-standard "
                       "connection factory");
        }
    }
    if (servers)
        servers->clear();
    if (error_message)
        error_message->clear();

    CDBConnectionFactory* factory = static_cast<CDBConnectionFactory*>(i_factory);
    string service_name(conn_params.GetServerName());
    string db_name(conn_params.GetDatabaseName());
    bool need_reread_servers = false;
    bool servers_reread = false;
    bool has_master = false;
    int cnt_switches = 0;

    do {
        if (serv_list.empty()) {
            list<string> servers;
            factory->GetServersList(db_name, service_name, &servers);
            ITERATE(list<string>, it, servers) {
                serv_list.push_back(new SMirrorServInfo());
                serv_list.back()->server_name = *it;
                serv_list.back()->state = eConnInitializing;
            }
            servers_reread = true;
        }
        else if (need_reread_servers) {
            list<string> new_serv_info;
            factory->GetServersList(db_name, service_name, &new_serv_info);
            ERASE_ITERATE(TMirrorServList, old_it, serv_list) {
                bool found = false;
                ITERATE(list<string>, it, new_serv_info) {
                    if (*it == (*old_it)->server_name) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    serv_list.erase(old_it);
                }
            }
            ITERATE(list<string>, new_it, new_serv_info) {
                bool found = false;
                ITERATE(TMirrorServList, old_it, serv_list) {
                    if ((*old_it)->server_name == *new_it) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    serv_list.push_back(new SMirrorServInfo());
                    serv_list.back()->server_name = *new_it;
                    serv_list.back()->state = eConnInitializing;
                }
            }
            servers_reread = true;
        }
        if (serv_list.empty()) {
            factory->WorkWithSingleServer("", service_name, kEmptyStr);
            ERR_POST_X(3, "No servers for service '" << service_name
                          << "' are available");
            if (error_message) {
                *error_message = "No servers for service '" + service_name
                                 + "' are available";
            }
            return eMirror_Unavailable;
        }

        for (TMirrorServList::iterator it = serv_list.end(); it != serv_list.begin(); )
        {
            --it;
            AutoPtr<IConnection>& conn  = (*it)->conn;
            const string& server_name   = (*it)->server_name;
            EMirrorConnState& state     = (*it)->state;
            if (!conn.get()) {
                conn = s_GetDataSource()->CreateConnection(eTakeOwnership);
            }
            if (!conn->IsAlive()) {
                if (state == eConnEstablished) {
                    ERR_POST_X(7, "Connection to server '" << server_name
                                  << "' for service '" << service_name
                                  << "' is lost.");
                    state = eConnNotConnected;
                }
                CUpdMirrorServerParams serv_params(conn_params);
                serv_params.SetServerName(server_name);
                serv_params.SetParam("is_pooled", "false");
                serv_params.SetParam("do_not_dispatch", "true");
                try {
                    conn->Close();
                    conn->Connect(serv_params);
                    if (state == eConnNotConnected) {
                        ERR_POST_X(8, "Connection to server '" << server_name
                                      << "' for service '" << service_name
                                      << "' is restored.");
                    }
                    state = eConnEstablished;
                }
                catch (CDB_Exception& /*ex*/) {
                    //ERR_POST_X(1, Info << "Error connecting to the server '"
                    //                   << server_name << "': " << ex);
                    if (state == eConnInitializing) {
                        ERR_POST_X(9, "Cannot establish connection to server '"
                                   << server_name << "' for service '"
                                   << service_name << "'.");
                        state = eConnNotConnected;
                    }
                    conn->Close();
                    need_reread_servers = true;
                }
            }
            bool success = false;
            if (conn->IsAlive()) {
                try {
                    AutoPtr<IStatement> stmt(conn->GetStatement());
                    stmt->ExecuteUpdate("use " + db_name);
                    success = true;
                }
                catch (CDB_Exception& /*ex*/) {
                    // success is already false
                    //ERR_POST_X(2, Info << "Check for mirror principal state failed: "
                    //                   << ex);
                }
            }
            if (!success  &&  server_name == mir_info.master) {
                ERR_POST_X(4, Warning << "Master server '" << server_name
                              << "' for service '" << service_name
                              << "' became inaccessible.");

                factory->WorkWithSingleServer("", service_name, kEmptyStr);
                s_GetDBContext()->CloseConnsForPool(conn_params.GetParam("pool_name"));
                mir_info.master.clear();
                need_reread_servers = true;
            }
            else if (success  &&  server_name != mir_info.master) {
                ERR_POST_X(5, Warning << "Mirror server '" << server_name
                              << "' for service '" << service_name
                              << "' became accessible. Switching the master.");
                factory->WorkWithSingleServer("", service_name, server_name);
                s_GetDBContext()->CloseConnsForPool(conn_params.GetParam("pool_name"));
                s_GetDBContext()->SatisfyPoolMinimum(conn_params);

                has_master = true;
                mir_info.master = server_name;
                serv_list.push_front(*it);
                TMirrorServList::iterator it_del = it++;
                serv_list.erase(it_del);
                need_reread_servers = true;

                if (++cnt_switches == 10) {
                    NCBI_THROW(CSDB_Exception, eInconsistent,
                               "Mirror database switches too frequently or "
                               "it isn't mirrored: " + service_name);
                }
            }
            else if (success  &&  server_name == mir_info.master) {
                has_master = true;
            }
        }
    }
    while (need_reread_servers  &&  !servers_reread);

    if (first_execution  &&  !has_master) {
        ERR_POST_X(10, "No master database is accessible for service '"
                       << service_name << "'.");
        factory->WorkWithSingleServer("", service_name, kEmptyStr);
        s_GetDBContext()->CloseConnsForPool(conn_params.GetParam("pool_name"));
    }

    if (servers) {
        ITERATE(TMirrorServList, it, serv_list) {
            servers->push_back((*it)->server_name);
        }
    }
    if (!has_master) {
        if (error_message)
            *error_message = "All database instances are inaccessible";
        return eMirror_Unavailable;
    }
    return cnt_switches == 0? eMirror_Steady: eMirror_NewMaster;
}



inline
CConnHolder::CConnHolder(IConnection* conn)
    : m_Conn(conn),
      m_DefaultTimeout(0),
      m_HasCustomTimeout(false),
      m_CntOpen(0),
      m_Context(new CDB_Exception::SContext)
{
    if (conn != NULL) {
        m_DefaultTimeout         = conn->GetTimeout();
        m_Context->server_name   = conn->GetCDB_Connection()->ServerName();
        m_Context->username      = conn->GetCDB_Connection()->UserName();
        m_Context->database_name = conn->GetDatabase();
    }
}

CConnHolder::~CConnHolder(void)
{
    delete m_Conn;
}

inline IConnection*
CConnHolder::GetConn(void) const
{
    return m_Conn;
}

inline void
CConnHolder::AddOpenRef(void)
{
    ++m_CntOpen;
}

inline void
CConnHolder::CloseRef(void)
{
    if (--m_CntOpen == 0) {
        m_Conn->Close();
    }
}

void CConnHolder::SetTimeout(const CTimeout& timeout)
{
    if (timeout.IsDefault()) {
        ResetTimeout();
    } else if (m_CntOpen > 0) {
        unsigned int sec, nanosec;
        timeout.GetNano(&sec, &nanosec);
        if (nanosec > 0  &&  sec != kMax_UInt) { // round up as appropriate
            ++sec;
        }
        m_HasCustomTimeout = true;
        m_Conn->SetTimeout(sec);
    }
}

void CConnHolder::ResetTimeout(void)
{
    if (m_HasCustomTimeout  &&  m_CntOpen > 0) {
        m_Conn->SetTimeout(m_DefaultTimeout);
    }
    m_HasCustomTimeout = false;
}

inline
const CDB_Exception::SContext& CConnHolder::GetContext(void) const
{
    return *m_Context;
}


inline
CDatabaseImpl::CDatabaseImpl(const CSDB_ConnectionParam& params)
    : m_IsOpen(false)
{
    CDBConnParamsBase lower_params;
    params.x_FillLowerParams(&lower_params);
    AutoPtr<IConnection> conn = s_GetDataSource()->CreateConnection();
    conn->Connect(lower_params);
    m_Conn.Reset(new CConnHolder(conn.release()));
    m_IsOpen = true;
    m_Conn->AddOpenRef();
}

inline
CDatabaseImpl::CDatabaseImpl(const CDatabaseImpl& other)
    : m_Conn(other.m_Conn),
      m_IsOpen(other.m_IsOpen)
{
    if (m_IsOpen)
        m_Conn->AddOpenRef();
}

inline void
CDatabaseImpl::Close(void)
{
    if (m_IsOpen) {
        m_IsOpen = false;
        m_Conn->CloseRef();
    }
}

inline
CDatabaseImpl::~CDatabaseImpl(void)
{
    try {
        Close();
    }
    STD_CATCH_ALL_X(11, "CDatabaseImpl::~CDatabaseImpl")
}

inline bool
CDatabaseImpl::IsOpen(void) const
{
    return m_IsOpen;
}

inline IConnection*
CDatabaseImpl::GetConnection(void)
{
    return m_Conn->GetConn();
}

inline void
CDatabaseImpl::SetTimeout(const CTimeout& timeout)
{
    m_Conn->SetTimeout(timeout);
}

inline void
CDatabaseImpl::ResetTimeout(void)
{
    m_Conn->ResetTimeout();
}

inline
const CDB_Exception::SContext&
CDatabaseImpl::GetContext(void) const
{
    return m_Conn->GetContext();
}


CDatabase::CDatabase(void)
{}

CDatabase::CDatabase(const CSDB_ConnectionParam& params)
    : m_Params(params)
{}

CDatabase::CDatabase(const string& url_string)
    : m_Params(url_string)
{}

CDatabase::CDatabase(const CDatabase& db)
{
    operator= (db);
}

CDatabase&
CDatabase::operator= (const CDatabase& db)
{
    m_Params = db.m_Params;
    m_Impl.Reset(db.m_Impl? new CDatabaseImpl(*db.m_Impl): NULL);
    return *this;
}

CDatabase::~CDatabase(void)
{
    if (m_Impl) {
        try {
            m_Impl->Close();
        }
        STD_CATCH_ALL_X(12, "CDatabase::~CDatabase");
    }
}

void
CDatabase::Connect(void)
{
    try {
        if (m_Impl) {
            m_Impl->Close();
            m_Impl.Reset();
        }
        m_Impl.Reset(new CDatabaseImpl(m_Params));
    }
    SDBAPI_CATCH_LOWLEVEL()
}

void
CDatabase::Close(void)
{
    if (!m_Impl)
        return;
    try {
        m_Impl->Close();
    }
    SDBAPI_CATCH_LOWLEVEL()
    m_Impl.Reset();
}

bool
CDatabase::IsConnected(EConnectionCheckMethod check_method)
{
    if (m_Impl == NULL  ||  !m_Impl->IsOpen()) {
        return false;
    } else if (check_method == eNoCheck) {
        return true;
    }

    IConnection* conn = m_Impl->GetConnection();
    _ASSERT(conn != NULL);
    if ( !conn->IsAlive() ) {
        Close();
        return false;
    } else if (check_method == eFastCheck) {
        return true;
    }

    _ASSERT(check_method == eFullCheck);
    try {
        CQuery query = NewQuery("SELECT 1");
        query.Execute();
        query.RequireRowCount(1);
        CQuery::CRowIterator row = query.begin();
        bool ok = (row != query.end()  &&  row.GetTotalColumns() == 1
                   &&  row[1].AsInt4() == 1);
        query.VerifyDone();
        return ok;
    } catch (exception&) {
        Close();
        return false;
    }
}

CDatabase
CDatabase::Clone(void)
{
    if (!m_Impl) {
        NCBI_THROW(CSDB_Exception, eClosed,
                   "Database cannot be cloned if it's not connected");
    }

    CDatabase result(m_Params);
    result.Connect();
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

CBlobBookmark
CDatabase::NewBookmark(const string& table_name, const string& column_name,
                       const string& search_conditions,
                       CBlobBookmark::EBlobType column_type)
{
    if (!m_Impl) {
        NCBI_THROW(CSDB_Exception, eClosed,
                   "Cannot create bookmark when not connected");
    }

    CDB_ITDescriptor::ETDescriptorType desc_type;
    switch (column_type) {
    case CBlobBookmark::eText:   desc_type = CDB_ITDescriptor::eText;    break;
    case CBlobBookmark::eBinary: desc_type = CDB_ITDescriptor::eBinary;  break;
    default:                     desc_type = CDB_ITDescriptor::eUnknown; break;
    }

    auto_ptr<I_ITDescriptor> desc
        (new CDB_ITDescriptor(table_name, column_name, search_conditions,
                              desc_type));
                                                       
    CRef<CBlobBookmarkImpl> bm(new CBlobBookmarkImpl(m_Impl, desc.release()));
    return CBlobBookmark(bm);
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
      m_WriteStarted(false),
      m_Context(new CDB_Exception::SContext(db_impl->GetContext()))
{
    m_BI = db_impl->GetConnection()->GetBulkInsert(table_name);
    m_Context->extra_msg = "Bulk insertion into " + table_name;
}

CBulkInsertImpl::~CBulkInsertImpl(void)
{
    try {
        if (m_BI  &&  m_RowsWritten != 0) {
            m_BI->Complete();
        }
    }
    STD_CATCH_ALL_X(13, "Error destroying CBulkInsert");
    delete m_BI;
}

inline
const CDB_Exception::SContext& CBulkInsertImpl::x_GetContext(void) const
{
    return *m_Context;
}

void
CBulkInsertImpl::x_CheckCanWrite(int col)
{
    if (!m_BI) {
        SDBAPI_THROW(eClosed, "Cannot write into completed CBulkInsert");
    }
    if (!m_DBImpl->IsOpen()) {
        m_BI->Cancel();
        delete m_BI;
        m_BI = NULL;
        SDBAPI_THROW(eClosed,
                     "Cannot write into CBulkInsert when CDatabase was closed");
    }
    if (col != 0  &&  col > int(m_Cols.size())) {
        SDBAPI_THROW(eInconsistent,
                     "Too many values were written to CBulkInsert: "
                     + NStr::NumericToString(col) + " > "
                     + NStr::NumericToString(m_Cols.size()));
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
        SDBAPI_THROW(eStarted,
                     "Cannot bind columns when already started to insert");
    }
    if (col - 1 != int(m_Cols.size())) {
        SDBAPI_THROW(eNotInOrder,
                     "Cannot bind columns in CBulkInsert randomly");
    }
    m_Cols.push_back(CVariant(s_ConvertType(type)));
    if (type == eSDB_StringUCS2  ||  type == eSDB_TextUCS2) {
        m_Cols.back().SetBulkInsertionEnc(eBulkEnc_UCS2FromChar);
    }
}

inline void
CBulkInsertImpl::EndRow(void)
{
    x_CheckCanWrite(0);
    if (m_ColsWritten != int(m_Cols.size())) {
        SDBAPI_THROW(eInconsistent,
                     "Not enough values were written to CBulkInsert: "
                     + NStr::NumericToString(m_ColsWritten) + " != "
                     + NStr::NumericToString(m_Cols.size()));
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
    if (m_BI == NULL) {
        // Make idempotent, rather than letting x_CheckCanWrite throw.
        return;
    }
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
CBulkInsert::operator<<(const TStringUCS2& val)
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


// NB: x_InitBeforeExec resets many of these fields.
inline
CQueryImpl::CQueryImpl(CDatabaseImpl* db_impl)
    : m_DBImpl(db_impl),
      m_Stmt(NULL),
      m_CallStmt(NULL),
      m_CurRS(NULL),
      m_IgnoreBounds(true),
      m_HasExplicitMode(false),
      m_RSBeginned(false),
      m_RSFinished(true),
      m_Executed(false),
      m_ReportedWrongRowCount(false),
      m_IsSP(false),
      m_CurRSNo(0),
      m_CurRowNo(0),
      m_CurRelRowNo(0),
      m_MinRowCount(0),
      m_MaxRowCount(kMax_Auto),
      m_RowCount(-1),
      m_Status(-1),
      m_Context(new CDB_Exception::SContext(m_DBImpl->GetContext()))
{
    m_Stmt = db_impl->GetConnection()->GetStatement();
}

CQueryImpl::~CQueryImpl(void)
{
    try {
        x_Close();
    }
    STD_CATCH_ALL_X(6, "Error destroying CQuery");
    x_ClearAllParams();
    delete m_Stmt;
}

const CDB_Exception::SContext&
CQueryImpl::x_GetContext(void) const
{
    if ( !m_Context->extra_msg.empty() ) {
        return *m_Context;
    }

    CNcbiOstrstream oss;
    oss << (m_IsSP ? "RPC: " : "SQL: ");
    oss << m_Sql;
    if ( !m_Params.empty() ) {
        string delim;
        oss << "; input parameter(s): ";
        ITERATE (TParamsMap, it, m_Params) {
            oss << delim;
            oss << it->first << " = ";
            if (it->second.value == NULL  ||  it->second.value->IsNull()) {
                oss << "NULL";
            } else {
                oss << it->second.value->GetData()->GetLogString();
            }
            delim = ", ";
        }
    }
    m_Context->extra_msg = CNcbiOstrstreamToString(oss);
    return *m_Context;
}

void
CQueryImpl::x_CheckCanWork(bool need_rs /* = false */) const
{
    if (!m_DBImpl->IsOpen()) {
        SDBAPI_THROW(eClosed,
                     "CQuery is not operational because CDatabase was closed");
    }
    if (need_rs  &&  !m_CurRS
        &&  !const_cast<CQueryImpl*>(this)->HasMoreResultSets())
    {
        SDBAPI_THROW(eClosed, "CQuery is closed or never executed");
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
                   "Parameter '" + string(name) + "' doesn't exist.  "
                   + x_GetContext());
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

void
CQueryImpl::x_ClearAllParams(void)
{
    ITERATE(TParamsMap, it, m_Params) {
        const SQueryParamInfo& info = it->second;
        delete info.field;
        delete info.value;
    }
    m_Params.clear();
}

inline void
CQueryImpl::ClearParameters(void)
{
    x_CheckCanWork();
    x_ClearAllParams();
    m_Context->extra_msg.clear();
}

inline void
CQueryImpl::SetSql(CTempString sql)
{
    x_CheckCanWork();

    m_Sql = sql.empty() ? CTempString(" ") : sql;
    m_Executed = false;
    m_IsSP = false;
}

void
CQueryImpl::x_Close(void)
{
    m_DBImpl->ResetTimeout();
    if (m_CurRS != NULL) {
        try {
            VerifyDone(CQuery::eAllResultSets);
        } catch (CSDB_Exception& e) {
            ERR_POST_X(14, Warning << e);
        }
        if (m_CurRSNo != 0) {
            _TRACE(m_CurRowNo << " row(s) from query.");
        }
    }

    delete m_CurRS;
    m_CurRS = NULL;
    if (m_CallStmt) {
        if (m_DBImpl->IsOpen())
            m_CallStmt->PurgeResults();
        delete m_CallStmt;
        m_CallStmt = NULL;
    }
    if (m_DBImpl->IsOpen()) {
        m_Stmt->PurgeResults();
        m_Stmt->Close();
    }
}

inline void
CQueryImpl::x_InitBeforeExec(void)
{
    m_IgnoreBounds = true;
    m_HasExplicitMode = false;
    m_RSBeginned = false;
    m_RSFinished = true;
    m_ReportedWrongRowCount = false;
    m_CurRSNo = 0;
    m_CurRowNo = 0;
    m_CurRelRowNo = 0;
    m_RowCount = 0;
    m_MinRowCount = 0;
    m_MaxRowCount = kMax_Auto;
    m_Status = -1;
}

inline void
CQueryImpl::Execute(const CTimeout& timeout)
{
    if (m_IsSP  ||  m_Sql.empty()) {
        SDBAPI_THROW(eInconsistent, "No statement to execute.");
    }

    x_CheckCanWork();

    try {
        x_Close();
        x_InitBeforeExec();

        m_Stmt->ClearParamList();
        ITERATE(TParamsMap, it, m_Params) {
            const SQueryParamInfo& info = it->second;
            m_Stmt->SetParam(*info.value, it->first);
        }
        if ( !timeout.IsDefault() ) {
            m_DBImpl->SetTimeout(timeout);
        }
        m_Executed = true;
        m_Stmt->SendSql(m_Sql);
        HasMoreResultSets();
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline void
CQueryImpl::ExecuteSP(CTempString sp, const CTimeout& timeout)
{
    x_CheckCanWork();

    m_Sql  = sp;
    m_IsSP = true;

    try {
        x_Close();
        x_InitBeforeExec();

        m_CallStmt = m_DBImpl->GetConnection()->GetCallableStatement(sp);
        ITERATE(TParamsMap, it, m_Params) {
            const SQueryParamInfo& info = it->second;
            if (info.type == eSP_In)
                m_CallStmt->SetParam(*info.value, it->first);
            else
                m_CallStmt->SetOutputParam(*info.value, it->first);
        }
        if ( !timeout.IsDefault() ) {
            m_DBImpl->SetTimeout(timeout);
        }
        m_Executed = true;
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
    m_HasExplicitMode = true;
    x_CheckRowCount();
}

inline unsigned int
CQueryImpl::GetResultSetNo(void) const
{
    x_CheckCanWork();
    return m_CurRSNo;
}

inline unsigned int
CQueryImpl::GetRowNo(CQuery::EHowMuch how_much) const
{
    x_CheckCanWork();
    if (m_IgnoreBounds  ||  how_much == CQuery::eAllResultSets) {
        return m_CurRowNo;
    } else {
        return m_CurRelRowNo;
    }
}

inline int
CQueryImpl::GetRowCount(void) const
{
    x_CheckCanWork();
    if ( !IsFinished(CQuery::eAllResultSets) ) {
        NCBI_THROW(CSDB_Exception, eInconsistent,
                   "CQuery::GetRowCount called with some results still"
                   " unread.  " + x_GetContext());
    } else {
        return m_RowCount;
    }
}

inline int
CQueryImpl::GetStatus(void) const
{
    x_CheckCanWork();
    if ( !IsFinished(CQuery::eAllResultSets) ) {
        NCBI_THROW(CSDB_Exception, eInconsistent,
                   "CQuery::GetStatus called with some results still"
                   " unread.  " + x_GetContext());
    } else {
        return m_Status;
    }
}

void CQueryImpl::x_CheckRowCount(void)
{
    if (m_ReportedWrongRowCount) {
        return;
    }

    unsigned int n;
    if ( !m_IgnoreBounds ) {
        n = m_CurRelRowNo;
#if 0 // Counts rows affected by INSERT and the like.
    } else if (m_RowCount > 0) {
        n = m_RowCount;
#endif
    } else {
        n = m_CurRowNo;
    }

    if (n > m_MaxRowCount) {
        m_ReportedWrongRowCount = true;
        NCBI_THROW(CSDB_Exception, eWrongParams,
                   "Too many rows returned (limited to "
                   + NStr::NumericToString(m_MaxRowCount) + ").  "
                   + x_GetContext());
    } else if (m_RSFinished  &&  n < m_MinRowCount) {
        m_ReportedWrongRowCount = true;
        NCBI_THROW(CSDB_Exception, eWrongParams,
                   "Not enough rows returned ("
                   + NStr::NumericToString(m_CurRowNo) + '/'
                   + NStr::NumericToString(m_MinRowCount) + ").  "
                   + x_GetContext());
    }
}

inline bool
CQueryImpl::x_Fetch(void)
{
    try {
        if (m_CurRS->Next()) {
            ++m_CurRowNo;
            ++m_CurRelRowNo;
            x_CheckRowCount();
            return true;
        }
        else {
            m_RSFinished = true;
            _TRACE(m_CurRelRowNo << " row(s) in set.");
            x_CheckRowCount();
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
                delete m_CurRS;
                m_CurRS = NULL;
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
                m_CurRS = NULL;
                break;
            case eDB_RowResult:
                if (++m_CurRSNo == 2  &&  !m_HasExplicitMode ) {
                    ERR_POST_X(16, Info
                               << "Multiple SDBAPI result sets found, but"
                               " neither SingleSet nor MultiSet explicitly"
                               " requested.  Now defaulting to SingleSet.  "
                               << x_GetContext());
                }
                if ( !m_IgnoreBounds ) {
                    m_ReportedWrongRowCount = false;
                }
                m_CurRelRowNo = 0;
                x_InitRSFields();
                return true;
            case eDB_ComputeResult:
            case eDB_CursorResult:
                _ASSERT(false);
            }
        }
    }
    SDBAPI_CATCH_LOWLEVEL()

    m_DBImpl->ResetTimeout();

    _TRACE(m_CurRowNo << " row(s) from query.");

    if (m_CallStmt) {
        m_Status = m_CallStmt->GetReturnStatus();
    }
    if (m_RowCount == 0) {
        if (m_CurRowNo != 0) {
            m_RowCount = m_CurRowNo;
            // m_CurRowNo = 0;
        }
        else {
            m_RowCount = stmt->GetRowCount();
        }
    }
    m_CurRSNo = 0;
    m_RSFinished = true;
    return false;
}

void
CQueryImpl::BeginNewRS(void)
{
    x_CheckCanWork();
    bool has_more_rs = HasMoreResultSets();
    if ( !has_more_rs  &&  !m_Executed ) {
        // Check has_more_rs in addition to m_Executed because SetSql
        // resets the latter.
        Execute(CTimeout(CTimeout::eDefault));
        has_more_rs = HasMoreResultSets();
    }
    if ( !has_more_rs ) {
        if (m_IgnoreBounds  &&  m_CurRowNo == 0) {
            // OK to have no results whatsoever in SingleSet mode.
            return;
        }
        NCBI_THROW(CSDB_Exception, eClosed,
                   "All result sets in CQuery were already iterated through.  "
                   + x_GetContext());
    }
    if (m_RSFinished) {
        // Complete recovering from a premature call to GetStatus or
        // GetRowCount.
        m_RSFinished  = false;
        m_CurRelRowNo = 0;
        x_InitRSFields();
    }
    while (HasMoreResultSets()  &&  !x_Fetch()  &&  m_IgnoreBounds)
        m_RSBeginned = true;
    m_RSBeginned = true;
}

inline void
CQueryImpl::PurgeResults(void)
{
    x_CheckCanWork();
    m_HasExplicitMode = true; // Doesn't particularly matter at this point!
    bool has_more = true;
    while (has_more) {
        try {
            if ((has_more = HasMoreResultSets())) {
                BeginNewRS();
            }
        } catch (CSDB_Exception& e) {
            while (has_more) {
                try {
                    if ((has_more = HasMoreResultSets())) {
                        BeginNewRS();
                    }
                } catch (CSDB_Exception& e2) {
                    has_more = true;
                    // Not technically the correct relationship, but by far
                    // the simplest way to consolidate the exceptions.
                    e.AddPrevious(&e2);
                }
            }
            throw;
        }
    }
}

inline int
CQueryImpl::GetTotalColumns(void) const
{
    x_CheckCanWork(true);
    try {
        return m_CurRS->GetTotalColumns();
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline string
CQueryImpl::GetColumnName(unsigned int col) const
{
    x_CheckCanWork(true);
    try {
        return m_CurRS->GetMetaData()->GetName(col);
    }
    SDBAPI_CATCH_LOWLEVEL()
}

inline ESDB_Type
CQueryImpl::GetColumnType(unsigned int col) const
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

inline void
CQueryImpl::RequireRowCount(unsigned int min_rows, unsigned int max_rows)
{
    if ( !m_Executed ) {
        SDBAPI_THROW(eInconsistent,
                     "RequireRowCount must follow Execute or ExecuteSP,"
                     " which reset any requirements.");
    }
    if (min_rows > max_rows) {
        SDBAPI_THROW(eWrongParams,
                     "Inconsistent row-count constraints: "
                     + NStr::NumericToString(min_rows) + " > "
                     + NStr::NumericToString(max_rows));
    }
    x_CheckCanWork();
    _TRACE("RequireRowCount(" << min_rows << ", " << max_rows << ')');
    m_MinRowCount = min_rows;
    m_MaxRowCount = max_rows;
    if (m_CurRS != NULL) {
        x_CheckRowCount();
    }
}

inline void
CQueryImpl::VerifyDone(CQuery::EHowMuch how_much)
{
    x_CheckCanWork();

    bool missed_results = false;
    bool want_all = m_IgnoreBounds  ||  how_much == CQuery::eAllResultSets;

    for (;;) {
        try {
            if (m_RSFinished) {
                x_CheckRowCount();
            } else if (m_CurRS) {
                // Tolerate having read just enough rows, unless that number
                // was zero but not necessarily required to be.
                missed_results
                    = x_Fetch()  ||  ( !m_RSBeginned  &&  m_MaxRowCount > 0);
            }
        } catch (...) {
            if (want_all) {
                PurgeResults();
            } else {
                HasMoreResultSets();
            }
            throw;
        }

        // We always want the effect of HasMoreResultSets.
        if (HasMoreResultSets()  &&  want_all) {
            BeginNewRS();
        } else {
            break;
        }
    }

    if (missed_results) {
        NCBI_THROW(CSDB_Exception, eInconsistent,
                   "Result set had unread rows.  " + x_GetContext());
    }
}

inline bool
CQueryImpl::IsFinished(CQuery::EHowMuch how_much) const
{
    if ( !m_RSFinished ) {
        return false;
    } else if (how_much == CQuery::eThisResultSet
               ||  ( !m_CurRS  &&  !m_Stmt->HasMoreResults())) {
        return true;
    }

    // Check whether any further result sets actually exist, in which
    // case some state (saved here) will need to be rolled back.
    // m_CurRS will have to remain advanced, but HasMoreResultSets
    // knows not to advance it again until BeginNewRS has run.
    TColNumsMap  saved_col_nums   = m_ColNums;
    TFields      saved_fields;
    unsigned int saved_rel_row_no = m_CurRelRowNo;
    CQueryImpl&  nc_self          = const_cast<CQueryImpl&>(*this);

    swap(nc_self.m_Fields, saved_fields);
    if (nc_self.HasMoreResultSets()) {
        nc_self.m_RSFinished  = true; // still match end()
        nc_self.m_ColNums     = saved_col_nums;
        nc_self.m_CurRelRowNo = saved_rel_row_no;
        swap(nc_self.m_Fields, saved_fields);
        return false;
    }

    return true;
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
                   "No such column in the result set: "
                   + (col.IsPositional()
                      ? NStr::NumericToString(col.GetPosition())
                      : col.GetName())
                   + ".  " + x_GetContext());
    }
    return *m_Fields[pos - 1];
}

inline CDatabaseImpl*
CQueryImpl::GetDatabase(void) const
{
    return m_DBImpl.GetNCPointer();
}

inline IConnection*
CQueryImpl::GetConnection(void)
{
    return m_DBImpl->GetConnection();
}


inline
CBlobBookmarkImpl::CBlobBookmarkImpl(CDatabaseImpl* db_impl, I_ITDescriptor* descr)
    : m_DBImpl(db_impl),
      m_Descr(descr)
{}

CNcbiOstream&
CBlobBookmarkImpl::GetOStream(size_t blob_size, TBlobOStreamFlags flags)
{
    try {
        CDB_Connection* db_conn = m_DBImpl->GetConnection()->GetCDB_Connection();
        m_OStream.reset(new CWStream
                        (new CxBlobWriter(db_conn, *m_Descr, blob_size,
                                          flags, false),
                         0, 0,
                         CRWStreambuf::fOwnWriter
                         | CRWStreambuf::fLogExceptions));
        return *m_OStream;
    }
    SDBAPI_CATCH_LOWLEVEL()
}


CBlobBookmark::CBlobBookmark(void)
{}

CBlobBookmark::CBlobBookmark(CBlobBookmarkImpl* bm_impl)
    : m_Impl(bm_impl)
{}

CBlobBookmark::CBlobBookmark(const CBlobBookmark& bm)
    : m_Impl(bm.m_Impl)
{}

CBlobBookmark&
CBlobBookmark::operator= (const CBlobBookmark& bm)
{
    m_Impl = bm.m_Impl;
    return *this;
}

CBlobBookmark::~CBlobBookmark(void)
{}

CNcbiOstream&
CBlobBookmark::GetOStream(size_t blob_size, TBlobOStreamFlags flags) const
{
    return m_Impl.GetNCPointer()->GetOStream(blob_size, flags);
}

CNcbiOstream&
CBlobBookmark::GetOStream(size_t blob_size, CQuery::EAllowLog log_it) const
{
    return GetOStream(blob_size,
                      (log_it == CQuery::eDisableLog) ? fBOS_SkipLogging : 0);
}

CAutoTrans::CSubject DBAPI_MakeTrans(CDatabase& db)
{
    return DBAPI_MakeTrans(*db.m_Impl->GetConnection());
}

CAutoTrans::CSubject DBAPI_MakeTrans(CQuery& query)
{
    return DBAPI_MakeTrans(*query.m_Impl->GetConnection());
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

inline
const CDB_Exception::SContext& CQuery::CRowIterator::x_GetContext(void) const
{
    return m_Query->x_GetContext();
}

unsigned int
CQuery::CRowIterator::GetResultSetNo(void) const
{
    return m_Query->GetResultSetNo();
}

unsigned int
CQuery::CRowIterator::GetRowNo(void) const
{
    return m_Query->GetRowNo();
}

int
CQuery::CRowIterator::GetTotalColumns(void) const
{
    return m_Query->GetTotalColumns();
}

string
CQuery::CRowIterator::GetColumnName(unsigned int col) const
{
    return m_Query->GetColumnName(col);
}

ESDB_Type
CQuery::CRowIterator::GetColumnType(unsigned int col) const
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
        SDBAPI_THROW(eInconsistent, "Cannot increase end() iterator");
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

inline
const CDB_Exception::SContext& CQuery::CField::x_GetContext(void) const
{
    return m_Query->x_GetContext();
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
    short value = 0;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

Int4
CQuery::CField::AsInt4(void) const
{
    Int4 value = 0;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

Int8
CQuery::CField::AsInt8(void) const
{
    Int8 value = 0;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

float
CQuery::CField::AsFloat(void) const
{
    float value = 0;
    s_ConvertValue(m_Query->GetFieldValue(*this), value);
    return value;
}

double
CQuery::CField::AsDouble(void) const
{
    double value = 0;
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
        SDBAPI_THROW(eUnsupported,
                     string("Method is unsupported for this type of data: ")
                     + CDB_Object::GetTypeName(var_type, false));
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
        SDBAPI_THROW(eUnsupported,
                     string("Method is unsupported for this type of data: ")
                     + CDB_Object::GetTypeName(var_type, false));
    }
    m_ValueForStream = var_val.GetString();
    m_IStream.reset
      (new CNcbiIstrstream(m_ValueForStream.data(), m_ValueForStream.size()));
    return *m_IStream;
}

bool
CQuery::CField::IsNull(void) const
{
    return m_Query->GetFieldValue(*this).IsNull();
}

CNcbiOstream&
CQuery::CField::GetOStream(size_t blob_size, TBlobOStreamFlags flags) const
{
    const CVariant& var_val = m_Query->GetFieldValue(*this);
    EDB_Type var_type = var_val.GetType();
    if (m_IsParam  ||  (var_type != eDB_Image  &&  var_type != eDB_Text)) {
        SDBAPI_THROW(eUnsupported,
                     string("Method is unsupported for this type of data: ")
                     + CDB_Object::GetTypeName(var_type, false));
    }
    try {
        IConnection* conn = m_Query->GetConnection()->CloneConnection();
        CDB_Connection* db_conn = conn->GetCDB_Connection();
        m_OStream.reset(new CWStream
                        (new CxBlobWriter(db_conn, var_val.GetITDescriptor(),
                                          blob_size, flags, false),
                         0, 0,
                         CRWStreambuf::fOwnWriter
                         | CRWStreambuf::fLogExceptions));
        return *m_OStream;
    }
    SDBAPI_CATCH_LOWLEVEL()
}

CNcbiOstream&
CQuery::CField::GetOStream(size_t blob_size, EAllowLog log_it) const
{
    return GetOStream(blob_size,
                      (log_it == eDisableLog) ? fBOS_SkipLogging : 0);
}

CBlobBookmark
CQuery::CField::GetBookmark(void) const
{
    const CVariant& var_val = m_Query->GetFieldValue(*this);
    EDB_Type var_type = var_val.GetType();
    if (m_IsParam  ||  (var_type != eDB_Image  &&  var_type != eDB_Text)) {
        SDBAPI_THROW(eUnsupported,
                     string("Method is unsupported for this type of data: ")
                     + CDB_Object::GetTypeName(var_type, false));
    }
    CRef<CBlobBookmarkImpl> bm(new CBlobBookmarkImpl(m_Query->GetDatabase(),
                                                     var_val.ReleaseITDescriptor()));
    return CBlobBookmark(bm);
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
CQuery::Execute(const CTimeout& timeout)
{
    m_Impl->Execute(timeout);
}

void
CQuery::ExecuteSP(CTempString sp, const CTimeout& timeout)
{
    m_Impl->ExecuteSP(sp, timeout);
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
CQuery::GetResultSetNo(void) const
{
    return m_Impl->GetResultSetNo();
}

unsigned int
CQuery::GetRowNo(EHowMuch how_much) const
{
    return m_Impl->GetRowNo(how_much);
}

int
CQuery::GetRowCount(void) const
{
    return m_Impl->GetRowCount();
}

int
CQuery::GetStatus(void) const
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

void
CQuery::RequireRowCount(unsigned int n)
{
    m_Impl->RequireRowCount(n, n);
}

void
CQuery::RequireRowCount(unsigned int min_rows, unsigned int max_rows)
{
    m_Impl->RequireRowCount(min_rows, max_rows);
}

void
CQuery::VerifyDone(EHowMuch how_much)
{
    m_Impl->VerifyDone(how_much);
}

int
CQuery::GetTotalColumns(void) const
{
    return m_Impl->GetTotalColumns();
}

string
CQuery::GetColumnName(unsigned int col) const
{
    return m_Impl->GetColumnName(col);
}

ESDB_Type
CQuery::GetColumnType(unsigned int col) const
{
    return m_Impl->GetColumnType(col);
}

CQuery::CRowIterator
CQuery::begin(void) const
{
    m_Impl.GetNCPointer()->BeginNewRS();
    return CRowIterator(m_Impl.GetNCPointer(), false);
}

CQuery::CRowIterator
CQuery::end(void) const
{
    return CRowIterator(m_Impl.GetNCPointer(), true);
}

END_NCBI_SCOPE
