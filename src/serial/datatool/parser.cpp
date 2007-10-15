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
#include <serial/datatool/parser.hpp>
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
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_Parsers

BEGIN_NCBI_SCOPE

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
    Consume(T_DEFINE, "::=");
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
            NCBI_RETHROW_SAME(e,"ASNParser::ModuleBody: failed");
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
    type->SetSourceLine(line);
    return type;
}

CDataType* ASNParser::x_Type(void)
{
    TToken tok = Next();
    switch ( tok ) {
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
    case T_TAG:
        string val = ConsumeAndValue();
        CDataType* tagged = x_Type();
        tagged->SetTag(NStr::StringToInt(val.substr(1, val.length()-2)));
        return tagged;
    }
    ParseError("type");
	return 0;
}

bool ASNParser::HaveMoreElements(void)
{
    const AbstractToken& token = NextToken();
    if ( token.GetToken() == T_SYMBOL ) {
        switch ( token.GetSymbol() ) {
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

AutoPtr<CDataMember> ASNParser::NamedDataType(bool allowDefaults)
{
    string name;
    if ( Next() == T_IDENTIFIER )
        name = Identifier();
    AutoPtr<CDataType> type(Type());

    AutoPtr<CDataMember> member(new CDataMember(name, type));
    if ( allowDefaults ) {
        switch ( Next() ) {
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
    return NStr::StringToDouble(ValueOf(T_DOUBLE, "double"));
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
