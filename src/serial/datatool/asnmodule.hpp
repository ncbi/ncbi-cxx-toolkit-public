#ifndef ASNMODULE_HPP
#define ASNMODULE_HPP

#include <ostream>
#include <set>
#include <list>
#include <map>
#include "autoptr.hpp"

using namespace std;

class ASNType;

void Warning(const string& message);

class ASNModule {
public:
    ASNModule();
    virtual ~ASNModule();

    typedef list< string > TExports;
    class Import {
    public:
        string module;
        list<string> types;
    };
    typedef list< AutoPtr<Import> > TImports;
    typedef list< pair< string, AutoPtr< ASNType > > > TDefinitions;

    void AddDefinition(const string& name, const AutoPtr<ASNType>& type);

    virtual ostream& Print(ostream& out) const;

    bool Check();
    bool CheckNames();

    string name;
    TExports exports;
    TImports imports;
    TDefinitions definitions;

    class TypeInfo {
    public:
        TypeInfo(const string& n)
            : name(n), type(0)
            {
            }
        string name;
        string module;
        ASNType* type;
    };

    const TypeInfo* FindType(const string& name) const;

    typedef map<string, TypeInfo> TTypes;
    TTypes types;
};

#endif
