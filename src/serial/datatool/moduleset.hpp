#ifndef MODULESET_HPP
#define MODULESET_HPP

#include <corelib/ncbistd.hpp>
#include <serial/typemapper.hpp>
#include <list>
#include <map>
#include "autoptr.hpp"
#include "module.hpp"

USING_NCBI_SCOPE;

class CModuleSet : public CTypeMapper
{
public:
    typedef list<AutoPtr<ASNModule> > TModules;
    typedef map<string, const CTypeInfo*> TTypes;

    CModuleSet(void);
    virtual ~CModuleSet(void);

    TTypeInfo MapType(const string& name);

    TModules modules;
    string rootTypeName;

    const ASNModule::TypeInfo* FindType(const ASNModule::TypeInfo* t) const;

private:
    TTypes m_Types;
};

#endif
