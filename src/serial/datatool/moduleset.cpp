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
    {
        TTypes::const_iterator ti = m_Types.find(name);
        if ( ti != m_Types.end() )
            return ti->second;
    }

    // find type definition
    for ( TModules::const_iterator i = modules.begin();
          i != modules.end(); ++i ) {
        ASNModule* module = (*i).get();
        const ASNModule::TypeInfo* typeInfo = module->FindType(name);
        if ( typeInfo && typeInfo->exported )
            return m_Types[name] = typeInfo->type->GetTypeInfo();
    }
    THROW1_TRACE(runtime_error, "type not found: " + name);
}

const ASNModule::TypeInfo* CModuleSet::FindType(const ASNModule::TypeInfo* t) const
{
    if ( t->module.empty() )
        THROW1_TRACE(runtime_error, "module not specified: " + t->name);

    // find module definition
    for ( TModules::const_iterator i = modules.begin();
          i != modules.end(); ++i ) {
        ASNModule* module = (*i).get();
        if ( module->name == t->module ) {
            t = module->FindType(t->name);
            if ( t && !t->exported )
                ERR_POST("not exported: " + module->name + "." + t->name);
            return t;
        }
    }
    THROW1_TRACE(runtime_error, "module not found: " + t->module);
}
