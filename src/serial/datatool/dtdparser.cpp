
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
*   DTD parser
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/dtdparser.hpp>
#include <serial/datatool/tokens.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/moduleset.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/statictype.hpp>
#include <serial/datatool/enumtype.hpp>
#include <serial/datatool/reftype.hpp>
#include <serial/datatool/unitype.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/datatool/choicetype.hpp>
#include <serial/datatool/value.hpp>
#include <algorithm>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE

const string& s_SpecialName = "#PCDATA";

/////////////////////////////////////////////////////////////////////////////
// DTDParser

DTDParser::DTDParser(DTDLexer& lexer)
    : AbstractParser(lexer)
{
    m_StackLexer.push(&lexer);
}

DTDParser::~DTDParser(void)
{
}


AutoPtr<CFileModules> DTDParser::Modules(const string& fileName)
{
    AutoPtr<CFileModules> modules(new CFileModules(fileName));

    while( GetNextToken() != T_EOF ) {
        CDirEntry entry(fileName);
        m_StackPath.push(entry.GetDir());
        m_StackLexerName.push_back(fileName);
        modules->AddModule(Module(entry.GetBase()));
        m_StackLexerName.pop_back();
        m_StackPath.pop();
    }
//    CopyComments(modules->LastComments());
    return modules;
}

AutoPtr<CDataTypeModule> DTDParser::Module(const string& name)
{
    AutoPtr<CDataTypeModule> module(new CDataTypeModule(name));

    BuildDocumentTree();

#if defined(NCBI_DTDPARSER_TRACE)
    PrintDocumentTree();
#endif

    GenerateDataTree(*module);
    return module;
}

void DTDParser::BuildDocumentTree(void)
{
    bool conditional_ignore = false;
    int conditional_level = 0;
    for (;;) {
        try {
            switch ( GetNextToken() ) {
            case K_ELEMENT:
                ConsumeToken();
                BeginElementContent();
                break;
            case K_ATTLIST:
                ConsumeToken();
                BeginAttributesContent();
                break;
            case K_ENTITY:
                ConsumeToken();
                BeginEntityContent();
                break;
            case T_EOF:
                return;
            case K_INCLUDE:
                ConsumeToken();
                break;
            case K_IGNORE:
                ConsumeToken();
                conditional_ignore = true;
                break;
            case T_CONDITIONAL_BEGIN:
                ++conditional_level;
                break;
            case T_CONDITIONAL_END:
                if (conditional_level == 0) {
                    ParseError("Incorrect format: unexpected end of conditional section",
                               "keyword");
                }
                --conditional_level;
                break;
            case T_SYMBOL:
                if(NextToken().GetSymbol() != '[') {
                    ParseError("Incorrect format","[");
                }
                ConsumeToken();
                if (conditional_ignore) {
                    SkipConditionalSection();
                    --conditional_level;
                    conditional_ignore = false;
                }
                break;
            default:
                ParseError("Invalid keyword", "keyword");
                return;
            }
        }
        catch (CException& e) {
            NCBI_RETHROW_SAME(e,"DTDParser::BuildDocumentTree: failed");
        }
        catch (exception& e) {
            ERR_POST(e.what());
            throw;
        }
    }
}

void DTDParser::SkipConditionalSection(void)
{
    for (;;) {
        try {
            switch ( Next() ) {
            case T_EOF:
                return;
            case T_CONDITIONAL_BEGIN:
                SkipConditionalSection();
                break;
            case T_CONDITIONAL_END:
                return;
            case T_IDENTIFIER_END:
                break;
            default:
                Consume();
                break;
            }
        }
        catch (CException& e) {
            NCBI_RETHROW_SAME(e,"DTDParser::BuildDocumentTree: failed");
        }
        catch (exception& e) {
            ERR_POST(e.what());
            throw;
        }
    }
}

string DTDParser::GetLocation(void)
{
    string loc;
    list<string>::const_iterator i;
    for (i = m_StackLexerName.begin(); i != m_StackLexerName.end(); ++i) {
        if (i != m_StackLexerName.begin()) {
            loc += "/";
        }
        loc += *i;
    }
    loc += ": ";
    return loc + AbstractParser::GetLocation();
}

