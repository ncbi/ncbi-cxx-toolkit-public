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
    typedef SSafeCP CP;

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
        return MakeCP<CP>(db_obj.Value());
    }

private:
    obj_type* m_Value;
};

////////////////////////////////////////////////////////////////////////////////
template <>
class CValueConvert<SSafeSqlCP, CDB_Result>
{
public:
    typedef const CDB_Result obj_type;
    typedef SSafeSqlCP CP;

    CValueConvert(const CValueConvert<CP, CDB_Result>& value)
    : m_Value(value.m_Value)
    {
    }
    CValueConvert(obj_type& value)
    : m_Value(&value)
    {
    }

public:
    operator bool(void) const
    {
        const int item_num = m_Value->CurrentItemNo();
        const EDB_Type db_type = m_Value->ItemDataType(item_num);

        // *null* is reported as eDB_Int by several drivers.
        // That means that *null* can be checked using Int4 type only.
        // List of *special* drivers: ftds64, ctlib.
        if (db_type == eDB_Int) {
            CDB_Int db_obj_int;

            const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj_int);
            if (db_obj_int.IsNULL()) {
                return bool();
            }

            throw CInvalidConversionException();
        }

        CDB_Bit db_obj;
        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);

        if (db_obj.IsNULL()) {
            return bool();
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
        CDB_Int db_obj;
        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);

        if (db_obj.IsNULL()) {
            return Int4();
        }

        // We are using SSafeCP intentionally here ...
        return MakeCP<SSafeCP>(db_obj.Value());
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
        const int item_num = m_Value->CurrentItemNo();
        const EDB_Type db_type = m_Value->ItemDataType(item_num);

        // *null* is reported as eDB_Int by several drivers.
        // That means that *null* can be checked using Int4 type only.
        // List of *special* drivers: ftds64, ctlib.
        CDB_Int db_obj_int;
        if (db_type == eDB_Int) {

            const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj_int);
            if (db_obj_int.IsNULL()) {
                return string();
            }

            throw CInvalidConversionException();
        }

        CDB_VarChar db_obj;
        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            return string();
        }
        return string(db_obj.Value(), db_obj.Size());
    }
    operator CTime(void) const
    {
        return ConvertFrom<CTime, CDB_DateTime>();
    }

private:
    template <typename TO, typename FROM>
    TO ConvertFrom(void) const
    {
        const int item_num = m_Value->CurrentItemNo();
        const EDB_Type db_type = m_Value->ItemDataType(item_num);

        // *null* is reported as eDB_Int by several drivers.
        // That means that *null* can be checked using Int4 type only.
        // List of *special* drivers: ftds64, ctlib.
        if (db_type == eDB_Int) {
            CDB_Int db_obj_int;

            const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj_int);
            if (db_obj_int.IsNULL()) {
                return TO();
            }

            throw CInvalidConversionException();
        }

        FROM db_obj;
        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);

        if (db_obj.IsNULL()) {
            return TO();
        }

        // We are using SSafeCP intentionally here ...
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

        return NCBI_CONVERT_TO(Convert(db_obj.Value()), TO);
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

    template <typename TO>
    void ReadCDBObject(TO& value) const
    {
        const int item_num = m_Value->CurrentItemNo();
        const EDB_Type db_type = m_Value->ItemDataType(item_num);

        switch (db_type) {
            case eDB_Int:
                value = ConvertFrom<TO, CDB_Int>();
                break;
            case eDB_SmallInt:
                value = ConvertFrom<TO, CDB_SmallInt>();
                break;
            case eDB_TinyInt:
                value = ConvertFrom<TO, CDB_TinyInt>();
                break;
            case eDB_BigInt:
                value = ConvertFrom<TO, CDB_BigInt>();
                break;
            case eDB_VarChar:
                value = ConvertFromStr<TO, CDB_VarChar>();
                break;
            case eDB_Char:
                value = ConvertFromChar<TO, CDB_Char>(item_num);
                break;
            case eDB_VarBinary:
                value = ConvertFromStr<TO, CDB_VarBinary>();
                break;
            case eDB_Binary:
                value = ConvertFromChar<TO, CDB_Binary>(item_num);
                break;
            case eDB_Float:
                value = ConvertFrom<TO, CDB_Float>();
                break;
            case eDB_Double:
                value = ConvertFrom<TO, CDB_Double>();
                break;
                // case eDB_DateTime:
                //     value = ConvertFrom<TO, CDB_DateTime>();
                // case eDB_SmallDateTime:
                //     value = ConvertFrom<TO, CDB_SmallDateTime>();
            case eDB_Text:
                value = ConvertFromLOB<TO, CDB_Text>();
                break;
            case eDB_Image:
                value = ConvertFromLOB<TO, CDB_Image>();
                break;
            case eDB_Bit:
                value = ConvertFrom<TO, CDB_Bit>();
                break;
            case eDB_Numeric:
                value = ConvertFrom<TO, CDB_Numeric>();
                break;
            case eDB_LongChar:
                value = ConvertFromChar<TO, CDB_LongChar>(item_num);
                break;
            case eDB_LongBinary:
                {
                    CDB_LongBinary db_obj(m_Value->ItemMaxSize(item_num));

                    const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
                    if (db_obj.IsNULL()) {
                        throw CInvalidConversionException();
                    }

                    value = Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.DataSize()));
                }
                break;
            default:
                throw CInvalidConversionException();
        }
    }

