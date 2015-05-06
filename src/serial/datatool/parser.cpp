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
*   ASN.1 parser
*
*/

#include <ncbi_pch.hpp>
#include "parser.hpp"
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


#define NCBI_USE_ERRCODE_X   Serial_Parsers

BEGIN_NCBI_SCOPE

ASNParser::ASNParser(ASNLexer& lexer)
    : AbstractParser(lexer)
{
    m_StackLexer.push(&lexer);
}

AutoPtr<CFileModules> ASNParser::Modules(const string& fileName)
{

// BUG:  MIPSpro 7.3.1.2m SEGVs on throw/catch from function:
//   CDataTypeModule::Resolve(const string& typeName)
// This is apparently not needed for MIPSpro 7.3.1.1m. -- First
// problems appeared with MIPSpro 7.3.1.2m.
// This also helps if this artificial catch is put to function:
//   CDataTool::LoadDefinitions()
#if defined(NCBI_COMPILER_MIPSPRO)
#  if NCBI_COMPILER_VERSION == 730
    try {
        throw runtime_error("MIPS_EXC_BUG");
    }
    catch (...) {
    }
#  endif
#endif

    AutoPtr<CFileModules> modules(new CFileModules(fileName));
    while ( Next() != T_EOF ) {
        modules->AddModule(Module());
    }
    CopyComments(modules->LastComments());
    return modules;
}

AutoPtr<CDataTypeModule> ASNParser::Module(void)
{
    string moduleName = ModuleReference();
    AutoPtr<CDataTypeModule> module(new CDataTypeModule(moduleName));
    module->SetSourceLine(Lexer().CurrentLine());
    CopyComments(module->Comments());

    Consume(K_DEFINITIONS, "DEFINITIONS");

// correct default is EXPLICIT!
// we use AUTOMATIC because our specs mean automatic, but do not specify it
// NOTE: when changing this, also change default m_TagType in CDataType ctor

//    m_TagDefault = CAsnBinaryDefs::eExplicit;
    m_TagDefault = CAsnBinaryDefs::eAutomatic;


    for (;;) {
        const AbstractToken& token = NextToken();
        if (token.GetText() == "::=") {
            break;
        }
        TToken tok = token.GetToken();
        if (tok == K_EXPLICIT) {
            m_TagDefault = CAsnBinaryDefs::eExplicit;
        } else if (tok == K_IMPLICIT) {
            m_TagDefault = CAsnBinaryDefs::eImplicit;
        } else if (tok == K_AUTOMATIC) {
            m_TagDefault = CAsnBinaryDefs::eAutomatic;
        }
        else if (tok == T_EOF) {
            ParseError("::=");
        }
        Consume();
    }
    module->SetTagDefault( m_TagDefault);
    Consume();
    Consume(K_BEGIN, "BEGIN");

    Next();
    ModuleBody(*module);
    Consume(K_END, "END");

    CopyComments(module->LastComments());

    return module;
}

void ASNParser::Imports(CDataTypeModule& module)
{
    do {
        list<string> types;
        TypeList(types);
        Consume(K_FROM, "FROM");
        module.AddImports(ModuleReference(), types);
    } while ( !ConsumeIfSymbol(';') );
}

void ASNParser::Exports(CDataTypeModule& module)
{
    list<string> types;
    TypeList(types);
    module.AddExports(types);
    ConsumeSymbol(';');
}

