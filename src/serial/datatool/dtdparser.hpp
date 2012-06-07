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
*   DTD parser
*
* ===========================================================================
*/

#include <corelib/ncbiutil.hpp>
#include "aparser.hpp"
#include "dtdlexer.hpp"
#include "dtdaux.hpp"
#include "moduleset.hpp"
#include <map>
#include <stack>
#include <set>

BEGIN_NCBI_SCOPE

class CFileModules;
class CDataModule;
class CDataType;
class CDataMemberContainerType;
class CDataValue;
class CDataMember;
class CEnumDataType;
class CEnumDataTypeValue;
class CComments;



/////////////////////////////////////////////////////////////////////////////
// DTDParser

class DTDParser : public AbstractParser
{
public:
    DTDParser( DTDLexer& lexer);
    virtual ~DTDParser(void);

    enum ESrcType {
        eDTD,
        eSchema,
        eWsdl
    };
        
    AutoPtr<CFileModules> Modules(const string& fileName);
    virtual void EndCommentBlock(void);

protected:
    void Module(AutoPtr<CFileModules>& modules, const string& name);

    virtual void BeginDocumentTree(void);
    virtual void BuildDocumentTree(CDataTypeModule& module);
    virtual void BuildDataTree(AutoPtr<CFileModules>& modules,
                               AutoPtr<CDataTypeModule>& module);
    void SkipConditionalSection(void);

    virtual string GetLocation(void);

    TToken GetNextToken(void);
    string GetNextTokenText(void);
    void   ConsumeToken(void);

    void BeginElementContent(void);
    void ParseElementContent(const string& name, bool embedded);
    void ConsumeElementContent(DTDElement& node);
    void AddElementContent(DTDElement& node, string& id_name,
                           char separator=0);
    void EndElementContent(DTDElement& node);
    string CreateEmbeddedName(const DTDElement& node, int depth) const;
    void FixEmbeddedNames(DTDElement& node);

    void BeginEntityContent(void);
    void ParseEntityContent(const string& name);
    virtual DTDEntity* PushEntityLexer(const string& name);
    virtual bool PopEntityLexer(void);
    virtual AbstractLexer* CreateEntityLexer(
        CNcbiIstream& in, const string& name, bool autoDelete=true);

    void BeginAttributesContent(void);
    void ParseAttributesContent(DTDElement& node);
    void ConsumeAttributeContent(DTDElement& node, const string& id_name);
    void ParseEnumeratedList(DTDAttribute& attrib);

    void GenerateDataTree(CDataTypeModule& module, const string& name_space);
    void ModuleType(CDataTypeModule& module, const DTDElement& node);
    AutoPtr<CDataType> Type(const DTDElement& node,
                            DTDElement::EOccurrence occ,
                            bool fromInside, bool ignoreAttrib=false);
    CDataType* x_Type(const DTDElement& node,
                      DTDElement::EOccurrence occ,
                      bool fromInside, bool ignoreAttrib=false);
    AutoPtr<CDataValue> Value(const DTDElement& node);
    AutoPtr<CDataValue> x_Value(const DTDElement& node);
    CDataType* TypesBlock(CDataMemberContainerType* containerType,
                          const DTDElement& node, bool ignoreAttrib=false);
    CDataType* CompositeNode(const DTDElement& node,
                             DTDElement::EOccurrence occ);
    void AddAttributes(AutoPtr<CDataMemberContainerType>& container,
                       const DTDElement& node);
    CDataType* AttribBlock(const DTDElement& node);
    CDataType* x_AttribType(const DTDAttribute& att);
    CDataValue* x_AttribValue(const DTDAttribute& att, const string& value);
    CDataType* EnumeratedBlock(const DTDAttribute& att,
                               CEnumDataType* enumType);
    void SetCommentsIfEmpty(CComments* comments);

#if defined(NCBI_DTDPARSER_TRACE)
    virtual void PrintDocumentTree(void);
    void PrintEntities(void);
    void PrintDocumentNode(const string& name, const DTDElement& node);
    void PrintNodeAttributes(const DTDElement& node);
    void PrintAttribute(const DTDAttribute& attrib, bool indent=true);
#endif
    map<string,DTDElement> m_MapElement;
    map<string,DTDEntity>  m_MapEntity;
    stack<AbstractLexer*>  m_StackLexer;
    stack<string>          m_StackPath;
    list<string>           m_StackLexerName;
    string                 m_IdentifierText;
    set<string>            m_GeneratedTypes;
    list<string>           m_ElementEmbTypes;
    ESrcType  m_SrcType;
    CComments* m_Comments;
    bool m_ExpectLastComment;
    static const string& s_SpecialName;
};

END_NCBI_SCOPE

#endif // DTDPARSER_HPP