private:
    obj_type* m_Value;
};

////////////////////////////////////////////////////////////////////////////////
template <>
class NCBI_DBAPIDRIVER_EXPORT CValueConvert<SRunTimeSqlCP, CDB_Result>
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

private:
    template <typename TO, typename FROM>
    TO ConvertFrom(void) const
    {
        FROM db_obj;

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            return TO();
        }

        return NCBI_CONVERT_TO(Convert(db_obj.Value()), TO);
    }

    template <typename TO, typename FROM>
    TO ConvertFromStr(void) const
    {
        FROM db_obj;

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            return TO();
        }

        return Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.Size()));
    }

    template <typename TO, typename FROM>
    TO ConvertFromChar(const int item_num) const
    {
        FROM db_obj(m_Value->ItemMaxSize(item_num));

        const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
        if (db_obj.IsNULL()) {
            return TO();
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
            return TO();
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
            return TO();
        }

        return CTime(time_t(Convert(db_obj.Value())));
    }

    template <typename TO>
    void ReadCDBObject(TO& value) const
    {
        const int item_num = m_Value->CurrentItemNo();
        const EDB_Type db_type = m_Value->ItemDataType(item_num);

        switch (db_type) {
            case eDB_Int:
                value = ConvertFrom<TO, CDB_Int>();
                break;
            case eDB_SmallInt:
                value = ConvertFrom<TO, CDB_SmallInt>();
                break;
            case eDB_TinyInt:
                value = ConvertFrom<TO, CDB_TinyInt>();
                break;
            case eDB_BigInt:
                value = ConvertFrom<TO, CDB_BigInt>();
                break;
            case eDB_VarChar:
                value = ConvertFromStr<TO, CDB_VarChar>();
                break;
            case eDB_Char:
                value = ConvertFromChar<TO, CDB_Char>(item_num);
                break;
            case eDB_VarBinary:
                value = ConvertFromStr<TO, CDB_VarBinary>();
                break;
            case eDB_Binary:
                value = ConvertFromChar<TO, CDB_Binary>(item_num);
                break;
            case eDB_Float:
                value = ConvertFrom<TO, CDB_Float>();
                break;
            case eDB_Double:
                value = ConvertFrom<TO, CDB_Double>();
                break;
                // case eDB_DateTime:
                //     value = ConvertFrom<TO, CDB_DateTime>();
                // case eDB_SmallDateTime:
                //     value = ConvertFrom<TO, CDB_SmallDateTime>();
            case eDB_Text:
                value = ConvertFromLOB<TO, CDB_Text>();
                break;
            case eDB_Image:
                value = ConvertFromLOB<TO, CDB_Image>();
                break;
            case eDB_Bit:
                value = ConvertFrom<TO, CDB_Bit>();
                break;
            case eDB_Numeric:
                value = ConvertFrom<TO, CDB_Numeric>();
                break;
            case eDB_LongChar:
                value = ConvertFromChar<TO, CDB_LongChar>(item_num);
                break;
            case eDB_LongBinary:
                {
                    CDB_LongBinary db_obj(m_Value->ItemMaxSize(item_num));

                    const_cast<CDB_Result*>(m_Value)->GetItem(&db_obj);
                    if (db_obj.IsNULL()) {
                        value = TO();
                    }

                    value = Convert(string(static_cast<const char*>(db_obj.Value()), db_obj.DataSize()));
                }
                break;
            default:
                throw CInvalidConversionException();
        }
    }

