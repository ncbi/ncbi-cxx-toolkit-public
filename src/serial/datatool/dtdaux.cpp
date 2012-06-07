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
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "dtdaux.hpp"

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// DTDEntity

DTDEntity::DTDEntity(void)
{
    m_External = false;
    m_Type = eEntity;
    m_ElementFormDefault = false;
    m_AttributeFormDefault = false;
}
DTDEntity::DTDEntity(const DTDEntity& other)
{
    m_Name = other.m_Name;
    m_Data = other.m_Data;
    m_External = other.m_External;
    m_Type = other.m_Type;
}
DTDEntity::~DTDEntity(void)
{
}

void DTDEntity::SetName(const string& name)
{
    m_Name = name;
}
const string& DTDEntity::GetName(void) const
{
    return m_Name;
}

void DTDEntity::SetData(const string& data)
{
    m_Data = data;
}
const string& DTDEntity::GetData(void) const
{
    return m_Data;
}

void DTDEntity::SetExternal(void)
{
    m_External = true;
}
bool DTDEntity::IsExternal(void) const
{
    return m_External;
}
void DTDEntity::SetType(EType type)
{
    m_Type = type;
}
DTDEntity::EType DTDEntity::GetType(void) const
{
    return m_Type;
}

void DTDEntity::SetParseAttributes( const string& namespaceName,
    bool elementForm, bool attributeForm,
    map<string,string>& prefixToNamespace)
{
    m_TargetNamespace = namespaceName;
    m_ElementFormDefault = elementForm;
    m_AttributeFormDefault = attributeForm;
    m_PrefixToNamespace = prefixToNamespace;
}

void DTDEntity::GetParseAttributes( string& namespaceName,
    bool& elementForm, bool& attributeForm,
    map<string,string>& prefixToNamespace) const
{
    namespaceName = m_TargetNamespace;
    elementForm = m_ElementFormDefault;
    attributeForm = m_AttributeFormDefault;
    prefixToNamespace = m_PrefixToNamespace;
}

/////////////////////////////////////////////////////////////////////////////
// DTDAttribute

DTDAttribute::DTDAttribute(void)
{
    m_SourceLine = 0;
    m_Type = eUnknown;
    m_ValueType = eImplied;
    m_Qualified = false;
}
DTDAttribute::DTDAttribute(const DTDAttribute& other)
{
    m_SourceLine= other.m_SourceLine;
    m_Name      = other.m_Name;
    m_TypeName  = other.m_TypeName;
    m_NamespaceName = other.m_NamespaceName;
    m_Type      = other.m_Type;
    m_ValueType = other.m_ValueType;
    m_Value     = other.m_Value;
    m_ListEnum  = other.m_ListEnum;
    m_ValueSourceLine = other.m_ValueSourceLine;
    m_ValueId = other.m_ValueId;
    m_Qualified = other.m_Qualified;
    m_Comments  = other.m_Comments;
}
DTDAttribute::~DTDAttribute(void)
{
}

DTDAttribute& DTDAttribute::operator= (const DTDAttribute& other)
{
    m_SourceLine= other.m_SourceLine;
    m_Name      = other.m_Name;
    m_TypeName  = other.m_TypeName;
    m_NamespaceName = other.m_NamespaceName;
    m_Type      = other.m_Type;
    m_ValueType = other.m_ValueType;
    m_Value     = other.m_Value;
    m_ListEnum  = other.m_ListEnum;
    m_ValueSourceLine = other.m_ValueSourceLine;
    m_ValueId = other.m_ValueId;
    m_Qualified = other.m_Qualified;
    m_Comments  = other.m_Comments;
    return *this;
}

void DTDAttribute::Merge(const DTDAttribute& other)
{
    m_Name      = other.m_Name;
    m_TypeName  = other.m_TypeName;
    m_Type      = other.m_Type;
    if (m_ValueType == eDefault) {
        m_ValueType = other.m_ValueType;
    }
    m_Value     = other.m_Value;
    m_ListEnum  = other.m_ListEnum;
    m_ValueSourceLine = other.m_ValueSourceLine;
    m_ValueId = other.m_ValueId;
    m_Qualified = other.m_Qualified;
}

