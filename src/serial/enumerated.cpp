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
* Revision 1.7  2000/01/10 19:46:39  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.6  2000/01/05 19:43:53  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.5  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.4  1999/10/28 15:37:40  vasilche
* Fixed null choice pointers handling.
* Cleaned enumertion interface.
*
* Revision 1.3  1999/10/19 13:43:07  vasilche
* Fixed error on IRIX
*
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

#include <corelib/ncbiutil.hpp>
#include <serial/enumerated.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE

CEnumeratedTypeValues::CEnumeratedTypeValues(const string& name,
                                             bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::CEnumeratedTypeValues(const char* name,
                                             bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::~CEnumeratedTypeValues(void)
{
}

long CEnumeratedTypeValues::FindValue(const string& name) const
{
    return FindValue(name.c_str());
}

long CEnumeratedTypeValues::FindValue(const char* name) const
{
    const TNameToValue& m = NameToValue();
    TNameToValue::const_iterator i = m.find(name);
    if ( i == m.end() ) {
        THROW1_TRACE(runtime_error,
                     "invalid value of enumerated type");
    }
    return i->second;
}

const string& CEnumeratedTypeValues::FindName(long value,
                                              bool allowBadValue) const
{
    const TValueToName& m = ValueToName();
    TValueToName::const_iterator i = m.find(value);
    if ( i == m.end() ) {
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
    m_Values.push_back(make_pair(name, value));
    m_ValueToName.reset(0);
    m_NameToValue.reset(0);
}

const CEnumeratedTypeValues::TValueToName&
CEnumeratedTypeValues::ValueToName(void) const
{
    TValueToName* m = m_ValueToName.get();
    if ( !m ) {
        m_ValueToName.reset(m = new TValueToName);
        iterate ( TValues, i, m_Values ) {
            (*m)[i->second] = i->first;
        }
    }
    return *m;
}

const CEnumeratedTypeValues::TNameToValue&
CEnumeratedTypeValues::NameToValue(void) const
{
    TNameToValue* m = m_NameToValue.get();
    if ( !m ) {
        m_NameToValue.reset(m = new TNameToValue);
        iterate ( TValues, i, m_Values ) {
            pair<TNameToValue::iterator, bool> p =
                m->insert(TNameToValue::value_type(i->first.c_str(),
                                                   i->second));
            if ( !p.second ) {
                THROW1_TRACE(runtime_error,
                             "duplicated enum value name " + i->first);
            }
        }
    }
    return *m;
}

void CEnumeratedTypeValues::AddValue(const char* name, long value)
{
    AddValue(string(name), value);
}

TTypeInfo CEnumeratedTypeValues::GetTypeInfoForSize(size_t size,
                                                    long /* dummy */) const
{
    if ( size == sizeof(int) )
        return new CEnumeratedTypeInfoTmpl<int>(this);
    if ( size == sizeof(short) )
        return new CEnumeratedTypeInfoTmpl<short>(this);
    if ( size == sizeof(signed char) )
        return new CEnumeratedTypeInfoTmpl<signed char>(this);
    if ( size == sizeof(long) )
        return new CEnumeratedTypeInfoTmpl<long>(this);
    THROW1_TRACE(runtime_error,
                 "Illegal enum size: " + NStr::UIntToString(size));
}

END_NCBI_SCOPE
