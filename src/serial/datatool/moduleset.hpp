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
    typedef map<string, AutoPtr<ASNModule> > TModules;
    typedef map<string, const CTypeInfo*> TTypes;

    CModuleSet(void);
    virtual ~CModuleSet(void);

    void SetMainTypes(void);

    bool Check(void) const;

    TTypeInfo MapType(const string& name);

    // main types
    TModules modules;

    string rootTypeName;

    ASNType* Resolve(const string& moduleName, const string& typeName) const;
    ASNType* ResolveFull(const string& fullName) const;

private:
    TTypes m_Types;
};

#endif