TToken DTDParser::GetNextToken(void)
{
    for (;;) {
        TToken tok = Next();
        switch (tok) {
        case T_ENTITY:
            PushEntityLexer(NextToken().GetText());
            break;
        case T_IDENTIFIER_END:
            if (!m_IdentifierText.empty()) {
                return T_IDENTIFIER;
            }
            break;
        case T_EOF:
            if (PopEntityLexer()) {
                Consume();
                break;
            } else {
                if (!m_IdentifierText.empty()) {
                    return T_IDENTIFIER;
                }
                return tok;
            }
        case T_IDENTIFIER:
            m_IdentifierText += NextToken().GetText();
            Consume();
            break;
        default:
            if (!m_IdentifierText.empty()) {
                return T_IDENTIFIER;
            }
            return tok;
        }
    }
// we should never be here
    return T_EOF;
}

string DTDParser::GetNextTokenText(void)
{
    if (!m_IdentifierText.empty()) {
        return m_IdentifierText;
    }
    GetNextToken();
    if (!m_IdentifierText.empty()) {
        return m_IdentifierText;
    }
    return NextToken().GetText();
}

void  DTDParser::ConsumeToken(void)
{
    if (!m_IdentifierText.empty()) {
        m_IdentifierText.erase();
        return;
    }
    Consume();
}

/////////////////////////////////////////////////////////////////////////////
// DTDParser - elements

void DTDParser::BeginElementContent(void)
{
    // element name
    string name = GetNextTokenText();
    ConsumeToken();
    ParseElementContent(name, false);
}

void DTDParser::ParseElementContent(const string& name, bool embedded)
{
    DTDElement& node = m_MapElement[ name];
    node.SetName(name);
    switch (GetNextToken()) {
    default:
    case T_IDENTIFIER:
        ParseError("incorrect format","element category");
        break;
    case K_ANY:     // category
        node.SetType(DTDElement::eAny);
        ConsumeToken();
        break;
    case K_EMPTY:   // category
        node.SetType(DTDElement::eEmpty);
        ConsumeToken();
        break;
    case T_SYMBOL:     // contents. the symbol must be '('
        ConsumeElementContent(node);
        if (embedded) {
            node.SetEmbedded();
            return;
        }
        break;
    }
    // element description is ended
    GetNextToken();
    ConsumeSymbol('>');
}

void DTDParser::ConsumeElementContent(DTDElement& node)
{
// Element content:
// http://www.w3.org/TR/2000/REC-xml-20001006#sec-element-content

    string id_name;
    char symbol;
    int emb=0;
    bool skip;

    if(NextToken().GetSymbol() != '(') {
        ParseError("Incorrect format","(");
    }

    for (skip = false; ;) {
        if (skip) {
            skip=false;
        } else {
            ConsumeToken();
        }
        switch (GetNextToken()) {
        default:
            ParseError("Unrecognized token","token");
            break;
        case T_EOF:
            ParseError("Unexpected end of file","token");
            break;
        case T_IDENTIFIER:
            id_name = GetNextTokenText();
            if(id_name.empty()) {
                ParseError("Incorrect format","identifier");
            }
            break;
        case K_PCDATA:
            id_name = s_SpecialName;
            break;
        case T_SYMBOL:
            switch (symbol = NextToken().GetSymbol()) {
            case '(':
                // embedded content
                id_name = node.GetName();
                id_name += "__emb#__";
                id_name += NStr::IntToString(emb++);
                ParseElementContent(id_name, true);
                skip = true;
                break;
            case ')':
                AddElementContent(node, id_name);
                EndElementContent( node);
                return;
            case ',':
            case '|':
                AddElementContent(node, id_name, symbol);
                break;
            case '+':
            case '*':
            case '?':
                if(id_name.empty()) {
                    ParseError("Incorrect format","identifier");
                }
                node.SetOccurrence(id_name,
                    symbol == '+' ? DTDElement::eOneOrMore :
                        (symbol == '*' ? DTDElement::eZeroOrMore :
                            DTDElement::eZeroOrOne));
                break;
            default:
                ParseError("Unrecognized symbol","symbol");
                break;
            }
            break;
        }
    }
}

