#ifndef GENERATE_HPP
#define GENERATE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <set>
#include <map>
#include "moduleset.hpp"

class CFileCode;

class CCodeGenerator
{
public:
    typedef set<string> TTypeNames;
    typedef map<string, AutoPtr<CFileCode> > TOutputFiles;

    CCodeGenerator(void);
    ~CCodeGenerator(void);

    void GenerateCode(void);

    void LoadConfig(const string& fileName);
    void GetAllTypes(void);
    static void GetTypes(TTypeNames& typeNames, const string& name);

    enum EContext {
        eOther,
        eRoot,
        eChoice,
        eReference
    };
    void CollectTypes(const ASNType* type, EContext context = eOther );
    bool AddType(const ASNType* type);

    CNcbiRegistry m_Config;
    CModuleSet m_Modules;
    TTypeNames m_GenerateTypes;
    TTypeNames m_ExcludeTypes;
    bool m_ExcludeAllTypes;
    string m_FileListFileName;
    string m_HeadersDir;
    string m_SourcesDir;
    string m_HeaderPrefix;

    TOutputFiles m_Files;
};

#endif
