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
* Revision 1.7  1999/11/15 19:36:15  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <set>
#include <map>
#include "moduleset.hpp"

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

    void IncludeTypes(const string& types);
    void ExcludeTypes(const string& types);
    void ExcludeRecursion(bool exclude = true)
        {
            m_ExcludeRecursion = exclude;
        }
    void IncludeAllMainTypes(void);

    void SetSourcesDir(const string& dir)
        {
            m_SourcesDir = dir;
        }
    void SetHeadersDir(const string& dir)
        {
            m_HeadersDir = dir;
        }
    void SetFileListFileName(const string& file)
        {
            m_FileListFileName = file;
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
    const string& GetSourceFileName(void) const;
    CDataType* InternalResolve(const string& moduleName,
                               const string& typeName) const;

    CDataType* ExternalResolve(const string& module, const string& type,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& type,
                                  bool allowInternal = false) const;

    CDataType* ResolveMain(const string& fullName) const;

protected:

    static void GetTypes(TTypeNames& typeNames, const string& name);

    enum EContext {
        eOther,
        eRoot,
        eChoice,
        eReference
    };
    void CollectTypes(const CDataType* type, EContext context = eOther );
    bool AddType(const CDataType* type);

private:

    CNcbiRegistry m_Config;
    CFileSet m_MainFiles;
    CFileSet m_ImportFiles;
    TTypeNames m_GenerateTypes;
    bool m_ExcludeRecursion;
    string m_FileListFileName;
    string m_HeadersDir;
    string m_SourcesDir;
    string m_HeaderPrefix;

    TOutputFiles m_Files;
};

#endif
