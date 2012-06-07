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
#include "exceptions.hpp"
#include "dtdparser.hpp"
#include "tokens.hpp"
#include "module.hpp"
#include "moduleset.hpp"
#include "type.hpp"
#include "statictype.hpp"
#include "enumtype.hpp"
#include "reftype.hpp"
#include "unitype.hpp"
#include "blocktype.hpp"
#include "choicetype.hpp"
#include "value.hpp"
#include <serial/error_codes.hpp>
#include <algorithm>
#include <corelib/ncbifile.hpp>


#define NCBI_USE_ERRCODE_X   Serial_Parsers

BEGIN_NCBI_SCOPE

const string& DTDParser::s_SpecialName = "#PCDATA";

/////////////////////////////////////////////////////////////////////////////
// DTDParser

DTDParser::DTDParser(DTDLexer& lexer)
    : AbstractParser(lexer)
{
    m_StackLexer.push(&lexer);
    m_SrcType = eDTD;
    m_Comments = 0;
    m_ExpectLastComment = false;
    lexer.SetParser(this);
}

DTDParser::~DTDParser(void)
{
}


AutoPtr<CFileModules> DTDParser::Modules(const string& fileName)
{
    AutoPtr<CFileModules> modules(new CFileModules(fileName));
    Lexer().BeginFile();

    while( GetNextToken() != T_EOF ) {
        CDirEntry entry(fileName);
        m_StackPath.push(entry.GetDir());
        m_StackLexerName.push_back(fileName);
        Module( modules, NStr::Replace( entry.GetBase(), ".", "_"));
        m_StackLexerName.pop_back();
        m_StackPath.pop();
    }
    return modules;
}

void DTDParser::EndCommentBlock()
{
    if (m_Comments) {
        if (Lexer().HaveComments()) {
            CopyComments(*m_Comments);
        }
        if (m_ExpectLastComment) {
            m_Comments = 0;
            m_ExpectLastComment = false;
        }
    } else if (m_ExpectLastComment) {
        Lexer().FlushComments();
        m_ExpectLastComment = false;
    }
}

void DTDParser::Module(
    AutoPtr<CFileModules>& modules, const string& name)
{
    AutoPtr<CDataTypeModule> module(new CDataTypeModule(name));
    module->SetSourceLine(Lexer().CurrentLine());

    try {
        CopyComments(module->Comments());
        BeginDocumentTree();
        BuildDocumentTree(*module);
        EndCommentBlock();
    }
    catch (CException& e) {
        NCBI_RETHROW_SAME(e,"DTDParser::BuildDocumentTree: failed");
    }
    catch (exception& e) {
        ERR_POST_X(5, e.what());
        throw;
    }

#if defined(NCBI_DTDPARSER_TRACE)
    PrintDocumentTree();
#endif

    BuildDataTree( modules, module);
}

void DTDParser::BuildDataTree(
    AutoPtr<CFileModules>& modules, AutoPtr<CDataTypeModule>& module)
{
    GenerateDataTree(*module, "*");
    modules->AddModule(module);
}

void DTDParser::BeginDocumentTree(void)
{
}

