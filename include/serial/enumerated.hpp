#ifndef ENUMERATED__HPP
#define ENUMERATED__HPP

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
* Revision 1.3  1999/10/28 15:37:37  vasilche
* Fixed null choice pointers handling.
* Cleaned enumertion interface.
*
* Revision 1.2  1999/10/18 20:11:15  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.1  1999/09/24 18:20:05  vasilche
* Removed dependency on NCBI toolkit.
*
*
* ===========================================================================
*/

#include <serial/stdtypes.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CEnumeratedTypeValues
{
public:
    typedef map<string, long> TNameToValue;
    typedef map<long, string> TValueToName;

    CEnumeratedTypeValues(const string& name, bool isInteger);
    ~CEnumeratedTypeValues(void);

    const string& GetName(void) const
        {
            return m_Name;
        }
    bool IsInteger(void) const
        {
            return m_Integer;
        }

    void AddValue(const string& name, long value);

    // returns value of enum element, if found
    // otherwise, throws exception
    long FindValue(const string& name) const;

    // returns name of enum element, if found
    // otherwise, if (allowBadValue == true) returns empty string,
    // otherwise, throws exception
    const string& FindName(long value, bool allowBadValue) const;

    // tries to read enum element. Success flag is in second member,
    // value is in first member
    pair<long, bool> ReadEnum(CObjectIStream& in) const;

    // tries to write enum element. Returns success flag
    bool WriteEnum(CObjectOStream& out, long value) const;

    TTypeInfo GetTypeInfoForSize(size_t size, long /* dummy */) const;

private:
    string m_Name;
    bool m_Integer;
    TNameToValue m_NameToValue;
    TValueToName m_ValueToName;
};

template<typename T>
class CEnumeratedTypeInfoTmpl : public CStdTypeInfo<T>
{
    typedef CStdTypeInfo<T> CParent;
public:

    // values should exist for all live time of our instance
    CEnumeratedTypeInfoTmpl(const CEnumeratedTypeValues* values)
        : CParent(values->GetName()), m_Values(*values)
        {
        }

    const CEnumeratedTypeValues& Values(void) const
        {
            return m_Values;
        }

protected:
    void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            pair<long, bool> value = Values().ReadEnum(in);
            if ( value.second ) {
                // value already read
                Get(object) = T(value.first);
            }
            else {
                // plain integer
                CParent::ReadData(in, object);
            }
        }
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            if ( !Values().WriteEnum(out, Get(object)) ) {
                // plain integer
                CParent::WriteData(out, object);
            }
        }

private:
    const CEnumeratedTypeValues& m_Values;
};

template<typename T>
inline
TTypeInfo CreateEnumeratedTypeInfo(const T& dummy,
                                   const CEnumeratedTypeValues* info)
{
    return info->GetTypeInfoForSize(sizeof(T), dummy);
}

// standard template for plain enums
typedef CEnumeratedTypeInfoTmpl<int> CEnumeratedTypeInfo;

END_NCBI_SCOPE

#endif  /* ENUMERATED__HPP */
