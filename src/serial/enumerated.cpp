/*  $Id$
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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/10/18 20:21:40  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.1  1999/09/24 18:20:07  vasilche
* Removed dependency on NCBI toolkit.
*
*
* ===========================================================================
*/

#include <serial/enumerated.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE

CEnumeratedTypeValues::CEnumeratedTypeValues(const string& name, bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::~CEnumeratedTypeValues(void)
{
}

long CEnumeratedTypeValues::FindValue(const string& name) const
{
    TNameToValue::const_iterator i = m_NameToValue.find(name);
    if ( i == m_NameToValue.end() )
        THROW1_TRACE(runtime_error,
                     "invalid value of enumerated type");
    return i->second;
}

const string& CEnumeratedTypeValues::FindName(long value,
                                              bool allowBadValue) const
{
    TValueToName::const_iterator i = m_ValueToName.find(value);
    if ( i == m_ValueToName.end() ) {
        if ( allowBadValue ) {
            return NcbiEmptyString;
        }
        else {
            THROW1_TRACE(runtime_error,
                         "invalid value of enumerated type");
        }
    }
    return i->second;
}

void CEnumeratedTypeValues::AddValue(const string& name, long value)
{
    if ( name.empty() )
        THROW1_TRACE(runtime_error, "empty enum value name");
    pair<TNameToValue::iterator, bool> p1 =
        m_NameToValue.insert(TNameToValue::value_type(name, value));
    if ( !p1.second )
        THROW1_TRACE(runtime_error,
                     "duplicated enum value name " + name);
    pair<TValueToName::iterator, bool> p2 =
        m_ValueToName.insert(TValueToName::value_type(value, name));
    if ( !p2.second ) {
        m_NameToValue.erase(p1.first);
        THROW1_TRACE(runtime_error,
                     "duplicated enum value " + name);
    }
}

pair<long, bool> CEnumeratedTypeValues::ReadEnum(CObjectIStream& in) const
{
    string name = in.ReadEnumName();
    if ( !name.empty() ) {
        // enum element by name
        return make_pair(FindValue(name), true);
    }
    else if ( !IsInteger() ) {
        // enum element by value
        long value = in.ReadEnumValue();
        FindName(value, false);
        return make_pair(value, true);
    }
    // plain integer
    return make_pair(0, false);
}

bool CEnumeratedTypeValues::WriteEnum(CObjectOStream& out, long value) const
{
    const string& name = FindName(value, IsInteger());
    if ( name.empty() ) {
        // non enum value and (isInteger == true)
        return false;
    }
    if ( !out.WriteEnumName(name) )
        out.WriteEnumValue(value);
    return true;
}

TTypeInfo CreateEnumeratedTypeInfoForSize(size_t size, long /* dummy */,
                                          const CEnumeratedTypeValues* enumInfo)
{
    if ( size == sizeof(int) )
        return new CEnumeratedTypeInfoTmpl<int>(enumInfo);
    if ( size == sizeof(short) )
        return new CEnumeratedTypeInfoTmpl<short>(enumInfo);
    if ( size == sizeof(char) )
        return new CEnumeratedTypeInfoTmpl<char>(enumInfo);
    if ( size == sizeof(long) )
        return new CEnumeratedTypeInfoTmpl<long>(enumInfo);
    THROW1_TRACE(runtime_error, "Illegal enum size: " + NStr::UIntToString(size));
}

END_NCBI_SCOPE
