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
*   XML Schema parser
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "exceptions.hpp"
#include "xsdparser.hpp"
#include "tokens.hpp"
#include "module.hpp"
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_Parsers

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// DTDParser

XSDParser::XSDParser(XSDLexer& lexer)
    : DTDParser(lexer)
{
    m_SrcType = eSchema;
    m_ResolveTypes = false;
}

XSDParser::~XSDParser(void)
{
}

void XSDParser::BuildDocumentTree(CDataTypeModule& module)
{
    Reset();
    ParseHeader();
    CopyComments(module.Comments());

    TToken tok;
    int emb=0;
    for (;;) {
        tok = GetNextToken();
        switch ( tok ) {
        case K_INCLUDE:
            ParseInclude();
            break;
        case K_ELEMENT:
            ParseElementContent(0, emb);
            break;
        case K_ATTRIBUTE:
            ParseAttributeContent();
            break;
        case K_COMPLEXTYPE:
        case K_SIMPLETYPE:
            CreateTypeDefinition(DTDEntity::eType);
            break;
        case K_GROUP:
            CreateTypeDefinition(DTDEntity::eGroup);
            break;
        case K_ATTPAIR:
            break;
        case K_ENDOFTAG:
            break;
        case T_EOF:
            ProcessNamedTypes();
            return;
        case K_IMPORT:
            ParseImport();
            break;
        case K_ATTRIBUTEGROUP:
            CreateTypeDefinition(DTDEntity::eAttGroup);
            break;
        case K_ANNOTATION:
            m_Comments = &(module.Comments());
            ParseAnnotation();
            break;
        default:
            ParseError("Invalid keyword", "keyword");
            return;
        }
    }
}

void XSDParser::Reset(void)
{
    m_PrefixToNamespace.clear();
    m_NamespaceToPrefix.clear();
    m_TargetNamespace.erase();
    m_ElementFormDefault = false;
}

TToken XSDParser::GetNextToken(void)
{
    TToken tok = DTDParser::GetNextToken();
    if (tok == T_EOF) {
        return tok;
    }
    string data = NextToken().GetText();
    string str1, str2, data2;

    m_Raw = data;
    m_Element.erase();
    m_ElementPrefix.erase();
    m_Attribute.erase();
    m_AttributePrefix.erase();
    m_Value.erase();
    m_ValuePrefix.erase();
    if (tok == K_ATTPAIR || tok == K_XMLNS) {
// format is
// ns:attr="ns:value"
        if (!NStr::SplitInTwo(data, "=", str1, data2)) {
            ParseError("Unexpected data", "attribute (name=\"value\")");
        }
// attribute
        data = str1;
        if (NStr::SplitInTwo(data, ":", str1, str2)) {
            m_Attribute = str2;
            m_AttributePrefix = str1;
        } else if (tok == K_XMLNS) {
            m_AttributePrefix = str1;
        } else {
            m_Attribute = str1;
        }
// value
        string::size_type first = 0, last = data2.length()-1;
        if (data2.length() < 2 ||
            (data2[first] != '\"' && data2[first] != '\'') ||
            (data2[last]  != '\"' && data2[last]  != '\'') ) {
            ParseError("Unexpected data", "attribute (name=\"value\")");
        }
        data = data2.substr(first+1, last - first - 1);
        if (tok == K_XMLNS) {
            if (m_PrefixToNamespace.find(m_Attribute) != m_PrefixToNamespace.end()) {
                if (!m_PrefixToNamespace[m_Attribute].empty() &&
                    m_PrefixToNamespace[m_Attribute] != data) {
                    ParseError("Unexpected xmlns data", "");
                }
            }
            m_PrefixToNamespace[m_Attribute] = data;
            m_NamespaceToPrefix[data] = m_Attribute;
            m_Value = data;
        } else {
            if (NStr::SplitInTwo(data, ":", str1, str2)) {
                if (m_PrefixToNamespace.find(str1) == m_PrefixToNamespace.end()) {
                    m_Value = data;
                } else {
                    m_Value = str2;
                    m_ValuePrefix = str1;
                }
            } else {
                m_Value = str1;
            }
        }
    } else if (tok != K_ENDOFTAG && tok != K_CLOSING) {
// format is
// ns:element
        if (NStr::SplitInTwo(data, ":", str1, str2)) {
            m_Element = str2;
            m_ElementPrefix = str1;
        } else {
            m_Element = str1;
        }
        if (m_PrefixToNamespace.find(m_ElementPrefix) == m_PrefixToNamespace.end()) {
            m_PrefixToNamespace[m_ElementPrefix] = "";
        }
    }
    ConsumeToken();
    return tok;
}

bool XSDParser::IsAttribute(const char* att) const
{
    return NStr::strcmp(m_Attribute.c_str(),att) == 0;
}

