#ifndef MODULE_HPP
#define MODULE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <list>
#include <map>
#include "autoptr.hpp"

USING_NCBI_SCOPE;

class CDataType;
class CModuleContainer;

class CDataTypeModule {
public:
    CDataTypeModule(const string& name);
    virtual ~CDataTypeModule();

    class Import {
    public:
        string moduleName;
        list<string> types;
    };
    typedef list< AutoPtr<Import> > TImports;
    typedef list< string > TExports;
    typedef list< pair< string, AutoPtr<CDataType> > > TDefinitions;

    bool Errors(void) const
        {
            return m_Errors;
        }

    const string& GetVar(const string& section, const string& value) const;
    const string& GetSourceFileName(void) const;
    
    void AddDefinition(const string& name, const AutoPtr<CDataType>& type);
    void AddExports(const TExports& exports);
    void AddImports(const TImports& imports);
    void AddImports(const string& module, const list<string>& types);

    virtual void PrintASN(CNcbiOstream& out) const;

    bool Check();
    bool CheckNames();

    const string& GetName(void) const
        {
            return m_Name;
        }
    const CModuleContainer& GetModuleContainer(void) const
        {
            _ASSERT(m_ModuleContainer != 0);
            return *m_ModuleContainer;
        }
    void SetModuleContainer(const CModuleContainer* container);
    const TDefinitions& GetDefinitions(void) const
        {
            return m_Definitions;
        }

    // return type visible from inside, or throw CTypeNotFound if none
    CDataType* InternalResolve(const string& name) const;
    // return type visible from outside, or throw CTypeNotFound if none
    CDataType* ExternalResolve(const string& name,
                               bool allowInternal = false) const;

private:
    bool m_Errors;
    string m_Name;

    const CModuleContainer* m_ModuleContainer;

    TExports m_Exports;
    TImports m_Imports;
    TDefinitions m_Definitions;

    typedef map<string, CDataType*> TTypesByName;
    typedef map<string, string> TImportsByName;

    TTypesByName m_LocalTypes;
    TTypesByName m_ExportedTypes;
    TImportsByName m_ImportedTypes;
};

#endif
