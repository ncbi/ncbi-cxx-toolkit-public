
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

#include <ncbi_pch.hpp>
#include <serial/datatool/dtdaux.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// DTDEntity

DTDEntity::DTDEntity(void)
{
    m_External = false;
}
DTDEntity::DTDEntity(const DTDEntity& other)
{
    m_Name = other.m_Name;
    m_Data = other.m_Data;
    m_External = other.m_External;
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

/////////////////////////////////////////////////////////////////////////////
// DTDAttribute

DTDAttribute::DTDAttribute(void)
{
    m_Type = eUnknown;
    m_ValueType = eDefault;
}
DTDAttribute::DTDAttribute(const DTDAttribute& other)
{
    m_Name = other.m_Name;
    m_Type = other.m_Type;
    m_ValueType = other.m_ValueType;
    m_Value = other.m_Value;
    for (list<string>::const_iterator i = other.m_ListEnum.begin();
        i != other.m_ListEnum.end(); ++i) {
        m_ListEnum.push_back( *i);
    }
}
DTDAttribute::~DTDAttribute(void)
{
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
DTDAttribute::EType DTDAttribute::GetType(void) const
{
    return m_Type;
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

void DTDAttribute::AddEnumValue(const string& value)
{
    m_ListEnum.push_back(value);
}
const list<string>& DTDAttribute::GetEnumValues(void) const
{
    return m_ListEnum;
}

/////////////////////////////////////////////////////////////////////////////
// DTDElement

DTDElement::DTDElement(void)
{
    m_Type = eUnknown;
    m_Occ  = eOne;
    m_Refd = false;
    m_Embd = false;
}

DTDElement::DTDElement(const DTDElement& other)
{
    m_Name = other.m_Name;
    m_Type = other.eUnknown;
    m_Occ  = other.m_Occ;
    m_Refd = other.m_Refd;
    m_Embd = other.m_Embd;
    for (list<string>::const_iterator i = other.m_Refs.begin();
        i != other.m_Refs.end(); ++i) {
        m_Refs.push_back( *i);
    }
    for (map<string,EOccurrence>::const_iterator i = other.m_RefOcc.begin();
        i != other.m_RefOcc.end(); ++i) {
        m_RefOcc[i->first] = i->second;
    }
     ;
    for (list<DTDAttribute>::const_iterator i = other.m_Attrib.begin();
        i != other.m_Attrib.end(); ++i) {
        m_Attrib.push_back(*i);
    }
}

DTDElement::~DTDElement(void)
{
}


void DTDElement::SetName(const string& name)
{
    m_Name = name;
}
const string& DTDElement::GetName(void) const
{
    return m_Name;
}


void DTDElement::SetType( EType type)
{
    _ASSERT(m_Type == eUnknown || m_Type == type);
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


void DTDElement::SetEmbedded(void)
{
    m_Embd = true;
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
        tmp[0] = toupper(tmp[0]);
        name += tmp;
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


/////////////////////////////////////////////////////////////////////////////
// DTDEntityLexer

DTDEntityLexer::DTDEntityLexer(CNcbiIstream& in, bool autoDelete)
    : DTDLexer(in)
{
    m_Str = &in;
    m_AutoDelete = autoDelete;
}
DTDEntityLexer::~DTDEntityLexer(void)
{
    if (m_AutoDelete) {
        delete m_Str;
    }
}


END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.2  2004/05/17 21:03:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.1  2002/11/14 21:02:15  gouriano
 * auxiliary classes to use by DTD parser
 *
 *
 *
 * ==========================================================================
 */
