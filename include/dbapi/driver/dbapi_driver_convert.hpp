#ifndef DBAPI_DRIVER___DBAPI_DRIVER_CONVERT__HPP
#define DBAPI_DRIVER___DBAPI_DRIVER_CONVERT__HPP

/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NTOICE
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
 *
 */

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/dbapi_object_convert.hpp>
// Future development ...
// #include <dbapi/driver/dbapi_driver_value_slice.hpp>

#include <vector>
#include <set>
#include <stack>

BEGIN_NCBI_SCOPE

namespace value_slice
{

////////////////////////////////////////////////////////////////////////////////
template <>
class CValueConvert<SSafeCP, CDB_Result>
{
public: 
    typedef const CDB_Result obj_type;

    CValueConvert(obj_type& value)
    : m_Value(&value)
    {
    }

public:
    operator bool(void) const
    {
        CDB_Bit db_obj;
        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            throw CInvalidConversionException();
        }
        return db_obj.Value() != 0;
    }
    operator Uint1(void) const
    {
        return ConvertFrom<Uint1, CDB_TinyInt>();
    }
    operator Int2(void) const
    {
        return ConvertFrom<Int2, CDB_SmallInt>();
    }
    operator Int4(void) const
    {
        return ConvertFrom<Int4, CDB_Int>();
    }
    operator Int8(void) const
    {
        return ConvertFrom<Int8, CDB_BigInt>();
    }
    operator float(void) const
    {
        return ConvertFrom<float, CDB_Float>();
    }
    operator double(void) const
    {
        return ConvertFrom<double, CDB_Double>();
    }
    operator string(void) const
    {
        CDB_VarChar db_obj;
        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            throw CInvalidConversionException();
        }
        return string(db_obj.Value(), db_obj.Size());
    }
    operator CTime(void) const
    {
        return ConvertFrom<const CTime&, CDB_DateTime>();
    }

private:
    template <typename TO, typename FROM>
    TO ConvertFrom(void) const
    {
        FROM db_obj;
        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            throw CInvalidConversionException();
        }
        return MakeCP<SSafeCP>(db_obj.Value());
    }

private:
    obj_type* m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <>
class NCBI_DBAPIDRIVER_EXPORT CValueConvert<SRunTimeCP, CDB_Result>
{
public: 
    typedef const CDB_Result obj_type;

    CValueConvert(obj_type& value);

public:
    operator bool(void) const;
    operator Int1(void) const;
    operator Uint1(void) const;
    operator Int2(void) const;
    operator Uint2(void) const;
    operator Int4(void) const;
    operator Uint4(void) const;
    operator Int8(void) const;
    operator Uint8(void) const;
    operator float(void) const;
    operator double(void) const;
    operator string(void) const;
    operator CTime(void) const;

    /* Template version of type conversion operator.
    template <typename TO>
    operator TO(void) const
    {
        const int item_num = m_Value->CurrentItemNo();
        const EDB_Type db_type = m_Value->ItemDataType(item_num);

        switch (db_type) {
        case eDB_Int:
            return ConvertFrom<TO, CDB_Int>();
        case eDB_SmallInt:
            return ConvertFrom<TO, CDB_SmallInt>();
        case eDB_TinyInt:
            return ConvertFrom<TO, CDB_TinyInt>();
        case eDB_BigInt:
            return ConvertFrom<TO, CDB_BigInt>();
        case eDB_VarChar:
            return ConvertFromStr<TO, CDB_VarChar>();
        case eDB_Char:
            return ConvertFromChar<TO, CDB_Char>(item_num);
        case eDB_VarBinary:
            return ConvertFromStr<TO, CDB_VarBinary>();
        case eDB_Binary:
            return ConvertFromChar<TO, CDB_Binary>(item_num);
        case eDB_Float:
            return ConvertFrom<TO, CDB_Float>();
        case eDB_Double:
            return ConvertFrom<TO, CDB_Double>();
        // case eDB_DateTime:
        //     return ConvertFrom<TO, CDB_DateTime>();
        // case eDB_SmallDateTime:
        //     return ConvertFrom<TO, CDB_SmallDateTime>();
        case eDB_Text:
            return ConvertFromLOB<TO, CDB_Text>();
        case eDB_Image:
            return ConvertFromLOB<TO, CDB_Image>();
        case eDB_Bit: 
            return ConvertFrom<TO, CDB_Bit>();
        case eDB_Numeric:
            return ConvertFrom<TO, CDB_Numeric>();
        case eDB_LongChar:
            return ConvertFromChar<TO, CDB_LongChar>(item_num);
        case eDB_LongBinary:
            {
                CDB_LongBinary db_obj(m_Value->ItemMaxSize(item_num));

                const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
                if (db_obj.IsNULL()) {
                    throw CInvalidConversionException();
                }

                return Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.DataSize()));
            }
        default:
            throw CInvalidConversionException();
        }

        return TO();
    }
    */

private:
    /* future development ...
    template <typename TO, typename FROM>
    TO ConvertFrom(void) const
    {
        FROM db_obj;
        wrapper<FROM> obj_wrapper(db_obj);

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (obj_wrapper.is_null()) {
            throw CInvalidConversionException();
        }

        // return MakeCP<SRunTimeCP>(obj_wrapper.get_value());
        return Convert(obj_wrapper.get_value());
    }
    */

