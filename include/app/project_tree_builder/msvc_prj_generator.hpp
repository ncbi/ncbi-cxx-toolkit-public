#ifndef MSVC_PRJ_GENERATOR_HEADER
#define MSVC_PRJ_GENERATOR_HEADER

#include <app/project_tree_builder/proj_item.hpp>
#include "VisualStudioProject.hpp"
#include <app/project_tree_builder/msvc_project_context.hpp>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

class CMsvcProjectGenerator
{
public:
    CMsvcProjectGenerator(const list<string>& configs);
    ~CMsvcProjectGenerator();
    
    bool Generate(const CProjItem& prj);

private:

    list<string> m_Configs;

    const string m_Platform;
    const string m_ProjectType;
    const string m_Version;
    const string m_Keyword;

    // Prohibited to:
    CMsvcProjectGenerator(void);
    CMsvcProjectGenerator(const CMsvcProjectGenerator&);
    CMsvcProjectGenerator& operator = (const CMsvcProjectGenerator&);

};

//------------------------------------------------------------------------------
END_NCBI_SCOPE

#endif // MSVC_PRJ_GENERATOR_HEADER