bool XSDParser::IsValue(const char* value) const
{
    return NStr::strcmp(m_Value.c_str(),value) == 0;
}

bool XSDParser::DefineElementType(DTDElement& node)
{
    if (IsValue("string") || IsValue("token") ||
        IsValue("normalizedString") ||
        IsValue("anyURI") || IsValue("QName") ||
        IsValue("dateTime") || IsValue("time") || IsValue("date")) {
        node.SetType(DTDElement::eString);
    } else if (IsValue("double") || IsValue("float") || IsValue("decimal")) {
        node.SetType(DTDElement::eDouble);
    } else if (IsValue("boolean")) {
        node.SetType(DTDElement::eBoolean);
    } else if (IsValue("integer") || IsValue("int")
            || IsValue("short") || IsValue("byte") 
            || IsValue("negativeInteger") || IsValue("nonNegativeInteger")
            || IsValue("positiveInteger") || IsValue("nonPositiveInteger")
            || IsValue("unsignedInt") || IsValue("unsignedShort")
            || IsValue("unsignedByte") ) {
        node.SetType(DTDElement::eInteger);
    } else if (IsValue("long") || IsValue("unsignedLong")) {
        node.SetType(DTDElement::eBigInt);
    } else if (IsValue("hexBinary")) {
        node.SetType(DTDElement::eOctetString);
    } else if (IsValue("base64Binary")) {
        node.SetType(DTDElement::eBase64Binary);
    } else {
        return false;
    }
    return true;
}

bool XSDParser::DefineAttributeType(DTDAttribute& attrib)
{
    if (IsValue("string") || IsValue("token") || IsValue("QName") ||
        IsValue("anyURI") ||
        IsValue("dateTime") || IsValue("time") || IsValue("date")) {
        attrib.SetType(DTDAttribute::eString);
    } else if (IsValue("ID")) {
        attrib.SetType(DTDAttribute::eId);
    } else if (IsValue("IDREF")) {
        attrib.SetType(DTDAttribute::eIdRef);
    } else if (IsValue("IDREFS")) {
        attrib.SetType(DTDAttribute::eIdRefs);
    } else if (IsValue("NMTOKEN")) {
        attrib.SetType(DTDAttribute::eNmtoken);
    } else if (IsValue("NMTOKENS")) {
        attrib.SetType(DTDAttribute::eNmtokens);
    } else if (IsValue("ENTITY")) {
        attrib.SetType(DTDAttribute::eEntity);
    } else if (IsValue("ENTITIES")) {
        attrib.SetType(DTDAttribute::eEntities);

    } else if (IsValue("boolean")) {
        attrib.SetType(DTDAttribute::eBoolean);
    } else if (IsValue("int") || IsValue("integer")
            || IsValue("short") || IsValue("byte") 
            || IsValue("negativeInteger") || IsValue("nonNegativeInteger")
            || IsValue("positiveInteger") || IsValue("nonPositiveInteger")
            || IsValue("unsignedInt") || IsValue("unsignedShort")
            || IsValue("unsignedByte") ) {
        attrib.SetType(DTDAttribute::eInteger);
    } else if (IsValue("long") || IsValue("unsignedLong")) {
        attrib.SetType(DTDAttribute::eBigInt);
    } else if (IsValue("float") || IsValue("double") || IsValue("decimal")) {
        attrib.SetType(DTDAttribute::eDouble);
    } else if (IsValue("base64Binary")) {
        attrib.SetType(DTDAttribute::eBase64Binary);
    } else {
        return false;
    }
    return true;
}

void XSDParser::ParseHeader()
{
// xml header
    TToken tok = GetNextToken();
    if (tok == K_XML) {
        for ( ; tok != K_ENDOFTAG; tok=GetNextToken())
            ;
        tok = GetNextToken();
    } else {
        ERR_POST_X(4, "LINE " << Location() << " XML declaration is missing");
    }
// schema    
    if (tok != K_SCHEMA) {
        ParseError("Unexpected token", "schema");
    }
    for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken()) {
        if (tok == K_ATTPAIR) {
            if (IsAttribute("targetNamespace")) {
                m_TargetNamespace = m_Value;
            } else if (IsAttribute("elementFormDefault")) {
                m_ElementFormDefault = IsValue("qualified");
            }
        }
    }
    if (tok != K_CLOSING) {
        ParseError("tag closing");
    }
}

