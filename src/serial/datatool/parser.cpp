#include "parser.hpp"
#include "tokens.hpp"
#include "module.hpp"
#include "moduleset.hpp"
#include "type.hpp"
#include "value.hpp"

void ASNParser::Modules(const CFilePosition& filePos, CModuleSet& moduleSet)
{
    CDataTypeContext ctx(filePos);
    while ( Next() != T_EOF ) {
        Module(ctx, moduleSet);
    }
}

void ASNParser::Module(const CDataTypeContext& ctx, CModuleSet& moduleSet)
{
    AutoPtr<ASNModule> module(new ASNModule());
    module->moduleSet = &moduleSet;
    module->name = ModuleReference();
    {
        Consume(K_DEFINITIONS, "DEFINITIONS");
        Consume(T_DEFINE, "::=");
        Consume(K_BEGIN, "BEGIN");
        ModuleBody(CDataTypeContext(ctx, *module));
        Consume(K_END, "END");
    }
    if ( !module->Check() )
        Warning("Errors was found in module " + module->name);
        
    moduleSet.modules.push_back(module);
}

void ASNParser::Imports(const CDataTypeContext& ctx)
{
    do {
        AutoPtr<ASNModule::Import> imp(new ASNModule::Import);
        TypeList(imp->types);
        Consume(K_FROM, "FROM");
        imp->module = ModuleReference();
        ctx.GetModule().imports.push_back(imp);
    } while ( !ConsumeIfSymbol(';') );
}

void ASNParser::Exports(const CDataTypeContext& ctx)
{
    try {
        TypeList(ctx.GetModule().exports);
    }
    catch (runtime_error e) {
        ERR_POST(e.what());
    }
    ConsumeSymbol(';');
}

void ASNParser::ModuleBody(const CDataTypeContext& ctx)
{
    string name;
    while ( true ) {
        try {
            switch ( Next() ) {
            case K_EXPORTS:
                Consume();
                Exports(ctx);
                break;
            case K_IMPORTS:
                Consume();
                Imports(ctx);
                break;
            case T_TYPE_REFERENCE:
            case T_IDENTIFIER:
                name = TypeReference();
                Consume(T_DEFINE, "::=");
                ctx.GetModule().AddDefinition(name,
                    Type(CDataTypeContext(ctx, name, CDataTypeContext::eSection)));
                break;
            case T_DEFINE:
                ERR_POST(Location() << "type name omitted");
                Consume();
                ctx.GetModule().AddDefinition("unnamed type", Type(ctx));
                break;
            case K_END:
                return;
            default:
                ERR_POST(Location() << "type definition expected");
                return;
            }
        }
        catch (runtime_error e) {
            ERR_POST(e.what());
        }
    }
}

AutoPtr<ASNType> ASNParser::Type(const CDataTypeContext& ctx)
{
    return x_Type(CDataTypeContext(ctx, NextToken().GetLine()));
}

AutoPtr<ASNType> ASNParser::x_Type(const CDataTypeContext& ctx)
{
    switch ( Next() ) {
    case K_BOOLEAN:
        Consume();
        return new ASNBooleanType(ctx);
    case K_INTEGER:
        Consume();
        if ( CheckSymbol('{') )
            return EnumeratedBlock(ctx, "INTEGER");
        else
            return new ASNIntegerType(ctx);
    case K_ENUMERATED:
        Consume();
        return EnumeratedBlock(ctx, "ENUMERATED");
    case K_REAL:
        Consume();
        return new ASNRealType(ctx);
    case K_BIT:
        Consume();
        Consume(K_STRING, "STRING");
        return new ASNBitStringType(ctx);
    case K_OCTET:
        Consume();
        Consume(K_STRING, "STRING");
        return new ASNOctetStringType(ctx);
    case K_NULL:
        Consume();
        return new ASNNullType(ctx);
    case K_SEQUENCE:
        Consume();
        if ( ConsumeIf(K_OF) ) {
            AutoPtr<ASNOfType> s(new ASNSequenceOfType(ctx));
            s->type = Type(CDataTypeContext(ctx, "._data",
                                            CDataTypeContext::eKey));
            return s.release();
        }
        else {
            AutoPtr<ASNSequenceType> s(new ASNSequenceType(ctx));
            TypesBlock(ctx, *s);
            return s.release();
        }
    case K_SET:
        Consume();
        if ( ConsumeIf(K_OF) ) {
            AutoPtr<ASNOfType> s(new ASNSetOfType(ctx));
            s->type = Type(CDataTypeContext(ctx, "._data",
                                            CDataTypeContext::eKey));
            return s.release();
        }
        else {
            AutoPtr<ASNSetType> s(new ASNSetType(ctx));
            TypesBlock(ctx, *s);
            return s.release();
        }
    case K_CHOICE:
        Consume();
        {
            AutoPtr<ASNChoiceType> choice(new ASNChoiceType(ctx));
            TypesBlock(ctx, *choice);
            return choice.release();
        }
    case K_VisibleString:
        Consume();
        return new ASNVisibleStringType(ctx);
    case K_StringStore:
        Consume();
        return new ASNStringStoreType(ctx);
    case T_IDENTIFIER:
    case T_TYPE_REFERENCE:
        return new ASNUserType(ctx, TypeReference());
    }
    ParseError("type");
	return 0;
}

