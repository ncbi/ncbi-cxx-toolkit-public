#ifndef ASNPARSER_HPP
#define ASNPARSER_HPP

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

#include <corelib/ncbiutil.hpp>
#include <serial/datatool/aparser.hpp>
#include <serial/datatool/lexer.hpp>
#include <serial/datatool/moduleset.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CFileModules;
class CDataModule;
class CDataType;
class CDataMemberContainerType;
class CDataValue;
class CDataMember;
class CEnumDataType;
class CEnumDataTypeValue;

class ASNParser : public AbstractParser
{
public:
    ASNParser(ASNLexer& lexer)
        : AbstractParser(lexer)
        {
        }

    const ASNLexer& L(void) const
        {
            return static_cast<const ASNLexer&>(Lexer());
        }
    ASNLexer& L(void)
        {
            return static_cast<ASNLexer&>(Lexer());
        }

    AutoPtr<CFileModules> Modules(const string& fileName);
    AutoPtr<CDataTypeModule> Module(void);
    void Imports(CDataTypeModule& module);
    void Exports(CDataTypeModule& module);
    void ModuleBody(CDataTypeModule& module);
    void ModuleType(CDataTypeModule& module, const string& name);
    AutoPtr<CDataType> Type(void);
    CDataType* x_Type(void);
    CDataType* TypesBlock(CDataMemberContainerType* containerType,
                          bool allowDefaults);
    AutoPtr<CDataMember> NamedDataType(bool allowDefaults);
    CEnumDataType* EnumeratedBlock(CEnumDataType* enumType);
    CEnumDataTypeValue& EnumeratedValue(CEnumDataType& enumType);
    void TypeList(list<string>& ids);
    AutoPtr<CDataValue> Value(void);
    AutoPtr<CDataValue> x_Value(void);
    Int4 Number(void);
    double Double(void);
    const string& String(void);
    const string& Identifier(void);
    const string& TypeReference(void);
    const string& ModuleReference(void);

    bool HaveMoreElements(void);
    void SkipTo(char ch);
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2006/10/18 13:04:26  gouriano
* Moved Log to bottom
*
* Revision 1.10  2004/02/26 16:23:48  gouriano
* Skip datatype constraints when parsing ASN.1 spec
*
* Revision 1.9  2004/02/25 19:45:48  gouriano
* Made it possible to define DEFAULT for data members of type REAL
*
* Revision 1.8  2000/12/15 15:38:35  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.7  2000/11/15 20:34:43  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.6  2000/11/14 21:41:14  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.5  2000/11/01 20:36:11  vasilche
* OPTIONAL and DEFAULT are not permitted in CHOICE.
* Fixed code generation for DEFAULT.
*
* Revision 1.4  2000/09/26 17:38:17  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.3  2000/08/25 15:58:47  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.2  2000/04/07 19:26:11  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:21  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.11  1999/12/28 18:56:00  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.10  1999/11/19 15:48:11  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.9  1999/11/15 19:36:18  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