void XSDParser::ParseInclude(void)
{
    TToken tok;
    string name;
    for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken()) {
        if (IsAttribute("schemaLocation")) {
            name = m_Value;
        }
    }
    if (tok != K_ENDOFTAG) {
        ParseError("endoftag");
    }
    if (name.empty()) {
        ParseError("schemaLocation");
    }
    DTDEntity& node = m_MapEntity[CreateEntityId(name, DTDEntity::eEntity)];
    if (node.GetName().empty()) {
        node.SetName(name);
        node.SetData(name);
        node.SetExternal();
        PushEntityLexer(name);
        Reset();
        ParseHeader();
    }
}

void XSDParser::ParseImport(void)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("namespace")) {
        if (IsValue("http://www.w3.org/XML/1998/namespace")) {
            string name = "xml:lang";
            m_MapAttribute[name].SetName(name);
            m_MapAttribute[name].SetType(DTDAttribute::eString);
        }
    }
    if (tok == K_CLOSING) {
        SkipContent();
    }
}

void XSDParser::ParseAnnotation(void)
{
    TToken tok;
    if (GetRawAttributeSet() == K_CLOSING) {
        for ( tok = GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
            if (tok == K_DOCUMENTATION) {
                ParseDocumentation();
            } else if (tok == K_APPINFO) {
                ParseAppInfo();
            } else {
                ParseError("documentation or appinfo");
            }
        }
    }
    m_ExpectLastComment = true;
}

void XSDParser::ParseDocumentation(void)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_ENDOFTAG) {
        return;
    }
    if (tok == K_CLOSING) {
        XSDLexer& l = dynamic_cast<XSDLexer&>(Lexer());
        while (l.ProcessDocumentation())
            ;
    }
    tok = GetNextToken();
    if (tok != K_ENDOFTAG) {
        ParseError("endoftag");
    }
    m_ExpectLastComment = true;
}

void XSDParser::ParseAppInfo(void)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        SkipContent();
    }
}

TToken XSDParser::GetRawAttributeSet(void)
{
    m_RawAttributes.clear();
    TToken tok;
    for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS;
          tok = GetNextToken()) {
        if (tok == K_ATTPAIR ) {
            m_RawAttributes[m_Attribute] = m_Value;
        }
    }
    return tok;
}

bool XSDParser::GetAttribute(const string& att)
{
    if (m_RawAttributes.find(att) != m_RawAttributes.end()) {
        m_Attribute = att;
        m_Value = m_RawAttributes[att];
        return true;
    }
    m_Attribute.erase();
    m_Value.erase();
    return false;
}

void XSDParser::SkipContent()
{
    TToken tok;
    bool eatEOT= false;
    XSDLexer& l = dynamic_cast<XSDLexer&>(Lexer());
    for ( tok = l.Skip(); ; tok = l.Skip()) {
        if (tok == T_EOF) {
            return;
        }
        ConsumeToken();
        switch (tok) {
        case K_ENDOFTAG:
            if (!eatEOT) {
                return;
            }
            break;
        case K_CLOSING:
            SkipContent();
            break;
        default:
        case K_ELEMENT:
            break;
        }
        eatEOT = tok == K_ELEMENT;
    }
}

DTDElement::EOccurrence XSDParser::ParseMinOccurs( DTDElement::EOccurrence occNow)
{
    DTDElement::EOccurrence occNew = occNow;
    if (GetAttribute("minOccurs")) {
        int m = NStr::StringToInt(m_Value);
        if (m == 0) {
            if (occNow == DTDElement::eOne) {
                occNew = DTDElement::eZeroOrOne;
            } else if (occNow == DTDElement::eOneOrMore) {
                occNew = DTDElement::eZeroOrMore;
            }
        } else if (m > 1) {
            ERR_POST_X(8, Warning << "Unsupported element minOccurs= " << m);
            occNew = DTDElement::eOneOrMore;
        }
    }
    return occNew;
}

DTDElement::EOccurrence XSDParser::ParseMaxOccurs( DTDElement::EOccurrence occNow)
{
    DTDElement::EOccurrence occNew = occNow;
    if (GetAttribute("maxOccurs")) {
        int m = IsValue("unbounded") ? -1 : NStr::StringToInt(m_Value);
        if (m == -1 || m > 1) {
            if (m > 1) {
                ERR_POST_X(8, Warning << "Unsupported element maxOccurs= " << m);
            }
            if (occNow == DTDElement::eOne) {
                occNew = DTDElement::eOneOrMore;
            } else if (occNow == DTDElement::eZeroOrOne) {
                occNew = DTDElement::eZeroOrMore;
            }
        } else if (m == 0) {
            occNew = DTDElement::eZero;
        }
    }
    return occNew;
}

