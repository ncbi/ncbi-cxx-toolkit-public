#ifndef ASNPARSER_HPP
#define ASNPARSER_HPP

#include "aparser.hpp"
#include "autoptr.hpp"
#include "typecontext.hpp"
#include "moduleset.hpp"
#include <list>

class CModuleSet;
class CDataModule;
class CDataType;
class CDataMemberContainerType;
class CDataValue;
class CDataMember;
class CEnumDataType;

class ASNParser : public AbstractParser
{
public:
    ASNParser(AbstractLexer& lexer)
        : AbstractParser(lexer)
        {
        }

    AutoPtr<CModuleSet> Modules(const string& fileName);
    AutoPtr<CDataTypeModule> Module(void);
    void Imports(CDataTypeModule& module);
    void Exports(CDataTypeModule& module);
    void ModuleBody(CDataTypeModule& module);
    AutoPtr<CDataType> Type(void);
    AutoPtr<CDataType> x_Type(void);
    AutoPtr<CDataType> TypesBlock(CDataMemberContainerType* containerType);
    AutoPtr<CDataMember> NamedDataType(void);
    AutoPtr<CDataType> EnumeratedBlock(CEnumDataType* enumType);
    void EnumeratedValue(CEnumDataType& enumType);
    void TypeList(list<string>& ids);
    AutoPtr<CDataValue> Value(void);
    AutoPtr<CDataValue> x_Value(void);
    long Number(void);
    string String(void);
    string Identifier(void);
    string TypeReference(void);
    string ModuleReference(void);
    string BinaryString(void);
    string HexadecimalString(void);
};

#endif
