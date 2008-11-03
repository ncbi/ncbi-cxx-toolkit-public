#ifndef DBAPI_DRIVER___DBAPI_OBJECT_CONVERT__HPP
#define DBAPI_DRIVER___DBAPI_OBJECT_CONVERT__HPP

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

#include <dbapi/driver/types.hpp>
#include <util/value_convert.hpp>

BEGIN_NCBI_SCOPE

namespace value_slice
{

////////////////////////////////////////////////////////////////////////////////
template <>
class NCBI_DBAPIDRIVER_EXPORT CValueConvert<SSafeCP, CDB_Object>
{
public: 
    typedef const CDB_Object obj_type;

    CValueConvert(const obj_type& value);

public:
    operator bool(void) const;
    operator Uint1(void) const;
    operator Int2(void) const;
    operator Int4(void) const;
    operator Int8(void) const;
    operator float(void) const;
    operator double(void) const;
    operator string(void) const;
    // operator CTime(void) const;
    operator const CTime&(void) const;

private:
    mutable obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <>
class NCBI_DBAPIDRIVER_EXPORT CValueConvert<SRunTimeCP, CDB_Object>
{
public: 
    typedef const CDB_Object obj_type;

    CValueConvert(const obj_type& value);

public:
    operator bool(void) const;
    operator Int1(void) const
    {
        return Convert(this->operator Int2());
    }
    operator Uint1(void) const;
    operator Int2(void) const;
    operator Uint2(void) const
    {
        return Convert(this->operator Uint4());
    }
    operator Int4(void) const;
    operator Uint4(void) const
    {
        return Convert(this->operator Uint8());
    }
    operator Int8(void) const;
    operator Uint8(void) const
    {
        return Convert(this->operator Int8());
    }
    operator float(void) const;
    operator double(void) const;
    operator string(void) const;
    // operator CTime(void) const;
    operator const CTime&(void) const;

private:
    mutable obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_Int>
{
public: 
    typedef CDB_Int obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_SmallInt>
{
public: 
    typedef CDB_SmallInt obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_TinyInt>
{
public: 
    typedef CDB_TinyInt obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_BigInt>
{
public: 
    typedef CDB_BigInt obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_VarChar>
{
public: 
    typedef CDB_VarChar obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_Char>
{
public: 
    typedef CDB_Char obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_LongChar>
{
public: 
    typedef CDB_LongChar obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_Float>
{
public: 
    typedef CDB_Float obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_Double>
{
public: 
    typedef CDB_Double obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_SmallDateTime>
{
public: 
    typedef CDB_SmallDateTime obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_DateTime>
{
public: 
    typedef CDB_DateTime obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_Bit>
{
public: 
    typedef CDB_Bit obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

////////////////////////////////////////////////////////////////////////////////
template <typename CP>
class CValueConvert<CP, CDB_Numeric>
{
public: 
    typedef CDB_Numeric obj_type;

    CValueConvert(const obj_type& value)
    : m_Value(value)
    {
        if (value.IsNULL()) {
            throw CInvalidConversionException();
        }
    }

public:
    template <typename TO>
    operator TO(void) const
    {
        return MakeCP<CP>(m_Value.Value());
    }

private:
    const obj_type& m_Value; 
};

// Still missing CDB_VarBinary, CDB_Binary, CDB_LongBinary, CDB_Stream, CDB_Text, CDB_Text, 

} // namespace value_slice

END_NCBI_SCOPE


#endif // DBAPI_DRIVER___DBAPI_OBJECT_CONVERT__HPP 
