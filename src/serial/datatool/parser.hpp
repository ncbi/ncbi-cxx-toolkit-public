#ifndef ASNPARSER_HPP
#define ASNPARSER_HPP

#include "aparser.hpp"
#include "autoptr.hpp"
#include <list>

class CModuleSet;
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

    void Modules(CModuleSet& moduleSet);
    void Module(CModuleSet& moduleSet);
    void Imports(ASNModule* module);
    void Exports(ASNModule* module);
    void ModuleBody(ASNModule* module);
    AutoPtr<ASNType> Type(ASNModule& module);
    AutoPtr<ASNType> x_Type(ASNModule& module);
    void TypesBlock(ASNModule& module, list<AutoPtr<ASNMember> >& members);
    AutoPtr<ASNMember> NamedDataType(ASNModule& module);
    AutoPtr<ASNType> EnumeratedBlock(ASNModule& module, const string& keyword);
    void EnumeratedValue(ASNEnumeratedType* t);
    void TypeList(list<string>& ids);
    AutoPtr<ASNValue> Value(void);
    AutoPtr<ASNValue> x_Value(void);
    int Number(void);
    string String(void);
    string Identifier(void);
    string TypeReference(void);
    string ModuleReference(void);
    string BinaryString(void);
    string HexadecimalString(void);
};

#endif
