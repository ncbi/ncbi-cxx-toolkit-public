#ifndef FILECODE_HPP
#define FILECODE_HPP

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
*   C++ file generator
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2000/03/29 15:51:41  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
*
* Revision 1.4  2000/03/17 16:47:38  vasilche
* Added copyright message to generated files.
* All objects pointers in choices now share the only CObject pointer.
*
* Revision 1.3  2000/02/17 21:26:22  vasilche
* Inline methods now will be at the end of *_Base.hpp files.
*
* Revision 1.2  2000/02/17 20:05:03  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.1  2000/02/01 21:46:18  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.5  1999/12/29 16:01:50  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.4  1999/12/20 21:00:17  vasilche
* Added generation of sources in different directories.
*
* Revision 1.3  1999/11/19 15:48:10  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.2  1999/11/15 19:36:15  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/tool/classctx.hpp>
#include <map>
#include <set>

USING_NCBI_SCOPE;

class CDataType;
class CTypeStrings;

class CFileCode : public CClassContext
{
public:
    typedef map<string, string> TForwards;
    typedef set<string> TAddedClasses;
    struct SClassInfo {
        string namespaceName;
        AutoPtr<CTypeStrings> code;
        SClassInfo(const string& namespaceName, AutoPtr<CTypeStrings> code);
    };
    typedef list< SClassInfo > TClasses;

    CFileCode(const string& baseName);
    ~CFileCode(void);

    bool AddType(const CDataType* type);

    string Include(const string& s) const;
    const string& GetFileBaseName(void) const
        {
            return m_BaseName;
        }
    const string& GetHeaderPrefix(void) const
        {
            return m_HeaderPrefix;
        }
    string GetUserFileBaseName(void) const;
    string GetBaseFileBaseName(void) const;
    string GetBaseHPPName(void) const;
    string GetBaseCPPName(void) const;
    string GetUserHPPName(void) const;
    string GetUserCPPName(void) const;
    string GetDefineBase(void) const;
    string GetBaseHPPDefine(void) const;
    string GetUserHPPDefine(void) const;

    string GetMethodPrefix(void) const;
    TIncludes& HPPIncludes(void);
    TIncludes& CPPIncludes(void);
    void AddForwardDeclaration(const string& className,
                               const string& namespaceName);
    void AddHPPCode(const CNcbiOstrstream& code);
    void AddINLCode(const CNcbiOstrstream& code);
    void AddCPPCode(const CNcbiOstrstream& code);

    void GenerateCode(void);
    void GenerateHPP(const string& path) const;
    void GenerateCPP(const string& path) const;
    bool GenerateUserHPP(const string& path) const;
    bool GenerateUserCPP(const string& path) const;

    CNcbiOstream& WriteSourceFile(CNcbiOstream& out) const;
    CNcbiOstream& WriteCopyrightHeader(CNcbiOstream& out) const;
    CNcbiOstream& WriteCopyright(CNcbiOstream& out) const;
    CNcbiOstream& WriteUserCopyright(CNcbiOstream& out) const;

private:
    // file names
    string m_BaseName;
    string m_HeaderPrefix;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;
    CNcbiOstrstream m_HPPCode, m_INLCode, m_CPPCode;

    set<string> m_SourceFiles;
    // classes code
    TAddedClasses m_AddedClasses;
    TClasses m_Classes;
    
    CFileCode(const CFileCode&);
    CFileCode& operator=(const CFileCode&);
};

class CNamespace
{
public:
    CNamespace(CNcbiOstream& out);
    ~CNamespace(void);
    
    void Set(const string& ns);

    void End(void);

private:
    void Begin(void);

    CNcbiOstream& m_Out;
    string m_Namespace;
};

#endif