void ASNParser::ModuleBody(CDataTypeModule& module)
{
    string name;
    while ( true ) {
        try {
            switch ( Next() ) {
            case K_EXPORTS:
                Consume();
                Exports(module);
                CopyComments(module.Comments());
                break;
            case K_IMPORTS:
                Consume();
                Imports(module);
                CopyComments(module.Comments());
                break;
            case T_TYPE_REFERENCE:
            case T_IDENTIFIER:
                name = TypeReference();
                Consume(T_DEFINE, "::=");
                ModuleType(module, name);
                break;
            case T_DEFINE:
                ERR_POST_X(1, "LINE " << Location() <<
                    " type name omitted");
                Consume();
                ModuleType(module, "unnamed type");
                break;
            case K_END:
                return;
            default:
                ERR_POST_X(2, "LINE " << Location() <<
                    " type definition expected");
                return;
            }
        }
        catch (CException& e) {
            NCBI_RETHROW_SAME(e,GetLocation() + " ASNParser::ModuleBody: failed");
        }
        catch (exception& e) {
            ERR_POST_X(3, e.what());
        }
    }
}

void ASNParser::ModuleType(CDataTypeModule& module, const string& name)
{
    AutoPtr<CDataType> type = Type();
    CopyLineComment(LastTokenLine(), type->Comments(),
                    eCombineNext);
    if (type->IsStdType()  ||  type->IsReference()) {
        type->SetIsAlias(true);
    }
    module.AddDefinition(name, type);
}

AutoPtr<CDataType> ASNParser::Type(void)
{
    int line = NextTokenLine();
    AutoPtr<CDataType> type(x_Type());
    if (!type->HasTag()) {
        type->SetTagType( m_TagDefault);
    }
    type->SetSourceLine(line);
    return type;
}

CDataType* ASNParser::x_Type(void)
{
    TToken tok = Next();
    switch ( tok ) {
    default:
        break;
    case K_BOOLEAN:
        Consume();
        return new CBoolDataType();
    case K_INTEGER:
        Consume();
        if ( CheckSymbol('{') )
            return EnumeratedBlock(new CIntEnumDataType());
        else
            return new CIntDataType();
    case K_BIGINT:
        Consume();
        if ( CheckSymbol('{') )
            return EnumeratedBlock(new CBigIntEnumDataType());
        else
            return new CBigIntDataType();
    case K_ENUMERATED:
        Consume();
        return EnumeratedBlock(new CEnumDataType());
    case K_REAL:
        Consume();
        return new CRealDataType();
    case K_BIT:
        Consume();
        Consume(K_STRING, "STRING");
        return new CBitStringDataType();
    case K_OCTET:
        Consume();
        Consume(K_STRING, "STRING");
        return new COctetStringDataType();
    case K_NULL:
        Consume();
        return new CNullDataType();
    case K_SEQUENCE:
        Consume();
        if ( ConsumeIf(K_OF) ) {
            AutoPtr<CDataType> elem = Type();
            CDataType* uni = new CUniSequenceDataType(elem);
            uni->Comments() = elem->Comments();
            elem->Comments() = CComments();
            return uni;
        }
        else
            return TypesBlock(new CDataSequenceType(), true);
    case K_SET:
        Consume();
        if ( ConsumeIf(K_OF) ) {
            AutoPtr<CDataType> elem = Type();
            CDataType* uni = new CUniSetDataType(elem);
            uni->Comments() = elem->Comments();
            elem->Comments() = CComments();
            return uni;
        }
        else
            return TypesBlock(new CDataSetType(), true);
    case K_CHOICE:
        Consume();
        return TypesBlock(new CChoiceDataType(), false);
    case K_VisibleString:
    case K_UTF8String:
        Consume();
        return new CStringDataType(
            tok == K_UTF8String ?
                CStringDataType::eStringTypeUTF8 :
                CStringDataType::eStringTypeVisible);
    case K_StringStore:
        Consume();
        return new CStringStoreDataType();
    case T_IDENTIFIER:
    case T_TYPE_REFERENCE:
        return new CReferenceDataType(TypeReference());

    case T_TAG_BEGIN:
        CAsnBinaryDefs::ETagClass tagclass = CAsnBinaryDefs::eContextSpecific;
        CAsnBinaryDefs::ETagType tagtype = m_TagDefault;
        int tag = 0;
        TToken ttk = tok;
        bool tagclosed = false;
        for (;;) {
            Consume();
            ttk = NextToken().GetToken();
            if (tagclosed && ttk != K_IMPLICIT && ttk != K_EXPLICIT) {
                break;
            }
            switch (ttk) {
            case K_APPLICATION:
                tagclass = CAsnBinaryDefs::eApplication;
                break;
            case K_PRIVATE:
                tagclass = CAsnBinaryDefs::ePrivate;
                break;
            case T_NUMBER:
                tag = NStr::StringToInt( NextToken().GetText());
                break;
            case T_TAG_END:
                tagclosed = true;
                break;
            case K_IMPLICIT:
                tagtype = CAsnBinaryDefs::eImplicit;
                break;
            case K_EXPLICIT:
                tagtype = CAsnBinaryDefs::eExplicit;
                break;
            default:
                break;
            case T_EOF:
                ParseError("Tag definition");
                break;
            }
        }
        CDataType* tagged = x_Type();
        tagged->SetTag(tag);
        tagged->SetTagClass(tagclass);
        tagged->SetTagType(tagtype);
        return tagged;
    }
    ParseError("type");
	return 0;
}

