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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  1999/12/20 21:00:19  vasilche
* Added generation of sources in different directories.
*
* Revision 1.9  1999/11/19 15:48:10  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.8  1999/11/15 19:36:17  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/typemapper.hpp>
#include <list>
#include <map>

class CDataType;
class CDataTypeModule;

BEGIN_NCBI_SCOPE

class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class CModuleContainer;
class CModuleSet;
class CFileSet;

enum EHeadersDirNameSource {
    eFromNone,
    eFromSourceFileName,
    eFromModuleName
};

class CModuleContainer
{
public:
    typedef map<string, AutoPtr<CDataTypeModule> > TModules;

    CModuleContainer(void);
    virtual ~CModuleContainer(void);

    virtual const CNcbiRegistry& GetConfig(void) const;
    virtual const string& GetSourceFileName(void) const;
    virtual const string& GetHeadersPrefix(void) const;
    virtual EHeadersDirNameSource GetHeadersDirNameSource(void) const;
    virtual CDataType* InternalResolve(const string& moduleName,
                                       const string& typeName) const;

	void SetModuleContainer(const CModuleContainer* parent);
	const CModuleContainer& GetModuleContainer(void) const;
private:
    const CModuleContainer* m_Parent;
};

class CFileSet : public CModuleContainer
{
public:
    typedef list< AutoPtr< CModuleSet > > TModuleSets;

    void AddFile(const AutoPtr<CModuleSet>& moduleSet);

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

    void PrintASN(CNcbiOstream& out) const;

    CDataType* ExternalResolve(const string& moduleName,
                               const string& typeName,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& fullName,
                                  bool allowInternal = false) const;

private:
    TModuleSets m_ModuleSets;
};

class CModuleSet : public CModuleContainer
{
public:
    CModuleSet(const string& fileName);

    bool Check(void) const;
    bool CheckNames(void) const;

    void PrintASN(CNcbiOstream& out) const;

    const string& GetSourceFileName(void) const;

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

private:
    TModules m_Modules;
    string m_SourceFileName;

    friend class CFileSet;
};

#endif
