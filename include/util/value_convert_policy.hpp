#ifndef UTIL___VALUE_CONV_POLICY__HPP
#define UTIL___VALUE_CONV_POLICY__HPP

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


#include <corelib/ncbitime.hpp>
#include <corelib/ncbiexpt.hpp>

#include <limits>

BEGIN_NCBI_SCOPE


class CInvalidConversionException : public CException
{
public:
    CInvalidConversionException()
    {
    }

    virtual const char* GetErrCodeString(void) const
    {
        return "Invalid run-time type conversion."; 
    }
}; 


namespace value_slice
{

////////////////////////////////////////////////////////////////////////////////
// Allowed conversions ...

////////////////////////////////////////////////////////////////////////////////
// Conversion policies.
struct SSafeCP {};
struct SRunTimeCP {};

////////////////////////////////////////////////////////////////////////////////
// Forward declaration.
//
template <typename CP, typename FROM> class CConvPolicy;

////////////////////////////////////////////////////////////////////////////////
inline
void ReportConversionError(void)
{
    throw CInvalidConversionException();
}

////////////////////////////////////////////////////////////////////////////////
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
    template <typename OT>
    operator OT(void) const
    {
        if (m_Value > numeric_limits<OT>::max() || m_Value < numeric_limits<OT>::min()) {
            ReportConversionError();
        }

        return static_cast<OT>(m_Value);
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
    template <typename OT>
    operator OT(void) const
    {
        if (m_Value > numeric_limits<OT>::max() || m_Value < numeric_limits<OT>::min()) {
            ReportConversionError();
        }

        return static_cast<OT>(m_Value);
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
    template <typename OT>
    operator OT(void) const
    {
        if (m_Value > numeric_limits<OT>::max() || m_Value < numeric_limits<OT>::min()) {
            ReportConversionError();
        }

        return static_cast<OT>(m_Value);
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
    template <typename OT>
    operator OT(void) const
    {
        if (m_Value > numeric_limits<OT>::max() || m_Value < numeric_limits<OT>::min()) {
            ReportConversionError();
        }

        return static_cast<OT>(m_Value);
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
    template <typename OT>
    operator OT(void) const
    {
        if (m_Value > numeric_limits<OT>::max() || m_Value < numeric_limits<OT>::min()) {
            ReportConversionError();
        }

        return static_cast<OT>(m_Value);
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
    template <typename OT>
    operator OT(void) const
    {
        if (m_Value > numeric_limits<OT>::max() || m_Value < numeric_limits<OT>::min()) {
            ReportConversionError();
        }

        return static_cast<OT>(m_Value);
    }

private:
    obj_type m_Value;
};

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
    template <typename OT>
    operator OT(void) const
    {
        if (m_Value > numeric_limits<OT>::max() || m_Value < numeric_limits<OT>::min()) {
            ReportConversionError();
        }

        return static_cast<OT>(m_Value);
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
    template <typename OT>
    operator OT(void) const
    {
        if (m_Value > numeric_limits<OT>::max() || m_Value < numeric_limits<OT>::min()) {
            ReportConversionError();
        }

        return static_cast<OT>(m_Value);
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
class CConvPolicy<SRunTimeCP, double>
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

    /*
    operator long double(void) const
    {
        return m_Value;
    }
    */

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
    operator bool(void) const
    {
        return m_Value;
    }
    operator float(void) const
    {
        return m_Value;
    }
    operator double(void) const
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
    operator bool(void) const
    {
        return m_Value;
    }
    operator float(void) const
    {
        return m_Value;
    }
    operator double(void) const
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
    operator bool(void) const
    {
        return m_Value;
    }
    operator float(void) const
    {
        return m_Value;
    }
    operator double(void) const
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
    operator bool(void) const
    {
        return m_Value;
    }
    operator float(void) const
    {
        return m_Value;
    }
    operator double(void) const
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
    operator bool(void) const
    {
        return m_Value;
    }
    operator float(void) const
    {
        return static_cast<float>(m_Value);
    }
    operator double(void) const
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
    operator bool(void) const
    {
        return m_Value;
    }
    operator float(void) const
    {
        return static_cast<float>(m_Value);
    }
    operator double(void) const
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
    operator bool(void) const
    {
        return m_Value;
    }
    operator float(void) const
    {
        return static_cast<float>(m_Value);
    }
    operator double(void) const
    {
        return static_cast<double>(m_Value);
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
    operator bool(void) const
    {
        return m_Value;
    }
    operator float(void) const
    {
        return static_cast<float>(m_Value);
    }
    operator double(void) const
    {
        return static_cast<double>(m_Value);
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

private:
    const obj_type* m_Value;
};

} // namespace value_slice

END_NCBI_SCOPE


#endif // UTIL___VALUE_CONV_POLICY__HPP

