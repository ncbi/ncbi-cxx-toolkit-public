#ifndef ENUMTYPE_HPP
#define ENUMTYPE_HPP

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
*   Enumerated type definition
*
*/

#include "type.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class CEnumDataTypeValue
{
public:
    CEnumDataTypeValue(const string& name, TEnumValueType value)
        : m_SourceLine(0), m_Name(name), m_Value(value), m_Flags(eNone), m_IdName(name)
        {
        }
    
    const string& GetName(void) const
        {
            return m_Name;
        }
    TEnumValueType GetValue(void) const
        {
            return m_Value;
        }

    CComments& GetComments(void)
        {
            return m_Comments;
        }
    const CComments& GetComments(void) const
        {
            return m_Comments;
        }
    void SetSourceLine(int line)
        {
            m_SourceLine = line;
        }
    int GetSourceLine(void) const
        {
            return m_SourceLine;
        }

    const string& GetEnumId(void) const {
        return m_IdName;
    }
    void SetEnumId(const string& id) const {
        m_IdName = id;
    }

    enum EValueFlags {
        eNone        = 0,
        eHideName = 1
    };
    typedef unsigned int TValueFlags;

    void SetFlag(EValueFlags flag) const {
        m_Flags |= flag;
    }
    TValueFlags GetFlags(void) const {
        return m_Flags;
    }
private:
    int m_SourceLine;
    string m_Name;
    TEnumValueType m_Value;
    CComments m_Comments;
    mutable TValueFlags m_Flags;
    mutable string m_IdName;
};

class CEnumDataType : public CDataType
{
    typedef CDataType CParent;
public:
    typedef CEnumDataTypeValue TValue;
    typedef list<TValue> TValues;

    CEnumDataType(void);
    virtual bool IsInteger(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual string GetXMLContents(void) const;

    TValue& AddValue(const string& name, TEnumValueType value);
    const TValues& GetValues(void) const
        {
            return m_Values;
        }

    void PrintASN(CNcbiOstream& out, int indent) const;
    virtual void PrintSpecDumpExtra(CNcbiOstream& out, int indent) const;
    void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;
    void PrintDTDExtra(CNcbiOstream& out) const;

    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;
    virtual string GetDefaultString(const CDataValue& value) const;
    virtual string GetXmlValueName(const string& value) const;

    struct SEnumCInfo {
        string enumName;
        string cType;
        string valuePrefix;
        
        SEnumCInfo(const string& name, const string& type,
                   const string& prefix)
            : enumName(name), cType(type), valuePrefix(prefix)
            {
            }
    };
    
    string DefaultEnumName(void) const;
    SEnumCInfo GetEnumCInfo(void) const;

    CComments& LastComments(void)
        {
            return m_LastComments;
        }

public:

    CTypeInfo* CreateTypeInfo(void);
    AutoPtr<CTypeStrings> GetRefCType(void) const;
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    AutoPtr<CTypeStrings> GenerateCode(void) const;

private:
    TValues m_Values;
    CComments m_LastComments;
};

class CIntEnumDataType : public CEnumDataType {
    typedef CEnumDataType CParent;
public:
    virtual bool IsInteger(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
    virtual string GetXmlValueName(const string& value) const;
};

class CBigIntEnumDataType : public CIntEnumDataType {
    typedef CIntEnumDataType CParent;
public:
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
};

END_NCBI_SCOPE

#endif
