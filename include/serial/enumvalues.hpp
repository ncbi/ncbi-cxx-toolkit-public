#ifndef ENUMVALUES__HPP
#define ENUMVALUES__HPP

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
* Revision 1.2  2000/06/01 19:06:55  vasilche
* Added parsing of XML data.
*
* Revision 1.1  2000/05/24 20:08:12  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/lightstr.hpp>
#include <list>
#include <map>
#include <memory>

BEGIN_NCBI_SCOPE

class CEnumeratedTypeValues
{
public:
    typedef list< pair<string, long> > TValues;
    typedef map<CLightString, long> TNameToValue;
    typedef map<long, const string*> TValueToName;

    CEnumeratedTypeValues(const string& name, bool isInteger);
    CEnumeratedTypeValues(const char* name, bool isInteger);
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
    void AddValue(const char* name, long value);

    // returns value of enum element, if found
    // otherwise, throws exception
    long FindValue(const CLightString& name) const;

    // returns name of enum element, if found
    // otherwise, if (allowBadValue == true) returns empty string,
    // otherwise, throws exception
    const string& FindName(long value, bool allowBadValue) const;

    TTypeInfo GetTypeInfoForSize(size_t size, long /* dummy */) const;

    const TNameToValue& NameToValue(void) const;
    const TValueToName& ValueToName(void) const;

private:
    string m_Name;
    bool m_Integer;
    TValues m_Values;
    mutable auto_ptr<TNameToValue> m_NameToValue;
    mutable auto_ptr<TValueToName> m_ValueToName;
};

//#include <serial/enumvalues.inl>

END_NCBI_SCOPE

#endif  /* ENUMVALUES__HPP */
