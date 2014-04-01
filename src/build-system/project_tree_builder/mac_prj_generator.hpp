#ifndef PROJECT_TREE_BUILDER__MAC_PRJ_GENERATOR__HPP
#define PROJECT_TREE_BUILDER__MAC_PRJ_GENERATOR__HPP

/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Andrei Gourianov
 *
 */

#include "proj_item.hpp"
#include "msvc_project_context.hpp"
#include "prj_file_collector.hpp"

#include <corelib/ncbienv.hpp>

#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)

#include <build-system/project_tree_builder/property_list__.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
class CPlist;
END_SCOPE(objects)

USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////

class CMacProjectGenerator
{
public:
    CMacProjectGenerator(const list<SConfigInfo>& configs,
                         const CProjectItemsTree& projects_tree);
    ~CMacProjectGenerator(void);
    
    void Generate(const string& solution);
    static string GetProjId(       const CProjItem& prj);

private:
    list<SConfigInfo> m_Configs;
    const CProjectItemsTree& m_Projects_tree;
    string m_SolutionDir;
    string m_OutputDir;
    map<string,string> m_TargetProduct;
    set<string> m_RefLibs;

    void Save(const string& solution_name, ncbi::objects::CPlist& xproj);
    void CreateConfigureScript(const string& name, bool with_gui) const;

    string CreateProjectFileGroups(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_objects, CArray& build_files);
    string CreateProjectScriptPhase(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_objects);
    string CreateProjectCustomScriptPhase(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_objects);
    string CreateProjectCopyBinScript(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_objects);
    string CreateProjectLinkPhase(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_objects);
    string CreateProjectBuildPhase(
        const CProjItem& prj,
        CDict& dict_objects, CRef<CArray>& build_files);
    void CollectLibToLibDependencies(
        set<string>& dep, set<string>& visited,
        const CProjItem& lib, const CProjItem& lib_dep);
    string CreateProjectTarget(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_objects, CRef<CArray>& build_phases, const string& product_id);

    string CreateBuildConfigurations(CDict& dict_objects);
    string CreateProjectBuildConfigurations(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_objects);
    string CreateAggregateBuildConfigurations(const string& target_name,
        CDict& dict_objects);

    void CreateBuildSettings(CDict& dict_cfg, const SConfigInfo& cfg);
    void CreateProjectBuildSettings(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_cfg, const SConfigInfo& cfg);
    void CreateAggregateBuildSettings( const string& target_name,
        CDict& dict_cfg, const SConfigInfo& cfg);

    string AddAggregateTarget(const string& target_name,
        CDict& dict_objects, CRef<CArray>& dependencies);
    string AddPreConfigureTarget(CArray& targets, CDict& dict_objects,
        const string& root_name);
    string AddConfigureTarget(const string& solution_name,
        CDict& dict_objects, bool gui, const string& preconf_dependency);
    string CreateRootObject(const string& configs_root,
        CDict& dict_objects, CRef<CArray>& targets,
        const string& root_group, const string& root_name,
        const string& products_group);
    
    string GetUUID(void);
    string AddFile(CDict& dict, const string& name, bool style_objcpp);
    string AddFileSource(CDict& dict, const string& name);
    void   AddGroupDict( CDict& dict, const string& key,
                         CRef<CArray>& children, const string& name);

    void         AddString( CArray& ar,  const string& value);
    void      InsertString( CArray& ar,  const string& value);
    void         AddString( CDict& dict, const string& key, const string& value);
    CRef<CArray> AddArray(  CDict& dict, const string& key);
    void         AddArray(  CDict& dict, const string& key, CRef<CArray>& array);
    CRef<CDict>  AddDict(   CDict& dict, const string& key);

    void AddCompilerSetting( CDict& settings, const SConfigInfo& cfg, const string& key);
    void AddLinkerSetting(   CDict& settings, const SConfigInfo& cfg, const string& key);
    void AddLibrarianSetting(CDict& settings, const SConfigInfo& cfg, const string& key);

    // convenience
    string GetRelativePath(const string& name, const string* from=0) const;

    // get names for generation
    static string GetProjTarget(   const CProjItem& prj);
    static string GetProjBuild(    const CProjItem& prj);
    static string GetProjProduct(  const CProjItem& prj);
    static string GetProjSources(  const CProjItem& prj);
    static string GetProjHeaders(  const CProjItem& prj);
    static string GetProjDependency(  const CProjItem& prj);
    static string GetProjContainer(   const CProjItem& prj);
    static string GetTargetName(   const CProjItem& prj);

    static string GetMachOType(    const CProjItem& prj);
    static string GetProductType(  const CProjItem& prj);
    static string GetExplicitType( const CProjItem& prj);

    /// Prohibited to.
    CMacProjectGenerator(void);
    CMacProjectGenerator(const CMacProjectGenerator&);
    CMacProjectGenerator& operator= (const CMacProjectGenerator&);
};

END_NCBI_SCOPE

#endif

#endif //PROJECT_TREE_BUILDER__MAC_PRJ_GENERATOR__HPP