bool ASNParser::HaveMoreElements(void)
{
    if (Next() == T_EOF) {
        EndComponentsDefinition();
    }
    const AbstractToken& token = NextToken();
    if ( token.GetToken() == T_SYMBOL ) {
        switch ( token.GetSymbol() ) {
        default:
            break;
        case ',':
            Consume();
            return true;
        case '}':
            Consume();
            return false;
        case '(':
            Consume();
	    // skip constraints definition
            SkipTo(')');
            return HaveMoreElements();
        }
    }
    ParseError("',' or '}'");
    return false;
}

void ASNParser::SkipTo(char ch)
{
    for ( TToken tok = Next(); tok != T_EOF; tok = Next() ) {
        if (tok == T_SYMBOL && NextToken().GetSymbol() == ch) {
            Consume();
            return;
        }
        Consume();
    }
}

CDataType* ASNParser::TypesBlock(CDataMemberContainerType* containerType,
                                 bool allowDefaults)
{
    AutoPtr<CDataMemberContainerType> container(containerType);
    int line = NextTokenLine();
    ConsumeSymbol('{');
    CopyLineComment(line, container->Comments(), eCombineNext);
    for ( bool more = true; more; ) {
        line = NextTokenLine();
        AutoPtr<CDataMember> member = NamedDataType(allowDefaults);
        more = HaveMoreElements();
        line = Lexer().CurrentLine();
        if ( more ) {
            if (member->GetType()->IsContainer()) {
                CDataMemberContainerType* cont = 
                    dynamic_cast<CDataMemberContainerType*>(member->GetType());
                if (!cont->GetMembers().empty()) {
                    CDataMember* mem = cont->GetMembers().back().get();
                    CopyLineComment(mem->GetType()->GetSourceLine(),
                                    mem->Comments(), eCombineNext);
                }
            } else if (member->GetType()->IsEnumType()) {
                CEnumDataType* cont = 
                    dynamic_cast<CEnumDataType*>(member->GetType());
                if (!cont->GetValues().empty()) {
                    const CEnumDataTypeValue& val = cont->GetValues().back();
                    CopyLineComment(val.GetSourceLine(),
                        const_cast<CComments&>(val.GetComments()), eCombineNext);
                }
            }
            CopyLineComment(line, member->Comments(), eCombineNext);
        } else {
            CopyComments(member->Comments());
            CopyLineComment(member->GetType()->GetSourceLine(), member->Comments());
        }
        container->AddMember(member);
    }
    return container.release();
}

