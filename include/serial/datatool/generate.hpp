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
*
* ---------------------------------------------------------------------------
* $Log$
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
    void LoadConfig(const string& fileName);
    void AddConfigLine(const string& s);

    void IncludeTypes(const string& types);
    void ExcludeTypes(const string& types);
    void ExcludeRecursion(bool exclude = true)
        {
            m_ExcludeRecursion = exclude;
        }
    void IncludeAllMainTypes(void);

    void SetCPPDir(const string& dir)
        {
            m_CPPDir = dir;
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
    CFileSet& GetImportModules(void)
        {
            return m_ImportFiles;
        }

    bool Check(void) const;

    void GenerateCode(void);

    bool Imported(const CDataType* type) const;

    // generation interface
    const CNcbiRegistry& GetConfig(void) const;
    string GetFileNamePrefix(void) const;
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

    TOutputFiles m_Files;
};

END_NCBI_SCOPE

#endif
