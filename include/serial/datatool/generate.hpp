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
#include <serial/datatool/moduleset.hpp>
#include <serial/datatool/namespace.hpp>

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

    void GenerateCode(void);
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

/*
* ===========================================================================
*
* $Log$
* Revision 1.12  2004/04/29 20:09:44  gouriano
* Generate DOXYGEN-style comments in C++ headers
*
* Revision 1.11  2003/05/29 17:22:59  gouriano
* added possibility of generation .cvsignore file
*
* Revision 1.10  2003/04/08 20:40:08  ucko
* Get client name(s) from [-]clients rather than hardcoding "client"
*
* Revision 1.9  2003/02/24 21:56:38  gouriano
* added odw flag - to issue a warning about missing DEF file
*
* Revision 1.8  2002/12/17 16:21:20  gouriano
* separated class name from the name of the file in which it will be written
*
* Revision 1.7  2002/11/13 00:46:06  ucko
* Add RPC client generator; CVS logs to end in generate.?pp
*
* Revision 1.6  2002/10/22 15:07:00  gouriano
* added possibillity to use quoted syntax form for generated include files
*
* Revision 1.5  2000/11/27 18:19:31  vasilche
* Datatool now conforms CNcbiApplication requirements.
*
* Revision 1.4  2000/08/25 15:58:46  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.3  2000/06/16 16:31:13  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.2  2000/04/07 19:26:09  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:19  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.11  1999/12/28 18:55:58  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.10  1999/12/21 17:18:34  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.9  1999/12/20 21:00:18  vasilche
* Added generation of sources in different directories.
*
* Revision 1.8  1999/12/09 20:01:23  vasilche
* Fixed bug with missed internal classes.
*
* Revision 1.7  1999/11/15 19:36:15  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#endif