string XSDParser::ParseElementContent(DTDElement* owner, int emb)
{
    TToken tok;
    string name, value;
    bool ref=false, named_type=false;
    int line = Lexer().CurrentLine();

    tok = GetRawAttributeSet();

    if (GetAttribute("ref")) {
        name = m_Value;
        ref=true;
    }
    if (GetAttribute("name")) {
        ref=false;
        name = m_Value;
        if (owner) {
            name = CreateTmpEmbeddedName(owner->GetName(), emb);
            m_MapElement[name].SetEmbedded();
            m_MapElement[name].SetNamed();
        }
        m_MapElement[name].SetName(m_Value);
        m_MapElement[name].SetSourceLine(line);
        SetCommentsIfEmpty(&(m_MapElement[name].Comments()));
    }
    if (GetAttribute("type")) {
        if (!DefineElementType(m_MapElement[name])) {
            m_MapElement[name].SetTypeName(m_Value);
            named_type = true;
        }
    }
    if (GetAttribute("default")) {
        m_MapElement[name].SetDefault(m_Value);
    }
    if (owner && !name.empty()) {
        owner->SetOccurrence(name, ParseMinOccurs( owner->GetOccurrence(name)));
        owner->SetOccurrence(name, ParseMaxOccurs( owner->GetOccurrence(name)));
    }
    if (tok != K_CLOSING && tok != K_ENDOFTAG) {
        ParseError("endoftag");
    }
    if (tok == K_CLOSING) {
        ParseContent(m_MapElement[name]);
    }
    m_ExpectLastComment = true;
    if (!ref && !named_type) {
        m_MapElement[name].SetTypeIfUnknown(DTDElement::eEmpty);
    }
    m_MapElement[name].SetNamespaceName(m_TargetNamespace);
    return name;
}

string XSDParser::ParseGroup(DTDElement* owner, int emb)
{
    string name;
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("ref")) {

        string id = CreateEntityId(m_Value,DTDEntity::eGroup);
        name = CreateTmpEmbeddedName(owner->GetName(), emb);
        DTDElement& node = m_MapElement[name];
        node.SetEmbedded();
        node.SetName(m_Value);
        node.SetOccurrence( ParseMinOccurs( node.GetOccurrence()));
        node.SetOccurrence( ParseMaxOccurs( node.GetOccurrence()));
        SetCommentsIfEmpty(&(node.Comments()));

        if (m_ResolveTypes) {
            if (m_MapEntity.find(id) != m_MapEntity.end()) {
                PushEntityLexer(id);
                ParseGroupRef(node);
            } else {
                ParseError("Unresolved entity", id.c_str());
            }
        } else {
            node.SetTypeName(node.GetName());
            node.SetType(DTDElement::eUnknownGroup);
            Lexer().FlushCommentsTo(node.Comments());
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(m_MapElement[name]);
    }
    m_ExpectLastComment = true;
    return name;
}

