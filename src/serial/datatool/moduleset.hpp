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

class CModuleContainer
{
public:
    typedef map<string, AutoPtr<CDataTypeModule> > TModules;

    virtual ~CModuleContainer(void);

    virtual const CNcbiRegistry& GetConfig(void) const = 0;
    virtual const string& GetSourceFileName(void) const = 0;
    virtual CDataType* InternalResolve(const string& moduleName,
                                       const string& typeName) const = 0;

    
};

class CFileSet : public CModuleContainer
{
public:
    typedef list< AutoPtr< CModuleSet > > TModuleSets;

    CFileSet(void);
	
	const CModuleContainer& GetModuleContainer(void) const
		{
			_ASSERT(m_Parent != 0);
			return *m_Parent;
		}
	void SetModuleContainer(const CModuleContainer* parent)
		{
			_ASSERT(m_Parent == 0 && parent != 0);
			m_Parent = parent;
		}

    void AddFile(const AutoPtr<CModuleSet>& moduleSet);

    const CNcbiRegistry& GetConfig(void) const;
    const string& GetSourceFileName(void) const;
    CDataType* InternalResolve(const string& moduleName,
                               const string& typeName) const;

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
    const CModuleContainer* m_Parent;
    TModuleSets m_ModuleSets;
};

class CModuleSet : public CModuleContainer
{
public:
    CModuleSet(const string& fileName);

    bool Check(void) const;
    bool CheckNames(void) const;

    void PrintASN(CNcbiOstream& out) const;

    const CNcbiRegistry& GetConfig(void) const;
    const string& GetSourceFileName(void) const;

    void AddModule(const AutoPtr<CDataTypeModule>& module);

    const CModuleContainer& GetModuleContainer(void) const
        {
            _ASSERT(m_ModuleContainer != 0);
            return *m_ModuleContainer;
        }

    const TModules& GetModules(void) const
        {
            return m_Modules;
        }

    CDataType* InternalResolve(const string& moduleName,
                               const string& typeName) const;

    CDataType* ExternalResolve(const string& moduleName,
                               const string& typeName,
                               bool allowInternal = false) const;
    CDataType* ResolveInAnyModule(const string& fullName,
                                  bool allowInternal = false) const;

private:
    TModules m_Modules;
    string m_SourceFileName;
    const CFileSet* m_ModuleContainer;

    friend class CFileSet;
};

#endif
