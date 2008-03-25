#ifndef GENERATE_HPP
#define GENERATE_HPP

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
*   Main generator: collects types, classes and files.
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <set>
#include <map>
#include "moduleset.hpp"
#include "namespace.hpp"

BEGIN_NCBI_SCOPE

class CFileCode;

class CCodeGenerator : public CModuleContainer
{
public:
    typedef set<string> TTypeNames;
    typedef map<string, AutoPtr<CFileCode> > TOutputFiles;

    CCodeGenerator(void);
    ~CCodeGenerator(void);

    // setup interface
    void LoadConfig(CNcbiIstream& in);
    void LoadConfig(const string& fileName, bool ignoreAbsense = false,
                    bool warningAbsense = true);
    void AddConfigLine(const string& s);

    void IncludeTypes(const string& types);
    void ExcludeTypes(const string& types);
    void ExcludeRecursion(bool exclude = true)
        {
            m_ExcludeRecursion = exclude;
        }
    void IncludeAllMainTypes(void);
    bool HaveGenerateTypes(void) const
        {
            return !m_GenerateTypes.empty();
        }

    void SetCPPDir(const string& dir)
        {
            m_CPPDir = dir;
        }
    const string& GetCPPDir(void) const
        {
            return m_CPPDir;
        }
    void SetHPPDir(const string& dir)
        {
            m_HPPDir = dir;
        }
    void SetFileListFileName(const string& file)
        {
            m_FileListFileName = file;
        }
    void SetCombiningFileName(const string& file)
        {
            m_CombiningFileName = file;
        }

    CFileSet& GetMainModules(void)
        {
            return m_MainFiles;
        }
    const CFileSet& GetMainModules(void) const
        {
            return m_MainFiles;
        }
    CFileSet& GetImportModules(void)
        {
            return m_ImportFiles;
        }
    const CFileSet& GetImportModules(void) const
        {
            return m_ImportFiles;
        }
    const string& GetDefFile(void) const
        {
            return m_DefFile;
        }
    void SetRootDir(const string& dir)
        {
            m_RootDir = dir;
        }
    const string& GetRootDir(void) const
        {
            return m_RootDir;
        }

    bool Check(void) const;

    void CheckFileNames(void);
    void GenerateCode(void);
    void GenerateDoxygenGroupDescription(map<string, pair<string,string> >& module_names);
    void GenerateFileList(const list<string>& generated, const list<string>& untouched,
        list<string>& allGeneratedHpp, list<string>& allGeneratedCpp,
        list<string>& allSkippedHpp, list<string>& allSkippedCpp);
    void GenerateCombiningFile(const list<string>& module_inc, const list<string>& module_src,
        list<string>& allHpp, list<string>& allCpp);
    void GenerateCvsignore(const string& outdir_cpp, const string& outdir_hpp,
        const list<string>& generated, map<string, pair<string,string> >& module_names);
    void GenerateModuleHPP(const string& path, list<string>& generated) const;
    void GenerateModuleCPP(const string& path, list<string>& generated) const;

    void GenerateClientCode(void);
    void GenerateClientCode(const string& name, bool mandatory);

    bool Imported(const CDataType* type) const;

    // generation interface
    const CNcbiRegistry& GetConfig(void) const;
    string GetFileNamePrefix(void) const;
    void UseQuotedForm(bool use);
    void CreateCvsignore(bool create);
    void SetFileNamePrefix(const string& prefix);
    EFileNamePrefixSource GetFileNamePrefixSource(void) const;
    void SetFileNamePrefixSource(EFileNamePrefixSource source);
    CDataType* InternalResolve(const string& moduleName,
                               const string& typeName) const;

    void SetDefaultNamespace(const string& ns);
    const CNamespace& GetNamespace(void) const;

    CDataType* ExternalResolve(const string& module, const string& type,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& type,
                                  bool allowInternal = false) const;

    CDataType* ResolveMain(const string& fullName) const;
    const string& ResolveFileName(const string& name) const;

    void SetDoxygenIngroup(const string& str)
        {
            m_DoxygenIngroup = str;
        }
    void SetDoxygenGroupDescription(const string& str)
        {
            m_DoxygenGroupDescription = str;
        }
    void ResolveImportRefs(void);
    void ResolveImportRefs(CDataTypeModule& head, const CDataTypeModule* ref);
    const CDataTypeModule* FindModuleByName(const string& name) const;

protected:

    static void GetTypes(TTypeNames& typeNames, const string& name);

    enum EContext {
        eRoot,
        eChoice,
        eReference,
        eElement,
        eMember
    };
    void CollectTypes(const CDataType* type, EContext context );
    bool AddType(const CDataType* type);

private:

    CNcbiRegistry m_Config;
    CFileSet m_MainFiles;
    CFileSet m_ImportFiles;
    TTypeNames m_GenerateTypes;
    bool m_ExcludeRecursion;
    string m_FileListFileName;
    string m_CombiningFileName;
    string m_HPPDir;
    string m_CPPDir;
    string m_FileNamePrefix;
    EFileNamePrefixSource m_FileNamePrefixSource;
    CNamespace m_DefaultNamespace;
    bool m_UseQuotedForm;
    bool m_CreateCvsignore;
    string m_DoxygenIngroup;
    string m_DoxygenGroupDescription;
    string m_DefFile;
    string m_RootDir;

    TOutputFiles m_Files;
};

END_NCBI_SCOPE

#endif
