#ifndef MODULESET_HPP
#define MODULESET_HPP

#include <corelib/ncbistd.hpp>
#include <serial/typemapper.hpp>
#include <list>
#include <map>
#include "autoptr.hpp"

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
