#ifndef UTIL___VALUE_CONVERT__HPP
#define UTIL___VALUE_CONVERT__HPP

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


#include <corelib/ncbistr.hpp>

#include "value_convert_policy.hpp"

BEGIN_NCBI_SCOPE

#if defined(NCBI_COMPILER_MSVC)
#  define NCBI_CONVERT_TO(x,y) (x).operator y()
#else
#  define NCBI_CONVERT_TO(x,y) (x)
#endif

namespace value_slice
{


////////////////////////////////////////////////////////////////////////////////
// Forward declaration.
//
template <typename CP, typename FROM> class CValueConvert;

////////////////////////////////////////////////////////////////////////////////
template <typename CP, typename FROM>
inline
CConvPolicy<CP, FROM> MakeCP(const FROM& value)
{
    return CConvPolicy<CP, FROM>(value);
}

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, string>
{ 
public:
    typedef string obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    operator obj_type(void) const
    {
        return m_Value;
    }
    operator bool(void) const
    { 
        return MakeCP<CP>(NStr::StringToBool(m_Value));
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value, NStr::fAllowTrailingSymbols));
    }
#if !defined(NCBI_INT8_IS_LONG)  &&  \
    (defined(NCBI_COMPILER_GCC)  ||  defined(NCBI_COMPILER_ICC)  ||  \
     defined(NCBI_COMPILER_WORKSHOP)  ||  defined(NCBI_COMPILER_MSVC))
    operator long(void) const
    {
        return MakeCP<CP>(NStr::StringToLong(m_Value, NStr::fAllowTrailingSymbols));
    }
#endif
    operator Uint8(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt8(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(NStr::StringToInt8(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator float(void) const
    {
        return MakeCP<CP>(NStr::StringToDouble(m_Value));
    }
    operator double(void) const
    {
        return MakeCP<CP>(NStr::StringToDouble(m_Value));
    }
    operator long double(void) const
    {
        return MakeCP<CP>(NStr::StringToDouble(m_Value));
    }
    operator CTime(void) const
    {
        return CTime(m_Value);
    }

#if defined(NCBI_COMPILER_GCC) && NCBI_COMPILER_VERSION >= 340 && NCBI_COMPILER_VERSION < 400
    operator char(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value));
    }
    operator unsigned char(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value));
    }
    operator unsigned short(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value));
    }
    operator short(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value));
    }
#if SIZEOF_LONG == 8
    operator unsigned long int(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt8(m_Value));
    }
    operator long int(void) const
    {
        return MakeCP<CP>(NStr::StringToInt8(m_Value));
    }
#else
    operator unsigned long int(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value));
    }
    // long already handled above
#endif
    operator unsigned long long(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt8(m_Value));
    }
    operator long long(void) const
    {
        return MakeCP<CP>(NStr::StringToInt8(m_Value));
    }
#endif

private:
    const obj_type  m_Value;
};

// Same as CValueConvert<string>
template <typename CP>
class CValueConvert<CP, const char*>
{
public:
    typedef const char* obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
    operator bool(void) const
    { 
        return MakeCP<CP>(NStr::StringToBool(m_Value));
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value, NStr::fAllowTrailingSymbols));
    }
#if !defined(NCBI_INT8_IS_LONG)  &&  \
    ((defined(NCBI_COMPILER_GCC) && NCBI_COMPILER_VERSION >= 400)  ||  \
     defined(NCBI_COMPILER_ICC)  ||  defined(NCBI_COMPILER_WORKSHOP) || \
     defined(NCBI_COMPILER_MSVC))
    operator long(void) const
    {
        return MakeCP<CP>(NStr::StringToLong(m_Value, NStr::fAllowTrailingSymbols));
    }
