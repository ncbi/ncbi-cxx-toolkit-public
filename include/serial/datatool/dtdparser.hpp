#ifndef DTDPARSER_HPP
#define DTDPARSER_HPP

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
* Author: Andrei Gourianov
*
* File Description:
*   DTD lexer
*
* ===========================================================================
*/

#include <corelib/ncbiutil.hpp>
#include <serial/datatool/aparser.hpp>
#include <serial/datatool/dtdlexer.hpp>
#include <serial/datatool/moduleset.hpp>
#include <list>
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


/////////////////////////////////////////////////////////////////////////////
// DTDElement

class DTDElement
{
public:
    DTDElement(void);
    DTDElement(const DTDElement& other);
    virtual ~DTDElement(void);

    enum EType {
        eUnknown,
        eString,   // #PCDATA
        eAny,      // ANY
        eEmpty,    // EMPTY
        eSequence, // (a,b,c)
        eChoice    // (a|b|c)
                   // mixed content is not implemented
    };
    enum EOccurrence {
        eOne,
        eOneOrMore,
        eZeroOrMore,
        eZeroOrOne
    };

    void SetName(const string& name);
    const string& GetName(void) const;

    void SetType( EType type);
    void SetTypeIfUnknown( EType type);
    EType GetType(void) const;

    void SetOccurrence( const string& ref_name, EOccurrence occ);
    EOccurrence GetOccurrence(const string& ref_name) const;

    void SetOccurrence( EOccurrence occ);
    EOccurrence GetOccurrence(void) const;

    // i.e. element contains other elements
    void AddContent( const string& ref_name);
    const list<string>& GetContent(void) const;

    // element is contained somewhere
    void SetReferenced(void);
    bool IsReferenced(void) const;

    // element does not have any specific name
    void SetEmbedded(void);
    bool IsEmbedded(void) const;
    string CreateEmbeddedName(int depth) const;

private:
    string m_Name;
    EType m_Type;
    EOccurrence m_Occ;
    list<string> m_Refs;
    map<string,EOccurrence> m_RefOcc;
    bool m_Refd;
    bool m_Embd;
};

/////////////////////////////////////////////////////////////////////////////
// DTDParser

class DTDParser : public AbstractParser
{
public:
    DTDParser( DTDLexer& lexer);
    virtual ~DTDParser(void);

/*
    const ASNLexer& L(void) const
        {
            return static_cast<const ASNLexer&>(Lexer());
        }
    ASNLexer& L(void)
        {
            return static_cast<ASNLexer&>(Lexer());
        }
*/

    AutoPtr<CFileModules> Modules(const string& fileName);

protected:
    AutoPtr<CDataTypeModule> Module(const string& name);

    void BuildDocumentTree(void);
#ifdef _DEBUG
    void PrintDocumentTree(void);
    void PrintDocumentNode(const string& name, DTDElement& node);
#endif
    void ParseElementContent(const string& name, bool embedded);
    void ConsumeElementContent(DTDElement& node);
    void EndElementContent(DTDElement& node);
    void FixEmbeddedNames(DTDElement& node);

    void GenerateDataTree(CDataTypeModule& module);
    void ModuleType(CDataTypeModule& module, const DTDElement& node);
    AutoPtr<CDataType> Type(const DTDElement& node, DTDElement::EOccurrence occ, bool in_elem);
    CDataType* x_Type(const DTDElement& node, DTDElement::EOccurrence occ, bool in_elem);
    CDataType* TypesBlock(CDataMemberContainerType* containerType,const DTDElement& node);

    map<string,DTDElement> m_mapElement;
/*
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
    const string& String(void);
    const string& Identifier(void);
    const string& TypeReference(void);
    const string& ModuleReference(void);

    bool HaveMoreElements(void);
*/
};

END_NCBI_SCOPE

#endif // DTDPARSER_HPP


/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2002/10/15 13:50:15  gouriano
 * DTD lexer and parser. first version
 *
 *
 * ==========================================================================
 */
