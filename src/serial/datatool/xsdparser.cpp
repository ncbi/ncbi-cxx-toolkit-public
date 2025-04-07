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
#include "datatool.hpp"
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
    m_EnableNamespaceRedefinition = false;
}

XSDParser::~XSDParser(void)
{
}

void XSDParser::BeginDocumentTree(void)
{
    Reset();
}

void XSDParser::BuildDocumentTree(CDataTypeModule& module)
{
    size_t lexerStackSize = m_StackLexer.size();
    bool skipEof = false;
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
        case T_EOF:
            if (skipEof) {
                skipEof = false;
                break;
            }
            ParseError("Unexpected end-of-file", "keyword");
            return;
        case K_ENDOFTAG:
            if (m_StackLexer.size() > lexerStackSize) {
                skipEof = true;
                break;
            }
            if (m_SrcType == eSchema) {
                ProcessNamedTypes();
            }
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
    m_AttributeFormDefault = false;
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
    if (tok == T_IDENTIFIER) {
        m_Raw = m_IdentifierText;
    } else if (tok == K_ATTPAIR || tok == K_XMLNS) {
// format is
// ns:attr="ns:value"
        if (!NStr::SplitInTwo(data, "=", str1, data2)) {
            ParseError("Unexpected data", "attribute (name=\"value\")");
        }
        NStr::TruncateSpacesInPlace(str1);
        NStr::TruncateSpacesInPlace(data2);
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
                if (!m_EnableNamespaceRedefinition &&
                    !m_PrefixToNamespace[m_Attribute].empty() &&
                    m_PrefixToNamespace[m_Attribute] != data) {
                    ParseError("Unexpected xmlns data", "");
                }
            }
            m_PrefixToNamespace[m_Attribute] = data;
            m_NamespaceToPrefix[data] = m_Attribute;
            m_Value = data;
        } else {
            if (m_Attribute == "targetNamespace") {
                m_Value = data;
            } else {
                if (NStr::SplitInTwo(data, ":", str1, str2)) {
//                    if (m_PrefixToNamespace.find(str1) == m_PrefixToNamespace.end()) {
//                        m_Value = data;
//                    } else {
                        m_Value = str2;
                        m_ValuePrefix = str1;
//                    }
                } else {
                    m_Value = str1;
                }
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

XSDParser::EElementNamespace XSDParser::GetElementNamespace(const string& prefix)
{
    if (m_PrefixToNamespace.find(prefix) != m_PrefixToNamespace.end()) {
        string ns(m_PrefixToNamespace[prefix]);
        if        (ns == "http://www.w3.org/2001/XMLSchema") {
            return eSchemaNamespace;
        } else if (ns == "http://schemas.xmlsoap.org/wsdl/") {
            return eWsdlNamespace;
        } else if (ns == "http://schemas.xmlsoap.org/wsdl/soap/") {
            return eSoapNamespace;
        }
    }
    return eUnknownNamespace;
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
    if (GetElementNamespace(m_ValuePrefix) != eSchemaNamespace) {
        return false;
    }
    if (IsValue("string") || IsValue("token") ||
        IsValue("normalizedString") ||
        IsValue("duration") || IsValue("language") ||
        IsValue("anyType") || IsValue("anyURI") ||
        IsValue("Name") ||IsValue("NCName") || IsValue("QName") ||
        IsValue("dateTime") || IsValue("time") || IsValue("date") ||
        IsValue("gYearMonth") || IsValue("gYear") || IsValue("gMonthDay") || IsValue("gMonth") || IsValue("gDay") || 
        IsValue("anySimpleType")) {
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
    if (GetElementNamespace(m_ValuePrefix) != eSchemaNamespace) {
        return false;
    }
    if (IsValue("string") || IsValue("token") || IsValue("QName") ||
        IsValue("language") ||
        IsValue("anyType") || IsValue("anyURI") || IsValue("NCName") ||
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
        if (m_SrcType == eSchema) {
            ERR_POST_X(4, GetLocation() << " XML declaration is missing");
        }
    }
// schema    
    if (tok != K_SCHEMA) {
        ParseError("Unexpected token", "schema");
    }
    m_EnableNamespaceRedefinition = true;
    for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken()) {
        if (tok == K_ATTPAIR) {
            if (IsAttribute("targetNamespace")) {
                m_TargetNamespace = m_Value;
            } else if (IsAttribute("elementFormDefault")) {
                m_ElementFormDefault = IsValue("qualified");
            } else if (IsAttribute("attributeFormDefault")) {
                m_AttributeFormDefault = IsValue("qualified");
            }
        }
    }
    m_EnableNamespaceRedefinition = false;
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
    string id(CreateEntityId(name, DTDEntity::eEntity));
    DTDEntity& node = m_MapEntity[id];
    if (node.GetName().empty()) {
        node.SetName(name);
        node.SetData(name);
        node.SetExternal();
        PushEntityLexer(id);
        ParseHeader();
    }
}

void XSDParser::ParseImport(void)
{
    bool import=true;
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("namespace")) {
        if (IsValue("http://www.w3.org/XML/1998/namespace") ||
            (IsValue("//www.w3.org/XML/1998/namespace") && m_ValuePrefix == "http")) {
            string name = "xml:lang";
            m_MapAttribute[name].SetName(name);
            m_MapAttribute[name].SetType(DTDAttribute::eString);
            name = "xml:space";
            m_MapAttribute[name].SetName(name);
            m_MapAttribute[name].SetType(DTDAttribute::eString);
            import=false;
        }
    }
    if (import && GetAttribute("schemaLocation")) {
        string name(m_Value);
        string id(CreateEntityId(name, DTDEntity::eEntity));
        DTDEntity& node = m_MapEntity[id];
        if (node.GetName().empty()) {
            node.SetName(name);
            node.SetData(name);
            node.SetExternal();
            PushEntityLexer(id);
            ParseHeader();
        }
        return;
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
            m_RawAttributes[m_Attribute] = make_pair(m_ValuePrefix, m_Value);
        }
    }
    return tok;
}

