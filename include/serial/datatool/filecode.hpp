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
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/datatool/classctx.hpp>
#include <serial/datatool/namespace.hpp>
#include <serial/datatool/typestr.hpp>
#include <map>
#include <set>
#include <list>

BEGIN_NCBI_SCOPE

class CDataType;
class CTypeStrings;
class CCodeGenerator;

class CFileCode : public CClassContext
{
public:
    typedef map<string, CNamespace> TForwards;
    typedef set<string> TAddedClasses;
    struct SClassInfo {
        SClassInfo(const CNamespace& classNamespace,
                   AutoPtr<CTypeStrings> classCode)
            : ns(classNamespace), code(classCode)
            {
            }

        CNamespace ns;
        AutoPtr<CTypeStrings> code;
        string hppCode, inlCode, cppCode;
    };
    typedef list< SClassInfo > TClasses;

    CFileCode(const CCodeGenerator* codeGenerator,const string& baseName);
    ~CFileCode(void);

    const CNamespace& GetNamespace(void) const;

    bool AddType(const CDataType* type);

    string Include(const string& s, bool addExt=false) const;
    const string& GetFileBaseName(void) const
        {
            return m_BaseName;
        }
    const string& ChangeFileBaseName(void);
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
    void AddForwardDeclaration(const string& className, const CNamespace& ns);
    void AddHPPCode(const CNcbiOstrstream& code);
    void AddINLCode(const CNcbiOstrstream& code);
    void AddCPPCode(const CNcbiOstrstream& code);

    void UseQuotedForm(bool use);
    void CreateFileFolder(const string& fileName) const;
    void GenerateCode(void);
    void GenerateHPP(const string& path, string& fileName) const;
    void GenerateCPP(const string& path, string& fileName) const;
    bool GenerateUserHPP(const string& path, string& fileName) const;
    bool GenerateUserCPP(const string& path, string& fileName) const;

    CNcbiOstream& WriteSourceFile(CNcbiOstream& out) const;
    static CNcbiOstream& WriteCopyrightHeader(CNcbiOstream& out);
    CNcbiOstream& WriteSpecRefs(CNcbiOstream& out) const;
    CNcbiOstream& WriteCopyright(CNcbiOstream& out, bool header) const;
    CNcbiOstream& WriteUserCopyright(CNcbiOstream& out, bool header) const;
    static CNcbiOstream& WriteLogKeyword(CNcbiOstream& out);

    void GetModuleNames( map<string,string>& names) const;

    static void SetPchHeader(const string& name)
        {
            m_PchHeader = name;
        }
    static const string& GetPchHeader(void)
        {
            return m_PchHeader;
        }
private:
    const CCodeGenerator* m_CodeGenerator;
    bool m_UseQuotedForm;
    // file names
    string m_BaseName;
    string m_HeaderPrefix;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;
    SClassInfo* m_CurrentClass;

    set<string> m_SourceFiles;
    // classes code
    TAddedClasses m_AddedClasses;
    TClasses m_Classes;
    static string m_PchHeader;
    
    CFileCode(const CFileCode&);
    CFileCode& operator=(const CFileCode&);

    void GenerateUserHPPCode(CNcbiOstream& code) const;
    void GenerateUserCPPCode(CNcbiOstream& code) const;

    typedef void (CFileCode::* TGenerateMethod)(CNcbiOstream& out) const;
    bool WriteUserFile(const string& path, const string& name,
                       string& fileName, TGenerateMethod method) const;
    void LoadLines(TGenerateMethod method, list<string>& lines) const;
    bool ModifiedByUser(const string& fileName,
                        const list<string>& newLines) const;
};

END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.18  2004/05/19 15:45:46  gouriano
* Add precompiled header into combining files as well
*
* Revision 1.17  2004/05/17 14:51:23  gouriano
* Added possibility to include precompiled header
*
* Revision 1.16  2004/04/29 20:09:44  gouriano
* Generate DOXYGEN-style comments in C++ headers
*
* Revision 1.15  2003/05/29 17:22:59  gouriano
* added possibility of generation .cvsignore file
*
* Revision 1.14  2002/12/17 16:21:20  gouriano
* separated class name from the name of the file in which it will be written
*
* Revision 1.13  2002/10/22 15:07:01  gouriano
* added possibillity to use quoted syntax form for generated include files
*
* Revision 1.12  2002/10/01 14:21:10  gouriano
* added more generation report data
*
* Revision 1.11  2002/06/10 18:41:25  ucko
* Move CVS logs (both internal and generated) to the end.
*
* Revision 1.10  2000/11/22 16:26:22  vasilche
* Added generation/checking of checksum to user files.
*
* Revision 1.9  2000/08/25 15:58:46  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.8  2000/04/17 19:11:05  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.7  2000/04/12 15:36:41  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.6  2000/04/07 19:26:09  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
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

#endif
