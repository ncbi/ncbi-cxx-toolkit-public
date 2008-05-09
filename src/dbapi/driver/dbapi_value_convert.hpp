#ifndef DBAPI_DRIVER___DBAPI_VALUE_CONVERT__HPP
#define DBAPI_DRIVER___DBAPI_VALUE_CONVERT__HPP

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
 *
 */


#include <corelib/ncbidbg.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>


BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
template <typename T>
class CValueConvert
{
public:
    typedef T obj_type;

    CValueConvert(const obj_type&)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        _ASSERT(false);
        return OT();
    }
};

template <>
class CValueConvert<string>
{
public:
    typedef string obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    operator bool(void)
    {
        return NStr::StringToBool(m_Value);
    }
    operator Uint1(void)
    {
        // !!! Apply a conversion policy here ...
        return static_cast<Uint1>(NStr::StringToUInt(m_Value));
    }
    operator Int1(void)
    {
        // !!! Apply a conversion policy here ...
        return static_cast<Int2>(NStr::StringToInt(m_Value));
    }
    operator Uint2(void)
    {
        // !!! Apply a conversion policy here ...
        return static_cast<Uint2>(NStr::StringToUInt(m_Value));
    }
    operator Int2(void)
    {
        // !!! Apply a conversion policy here ...
        return static_cast<Int2>(NStr::StringToInt(m_Value));
    }
    operator Uint4(void)
    {
        return NStr::StringToUInt(m_Value);
    }
    operator Int4(void)
    {
        return NStr::StringToInt(m_Value);
    }
    operator Uint8(void)
    {
        return NStr::StringToUInt8(m_Value);
    }
    operator Int8(void)
    {
        return NStr::StringToInt8(m_Value);
    }
    operator float(void)
    {
        // !!! Apply a conversion policy here ...
        return static_cast<float>(NStr::StringToDouble(m_Value));
    }
    operator double(void)
    {
        return NStr::StringToDouble(m_Value);
    }
    operator string(void)
    {
        return m_Value;
    }
    operator CTime(void)
    {
        return CTime(m_Value);
    }

private:
    const obj_type  m_Value;
};

template <>
class CValueConvert<bool>
{
public:
    typedef bool obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        return (m_Value ? 1 : 0);
    }
    operator bool(void)
    {
        return m_Value;
    }
    operator string(void)
    {
        return NStr::BoolToString(m_Value);
    }
    operator CTime(void)
    {
        if (m_Value) {
            return CTime(CTime::eCurrent);
        }

        return CTime();
    }

private:
    const obj_type  m_Value;
};

template <>
class CValueConvert<Uint1>
{
public:
    typedef Uint1 obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator Uint1(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return (m_Value != 0);
    }
    operator string(void)
    {
        return NStr::UIntToString(static_cast<unsigned long>(m_Value));
    }
    operator CTime(void)
    {
        return CTime(static_cast<time_t>(m_Value));
    }

private:
    const obj_type  m_Value;
};

template <>
class CValueConvert<Int1>
{
public:
    typedef Int1 obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator Int1(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return (m_Value != 0);
    }
    operator string(void)
    {
        return NStr::IntToString(static_cast<long>(m_Value));
    }
    operator CTime(void)
    {
        return CTime(static_cast<time_t>(m_Value));
    }

private:
    const obj_type  m_Value;
};

template <>
class CValueConvert<Uint2>
{
public:
    typedef Uint2 obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator Uint2(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return (m_Value != 0);
    }
    operator string(void)
    {
        return NStr::UIntToString(static_cast<unsigned long>(m_Value));
    }
    operator CTime(void)
    {
        return CTime(static_cast<time_t>(m_Value));
    }

private:
    const obj_type  m_Value;
};

template <>
class CValueConvert<Int2>
{
public:
    typedef Int2 obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator Int2(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return (m_Value != 0);
    }
    operator string(void)
    {
        return NStr::IntToString(static_cast<long>(m_Value));
    }
    operator CTime(void)
    {
        return CTime(static_cast<time_t>(m_Value));
    }

private:
    const obj_type  m_Value;
};

template <>
class CValueConvert<Uint4>
{
public:
    typedef Uint4 obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator Uint4(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return (m_Value != 0);
    }
    operator string(void)
    {
        return NStr::UIntToString(static_cast<unsigned long>(m_Value));
    }
    operator CTime(void)
    {
        return CTime(static_cast<time_t>(m_Value));
    }

private:
    const obj_type m_Value;
};

template <>
class CValueConvert<Int4>
{
public:
    typedef Int4 obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator Int4(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return (m_Value != 0);
    }
    operator string(void)
    {
        return NStr::IntToString(static_cast<long>(m_Value));
    }
    operator CTime(void)
    {
        return CTime(static_cast<time_t>(m_Value));
    }

private:
    const obj_type  m_Value;
};
template <>
class CValueConvert<Uint8>
{
public:
    typedef Uint8 obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator Uint8(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return (m_Value != 0);
    }
    operator string(void)
    {
        return NStr::UInt8ToString(static_cast<Uint8>(m_Value));
    }
    operator CTime(void)
    {
        return CTime(static_cast<time_t>(m_Value));
    }

private:
    const obj_type m_Value;
};

template <>
class CValueConvert<Int8>
{
public:
    typedef Int8 obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator Int8(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return (m_Value != 0);
    }
    operator string(void)
    {
        return NStr::IntToString(static_cast<Int8>(m_Value));
    }
    operator CTime(void)
    {
        return CTime(static_cast<time_t>(m_Value));
    }

private:
    const obj_type  m_Value;
};

template <>
class CValueConvert<CTime>
{
public:
    typedef CTime obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }
    CValueConvert(const string& value)
    : m_Value(value)
    {
    }
    CValueConvert(const time_t& value)
    : m_Value(value)
    {
    }

public:
    template <typename OT>
    operator OT(void)
    {
        // Apply a conversion policy ...
        return m_Value;
    }
    operator CTime(void)
    {
        return m_Value;
    }
    operator bool(void)
    {
        return !m_Value.IsEmpty();
    }
    operator string(void)
    {
        return m_Value.AsString();
    }

private:
    const obj_type  m_Value;
};


////////////////////////////////////////////////////////////////////////////////
// A limited case ...
template <typename T>
CValueConvert<T> convert(T& value)
{
    return CValueConvert<T>(value);
}

template <typename T>
CValueConvert<T> convert(const T& value)
{
    return CValueConvert<T>(value);
}


END_NCBI_SCOPE


#endif // DBAPI_DRIVER___DBAPI_VALUE_CONVERT__HPP