private:
    obj_type* m_Value;
};

///////////////////////////////////////////////////////////////////////////////
template <typename CP, typename R, typename S>
class CMakeObject
{
public:
    static R Make(S& source)
    {
        return R();
    }
};

template <typename CP, typename R>
class CMakeObject<CP, R, CDB_Result>
{
public:
    static R Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        return TResult(source);
    }
};

template <typename T1, typename T2>
class CMakeObject<SRunTimeCP, pair<T1, T2>, CDB_Result>
{
public:
    typedef pair<T1, T2> TValue;
    typedef SRunTimeCP CP;

    static TValue Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);

        // We may get an error at run-time ...
        T1 v1 = CMakeObject<CP, T1, CDB_Result>::Make(source);
        T2 v2 = CMakeObject<CP, T2, CDB_Result>::Make(source);

        return TValue(v1, v2);
    }
};

template <typename T1, typename T2>
class CMakeObject<SSafeCP, pair<T1, T2>, CDB_Result>
{
public:
    typedef pair<T1, T2> TValue;
    typedef SSafeCP CP;

    static TValue Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);

        /* Not all data types have default constructor. */
        const unsigned int n = source.NofItems();

        T1 v1 = T1();
        T2 v2 = T2();

        if (static_cast<unsigned int>(source.CurrentItemNo()) < n) {
            v1 = CMakeObject<CP, T1, CDB_Result>::Make(source);
        }

        if (static_cast<unsigned int>(source.CurrentItemNo()) < n) {
            v2 = CMakeObject<CP, T2, CDB_Result>::Make(source);
        }

        return TValue(v1, v2);
    }
};

template <typename CP, typename T>
class CMakeObject<CP, vector<T>, CDB_Result>
{
public:
    typedef vector<T> TValue;

    static TValue Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);
        const unsigned int n = source.NofItems();
        TValue res_val;

        for (unsigned int i = source.CurrentItemNo(); i < n; i = source.CurrentItemNo()) {
            res_val.push_back(CMakeObject<CP, T, CDB_Result>::Make(source));
        }

        return res_val;
    }
};

template <typename CP, typename T>
class CMakeObject<CP, stack<T>, CDB_Result>
{
public:
    typedef stack<T> TValue;

    static TValue Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);
        const unsigned int n = source.NofItems();
        TValue res_val;

        for (unsigned int i = source.CurrentItemNo(); i < n; i = source.CurrentItemNo()) {
            res_val.push(CMakeObject<CP, T, CDB_Result>::Make(source));
        }

        return res_val;
    }
};

template <typename CP, typename T>
class CMakeObject<CP, deque<T>, CDB_Result>
{
public:
    typedef deque<T> TValue;

    static TValue Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);
        const unsigned int n = source.NofItems();
        TValue res_val;

        for (unsigned int i = source.CurrentItemNo(); i < n; i = source.CurrentItemNo()) {
            res_val.push_back(CMakeObject<CP, T, CDB_Result>::Make(source));
        }

        return res_val;
    }
};