void XSDParser::ParseGroupRef(DTDElement& node)
{
    if (GetNextToken() != K_GROUP) {
        ParseError("group");
    }
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseContent(DTDElement& node, bool extended /*=false*/)
{
    int emb=0;
    bool eatEOT= false;
    TToken tok;
    for ( tok=GetNextToken(); ; tok=GetNextToken()) {
        emb= node.GetContent().size();
        switch (tok) {
        case T_EOF:
            return;
        case K_ENDOFTAG:
            if (eatEOT) {
                eatEOT= false;
                break;
            }
            FixEmbeddedNames(node);
            return;
        case K_COMPLEXTYPE:
            ParseComplexType(node);
            break;
        case K_SIMPLECONTENT:
            ParseSimpleContent(node);
            break;
        case K_EXTENSION:
            ParseExtension(node);
            break;
        case K_RESTRICTION:
            ParseRestriction(node);
            break;
        case K_ATTRIBUTE:
            ParseAttribute(node);
            break;
        case K_ATTRIBUTEGROUP:
            ParseAttributeGroup(node);
            break;
        case K_ANY:
            node.SetTypeIfUnknown(DTDElement::eSequence);
            if (node.GetType() != DTDElement::eSequence) {
                ParseError("sequence");
            } else {
                string name = CreateTmpEmbeddedName(node.GetName(), emb);
                DTDElement& elem = m_MapElement[name];
                elem.SetName(name);
                elem.SetSourceLine(Lexer().CurrentLine());
                elem.SetEmbedded();
                elem.SetType(DTDElement::eAny);
                ParseAny(elem);
                AddElementContent(node,name);
            }
            break;
        case K_SEQUENCE:
            emb= node.GetContent().size();
            if (emb != 0 && extended) {
                node.SetTypeIfUnknown(DTDElement::eSequence);
                if (node.GetType() != DTDElement::eSequence) {
                    ParseError("sequence");
                }
                tok = GetRawAttributeSet();
                eatEOT = true;
                break;
            }
            if (node.GetType() == DTDElement::eUnknown ||
                node.GetType() == DTDElement::eUnknownGroup) {
                node.SetType(DTDElement::eSequence);
                ParseContainer(node);
            } else {
                string name = CreateTmpEmbeddedName(node.GetName(), emb);
                DTDElement& elem = m_MapElement[name];
                elem.SetName(name);
                elem.SetSourceLine(Lexer().CurrentLine());
                elem.SetEmbedded();
                elem.SetType(DTDElement::eSequence);
                ParseContainer(elem);
                AddElementContent(node,name);
            }
            break;
        case K_CHOICE:
            if (node.GetType() == DTDElement::eUnknown ||
                node.GetType() == DTDElement::eUnknownGroup) {
                node.SetType(DTDElement::eChoice);
                ParseContainer(node);
            } else {
                string name = CreateTmpEmbeddedName(node.GetName(), emb);
                DTDElement& elem = m_MapElement[name];
                elem.SetName(name);
                elem.SetSourceLine(Lexer().CurrentLine());
                elem.SetEmbedded();
                elem.SetType(DTDElement::eChoice);
                ParseContainer(elem);
                AddElementContent(node,name);
            }
            break;
        case K_SET:
            if (node.GetType() == DTDElement::eUnknown ||
                node.GetType() == DTDElement::eUnknownGroup) {
                node.SetType(DTDElement::eSet);
                ParseContainer(node);
            } else {
                string name = CreateTmpEmbeddedName(node.GetName(), emb);
                DTDElement& elem = m_MapElement[name];
                elem.SetName(name);
                elem.SetSourceLine(Lexer().CurrentLine());
                elem.SetEmbedded();
                elem.SetType(DTDElement::eSet);
                ParseContainer(elem);
                AddElementContent(node,name);
            }
            break;
        case K_ELEMENT:
            {
	            string name = ParseElementContent(&node,emb);
	            AddElementContent(node,name);
            }
            break;
        case K_GROUP:
            {
	            string name = ParseGroup(&node,emb);
	            AddElementContent(node,name);
            }
            break;
        case K_ANNOTATION:
            SetCommentsIfEmpty(&(node.Comments()));
            ParseAnnotation();
            break;
        default:
            for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken())
                ;
            if (tok == K_CLOSING) {
                ParseContent(node);
            }
            break;
        }
    }
    FixEmbeddedNames(node);
}

