#ifndef ASNMODULE_HPP
#define ASNMODULE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <set>
#include <list>
#include <map>
#include <autoptr.hpp>

USING_NCBI_SCOPE;

class ASNType;
class CModuleSet;

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

    void AddDefinition(const string& name, const AutoPtr<ASNType> type);

    virtual CNcbiOstream& Print(CNcbiOstream& out) const;

    bool Check();
    bool CheckNames();

    string name;
    TExports exports;
    TImports imports;
    TDefinitions definitions;

    class TypeInfo {
    public:
        TypeInfo(const string& n)
            : name(n), type(0), exported(false)
            {
            }
        string name;
        string module;  // non empty for imports
        ASNType* type;  // non empty for non imports
        bool exported;  // true for exports
    };

    const TypeInfo* FindType(const string& name) const;

    typedef map<string, TypeInfo> TTypes;
    TTypes types;

    CModuleSet* moduleSet;
};

#endif