template <typename CP, typename T>
class CMakeObject<CP, set<T>, CDB_Result>
{
public:
    typedef set<T> TValue;

    static TValue Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);
        const unsigned int n = source.NofItems();
        TValue res_val;

        for (unsigned int i = source.CurrentItemNo(); i < n; i = source.CurrentItemNo()) {
            res_val.insert(CMakeObject<CP, T, CDB_Result>::Make(source));
        }

        return res_val;
    }
};

template <typename K, typename V>
class CMakeObject<SRunTimeCP, map<K, V>, CDB_Result>
{
public:
    typedef map<K, V> TValue;
    typedef SRunTimeCP CP;

    static TValue Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);
        const unsigned int n = source.NofItems();
        TValue res_val;

        for (unsigned int i = source.CurrentItemNo(); i < n; i = source.CurrentItemNo()) {
            // We may get an error at run-time ...
            K k = CMakeObject<CP, K, CDB_Result>::Make(source);
            V v = CMakeObject<CP, V, CDB_Result>::Make(source);

            res_val.insert(pair<K, V>(k, v));
        }

        return res_val;
    }
};

template <typename K, typename V>
class CMakeObject<SSafeCP, map<K, V>, CDB_Result>
{
public:
    typedef map<K, V> TValue;
    typedef SSafeCP CP;

    static TValue Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);
        const unsigned int n = source.NofItems();
        TValue res_val;

        for (unsigned int i = source.CurrentItemNo(); i < n; i = source.CurrentItemNo()) {
            /* Not all data types have default constructor ... */
            K k = CMakeObject<CP, K, CDB_Result>::Make(source);
            V v = V();

            if (static_cast<unsigned int>(source.CurrentItemNo()) < n) {
                v = CMakeObject<CP, V, CDB_Result>::Make(source);
            }

            res_val.insert(pair<K, V>(k, v));
        }

        return res_val;
    }
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP, typename TO>
class CConvertTO
{
public:
    typedef TO TValue;

    static void Convert(const auto_ptr<CDB_Result>& rs, TValue& value)
    {
        if (rs->Fetch()) {
             value = CMakeObject<CP, TValue, CDB_Result>::Make(*rs);
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
        while (rs->Fetch()) {
            value.push_back(CMakeObject<CP, T, CDB_Result>::Make(*rs));
        }
    }
};

template <typename CP, typename T>
class CConvertTO<CP, deque<T> >
{
public:
    typedef deque<T> TValue;

    static void Convert(const auto_ptr<CDB_Result>& rs, TValue& value)
    {
        while (rs->Fetch()) {
            value.push_back(CMakeObject<CP, T, CDB_Result>::Make(*rs));
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
        while (rs->Fetch()) {
            value.insert(CMakeObject<CP, T, CDB_Result>::Make(*rs));
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
        while (rs->Fetch()) {
            value.push(CMakeObject<CP, T, CDB_Result>::Make(*rs));
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
        while (rs->Fetch()) {
            K k = CMakeObject<CP, K, CDB_Result>::Make(*rs);
            V v = CMakeObject<CP, V, CDB_Result>::Make(*rs);

            value.insert(pair<K, V>(k, v));
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

            if (rs.get() == NULL) {
                continue;
            }

            CConvertTO<CP, TO>::Convert(rs, result);

            return result;
        }

        // return TO();
        throw CInvalidConversionException();
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

            if (rs.get() == NULL) {
                continue;
            }

            CConvertTO<CP, TO>::Convert(rs, result);

            return result;
        }

        // return TO();
        throw CInvalidConversionException();
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

            if (rs.get() == NULL) {
                continue;
            }

            CConvertTO<CP, TO>::Convert(rs, result);

            return result;
        }

        // return TO();
        throw CInvalidConversionException();
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

            if (rs.get() == NULL) {
                continue;
            }

            CConvertTO<CP, TO>::Convert(rs, result);

            return result;
        }

        // return TO();
        throw CInvalidConversionException();
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