void DTDAttribute::SetSourceLine(int line)
{
    m_SourceLine = line;
}

int DTDAttribute::GetSourceLine(void) const
{
    return m_SourceLine;
}

void DTDAttribute::SetName(const string& name)
{
    m_Name = name;
}
const string& DTDAttribute::GetName(void) const
{
    return m_Name;
}

void DTDAttribute::SetType(EType type)
{
    m_Type = type;
}

void DTDAttribute::SetTypeIfUnknown( EType type)
{
    if (m_Type == eUnknown) {
        m_Type = type;
    }
}

DTDAttribute::EType DTDAttribute::GetType(void) const
{
    return m_Type;
}

void DTDAttribute::SetTypeName( const string& name)
{
    m_TypeName = name;
}

const string& DTDAttribute::GetTypeName( void) const
{
    return m_TypeName;
}

void DTDAttribute::SetValueType(EValueType valueType)
{
    m_ValueType = valueType;
}
DTDAttribute::EValueType DTDAttribute::GetValueType(void) const
{
    return m_ValueType;
}

void DTDAttribute::SetValue(const string& value)
{
    m_Value = value;
}
const string& DTDAttribute::GetValue(void) const
{
    return m_Value;
}

void DTDAttribute::AddEnumValue(const string& value, int line, int id)
{
    m_ListEnum.push_back(value);
    m_ValueSourceLine[value] = line;
    m_ValueId[value] = id;
}
const list<string>& DTDAttribute::GetEnumValues(void) const
{
    return m_ListEnum;
}

int DTDAttribute::GetEnumValueSourceLine(const string& value) const
{
    if (m_ValueSourceLine.find(value) != m_ValueSourceLine.end()) {
        return m_ValueSourceLine.find(value)->second;
    }
    return 0;
}

int DTDAttribute::GetEnumValueId(const string& value) const
{
    if (m_ValueId.find(value) != m_ValueId.end()) {
        return m_ValueId.find(value)->second;
    }
    return 0;
}

void DTDAttribute::SetNamespaceName(const string& name)
{
    m_NamespaceName = name;
}
const string& DTDAttribute::GetNamespaceName(void) const
{
    return m_NamespaceName;
}

/////////////////////////////////////////////////////////////////////////////
// DTDElement

DTDElement::DTDElement(void)
{
    m_SourceLine = 0;
    m_Type = eUnknown;
    m_Occ  = eOne;
    m_Refd = false;
    m_Embd = false;
    m_Named= false;
    m_Qualified = false;
}

DTDElement::DTDElement(const DTDElement& other)
{
    m_SourceLine = other.m_SourceLine;
    m_Name     = other.m_Name;
    m_TypeName = other.m_TypeName;
    m_NamespaceName = other.m_NamespaceName;
    m_Default  = other.m_Default;
    m_Type     = other.eUnknown;
    m_Occ      = other.m_Occ;
    m_Refd     = other.m_Refd;
    m_Embd     = other.m_Embd;
    m_Refs     = other.m_Refs;
    m_RefOcc   = other.m_RefOcc;
    m_Attrib   = other.m_Attrib;
    m_Named    = other.m_Named;
    m_Qualified= other.m_Qualified;
    m_Comments = other.m_Comments;
    m_AttribComments = other.m_AttribComments;
}

DTDElement::~DTDElement(void)
{
}

void DTDElement::SetSourceLine(int line)
{
    m_SourceLine = line;
}
int DTDElement::GetSourceLine(void) const
{
    return m_SourceLine;
}

void DTDElement::SetName(const string& name)
{
    m_Name = name;
}
const string& DTDElement::GetName(void) const
{
    return m_Name;
}
void DTDElement::SetNamed(bool named)
{
    m_Named = named;
}
bool DTDElement::IsNamed(void) const
{
    return m_Named;
}

void DTDElement::SetType( EType type)
{
    _ASSERT(m_Type == eUnknown ||
            m_Type == eUnknownGroup ||
            m_Type == eWsdlEndpoint ||
            m_Type == type ||
            m_Type == eEmpty);
    m_Type = type;
}

