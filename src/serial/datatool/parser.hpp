#ifndef ASNPARSER_HPP
#define ASNPARSER_HPP

#include <aparser.hpp>
#include <autoptr.hpp>
#include <list>

class ASNModule;
class ASNType;
class ASNValue;
class ASNMember;
class ASNEnumeratedType;

class ASNParser : public AbstractParser
{
public:
    ASNParser(AbstractLexer& lexer)
        : AbstractParser(lexer)
        {
        }

    void Modules(list<AutoPtr<ASNModule> >& modules);
    AutoPtr<ASNModule> Module(void);
    void Imports(ASNModule* module);
    void Exports(ASNModule* module);
    void ModuleBody(ASNModule* module);
    AutoPtr<ASNType> Type(void);
    AutoPtr<ASNType> x_Type(void);
    void TypesBlock(list<AutoPtr<ASNMember> >& members);
    AutoPtr<ASNMember> NamedDataType(void);
    AutoPtr<ASNType> EnumeratedBlock(const string& keyword);
    void EnumeratedValue(ASNEnumeratedType* t);
    void TypeList(list<string>& ids);
    AutoPtr<ASNValue> Value(void);
    AutoPtr<ASNValue> x_Value(void);
    long Number(void);
    string String(void);
    string Identifier(void);
    string TypeReference(void);
    string ModuleReference(void);
    string BinaryString(void);
    string HexadecimalString(void);
};

#endif
