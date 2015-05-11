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
#include <serial/serialdef.hpp>
#include <serial/impl/objstrasnb.hpp>
#include "aparser.hpp"
#include "lexer.hpp"
#include "moduleset.hpp"
#include <list>
#include <stack>
#include <map>

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
    ASNParser(ASNLexer& lexer);

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
    void BeginComponentsDefinition(void);
    void EndComponentsDefinition(void);
    AutoPtr<CDataMember> NamedDataType(bool allowDefaults);
    CEnumDataType* EnumeratedBlock(CEnumDataType* enumType);
    CEnumDataTypeValue& EnumeratedValue(CEnumDataType& enumType);
    void TypeList(list<string>& ids);
    AutoPtr<CDataValue> Value(const CDataType* type);
    AutoPtr<CDataValue> x_Value(const CDataType* type);
    Int4 Number(void);
    double Double(TToken token);
    const string& String(void);
    const string& Identifier(void);
    const string& TypeReference(void);
    const string& ModuleReference(void);

    bool HaveMoreElements(void);
    void SkipTo(char ch);

private:
    CAsnBinaryDefs::ETagType m_TagDefault;
    stack<AbstractLexer*>  m_StackLexer;
    map<string, string> m_MapDefinitions;
};

END_NCBI_SCOPE

#endif
