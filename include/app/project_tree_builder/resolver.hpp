#ifndef RESOLVER_01072004_HEADER
#define RESOLVER_01072004_HEADER

#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/file_contents.hpp>

#include <map>
#include <string>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbienv.hpp>

USING_NCBI_SCOPE;

class CSymResolver
{
public:
    CSymResolver(void);
    CSymResolver(const CSymResolver& resolver);
    CSymResolver& operator = (const CSymResolver& resolver);
    CSymResolver(const string& file_path);
    ~CSymResolver(void);

    void Resolve(const string& define, list<string> * pResolvedDef);

    static void LoadFrom(const string& file_path, CSymResolver * pResolver);

    bool IsEmpty(void) const;

    static bool IsDefine(const string& param);
private:
    void Clear(void);
    void SetFrom(const CSymResolver& resolver);

    CSimpleMakeFileContents m_Data;

    CSimpleMakeFileContents::TContents m_Cache;
};

#endif //RESOLVER_01072004_HEADER