void XSDParser::ParseContainer(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    m_ExpectLastComment = true;
    node.SetOccurrence( ParseMinOccurs( node.GetOccurrence()));
    node.SetOccurrence( ParseMaxOccurs( node.GetOccurrence()));
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseComplexType(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("mixed")) {
        if (IsValue("true")) {
            string name(s_SpecialName);
	        AddElementContent(node,name);
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseSimpleContent(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseExtension(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    bool extended=false;
    if (GetAttribute("base")) {
        if (!DefineElementType(node)) {
            if (m_ResolveTypes) {
                string id = CreateEntityId(m_Value,DTDEntity::eType);
                if (m_MapEntity.find(id) != m_MapEntity.end()) {
                    PushEntityLexer(id);
                    extended=true;
                } else {
                    ParseError("Unresolved entity", id.c_str());
                }
            } else {
                node.SetTypeName(m_Value);
            }
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(node, extended);
    }
}

void XSDParser::ParseRestriction(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    bool extended=false;
    if (GetAttribute("base")) {
        if (!DefineElementType(node)) {
            string id = CreateEntityId(m_Value,DTDEntity::eType);
            if (m_ResolveTypes) {
                if (m_MapEntity.find(id) != m_MapEntity.end()) {
                    PushEntityLexer(id);
                    extended=true;
                } else {
                    ParseError("Unresolved entity", id.c_str());
                }
            } else {
                node.SetTypeName(m_Value);
            }
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(node,extended);
    }
}

void XSDParser::ParseAttribute(DTDElement& node)
{
    DTDAttribute a;
    node.AddAttribute(a);
    DTDAttribute& att = node.GetNonconstAttributes().back();
    att.SetSourceLine(Lexer().CurrentLine());
    SetCommentsIfEmpty(&(att.Comments()));

    TToken tok = GetRawAttributeSet();
    if (GetAttribute("ref")) {
        att.SetName(m_Value);
    }
    if (GetAttribute("name")) {
        att.SetName(m_Value);
    }
    if (GetAttribute("type")) {
        if (!DefineAttributeType(att)) {
            att.SetTypeName(m_Value);
        }
    }
    if (GetAttribute("use")) {
        if (IsValue("required")) {
            att.SetValueType(DTDAttribute::eRequired);
        } else if (IsValue("optional")) {
            att.SetValueType(DTDAttribute::eImplied);
        } else if (IsValue("prohibited")) {
            att.SetValueType(DTDAttribute::eProhibited);
        }
    }
    if (GetAttribute("default")) {
        att.SetValue(m_Value);
    }
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
    m_ExpectLastComment = true;
}

void XSDParser::ParseAttributeGroup(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("ref")) {
        if (m_ResolveTypes) {
            string id = CreateEntityId(m_Value,DTDEntity::eAttGroup);
            if (m_MapEntity.find(id) != m_MapEntity.end()) {
                PushEntityLexer(id);
                ParseAttributeGroupRef(node);
            } else {
                ParseError("Unresolved entity", id.c_str());
            }
        } else {
            DTDAttribute a;
            a.SetType(DTDAttribute::eUnknownGroup);
            a.SetTypeName(m_Value);
            Lexer().FlushCommentsTo(node.AttribComments());
            node.AddAttribute(a);
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseAttributeGroupRef(DTDElement& node)
{
    if (GetNextToken() != K_ATTRIBUTEGROUP) {
        ParseError("attributeGroup");
    }
    if (GetRawAttributeSet() == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseAny(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("processContents")) {
        if (!IsValue("lax") && !IsValue("skip")) {
            ParseError("lax or skip");
        }
    }
    node.SetOccurrence( ParseMinOccurs( node.GetOccurrence()));
    node.SetOccurrence( ParseMaxOccurs( node.GetOccurrence()));
    if (GetAttribute("namespace")) {
        node.SetNamespaceName(m_Value);
    }
    SetCommentsIfEmpty(&(node.Comments()));
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
    m_ExpectLastComment = true;
}

void XSDParser::ParseUnion(DTDElement& node)
{
    ERR_POST_X(9, Warning
        << "Unsupported element type: union; in node "
        << node.GetName());
    node.SetType(DTDElement::eString);
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
    m_ExpectLastComment = true;
}

void XSDParser::ParseList(DTDElement& node)
{
    ERR_POST_X(10, Warning
        << "Unsupported element type: list; in node "
        << node.GetName());
    node.SetType(DTDElement::eString);
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
    m_ExpectLastComment = true;
}

string XSDParser::ParseAttributeContent()
{
    TToken tok = GetRawAttributeSet();
    string name;
    if (GetAttribute("ref")) {
        name = m_Value;
    }
    if (GetAttribute("name")) {
        name = m_Value;
        m_MapAttribute[name].SetName(name);
        SetCommentsIfEmpty(&(m_MapAttribute[name].Comments()));
    }
    if (GetAttribute("type")) {
        if (!DefineAttributeType(m_MapAttribute[name])) {
            m_MapAttribute[name].SetTypeName(m_Value);
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(m_MapAttribute[name]);
    }
    m_ExpectLastComment = true;
    return name;
}

void XSDParser::ParseContent(DTDAttribute& att)
{
    TToken tok;
    for ( tok=GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
        switch (tok) {
        case T_EOF:
            return;
        case K_ENUMERATION:
            ParseEnumeration(att);
            break;
        case K_EXTENSION:
            ParseExtension(att);
            break;
        case K_RESTRICTION:
            ParseRestriction(att);
            break;
        case K_ANNOTATION:
            SetCommentsIfEmpty(&(att.Comments()));
            ParseAnnotation();
            break;
        case K_UNION:
            ParseUnion(att);
            break;
        case K_LIST:
            ParseList(att);
            break;
        default:
            tok = GetRawAttributeSet();
            if (tok == K_CLOSING) {
                ParseContent(att);
            }
            break;
        }
    }
}

void XSDParser::ParseExtension(DTDAttribute& att)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("base")) {
        if (!DefineAttributeType(att)) {
            if (m_ResolveTypes) {
                string id = CreateEntityId(m_Value,DTDEntity::eType);
                if (m_MapEntity.find(id) != m_MapEntity.end()) {
                    PushEntityLexer(id);
                } else {
                    ParseError("Unresolved entity", id.c_str());
                }
            } else {
                att.SetTypeName(m_Value);
            }
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
}

void XSDParser::ParseRestriction(DTDAttribute& att)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("base")) {
        if (!DefineAttributeType(att)) {
            if (m_ResolveTypes) {
                string id = CreateEntityId(m_Value,DTDEntity::eType);
                if (m_MapEntity.find(id) != m_MapEntity.end()) {
                    PushEntityLexer(id);
                } else {
                    ParseError("Unresolved entity", id.c_str());
                }
            } else {
                att.SetTypeName(m_Value);
            }
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
}

void XSDParser::ParseEnumeration(DTDAttribute& att)
{
    TToken tok = GetRawAttributeSet();
    att.SetType(DTDAttribute::eEnum);
    int id = 0;
    if (GetAttribute("intvalue")) {
        id = NStr::StringToInt(m_Value);
        att.SetType(DTDAttribute::eIntEnum);
    }
    if (GetAttribute("value")) {
        att.AddEnumValue(m_Value, Lexer().CurrentLine(), id);
    }
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
}

void XSDParser::ParseUnion(DTDAttribute& att)
{
    ERR_POST_X(9, Warning
        << "Unsupported attribute type: union; in attribute "
        << att.GetName());
    att.SetType(DTDAttribute::eString);
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
}

void XSDParser::ParseList(DTDAttribute& att)
{
    ERR_POST_X(10, Warning
        << "Unsupported attribute type: list; in attribute "
        << att.GetName());
    att.SetType(DTDAttribute::eString);
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
}

string XSDParser::CreateTmpEmbeddedName(const string& name, int emb)
{
    string emb_name(name);
    emb_name += "__emb#__";
    emb_name += NStr::IntToString(emb);
    while (m_EmbeddedNames.find(emb_name) != m_EmbeddedNames.end()) {
        emb_name += 'a';
    }
    m_EmbeddedNames.insert(emb_name);
    return emb_name;
}

string XSDParser::CreateEntityId( const string& name, DTDEntity::EType type)
{
    string id;
    switch (type) {
    case DTDEntity::eType:
        id = string("type:") + name;
        break;
    case DTDEntity::eGroup:
        id = string("group:") + name;
        break;
    case DTDEntity::eAttGroup:
        id = string("attgroup:") + name;
        break;
    default:
        id = name;
        break;
    }
    return id;
}

void XSDParser::CreateTypeDefinition(DTDEntity::EType type)
{
    string id, name, data;
    TToken tok;
    data += "<" + m_Raw;
    for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken()) {
        data += " " + m_Raw;
        if (IsAttribute("name")) {
            name = m_Value;
            id = CreateEntityId(name,type);
            m_MapEntity[id].SetName(name);
        }
    }
    data += m_Raw;
    if (name.empty()) {
        ParseError("name");
    }
    m_MapEntity[id].SetData(data);
    if (tok == K_CLOSING) {
        ParseTypeDefinition(m_MapEntity[id]);
    }
}

void XSDParser::ParseTypeDefinition(DTDEntity& ent)
{
    string data = ent.GetData();
    string closing;
    TToken tok;
    CComments comments;
    for ( tok=GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
        if (tok == T_EOF) {
            break;
        }
        {
            CComments comm;
            Lexer().FlushCommentsTo(comm);
            if (!comm.Empty()) {
                CNcbiOstrstream buffer;
                comm.PrintDTD(buffer);
                data += CNcbiOstrstreamToString(buffer);
                data += closing;
                closing.erase();
            }
        }
        if (tok == K_DOCUMENTATION) {
            data += "<" + m_Raw;
            m_Comments = &comments;
            ParseDocumentation();
            if (m_Raw == "/>") {
                data += "/>";
                closing.erase();
            } else {
                data += ">";
                closing = m_Raw;
            }
        } else if (tok == K_APPINFO) {
            ParseAppInfo();
        } else {
            data += "<" + m_Raw;
            for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken()) {
                data += " " + m_Raw;
            }
            data += m_Raw;
        }
        if (tok == K_CLOSING) {
            ent.SetData(data);
            ParseTypeDefinition(ent);
            data = ent.GetData();
        }
    }
    if (!comments.Empty()) {
        CNcbiOstrstream buffer;
        comments.Print(buffer, "", "\n", "");
        data += CNcbiOstrstreamToString(buffer);
        data += closing;
    }
    data += '\n';
    data += m_Raw;
    ent.SetData(data);
}

void XSDParser::ProcessNamedTypes(void)
{
    m_ResolveTypes = true;
    bool found;
    do {
        found = false;
        map<string,DTDElement>::iterator i;
        for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {

            DTDElement& node = i->second;
            if (!node.GetTypeName().empty()) {
                if ( node.GetType() == DTDElement::eUnknown) {
                    found = true;
// in rare cases of recursive type definition this node type can already be defined
                    map<string,DTDElement>::iterator j;
                    for (j = m_MapElement.begin(); j != m_MapElement.end(); ++j) {
                        if (j->second.GetName() == node.GetName() &&
                            j->second.GetTypeName() == node.GetTypeName() &&
                            j->second.GetType() != DTDElement::eUnknown) {
                            m_MapElement[i->first] = j->second;
                            break;
                        }
                    }
                    if (j != m_MapElement.end()) {
                        break;
                    }
                    PushEntityLexer(CreateEntityId(node.GetTypeName(),DTDEntity::eType));
                    ParseContent(node);
                    node.SetTypeIfUnknown(DTDElement::eEmpty);
// this is not always correct, but it seems that local elements
// defined by means of global types should be made global as well
                    if (node.IsNamed() && node.IsEmbedded()) {
                        node.SetEmbedded(false);
                    }
                } else if ( node.GetType() == DTDElement::eUnknownGroup) {
                    found = true;
                    PushEntityLexer(CreateEntityId(node.GetTypeName(),DTDEntity::eGroup));
                    ParseGroupRef(node);
                    if (node.GetType() == DTDElement::eUnknownGroup) {
                        node.SetType(DTDElement::eEmpty);
                    }
                }
            }
        }
    } while (found);

    do {
        found = false;
        map<string,DTDElement>::iterator i;
        for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {

            DTDElement& node = i->second;
            if (node.HasAttributes()) {
                list<DTDAttribute>& atts = node.GetNonconstAttributes();
                list<DTDAttribute>::iterator a;
                for (a = atts.begin(); a != atts.end(); ++a) {
                    if (a->GetType() == DTDAttribute::eUnknown &&
                        m_MapAttribute.find(a->GetName()) != m_MapAttribute.end()) {
                        found = true;
                        a->Merge(m_MapAttribute[a->GetName()]);
                    }
                }
            }
        }
    } while (found);

    do {
        found = false;
        map<string,DTDElement>::iterator i;
        for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {

            DTDElement& node = i->second;
            if (node.HasAttributes()) {
                list<DTDAttribute>& atts = node.GetNonconstAttributes();
                list<DTDAttribute>::iterator a;
                for (a = atts.begin(); a != atts.end(); ++a) {

                    if (!a->GetTypeName().empty()) { 
                        if ( a->GetType() == DTDAttribute::eUnknown) {
                            found = true;
                            PushEntityLexer(CreateEntityId(a->GetTypeName(),DTDEntity::eType));
                            ParseContent(*a);
                            if (a->GetType() == DTDAttribute::eUnknown) {
                                a->SetType(DTDAttribute::eString);
                            }
                        } else if ( a->GetType() == DTDAttribute::eUnknownGroup) {
                            found = true;
                            PushEntityLexer(CreateEntityId(a->GetTypeName(),DTDEntity::eAttGroup));
                            atts.erase(a);
                            ParseAttributeGroupRef(node);
                            break;
                        }
                    }
                }
            }
        }
    } while (found);
    {
        map<string,DTDElement>::iterator i;
        for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {
            i->second.MergeAttributes();
        }
    }
    m_ResolveTypes = false;
}

void XSDParser::PushEntityLexer(const string& name)
{
    DTDParser::PushEntityLexer(name);

    m_StackPrefixToNamespace.push(m_PrefixToNamespace);
    m_StackNamespaceToPrefix.push(m_NamespaceToPrefix);
    m_StackTargetNamespace.push(m_TargetNamespace);
    m_StackElementFormDefault.push(m_ElementFormDefault);
}

bool XSDParser::PopEntityLexer(void)
{
    if (DTDParser::PopEntityLexer()) {
        m_PrefixToNamespace = m_StackPrefixToNamespace.top();
        m_NamespaceToPrefix = m_StackNamespaceToPrefix.top();
        m_TargetNamespace = m_StackTargetNamespace.top();
        m_ElementFormDefault = m_StackElementFormDefault.top();

        m_StackPrefixToNamespace.pop();
        m_StackNamespaceToPrefix.pop();
        m_StackTargetNamespace.pop();
        m_StackElementFormDefault.pop();
        return true;
    }
    return false;
}

AbstractLexer* XSDParser::CreateEntityLexer(
    CNcbiIstream& in, const string& name, bool autoDelete /*=true*/)
{
    return new XSDEntityLexer(in,name);
}

#if defined(NCBI_DTDPARSER_TRACE)
void XSDParser::PrintDocumentTree(void)
{
    cout << " === Namespaces ===" << endl;
    map<string,string>::const_iterator i;
    for (i = m_PrefixToNamespace.begin(); i != m_PrefixToNamespace.end(); ++i) {
        cout << i->first << ":  " << i->second << endl;
    }
    
    cout << " === Target namespace ===" << endl;
    cout << m_TargetNamespace << endl;
    
    cout << " === Element form default ===" << endl;
    cout << (m_ElementFormDefault ? "qualified" : "unqualified") << endl;
    cout << endl;

    DTDParser::PrintDocumentTree();

    if (!m_MapAttribute.empty()) {
        cout << " === Standalone Attribute definitions ===" << endl;
        map<string,DTDAttribute>::const_iterator a;
        for (a= m_MapAttribute.begin(); a != m_MapAttribute.end(); ++ a) {
            PrintAttribute( a->second, false);
        }
    }
}
#endif

END_NCBI_SCOPE