    template <typename TO, typename FROM>
    TO ConvertFrom(void) const
    {
        FROM db_obj;

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            throw CInvalidConversionException();
        }

        return Convert(db_obj.Value());
    }

    template <typename TO, typename FROM>
    TO ConvertFromStr(void) const
    {
        FROM db_obj;

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            throw CInvalidConversionException();
        }

        return Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.Size()));
    }

    template <typename TO, typename FROM>
    TO ConvertFromChar(const int item_num) const
    {
        FROM db_obj(m_Value->ItemMaxSize(item_num));

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            throw CInvalidConversionException();
        }

        return Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.Size()));
    }

    template <typename TO, typename FROM>
    TO ConvertFromLOB(void) const
    {
        FROM db_obj;
        string result;

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            throw CInvalidConversionException();
        }

        result.resize(db_obj.Size());
        db_obj.Read(const_cast<void*>(static_cast<const void*>(result.c_str())),
                db_obj.Size()
                );

        return Convert(result);
    }

    template <typename TO, typename FROM>
    TO Convert2CTime(void) const
    {
        FROM db_obj;

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            throw CInvalidConversionException();
        }

        return CTime(time_t(Convert(db_obj.Value())));
    }

private:
    obj_type* m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP, typename TO> 
class CConvertTO
{
public:
    typedef TO TValue;

    static void Convert(const auto_ptr<CDB_Result>& rs, TValue& value)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        if (rs->Fetch()) {
             value = TResult(*rs);
        }
    }
};

template <typename CP, typename T1, typename T2> 
class CConvertTO<CP, pair<T1, T2> >
{
public:
    typedef pair<T1, T2> TValue;

    static void Convert(const auto_ptr<CDB_Result>& rs, TValue& value)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        if (rs->Fetch()) {
            TResult result(*rs);
            T1 v1 = result;
            T2 v2 = result;

            value = pair<T1, T2>(v1, v2);
        }
    }
};

template <typename CP, typename T> 
class CConvertTO<CP, vector<T> >
{
public:
    typedef vector<T> TValue;

    static void Convert(const auto_ptr<CDB_Result>& rs, TValue& value)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(*rs);
        while (rs->Fetch()) {
            value.push_back(result);
        }
    }
};

template <typename CP, typename T> 
class CConvertTO<CP, set<T> >
{
public:
    typedef set<T> TValue;

    static void Convert(const auto_ptr<CDB_Result>& rs, TValue& value)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(*rs);
        while (rs->Fetch()) {
            value.insert(result);
        }
    }
};

template <typename CP, typename T> 
class CConvertTO<CP, stack<T> >
{
public:
    typedef stack<T> TValue;

    static void Convert(const auto_ptr<CDB_Result>& rs, TValue& value)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(*rs);
        while (rs->Fetch()) {
            value.push(result);
        }
    }
};

template <typename CP, typename K, typename V> 
class CConvertTO<CP, map<K, V> >
{
public:
    typedef map<K, V> TValue;

    static void Convert(const auto_ptr<CDB_Result>& rs, TValue& value)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(*rs);
        while (rs->Fetch()) {
            K k = result;
            V v = result;

            value.insert(make_pair(k, v));
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_LangCmd>
{
public: 
    typedef CDB_LangCmd TObj;

    CValueConvert(const CValueConvert<CP, TObj>& other)
    : m_Stmt(other.m_Stmt)
    {
    }
    CValueConvert(TObj& value)
    : m_Stmt(&value)
    {
        if (!m_Stmt->Send()) {
            throw CInvalidConversionException();
        }
    }
    ~CValueConvert(void)
    {
        try {
            m_Stmt->DumpResults();
        }
        // NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
        catch (...)
        {
            ;
        }
    }

public:
    template <typename TO> 
    operator TO(void) const 
    {
        TO result;

        while (m_Stmt->HasMoreResults()) {
            auto_ptr<CDB_Result> rs(m_Stmt->Result());

            if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                continue;
            }

            CConvertTO<CP, TO>::Convert(rs, result);
        }

        return result;
    }

private:
    TObj* m_Stmt; 
};

template <typename CP>
class CValueConvert<CP, CDB_LangCmd*>
{
public: 
    typedef CDB_LangCmd TObj;

