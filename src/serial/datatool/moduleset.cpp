#include "moduleset.hpp"
#include "module.hpp"
#include "type.hpp"

CModuleSet::CModuleSet(void)
{
}

CModuleSet::~CModuleSet(void)
{
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
        ASNModule* module = (*i).get();
        const ASNModule::TypeInfo* typeInfo = module->FindType(name);
        if ( typeInfo && typeInfo->exported )
            return (m_Types[name] = typeInfo->type->GetTypeInfo());
    }
    THROW1_TRACE(runtime_error, "type not found: " + name);
}

const ASNModule::TypeInfo* CModuleSet::FindType(const ASNModule::TypeInfo* t) const
{
    if ( t->module.empty() )
        THROW1_TRACE(runtime_error, "module not specified: " + t->name);

    return FindType(t->module, t->name);
}

const ASNModule::TypeInfo* CModuleSet::FindType(const string& fullName) const
{
    SIZE_TYPE dot = fullName.find('.');
    if ( dot == NPOS ) {
        const ASNModule::TypeInfo* type = 0;
        for ( TModules::const_iterator i = modules.begin();
              i != modules.end(); ++i ) {
            ASNModule* module = (*i).get();
            const ASNModule::TypeInfo* t = module->FindType(fullName);
            if ( t ) {
                if ( type == 0 )
                    type = t;
                else {
                    THROW1_TRACE(runtime_error, "ambiguous type: " + fullName);
                }
            }
        }
        if ( !type )
            THROW1_TRACE(runtime_error, "type not found: " + fullName);
        return type;
    }
    else {
        return FindType(fullName.substr(0, dot), fullName.substr(dot + 1));
    }
}

const ASNModule::TypeInfo* CModuleSet::FindType(const string& moduleName,
                                                const string& typeName) const
{
    // find module definition
    for ( TModules::const_iterator i = modules.begin();
          i != modules.end(); ++i ) {
        ASNModule* module = (*i).get();
        if ( module->name == moduleName ) {
            const ASNModule::TypeInfo* t = module->FindType(typeName);
            if ( t && !t->exported )
                ERR_POST("not exported: " + moduleName + "." + typeName);
            return t;
        }
    }
    THROW1_TRACE(runtime_error, "module not found: " + moduleName);
    return 0;
}
