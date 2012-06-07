#ifndef DTDAUX_HPP
#define DTDAUX_HPP

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
* Author: Andrei Gourianov
*
* File Description:
*   DTD parser's auxiliary stuff:
*       DTDEntity
*       DTDAttribute
*       DTDElement
*       DTDEntityLexer
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include "comments.hpp"
#include <list>
#include <map>

BEGIN_NCBI_SCOPE

class CFileModules;
class CDataModule;
class CDataType;
class CDataMemberContainerType;
class CDataValue;
class CDataMember;
class CEnumDataType;
class CEnumDataTypeValue;


/////////////////////////////////////////////////////////////////////////////
// DTDEntity

class DTDEntity
{
public:
    DTDEntity(void);
    DTDEntity(const DTDEntity& other);
    virtual ~DTDEntity(void);

    enum EType {
        eEntity,
        eType,
        eGroup,
        eAttGroup,
        
        eWsdlInterface,
        eWsdlBinding
    };

    void SetName(const string& name);
    const string& GetName(void) const;

    void SetData(const string& data);
    const string& GetData(void) const;

    void SetExternal(void);
    bool IsExternal(void) const;

    void SetType(EType type);
    EType GetType(void) const;

    void SetParseAttributes( const string& namespaceName,
        bool elementForm, bool attributeForm,
        map<string,string>& prefixToNamespace);
    void GetParseAttributes( string& namespaceName,
        bool& elementForm, bool& attributeForm,
        map<string,string>& prefixToNamespace) const;
private:
    string m_Name;
    string m_Data;
    bool   m_External;
    EType  m_Type;
    string m_TargetNamespace;
    bool m_ElementFormDefault;
    bool m_AttributeFormDefault;
    map<string,string> m_PrefixToNamespace;
};

/////////////////////////////////////////////////////////////////////////////
// DTDAttribute

class DTDAttribute
{
public:
    DTDAttribute(void);
    DTDAttribute(const DTDAttribute& other);
    virtual ~DTDAttribute(void);

    enum EType {
        eUnknown,
        eUnknownGroup,

        eString,
        eEnum,
        eIntEnum,
        eId,
        eIdRef,
        eIdRefs,
        eNmtoken,
        eNmtokens,
        eEntity,
        eEntities,
        eNotation,

        eBoolean,
        eInteger,
        eBigInt,
        eDouble,
        eBase64Binary
    };
    enum EValueType {
        eDefault,
        eRequired,
        eImplied,
        eFixed,
        eProhibited
    };
    
    DTDAttribute& operator= (const DTDAttribute& other);
    void Merge(const DTDAttribute& other);

    void SetSourceLine(int line);
    int GetSourceLine(void) const;

    void SetName(const string& name);
    const string& GetName(void) const;

    void SetType(EType type);
    void SetTypeIfUnknown( EType type);
    EType GetType(void) const;
    void SetTypeName( const string& name);
    const string& GetTypeName( void) const;

    void SetValueType(EValueType valueType);
    EValueType GetValueType(void) const;

    void SetValue(const string& value);
    const string& GetValue(void) const;

    void AddEnumValue(const string& value, int line, int id=0);
    const list<string>& GetEnumValues(void) const;
    int GetEnumValueSourceLine(const string& value) const;
    int GetEnumValueId(const string& value) const;
    
    void SetNamespaceName(const string& name);
    const string& GetNamespaceName(void) const;
    void SetQualified(bool qualified)
    {
        m_Qualified = qualified;
    }
    bool IsQualified(void) const
    {
        return m_Qualified;
    }

    CComments& Comments(void)
    {
        return m_Comments;
    }
    const CComments& GetComments(void) const
    {
        return m_Comments;
    }
private:
    int m_SourceLine;
    string m_Name;
    string m_TypeName;
    string m_NamespaceName;
    EType m_Type;
    EValueType m_ValueType;
    string m_Value;
    list<string> m_ListEnum;
    map<string,int> m_ValueSourceLine;
    map<string,int> m_ValueId;
    bool m_Qualified;
    CComments m_Comments;
};

/////////////////////////////////////////////////////////////////////////////
// DTDElement

class DTDElement
{
public:
    DTDElement(void);
    DTDElement(const DTDElement& other);
    virtual ~DTDElement(void);

    enum EType {
        eUnknown,
        eUnknownGroup,

        eString,   // #PCDATA
        eAny,      // ANY
        eEmpty,    // EMPTY
        eSequence, // (a,b,c)
        eChoice,   // (a|b|c)
        eSet,      // (a,b,c)

        eBoolean,
        eInteger,
        eBigInt,
        eDouble,
        eOctetString,
        eBase64Binary,

        eWsdlService,
        eWsdlEndpoint,
        eWsdlUnsupportedEndpoint,
        eWsdlOperation,
        eWsdlHeaderInput,
        eWsdlInput,
        eWsdlHeaderOutput,
        eWsdlOutput,
        eWsdlMessage
    };
    enum EOccurrence {
        eZero,
        eOne,
        eOneOrMore,
        eZeroOrMore,
        eZeroOrOne
    };

    void SetSourceLine(int line);
    int GetSourceLine(void) const;

    void SetName(const string& name);
    const string& GetName(void) const;
    void SetNamed(bool named=true);
    bool IsNamed(void) const;

    void SetType( EType type);
    void ResetType( EType type);
    void SetTypeIfUnknown( EType type);
    EType GetType(void) const;
    void SetTypeName( const string& name);
    const string& GetTypeName( void) const;

    void SetOccurrence( const string& ref_name, EOccurrence occ);
    EOccurrence GetOccurrence(const string& ref_name) const;

    void SetOccurrence( EOccurrence occ);
    EOccurrence GetOccurrence(void) const;

    // i.e. element contains other elements
    void AddContent( const string& ref_name);
    void RemoveContent( const string& ref_name);
    void RemoveContent( void);
    const list<string>& GetContent(void) const;

    // element is contained somewhere
    void SetReferenced(void);
    bool IsReferenced(void) const;

    // element does not have any specific name
    void SetEmbedded(bool set=true);
    bool IsEmbedded(void) const;
    string CreateEmbeddedName(int depth) const;

    void AddAttribute(DTDAttribute& attrib);
    bool HasAttributes(void) const;
    const list<DTDAttribute>& GetAttributes(void) const;
    list<DTDAttribute>& GetNonconstAttributes(void);
    void MergeAttributes(void);
    
    void SetNamespaceName(const string& name);
    const string& GetNamespaceName(void) const;
    void SetQualified(bool qualified)
    {
        m_Qualified = qualified;
    }
    bool IsQualified(void) const
    {
        return m_Qualified;
    }

    void SetDefault(const string& value);
    const string& GetDefault(void) const;

    CComments& Comments(void)
    {
        return m_Comments;
    }
    const CComments& GetComments(void) const
    {
        return m_Comments;
    }
    CComments& AttribComments(void)
    {
        return m_AttribComments;
    }
    const CComments& GetAttribComments(void) const
    {
        return m_AttribComments;
    }
private:
    int m_SourceLine;
    string m_Name;
    string m_TypeName;
    string m_NamespaceName;
    string m_Default;
    EType m_Type;
    EOccurrence m_Occ;
    list<string> m_Refs;
    list<DTDAttribute> m_Attrib;
    map<string,EOccurrence> m_RefOcc;
    bool m_Refd;
    bool m_Embd;
    bool m_Named;
    bool m_Qualified;
    CComments m_Comments;
    CComments m_AttribComments;
};

END_NCBI_SCOPE

#endif // DTDAUX_HPP
