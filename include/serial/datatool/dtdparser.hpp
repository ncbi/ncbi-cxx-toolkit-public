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
#include <stack>

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
// DTDEntity

class DTDEntity
{
public:
    DTDEntity(void);
    DTDEntity(const DTDEntity& other);
    virtual ~DTDEntity(void);

    void SetName(const string& name);
    const string& GetName(void) const;

    void SetData(const string& data);
    const string& GetData(void) const;

    void SetExternal(void);
    bool IsExternal(void) const;

private:
    string m_Name;
    string m_Data;
    bool   m_External;
};

/////////////////////////////////////////////////////////////////////////////
// DTDEntityLexer

class DTDEntityLexer : public DTDLexer
{
public:
    DTDEntityLexer(CNcbiIstream& in, bool autoDelete=true);
    virtual ~DTDEntityLexer(void);
protected:
    bool m_AutoDelete;
    CNcbiIstream* m_Str;
};

/////////////////////////////////////////////////////////////////////////////
// DTDParser

class DTDParser : public AbstractParser
{
public:
    DTDParser( DTDLexer& lexer);
    virtual ~DTDParser(void);

    AutoPtr<CFileModules> Modules(const string& fileName);

protected:
    AutoPtr<CDataTypeModule> Module(const string& name);

    void BuildDocumentTree(void);

    void BeginElementContent(void);
    void ParseElementContent(const string& name, bool embedded);
    void ConsumeElementContent(DTDElement& node);
    void AddElementContent(DTDElement& node, string& id_name,
                           char separator=0);
    void EndElementContent(DTDElement& node);
    void FixEmbeddedNames(DTDElement& node);

    void BeginEntityContent(void);
    void PushEntityLexer(const string& name);
    bool PopEntityLexer(void);

    void GenerateDataTree(CDataTypeModule& module);
    void ModuleType(CDataTypeModule& module, const DTDElement& node);
    AutoPtr<CDataType> Type(const DTDElement& node,
                            DTDElement::EOccurrence occ, bool in_elem);
    CDataType* x_Type(const DTDElement& node,
                      DTDElement::EOccurrence occ, bool in_elem);
    CDataType* TypesBlock(CDataMemberContainerType* containerType,
                          const DTDElement& node);

#ifdef _DEBUG
    void PrintDocumentTree(void);
    void PrintEntities(void);
    void PrintDocumentNode(const string& name, DTDElement& node);
#endif
    map<string,DTDElement> m_MapElement;
    map<string,DTDEntity>  m_MapEntity;
    stack<AbstractLexer*>  m_StackLexer;
    stack<string>          m_StackPath;
};

END_NCBI_SCOPE

#endif // DTDPARSER_HPP


/*
 * ==========================================================================
 * $Log$
 * Revision 1.3  2002/10/21 16:10:55  gouriano
 * added parsing of external entities
 *
 * Revision 1.2  2002/10/18 14:35:42  gouriano
 * added parsing of internal parsed entities
 *
 * Revision 1.1  2002/10/15 13:50:15  gouriano
 * DTD lexer and parser. first version
 *
 *
 * ==========================================================================
 */
