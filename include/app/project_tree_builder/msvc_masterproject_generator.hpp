#ifndef MSVC_MASTERPROJECT_GENERATOR_HEADER
#define MSVC_MASTERPROJECT_GENERATOR_HEADER

#include <app/project_tree_builder/proj_item.hpp>
#include "VisualStudioProject.hpp"

#include "Platforms.hpp"
#include "Platform.hpp"
#include "Configurations.hpp"
#include "Configuration.hpp"
#include "Tool.hpp"
#include "File.hpp"
#include "Files.hpp"
#include "Filter.hpp"
#include "FileConfiguration.hpp"

#include <set>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

class CMsvcMasterProjectGenerator
{
public:
    CMsvcMasterProjectGenerator(const CProjectItemsTree& tree,
                                const list<string>& configs,
                                const string& project_dir);

    ~CMsvcMasterProjectGenerator(void);

    const CProjectItemsTree& m_Tree;
    list<string> m_Configs;

    // base name
    void SaveProject(const string& base_name);

private:
  	const string m_Name;

    const string m_ProjectDir;

    const string m_ProjectItemExt;

    string m_CustomBuildCommand;

    ///Helpers:
    string ConfigName(const string& config) const;

    typedef map<string, CRef<CFilter> > TFiltersCache;
    TFiltersCache m_FiltersCache;
    CRef<CFilter> FindOrCreateFilter(const string& project_id,
                                     CSerialObject * pParent);
    set<string> m_ProcessedIds;
    bool IsAlreadyProcessed(const string& project_id);
    
   
    void AddProjectToFilter(CRef<CFilter>& filter, const string& project_id);


    void ProcessProjectBranch(const string& project_id, 
                              CSerialObject * pParent);

    void CreateProjectFileItem(const CProjItem& project);


    /// Prohibited to:
    CMsvcMasterProjectGenerator(void);
    CMsvcMasterProjectGenerator(const CMsvcMasterProjectGenerator& generator);
    CMsvcMasterProjectGenerator& operator = (
                                const CMsvcMasterProjectGenerator& generator);

};

//------------------------------------------------------------------------------
END_NCBI_SCOPE

#endif  // MSVC_MASTERPROJECT_GENERATOR_HEADER