#endif
    operator Uint8(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt8(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(NStr::StringToInt8(m_Value, NStr::fAllowTrailingSymbols));
    }
    operator float(void) const
    {
        return MakeCP<CP>(NStr::StringToDouble(m_Value));
    }
    operator double(void) const
    {
        return MakeCP<CP>(NStr::StringToDouble(m_Value));
    }
    operator long double(void) const
    {
        return MakeCP<CP>(NStr::StringToDouble(m_Value));
    }
    operator CTime(void) const
    {
        return CTime(m_Value);
    }

#if defined(NCBI_COMPILER_GCC) && NCBI_COMPILER_VERSION < 400
    operator char(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value));
    }
    operator unsigned char(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value));
    }
    operator unsigned short(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value));
    }
    operator short(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value));
    }
    operator unsigned long int(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt(m_Value));
    }
    operator long int(void) const
    {
        return MakeCP<CP>(NStr::StringToInt(m_Value));
    }
    operator unsigned long long(void) const
    {
        return MakeCP<CP>(NStr::StringToUInt8(m_Value));
    }
    operator long long(void) const
    {
        return MakeCP<CP>(NStr::StringToInt8(m_Value));
    }
#endif

private:
    obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, bool>
{
public:
    typedef bool obj_type;

    CValueConvert(obj_type value)
        : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value ? 1 : 0;
    }
    operator Uint1(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int1(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint2(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int2(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint4(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int4(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint8(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int8(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator float(void) const
    {
        return m_Value ? 1.0 : 0.0;
    }
    operator double(void) const
    {
        return m_Value ? 1.0 : 0.0;
    }
    operator long double(void) const
    {
        return m_Value ? 1.0 : 0.0;
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return m_Value ? static_cast<TO>(1) : static_cast<TO>(0);
    }
#endif

    operator string(void) const
    {
        return NStr::BoolToString(m_Value);
    }

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, Uint1>
{
public:
    typedef Uint1 obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        return NStr::UIntToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(m_Value);
    }

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, Int1>
{
public:
    typedef Int1 obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        return NStr::IntToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(m_Value);
    }

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, Uint2>
{
public:
    typedef Uint2 obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        return NStr::UIntToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(m_Value);
    }

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, Int2>
{
public:
    typedef Int2 obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#if NCBI_PLATFORM_BITS == 32
    operator time_t(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        return NStr::IntToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(m_Value);
    }

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, Uint4>
{
public:
    typedef Uint4 obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        return NStr::UIntToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(MakeCP<CP>(m_Value));
    }

private:
    const obj_type m_Value;
};

template <typename CP>
class CValueConvert<CP, Int4>
{
public:
    typedef Int4 obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#if NCBI_PLATFORM_BITS == 32
    operator time_t(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        return NStr::IntToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(MakeCP<CP>(m_Value));
    }

private:
    const obj_type  m_Value;
};

#if SIZEOF_LONG == 8  &&  !defined(NCBI_INT8_IS_LONG)
template <typename CP>
class CValueConvert<CP, unsigned long>
{
public:
    typedef unsigned long obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator string(void) const
    {
        return NStr::ULongToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(MakeCP<CP>(m_Value));
    }

private:
    const obj_type m_Value;
};

template <typename CP>
class CValueConvert<CP, long>
{
public:
    typedef long obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator string(void) const
    {
        return NStr::LongToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(MakeCP<CP>(m_Value));
    }

private:
    const obj_type m_Value;
};
#endif

template <typename CP>
class CValueConvert<CP, Uint8>
{
public:
    typedef Uint8 obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        return NStr::UInt8ToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(MakeCP<CP>(m_Value));
    }

private:
    const obj_type m_Value;
};

template <typename CP>
class CValueConvert<CP, Int8>
{
public:
    typedef Int8 obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#if NCBI_PLATFORM_BITS == 32
    operator time_t(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        return NStr::Int8ToString(m_Value);
    }
    operator CTime(void) const
    {
        return CTime(MakeCP<CP>(m_Value));
    }

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, float>
{
public:
    typedef float obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_MSVC)
    operator bool(void) const
    { 
        return m_Value != 0.0F;
    }
#endif
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0.0F;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#if NCBI_PLATFORM_BITS == 32
    operator time_t(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        string value;
        NStr::DoubleToString(value, m_Value);

        return value;
    }

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, double>
{
public:
    typedef double obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_MSVC)
    operator bool(void) const
    { 
        return m_Value != 0.0;
    }
#endif
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0.0;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#if NCBI_PLATFORM_BITS == 32
    operator time_t(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif
    operator string(void) const
    {
        string value;
        NStr::DoubleToString(value, m_Value);

        return value;
    }

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, long double>
{
public:
    typedef long double obj_type;

    CValueConvert(obj_type value)
    : m_Value(value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return m_Value != 0.0L;
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator long double(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value);
    }
#endif

private:
    const obj_type  m_Value;
};

template <typename CP>
class CValueConvert<CP, CTime>
{
public:
    typedef CTime obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(&value)
    {
    }

public:
#if defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550
    operator bool(void) const
    { 
        return !m_Value->IsEmpty();
    }
    operator Uint1(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator Int1(void) const
    {
        return MakeCP<CP>(m_Value);
    }
    operator Uint2(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator Int2(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator Uint4(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator Int4(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator Uint8(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator Int8(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator float(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator double(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
#else
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
#endif

    // Convert to itself.
    operator const obj_type&(void) const
    {
        return MakeCP<CP>(*m_Value);
    }
    operator string(void) const
    {
        return m_Value->AsString();
    }

private:
    const obj_type* m_Value;
};

////////////////////////////////////////////////////////////////////////////////
// Specializations for conversion to bool.
#if !(defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550)
template <> template <>
inline
CValueConvert<SSafeCP, Uint1>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SRunTimeCP, Uint1>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SSafeCP, Int1>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SRunTimeCP, Int1>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SSafeCP, Uint2>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SRunTimeCP, Uint2>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SSafeCP, Int2>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SRunTimeCP, Int2>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SSafeCP, Uint4>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SRunTimeCP, Uint4>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SSafeCP, Int4>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SRunTimeCP, Int4>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SSafeCP, Uint8>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SRunTimeCP, Uint8>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SSafeCP, Int8>::operator bool(void) const
{   
        return m_Value != 0;
}

template <> template <>
inline
CValueConvert<SRunTimeCP, Int8>::operator bool(void) const
{   
        return m_Value != 0;
}

// CTime has "empty" value semantic.
template <> template <>
inline
CValueConvert<SRunTimeCP, CTime>::operator bool(void) const
{   
		return !m_Value->IsEmpty();
}

#endif

} // namespace value_slice

#if defined(NCBI_COMPILER_WORKSHOP) || \
    (defined(NCBI_COMPILER_MSVC) && (_MSC_VER < 1400))
namespace value_slice
{

template <typename CP, typename FROM>
inline
bool operator !(CValueConvert<CP, FROM> const& value)
{
    const bool bool_expr = value;
    return !bool_expr;
}

template <
    typename CP1, 
    typename CP2, 
    typename FROM1, 
    typename FROM2
    >
inline
bool operator &&(CValueConvert<CP1, FROM1> const& l, CValueConvert<CP2, FROM2> const& r)
{
    const bool l_expr = l;

    if (!l) {
        return false;
    }

    return r;
}

template <
    typename CP1, 
    typename CP2, 
    typename FROM1,
    typename FROM2
    >
inline
bool operator ||(CValueConvert<CP1, FROM1> const& l, CValueConvert<CP2, FROM2> const& r)
{
    const bool l_expr = l;

    if (l) {
        return true;
    }

    return r;
}

template <typename CP, typename FROM>
inline
bool operator &&(bool l, CValueConvert<CP, FROM> const& r)
{
    if (!l) {
        return false;
    }

    return r;
}

template <typename CP, typename FROM>
inline
bool operator &&(CValueConvert<CP, FROM> const& l, bool r)
{
    const bool l_expr = l;
    return l_expr && r;
}

template <typename CP, typename FROM>
inline
bool operator ||(bool l, CValueConvert<CP, FROM> const& r)
{
    if (l) {
        return true;
    }

    return r;
}

template <typename CP, typename FROM>
inline
bool operator ||(CValueConvert<CP, FROM> const& l, bool r)
{
    const bool l_expr = l;
    return l_expr || r;
}

}

#endif

////////////////////////////////////////////////////////////////////////////////
template <typename CP, typename FROM>
inline
string operator+(const string& s, const value_slice::CValueConvert<CP, FROM>& value)
{
    string str_value(s);

    str_value += value.operator string();
    return str_value;
}

template <typename CP, typename FROM>
inline
string operator+(const value_slice::CValueConvert<CP, FROM>& value, const string& s)
{
    string str_value = value.operator string();

    str_value += s;
    return str_value;
}

template <typename CP, typename FROM>
inline
string operator+(const char* s, const value_slice::CValueConvert<CP, FROM>& value)
{
    string str_value;

    if (s) {
        str_value += s;
    }

    str_value += value.operator string();
    
    return str_value;
}

template <typename CP, typename FROM>
inline
string operator+(const value_slice::CValueConvert<CP, FROM>& value, const char* s)
{
    string str_value = value.operator string();

    if (s) {
        str_value += s;
    }

    return str_value;
}

template <typename CP, typename FROM>
inline
string& operator+=(string& s, const value_slice::CValueConvert<CP, FROM>& value)
{
    s += value.operator string();
    return s;
}

////////////////////////////////////////////////////////////////////////////////
// A limited case ...
template <typename FROM>
inline
const value_slice::CValueConvert<value_slice::SRunTimeCP, FROM> 
Convert(const FROM& value)
{
    return value_slice::CValueConvert<value_slice::SRunTimeCP, FROM>(value);
}

#if SIZEOF_LONG == 4
inline
const value_slice::CValueConvert<value_slice::SRunTimeCP, Int4>
Convert(long value)
{
    return value_slice::CValueConvert<value_slice::SRunTimeCP, Int4>(value);
}

inline
const value_slice::CValueConvert<value_slice::SRunTimeCP, Uint4>
Convert(unsigned long value)
{
    return value_slice::CValueConvert<value_slice::SRunTimeCP, Uint4>(value);
}
#endif

template <typename FROM>
inline
const value_slice::CValueConvert<value_slice::SRunTimeCP, FROM> 
Convert(FROM& value)
{
    return value_slice::CValueConvert<value_slice::SRunTimeCP, FROM>(value);
}

////////////////////////////////////////////////////////////////////////////////
// Safe (compile-time) conversion ...
template <typename FROM>
inline
const value_slice::CValueConvert<value_slice::SSafeCP, FROM> 
ConvertSafe(const FROM& value)
{
    return value_slice::CValueConvert<value_slice::SSafeCP, FROM>(value);
}

#if SIZEOF_LONG == 4
inline
const value_slice::CValueConvert<value_slice::SSafeCP, Int4>
ConvertSafe(long value)
{
    return value_slice::CValueConvert<value_slice::SSafeCP, Int4>(value);
}

inline
const value_slice::CValueConvert<value_slice::SSafeCP, Uint4>
ConvertSafe(unsigned long value)
{
    return value_slice::CValueConvert<value_slice::SSafeCP, Uint4>(value);
}
#endif

template <typename FROM>
inline
const value_slice::CValueConvert<value_slice::SSafeCP, FROM> 
ConvertSafe(FROM& value)
{
    return value_slice::CValueConvert<value_slice::SSafeCP, FROM>(value);
}

END_NCBI_SCOPE


#endif // UTIL___VALUE_CONVERT__HPP