void ASNParser::BeginComponentsDefinition(void)
{
    TToken tok = Next();
    if ( tok != T_TYPE_REFERENCE ) {
        ParseError("type reference");
    }
    string reftype = TypeReference();
    AbstractLexer& curlexer = Lexer();
    string lexer_name = curlexer.GetName();

    if (m_MapDefinitions.find(reftype) == m_MapDefinitions.end()) {

        AutoPtr<CNcbiIfstream> in( new CNcbiIfstream(lexer_name.c_str()));
        if (!in.get()->is_open()) {
            ParseError("cannot access file",lexer_name.c_str());
        }
        ASNLexer tmplexer(*in.get(), lexer_name);
        SetLexer(&tmplexer);
        int level=0;
        string collected;
        bool started = false;
        for ( ;; ) {
            const AbstractToken& token = NextToken();
            tok = token.GetToken();
            string toktext = token.GetText();
            if (tok == T_EOF) {
                break;
            }
            Consume();
            if (started) {
                if ( tok == T_SYMBOL && toktext == "}") {
                    if (--level <= 0) {
                        break;
                    }
                }
                collected += ' ';
                collected += toktext;
                if (tok == T_SYMBOL && toktext == "{") {
                    ++level;
                }
                continue;
            }
            if (tok == K_COMPONENTS) {
                Consume(K_OF, "OF");
                NextToken();
                Consume();
            }
            if (tok == T_TYPE_REFERENCE) {
                started = (toktext == reftype);
                if (started) {
                    Consume(T_DEFINE, "::=");
                    Consume(K_SEQUENCE, "SEQUENCE");
                    Consume(T_SYMBOL, "{");
                    level = 1;
                }
            }
        }
        SetLexer(&curlexer);
        m_MapDefinitions[reftype] = collected;
    }

    CNcbiIstream* in = new CNcbiIstrstream(m_MapDefinitions[reftype].c_str());
    AbstractLexer *lexer = new ASNLexer(*in,lexer_name);
    Lexer().FlushCommentsTo(*lexer);
    m_StackLexer.push(lexer);
    SetLexer(lexer);
}

void ASNParser::EndComponentsDefinition(void)
{
    if (m_StackLexer.size() > 1) {
        delete m_StackLexer.top();
        m_StackLexer.pop();
        SetLexer(m_StackLexer.top());
    }
}

AutoPtr<CDataMember> ASNParser::NamedDataType(bool allowDefaults)
{
    string name;
    TToken tok = Next();
    if (tok == K_COMPONENTS) {
        Consume();
        Consume(K_OF, "OF");
        BeginComponentsDefinition();
        tok = Next();
    }
    if ( tok == T_IDENTIFIER ) {
        name = Identifier();
    }
    AutoPtr<CDataType> type(Type());

    AutoPtr<CDataMember> member(new CDataMember(name, type));
    if ( allowDefaults ) {
        switch ( Next() ) {
        default:
            break;
        case K_OPTIONAL:
            Consume();
            member->SetOptional();
            break;
        case K_DEFAULT:
            Consume();
            member->SetDefault(Value());
            break;
        }
    }
    CopyComments(member->Comments());
    return member;
}

CEnumDataType* ASNParser::EnumeratedBlock(CEnumDataType* enumType)
{
    AutoPtr<CEnumDataType> e(enumType);
    int line = NextTokenLine();
    ConsumeSymbol('{');
    CopyLineComment(line, e->Comments(), eCombineNext);
    for ( bool more = true; more; ) {
        line = NextTokenLine();
        CEnumDataTypeValue& value = EnumeratedValue(*e);
        value.SetSourceLine(line);
        more = HaveMoreElements();
        CopyLineComment(line, value.GetComments(), eCombineNext);
    }
    return e.release();
}

CEnumDataTypeValue& ASNParser::EnumeratedValue(CEnumDataType& t)
{
    string id = Identifier();
    ConsumeSymbol('(');
    Int4 value = Number();
    ConsumeSymbol(')');
    CEnumDataTypeValue& ret = t.AddValue(id, value);
    CopyComments(ret.GetComments());
    return ret;
}

