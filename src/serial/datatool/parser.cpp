#include <parser.hpp>
#include <tokens.hpp>
#include <module.hpp>
#include <type.hpp>
#include <value.hpp>

void ASNParser::Modules(list<AutoPtr<ASNModule> >& modules)
{
    while ( Next() != T_EOF ) {
        modules.push_back(Module());
    }
}

AutoPtr<ASNModule> ASNParser::Module(void)
{
    AutoPtr<ASNModule> module(new ASNModule());
    module->name = ModuleReference();
    Consume(K_DEFINITIONS, "DEFINITIONS");
    Consume(T_DEFINE, "::=");
    Consume(K_BEGIN, "BEGIN");
    ModuleBody(module.get());
    Consume(K_END, "END");

    if ( !module->Check() )
        Warning("Errors was found in module " + module->name);

    return module;
}

void ASNParser::Imports(ASNModule* module)
{
    do {
        AutoPtr<ASNModule::Import> imp(new ASNModule::Import);
        TypeList(imp->types);
        Consume(K_FROM, "FROM");
        imp->module = ModuleReference();
        module->imports.push_back(imp);
    } while ( !ConsumeIfSymbol(';') );
}

void ASNParser::Exports(ASNModule* module)
{
    try {
        TypeList(module->exports);
    }
    catch (runtime_error e) {
        ERR_POST(e.what());
    }
    ConsumeSymbol(';');
}

