#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbireg.hpp>
#include <typeinfo>
#include "module.hpp"
#include "moduleset.hpp"
#include "exceptions.hpp"
#include "type.hpp"

void Warning(const string& message)
{
    CNcbiDiag() << Warning << message;
}

CDataTypeModule::CDataTypeModule(const string& n)
    : m_ModuleContainer(0), m_Name(n), m_Errors(false)
{
}

CDataTypeModule::~CDataTypeModule()
{
}

void CDataTypeModule::SetModuleContainer(const CModuleContainer* container)
{
    _ASSERT(m_ModuleContainer == 0 && container != 0);
    m_ModuleContainer = container;
}

void CDataTypeModule::AddDefinition(const string& name,
                                    const AutoPtr<CDataType>& type)
{
    CDataType*& oldType = m_LocalTypes[name];
    if ( oldType ) {
        Warning(type->LocationString() + ": redefinition, original: " +
                oldType->LocationString());
        m_Errors = true;
        return;
    }
    oldType = type.get();
    m_Definitions.push_back(make_pair(name, type));
}

void CDataTypeModule::AddExports(const TExports& exports)
{
    m_Exports.insert(m_Exports.end(), exports.begin(), exports.end());
}

void CDataTypeModule::AddImports(const TImports& imports)
{
    m_Imports.insert(m_Imports.end(), imports.begin(), imports.end());
}

void CDataTypeModule::AddImports(const string& module, const list<string>& types)
{
    AutoPtr<Import> import(new Import());
    import->moduleName = module;
    import->types.insert(import->types.end(), types.begin(), types.end());
    m_Imports.push_back(import);
}

void CDataTypeModule::PrintASN(CNcbiOstream& out) const
{
    out << GetName() << " DEFINITIONS ::=" << NcbiEndl;
    out << "BEGIN" << NcbiEndl << NcbiEndl;

    if ( !m_Exports.empty() ) {
        out << "EXPORTS ";
        for ( TExports::const_iterator begin = m_Exports.begin(),
                  end = m_Exports.end(), i = begin; i != end; ++i ) {
            if ( i != begin )
                out << ", ";
            out << *i;
        }
        out << ';' << NcbiEndl << NcbiEndl;
    }

    if ( !m_Imports.empty() ) {
        out << "IMPORTS ";
        for ( TImports::const_iterator mbegin = m_Imports.begin(),
                  mend = m_Imports.end(), m = mbegin; m != mend; ++m ) {
            if ( m != mbegin )
                out << NcbiEndl << "        ";

            const Import& imp = **m;
            for ( list<string>::const_iterator begin = imp.types.begin(),
                  end = imp.types.end(), i = begin; i != end; ++i ) {
                if ( i != begin )
                    out << ", ";
                out << *i;
            }
            out << " FROM " << imp.moduleName;
        }
        out << ';' << NcbiEndl << NcbiEndl;
    }

    for ( TDefinitions::const_iterator i = m_Definitions.begin();
          i != m_Definitions.end(); ++i ) {
        out << i->first << " ::= ";
        i->second->PrintASN(out, 0);
        out << NcbiEndl << NcbiEndl;
    }

    out << "END" << NcbiEndl << NcbiEndl;
}

bool CDataTypeModule::Check()
{
    bool ok = true;
    for ( TDefinitions::const_iterator d = m_Definitions.begin();
          d != m_Definitions.end(); ++d ) {
        if ( !d->second->Check() )
            ok = false;
    }
    return ok;
}

bool CDataTypeModule::CheckNames()
{
    iterate ( TDefinitions, d, m_Definitions ) {
        d->second->SetParent(this, d->first);
    }
    bool ok = true;
    for ( TExports::const_iterator e = m_Exports.begin();
          e != m_Exports.end(); ++e ) {
        const string& name = *e;
        TTypesByName::iterator it = m_LocalTypes.find(name);
        if ( it == m_LocalTypes.end() ) {
            Warning("undefined export type: " + name);
            ok = false;
        }
        else {
            m_ExportedTypes[name] = it->second;
        }
    }
    for ( TImports::const_iterator i = m_Imports.begin();
          i != m_Imports.end(); ++i ) {
        const Import& imp = **i;
        const string& module = imp.moduleName;
        for ( list<string>::const_iterator t = imp.types.begin();
              t != imp.types.end(); ++t ) {
            const string& name = *t;
            if ( m_LocalTypes.find(name) != m_LocalTypes.end() ) {
                Warning("import conflicts with local defenition: " + name);
                ok = false;
                continue;
            }
            pair<TImportsByName::iterator, bool> ins =
                m_ImportedTypes.insert(TImportsByName::value_type(name, module));
            if ( !ins.second ) {
                Warning("duplicated import: " + name);
                ok = false;
                continue;
            }
        }
    }
    return ok;
}

CDataType* CDataTypeModule::ExternalResolve(const string& typeName,
                                            bool allowInternal) const
{
    const TTypesByName& types(allowInternal? m_LocalTypes: m_ExportedTypes);
    TTypesByName::const_iterator t = types.find(typeName);
    if ( t != types.end() )
        return t->second;

    if ( !allowInternal && m_LocalTypes.find(typeName) != m_LocalTypes.end() ) {
        THROW1_TRACE(CTypeNotFound, "not exported type: " + typeName);
    }

    THROW1_TRACE(CTypeNotFound, "undefined type: " + typeName);
}

CDataType* CDataTypeModule::InternalResolve(const string& typeName) const
{
    TTypesByName::const_iterator t = m_LocalTypes.find(typeName);
    if ( t != m_LocalTypes.end() )
        return t->second;
    TImportsByName::const_iterator i = m_ImportedTypes.find(typeName);
    if ( i != m_ImportedTypes.end() )
        return GetModuleContainer().InternalResolve(i->second, typeName);
    THROW1_TRACE(CTypeNotFound, "undefined type: " + typeName);
}

const string& CDataTypeModule::GetSourceFileName(void) const
{
    return GetModuleContainer().GetSourceFileName();
}

const string& CDataTypeModule::GetVar(const string& section,
                                      const string& value) const
{
    _ASSERT(!section.empty());
    _ASSERT(!value.empty());
    const CNcbiRegistry& registry = GetModuleContainer().GetConfig();
    const string& s1 = registry.Get(GetName() + '.' + section, value);
    if ( !s1.empty() )
        return s1;
    const string& s2 = registry.Get(section, value);
    if ( !s2.empty() )
        return s2;
    return registry.Get(GetName(), value);
}

