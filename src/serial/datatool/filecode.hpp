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
#include "classctx.hpp"
#include "namespace.hpp"
#include "typestr.hpp"
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
    typedef map<string, set<string> > TForwards;
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
    CTypeStrings* GetPrimaryClass(void);

    bool GetClasses(list<CTypeStrings*>& types);
    CNamespace GetClassNamespace(CTypeStrings* type);

    CNcbiOstream& WriteSourceFile(CNcbiOstream& out) const;
    static CNcbiOstream& WriteCopyrightHeader(CNcbiOstream& out);
    CNcbiOstream& WriteSpecRefs(CNcbiOstream& out) const;
    CNcbiOstream& WriteCopyright(CNcbiOstream& out, bool header) const;
    CNcbiOstream& WriteUserCopyright(CNcbiOstream& out, bool header) const;
    static CNcbiOstream& WriteLogKeyword(CNcbiOstream& out);

    void GetModuleNames( map<string, pair<string,string> >& names) const;

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

#endif
