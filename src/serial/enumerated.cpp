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

CEnumeratedTypeInfo::CEnumeratedTypeInfo(const string& name, bool isInteger)
    : CParent(name), m_Integer(isInteger)
{
}

int CEnumeratedTypeInfo::FindValue(const string& name) const
{
    TNameToValue::const_iterator i = m_NameToValue.find(name);
    if ( i == m_NameToValue.end() )
        THROW1_TRACE(runtime_error,
                     "invalid value of enumerated type");
    return i->second;
}

const string& CEnumeratedTypeInfo::FindName(TValue value) const
{
    TValueToName::const_iterator i = m_ValueToName.find(value);
    if ( i == m_ValueToName.end() )
        THROW1_TRACE(runtime_error,
                     "invalid value of enumerated type");
    return i->second;
}

void CEnumeratedTypeInfo::AddValue(const string& name, TValue value)
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

void CEnumeratedTypeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    string name = in.ReadEnumName();
    if ( name.empty() ) {
        if ( m_Integer ) {
            in.ReadStd(Get(object));
        }
        else {
            FindName(Get(object) = in.ReadEnumValue());
        }
    }
    else {
        Get(object) = FindValue(name);
    }
}

void CEnumeratedTypeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    const string& name = FindName(Get(object));
    if ( !out.WriteEnumName(name) ) {
        if ( m_Integer )
            out.WriteStd(Get(object));
        else
            out.WriteEnumValue(Get(object));
    }
}

END_NCBI_SCOPE
