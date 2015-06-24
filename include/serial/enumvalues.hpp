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
*   Description of enumerated data type values (named integers)
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <corelib/tempstr.hpp>
#include <list>
#include <map>
#include <memory>


/** @addtogroup FieldsComplex
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CEnumeratedTypeValues
{
public:
    typedef list< pair<string, TEnumValueType> > TValues;
    typedef map<CTempString, TEnumValueType, PQuickStringLess> TNameToValue;
    typedef map<TEnumValueType, const string*> TValueToName;

    enum EValueFlags {
        eNone        = 0,
        eHideIntName = 1
    };
    typedef unsigned int TValueFlags;    ///< Binary OR of EValueFlags

    CEnumeratedTypeValues(const char* name, bool isInteger);
    CEnumeratedTypeValues(const string& name, bool isInteger);
    ~CEnumeratedTypeValues(void);

    const string& GetName(void) const;
    /// Get ASN.1 module name
    const string& GetModuleName(void) const;
    /// Set ASN.1 module name
    void SetModuleName(const string& name);

    /// Check whether the type is defined as INTEGER in ASN.1 spec
    bool IsInteger(void) const
        {
            return m_Integer;
        }

    /// Check if this enum describes internal unnamed type
    bool IsInternal(void) const
        {
            return m_IsInternal;
        }
    /// Return internal type access string e.g. Int-fuzz.lim
    const string& GetInternalName(void) const;
    /// Return internal type's owner module name
    const string& GetInternalModuleName(void) const;
    /// Mark this enum as internal
    void SetInternalName(const string& name);

    /// Return internal or regular name
    const string& GetAccessName(void) const;
    /// Return internal or regular module name
    const string& GetAccessModuleName(void) const;

    /// Get the list of name-value pairs
    const TValues& GetValues(void) const
        {
            return m_Values;
        }

    /// Add name-value pair
    void AddValue(const string& name, TEnumValueType value, TValueFlags flags = eNone);
    /// Add name-value pair
    void AddValue(const char* name,   TEnumValueType value, TValueFlags flags = eNone);

    /// Find numeric value by the name of the enum
    ///
    /// @param name
    ///   Name of enum value
    /// @return
    ///   Numeric value, if found; otherwise, throws an exception
    TEnumValueType FindValue(const CTempString& name) const;
    
    /// Check whether enum with this name is defined
    ///
    /// @param name 
    ///   Name of enum value
    /// @return
    ///   TRUE, if it is defined
    bool IsValidName(const CTempString& name) const;

    /// Find name of the enum by its numeric value
    ///
    /// @param value
    ///   Numeric value
    /// @param allowBadValue
    ///   When TRUE, and the name is not found, return empty string;
    ///   otherwise, throw an exception
    /// @return
    ///   Name of the enum
    const string& FindName(TEnumValueType value, bool allowBadValue) const;

    string GetDisplayName(TEnumValueType value) const;

    /// Get name-to-value map
    const TNameToValue& NameToValue(void) const;
    /// Get value-to-name map
    const TValueToName& ValueToName(void) const;

    void SetBitset(bool bitset=true) {
        m_IsBitset = bitset;
    }
    bool IsBitset(void) const {
        return m_IsBitset;
    }

    TValueFlags GetValueFlags(TEnumValueType) const;
    const string& FindNameEx(TEnumValueType value, bool allowBadValue) const;

private:
    string m_Name;
    string m_ModuleName;

    bool m_Integer;
    bool m_IsBitset;
    bool m_IsInternal;
    TValues m_Values;
    map<TEnumValueType, TValueFlags> m_ValueFlags;
    mutable auto_ptr<TNameToValue> m_NameToValue;
    mutable auto_ptr<TValueToName> m_ValueToName;
};


/* @} */


END_NCBI_SCOPE

#endif  /* ENUMVALUES__HPP */