void ASNParser::TypeList(list<string>& ids)
{
    do {
        ids.push_back(TypeReference());
    } while ( ConsumeIfSymbol(',') );
}

AutoPtr<CDataValue> ASNParser::Value(void)
{
    int line = NextTokenLine();
    AutoPtr<CDataValue> value(x_Value());
    value->SetSourceLine(line);
    return value;
}

AutoPtr<CDataValue> ASNParser::x_Value(void)
{
    switch ( Next() ) {
    default:
        break;
    case T_NUMBER:
        return AutoPtr<CDataValue>(new CIntDataValue(Number()));
    case T_DOUBLE:
        return AutoPtr<CDataValue>(new CDoubleDataValue(Double()));
    case T_STRING:
        return AutoPtr<CDataValue>(new CStringDataValue(String()));
    case K_NULL:
        Consume();
        return AutoPtr<CDataValue>(new CNullDataValue());
    case K_FALSE:
        Consume();
        return AutoPtr<CDataValue>(new CBoolDataValue(false));
    case K_TRUE:
        Consume();
        return AutoPtr<CDataValue>(new CBoolDataValue(true));
    case T_IDENTIFIER:
        {
            string id = Identifier();
            if ( CheckSymbols(',', '}') )
                return AutoPtr<CDataValue>(new CIdDataValue(id));
            else
                return AutoPtr<CDataValue>(new CNamedDataValue(id, Value()));
        }
    case T_BINARY_STRING:
    case T_HEXADECIMAL_STRING:
        return AutoPtr<CDataValue>(new CBitStringDataValue(ConsumeAndValue()));
    case T_SYMBOL:
        switch ( NextToken().GetSymbol() ) {
        default:
            break;
        case '-':
            return AutoPtr<CDataValue>(new CIntDataValue(Number()));
        case '{':
            {
                Consume();
                AutoPtr<CBlockDataValue> b(new CBlockDataValue());
                if ( !CheckSymbol('}') ) {
                    do {
                        b->GetValues().push_back(Value());
                    } while ( ConsumeIfSymbol(',') );
                }
                ConsumeSymbol('}');
                return AutoPtr<CDataValue>(b.release());
            }
        }
		break;
    }
    ParseError("value");
	return AutoPtr<CDataValue>(0);
}

Int4 ASNParser::Number(void)
{
    return NStr::StringToInt(ValueOf(T_NUMBER, "number"));
}

double ASNParser::Double(void)
{
    return NStr::StringToDouble(ValueOf(T_DOUBLE, "double"), NStr::fDecimalPosix);
}

const string& ASNParser::String(void)
{
    Expect(T_STRING, "string");
    Consume();
    return L().StringValue();
}

const string& ASNParser::Identifier(void)
{
    switch ( Next() ) {
    default:
        break;
    case T_TYPE_REFERENCE:
        Lexer().LexerError("identifier must begin with lowercase letter");
        return ConsumeAndValue();
    case T_IDENTIFIER:
        return ConsumeAndValue();
    }
    ParseError("identifier");
	return NcbiEmptyString;
}

const string& ASNParser::TypeReference(void)
{
    switch ( Next() ) {
    default:
        break;
    case T_TYPE_REFERENCE:
        return ConsumeAndValue();
    case T_IDENTIFIER:
        Lexer().LexerError("type name must begin with uppercase letter");
        return ConsumeAndValue();
    }
    ParseError("type name");
	return NcbiEmptyString;
}

const string& ASNParser::ModuleReference(void)
{
    switch ( Next() ) {
    default:
        break;
    case T_TYPE_REFERENCE:
        return ConsumeAndValue();
    case T_IDENTIFIER:
        Lexer().LexerError("module name must begin with uppercase letter");
        return ConsumeAndValue();
    }
    ParseError("module name");
	return NcbiEmptyString;
}

END_NCBI_SCOPE