void DTDParser::AddElementContent(DTDElement& node, string& id_name,
    char separator)
{
    DTDElement::EType type = node.GetType();
    if (type != DTDElement::eUnknown &&
        type != DTDElement::eSequence &&
        type != DTDElement::eChoice) {
        ParseError("Unexpected element contents", "");
    }
    if (id_name == s_SpecialName) {
        if (type == DTDElement::eUnknown && separator == 0) {
            node.SetType(DTDElement::eString);
        } else {
            node.AddContent(id_name);
            if (separator != 0) {
                node.SetType(separator == ',' ?
                    DTDElement::eSequence : DTDElement::eChoice);
            }
        }
        id_name.erase();
        return;
    }
    node.AddContent(id_name);
    if (separator != 0) {
        node.SetType(separator == ',' ?
            DTDElement::eSequence : DTDElement::eChoice);
    } else {
        node.SetTypeIfUnknown(DTDElement::eSequence);
    }
    m_MapElement[ id_name].SetReferenced();
    id_name.erase();
}

void DTDParser::EndElementContent(DTDElement& node)
{
    if (NextToken().GetSymbol() != ')') {
        ParseError("Incorrect format", ")");
    }
    ConsumeToken();
// occurrence
    char symbol;
    switch (GetNextToken()) {
    default:
        break;
    case T_SYMBOL:
        switch (symbol = NextToken().GetSymbol()) {
        default:
            break;
        case '+':
        case '*':
        case '?':
            node.SetOccurrence(
                symbol == '+' ? DTDElement::eOneOrMore :
                    (symbol == '*' ? DTDElement::eZeroOrMore :
                        DTDElement::eZeroOrOne));
            ConsumeToken();
            break;
        }
        break;
    }
    FixEmbeddedNames(node);
}

void DTDParser::FixEmbeddedNames(DTDElement& node)
{
    const list<string>& refs = node.GetContent();
    list<string> fixed;
    for (list<string>::const_iterator i= refs.begin(); i != refs.end(); ++i) {
        DTDElement& refNode = m_MapElement[*i];
        if (refNode.IsEmbedded()) {
            for ( int depth=1; depth<100; ++depth) {
                string testName = refNode.CreateEmbeddedName(depth);
                if (find(refs.begin(),refs.end(),testName) == refs.end()) {
                    if (find(fixed.begin(),fixed.end(),testName) == fixed.end()) {
                        fixed.push_back(testName);
                        refNode.SetName(testName);
                        break;
                    }
                }
            }
        }
    }
 }

/////////////////////////////////////////////////////////////////////////////
// DTDParser - entities

void DTDParser::BeginEntityContent(void)
{
// Entity:
// http://www.w3.org/TR/2000/REC-xml-20001006#sec-entity-decl

    TToken tok = GetNextToken();
    if (tok != T_SYMBOL || NextToken().GetSymbol() != '%') {
        ParseError("Incorrect format", "%");
    }

    ConsumeToken();
    tok = GetNextToken();
    if (tok != T_IDENTIFIER) {
        ParseError("identifier");
    }
    // entity name
    string name = GetNextTokenText();
    ConsumeToken();
    ParseEntityContent(name);
}

void DTDParser::ParseEntityContent(const string& name)
{
    DTDEntity& node = m_MapEntity[name];
    node.SetName(name);

    TToken tok = GetNextToken();
    if ((tok==K_SYSTEM) || (tok==K_PUBLIC)) {
        node.SetExternal();
        ConsumeToken();
        if (tok==K_PUBLIC) {
            // skip public id
            tok = GetNextToken();
            if (tok!=T_STRING) {
                ParseError("string");
            }
            ConsumeToken();
        }
        tok = GetNextToken();
    }
    if (tok!=T_STRING) {
        ParseError("string");
    }
    node.SetData(GetNextTokenText());
    ConsumeToken();
    // entity description is ended
    GetNextToken();
    ConsumeSymbol('>');
}

