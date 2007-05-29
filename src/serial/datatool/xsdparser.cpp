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
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/xsdparser.hpp>
#include <serial/datatool/tokens.hpp>
#include <serial/datatool/module.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// DTDParser

XSDParser::XSDParser(XSDLexer& lexer)
    : DTDParser(lexer)
{
    m_SrcType = eSchema;
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
            CreateTypeDefinition();
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
// unsupported
        case K_ATTRIBUTEGROUP:
            if (GetRawAttributeSet() == K_CLOSING) {
                SkipContent();
            }
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
                if (!m_PrefixToNamespace[m_Attribute].empty()) {
                    ParseError("Unexpected data");
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
        IsValue("anyURI") || IsValue("QName")) {
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
    } else {
        return false;
    }
    return true;
}

bool XSDParser::DefineAttributeType(DTDAttribute& attrib)
{
    if (IsValue("string") || IsValue("QName")) {
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
    } else if (IsValue("int") || IsValue("integer")) {
        attrib.SetType(DTDAttribute::eInteger);
    } else if (IsValue("float") || IsValue("double")) {
        attrib.SetType(DTDAttribute::eDouble);
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
        ERR_POST("LINE " << Location() << " XML declaration is missing");
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
        ParseError("Unexpected token");
    }
}

void XSDParser::ParseInclude(void)
{
    TToken tok;
    string name;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        if (IsAttribute("schemaLocation")) {
            name = m_Value;
        }
    }
    if (tok != K_ENDOFTAG) {
        ParseError("Unexpected token");
    }
    DTDEntity& node = m_MapEntity[name];
    node.SetName(name);
    node.SetData(name);
    node.SetExternal();
    PushEntityLexer(name);
    Reset();
    ParseHeader();
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

TToken XSDParser::GetRawAttributeSet(void)
{
    m_RawAttributes.clear();
    TToken tok;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        m_RawAttributes[m_Attribute] = m_Value;
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
    for ( tok=GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
        if (tok == K_DOCUMENTATION) {
            m_Comments = 0;
            ParseDocumentation();
        } else {
            if (GetRawAttributeSet() == K_CLOSING) {
                SkipContent();
            }
        }
    }
}