void ASNParser::ModuleBody(ASNModule* module)
{
    while ( true ) {
        try {
            switch ( Next() ) {
            case K_EXPORTS:
                Consume();
                Exports(module);
                break;
            case K_IMPORTS:
                Consume();
                Imports(module);
                break;
            case T_TYPE_REFERENCE:
            case T_IDENTIFIER:
                {
                    string name = TypeReference();
                    Consume(T_DEFINE, "::=");
                    module->AddDefinition(name, Type());
                }
                break;
            case T_DEFINE:
                ERR_POST(Location() << "type name omitted");
                Consume();
                module->AddDefinition("unnamed type", Type());
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

AutoPtr<ASNType> ASNParser::Type(void)
{
    int line = NextToken().GetLine();
    AutoPtr<ASNType> type(x_Type());
    type->line = line;
    return type;
}

AutoPtr<ASNType> ASNParser::x_Type(void)
{
    switch ( Next() ) {
    case K_BOOLEAN:
        Consume();
        return new ASNBooleanType;
    case K_INTEGER:
        Consume();
        if ( CheckSymbol('{') )
            return EnumeratedBlock("INTEGER");
        else
            return new ASNIntegerType;
    case K_ENUMERATED:
        Consume();
        return EnumeratedBlock("ENUMERATED");
    case K_REAL:
        Consume();
        return new ASNRealType;
    case K_BIT:
        Consume();
        Consume(K_STRING, "STRING");
        return new ASNBitStringType;
    case K_OCTET:
        Consume();
        Consume(K_STRING, "STRING");
        return new ASNOctetStringType;
    case K_NULL:
        Consume();
        return new ASNNullType;
    case K_SEQUENCE:
        Consume();
        if ( ConsumeIf(K_OF) )
            return new ASNSequenceOfType(Type());
        else {
            AutoPtr<ASNSequenceType> s(new ASNSequenceType);
            TypesBlock(s->members);
            return s.release();
        }
    case K_SET:
        Consume();
        if ( ConsumeIf(K_OF) )
            return new ASNSetOfType(Type());
        else {
            AutoPtr<ASNSetType> s(new ASNSetType);
            TypesBlock(s->members);
            return s.release();
        }
    case K_CHOICE:
        Consume();
        {
            AutoPtr<ASNChoiceType> choice(new ASNChoiceType);
            TypesBlock(choice->members);
            return choice.release();
        }
    case K_VisibleString:
        Consume();
        return new ASNVisibleStringType;
    case K_StringStore:
        Consume();
        return new ASNStringStoreType;
    case T_IDENTIFIER:
    case T_TYPE_REFERENCE:
        return new ASNUserType(TypeReference());
    default:
        ParseError("type");
    }
}

void ASNParser::TypesBlock(list<AutoPtr<ASNMember> >& members)
{
    ConsumeSymbol('{');
    do {
        members.push_back(NamedDataType());
    } while ( ConsumeIfSymbol(',') );
    ConsumeSymbol('}');
}

AutoPtr<ASNMember> ASNParser::NamedDataType(void)
{
    AutoPtr<ASNMember> member(new ASNMember);
    member->name = Identifier();
    member->type = Type();
    switch ( Next() ) {
    case K_OPTIONAL:
        Consume();
        member->optional = true;
        break;
    case K_DEFAULT:
        Consume();
        member->defaultValue = Value();
        break;
    }
    return member.release();
}

AutoPtr<ASNType> ASNParser::EnumeratedBlock(const string& keyword)
{
    AutoPtr<ASNEnumeratedType> e(new ASNEnumeratedType(keyword));
    ConsumeSymbol('{');
    do {
        EnumeratedValue(e.get());
    } while ( ConsumeIfSymbol(',') );
    ConsumeSymbol('}');
    return e.release();
}

void ASNParser::EnumeratedValue(ASNEnumeratedType* t)
{
    string id = Identifier();
    if ( ConsumeIfSymbol('(') ) {
        long value = Number();
        ConsumeSymbol(')');
        t->AddValue(id, value);
    }
    else {
        t->AddValue(id);
    }
}

void ASNParser::TypeList(list<string>& ids)
{
    do {
        ids.push_back(TypeReference());
    } while ( ConsumeIfSymbol(',') );
}

AutoPtr<ASNValue> ASNParser::Value(void)
{
    int line = NextToken().GetLine();
    AutoPtr<ASNValue> value(x_Value());
    value->line = line;
    return value;
}

AutoPtr<ASNValue> ASNParser::x_Value(void)
{
    switch ( Next() ) {
    case T_NUMBER:
        return new ASNIntegerValue(Number());
    case T_STRING:
        return new ASNStringValue(String());
    case K_NULL:
        Consume();
        return new ASNNullValue;
    case K_FALSE:
        Consume();
        return new ASNBoolValue(false);
    case K_TRUE:
        Consume();
        return new ASNBoolValue(true);
    case T_IDENTIFIER:
        {
            string id = Identifier();
            if ( CheckSymbols(',', '}') )
                return new ASNIdValue(id);
            else
                return new ASNNamedValue(id, Value());
        }
    case T_BINARY_STRING:
    case T_HEXADECIMAL_STRING:
        return new ASNBitStringValue(ConsumeAndValue());
    case T_SYMBOL:
        switch ( NextToken().GetSymbol() ) {
        case '-':
            return new ASNIntegerValue(Number());
        case '{':
            {
                Consume();
                AutoPtr<ASNBlockValue> b(new ASNBlockValue);
                if ( !CheckSymbol('}') ) {
                    do {
                        b->values.push_back(Value());
                    } while ( ConsumeIfSymbol(',') );
                }
                ConsumeSymbol('}');
                return b.release();
            }
        default:
            ParseError("value");
        }
    default:
        ParseError("value");
    }
}

long ASNParser::Number(void)
{
    bool minus = ConsumeIfSymbol('-');
    long value = NStr::StringToUInt(ValueOf(T_NUMBER, "number"));
    if ( minus )
        value = -value;
    return value;
}

string ASNParser::String(void)
{
    return ValueOf(T_STRING, "string");
}

string ASNParser::Identifier(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        ERR_POST(Location() << "identifier must begin with lowercase letter");
        return ConsumeAndValue();
    case T_IDENTIFIER:
        return ConsumeAndValue();
    default:
        ParseError("identifier");
    }
}

string ASNParser::TypeReference(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        return ConsumeAndValue();
    case T_IDENTIFIER:
        ERR_POST(Location() << "type name must begin with uppercase letter");
        return ConsumeAndValue();
    default:
        ParseError("type name");
    }
}

string ASNParser::ModuleReference(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        return ConsumeAndValue();
    case T_IDENTIFIER:
        ERR_POST(Location() << "module name must begin with uppercase letter");
        return ConsumeAndValue();
    default:
        ParseError("module name");
    }
}