void DTDParser::BuildDocumentTree(CDataTypeModule& /*module*/)
{
    bool conditional_ignore = false;
    int conditional_level = 0;
    for (;;) {
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
            ERR_POST_X(6, e.what());
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
                if (Lexer().TokenStarted()) {
                    Consume();
                    break;
                }
                return tok;
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
    node.SetSourceLine(Lexer().CurrentLine());
    m_Comments = &(node.Comments());
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
    m_ExpectLastComment = true;
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
        case K_EMPTY:
            node.SetType(DTDElement::eEmpty);
            ConsumeToken();
            GetNextToken();
            EndElementContent( node);
            return;
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
                AddElementContent(node, id_name, symbol);
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
    char separator /* =0 */)
{
    DTDElement::EType type = node.GetType();
    if (type != DTDElement::eUnknown &&
        type != DTDElement::eSequence &&
        type != DTDElement::eChoice &&
        type != DTDElement::eSet &&
        type < DTDElement::eWsdlService) {
        ParseError("Unexpected element contents", "");
    }
    const list<string>& content = node.GetContent();
    if (id_name == s_SpecialName) {
        if (find(content.begin(), content.end(), id_name) != content.end()) {
            // already there
            return;
        }
    } else {
        list<string>::const_iterator i;
        DTDElement& candidate = m_MapElement[ id_name ];
        for (i = content.begin(); i != content.end(); ++i) {
            DTDElement& elem = m_MapElement[ *i ];
            if (!elem.GetName().empty() &&
                candidate.GetName() == elem.GetName()) {
/*
                if (candidate.GetType() != elem.GetType() ||
                    candidate.GetTypeName() != elem.GetTypeName()) {
                    ParseError("Unexpected element type", "");
                }
*/
                DTDElement::EOccurrence occ_candidate = node.GetOccurrence(id_name);
                DTDElement::EOccurrence occ_elem      = node.GetOccurrence(*i);
                if (occ_candidate == DTDElement::eZero) {
                    node.RemoveContent(*i);
                    return;
                }
                if (occ_candidate != occ_elem) {
                    node.SetOccurrence(*i, occ_candidate);
                }
                occ_candidate = candidate.GetOccurrence();
                occ_elem      = elem.GetOccurrence();
                if (occ_candidate != occ_elem) {
                    elem.SetOccurrence( occ_candidate );
                }
                if (!candidate.GetComments().Empty()) {
                    elem.Comments() = candidate.GetComments();
                }
                return;
            }
        }
    }
    
    if (id_name == s_SpecialName) {
        if (type == DTDElement::eUnknown && separator == ')') {
            node.SetType(DTDElement::eString);
        } else {
            node.AddContent(id_name);
            if (separator == ',' || separator == '|') {
                node.SetType(separator == ',' ?
                    DTDElement::eSequence : DTDElement::eChoice);
            }
        }
        id_name.erase();
        return;
    }
    node.AddContent(id_name);
    if (separator == ',' || separator == '|') {
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

string DTDParser::CreateEmbeddedName(const DTDElement& node, int depth) const
{
#ifdef _DEBUG
    string old_var = node.CreateEmbeddedName(depth);
#endif
    string new_var;

    if (node.GetType() == DTDElement::eAny) {
        new_var = "AnyContent";
    } else {
        string tmp, refname;
        list<string>::const_iterator i;
        map<string,DTDElement>::const_iterator r;
        const list<string>& refs = node.GetContent();

        for ( i = refs.begin(); i != refs.end(); ++i) {
            r = m_MapElement.find(*i);
            if (r != m_MapElement.end()) {
                refname = r->second.GetName();
            }
            if (refname.empty()) {
                refname = *i;
            }
            if (!refname.empty()) {
                string::size_type name = refname.find(':');
                name = (name != string::npos && (name+1) < refname.size()) ? (name+1) : 0;
                new_var += toupper((unsigned char) refname[name]);;
// try to avoid very long names
                if (new_var.size() > 8) {
                    break;
                }
            }
        }
        if (depth > 1) {
            new_var += '_';
            new_var += NStr::IntToString(depth);
        }
    }
    return new_var;
}

void DTDParser::FixEmbeddedNames(DTDElement& node)
{
    const list<string>& refs = node.GetContent();
    list<string> fixed;
    for (list<string>::const_iterator i= refs.begin(); i != refs.end(); ++i) {
        DTDElement& refNode = m_MapElement[*i];
        if (refNode.IsEmbedded() && !refNode.IsNamed() && refNode.GetName() == *i) {
            for ( int depth=1; depth<100; ++depth) {

                string testName = CreateEmbeddedName(refNode,depth);
                bool allowed =
                    find(refs.begin(),refs.end(),testName) == refs.end() &&
                    find(fixed.begin(),fixed.end(),testName) == fixed.end();
                if (allowed) {
                    const list<string>& refrefs = refNode.GetContent();
                    list<string>::const_iterator r= refrefs.begin();
                    for (; allowed && r != refrefs.end(); ++r) {
                        const string& t = m_MapElement[*r].GetName();
                        allowed = t != testName;
                    }
                }
                if (allowed) {
                    fixed.push_back(testName);
                    refNode.SetName(testName);
                    break;
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
    if (tok == T_IDENTIFIER) {
// skip
        ConsumeToken();
        tok = GetNextToken();
        if (tok!=T_STRING) {
            ParseError("string");
        }
        ConsumeToken();
        GetNextToken();
        ConsumeSymbol('>');
        return;
    }
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

DTDEntity* DTDParser::PushEntityLexer(const string& name)
{
    map<string,DTDEntity>::iterator i = m_MapEntity.find(name);
    if (i == m_MapEntity.end()) {
        string msg("Undefined entity: ");
        msg += name;
        ParseError(msg.c_str(),"entity definition");
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
    AbstractLexer *lexer = CreateEntityLexer(*in,lexer_name);
    lexer->SetParser(this);
    Lexer().FlushCommentsTo(*lexer);
    m_StackLexer.push(lexer);
    SetLexer(lexer);
    if (m_MapEntity[name].IsExternal()) {
        Lexer().BeginFile();
    }
    return &(m_MapEntity[name]);
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

AbstractLexer* DTDParser::CreateEntityLexer(
    CNcbiIstream& in, const string& name, bool autoDelete /*=true*/)
{
    return new DTDEntityLexer(in,name);
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
    ParseAttributesContent(m_MapElement[ name]);
}

void DTDParser::ParseAttributesContent(DTDElement& node)
{
    m_Comments = &(node.AttribComments());
    m_ExpectLastComment = true;
    string id_name;
    while (GetNextToken()==T_IDENTIFIER) {
        // attribute name
        id_name = GetNextTokenText();
        ConsumeToken();
        ConsumeAttributeContent(node, id_name);
    }
    // attlist description is ended
    GetNextToken();
    ConsumeSymbol('>');
    m_ExpectLastComment = true;
}

void DTDParser::ConsumeAttributeContent(DTDElement& node,
                                        const string& id_name)
{
    bool done=false;
    DTDAttribute a;
    a.SetName(id_name);
    node.AddAttribute(a);
    DTDAttribute& attrib = node.GetNonconstAttributes().back();
    attrib.SetSourceLine(Lexer().CurrentLine());
    m_Comments = &(attrib.Comments());
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
            m_ExpectLastComment = true;
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
            m_ExpectLastComment = true;
            break;
        case K_REQUIRED:
            attrib.SetValueType(DTDAttribute::eRequired);
            m_ExpectLastComment = true;
            break;
        case K_IMPLIED:
            attrib.SetValueType(DTDAttribute::eImplied);
            m_ExpectLastComment = true;
            break;
        case K_FIXED:
            attrib.SetValueType(DTDAttribute::eFixed);
            m_ExpectLastComment = true;
            break;
        }
        if (!done) {
            ConsumeToken();
        }
    }
}

void DTDParser::ParseEnumeratedList(DTDAttribute& attrib)
{
    for (;;) {
        switch(GetNextToken()) {
        default:
            ParseError("Unknown token", "token");
            break;
        case T_IDENTIFIER:
        case T_NMTOKEN:
            attrib.AddEnumValue(GetNextTokenText(),Lexer().CurrentLine());
            ConsumeToken();
            break;
        case T_SYMBOL:
            // may be either '|' or ')'
            if (NextToken().GetSymbol() == ')') {
                return;
            }
            if (NextToken().GetSymbol() != '|') {
                ParseError("Unknown token", "|");
            }
            ConsumeToken();
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// model generation

void DTDParser::GenerateDataTree(CDataTypeModule& module, const string& name_space)
{
    m_GeneratedTypes.clear();
    m_ElementEmbTypes.clear();
    map<string,DTDElement>::iterator i;
    for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {

        DTDElement::EType type = i->second.GetType();
        if (i->second.GetName().empty() &&
            i->first != s_SpecialName &&
            type != DTDElement::eAny) {
            ParseError(i->first.c_str(),"definition");
        }

        bool generate = type != DTDElement::eUnknown &&
                        type != DTDElement::eUnknownGroup &&
                        type != DTDElement::eWsdlUnsupportedEndpoint;
        if (m_SrcType == eDTD) {
            generate = ( type == DTDElement::eSequence ||
                         type == DTDElement::eChoice ||
                         type == DTDElement::eSet ||
                         i->second.HasAttributes());
        }
        if (generate && !i->second.IsEmbedded())
        {
            if (name_space != "*" &&
                i->second.GetNamespaceName() != name_space) {
                continue;
            }
            string qname = i->second.GetName() + i->second.GetNamespaceName();
            if (m_GeneratedTypes.find(qname) == m_GeneratedTypes.end()) {
                m_GeneratedTypes.insert(qname);
                m_ElementEmbTypes.push_back(qname);
                ModuleType(module, i->second);
                m_ElementEmbTypes.clear();
            }
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
    if (m_SrcType != eDTD && !fromInside) {
        type->Comments() = node.GetComments();
    }
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
            if (refNode.IsEmbedded() && !refNode.IsNamed() &&
                (node.GetOccurrence(refName) == DTDElement::eOne) &&
                refNode.GetType() != DTDElement::eAny) {
                DTDElement::EOccurrence ref_occ = refNode.GetOccurrence();
                DTDElement::EOccurrence new_occ =
                    (fromInside || ref_occ == DTDElement::eOne) ? occ : ref_occ;
                type = x_Type(refNode, new_occ, fromInside);
                if (node.IsEmbedded()) {
                    DTDElement& emb = const_cast<DTDElement&>(node);
                    emb.SetName(refNode.GetName());
                }
                type->SetNamespaceName( node.GetNamespaceName());
                type->SetNsQualified(node.IsQualified());
                return type;
            }
        }
    }

    bool uniseq = (occ == DTDElement::eOneOrMore ||
                   occ == DTDElement::eZeroOrMore);
    bool cont = (node.GetType() == DTDElement::eSequence ||
                 node.GetType() == DTDElement::eChoice ||
                 node.GetType() == DTDElement::eSet);
    bool attrib = !ignoreAttrib && node.HasAttributes();
    bool ref = fromInside && !node.IsEmbedded();
    bool ref_to_parent = false;

    string embtype;
    if (fromInside && node.IsEmbedded() && !node.GetTypeName().empty()) {
        embtype = node.GetName() + node.GetTypeName() + "###";
        ref_to_parent = ref =
            find(m_ElementEmbTypes.begin(), m_ElementEmbTypes.end(), embtype) != m_ElementEmbTypes.end();
        m_ElementEmbTypes.push_back(embtype);
    }

    bool keep_global;
    keep_global = (cont && uniseq && (attrib ||
        (node.IsEmbedded() && !node.IsNamed() && fromInside))) ||
        (!cont && attrib);
    if (m_SrcType != eDTD) {
        keep_global = keep_global || ref;
    }

    if (keep_global) {
        if (ref) {
            type = new CReferenceDataType(node.GetName(), ref_to_parent);
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
        case DTDElement::eSet:
            if (ref) {
                type = new CReferenceDataType(node.GetName());
            } else {
                type = TypesBlock(new CDataSetType(),node,ignoreAttrib);
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

        case DTDElement::eBoolean:
            type = new CBoolDataType();
            break;
        case DTDElement::eInteger:
            type = new CIntDataType();
            break;
        case DTDElement::eBigInt:
            type = new CBigIntDataType();
            break;
        case DTDElement::eDouble:
            type = new CRealDataType();
            break;
        case DTDElement::eOctetString:
            type = new COctetStringDataType();
            break;
        case DTDElement::eBase64Binary:
            type = new CBase64BinaryDataType();
            break;

        default:
            if (node.GetType() >= DTDElement::eWsdlService) {
                CWsdlDataType* w = new CWsdlDataType();
                CWsdlDataType::EType wt = CWsdlDataType::eWsdlService;
                switch (node.GetType()) {
                case DTDElement::eWsdlService:   wt = CWsdlDataType::eWsdlService;   break;
                case DTDElement::eWsdlEndpoint:
                    wt = CWsdlDataType::eWsdlEndpoint;
                    break;
                case DTDElement::eWsdlOperation:    wt = CWsdlDataType::eWsdlOperation;   break;
                case DTDElement::eWsdlHeaderInput:  wt = CWsdlDataType::eWsdlHeaderInput; break;
                case DTDElement::eWsdlInput:        wt = CWsdlDataType::eWsdlInput;       break;
                case DTDElement::eWsdlHeaderOutput: wt = CWsdlDataType::eWsdlHeaderOutput; break;
                case DTDElement::eWsdlOutput:       wt = CWsdlDataType::eWsdlOutput;      break;
                case DTDElement::eWsdlMessage:      wt = CWsdlDataType::eWsdlMessage;     break;
                default:
                   ParseError("Unknown WSDL element type", "element");
                   break;
                }
                w->SetWsdlType(wt);
                type = TypesBlock(w,node,ignoreAttrib);
            } else {
                ParseError("Unknown element", "element");
                type = 0;
            }
            break;
        }
    }
    type->SetSourceLine(node.GetSourceLine());
    type->SetNamespaceName( node.GetNamespaceName());
    type->SetNsQualified( node.IsQualified());
    if (uniseq) {
        CUniSequenceDataType* uniType = new CUniSequenceDataType(type);
        uniType->SetSourceLine(type->GetSourceLine());
        uniType->SetNonEmpty( occ == DTDElement::eOneOrMore);
        uniType->SetNoPrefix(true);
        type = uniType;
    }
    else if (!fromInside && cont && occ == DTDElement::eZeroOrOne) {
        string refname(node.GetName());
        refname.insert(0,"E");
        AutoPtr<CDataMemberContainerType> container(new CDataSequenceType());
        AutoPtr<CDataMember> member(new CDataMember(refname, type));
        container->SetSourceLine(type->GetSourceLine());
        member->SetOptional();
        member->SetNotag();
        member->SetNoPrefix();
        container->AddMember(member);
        type = container.release();
    }

    if (!embtype.empty()) {
        m_ElementEmbTypes.pop_back();
    }
    return type;
}

AutoPtr<CDataValue> DTDParser::Value(const DTDElement& node)
{
    AutoPtr<CDataValue> value(x_Value(node));
    value->SetSourceLine(node.GetSourceLine());
    return value;
}

AutoPtr<CDataValue> DTDParser::x_Value(const DTDElement& node)
{
    switch (node.GetType()) {
    default:
        break;;
    case DTDElement::eString:
        return AutoPtr<CDataValue>(new CStringDataValue(node.GetDefault()));
    case DTDElement::eOctetString:
        return AutoPtr<CDataValue>(new CBitStringDataValue(node.GetDefault()));
    case DTDElement::eEmpty:
        return AutoPtr<CDataValue>(new CNullDataValue());
    case DTDElement::eBoolean:
        {
            bool b;
            if (node.GetDefault() == "0") {
                b = false;
            } else if (node.GetDefault() == "1") {
                b = true;
            } else {
                b = NStr::StringToBool(node.GetDefault());
            }
            return AutoPtr<CDataValue>(new CBoolDataValue(b));
        }
    case DTDElement::eInteger:
    case DTDElement::eBigInt:
        return AutoPtr<CDataValue>(new CIntDataValue(
            NStr::StringToInt8(node.GetDefault())));
    case DTDElement::eDouble:
        return AutoPtr<CDataValue>(new CDoubleDataValue(
            NStr::StringToDouble(node.GetDefault(), NStr::fDecimalPosix)));
    }
    ParseError("value");
    return AutoPtr<CDataValue>(0);
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
            if (refNode.GetType() == DTDElement::eAny) {
                refNode.SetName("_Any");
            } else {
                ERR_POST_X(7, Warning << "Element with no name: " << *i);
                refNode.SetName(*i);
            }
        }
        DTDElement::EOccurrence occ = node.GetOccurrence(*i);
        if (refNode.IsEmbedded()) {
            occ = refNode.GetOccurrence();
        }
        AutoPtr<CDataType> type(Type(refNode, occ, true));
        if (refNode.IsEmbedded() && refNode.IsNamed()) {
            bool optional = false, uniseq = false, uniseq2 = false, refseq = false;
            uniseq = (occ == DTDElement::eOneOrMore || occ == DTDElement::eZeroOrMore);
            optional = (occ == DTDElement::eZeroOrOne || occ == DTDElement::eZeroOrMore);
            refseq = refNode.GetType() == DTDElement::eSequence ||
                     refNode.GetType() == DTDElement::eChoice ||
                     refNode.GetType() == DTDElement::eSet;
            occ = node.GetOccurrence(*i);
            uniseq2 = (occ == DTDElement::eOneOrMore || occ == DTDElement::eZeroOrMore);

            if (uniseq || (optional && refseq)) {

                string refname(refNode.GetName());
                if (uniseq2 || (optional && refseq)) {
                    refname.insert(0,"E");
                }
                AutoPtr<CDataMemberContainerType> container(new CDataSequenceType());
                AutoPtr<CDataMember> member(new CDataMember(refname, type));
                container->SetSourceLine(type->GetSourceLine());
                if (optional) {
                    member->SetOptional();
                }
                member->SetNotag();
                member->SetNoPrefix();
                container->AddMember(member);
                type.reset(container.release());
            }
            if (uniseq2) {

                CUniSequenceDataType* uniType = new CUniSequenceDataType(type);
                uniType->SetSourceLine( type->GetSourceLine());
                uniType->SetNonEmpty( occ == DTDElement::eOneOrMore);
                type.reset(uniType);

            }
        }
        AutoPtr<CDataMember> member(new CDataMember(refNode.GetName(), type));
        if ((occ == DTDElement::eZeroOrOne) ||
            (occ == DTDElement::eZeroOrMore)) {
            member->SetOptional();
        }
        if (refNode.IsEmbedded() && !refNode.IsNamed()) {
            member->SetNotag();
        }
        if (!refNode.GetDefault().empty()) {
            member->SetDefault(Value(refNode));
        }
        member->SetNoPrefix();
        if (m_SrcType == eDTD || refNode.IsEmbedded()) {
            member->Comments() = refNode.GetComments();
        }
        container->AddMember(member);
    }
    if (m_SrcType == eDTD || node.IsEmbedded()) {
        container->Comments() = node.GetComments();
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
    if (uniseq) {
        member->GetType()->SetSourceLine( type->GetSourceLine() );
    }

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
    if (m_SrcType == eDTD || node.IsEmbedded()) {
        container->Comments() = node.GetComments();
    }
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
        member->Comments() = node.GetAttribComments();
        container->AddMember(member);
    }
}

CDataType* DTDParser::AttribBlock(const DTDElement& node)
{
    AutoPtr<CDataMemberContainerType> container(new CDataSetType());
    const list<DTDAttribute>& att = node.GetAttributes();
    for (list<DTDAttribute>::const_iterator i= att.begin();
        i != att.end(); ++i) {
        AutoPtr<CDataType> type(x_AttribType(*i));
        AutoPtr<CDataMember> member(new CDataMember(i->GetName(), type));
        string defValue( i->GetValue());
        if (!defValue.empty()) {
            member->SetDefault(x_AttribValue(*i,defValue));
        }
        if (i->GetValueType() == DTDAttribute::eImplied) {
            member->SetOptional();
        }
        member->SetNoPrefix();
        member->Comments() = i->GetComments();
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
    case DTDAttribute::eUnknownGroup:
        ParseError("Unknown attribute", "attribute");
        break;
    default:
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
    case DTDAttribute::eIntEnum:
        type = EnumeratedBlock(att, new CIntEnumDataType());
        break;

    case DTDAttribute::eBoolean:
        type = new CBoolDataType();
        break;
    case DTDAttribute::eInteger:
        type = new CIntDataType();
        break;
    case DTDAttribute::eBigInt:
        type = new CBigIntDataType();
        break;
    case DTDAttribute::eDouble:
        type = new CRealDataType();
        break;
    case DTDAttribute::eBase64Binary:
        type = new CBase64BinaryDataType();
        break;
    }
    type->SetSourceLine(att.GetSourceLine());
    type->SetNsQualified(att.IsQualified());
    return type;
}

CDataValue* DTDParser::x_AttribValue(const DTDAttribute& att,
                                     const string& defvalue)
{
    CDataValue* value=0;
    switch (att.GetType()) {
    case DTDAttribute::eUnknown:
        ParseError("Unknown attribute", "attribute");
        break;
    case DTDAttribute::eUnknownGroup:
        ParseError("Unknown attribute", "attribute");
        break;
    default:
    case DTDAttribute::eId:
    case DTDAttribute::eIdRef:
    case DTDAttribute::eIdRefs:
    case DTDAttribute::eNmtoken:
    case DTDAttribute::eNmtokens:
    case DTDAttribute::eEntity:
    case DTDAttribute::eEntities:
    case DTDAttribute::eNotation:
    case DTDAttribute::eString:
        value = new CStringDataValue(defvalue);
        break;
    case DTDAttribute::eEnum:
        value = new CIdDataValue(defvalue);
        break;

    case DTDAttribute::eBoolean:
        value = new CBoolDataValue(NStr::StringToBool(defvalue));
        break;
    case DTDAttribute::eIntEnum:
        value = new CIntDataValue(att.GetEnumValueId(defvalue));
        break;
    case DTDAttribute::eInteger:
    case DTDAttribute::eBigInt:
        value = new CIntDataValue(NStr::StringToInt8(defvalue));
        break;
    case DTDAttribute::eDouble:
        value = new CDoubleDataValue(
            NStr::StringToDouble(defvalue, NStr::fDecimalPosix));
        break;
    }
    return value;
}


CDataType* DTDParser::EnumeratedBlock(const DTDAttribute& att,
    CEnumDataType* enumType)
{
    int v=1;
    const list<string>& attEnums = att.GetEnumValues();
    list<string>::const_iterator i;
    for (i = attEnums.begin(); i != attEnums.end(); ++i, ++v) {
        if (enumType->IsInteger()) {
            v = att.GetEnumValueId(*i);
        }
        enumType->AddValue( *i, v).SetSourceLine(
            att.GetEnumValueSourceLine(*i));
    }
    return enumType;
}

void DTDParser::SetCommentsIfEmpty(CComments* comments)
{
    if (comments->Empty()) {
        m_Comments = comments;
    } else {
        m_Comments = 0;
    }
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
        if ((type == DTDElement::eSequence ||
             type == DTDElement::eChoice ||
             type == DTDElement::eSet ||
             type >= DTDElement::eWsdlService ||
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
        if ((type != DTDElement::eSequence &&
             type != DTDElement::eChoice &&
             type != DTDElement::eSet &&
            !node.HasAttributes()) && node.IsReferenced()) {
            if (!started) {
                cout << " === REFERENCED simpletype elements ===" << endl;
                started = true;
            }
            PrintDocumentNode(i->first,i->second);
        }
    }
    started = false;
    for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {
        DTDElement& node = i->second;
        DTDElement::EType type = node.GetType();
        if ((type != DTDElement::eSequence &&
             type != DTDElement::eChoice &&
             type != DTDElement::eSet &&
             type < DTDElement::eWsdlService) &&
             !node.IsReferenced()) {
            if (!started) {
                cout << " === UNREFERENCED simpletype elements ===" << endl;
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
            cout << i->first << " = \"" << i->second.GetData() << "\"" << endl << endl;
        }
    }
}

void DTDParser::PrintDocumentNode(const string& name, const DTDElement& node)
{
    cout << name << ": ";
    switch (node.GetType()) {
    default:
    case DTDElement::eUnknown:       cout << "UNKNOWN"; break;
    case DTDElement::eUnknownGroup:  cout << "UNKNOWN"; break;

    case DTDElement::eString:   cout << "string";  break;
    case DTDElement::eAny:      cout << "any";     break;
    case DTDElement::eEmpty:    cout << "empty";   break;
    case DTDElement::eSequence: cout << "sequence";break;
    case DTDElement::eChoice:   cout << "choice";  break;
    case DTDElement::eSet:      cout << "set";     break;

    case DTDElement::eBoolean:      cout << "boolean";   break;
    case DTDElement::eInteger:      cout << "integer";   break;
    case DTDElement::eBigInt:       cout << "BigInt";    break;
    case DTDElement::eDouble:       cout << "double";    break;
    case DTDElement::eOctetString:  cout << "OctetString";  break;
    case DTDElement::eBase64Binary: cout << "Base64Binary";  break;

    case DTDElement::eWsdlService:   cout << "WsdlService";  break;
    case DTDElement::eWsdlEndpoint:  cout << "WsdlEndpoint";  break;
    case DTDElement::eWsdlUnsupportedEndpoint:  cout << "WsdlUnsupportedEndpoint";  break;
    case DTDElement::eWsdlOperation: cout << "WsdlOperation"; break;
    case DTDElement::eWsdlHeaderInput:  cout << "WsdlHeaderInput";    break;
    case DTDElement::eWsdlInput:     cout << "WsdlInput";    break;
    case DTDElement::eWsdlHeaderOutput:  cout << "WsdlHeaderOutput";    break;
    case DTDElement::eWsdlOutput:    cout << "WsdlOutput";   break;
    case DTDElement::eWsdlMessage:   cout << "WsdlMessage";  break;
    }
    switch (node.GetOccurrence()) {
    default:
    case DTDElement::eOne:         cout << "(1)";    break;
    case DTDElement::eOneOrMore:   cout << "(1..*)"; break;
    case DTDElement::eZeroOrMore:  cout << "(0..*)"; break;
    case DTDElement::eZeroOrOne:   cout << "(0..1)"; break;
    }
    if (!node.GetDefault().empty()) {
        cout << ", default=";
        cout << "\"" << node.GetDefault() << "\"";
    }
    if (!node.GetNamespaceName().empty()) {
        cout << endl;
        cout << "Namespace: " << node.GetNamespaceName() << endl;
        cout << "form: " << (node.IsQualified() ? "qualified" : "unqualified") << endl;
    }
    cout << endl;
    if (!node.GetComments().Empty()) {
        cout << "        === Comments ===" << endl;
        node.GetComments().PrintDTD(cout, CComments::eDoNotWriteBlankLine);
    }
    if (!node.GetAttribComments().Empty()) {
        cout << "        === AttribComments ===" << endl;
        node.GetAttribComments().PrintDTD(cout, CComments::eDoNotWriteBlankLine);
    }
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
            if (m_MapElement.find(*ir) != m_MapElement.end() &&
                m_MapElement[*ir].IsEmbedded()) {
                cout << "        [" << m_MapElement[*ir].GetName() << "]";
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
        PrintAttribute( *i);
    }
}

void DTDParser::PrintAttribute(const DTDAttribute& attrib, bool indent/*=true*/)
{
    if (indent) {
        cout << "        ";
    }
    cout << attrib.GetName();
    cout << ": ";
    switch (attrib.GetType()) {
    case DTDAttribute::eUnknown:       cout << "UNKNOWN"; break;
    case DTDAttribute::eUnknownGroup:  cout << "UNKNOWN"; break;
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

    case DTDAttribute::eBoolean:     cout << "boolean";  break;
    case DTDAttribute::eInteger:     cout << "integer";  break;
    case DTDAttribute::eBigInt:      cout << "BigInt";   break;
    case DTDAttribute::eDouble:      cout << "double";   break;
    }
    {
        const list<string>& enumV = attrib.GetEnumValues();
        if (!enumV.empty()) {
            cout << " (";
            for (list<string>::const_iterator ie= enumV.begin();
                ie != enumV.end(); ++ie) {
                if (ie != enumV.begin()) {
                    cout << ",";
                }
                cout << *ie << "(" << attrib.GetEnumValueId(*ie) << ")";
            }
            cout << ")";
        }
    }
    cout << ", ";
    switch (attrib.GetValueType()) {
    case DTDAttribute::eDefault:  cout << "eDefault"; break;
    case DTDAttribute::eRequired: cout << "eRequired"; break;
    case DTDAttribute::eImplied:  cout << "eImplied"; break;
    case DTDAttribute::eFixed:    cout << "eFixed"; break;
    }
    cout << ", ";
    cout << "\"" << attrib.GetValue() << "\"";

    cout << endl;
    cout << "form:" << (attrib.IsQualified() ? "qualified" : "unqualified") << endl;
    if (!attrib.GetNamespaceName().empty()) {
        cout << "Namespace: " << attrib.GetNamespaceName() << endl;
    }
    cout << endl;
    if (!attrib.GetComments().Empty()) {
        cout << "        === Comments ===" << endl;
        attrib.GetComments().PrintDTD(cout, CComments::eDoNotWriteBlankLine);
    }
}

#endif

END_NCBI_SCOPE
