#include "moduleset.hpp"
#include "module.hpp"
#include "type.hpp"
#include "exceptions.hpp"

CModuleSet::CModuleSet(void)
{
}

CModuleSet::~CModuleSet(void)
{
}

void CModuleSet::SetMainTypes(void)
{
    for ( TModules::iterator mi = modules.begin();
          mi != modules.end(); ++mi ) {
        ASNModule* module = mi->second.get();
        for ( ASNModule::TDefinitions::iterator di = module->definitions.begin();
              di != module->definitions.end();
              ++di ) {
            (*di)->main = true;
        }
    }
}

bool CModuleSet::Check(void) const
{
    bool ok = true;
    for ( TModules::const_iterator mi = modules.begin();
          mi != modules.end(); ++mi ) {
        if ( !mi->second->Check() )
            ok = false;
    }
    return ok;
}

TTypeInfo CModuleSet::MapType(const string& n)
{
    string name(n.empty()? rootTypeName: n);
    // find cached typeinfo
    TTypes::const_iterator ti = m_Types.find(name);
    if ( ti != m_Types.end() )
        return ti->second;

    // find type definition
    for ( TModules::const_iterator i = modules.begin();
          i != modules.end(); ++i ) {
        ASNModule* module = i->second.get();
        const ASNModule::TypeInfo* typeInfo = module->FindType(name);
        if ( typeInfo && typeInfo->type && typeInfo->type->exported )
            return (m_Types[name] = typeInfo->type->GetTypeInfo());
    }
    THROW1_TRACE(runtime_error, "type not found: " + name);
}

ASNType* CModuleSet::ResolveFull(const string& fullName) const
{
    SIZE_TYPE dot = fullName.find('.');
    if ( dot != NPOS ) {
        // module specified
        return Resolve(fullName.substr(0, dot), fullName.substr(dot + 1));
    }

    // module not specified - we'll scan all modules for type
    const ASNModule::TypeInfo* type = 0;
    for ( TModules::const_iterator i = modules.begin();
          i != modules.end(); ++i ) {
        ASNModule* module = i->second.get();
        const ASNModule::TypeInfo* t = module->FindType(fullName);
        if ( t && t->type && t->type->main ) {
            if ( type == 0 )
                type = t;
            else {
                THROW1_TRACE(CTypeNotFound, "ambiguous type: " + fullName);
            }
        }
    }
    if ( !type )
        THROW1_TRACE(CTypeNotFound, "type not found: " + fullName);
    return type->type;
}

ASNType* CModuleSet::Resolve(const string& moduleName,
                             const string& typeName) const
{
    // find module definition
    TModules::const_iterator mi = modules.find(moduleName);
    if ( mi != modules.end() ) {
        ASNModule* module = mi->second.get();
        const ASNModule::TypeInfo* t = module->FindType(typeName);
        if ( t && t->type ) {
            if ( !t->type->exported )
                ERR_POST("not exported: " + moduleName + "." + typeName);
            return t->type;
        }
        THROW1_TRACE(CTypeNotFound,
                     "type not found: " + moduleName + '.' + typeName);
    }
    // no such module
    THROW1_TRACE(CModuleNotFound, "module not found: " + moduleName);
}
