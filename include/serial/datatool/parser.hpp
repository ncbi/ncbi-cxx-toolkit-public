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
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbiutil.hpp>
#include <serial/tool/aparser.hpp>
#include <serial/tool/moduleset.hpp>
#include <list>

class CFileModules;
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

    AutoPtr<CFileModules> Modules(const string& fileName);
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