string XSDParser::ParseElementContent(DTDElement* owner, int& emb)
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
            name = owner->GetName();
            name += "__emb#__";
            name += NStr::IntToString(emb++);
            m_MapElement[name].SetEmbedded();
            m_MapElement[name].SetNamed();
        }
        m_MapElement[name].SetName(m_Value);
        m_MapElement[name].SetSourceLine(line);
        m_Comments = &(m_MapElement[name].Comments());
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
    if (GetAttribute("minOccurs")) {
        if (!owner || name.empty()) {
            ParseError("Unexpected attribute");
        }
        int m = NStr::StringToInt(m_Value);
        DTDElement::EOccurrence occNow, occNew;
        occNew = occNow = owner->GetOccurrence(name);
        if (m == 0) {
            if (occNow == DTDElement::eOne) {
                occNew = DTDElement::eZeroOrOne;
            } else if (occNow == DTDElement::eOneOrMore) {
                occNew = DTDElement::eZeroOrMore;
            }
        } else if (m != 1) {
            ParseError("Unsupported attribute");
        }
        if (occNow != occNew) {
            owner->SetOccurrence(name, occNew);
        }
    }
    if (GetAttribute("maxOccurs")) {
        if (!owner || name.empty()) {
            ParseError("Unexpected attribute");
        }
        int m = IsValue("unbounded") ? -1 : NStr::StringToInt(m_Value);
        DTDElement::EOccurrence occNow, occNew;
        occNew = occNow = owner->GetOccurrence(name);
        if (m == -1) {
            if (occNow == DTDElement::eOne) {
                occNew = DTDElement::eOneOrMore;
            } else if (occNow == DTDElement::eZeroOrOne) {
                occNew = DTDElement::eZeroOrMore;
            }
        } else if (m != 1) {
            ParseError("Unsupported attribute");
        }
        if (occNow != occNew) {
            owner->SetOccurrence(name, occNew);
        }
    }
    if (tok != K_CLOSING && tok != K_ENDOFTAG) {
        ParseError("Unexpected token");
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

void XSDParser::ParseContent(DTDElement& node)
{
    int emb=0;
    TToken tok;
    for ( tok=GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
        switch (tok) {
        case T_EOF:
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
        case K_ANY:
            if (node.GetType() != DTDElement::eSequence) {
                ParseError("Unexpected element type");
            } else {
                string name = node.GetName();
                name += "__emb#__";
                name += NStr::IntToString(emb++);
                m_MapElement[name].SetName(name);
                m_MapElement[name].SetSourceLine(Lexer().CurrentLine());
                m_MapElement[name].SetEmbedded();
                m_MapElement[name].SetType(DTDElement::eAny);
                ParseAny(m_MapElement[name]);
                AddElementContent(node,name);
            }
            break;
        case K_SEQUENCE:
            if (node.GetType() == DTDElement::eUnknown) {
                node.SetType(DTDElement::eSequence);
                ParseContainer(node);
            } else {
                string name = node.GetName();
                name += "__emb#__";
                name += NStr::IntToString(emb++);
                m_MapElement[name].SetName(name);
                m_MapElement[name].SetSourceLine(Lexer().CurrentLine());
                m_MapElement[name].SetEmbedded();
                m_MapElement[name].SetType(DTDElement::eSequence);
                ParseContainer(m_MapElement[name]);
                AddElementContent(node,name);
            }
            break;
        case K_CHOICE:
            if (node.GetType() == DTDElement::eUnknown) {
                node.SetType(DTDElement::eChoice);
                ParseContainer(node);
            } else {
                string name = node.GetName();
                name += "__emb#__";
                name += NStr::IntToString(emb++);
                m_MapElement[name].SetName(name);
                m_MapElement[name].SetSourceLine(Lexer().CurrentLine());
                m_MapElement[name].SetEmbedded();
                m_MapElement[name].SetType(DTDElement::eChoice);
                ParseContainer(m_MapElement[name]);
                AddElementContent(node,name);
            }
            break;
        case K_ELEMENT:
            {
	            string name = ParseElementContent(&node,emb);
	            AddElementContent(node,name);
            }
            break;
        case K_DOCUMENTATION:
            m_Comments = &(node.Comments());
            ParseDocumentation();
            break;
        default:
            for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken())
                ;
            if (tok == K_CLOSING) {
                ParseContent(node);
            }
            break;
        }
    }
    FixEmbeddedNames(node);
}

void XSDParser::ParseDocumentation(void)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        XSDLexer& l = dynamic_cast<XSDLexer&>(Lexer());
        while (l.ProcessDocumentation())
            ;
    }
    tok = GetNextToken();
    if (tok != K_ENDOFTAG) {
        ParseError("Unexpected tag", "endoftag");
    }
    m_ExpectLastComment = true;
}