void DTDElement::ResetType( EType type)
{
    _ASSERT(type == eUnknown || type == eUnknownGroup);
    _ASSERT(m_Refs.size() == 0);
    m_Type = type;
}

void DTDElement::SetTypeIfUnknown( EType type)
{
    if (m_Type == eUnknown) {
        m_Type = type;
    }
}

DTDElement::EType DTDElement::GetType(void) const
{
    return (EType)m_Type;
}

void DTDElement::SetTypeName( const string& name)
{
    m_TypeName = name;
}
const string& DTDElement::GetTypeName( void) const
{
    return m_TypeName;
}


void DTDElement::SetOccurrence( const string& ref_name, EOccurrence occ)
{
    m_RefOcc[ref_name] = occ;
}
DTDElement::EOccurrence DTDElement::GetOccurrence(
    const string& ref_name) const
{
    map<string,EOccurrence>::const_iterator i = m_RefOcc.find(ref_name);
    return (i != m_RefOcc.end()) ? i->second : eOne;
}


void DTDElement::SetOccurrence( EOccurrence occ)
{
    m_Occ = occ;
}
DTDElement::EOccurrence DTDElement::GetOccurrence(void) const
{
    return m_Occ;
}


void DTDElement::AddContent( const string& ref_name)
{
    m_Refs.push_back( ref_name);
}

void DTDElement::RemoveContent( const string& ref_name)
{
    string t(ref_name);
    m_Refs.remove(t);
}

void DTDElement::RemoveContent( void)
{
    m_Refs.clear();
}

const list<string>& DTDElement::GetContent(void) const
{
    return m_Refs;
}


void DTDElement::SetReferenced(void)
{
    m_Refd = true;
}
bool DTDElement::IsReferenced(void) const
{
    return m_Refd;
}


void DTDElement::SetEmbedded(bool set)
{
    m_Embd = set;
}
bool DTDElement::IsEmbedded(void) const
{
    return m_Embd;
}
string DTDElement::CreateEmbeddedName(int depth) const
{
    string name, tmp;
    list<string>::const_iterator i;
    for ( i = m_Refs.begin(); i != m_Refs.end(); ++i) {
        tmp = i->substr(0,depth);
        tmp[0] = toupper((unsigned char) tmp[0]);
        name += tmp;
    }
    if (m_Type == eAny) {
        name = "AnyContent";
    }
    return name;
}

void DTDElement::AddAttribute(DTDAttribute& attrib)
{
    m_Attrib.push_back(attrib);
}
bool DTDElement::HasAttributes(void) const
{
    return !m_Attrib.empty();
}
const list<DTDAttribute>& DTDElement::GetAttributes(void) const
{
    return m_Attrib;
}
list<DTDAttribute>& DTDElement::GetNonconstAttributes(void)
{
    return m_Attrib;
}

void DTDElement::MergeAttributes(void)
{
    list<DTDAttribute>::iterator i, redef;
    for (i = m_Attrib.begin(); i != m_Attrib.end();) {
        bool found = false;
        redef = i;
        ++redef;
        while ( !found && redef != m_Attrib.end() ) {
            if (i->GetName() == redef->GetName()) {
/*
                if (i->GetType() != redef->GetType()) {
                }
*/
                if (i->GetValueType() != redef->GetValueType()) {
                    i->SetValueType( redef->GetValueType() );
                }
                DTDAttribute::EValueType t = redef->GetValueType();
                redef = m_Attrib.erase(redef);
                if (t == DTDAttribute::eProhibited) {
                    i = m_Attrib.erase(i);
                    found = true;
                }
            } else {
                ++redef;
            }
        }
        if (!found) {
            ++i;
        }
    }
}

void DTDElement::SetNamespaceName(const string& name)
{
    m_NamespaceName = name;
}

const string& DTDElement::GetNamespaceName(void) const
{
    return m_NamespaceName;
}

void DTDElement::SetDefault(const string& value)
{
    m_Default = value;
}

const string& DTDElement::GetDefault(void) const
{
    return m_Default;
}


END_NCBI_SCOPE