void ASNParser::TypesBlock(const CDataTypeContext& ctx,
                           ASNMemberContainerType& container)
{
    ConsumeSymbol('{');
    do {
        container.members.push_back(NamedDataType(ctx));
    } while ( ConsumeIfSymbol(',') );
    ConsumeSymbol('}');
}

AutoPtr<ASNMember> ASNParser::NamedDataType(const CDataTypeContext& ctx)
{
    AutoPtr<ASNMember> member(new ASNMember);
    string memberKeyAdd;
    if ( Next() == T_IDENTIFIER ) {
        memberKeyAdd = member->name = Identifier();
    }
    else {
        memberKeyAdd = "_parent";
    }
    member->type = Type(CDataTypeContext(ctx, memberKeyAdd,
                                         CDataTypeContext::eKey));
    switch ( Next() ) {
    case K_OPTIONAL:
        Consume();
        member->optional = true;
        break;
    case K_DEFAULT:
        Consume();
        member->defaultValue = Value(ctx);
        break;
    }
    return member.release();
}

AutoPtr<ASNType> ASNParser::EnumeratedBlock(const CDataTypeContext& ctx,
                                            const string& keyword)
{
    AutoPtr<ASNEnumeratedType> e(new ASNEnumeratedType(ctx, keyword));
    ConsumeSymbol('{');
    do {
        EnumeratedValue(*e);
    } while ( ConsumeIfSymbol(',') );
    ConsumeSymbol('}');
    return e.release();
}

void ASNParser::EnumeratedValue(ASNEnumeratedType& t)
{
    string id = Identifier();
    ConsumeSymbol('(');
    int value = Number();
    ConsumeSymbol(')');
    t.AddValue(id, value);
}

void ASNParser::TypeList(list<string>& ids)
{
    do {
        ids.push_back(TypeReference());
    } while ( ConsumeIfSymbol(',') );
}

AutoPtr<ASNValue> ASNParser::Value(const CDataTypeContext& ctx)
{
    return x_Value(CDataTypeContext(ctx, NextToken().GetLine()));
}

AutoPtr<ASNValue> ASNParser::x_Value(const CDataTypeContext& ctx)
{
    switch ( Next() ) {
    case T_NUMBER:
        return new ASNIntegerValue(ctx, Number());
    case T_STRING:
        return new ASNStringValue(ctx, String());
    case K_NULL:
        Consume();
        return new ASNNullValue(ctx);
    case K_FALSE:
        Consume();
        return new ASNBoolValue(ctx, false);
    case K_TRUE:
        Consume();
        return new ASNBoolValue(ctx, true);
    case T_IDENTIFIER:
        {
            string id = Identifier();
            if ( CheckSymbols(',', '}') )
                return new ASNIdValue(ctx, id);
            else
                return new ASNNamedValue(ctx, id, Value(ctx));
        }
    case T_BINARY_STRING:
    case T_HEXADECIMAL_STRING:
        return new ASNBitStringValue(ctx, ConsumeAndValue());
    case T_SYMBOL:
        switch ( NextToken().GetSymbol() ) {
        case '-':
            return new ASNIntegerValue(ctx, Number());
        case '{':
            {
                Consume();
                AutoPtr<ASNBlockValue> b(new ASNBlockValue(ctx));
                if ( !CheckSymbol('}') ) {
                    do {
                        b->values.push_back(Value(ctx));
                    } while ( ConsumeIfSymbol(',') );
                }
                ConsumeSymbol('}');
                return b.release();
            }
        }
		break;
    }
    ParseError("value");
	return 0;
}

int ASNParser::Number(void)
{
    bool minus = ConsumeIfSymbol('-');
    int value = NStr::StringToUInt(ValueOf(T_NUMBER, "number"));
    if ( minus )
        value = -value;
    return value;
}

string ASNParser::String(void)
{
    Expect(T_STRING, "string");
    const string& ret = Lexer().CurrentTokenValue();
    Consume();
    return ret;
}

string ASNParser::Identifier(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        ERR_POST(Location() << "identifier must begin with lowercase letter");
        return ConsumeAndValue();
    case T_IDENTIFIER:
        return ConsumeAndValue();
    }
    ParseError("identifier");
	return NcbiEmptyString;
}

string ASNParser::TypeReference(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        return ConsumeAndValue();
    case T_IDENTIFIER:
        ERR_POST(Location() << "type name must begin with uppercase letter");
        return ConsumeAndValue();
    }
    ParseError("type name");
	return NcbiEmptyString;
}

string ASNParser::ModuleReference(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        return ConsumeAndValue();
    case T_IDENTIFIER:
        ERR_POST(Location() << "module name must begin with uppercase letter");
        return ConsumeAndValue();
    }
    ParseError("module name");
	return NcbiEmptyString;
}
