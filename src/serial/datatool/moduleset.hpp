#ifndef MODULESET_HPP
#define MODULESET_HPP

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
*   Module set: equivalent of ASN.1 source file
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/serialdef.hpp>
#include "mcontainer.hpp"
#include "comments.hpp"
#include <list>
#include <map>
#include <set>

BEGIN_NCBI_SCOPE

class CDataType;
class CDataTypeModule;
class CFileModules;
class CFileSet;

class CFileModules : public CModuleContainer
{
public:
    typedef list< AutoPtr<CDataTypeModule> > TModules;
    typedef map<string, CDataTypeModule*> TModulesByName;

    CFileModules(const string& fileName);

    bool Check(void) const;
    bool CheckNames(void) const;

    void PrintSampleDEF(const string& rootdir) const;
    void PrintASN(CNcbiOstream& out) const;
    void PrintSpecDump(CNcbiOstream& out) const;
    void PrintXMLSchema(CNcbiOstream& out) const;

    void GetRefInfo(list<string>& info) const;
    void PrintASNRefInfo(CNcbiOstream& out) const;
    void PrintXMLRefInfo(CNcbiOstream& out) const;

    void PrintDTD(CNcbiOstream& out) const;
    void PrintDTDModular(void) const;

    void PrintXMLSchemaModular(void) const;
    void BeginXMLSchema(CNcbiOstream& out) const;
    void EndXMLSchema(CNcbiOstream& out) const;

    const string& GetSourceFileName(void) const;
    string GetFileNamePrefix(void) const;

    void AddModule(const AutoPtr<CDataTypeModule>& module);

    const TModules& GetModules(void) const
        {
            return m_Modules;
        }

    CDataType* ExternalResolve(const string& moduleName,
                               const string& typeName,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& fullName,
                                  bool allowInternal = false) const;
    void CollectAllTypeinfo(set<TTypeInfo>& types) const;

    CComments& LastComments(void)
        {
            return m_LastComments;
        }

private:
    TModules m_Modules;
    TModulesByName m_ModulesByName;
    string m_SourceFileName;
    CComments m_LastComments;
    mutable string m_PrefixFromSourceFileName;

    friend class CFileSet;
};

class CFileSet : public CModuleContainer
{
public:
    typedef list< AutoPtr< CFileModules > > TModuleSets;

    void AddFile(const AutoPtr<CFileModules>& moduleSet);

    const TModuleSets& GetModuleSets(void) const
        {
            return m_ModuleSets;
        }
    TModuleSets& GetModuleSets(void)
        {
            return m_ModuleSets;
        }

    bool Check(void) const;
    bool CheckNames(void) const;

    void PrintSampleDEF(const string& rootdir) const;
    void PrintASN(CNcbiOstream& out) const;
    void PrintSpecDump(CNcbiOstream& out) const;
    void PrintXMLSchema(CNcbiOstream& out) const;
    void PrintDTD(CNcbiOstream& out) const;

    void PrintDTDModular(void) const;
    void PrintXMLSchemaModular(void) const;

    CDataType* ExternalResolve(const string& moduleName,
                               const string& typeName,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& fullName,
                                  bool allowInternal = false) const;

    void CollectAllTypeinfo(set<TTypeInfo>& types) const;
private:
    TModuleSets m_ModuleSets;
};

END_NCBI_SCOPE

#endif
