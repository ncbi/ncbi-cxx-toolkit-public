#ifndef ASNPARSER_HPP
#define ASNPARSER_HPP

#include "aparser.hpp"
#include "autoptr.hpp"
#include "typecontext.hpp"
#include "moduleset.hpp"
#include <list>

class CModuleSet;
class ASNModule;
class ASNType;
class ASNMemberContainerType;
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

    void Modules(const CFilePosition& filePos, CModuleSet& moduleSet,
                 CModuleSet::TModules& modules);
    void Module(const CDataTypeContext& ctx, CModuleSet& moduleSet,
                CModuleSet::TModules& modules);
    void Imports(const CDataTypeContext& ctx);
    void Exports(const CDataTypeContext& ctx);
    void ModuleBody(const CDataTypeContext& ctx);
    AutoPtr<ASNType> Type(const CDataTypeContext& ctx);
    AutoPtr<ASNType> x_Type(const CDataTypeContext& ctx);
    void TypesBlock(const CDataTypeContext& ctx,
                    ASNMemberContainerType& container);
    AutoPtr<ASNMember> NamedDataType(const CDataTypeContext& ctx);
    AutoPtr<ASNType> EnumeratedBlock(const CDataTypeContext& ctx,
                                     const string& keyword);
    void EnumeratedValue(ASNEnumeratedType& t);
    void TypeList(list<string>& ids);
    AutoPtr<ASNValue> Value(const CDataTypeContext& ctx);
    AutoPtr<ASNValue> x_Value(const CDataTypeContext& ctx);
    int Number(void);
    string String(void);
    string Identifier(void);
    string TypeReference(void);
    string ModuleReference(void);
    string BinaryString(void);
    string HexadecimalString(void);
};

#endif