    CValueConvert(const CValueConvert<CP, TObj*>& other)
    : m_Stmt(other.m_Stmt)
    {
    }
    CValueConvert(TObj* value)
    : m_Stmt(value)
    {
        if (!m_Stmt->Send()) {
            throw CInvalidConversionException();
        }
    }
    ~CValueConvert(void)
    {
        try {
            m_Stmt->DumpResults();
        }
        // NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
        catch (...)
        {
            ;
        }
    }

public:
    template <typename TO> 
    operator TO(void) const 
    {
        TO result;

        while (m_Stmt->HasMoreResults()) {
            auto_ptr<CDB_Result> rs(m_Stmt->Result());

            if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                continue;
            }

            CConvertTO<CP, TO>::Convert(rs, result);
        }

        return result;
    }

private:
    mutable auto_ptr<TObj> m_Stmt; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_RPCCmd>
{
public: 
    typedef CDB_RPCCmd TObj;

    CValueConvert(const CValueConvert<CP, TObj>& other)
    : m_Stmt(other.m_Stmt)
    {
    }
    CValueConvert(TObj& value)
    : m_Stmt(&value)
    {
        if (!m_Stmt->Send()) {
            throw CInvalidConversionException();
        }
    }
    ~CValueConvert(void)
    {
        try {
            m_Stmt->DumpResults();
        }
        // NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
        catch (...)
        {
            ;
        }
    }

public:
    template <typename TO> 
    operator TO(void) const 
    {
        TO result;

        while (m_Stmt->HasMoreResults()) {
            auto_ptr<CDB_Result> rs(m_Stmt->Result());

            if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                continue;
            }

            CConvertTO<CP, TO>::Convert(rs, result);
        }

        return result;
    }

private:
    TObj* m_Stmt; 
};

template <typename CP>
class CValueConvert<CP, CDB_RPCCmd*>
{
public: 
    typedef CDB_RPCCmd TObj;

    CValueConvert(const CValueConvert<CP, TObj*>& other)
    : m_Stmt(other.m_Stmt)
    {
    }
    CValueConvert(TObj* value)
    : m_Stmt(value)
    {
        if (!m_Stmt->Send()) {
            throw CInvalidConversionException();
        }
    }
    ~CValueConvert(void)
    {
        try {
            m_Stmt->DumpResults();
        }
        // NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
        catch (...)
        {
            ;
        }
    }

public:
    template <typename TO> 
    operator TO(void) const 
    {
        TO result;

        while (m_Stmt->HasMoreResults()) {
            auto_ptr<CDB_Result> rs(m_Stmt->Result());

            if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                continue;
            }

            CConvertTO<CP, TO>::Convert(rs, result);
        }

        return result;
    }

private:
    mutable auto_ptr<TObj> m_Stmt; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_CursorCmd>
{
public: 
    typedef CDB_CursorCmd TObj;

    CValueConvert(const CValueConvert<CP, TObj>& other)
    : m_Stmt(other.m_Stmt)
    , m_RS(other.m_RS)
    {
    }
    CValueConvert(TObj& value)
    : m_Stmt(&value)
    , m_RS(value.Open())
    {
        if (!m_RS.get()) {
            throw CInvalidConversionException();
        }
    }
    ~CValueConvert(void)
    {
    }

public:
    template <typename TO> 
    operator TO(void) const 
    {
        TO result;

        CConvertTO<CP, TO>::Convert(m_RS, result);

        return result;
    }

private:
    TObj* m_Stmt; 
    auto_ptr<CDB_Result> m_RS;
};

template <typename CP>
class CValueConvert<CP, CDB_CursorCmd*>
{
public: 
    typedef CDB_CursorCmd TObj;

    CValueConvert(const CValueConvert<CP, TObj*>& other)
    : m_Stmt(other.m_Stmt)
    , m_RS(other.m_RS)
    {
    }
    CValueConvert(TObj* value)
    : m_Stmt(value)
    , m_RS(value->Open())
    {
        if (!m_RS.get()) {
            throw CInvalidConversionException();
        }
    }
    ~CValueConvert(void)
    {
    }

public:
    template <typename TO> 
    operator TO(void) const 
    {
        TO result;

        CConvertTO<CP, TO>::Convert(m_RS, result);

        return result;
    }

private:
    mutable auto_ptr<TObj> m_Stmt; 
    mutable auto_ptr<CDB_Result> m_RS;
};


} // namespace value_slice

END_NCBI_SCOPE


#endif // DBAPI_DRIVER___DBAPI_DRIVER_CONVERT__HPP 

