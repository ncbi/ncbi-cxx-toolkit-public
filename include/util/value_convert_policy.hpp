#ifndef UTIL___VALUE_CONV_POLICY__HPP
#define UTIL___VALUE_CONV_POLICY__HPP

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


#include <corelib/ncbitime.hpp>
#include <corelib/ncbiexpt.hpp>

#include <limits>

BEGIN_NCBI_SCOPE


class CInvalidConversionException : public CException
{
public:
    enum EErrCode {
        eInvalidConversion
    };
    virtual const char* GetErrCodeString(void) const
    {
        return "Invalid run-time type conversion."; 
    }

    NCBI_EXCEPTION_DEFAULT(CInvalidConversionException, CException);
}; 

#define NCBI_REPORT_CONVERSION_ERROR(x) \
    NCBI_THROW(CInvalidConversionException, eInvalidConversion, \
               FORMAT("Invalid run-time type conversion (unable to convert " \
               << x << ")."))

#define NCBI_REPORT_CONSTANT_CONVERSION_ERROR(x) \
    NCBI_THROW(CInvalidConversionException, eInvalidConversion, \
               "Invalid run-time type conversion (unable to convert " x ").")

namespace value_slice
{

////////////////////////////////////////////////////////////////////////////////
// Allowed conversions ...

////////////////////////////////////////////////////////////////////////////////
// Conversion policies.
struct SSafeCP {};
struct SRunTimeCP {};

////////////////////////////////////////////////////////////////////////////////
// Range checking ...
template <bool x_is_signed, bool y_is_signed>
struct SLessThanTypeMin
{
    template <class X, class Y>
    static bool Check(X x, Y y_min)
    { 
        return x < y_min; 
    }
}; 

template <>
struct SLessThanTypeMin<false, true>
{
    template <class X, class Y>
    static bool Check(X, Y)
    { 
        return false; 
    }
}; 

template <>
struct SLessThanTypeMin<true, false>
{
    template <class X, class Y>
    static bool Check(X x, Y)
    { 
        return x < 0; 
    }
}; 

template <bool same_sign, bool x_is_signed> struct SGreaterThanTypeMax; 

template <>
struct SGreaterThanTypeMax<true, true>
{
    template <class X, class Y>
    static bool Check(X x, Y y_max)
    { 
        return x > y_max; 
    }
}; 

template <>
struct SGreaterThanTypeMax<false, true>
{
    template <class X, class Y>
    static bool Check(X x, Y)
    { 
        return x >= 0 && static_cast<X>(static_cast<Y>(x)) != x; 
    } 
};

template<>
struct SGreaterThanTypeMax<true, false>
{
    template <class X, class Y>
    static bool Check(X x, Y y_max)
    { 
        return x > y_max; 
    }
}; 

template <>
struct SGreaterThanTypeMax<false, false>
{
    template <class X, class Y>
    static bool Check(X x, Y)
    { 
        const Y y = static_cast<Y>(x);
        return y < 0 || static_cast<X>(y) != x;
    }
}; 

////////////////////////////////////////////////////////////////////////////////
// Forward declaration.
//
template <typename CP, typename FROM> class CConvPolicy;


////////////////////////////////////////////////////////////////////////////////
template <bool to_is_integer, bool from_is_integer>
struct SConvertUsingRunTimeCP
{
    template <typename TO, typename FROM> 
    static 
    TO Convert(const FROM& value)
    {
        const TO min_allowed = kMin_Auto, max_allowed = kMax_Auto;
        if (value < min_allowed  ||  value > max_allowed) {
            NCBI_REPORT_CONVERSION_ERROR(value);
        }

        return static_cast<TO>(value);
    }
};


template <>
struct SConvertUsingRunTimeCP<true, true>
{
    template <typename TO, typename FROM> 
    static 
    TO Convert(const FROM& value)
    {
        const bool from_is_signed = numeric_limits<FROM>::is_signed;
        const bool to_is_signed   = numeric_limits<TO  >::is_signed;
        const bool same_sign      = from_is_signed == to_is_signed; 

        const TO min_allowed = kMin_Auto, max_allowed = kMax_Auto;

        if (SLessThanTypeMin<from_is_signed, to_is_signed>::Check(
                value, min_allowed)
            ||  SGreaterThanTypeMax<same_sign, from_is_signed>::Check(
                value, max_allowed)) {
            NCBI_REPORT_CONVERSION_ERROR(value);
        }

        return static_cast<TO>(value);
    }
};

////////////////////////////////////////////////////////////////////////////////
template <typename TO, typename FROM> 
inline
TO ConvertUsingRunTimeCP(const FROM& value)
{
    const bool to_is_integer   = numeric_limits<TO>::is_integer;
    const bool from_is_integer = numeric_limits<FROM>::is_integer;

    return SConvertUsingRunTimeCP<
        to_is_integer, 
        from_is_integer
        >::template Convert<TO, FROM>(value);
}

////////////////////////////////////////////////////////////////////////////////
// We are trying to avoid partial specialization.
//
template <>
class CConvPolicy<SRunTimeCP, bool>
{
public:
    typedef bool obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    operator Int1(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int2(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int4(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int8(void) const
    {
        return m_Value ? 1 : 0;
    }

    operator Uint1(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint2(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint4(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint8(void) const
    {
        return m_Value ? 1 : 0;
    }

    operator float(void) const
    {
        return m_Value ? static_cast<float>(1) : static_cast<float>(0);
    }
    operator double(void) const
    {
        return m_Value ? static_cast<double>(1) : static_cast<double>(0);
    }
    operator long double(void) const
    {
        return m_Value ? static_cast<long double>(1) : static_cast<long double>(0);
    }

private:
    const obj_type m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, Int1>
{
public:
    typedef Int1 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, Uint1>
{
public:
    typedef Uint1 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, Int2>
{
public:
    typedef Int2 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, Uint2>
{
public:
    typedef Uint2 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, Int4>
{
public:
    typedef Int4 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, Uint4>
{
public:
    typedef Uint4 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};

#ifndef NCBI_INT8_IS_LONG
template <>
class CConvPolicy<SRunTimeCP, long>
{
public:
    typedef long obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};
#endif

template <>
class CConvPolicy<SRunTimeCP, Int8>
{
public:
    typedef Int8 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, Uint8>
{
public:
    typedef Uint8 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, string>
{
public:
    typedef string obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    // Convert only to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

private:
    const obj_type& m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, float>
{
public:
    typedef float obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    const obj_type& m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, double>
{ 
public:
    typedef double obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    const obj_type& m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, long double>
{ 
public:
    typedef long double obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return ConvertUsingRunTimeCP<TO>(m_Value);
    }

private:
    const obj_type& m_Value;
};

template <>
class CConvPolicy<SRunTimeCP, CTime>
{
public:
    typedef CTime obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(&value)
    {
    }

public:
    // Convert only to itself.
    operator const obj_type&(void) const
    {
        return *m_Value;
    }

private:
    const obj_type* m_Value;
};


////////////////////////////////////////////////////////////////////////////////
template <>
class CConvPolicy<SSafeCP, bool>
{
public:
    typedef bool obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    // Unsigned to signed ...

    operator Int1(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int2(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int4(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Int8(void) const
    {
        return m_Value ? 1 : 0;
    }

    // Unsigned to unsigned ...

    operator Uint1(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint2(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint4(void) const
    {
        return m_Value ? 1 : 0;
    }
    operator Uint8(void) const
    {
        return m_Value ? 1 : 0;
    }


private:
    const obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, Int1>
{
public:
    typedef Int1 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    // Signed to signed ...

    operator Int2(void) const
    {
        return m_Value;
    }
    operator Int4(void) const
    {
        return m_Value;
    }
    operator Int8(void) const
    {
        return m_Value;
    }

    //
    operator float(void) const
    {
        return m_Value;
    }
    operator double(void) const
    {
        return m_Value;
    }
    operator long double(void) const
    {
        return m_Value;
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, Uint1>
{
public:
    typedef Uint1 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    // Unsigned to signed ...

    operator Int2(void) const
    {
        return m_Value;
    }
    operator Int4(void) const
    {
        return m_Value;
    }
    operator Int8(void) const
    {
        return m_Value;
    }

    // Unsigned to unsigned ...

    operator Uint2(void) const
    {
        return m_Value;
    }
    operator Uint4(void) const
    {
        return m_Value;
    }
    operator Uint8(void) const
    {
        return m_Value;
    }

    //
    operator float(void) const
    {
        return m_Value;
    }
    operator double(void) const
    {
        return m_Value;
    }
    operator long double(void) const
    {
        return m_Value;
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, Int2>
{
public:
    typedef Int2 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    // Signed to signed ...

    operator Int4(void) const
    {
        return m_Value;
    }
    operator Int8(void) const
    {
        return m_Value;
    }

    //
    operator float(void) const
    {
        return m_Value;
    }
    operator double(void) const
    {
        return m_Value;
    }
    operator long double(void) const
    {
        return m_Value;
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, Uint2>
{
public:
    typedef Uint2 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    // Unsigned to signed ...

    operator Int4(void) const
    {
        return m_Value;
    }
    operator Int8(void) const
    {
        return m_Value;
    }

    // Unsigned to unsigned ...

    operator Uint4(void) const
    {
        return m_Value;
    }
    operator Uint8(void) const
    {
        return m_Value;
    }

    //
    operator float(void) const
    {
        return m_Value;
    }
    operator double(void) const
    {
        return m_Value;
    }
    operator long double(void) const
    {
        return m_Value;
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, Int4>
{
public:
    typedef Int4 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    // Signed to signed ...

    operator Int8(void) const
    {
        return m_Value;
    }

    //
    operator float(void) const
    {
        return static_cast<float>(m_Value);
    }
    operator double(void) const
    {
        return m_Value;
    }
    operator long double(void) const
    {
        return m_Value;
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, Uint4>
{
public:
    typedef Uint4 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    // Unsigned to signed ...

    operator Int8(void) const
    {
        return m_Value;
    }

    // Unsigned to unsigned ...

    operator Uint8(void) const
    {
        return m_Value;
    }

    //
    operator float(void) const
    {
        return static_cast<float>(m_Value);
    }
    operator double(void) const
    {
        return m_Value;
    }
    operator long double(void) const
    {
        return m_Value;
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, Int8>
{
public:
    typedef Int8 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert only to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    //
    operator float(void) const
    {
        return static_cast<float>(m_Value);
    }
    operator double(void) const
    {
        return static_cast<double>(m_Value);
    }
    operator long double(void) const
    {
        return static_cast<long double>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, Uint8>
{
public:
    typedef Uint8 obj_type;

    CConvPolicy(obj_type value)
    : m_Value(value)
    {
    }

public:
    // Convert only to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    //
    operator float(void) const
    {
        return static_cast<float>(m_Value);
    }
    operator double(void) const
    {
        return static_cast<double>(m_Value);
    }
    operator long double(void) const
    {
        return static_cast<long double>(m_Value);
    }

private:
    obj_type m_Value;
};

template <>
class CConvPolicy<SSafeCP, string>
{
public:
    typedef string obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    // Convert only to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

private:
    const obj_type& m_Value;
};

template <>
class CConvPolicy<SSafeCP, float>
{
public:
    typedef float obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    // Convert only to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    operator double(void) const
    {
        return m_Value;
    }
    operator long double(void) const
    {
        return m_Value;
    }

private:
    const obj_type& m_Value;
};

template <>
class CConvPolicy<SSafeCP, double>
{ 
public:
    typedef double obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(value)
    {
    }

public:
    // Convert only to itself.
    operator obj_type(void) const
    {
        return m_Value;
    }

    operator long double(void) const
    {
        return m_Value;
    }

private:
    const obj_type& m_Value;
};

template <>
class CConvPolicy<SSafeCP, CTime>
{
public:
    typedef CTime obj_type;

    CConvPolicy(const obj_type& value)
    : m_Value(&value)
    {
    }

public:
    // Convert only to itself.
    operator const obj_type&(void) const
    {
        return *m_Value;
    }

    operator bool(void) const
    {
        return !m_Value->IsEmpty();
    }

private:
    operator Int1(void) const
    {
        return 0;
    }
    operator Uint1(void) const
    {
        return 0;
    }

private:
    const obj_type* m_Value;
};

} // namespace value_slice

END_NCBI_SCOPE


#endif // UTIL___VALUE_CONV_POLICY__HPP