void DTDParser::PushEntityLexer(const string& name)
{
    map<string,DTDEntity>::iterator i = m_MapEntity.find(name);
    if (i == m_MapEntity.end()) {
        ParseError("Undefined entity","entity");
    }
    CNcbiIstream* in;
    string lexer_name;
    if (m_MapEntity[name].IsExternal()) {
        string filename(m_MapEntity[name].GetData());
        string fullname = CDirEntry::MakePath(m_StackPath.top(), filename);
        CFile  file(fullname);
        if (!file.Exists()) {
            ParseError("file not found", fullname.c_str());
        }
        in = new CNcbiIfstream(fullname.c_str());
        if (!((CNcbiIfstream*)in)->is_open()) {
            ParseError("cannot access file",fullname.c_str());
        }
        m_StackPath.push(file.GetDir());
        m_StackLexerName.push_back(fullname);
        lexer_name = fullname;
    } else {
        in = new CNcbiIstrstream(m_MapEntity[name].GetData().c_str());
        m_StackPath.push("");
        m_StackLexerName.push_back(name);
        lexer_name = name;
    }
    DTDEntityLexer *lexer = new DTDEntityLexer(*in,lexer_name);
    m_StackLexer.push(lexer);
    SetLexer(lexer);
}

bool DTDParser::PopEntityLexer(void)
{
    if (m_StackLexer.size() > 1) {
        delete m_StackLexer.top();
        m_StackLexer.pop();
        SetLexer(m_StackLexer.top());
        m_StackPath.pop();
        m_StackLexerName.pop_back();
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
// DTDParser - attributes

void DTDParser::BeginAttributesContent(void)
{
// Attributes
// http://www.w3.org/TR/2000/REC-xml-20001006#attdecls
    // element name
    string name = GetNextTokenText();
    ConsumeToken();
    ParseAttributesContent(name);
}

void DTDParser::ParseAttributesContent(const string& name)
{
    string id_name;
    DTDElement& node = m_MapElement[ name];
    while (GetNextToken()==T_IDENTIFIER) {
        // attribute name
        id_name = GetNextTokenText();
        ConsumeToken();
        ConsumeAttributeContent(node, id_name);
    }
    // attlist description is ended
    GetNextToken();
    ConsumeSymbol('>');
}

void DTDParser::ConsumeAttributeContent(DTDElement& node,
                                        const string& id_name)
{
    bool done=false;
    DTDAttribute attrib;
    attrib.SetName(id_name);
    for (done=false; !done;) {
        switch(GetNextToken()) {
        default:
            ParseError("Unknown token", "token");
            break;
        case T_IDENTIFIER:
            if (attrib.GetType() == DTDAttribute::eUnknown) {
                ParseError(attrib.GetName().c_str(), "attribute type");
            }
            done = true;
            break;
        case T_SYMBOL:
            switch (NextToken().GetSymbol()) {
            default:
                done = true;
                break;
            case '(':
                // parse enumerated list
                attrib.SetType(DTDAttribute::eEnum);
                ConsumeToken();
                ParseEnumeratedList(attrib);
                break;
            }
            break;
        case T_STRING:
            attrib.SetValue(GetNextTokenText());
            break;
        case K_CDATA:
            attrib.SetType(DTDAttribute::eString);
            break;
        case K_ID:
            attrib.SetType(DTDAttribute::eId);
            break;
        case K_IDREF:
            attrib.SetType(DTDAttribute::eIdRef);
            break;
        case K_IDREFS:
            attrib.SetType(DTDAttribute::eIdRefs);
            break;
        case K_NMTOKEN:
            attrib.SetType(DTDAttribute::eNmtoken);
            break;
        case K_NMTOKENS:
            attrib.SetType(DTDAttribute::eNmtokens);
            break;
        case K_ENTITY:
            attrib.SetType(DTDAttribute::eEntity);
            break;
        case K_ENTITIES:
            attrib.SetType(DTDAttribute::eEntities);
            break;
        case K_NOTATION:
            attrib.SetType(DTDAttribute::eNotation);
            break;
        case K_DEFAULT:
            attrib.SetValueType(DTDAttribute::eDefault);
            break;
        case K_REQUIRED:
            attrib.SetValueType(DTDAttribute::eRequired);
            break;
        case K_IMPLIED:
            attrib.SetValueType(DTDAttribute::eImplied);
            break;
        case K_FIXED:
            attrib.SetValueType(DTDAttribute::eFixed);
            break;
        }
        if (!done) {
            ConsumeToken();
        }
    }
    node.AddAttribute(attrib);
}

void DTDParser::ParseEnumeratedList(DTDAttribute& attrib)
{
    for (;;) {
        switch(GetNextToken()) {
        default:
            ParseError("Unknown token", "token");
            break;
        case T_IDENTIFIER:
            attrib.AddEnumValue(GetNextTokenText());
            ConsumeToken();
            break;
        case T_SYMBOL:
            // may be either '|' or ')'
            if (NextToken().GetSymbol() == ')') {
                return;
            }
            ConsumeToken();
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// model generation

void DTDParser::GenerateDataTree(CDataTypeModule& module)
{
    map<string,DTDElement>::iterator i;
    for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {

        if (i->second.GetName().empty() && i->first != s_SpecialName) {
            ParseError(i->first.c_str(),"definition");
        }
        DTDElement::EType type = i->second.GetType();

        if (((type == DTDElement::eSequence) ||
            (type == DTDElement::eChoice) ||
            i->second.HasAttributes()) &&
            !i->second.IsEmbedded())
        {
            ModuleType(module, i->second);
        }
    }
}

void DTDParser::ModuleType(CDataTypeModule& module, const DTDElement& node)
{
    AutoPtr<CDataType> type = Type(node, node.GetOccurrence(), false);
    module.AddDefinition(node.GetName(), type);
}


AutoPtr<CDataType> DTDParser::Type(
    const DTDElement& node, DTDElement::EOccurrence occ,
    bool fromInside, bool ignoreAttrib)
{
    AutoPtr<CDataType> type(x_Type(node, occ, fromInside, ignoreAttrib));
    return type;
}


CDataType* DTDParser::x_Type(
    const DTDElement& node, DTDElement::EOccurrence occ,
    bool fromInside, bool ignoreAttrib)
{
    CDataType* type;

// if the node contains single embedded element - prune it
    if ((!fromInside || node.IsEmbedded()) && !node.HasAttributes()) {
        const list<string>& refs = node.GetContent();
        if (refs.size() == 1) {
            string refName = refs.front();
            DTDElement& refNode = m_MapElement[refName];
            if (refNode.IsEmbedded() &&
                (node.GetOccurrence(refName) == DTDElement::eOne)) {
                type = x_Type(refNode,
                              fromInside ? occ : refNode.GetOccurrence(),
                              fromInside);
                if (node.IsEmbedded()) {
                    DTDElement& emb = const_cast<DTDElement&>(node);
                    emb.SetName(refNode.GetName());
                }
                return type;
            }
        }
    }

    bool uniseq = (occ == DTDElement::eOneOrMore ||
                   occ == DTDElement::eZeroOrMore);
    bool cont = (node.GetType() == DTDElement::eSequence ||
                 node.GetType() == DTDElement::eChoice);
    bool attrib = !ignoreAttrib && node.HasAttributes();
    bool ref = fromInside && !node.IsEmbedded();

    if ((cont && uniseq && (attrib || (node.IsEmbedded() && fromInside))) || (!cont && attrib)) {
        if (ref) {
            type = new CReferenceDataType(node.GetName());
        } else {
            type = CompositeNode(node, occ);
            uniseq = false;
        }
    } else {
        switch (node.GetType()) {
        case DTDElement::eSequence:
            if (ref) {
                type = new CReferenceDataType(node.GetName());
            } else {
                type = TypesBlock(new CDataSequenceType(),node,ignoreAttrib);
            }
            break;
        case DTDElement::eChoice:
            if (ref) {
                type = new CReferenceDataType(node.GetName());
            } else {
                type = TypesBlock(new CChoiceDataType(),node,ignoreAttrib);
            }
            break;
        case DTDElement::eString:
            type = new CStringDataType();
            break;
        case DTDElement::eAny:
            type = new CAnyContentDataType();
            break;
        case DTDElement::eEmpty:
            type = new CNullDataType();
            break;
        default:
            ParseError("Unknown element", "element");
            break;
        }
    }
    if (uniseq) {
        CUniSequenceDataType* uniType = new CUniSequenceDataType(type);
        uniType->SetNonEmpty( occ == DTDElement::eOneOrMore);
        uniType->SetNoPrefix(true);
        type = uniType;
    }
    return type;
}

CDataType* DTDParser::TypesBlock(
    CDataMemberContainerType* containerType,const DTDElement& node,
    bool ignoreAttrib)
{
    AutoPtr<CDataMemberContainerType> container(containerType);

    if (!ignoreAttrib) {
        AddAttributes(container, node);
    }
    const list<string>& refs = node.GetContent();
    for (list<string>::const_iterator i= refs.begin(); i != refs.end(); ++i) {
        if (*i == s_SpecialName) {
            AutoPtr<CDataType> stype(new CStringDataType());
            AutoPtr<CDataMember> smember(new CDataMember("_CharData", stype));
            smember->SetNotag();
            smember->SetNoPrefix();
            container->AddMember(smember);
            continue;
        }
        DTDElement& refNode = m_MapElement[*i];
        if (refNode.GetName().empty()) {
            ParseError(i->c_str(),"definition");
        }
        DTDElement::EOccurrence occ = node.GetOccurrence(*i);
        if (refNode.IsEmbedded()) {
            occ = refNode.GetOccurrence();
        }
        AutoPtr<CDataType> type(Type(refNode, occ, true));
        AutoPtr<CDataMember> member(new CDataMember(refNode.GetName(), type));
        if ((occ == DTDElement::eZeroOrOne) ||
            (occ == DTDElement::eZeroOrMore)) {
            member->SetOptional();
        }
        if (refNode.IsEmbedded()) {
            member->SetNotag();
        }
        member->SetNoPrefix();
        container->AddMember(member);
    }
    return container.release();
}

CDataType* DTDParser::CompositeNode(
    const DTDElement& node, DTDElement::EOccurrence occ)
{
    AutoPtr<CDataMemberContainerType> container(new CDataSequenceType());

    AddAttributes(container, node);
    bool uniseq =
        (occ == DTDElement::eOneOrMore || occ == DTDElement::eZeroOrMore);

    AutoPtr<CDataType> type(Type(node, DTDElement::eOne, false, true));
    AutoPtr<CDataMember> member(new CDataMember(node.GetName(),
        uniseq ? (AutoPtr<CDataType>(new CUniSequenceDataType(type))) : type));

    if ((occ == DTDElement::eZeroOrOne) ||
        (occ == DTDElement::eZeroOrMore)) {
        member->SetOptional();
    }
    member->SetNotag();
    member->SetNoPrefix();
    if (!uniseq) {
        member->SetSimpleType();
    }
    container->AddMember(member);

    return container.release();
}

void DTDParser::AddAttributes(
    AutoPtr<CDataMemberContainerType>& container, const DTDElement& node)
{
    if (node.HasAttributes()) {
        AutoPtr<CDataMember> member(
            new CDataMember("Attlist", AttribBlock(node)));
        member->SetNoPrefix();
        member->SetAttlist();
        container->AddMember(member);
    }
}

CDataType* DTDParser::AttribBlock(const DTDElement& node)
{
    AutoPtr<CDataMemberContainerType> container(new CDataSequenceType());
    const list<DTDAttribute>& att = node.GetAttributes();
    for (list<DTDAttribute>::const_iterator i= att.begin();
        i != att.end(); ++i) {
        AutoPtr<CDataType> type(x_AttribType(*i));
        AutoPtr<CDataMember> member(new CDataMember(i->GetName(), type));
        string defValue( i->GetValue());
        if (!defValue.empty()) {
            member->SetDefault(new CIdDataValue(defValue));
        }
        if (i->GetValueType() == DTDAttribute::eImplied) {
            member->SetOptional();
        }
        member->SetNoPrefix();
        container->AddMember(member);
    }
    return container.release();
}



CDataType* DTDParser::x_AttribType(const DTDAttribute& att)
{
    CDataType* type=0;
    switch (att.GetType()) {
    case DTDAttribute::eUnknown:
        ParseError("Unknown attribute", "attribute");
        break;
    case DTDAttribute::eId:
    case DTDAttribute::eIdRef:
    case DTDAttribute::eIdRefs:
    case DTDAttribute::eNmtoken:
    case DTDAttribute::eNmtokens:
    case DTDAttribute::eEntity:
    case DTDAttribute::eEntities:
    case DTDAttribute::eNotation:
    case DTDAttribute::eString:
        type = new CStringDataType();
        break;
    case DTDAttribute::eEnum:
        type = EnumeratedBlock(att, new CEnumDataType());
        break;
    }
    return type;
}

CDataType* DTDParser::EnumeratedBlock(const DTDAttribute& att,
    CEnumDataType* enumType)
{
    int v=1;
    const list<string>& attEnums = att.GetEnumValues();
    for (list<string>::const_iterator i = attEnums.begin();
        i != attEnums.end(); ++i, ++v)
    {
        enumType->AddValue( *i, v);
    }
    return enumType;
}

/////////////////////////////////////////////////////////////////////////////
// debug printing

#if defined(NCBI_DTDPARSER_TRACE)
void DTDParser::PrintDocumentTree(void)
{
    PrintEntities();

    cout << " === Elements ===" << endl;
    map<string,DTDElement>::iterator i;
    for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {
        DTDElement& node = i->second;
        DTDElement::EType type = node.GetType();
        if (((type == DTDElement::eSequence) ||
            (type == DTDElement::eChoice) ||
            node.HasAttributes()) && !node.IsEmbedded()) {
            PrintDocumentNode(i->first,i->second);
        }
    }
    bool started = false;
    for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {
        DTDElement& node = i->second;
        if (node.IsEmbedded()) {
            if (!started) {
                cout << " === Embedded elements ===" << endl;
                started = true;
            }
            PrintDocumentNode(i->first,i->second);
        }
    }
    started = false;
    for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {
        DTDElement& node = i->second;
        DTDElement::EType type = node.GetType();
        if (((type != DTDElement::eSequence) &&
            (type != DTDElement::eChoice)) && !node.IsReferenced()) {
            if (!started) {
                cout << " === UNREFERENCED elements ===" << endl;
                started = true;
            }
            PrintDocumentNode(i->first,i->second);
        }
    }
    cout << endl;
}

void DTDParser::PrintEntities(void)
{
    if (!m_MapEntity.empty()) {
        cout << " === Entities ===" << endl;
        map<string,DTDEntity>::iterator i;
        for (i = m_MapEntity.begin(); i != m_MapEntity.end(); ++i) {
            cout << i->second.GetName() << " = \"" << i->second.GetData() << "\"" << endl;
        }
        cout << endl;
    }
}

void DTDParser::PrintDocumentNode(const string& name, const DTDElement& node)
{
    cout << name << ": ";
    switch (node.GetType()) {
    default:
    case DTDElement::eUnknown:  cout << "unknown"; break;
    case DTDElement::eString:   cout << "string";  break;
    case DTDElement::eAny:      cout << "any";     break;
    case DTDElement::eEmpty:    cout << "empty";   break;
    case DTDElement::eSequence: cout << "sequence";break;
    case DTDElement::eChoice:   cout << "choice";  break;
    }
    switch (node.GetOccurrence()) {
    default:
    case DTDElement::eOne:         cout << "(1)";    break;
    case DTDElement::eOneOrMore:   cout << "(1..*)"; break;
    case DTDElement::eZeroOrMore:  cout << "(0..*)"; break;
    case DTDElement::eZeroOrOne:   cout << "(0..1)"; break;
    }
    cout << endl;
    if (node.HasAttributes()) {
        PrintNodeAttributes(node);
    }
    const list<string>& refs = node.GetContent();
    if (!refs.empty()) {
        cout << "        === Contents ===" << endl;
        for (list<string>::const_iterator ir= refs.begin();
            ir != refs.end(); ++ir) {
            cout << "        " << *ir;
            switch (node.GetOccurrence(*ir)) {
            default:
            case DTDElement::eOne:         cout << "(1)"; break;
            case DTDElement::eOneOrMore:   cout << "(1..*)"; break;
            case DTDElement::eZeroOrMore:  cout << "(0..*)"; break;
            case DTDElement::eZeroOrOne:   cout << "(0..1)"; break;
            }
            cout << endl;
        }
    }
    cout << endl;
}

void DTDParser::PrintNodeAttributes(const DTDElement& node)
{
    const list<DTDAttribute>& att = node.GetAttributes();
    cout << "        === Attributes ===" << endl;
    for (list<DTDAttribute>::const_iterator i= att.begin();
        i != att.end(); ++i) {
        cout << "        ";
        cout << i->GetName();
        cout << ": ";
        switch (i->GetType()) {
        case DTDAttribute::eUnknown:  cout << "eUnknown"; break;
        case DTDAttribute::eString:   cout << "eString"; break;
        case DTDAttribute::eEnum:     cout << "eEnum"; break;
        case DTDAttribute::eId:       cout << "eId"; break;
        case DTDAttribute::eIdRef:    cout << "eIdRef"; break;
        case DTDAttribute::eIdRefs:   cout << "eIdRefs"; break;
        case DTDAttribute::eNmtoken:  cout << "eNmtoken"; break;
        case DTDAttribute::eNmtokens: cout << "eNmtokens"; break;
        case DTDAttribute::eEntity:   cout << "eEntity"; break;
        case DTDAttribute::eEntities: cout << "eEntities"; break;
        case DTDAttribute::eNotation: cout << "eNotation"; break;
        }
        {
            const list<string>& enumV = i->GetEnumValues();
            if (!enumV.empty()) {
                cout << " (";
                for (list<string>::const_iterator ie= enumV.begin();
                    ie != enumV.end(); ++ie) {
                    if (ie != enumV.begin()) {
                        cout << ",";
                    }
                    cout << *ie;
                }
                cout << ")";
            }
        }
        cout << ", ";
        switch (i->GetValueType()) {
        case DTDAttribute::eDefault:  cout << "eDefault"; break;
        case DTDAttribute::eRequired: cout << "eRequired"; break;
        case DTDAttribute::eImplied:  cout << "eImplied"; break;
        case DTDAttribute::eFixed:    cout << "eFixed"; break;
        }
        cout << ", ";
        cout << "\"" << i->GetValue() << "\"";
        cout << endl;
    }
}

#endif

END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.24  2005/02/09 14:37:39  gouriano
 * Implemented elements with mixed content
 *
 * Revision 1.23  2005/01/12 18:05:10  gouriano
 * Corrected generation of XML schema for sequence of choice types,
 * and simple types with default
 *
 * Revision 1.22  2005/01/06 20:30:38  gouriano
 * added support for compound identifier names
 *
 * Revision 1.21  2005/01/03 16:51:15  gouriano
 * Added parsing of conditional sections
 *
 * Revision 1.20  2004/08/17 14:10:41  gouriano
 * Corrected data tree generation for embedded elements
 *
 * Revision 1.19  2004/05/19 17:24:18  gouriano
 * Corrected generation of C++ code by DTD for containers
 *
 * Revision 1.18  2004/05/17 21:03:14  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.17  2004/04/27 18:38:53  gouriano
 * In FixEmbeddedNames() not to use the same "fixed" name twice
 *
 * Revision 1.16  2004/04/01 14:14:02  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.15  2004/01/12 16:51:01  gouriano
 * Improved diagnostics when parsing a DTD
 *
 * Revision 1.14  2003/08/13 15:45:54  gouriano
 * implemented generation of code, which uses AnyContent objects
 *
 * Revision 1.13  2003/06/24 20:55:42  gouriano
 * corrected code generation and serialization of non-empty unnamed containers (XML)
 *
 * Revision 1.12  2003/06/16 14:41:05  gouriano
 * added possibility to convert DTD to XML schema
 *
 * Revision 1.11  2003/03/10 18:55:18  gouriano
 * use new structured exceptions (based on CException)
 *
 * Revision 1.10  2003/02/10 17:56:15  gouriano
 * make it possible to disable scope prefixes when reading and writing objects generated from ASN specification in XML format, or when converting an ASN spec into DTD.
 *
 * Revision 1.9  2003/01/21 19:34:17  gouriano
 * corrected parsing of entities
 *
 * Revision 1.8  2003/01/14 19:02:09  gouriano
 * added parsing of entities as attribute contents
 *
 * Revision 1.7  2002/12/17 16:24:43  gouriano
 * replaced _ASSERTs by throwing an exception
 *
 * Revision 1.6  2002/11/26 22:00:29  gouriano
 * added unnamed lists of sequences (or choices) as container elements
 *
 * Revision 1.5  2002/11/19 19:48:28  gouriano
 * added support of XML attributes of choice variants
 *
 * Revision 1.4  2002/11/14 21:05:27  gouriano
 * added support of XML attribute lists
 *
 * Revision 1.3  2002/10/21 16:11:13  gouriano
 * added parsing of external entities
 *
 * Revision 1.2  2002/10/18 14:38:56  gouriano
 * added parsing of internal parsed entities
 *
 * Revision 1.1  2002/10/15 13:54:01  gouriano
 * DTD lexer and parser, first version
 *
 *
 * ==========================================================================
 */