bool XSDParser::GetAttribute(const string& att)
{
    if (m_RawAttributes.find(att) != m_RawAttributes.end()) {
        m_Attribute = att;
        m_ValuePrefix = m_RawAttributes[att].first;
        m_Value = m_RawAttributes[att].second;
        return true;
    }
    m_Attribute.erase();
    m_ValuePrefix.erase();
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

DTDElement::EOccurrence XSDParser::ParseMinOccurs( DTDElement& node, DTDElement::EOccurrence occNow)
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
            node.AddFacet( CMemberFacet( ESerialFacet::eMinItems, m_Value));
            occNew = DTDElement::eOneOrMore;
        }
    }
    return occNew;
}

DTDElement::EOccurrence XSDParser::ParseMaxOccurs( DTDElement& node, DTDElement::EOccurrence occNow)
{
    DTDElement::EOccurrence occNew = occNow;
    if (GetAttribute("maxOccurs")) {
        int m = IsValue("unbounded") ? -1 : NStr::StringToInt(m_Value);
        if (m == -1 || m > 1) {
            if (m > 1) {
                node.AddFacet( CMemberFacet( ESerialFacet::eMaxItems, m_Value));
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
    string name, value, name_space;
    bool ref=false, named_type=false;
    bool qualified = m_ElementFormDefault;
    int line = Lexer().CurrentLine();

    tok = GetRawAttributeSet();

    if (GetAttribute("ref")) {
        if (!owner) {
            ParseError("ref attribute not allowed for root elements", "");
        }
        if (IsValue("schema") &&
            GetElementNamespace(m_ValuePrefix) == eSchemaNamespace) {
            name = CreateTmpEmbeddedName(owner->GetName(), emb);
            DTDElement& elem = m_MapElement[name];
            elem.SetEmbedded();
            elem.SetName(m_Value);
            elem.SetSourceLine(Lexer().CurrentLine());
            elem.SetType(DTDElement::eAny);
            ref=false;
        } else {
            ref=true;
            name_space = m_ResolveTypes ? m_TargetNamespace :
                                          m_PrefixToNamespace[m_ValuePrefix];
            name = m_Value + NStr::Replace(name_space, ":", "/");
        }
    }
    if (GetAttribute("name")) {
        ref=false;
        name_space = m_TargetNamespace;
        name = m_Value + NStr::Replace(name_space, ":", "/");;
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
            m_MapElement[name].SetTypeName(
                CreateEntityId(m_Value,DTDEntity::eType, &m_ValuePrefix));
            named_type = true;
        }
    }
    if (owner && GetAttribute("form")) {
        qualified = IsValue("qualified");
    }
    if (GetAttribute("default")) {
        m_MapElement[name].SetDefault(m_Value);
    }
    if (GetAttribute("nillable")) {
        m_MapElement[name].SetNillable(IsValue("true"));
    }
    if (owner && !name.empty()) {
        owner->SetOccurrence(name, ParseMinOccurs( m_MapElement[name], owner->GetOccurrence(name)));
        owner->SetOccurrence(name, ParseMaxOccurs( m_MapElement[name], owner->GetOccurrence(name)));
    }
    if (tok != K_CLOSING && tok != K_ENDOFTAG) {
        ParseError("endoftag");
    }
    m_MapElement[name].SetNamespaceName(name_space);
    m_MapElement[name].SetQualified(qualified);
    bool hasContents = false;
    if (tok == K_CLOSING) {
        hasContents = ParseContent(m_MapElement[name]);
    }
    m_ExpectLastComment = true;
    if (!ref && !named_type) {
        named_type = NStr::StartsWith( m_MapElement[name].GetTypeName(), "type:");
        if (!named_type) {
            m_MapElement[name].SetTypeIfUnknown(
                hasContents ? DTDElement::eEmpty : DTDElement::eString);
        }
    }
    return name;
}

string XSDParser::ParseGroup(DTDElement* owner, int emb)
{
    string name;
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("ref")) {

        string id( CreateEntityId(m_Value,DTDEntity::eGroup,&m_ValuePrefix));
        name = CreateTmpEmbeddedName(owner->GetName(), emb);
        DTDElement& node = m_MapElement[name];
        node.SetEmbedded();
        node.SetName(m_Value);
        node.SetOccurrence( ParseMinOccurs( node, node.GetOccurrence()));
        node.SetOccurrence( ParseMaxOccurs( node, node.GetOccurrence()));
        node.SetQualified(owner->IsQualified());
        SetCommentsIfEmpty(&(node.Comments()));

        bool already_there = find(m_StackLexerName.begin(), m_StackLexerName.end(), id) != m_StackLexerName.end();
        if (m_ResolveTypes && !already_there) {
            PushEntityLexer(id);
            ParseGroupRef(node);
        } else {
            node.SetTypeName(id);
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
    if (node.GetName().empty() && GetAttribute("name")) {
        node.SetNamespaceName(m_ValuePrefix.empty() ? m_TargetNamespace : m_PrefixToNamespace[m_ValuePrefix]);
        node.SetName(m_Value);
    }
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
    PopEntityLexer();
}

bool XSDParser::ParseContent(DTDElement& node, bool extended /*=false*/)
{
    DTDElement::EType curr_type;
    int emb=0;
    bool eatEOT= false;
    bool hasContents= false;
    TToken tok;
    for ( tok=GetNextToken(); ; tok=GetNextToken()) {
        emb = (int)node.GetContent().size();
        if (tok != T_EOF &&
            tok != K_ENDOFTAG &&
            tok != K_ANNOTATION) {
            hasContents= true;
        }
        switch (tok) {
        case T_EOF:
            return hasContents;
        case K_ENDOFTAG:
            if (eatEOT) {
                eatEOT= false;
                break;
            }
            FixEmbeddedNames(node);
            return hasContents;
        case K_COMPLEXTYPE:
            ParseComplexType(node);
            break;
        case K_SIMPLETYPE:
            ParseSimpleType(node);
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
        case K_MINLENGTH:
        case K_MAXLENGTH:
        case K_LENGTH:
        case K_PATTERN:
        case K_INCMIN:
        case K_EXCMIN:
        case K_INCMAX:
        case K_EXCMAX:
            ParseFacet(node, tok);
            break;
        case K_ENUMERATION:
            ParseEnumeration(node);
            break;
        case K_ATTRIBUTE:
            ParseAttribute(node);
            break;
        case K_ATTRIBUTEGROUP:
            ParseAttributeGroup(node);
            break;
        case K_ANY:
            node.SetTypeIfUnknown(DTDElement::eSequence);
            {
                string name = CreateTmpEmbeddedName(node.GetName(), emb);
                DTDElement& elem = m_MapElement[name];
                elem.SetEmbedded();
                elem.SetName(name);
                elem.SetSourceLine(Lexer().CurrentLine());
                elem.SetType(DTDElement::eAny);
                elem.SetQualified(node.IsQualified());
                ParseAny(elem);
                AddElementContent(node,name);
            }
            break;
        case K_SEQUENCE:
            emb = (int)node.GetContent().size();
            if (emb != 0 && extended) {
                node.SetTypeIfUnknown(DTDElement::eSequence);
                if (node.GetType() != DTDElement::eSequence) {
                    ParseError("sequence");
                }
                tok = GetRawAttributeSet();
                eatEOT = true;
                break;
            }
            curr_type = node.GetType();
            if (curr_type == DTDElement::eUnknown ||
                curr_type == DTDElement::eUnknownGroup ||
                (m_ResolveTypes && curr_type == DTDElement::eEmpty)) {
                node.SetType(DTDElement::eSequence);
                ParseContainer(node);
                if (node.GetContent().empty()) {
                    node.ResetType(curr_type);
                }
            } else {
                string name = CreateTmpEmbeddedName(node.GetName(), emb);
                DTDElement& elem = m_MapElement[name];
                elem.SetEmbedded();
                elem.SetName(name);
                elem.SetSourceLine(Lexer().CurrentLine());
                elem.SetType(DTDElement::eSequence);
                elem.SetQualified(node.IsQualified());
                ParseContainer(elem);
                AddElementContent(node,name);
            }
            break;
        case K_CHOICE:
            curr_type = node.GetType();
            if (curr_type == DTDElement::eUnknown ||
                curr_type == DTDElement::eUnknownGroup ||
                (m_ResolveTypes && curr_type == DTDElement::eEmpty)) {
                node.SetType(DTDElement::eChoice);
                ParseContainer(node);
                if (node.GetContent().empty()) {
                    node.ResetType(curr_type);
                }
            } else {
                string name = CreateTmpEmbeddedName(node.GetName(), emb);
                DTDElement& elem = m_MapElement[name];
                elem.SetEmbedded();
                elem.SetName(name);
                elem.SetSourceLine(Lexer().CurrentLine());
                elem.SetType(DTDElement::eChoice);
                elem.SetQualified(node.IsQualified());
                ParseContainer(elem);
                AddElementContent(node,name);
            }
            break;
        case K_SET:
            curr_type = node.GetType();
            if (curr_type == DTDElement::eUnknown ||
                curr_type == DTDElement::eUnknownGroup ||
                (m_ResolveTypes && curr_type == DTDElement::eEmpty)) {
                node.SetType(DTDElement::eSet);
                ParseContainer(node);
                if (node.GetContent().empty()) {
                    node.ResetType(curr_type);
                }
            } else {
                string name = CreateTmpEmbeddedName(node.GetName(), emb);
                DTDElement& elem = m_MapElement[name];
                elem.SetEmbedded();
                elem.SetName(name);
                elem.SetSourceLine(Lexer().CurrentLine());
                elem.SetType(DTDElement::eSet);
                elem.SetQualified(node.IsQualified());
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
        case K_UNION:
            ParseUnion(node);
            break;
        case K_LIST:
            ParseList(node);
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
    return hasContents;
}

void XSDParser::ParseContainer(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    m_ExpectLastComment = true;
    node.SetOccurrence( ParseMinOccurs( node, node.GetOccurrence()));
    node.SetOccurrence( ParseMaxOccurs( node, node.GetOccurrence()));
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseComplexType(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (node.GetName().empty() && GetAttribute("name")) {
        node.SetNamespaceName(m_ValuePrefix.empty() ? m_TargetNamespace : m_PrefixToNamespace[m_ValuePrefix]);
        node.SetName(m_Value);
        node.SetNamed();
    }
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

void XSDParser::ParseSimpleType(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (node.GetName().empty() && GetAttribute("name")) {
        node.SetNamespaceName(m_ValuePrefix.empty() ? m_TargetNamespace : m_PrefixToNamespace[m_ValuePrefix]);
        node.SetName(m_Value);
        node.SetNamed();
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
            string id( CreateEntityId(m_Value,DTDEntity::eType,&m_ValuePrefix));
            if (m_ResolveTypes) {
                PushEntityLexer(id);
                ParseContent(node);
                extended=true;
            } else {
                node.SetTypeName(id);
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
            string id( CreateEntityId(m_Value,DTDEntity::eType,&m_ValuePrefix));
            if (m_ResolveTypes) {
                PushEntityLexer(id);
                ParseContent(node);
                extended=true;
            } else {
                node.SetTypeName(id);
            }
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(node,extended);
    }
}

void XSDParser::ParseFacet(DTDElement& node, TToken facet)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("value")) {
        switch (facet) {
        case K_MINLENGTH:
            node.AddFacet( CMemberFacet( ESerialFacet::eMinLength, m_Value));
            break;
        case K_MAXLENGTH:
            node.AddFacet( CMemberFacet( ESerialFacet::eMaxLength, m_Value));
            break;
        case K_LENGTH:
            node.AddFacet( CMemberFacet( ESerialFacet::eLength, m_Value));
            break;
        case K_PATTERN:
            node.AddFacet( CMemberFacet( ESerialFacet::ePattern, m_Value));
            break;
        case K_INCMIN:
            node.AddFacet( CMemberFacet( ESerialFacet::eInclusiveMinimum, m_Value));
            break;
        case K_EXCMIN:
            node.AddFacet( CMemberFacet( ESerialFacet::eExclusiveMinimum, m_Value));
            break;
        case K_INCMAX:
            node.AddFacet( CMemberFacet( ESerialFacet::eInclusiveMaximum, m_Value));
            break;
        case K_EXCMAX:
            node.AddFacet( CMemberFacet( ESerialFacet::eExclusiveMaximum, m_Value));
            break;
        default:
            break;
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseEnumeration(DTDElement& node)
{
    TToken tok;
    if (DataTool().IsSetCodeGenerationStyle(CDataTool::eXmlElementEnums)) {
        tok = GetRawAttributeSet();
        if (node.GetType() == DTDElement::eInteger) {
            node.ResetType(DTDElement::eUnknown);
            node.SetType(DTDElement::eIntEnum);
        } else if (node.GetType() == DTDElement::eString) {
            node.ResetType(DTDElement::eUnknown);
            node.SetType(DTDElement::eEnum);
        } else if (node.GetType() != DTDElement::eIntEnum && node.GetType() != DTDElement::eEnum) {
            ParseError("enum restriction not supported", "string or integer type");
        }
        if (GetAttribute("value")) {
            node.AddContent(m_Value);
        }
        return;
    }
    for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken())
        ;
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseAttribute(DTDElement& node)
{
    DTDAttribute a;
    node.AddAttribute(a);
    DTDAttribute& att = node.GetNonconstAttributes().back();
    att.SetSourceLine(Lexer().CurrentLine());
    SetCommentsIfEmpty(&(att.Comments()));
    bool ref=false, named_type=false;
    bool qualified = m_AttributeFormDefault;

    TToken tok = GetRawAttributeSet();
    if (GetAttribute("ref")) {
        att.SetName(m_Value);
        if (m_ValuePrefix == "xml") {
            att.SetName(m_ValuePrefix + ":" + m_Value);
        }
        ref=true;
    }
    if (GetAttribute("name")) {
        att.SetName(m_Value);
    }
    if (GetAttribute("type")) {
        if (!DefineAttributeType(att)) {
            att.SetTypeName(
                CreateEntityId(m_Value,DTDEntity::eType,&m_ValuePrefix));
            named_type = true;
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
    if (GetAttribute("form")) {
        qualified = IsValue("qualified");
    }
    att.SetQualified(qualified);
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
    if (!ref && !named_type) {
        att.SetTypeIfUnknown(DTDAttribute::eString);
    }
    m_ExpectLastComment = true;
}

void XSDParser::ParseAttributeGroup(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("ref")) {
        string id( CreateEntityId(m_Value,DTDEntity::eAttGroup,&m_ValuePrefix));
        if (m_ResolveTypes) {
            PushEntityLexer(id);
            ParseAttributeGroupRef(node);
        } else {
            DTDAttribute a;
            a.SetType(DTDAttribute::eUnknownGroup);
            a.SetTypeName(id);
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
    PopEntityLexer();
}

void XSDParser::ParseAny(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
#if 0
    if (GetAttribute("processContents")) {
        if (!IsValue("lax") && !IsValue("skip")) {
            ParseError("lax or skip");
        }
    }
#endif
    node.SetOccurrence( ParseMinOccurs( node, node.GetOccurrence()));
    node.SetOccurrence( ParseMaxOccurs( node, node.GetOccurrence()));
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
        << GetLocation()
        << "Unsupported element type: union; in node "
        << node.GetName());
    node.SetType(DTDElement::eString);
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
//        ParseContent(node);
        SkipContent();
    }
    m_ExpectLastComment = true;
}

void XSDParser::ParseList(DTDElement& node)
{
    ERR_POST_X(10, Warning
        << GetLocation()
        << "Unsupported element type: list; in node "
        << node.GetName());
    node.SetType(DTDElement::eString);
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
//        ParseContent(node);
        SkipContent();
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
            m_MapAttribute[name].SetTypeName(
                CreateEntityId(m_Value, DTDEntity::eType,&m_ValuePrefix));
        }
    }
    m_MapAttribute[name].SetQualified(true);
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
            string id( CreateEntityId(m_Value,DTDEntity::eType,&m_ValuePrefix));
            if (m_ResolveTypes) {
                PushEntityLexer(id);
                ParseContent(att);
            } else {
                att.SetTypeName(id);
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
            string id( CreateEntityId(m_Value,DTDEntity::eType,&m_ValuePrefix));
            if (m_ResolveTypes) {
                PushEntityLexer(id);
                ParseContent(att);
            } else {
                att.SetTypeName(id);
            }
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
}

void XSDParser::ParseEnumeration(DTDAttribute& att)
// enumeration
// http://www.w3.org/TR/2004/REC-xmlschema-2-20041028/datatypes.html#rf-enumeration
// actual value
// http://www.w3.org/TR/2004/REC-xmlschema-1-20041028/structures.html#key-vv
{
    TToken tok = GetRawAttributeSet();
    if (att.GetType() == DTDAttribute::eInteger ||
        att.GetType() == DTDAttribute::eBigInt ||
        att.GetType() == DTDAttribute::eIntEnum) {
        att.SetType(DTDAttribute::eIntEnum);
    } else {
        att.SetType(DTDAttribute::eEnum);
    }
    int id = 0;
    bool hasint = false;
    if (GetAttribute("intvalue")) {
        id = NStr::StringToInt(m_Value);
        att.SetType(DTDAttribute::eIntEnum);
        hasint = true;
    }
    if (GetAttribute("value")) {
        if (!hasint && att.GetType() == DTDAttribute::eIntEnum) {
            id = NStr::StringToInt(m_Value);
        }
        string v(m_ValuePrefix);
        if (!v.empty()) {
            v += ':';
        }
        v += m_Value;
        NStr::TruncateSpacesInPlace(v);
        att.AddEnumValue(v, Lexer().CurrentLine(), id);
    }
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
}

void XSDParser::ParseUnion(DTDAttribute& att)
{
    ERR_POST_X(9, Warning
        << GetLocation()
        << "Unsupported attribute type: union; in attribute "
        << att.GetName());
    att.SetType(DTDAttribute::eString);
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
//        ParseContent(att);
        SkipContent();
    }
}

void XSDParser::ParseList(DTDAttribute& att)
{
    ERR_POST_X(10, Warning
        << GetLocation()
        << "Unsupported attribute type: list; in attribute "
        << att.GetName());
    att.SetType(DTDAttribute::eString);
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
//        ParseContent(att);
        SkipContent();
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

string XSDParser::CreateEntityId(
    const string& name, DTDEntity::EType type, const string* prefix)
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
    case DTDEntity::eWsdlInterface:
        id = string("interface:") + name;
        break;
    case DTDEntity::eWsdlBinding:
        id = string("binding:") + name;
        break;
    default:
        id = name;
        break;
    }
    if (prefix) {
        if (m_PrefixToNamespace.find(*prefix) == m_PrefixToNamespace.end() && !prefix->empty()) {
            string msg("Namespace prefix not defined: ");
            msg += *prefix;
            ParseError(msg.c_str(), "namespace declaration");
        }
        id += m_PrefixToNamespace[*prefix];
    } else {
        id += m_TargetNamespace;
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
            id = CreateEntityId(m_Value,type);
            m_MapEntity[id].SetName(name);
        }
    }
    data += m_Raw;
    if (name.empty()) {
        ParseError("name");
    }
    m_MapEntity[id].SetData(data);
    m_MapEntity[id].SetType(type);
    m_MapEntity[id].SetParseAttributes( m_TargetNamespace,
        m_ElementFormDefault,m_AttributeFormDefault,
        m_PrefixToNamespace);
    Lexer().FlushComments();
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
    bool doctag_open = false;
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
            if (!closing.empty()) {
                if (!comments.Empty()) {
                    CNcbiOstrstream buffer;
                    comments.Print(buffer, "", "\n", "");
                    data += CNcbiOstrstreamToString(buffer);
                    comments = CComments();
                }
                data += closing;
                closing.erase();
                doctag_open = false;
            }
        }
        if (tok == K_DOCUMENTATION) {
            if (!doctag_open) {
                data += "<" + m_Raw;
            }
            m_Comments = &comments;
            ParseDocumentation();
            if (!doctag_open) {
                if (m_Raw == "/>") {
                    data += "/>";
                    closing.erase();
                } else {
                    data += ">";
                    closing = m_Raw;
                    doctag_open = true;
                    EndCommentBlock();
                }
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
    }
    data += closing;
    data += '\n';
    data += m_Raw;
    ent.SetData(data);
    m_Comments = 0;
}

void XSDParser::ProcessNamedTypes(void)
{
    const CDataTool& app = DataTool();
    m_ResolveTypes = true;
    set<string> processed;
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
                    bool elementForm = m_ElementFormDefault;
                    if (app.IsSetCodeGenerationStyle(CDataTool::eNoGlobalTypeClasses)) {
                        PushEntityLexer(node.GetTypeName());
                        bool hasContents = ParseContent(node);
                        node.SetTypeIfUnknown(hasContents ? DTDElement::eEmpty : DTDElement::eString);
                    } else {
                        if (m_MapElement.find(node.GetTypeName()) != m_MapElement.end()) {
                            node.SetType(DTDElement::eAlias);
                        } else {
                            PushEntityLexer(node.GetTypeName());
                            DTDElement item;
                            bool hasContents = ParseContent(item);
                            if (item.GetName().empty() || 
                                (!item.GetName().empty() && item.GetName() == node.GetName()) || 
                                item.GetNamespaceName() != node.GetNamespaceName()) {
                                PushEntityLexer(node.GetTypeName());
                                hasContents = ParseContent(node);
                                node.SetTypeIfUnknown(hasContents ? DTDElement::eEmpty : DTDElement::eString);
                            } else {
                                item.SetTypeIfUnknown(hasContents ? DTDElement::eEmpty : DTDElement::eString);
                                DTDElement::EType  itype = item.GetType();
                                bool pod = (itype >= DTDElement::eString) && (itype <= DTDElement::eBase64Binary);
                                if (node.IsEmbedded() && pod &&
                                    item.GetContent().empty() && !item.HasAttributes() &&
                                    node.GetContent().empty() && !node.HasAttributes()) {
// special case - plain std type
                                    node.SetType(item.GetType());
                                    for( const CMemberFacet& f : item.GetRestrictions()) {
                                        node.AddFacet(f);
                                    }
                                } else {
                                    item.SetEmbedded(false);
                                    item.SetGlobalType(true);
                                    m_MapElement[node.GetTypeName()] = item;
                                    node.SetType(DTDElement::eAlias);
                                }
                            }
                        }
                    }

// Make local elements defined by means of global types global.
// In fact, this is incorrect; also, in case of unqualified form default we must keep
// such elements embedded, otherwise they will be treated as ns-qualified.

// for us, this trick solves the problem of recursive type definitions:
// local element A contains local element B, which contains local element A, etc.
// the way it is now, code generator will simply crash.
// The better solution would be to modify C++ code generation, of course.

// as of 24may11, the code generator is modified.
// BUT, the mistake is already made; we want to provide backward compatibility now.
                    if (!app.IsSetCodeGenerationStyle(CDataTool::ePreserveNestedElements) &&
                        node.IsNamed() && node.IsEmbedded() && elementForm) {

                        map<string,DTDElement>::iterator k;
                        for (k = m_MapElement.begin(); k != m_MapElement.end(); ++k) {
                            if (!k->second.IsEmbedded() && k->second.IsNamed() &&
                                k->second.GetName() == node.GetName() && 
                                k->second.GetTypeName() != node.GetTypeName()) {
                                break;
                            }
                        }
                        if (k == m_MapElement.end()) {
                            node.SetEmbedded(false);
                        }
                    }
                } else if ( node.GetType() == DTDElement::eUnknownGroup) {
                    found = true;
                    if (app.IsSetCodeGenerationStyle(CDataTool::eNoGlobalGroupClasses) ||
                        node.GetOccurrence() == DTDElement::eZero) {
                        PushEntityLexer(node.GetTypeName());
                        ParseGroupRef(node);
                        if (node.GetType() == DTDElement::eUnknownGroup) {
                            node.SetType(DTDElement::eEmpty);
                        }
                    } else {
                        if (m_MapElement.find(node.GetTypeName()) != m_MapElement.end()) {
                            node.SetType(DTDElement::eAlias);
                            node.SetGlobalGroup(true);
                        } else {
                            PushEntityLexer(node.GetTypeName());
                            DTDElement item;
                            ParseGroupRef(item);
                            if (item.GetName().empty()) {
                                ParseError("Anonymous group", "group name");
                            } else {
                                item.SetEmbedded(false);
                                item.SetNamed(false);
                                item.SetGlobalGroup(true);
                                m_MapElement[node.GetTypeName()] = item;
                                DTDElement::EOccurrence occ = node.GetOccurrence();
                                if (occ == DTDElement::eOne || occ == DTDElement::eZeroOrOne) {
                                    node.SetType(DTDElement::eAlias);
                                    node.SetGlobalGroup(true);
                                } else if (occ == DTDElement::eOneOrMore || occ == DTDElement::eZeroOrMore) {
                                    node.SetType(DTDElement::eSequence);
                                    node.SetName(item.GetName());
                                    node.SetOccurrence(occ == DTDElement::eZeroOrMore ? DTDElement::eZeroOrOne : DTDElement::eOne);
                                    string tmp2 = node.GetTypeName();
                                    node.SetOccurrence(tmp2, occ);
                                    node.SetGlobalGroup(true);
                                    AddElementContent(node, tmp2);
                                }
                            }
                        }
                    }
                }
                else if (processed.find(i->second.GetName() + i->second.GetNamespaceName())
                            == processed.end()) {
                    if (node.GetType() < DTDElement::eWsdlService) {
                        PushEntityLexer(node.GetTypeName());
                        ParseContent(node);
                    }
                }
                processed.insert(i->second.GetName() + i->second.GetNamespaceName());
            } else {
                node.SetTypeIfUnknown(node.GetContent().empty() ? DTDElement::eEmpty : DTDElement::eString);
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
                            PushEntityLexer(a->GetTypeName());
                            ParseContent(*a);
                            a->SetTypeIfUnknown(DTDAttribute::eString);
                        } else if ( a->GetType() == DTDAttribute::eUnknownGroup) {
                            found = true;
                            PushEntityLexer(a->GetTypeName());
                            atts.erase(a);
                            ParseAttributeGroupRef(node);
                            break;
                        }
                    } else {
                        a->SetTypeIfUnknown(DTDAttribute::eString);
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
                        a->GetTypeName().empty() &&
                        m_MapAttribute.find(a->GetName()) != m_MapAttribute.end()) {
                        found = true;
                        a->Merge(m_MapAttribute[a->GetName()]);
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

void XSDParser::BeginScope(DTDEntity* ent)
{
    m_StackPrefixToNamespace.push(m_PrefixToNamespace);
    m_StackNamespaceToPrefix.push(m_NamespaceToPrefix);
    m_StackTargetNamespace.push(m_TargetNamespace);
    m_StackElementFormDefault.push(m_ElementFormDefault);
    m_StackAttributeFormDefault.push(m_AttributeFormDefault);
    if (ent && ent->GetType() != DTDEntity::eEntity) {
        ent->GetParseAttributes(m_TargetNamespace,
            m_ElementFormDefault,m_AttributeFormDefault,
            m_PrefixToNamespace);
    }
}
void XSDParser::EndScope(void)
{
    m_PrefixToNamespace = m_StackPrefixToNamespace.top();
    m_NamespaceToPrefix = m_StackNamespaceToPrefix.top();
    m_TargetNamespace = m_StackTargetNamespace.top();
    m_ElementFormDefault = m_StackElementFormDefault.top();
    m_AttributeFormDefault = m_StackAttributeFormDefault.top();

    m_StackPrefixToNamespace.pop();
    m_StackNamespaceToPrefix.pop();
    m_StackTargetNamespace.pop();
    m_StackElementFormDefault.pop();
    m_StackAttributeFormDefault.pop();
}

DTDEntity* XSDParser::PushEntityLexer(const string& name)
{
    DTDEntity* ent = DTDParser::PushEntityLexer(name);
    BeginScope(ent);
    return ent;
}

bool XSDParser::PopEntityLexer(void)
{
    if (DTDParser::PopEntityLexer()) {
        EndScope();
        return true;
    }
    return false;
}

AbstractLexer* XSDParser::CreateEntityLexer(
    CNcbiIstream& in, const string& name, bool /*autoDelete*/ /*=true*/)
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
    cout << " === Attribute form default ===" << endl;
    cout << (m_AttributeFormDefault ? "qualified" : "unqualified") << endl;
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