void XSDParser::ParseContainer(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    m_ExpectLastComment = true;

    if (GetAttribute("minOccurs")) {
        int m = NStr::StringToInt(m_Value);
        DTDElement::EOccurrence occNow, occNew;
        occNew = occNow = node.GetOccurrence();
        if (m == 0) {
            if (occNow == DTDElement::eOne) {
                occNew = DTDElement::eZeroOrOne;
            } else if (occNow == DTDElement::eOneOrMore) {
                occNew = DTDElement::eZeroOrMore;
            }
        } else if (m != 1) {
            ParseError("Unsupported attribute");
        }
        if (occNow != occNew) {
            node.SetOccurrence(occNew);
        }
    }
    if (GetAttribute("maxOccurs")) {
        int m = IsValue("unbounded") ? -1 : NStr::StringToInt(m_Value);
        DTDElement::EOccurrence occNow, occNew;
        occNew = occNow = node.GetOccurrence();
        if (m == -1) {
            if (occNow == DTDElement::eOne) {
                occNew = DTDElement::eOneOrMore;
            } else if (occNow == DTDElement::eZeroOrOne) {
                occNew = DTDElement::eZeroOrMore;
            }
        } else if (m != 1) {
            ParseError("Unsupported attribute");
        }
        if (occNow != occNew) {
            node.SetOccurrence(occNew);
        }
    }
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
    if (GetAttribute("base")) {
        if (!DefineElementType(node)) {
            node.SetTypeName(m_Value);
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(node);
    }
}

void XSDParser::ParseRestriction(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("base")) {
        if (!DefineElementType(node)) {
            node.SetTypeName(m_Value);
        }
    }
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
    m_Comments = &(att.Comments());

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

void XSDParser::ParseAny(DTDElement& node)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("processContents")) {
        if (!IsValue("lax") && !IsValue("skip")) {
            ParseError("Unsupported attribute");
        }
    }
    if (GetAttribute("minOccurs")) {
        int m = NStr::StringToInt(m_Value);
        DTDElement::EOccurrence occNow, occNew;
        occNew = occNow = node.GetOccurrence();
        if (m == 0) {
            if (occNow == DTDElement::eOne) {
                occNew = DTDElement::eZeroOrOne;
            } else if (occNow == DTDElement::eOneOrMore) {
                occNew = DTDElement::eZeroOrMore;
            }
        } else if (m != 1) {
            ParseError("Unsupported attribute");
        }
        if (occNow != occNew) {
            node.SetOccurrence(occNew);
        }
    }
    if (GetAttribute("maxOccurs")) {
        int m = IsValue("unbounded") ? -1 : NStr::StringToInt(m_Value);
        DTDElement::EOccurrence occNow, occNew;
        occNew = occNow = node.GetOccurrence();
        if (m == -1) {
            if (occNow == DTDElement::eOne) {
                occNew = DTDElement::eOneOrMore;
            } else if (occNow == DTDElement::eZeroOrOne) {
                occNew = DTDElement::eZeroOrMore;
            }
        } else if (m != 1) {
            ParseError("Unsupported attribute");
        }
        if (occNow != occNew) {
            node.SetOccurrence(occNew);
        }
    }
    if (GetAttribute("namespace")) {
        node.SetNamespaceName(m_Value);
    }
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
        m_Comments = &(m_MapAttribute[name].Comments());
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
        case K_RESTRICTION:
            ParseRestriction(att);
            break;
        case K_DOCUMENTATION:
            m_Comments = &(att.Comments());
            ParseDocumentation();
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

void XSDParser::ParseRestriction(DTDAttribute& att)
{
    TToken tok = GetRawAttributeSet();
    if (GetAttribute("base")) {
        if (!DefineAttributeType(att)) {
            att.SetTypeName(m_Value);
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
    if (GetAttribute("value")) {
        att.AddEnumValue(m_Value, Lexer().CurrentLine());
    }
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
}

void XSDParser::CreateTypeDefinition(void)
{
    string name, data;
    TToken tok;
    data += "<" + m_Raw;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        data += " " + m_Raw;
        if (IsAttribute("name")) {
            name = m_Value;
            m_MapEntity[name].SetName(name);
        }
    }
    data += m_Raw;
    m_MapEntity[name].SetData(data);
    if (tok == K_CLOSING) {
        ParseTypeDefinition(m_MapEntity[name]);
    }
}

void XSDParser::ParseTypeDefinition(DTDEntity& ent)
{
    string data = ent.GetData();
    string closing;
    TToken tok;
    CComments Comments;
    for ( tok=GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
        data += "<" + m_Raw;
        if (tok == K_DOCUMENTATION) {
            data += ">";
            m_Comments = &Comments;
            ParseDocumentation();
            closing = m_Raw;
        } else {
            for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
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
    if (!Comments.Empty()) {
        CNcbiOstrstream buffer;
        Comments.Print(buffer, "", "\n", "");
        data += CNcbiOstrstreamToString(buffer);
        data += closing;
    }
    data += m_Raw;
    ent.SetData(data);
}

void XSDParser::ProcessNamedTypes(void)
{
    bool found;
    do {
        found = false;
        map<string,DTDElement>::iterator i;
        for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {

            DTDElement& node = i->second;
            if ( node.GetType() == DTDElement::eUnknown &&
                !node.GetTypeName().empty()) {
                found = true;
                PushEntityLexer(node.GetTypeName());
                ParseContent(node);
                node.SetTypeIfUnknown(DTDElement::eEmpty);
// this is not always correct, but it seems that local elements
// defined by means of global types should be made global as well
                if (node.IsNamed() && node.IsEmbedded()) {
                    node.SetEmbedded(false);
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
                    if ( a->GetType() == DTDAttribute::eUnknown &&
                        !a->GetTypeName().empty()) {
                        found = true;
                        PushEntityLexer(a->GetTypeName());
                        ParseContent(*a);
                    }
                }
            }
        }
    } while (found);
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
