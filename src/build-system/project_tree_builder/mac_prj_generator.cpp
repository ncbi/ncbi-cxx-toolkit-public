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

#include <ncbi_pch.hpp>

#include "mac_prj_generator.hpp"

#include "proj_builder_app.hpp"
#include "msvc_project_context.hpp"
#include "proj_tree_builder.hpp"


#include "msvc_prj_utils.hpp"
#include "msvc_prj_defines.hpp"
#include "msvc_prj_files_collector.hpp"
#include "ptb_err_codes.hpp"

#include <algorithm>

#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
#  include <serial/objostrxml.hpp>
#  include <serial/serial.hpp>
#endif

BEGIN_NCBI_SCOPE

#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)

// 
// 1 - use human-friendly names (still works with XCode, but generates lots of warnings)
// 0 - use XCode-friendly hexadecimal ids
#define USE_VERBOSE_NAMES 0

/////////////////////////////////////////////////////////////////////////////

const char* s_ptb_makecandidate = "__PTB__MAKE__CANDIDATE__";

bool s_ProjId_less(const CProjItem* x, const CProjItem* y)
{
    return NStr::CompareNocase(
        CMacProjectGenerator::GetProjId(*x),
        CMacProjectGenerator::GetProjId(*y)) < 0;
}

bool s_ProjItem_less(const CProjItem& x, const CProjItem& y)
{
    ITERATE( list<CProjKey>, i, x.m_Depends) {
        if (y.m_ID == i->Id()) {
            return true;
        }
    }
    return false;
}

bool s_String_less(const CRef<CArray::C_E>& x, const CRef<CArray::C_E>& y)
{
    return NStr::CompareNocase(x->GetString(), y->GetString()) < 0;
}

bool s_Key_less(const CRef<CDict::C_E>& x, const CRef<CDict::C_E>& y)
{
    return NStr::CompareNocase(x->GetKey(), y->GetKey()) < 0;
}


/////////////////////////////////////////////////////////////////////////////

CMacProjectGenerator::CMacProjectGenerator(
    const list<SConfigInfo>& configs, const CProjectItemsTree& projects_tree)
    :m_Configs(configs), m_Projects_tree(projects_tree)
{
}

CMacProjectGenerator::~CMacProjectGenerator(void)
{
}

void CMacProjectGenerator::Generate(const string& solution)
{
    m_SolutionDir = CDirEntry::IsAbsolutePath(solution) ? 
        solution : CDirEntry::ConcatPath( CDir::GetCwd(), solution);
    string solution_name = CDirEntry(m_SolutionDir).GetName();
    m_SolutionDir = CDirEntry(m_SolutionDir).GetDir(CDirEntry::eIfEmptyPath_Empty);

    m_OutputDir = CDirEntry::AddTrailingPathSeparator(
        CDirEntry::ConcatPath( CDirEntry::ConcatPath(
            GetApp().GetProjectTreeInfo().m_Compilers,
            GetApp().GetRegSettings().m_CompilersSubdir),
            GetApp().GetBuildType().GetTypeStr()));
    m_OutputDir = GetRelativePath(m_OutputDir);
    
    CRef<CPlist> xproj( new CPlist);
    CDict& dict_root = xproj->SetPlistObject().SetDict();
    
    xproj->SetAttlist().SetVersion("1.0");
    AddString( dict_root, "archiveVersion", "1");
    AddDict(   dict_root, "classes");
// 39  42  44
    AddString( dict_root, "objectVersion", GetApp().GetRegSettings().m_Version);
    CRef<CDict> dict_objects( AddDict( dict_root, "objects"));
    string configs_root( CreateBuildConfigurations( *dict_objects));

    CRef<CArray> file_groups( new CArray);
    CRef<CArray> targets( new CArray);
    CRef<CArray> all_dependencies( new CArray);
    CRef<CArray> app_dependencies( new CArray);
    CRef<CArray> lib_dependencies( new CArray);
    CRef<CArray> dataspec_dependencies( new CArray);
    CRef<CArray> products( new CArray);
    bool add_composite = GetApp().m_ProjTags == "*";
    vector< CRef<CArray> > composite_dependencies;
    for (map<string,string>::const_iterator composite = GetApp().m_CompositeProjectTags.begin();
        composite != GetApp().m_CompositeProjectTags.end(); ++composite) {
        composite_dependencies.push_back( CRef<CArray>(new CArray) );
    }

    
#if USE_VERBOSE_NAMES
    string root_name("ROOT_OBJECT");
#else
    string root_name( GetUUID());
#endif
    // set GUIDs
    list<const CProjItem*> all_projects;
    ITERATE(CProjectItemsTree::TProjects, p, m_Projects_tree.m_Projects) {
        const CProjItem& prj(p->second);
        prj.m_GUID = GetUUID();
        all_projects.push_back(&prj);
    }
    all_projects.sort(s_ProjId_less);

    // generate product IDs
    m_TargetProduct.clear();
    ITERATE(list<const CProjItem*>, p, all_projects) {
        const CProjItem& prj(**p);
        string target_id(GetProjId(prj));
        m_TargetProduct[target_id] = GetUUID();
    }
    // generate targets
    ITERATE(list<const CProjItem*>, p, all_projects) {
        const CProjItem& prj(**p);

#if USE_VERBOSE_NAMES
        string proj_dependency(GetProjDependency(prj));
        string proj_container( GetProjContainer(prj));
#else
        string proj_dependency(prj.m_GUID);
        string proj_container( GetUUID());
#endif
        string explicit_type( GetExplicitType( prj));
        string target_name(GetTargetName(prj));
        string target_id(GetProjId(prj));

        CProjectFileCollector prj_files( prj, m_Configs, m_SolutionDir+m_OutputDir);
        if (!prj_files.CheckProjectConfigs()) {
            continue;
        }
        if (prj.m_MakeType == eMakeType_ExcludedByReq) {
            PTB_WARNING_EX(prj.m_ID, ePTB_ProjectExcluded,
                           "Excluded due to unmet requirements");
            continue;
        }
// see CXX-542
        bool excluded = (prj.m_MakeType >= eMakeType_Expendable);
//        bool excluded = (prj.m_MakeType > eMakeType_Expendable);

        prj_files.DoCollect();
        if (!excluded) {
            if (prj.m_ProjType == CProjKey::eLib || prj.m_ProjType == CProjKey::eDll) {
                AddString( *lib_dependencies, proj_dependency);
//            } else if (prj.m_ProjType == CProjKey::eApp) {
//                AddString( *app_dependencies, proj_dependency);
//            } else if (prj.m_ProjType == CProjKey::eDataSpec) {
            }
            if (!prj.m_DatatoolSources.empty()) {
                AddString( *dataspec_dependencies, proj_dependency);
            }
            if (prj.m_ProjType != CProjKey::eLib && prj.m_ProjType != CProjKey::eDll &&
                prj.m_ProjType != CProjKey::eApp && prj.m_ProjType != CProjKey::eDataSpec &&
                prj.m_ProjType != CProjKey::eMsvc) {
                continue;
            }
            if (add_composite) {
                int c = 0;
                for (map<string,string>::const_iterator composite = GetApp().m_CompositeProjectTags.begin();
                    composite != GetApp().m_CompositeProjectTags.end(); ++composite, ++c) {
                    
                    if (GetApp().IsAllowedProjectTag(prj, &composite->second)) {
                        AddString( *(composite_dependencies[c]), proj_dependency);
                    }
                }
            }
            AddString( *all_dependencies, proj_dependency);
        }

        if (prj.m_ProjType == CProjKey::eApp) {
            string app_type = prj_files.GetProjectContext().GetMsvcProjectMakefile().GetLinkerOpt("APP_TYPE",SConfigInfo());
            if (app_type == "application") {
                prj.m_IsBundle = true;
                explicit_type = GetExplicitType( prj);
            }
        }
        
        CRef<CArray> build_phases( new CArray);
        CRef<CArray> build_files( new CArray);

        // project file groups
        AddString( *file_groups,
            CreateProjectFileGroups(prj, prj_files, *dict_objects, *build_files));

        // project script phase
        string proj_script(
            CreateProjectScriptPhase(prj, prj_files, *dict_objects));
        if (!proj_script.empty()) {
            AddString( *build_phases, proj_script);
        }
        // project build phase
        string proj_build(
            CreateProjectBuildPhase(prj, *dict_objects, build_files));
        if (!proj_build.empty()) {
            AddString( *build_phases, proj_build);
        }
        // project custom script phase
        string proj_cust_script(
            CreateProjectCustomScriptPhase(prj, prj_files, *dict_objects));
        if (!proj_cust_script.empty()) {
            AddString( *build_phases, proj_cust_script);
        }
        // link binary with libraries phase
        string proj_link(
            CreateProjectLinkPhase(prj, prj_files, *dict_objects));
        if (!proj_link.empty()) {
            AddString( *build_phases, proj_link);
        }
        // project copybin script phase
        string proj_copybin_script(
            CreateProjectCopyBinScript(prj, prj_files, *dict_objects));
        if (!proj_copybin_script.empty()) {
            AddString( *build_phases, proj_copybin_script);
        }
        // project target and dependencies
        string proj_product( m_TargetProduct[target_id] );
        string proj_target(
            CreateProjectTarget( prj, prj_files, *dict_objects, build_phases,
                                 proj_product));
        if (!proj_target.empty()) {
            AddString( *targets, proj_target);
        }

        // project dependency key
        {
            CRef<CDict> dict_dep( AddDict( *dict_objects, proj_dependency));
            AddString( *dict_dep, "isa", "PBXTargetDependency");
            AddString( *dict_dep, "target", proj_target);
            AddString( *dict_dep, "targetProxy", proj_container);
        }
        // project container
        {
            CRef<CDict> dict_con( AddDict( *dict_objects, proj_container));
            AddString( *dict_con, "containerPortal", root_name);
            AddString( *dict_con, "isa", "PBXContainerItemProxy");
            AddString( *dict_con, "proxyType", "1");
            AddString( *dict_con, "remoteGlobalIDString", proj_target);
            AddString( *dict_con, "remoteInfo", target_name);
        }
        // project product
        if (!explicit_type.empty()) {
            AddString( *products, proj_product);
            CRef<CDict> dict_product( AddDict( *dict_objects, proj_product));
            AddString( *dict_product, "explicitFileType", explicit_type);
            AddString( *dict_product, "includeInIndex", "0");
            AddString( *dict_product, "isa", "PBXFileReference");
            if (prj.m_ProjType == CProjKey::eDll) {
                AddString( *dict_product, "path", string("lib") + prj.m_ID + string(".dylib"));
            } else if (prj.m_ProjType == CProjKey::eApp) {
                AddString( *dict_product, "path", prj.m_ID);
            } else if (prj.m_ProjType == CProjKey::eLib) {
                AddString( *dict_product, "path", string("lib") + prj.m_ID + string(".a"));
            }
            AddString( *dict_product, "sourceTree", "BUILT_PRODUCTS_DIR");
        }
        // watchers
        GetApp().RegisterProjectWatcher( target_name, prj.m_SourcesBaseDir, prj.m_Watchers);
    }

// collect file groups
#if USE_VERBOSE_NAMES
    string source_files("Source_Files");
    string root_group("Main_Group");
    string products_group("Products_Group");
#else
    string source_files(   GetUUID());
    string root_group(     GetUUID());    
    string products_group( GetUUID());
#endif

    file_groups->Set().sort(s_String_less);
    AddGroupDict( *dict_objects, source_files, file_groups, "Sources");
    AddGroupDict( *dict_objects, products_group, products, "Products");
    CRef<CArray> main_groups( new CArray);
    AddString( *main_groups, source_files);
    AddString( *main_groups, products_group);
    AddGroupDict( *dict_objects, root_group, main_groups, "NCBI C++ Toolkit");

    targets->Set().sort(s_String_less);
    dataspec_dependencies->Set().sort(s_String_less);
    lib_dependencies->Set().sort(s_String_less);
//    app_dependencies->Set().sort(s_String_less);
    all_dependencies->Set().sort(s_String_less);
    if (add_composite) {
        int c = 0;
        for (map<string,string>::const_iterator composite = GetApp().m_CompositeProjectTags.begin();
            composite != GetApp().m_CompositeProjectTags.end(); ++composite, ++c) {
            
            if (!composite_dependencies[c]->Get().empty()) {
                composite_dependencies[c]->Set().sort(s_String_less);
                InsertString( *targets,
                    AddAggregateTarget("_TAG_" + composite->first, *dict_objects, composite_dependencies[c]));
            }
        }
    }
// aggregate targets
    string preconf_dependency(AddPreConfigureTarget(*targets,*dict_objects, root_name));
    InsertString( *targets,
        AddConfigureTarget(solution_name,  *dict_objects, true, preconf_dependency));
    InsertString( *targets,
        AddConfigureTarget(solution_name,  *dict_objects, false, preconf_dependency));
    InsertString( *targets,
        AddAggregateTarget("DATASPEC_ALL", *dict_objects, dataspec_dependencies));
    InsertString( *targets,
        AddAggregateTarget("LIBS_ALL", *dict_objects, lib_dependencies));
//    InsertString( *targets,
//        AddAggregateTarget("BUILD_APPS", *dict_objects, app_dependencies));
    InsertString( *targets,
        AddAggregateTarget("BUILD_ALL",  *dict_objects, all_dependencies));

// root object
    AddString( dict_root, "rootObject",
        CreateRootObject(configs_root, *dict_objects, targets,
            root_group, root_name, products_group));

#if !USE_VERBOSE_NAMES
    dict_objects->Set().sort(s_Key_less);
#endif

// save project
    Save(solution_name, *xproj);
}

void CMacProjectGenerator::Save(const string& solution_name, CPlist& xproj)
{
    bool make_candidate = !GetApp().GetEnvironment().Get(s_ptb_makecandidate).empty();
    string solution_dir(m_SolutionDir);
    solution_dir = CDirEntry::ConcatPath(solution_dir, solution_name);
    solution_dir += ".xcodeproj";
    CDir(solution_dir).CreatePath();
    string solution_file( CDirEntry::ConcatPath(solution_dir, "project.pbxproj"));
    GetApp().RegisterGeneratedFile( solution_file );
    if (make_candidate) {
        solution_file += ".candidate";
    }
    {
        auto_ptr<CObjectOStream> out(CObjectOStream::Open(solution_file, eSerial_Xml));
        CObjectOStreamXml *ox = dynamic_cast<CObjectOStreamXml*>(out.get());
        ox->SetReferenceDTD(true);
        ox->SetDTDPublicId("-//Apple//DTD PLIST 1.0//EN");
        ox->SetDTDFilePrefix("http://www.apple.com/DTDs/");
        ox->SetDTDFileName("PropertyList-1.0");
        ox->SetEncoding(eEncoding_UTF8);
        *out << xproj;
    }
    CreateConfigureScript(solution_name, false);
    CreateConfigureScript(solution_name, true);
}

void CMacProjectGenerator::CreateConfigureScript(const string& name, bool with_gui) const
{
    string script = CDirEntry::ConcatPath(m_SolutionDir,"UtilityProjects/configure_");
    if (with_gui) {
        script += "gui_";
    }
    script += name + ".sh";
    string candidate = script + ".candidate";

    CNcbiOfstream  ofs(candidate.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, script);

    GetApp().RegisterGeneratedFile( script );
    ofs << "#!/bin/sh\n";
    ofs << "export PTB_FLAGS=\"";
    if ( GetApp().GetBuildType().GetType() == CBuildType::eDll )
        ofs << " -dll";
    if (!GetApp().m_BuildPtb) {
        ofs << " -nobuildptb";
    }
    if (GetApp().m_Dtdep) {
        ofs << " -dtdep";
    }
    if (!GetApp().m_AddMissingDep) {
        ofs << " -noadddep";
    }
    ofs << " -libdep " << (GetApp().m_LibDep ? "true" : "false");
    if (GetApp().m_AddMissingLibs) {
        ofs << " -ext";
    }
    if (!GetApp().m_ScanWholeTree) {
        ofs << " -nws";
    }
    if (!GetApp().m_BuildRoot.empty()) {
        ofs << " -extroot \"" << GetApp().m_BuildRoot << "\"";
    }
    if (with_gui /*|| GetApp().m_ConfirmCfg*/) {
        ofs << " -cfg";
    }
    if (GetApp().m_ProjTagCmnd) {
        if (GetApp().m_ProjTags != "*") {
            ofs << " -projtag \\\"" << GetApp().m_ProjTags << "\\\"";
        } else {
            ofs << " -projtag #";
        }
    }
    ofs << "\"\n";
    ofs << "export PTB_PROJECT_REQ=" << GetApp().m_Subtree << "\n";
    ofs << "$BUILD_TREE_ROOT/ptb.sh\n";
    ofs.close();
    PromoteIfDifferent(script, candidate);
    CDirEntry(script).SetMode(
        CDirEntry::fExecute | CDirEntry::fRead | CDirEntry::fWrite,
        CDirEntry::fExecute | CDirEntry::fRead | CDirEntry::fWrite,
        CDirEntry::fExecute | CDirEntry::fRead | CDirEntry::fWrite);
}

string CMacProjectGenerator::CreateProjectFileGroups(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects, CArray& build_files)
{
    CDllSrcFilesDistr& dll_src( GetApp().GetDllFilesDistr());
    CProjKey proj_key(prj.m_ProjType, prj.m_ID);

    string proj_id(        GetProjId( prj) );
#if USE_VERBOSE_NAMES
    string proj_src(       proj_id + "_src");
    string proj_include(   proj_id + "_include");
    string proj_specs(     proj_id + "_specs");
    string src_group_name( proj_id + "_sources");
#else
    string proj_src(       GetUUID());
    string proj_include(   GetUUID());
    string proj_specs(     GetUUID());
    string src_group_name( GetUUID());
#endif

    CRef<CArray> proj_cpps(  new CArray);
    CRef<CArray> proj_hpps(  new CArray);
    CRef<CArray> specs( new CArray);

    map<string, CRef<CArray> > hosted_cpps;
    map<string, CRef<CArray> > hosted_hpps;
    map<string, CRef<CArray> > hosted_srcs;
    ITERATE( list<string>, hosted_lib, prj.m_HostedLibs) {
        hosted_cpps[ *hosted_lib] = new CArray;
        hosted_hpps[ *hosted_lib] = new CArray;
        hosted_srcs[ *hosted_lib] = new CArray;
    }

    // for each source file in project
    ITERATE ( list<string>, f, prj_files.GetSources()) {
        string src( AddFile( dict_objects, *f, prj.m_StyleObjcpp));
        if (!src.empty()) {
            bool added=false;
            if (prj.m_ProjType == CProjKey::eDll) {
                CProjKey hosted_key = dll_src.GetSourceLib( *f, proj_key);
                if (!hosted_key.Id().empty()) {
                    AddString( *hosted_cpps[ hosted_key.Id() ], src);
                    added = true;
                }
            }
            if (!added) {
                AddString( *proj_cpps, src);
            }
            AddString( build_files, AddFileSource( dict_objects, src));
        }
    }
    // for each header file in project
    ITERATE ( list<string>, f, prj_files.GetHeaders()) {
        string src( AddFile( dict_objects, *f, prj.m_StyleObjcpp));
        if (!src.empty()) {
            bool added=false;
            if (prj.m_ProjType == CProjKey::eDll) {
                CProjKey hosted_key = dll_src.GetHeaderLib( *f, proj_key);
                if (!hosted_key.Id().empty()) {
                    AddString( *hosted_hpps[ hosted_key.Id() ], src);
                    added = true;
                } else {
                    CProjKey hosted_inl = dll_src.GetInlineLib( *f, proj_key);
                    if (!hosted_inl.Id().empty()) {
                        AddString( *hosted_hpps[ hosted_inl.Id() ], src);
                        added = true;
                    }
                }
            }
            if (!added) {
                AddString( *proj_hpps, src);
            }
        }
    }
    // dataspecs
    ITERATE ( list<string>, f, prj_files.GetDataSpecs()) {
        string src( AddFile( dict_objects, *f, prj.m_StyleObjcpp));
        if (!src.empty()) {
            AddString( *specs,src);
        }
    }

    string source_files(  "Source Files");
    string header_files(  "Header Files");
    string datatool_files("Datatool Files");
    CRef<CArray> prj_sources( new CArray);
    ITERATE( list<string>, hosted_lib, prj.m_HostedLibs) {
        CRef<CArray>& cpps = hosted_cpps[ *hosted_lib];
        CRef<CArray>& hpps = hosted_hpps[ *hosted_lib];
        CRef<CArray>& srcs = hosted_srcs[ *hosted_lib];
#if USE_VERBOSE_NAMES
        string hosted_src(   *hosted_lib + "_hosted_src");
        string hosted_inc(   *hosted_lib + "_hosted_include");
        string hosted_group( *hosted_lib + "_hosted_sources");
#else
        string hosted_src(   GetUUID());
        string hosted_inc(   GetUUID());
        string hosted_group( GetUUID());
#endif
        if (!cpps->Get().empty()) {
            cpps->Set().sort(s_String_less);
            AddString( *srcs, hosted_src);
            AddGroupDict( dict_objects, hosted_src, cpps, source_files);
        }
        if (!hpps->Get().empty()) {
            hpps->Set().sort(s_String_less);
            AddString( *srcs, hosted_inc);
            AddGroupDict( dict_objects, hosted_inc, hpps, header_files);
        }
        AddGroupDict( dict_objects, hosted_group, srcs, *hosted_lib);
        AddString( *prj_sources, hosted_group);
    }
    if (!prj.m_HostedLibs.empty()) {
        prj_sources->Set().sort(s_String_less);
    }
    if (!proj_cpps->Get().empty()) {
        proj_cpps->Set().sort(s_String_less);
        AddString( *prj_sources, proj_src);
        AddGroupDict( dict_objects, proj_src,     proj_cpps, source_files);
    }
    if (!proj_hpps->Get().empty()) {
        proj_hpps->Set().sort(s_String_less);
        AddString( *prj_sources, proj_include);
        AddGroupDict( dict_objects, proj_include, proj_hpps, header_files);
    }
    if (!specs->Get().empty()) {
        AddString( *prj_sources, proj_specs);
        AddGroupDict( dict_objects, proj_specs,   specs, datatool_files);
    }
    AddGroupDict( dict_objects, src_group_name, prj_sources, GetTargetName(prj));
    return src_group_name;
}

string CMacProjectGenerator::CreateProjectScriptPhase(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects)
{
    string script;
    CRef<CArray> inputs(  new CArray);
    CRef<CArray> outputs( new CArray);
    ITERATE( list<CProjKey>, p, prj_files.GetProjectContext().PreBuilds()) {
        const CProjKey& proj_key = *p;
        if (m_Projects_tree.m_Projects.find(proj_key) ==
            m_Projects_tree.m_Projects.end()) {
            PTB_WARNING_EX(
                CDirEntry::ConcatPath(prj.m_SourcesBaseDir, CreateProjectName(CProjKey(prj.m_ProjType, prj.m_ID))),
                ePTB_ProjectNotFound, " depends on missing project: " << proj_key.Id());
            if (!SMakeProjectT::IsConfigurableDefine(proj_key.Id())) {
                script += "echo ERROR: this project depends on missing " + CreateProjectName(proj_key);
                script += "\nexit 1\n";
            }
        }
    }
    // configurable files
    ITERATE ( list<string>, f, prj_files.GetConfigurableSources()) {
        string outfile(GetRelativePath( *f));
//        AddString( *outputs, outfile);
        CDirEntry ent(outfile);
        string infile(CDirEntry::ConcatPath( ent.GetDir(), ent.GetBase()));
        infile += ".$CONFIGURATION";
        infile += ent.GetExt();
        script += "cmp -s " + infile + " " + outfile + "\n";
        script += "if test $? -ne 0; then\n";
        script += "cp -p " + infile + " " + outfile + "\n";
        script += "fi\n";
    }
    // datatool
    if (!prj.m_DatatoolSources.empty()) {
#if 0
        ITERATE ( list<string>, f, prj_files.GetSources()) {
            if (prj_files.IsProducedByDatatool(prj,*f)) {
                string outfile(GetRelativePath( *f));
                AddString( *outputs, outfile);
            }
        }
#endif
        string pch_name = GetApp().GetMetaMakefile().GetDefaultPch();
        bool dataspec_first = true;
        ITERATE ( list<string>, f, prj_files.GetDataSpecs()) {
            CDirEntry entry(*f);
            string spec_base( CDirEntry(GetRelativePath(*f)).GetDir() + entry.GetBase());
            AddString( *inputs, GetRelativePath(*f));
            AddString( *inputs, spec_base + ".def");
            AddString( *outputs, spec_base + ".files");
            AddString( *outputs, spec_base + "__.cpp");
            AddString( *outputs, spec_base + "___.cpp");
#if 0
            script += "echo Using datatool to create a C++ objects from ASN/DTD/Schema " + entry.GetName() + "\n";
            script += m_OutputDir + GetApp().GetDatatoolPathForApp();
#else
            if (dataspec_first) {
                script += "export PTB_PLATFORM=\"$ARCHS\"\n";
                script += "export DATATOOL_PATH=" + m_OutputDir + "../static/bin/ReleaseDLL\n";
                script += "export TREE_ROOT=" +
                    CDirEntry::DeleteTrailingPathSeparator( GetRelativePath( GetApp().m_Root)) + "\n";
                script += "export BUILD_TREE_ROOT=" +
                    CDirEntry::DeleteTrailingPathSeparator( GetRelativePath(
                    CDirEntry::AddTrailingPathSeparator( CDirEntry::ConcatPath(
                        GetApp().GetProjectTreeInfo().m_Compilers,
                        GetApp().GetRegSettings().m_CompilersSubdir)))) + "\n";
                dataspec_first = false;
            }
            script +=  "\"$BUILD_TREE_ROOT/datatool.sh\"";
#endif
            script += " " + GetApp().GetDatatoolCommandLine() + " -pch " + pch_name;
            script += " -m " + GetRelativePath( entry.GetPath(), &GetApp().GetProjectTreeInfo().m_Src);
            string imports( prj_files.GetDataSpecImports(*f));
            if (!imports.empty()) {
#ifdef PSEUDO_XCODE
                NStr::ReplaceInPlace(imports, "\\", "/");
#endif
                script += " -M \"" + imports + "\"";
            }
            script += " -oc " + entry.GetBase();
            script += " -od " + spec_base + ".def";
            script += " -or " + GetRelativePath( entry.GetDir(), &GetApp().GetProjectTreeInfo().m_Src);
            script += " -oR " + GetRelativePath( GetApp().GetProjectTreeInfo().m_Root) + "\n";
        }
    }
    if (!script.empty()) {
#if USE_VERBOSE_NAMES
        string proj_script(   GetProjId(       prj) + "_script");
#else
        string proj_script(   GetUUID());
#endif
        CRef<CDict> dict_script( AddDict( dict_objects, proj_script));
        AddArray(  *dict_script, "files");
        AddArray(  *dict_script, "inputPaths",  inputs);
        AddArray(  *dict_script, "outputPaths", outputs);
        AddString( *dict_script, "isa", "PBXShellScriptBuildPhase");
        AddString( *dict_script, "shellPath", "/bin/sh");
        AddString( *dict_script, "shellScript", script);
        AddString( *dict_script, "showEnvVarsInLog", "0");
        return proj_script;
    }
    return kEmptyStr;
}

string CMacProjectGenerator::CreateProjectCustomScriptPhase(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects)
{
    SCustomScriptInfo info;
    prj_files.GetProjectContext().GetMsvcProjectMakefile().GetCustomScriptInfo(info);

    if (!info.m_Script.empty()) {
#if USE_VERBOSE_NAMES
        string proj_script(   GetProjId(       prj) + "_cust_script");
#else
        string proj_script(   GetUUID());
#endif
        CRef<CDict> dict_script( AddDict( dict_objects, proj_script));
        string script_loc( prj.m_SourcesBaseDir);

        CRef<CArray> inputs(  new CArray);
        CRef<CArray> outputs( new CArray);
        list<string> in_list;
        NStr::Split(info.m_Input, LIST_SEPARATOR, in_list);
        ITERATE( list<string>, i, in_list) {
            AddString( *inputs,
                GetRelativePath(CDirEntry::ConcatPath(script_loc,*i)));
        }
        list<string> out_list;
        NStr::Split(info.m_Output, LIST_SEPARATOR, out_list);
        ITERATE( list<string>, o, out_list) {
            AddString( *outputs,
                GetRelativePath(CDirEntry::ConcatPath(script_loc,*o)));
        }
        AddArray(  *dict_script, "files");
        AddArray(  *dict_script, "inputPaths",  inputs);
        AddArray(  *dict_script, "outputPaths", outputs);
        AddString( *dict_script, "isa", "PBXShellScriptBuildPhase");
        if (info.m_Shell.empty()) {
            info.m_Shell = "/bin/sh";
        }
        AddString( *dict_script, "shellPath", info.m_Shell);
        AddString( *dict_script, "shellScript",
            GetRelativePath(CDirEntry::ConcatPath(script_loc,info.m_Script)));
        AddString( *dict_script, "showEnvVarsInLog", "0");
        return proj_script;
    }
    return kEmptyStr;
}

string CMacProjectGenerator::CreateProjectCopyBinScript(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects)
{
    string bins_out_install(m_OutputDir + "bin/${CONFIGURATION}");
    if (prj.m_ProjType == CProjKey::eApp ||
        (prj.m_ProjType == CProjKey::eDll && prj.m_IsBundle)) {

        string script;
        script += "export BUILD_TREE_ROOT=" +
            CDirEntry::DeleteTrailingPathSeparator( GetRelativePath(
            CDirEntry::AddTrailingPathSeparator( CDirEntry::ConcatPath(
                GetApp().GetProjectTreeInfo().m_Compilers,
                GetApp().GetRegSettings().m_CompilersSubdir)))) + "\n";
        script += "export BUILD_TREE_BIN=" + bins_out_install + "\n";
        script +=  "\"$BUILD_TREE_ROOT/copybin.sh\"";
        string proj_script(   GetUUID());
        CRef<CArray> inputs(  new CArray);
        AddString( *inputs, "$(TARGET_BUILD_DIR)/$(TARGET_NAME)");
        CRef<CArray> outputs( new CArray);
        CRef<CDict> dict_script( AddDict( dict_objects, proj_script));
        AddArray(  *dict_script, "files");
        AddArray(  *dict_script, "inputPaths",  inputs);
        AddArray(  *dict_script, "outputPaths", outputs);
        AddString( *dict_script, "isa", "PBXShellScriptBuildPhase");
        AddString( *dict_script, "shellPath", "/bin/sh");
        AddString( *dict_script, "shellScript", script);
        AddString( *dict_script, "showEnvVarsInLog", "0");
        return proj_script;
    }
    return kEmptyStr;
}

string CMacProjectGenerator::CreateProjectLinkPhase(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects)
{
    if (prj.m_ProjType == CProjKey::eDll || prj.m_ProjType == CProjKey::eApp) {
        list<CProjItem> ldlibs;
        ITERATE( list<CProjKey>, d, prj.m_Depends) {
            CProjectItemsTree::TProjects::const_iterator
                dp = m_Projects_tree.m_Projects.find( *d);
            if ( dp != m_Projects_tree.m_Projects.end() &&
                (dp->first.Id() != prj.m_ID || dp->first.Type() != prj.m_ProjType) &&
                (dp->first.Type() == CProjKey::eLib || dp->first.Type() == CProjKey::eDll)) {

                if (dp->first.Type() == CProjKey::eLib &&
                    GetApp().GetSite().Is3PartyLib(dp->first.Id())) {
                        continue;
                }
                ldlibs.push_back(dp->second);
            }
        }
        if (!ldlibs.empty()) {
            string proj_link( GetUUID());
            CRef<CDict> dict_link( AddDict( dict_objects, proj_link));
            CRef<CArray> link_libs( AddArray( *dict_link, "files"));
            ITERATE( list<CProjItem>, d, ldlibs) {
                string lib_ref( GetUUID());
                {
                    CRef<CDict> dict_lib_ref( AddDict( dict_objects, lib_ref));
                    AddString( *dict_lib_ref, "fileRef", m_TargetProduct[GetProjId(*d)]);
                    AddString( *dict_lib_ref, "isa", "PBXBuildFile");
                }
                AddString( *link_libs, lib_ref);
            }
            AddString( *dict_link, "isa", "PBXFrameworksBuildPhase");
            return proj_link;
        }
    }
    return kEmptyStr;
}

string CMacProjectGenerator::CreateProjectBuildPhase(
    const CProjItem& prj,
    CDict& dict_objects, CRef<CArray>& build_files)
{
    if (prj.m_ProjType == CProjKey::eDataSpec || prj.m_ProjType == CProjKey::eMsvc) {
        return kEmptyStr;
    }
#if USE_VERBOSE_NAMES
    string proj_build(    GetProjBuild( prj));
#else
    string proj_build(    GetUUID());
#endif
    CRef<CDict> dict_build( AddDict( dict_objects, proj_build));
    AddArray( *dict_build, "files", build_files);
    AddString( *dict_build, "isa", "PBXSourcesBuildPhase");
    return proj_build;
}

void CMacProjectGenerator::CollectLibToLibDependencies(
    set<string>& dep, set<string>& visited,
    const CProjItem& lib, const CProjItem& lib_dep)
{
#if USE_VERBOSE_NAMES
    string lib_name(GetProjDependency(lib));
    string lib_dep_name(GetProjDependency(lib_dep));
#else
    string lib_name(lib.m_GUID);
    string lib_dep_name(lib_dep.m_GUID);
#endif
    if (GetApp().m_AllDllBuild) {
        dep.insert(lib_dep_name);
        return;
    }

    if (visited.find(lib_dep_name) != visited.end() ||
        lib_dep_name == lib_name) {
        return;
    }
    visited.insert(lib_dep_name);
    if (!lib_dep.m_DatatoolSources.empty() ||
        !lib_dep.m_ExportHeaders.empty() ||
        lib.m_UnconditionalDepends.find(
            CProjKey(lib_dep.m_ProjType, lib_dep.m_ID)) !=
            lib.m_UnconditionalDepends.end()) {
        dep.insert(lib_dep_name);
    }
    ITERATE(list<CProjKey>, p, lib_dep.m_Depends) {
        if (p->Type() == CProjKey::eLib) {
            CProjectItemsTree::TProjects::const_iterator n =
                m_Projects_tree.m_Projects.find(*p);
            if (n != m_Projects_tree.m_Projects.end()) {
                CollectLibToLibDependencies(dep, visited, lib, n->second);
            }
        }
    }
}

string CMacProjectGenerator::CreateProjectTarget(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects, CRef<CArray>& build_phases,
    const string& product_id)
{
#if USE_VERBOSE_NAMES
    string proj_target(   GetProjTarget(   prj));
#else
    string proj_target(   GetUUID());
#endif
    string target_name(   GetTargetName(   prj));
    string product_type(  GetProductType(  prj));
    CRef<CArray> dependencies( new CArray);
    string configs_prj(
        CreateProjectBuildConfigurations( prj, prj_files, dict_objects));
    CRef<CDict> dict_target( AddDict( dict_objects, proj_target));
    AddString( *dict_target, "buildConfigurationList",configs_prj);
    AddArray( *dict_target, "buildPhases", build_phases);

    list<string> proj_guid;
    set<string> lib_guid, visited;
    ITERATE( list<CProjKey>, d, prj.m_Depends) {
        CProjectItemsTree::TProjects::const_iterator
            dp = m_Projects_tree.m_Projects.find( *d);
        if ( dp != m_Projects_tree.m_Projects.end() &&
            (dp->first.Id() != prj.m_ID || dp->first.Type() != prj.m_ProjType)) {

            if (dp->first.Type() == CProjKey::eLib &&
                GetApp().GetSite().Is3PartyLib(dp->first.Id())) {
                    continue;
            }
            if (prj.m_ProjType == CProjKey::eLib && dp->first.Type() == CProjKey::eLib) {
                CollectLibToLibDependencies(lib_guid, visited, prj, dp->second);
                continue;
            }
#if USE_VERBOSE_NAMES
            proj_guid.push_back( GetProjDependency(dp->second));
#else
            proj_guid.push_back( dp->second.m_GUID);
#endif
        }
    }
    copy(lib_guid.begin(), lib_guid.end(), back_inserter(proj_guid));
    if (!proj_guid.empty()) {
        proj_guid.sort();
        proj_guid.unique();
        ITERATE(list<string>, p, proj_guid) {
            AddString( *dependencies, *p);
        }
    }
    AddArray(  *dict_target, "dependencies", dependencies);
    if (prj.m_ProjType == CProjKey::eDataSpec || prj.m_ProjType == CProjKey::eMsvc) {
        AddString( *dict_target, "isa", "PBXAggregateTarget");
    } else {
        AddString( *dict_target, "isa", "PBXNativeTarget");
    }
    AddString( *dict_target, "name", target_name);
    AddString( *dict_target, "productName", target_name);
    if (prj.m_ProjType != CProjKey::eDataSpec && prj.m_ProjType != CProjKey::eMsvc) {
        AddString( *dict_target, "productReference", product_id);
        AddString( *dict_target, "productType", product_type);
    }
    return proj_target;
}

string CMacProjectGenerator::CreateBuildConfigurations(CDict& dict_objects)
{
    CRef<CArray> build_settings( new CArray);
#if USE_VERBOSE_NAMES
    string bld_cfg(      "Build_Configuration_");
    string bld_cfg_list( "Build_Configurations");
#else
    string bld_cfg_list( GetUUID());
#endif

    ITERATE(list<SConfigInfo>, cfg, m_Configs) {
        if (cfg->m_rtType == SConfigInfo::rtSingleThreaded ||
            cfg->m_rtType == SConfigInfo::rtSingleThreadedDebug ||
            cfg->m_rtType == SConfigInfo::rtUnknown) {
            continue;
        }
#if USE_VERBOSE_NAMES
        string bld_cfg_name(bld_cfg + cfg->m_Name);
#else
        string bld_cfg_name( GetUUID());
#endif
        CreateBuildSettings( *AddDict( dict_objects, bld_cfg_name), *cfg);
        AddString( *build_settings, bld_cfg_name);
    }

    CRef<CDict> configs( AddDict( dict_objects, bld_cfg_list));
    AddArray(  *configs, "buildConfigurations", build_settings);
    AddString( *configs, "defaultConfigurationIsVisible", "0");
    AddString( *configs, "defaultConfigurationName", "ReleaseDLL");
    AddString( *configs, "isa", "XCConfigurationList");
    return bld_cfg_list;
}

string CMacProjectGenerator::CreateProjectBuildConfigurations(
    const CProjItem& prj, const CProjectFileCollector& prj_files,
    CDict& dict_objects)
{
    CRef<CArray> build_settings( new CArray);
    string proj_id(      GetProjId( prj) );
#if USE_VERBOSE_NAMES
    string bld_cfg(      proj_id + "_Build_Configuration_");
    string bld_cfg_list( proj_id + "_Build_Configurations");
#else
    string bld_cfg_list( GetUUID());
#endif

    ITERATE(list<SConfigInfo>, cfg, prj_files.GetEnabledConfigs()) {
        if (cfg->m_rtType == SConfigInfo::rtSingleThreaded ||
            cfg->m_rtType == SConfigInfo::rtSingleThreadedDebug ||
            cfg->m_rtType == SConfigInfo::rtUnknown) {
            continue;
        }
        prj.m_CheckConfigs.insert(cfg->GetConfigFullName());
#if USE_VERBOSE_NAMES
        string bld_cfg_name(bld_cfg + cfg->m_Name);
#else
        string bld_cfg_name( GetUUID());
#endif
        CreateProjectBuildSettings( prj, prj_files,
            *AddDict( dict_objects, bld_cfg_name), *cfg);
        AddString( *build_settings, bld_cfg_name);
    }

    CRef<CDict> configs( AddDict( dict_objects, bld_cfg_list));
    AddArray(  *configs, "buildConfigurations", build_settings);
    AddString( *configs, "defaultConfigurationIsVisible", "0");
    AddString( *configs, "defaultConfigurationName", "ReleaseDLL");
    AddString( *configs, "isa", "XCConfigurationList");
    return bld_cfg_list;
}

string CMacProjectGenerator::CreateAggregateBuildConfigurations(
    const string& target_name, CDict& dict_objects)
{
    CRef<CArray> build_settings( new CArray);
    string proj_id(      target_name );
#if USE_VERBOSE_NAMES
    string bld_cfg(      target_name + "_Build_Configuration_");
    string bld_cfg_list( target_name + "_Build_Configurations");
#else
    string bld_cfg_list( GetUUID());
#endif

    ITERATE(list<SConfigInfo>, cfg, m_Configs) {
        if (cfg->m_rtType == SConfigInfo::rtSingleThreaded ||
            cfg->m_rtType == SConfigInfo::rtSingleThreadedDebug ||
            cfg->m_rtType == SConfigInfo::rtUnknown) {
            continue;
        }
#if USE_VERBOSE_NAMES
        string bld_cfg_name(bld_cfg + cfg->m_Name);
#else
        string bld_cfg_name( GetUUID());
#endif
        CreateAggregateBuildSettings( target_name,
            *AddDict( dict_objects, bld_cfg_name), *cfg);
        AddString( *build_settings, bld_cfg_name);
    }

    CRef<CDict> configs( AddDict( dict_objects, bld_cfg_list));
    AddArray(  *configs, "buildConfigurations", build_settings);
    AddString( *configs, "defaultConfigurationIsVisible", "0");
    AddString( *configs, "defaultConfigurationName", "ReleaseDLL");
    AddString( *configs, "isa", "XCConfigurationList");
    return bld_cfg_list;
}

void CMacProjectGenerator::CreateBuildSettings(CDict& dict_cfg, const SConfigInfo& cfg)
{
    string tmp_str;
    list<string> tmp_list;

    CRef<CDict> settings( AddDict( dict_cfg, "buildSettings"));
    AddString( dict_cfg, "isa", "XCBuildConfiguration");
    AddString( dict_cfg, "name", cfg.m_Name);

    tmp_str = CMsvc7RegSettings::GetRequestedArchs();
    if (tmp_str.empty()) {
        tmp_str = GetApp().GetMetaMakefile().GetCompilerOpt("ARCHS", cfg);
    }
    tmp_list.clear();
    NStr::Split(tmp_str, LIST_SEPARATOR, tmp_list);
    CRef<CArray> archs( AddArray( *settings, "ARCHS"));
    ITERATE( list<string>, a, tmp_list) {
        AddString( *archs, *a);
    }

    AddCompilerSetting( *settings, cfg, "GCC_WARN_ABOUT_RETURN_TYPE");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_UNUSED_VARIABLE");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_EFFECTIVE_CPLUSPLUS_VIOLATIONS");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_HIDDEN_VIRTUAL_FUNCTIONS");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_NON_VIRTUAL_DESTRUCTOR");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_PEDANTIC");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_SHADOW");
    AddCompilerSetting( *settings, cfg, "GCC_WARN_SIGN_COMPARE");

    AddCompilerSetting( *settings, cfg, "GCC_DYNAMIC_NO_PIC");
    AddCompilerSetting( *settings, cfg, "GCC_ENABLE_FIX_AND_CONTINUE");
    AddCompilerSetting( *settings, cfg, "GCC_MODEL_TUNING");
    AddCompilerSetting( *settings, cfg, "GCC_ENABLE_CPP_EXCEPTIONS");
    AddCompilerSetting( *settings, cfg, "GCC_ENABLE_CPP_RTTI");

    AddCompilerSetting( *settings, cfg, "COPY_PHASE_STRIP");
    AddCompilerSetting( *settings, cfg, "GCC_GENERATE_DEBUGGING_SYMBOLS");
    AddCompilerSetting( *settings, cfg, "GCC_OPTIMIZATION_LEVEL");
    AddCompilerSetting( *settings, cfg, "DEBUG_INFORMATION_FORMAT");
    AddCompilerSetting( *settings, cfg, "GCC_DEBUGGING_SYMBOLS");

    AddCompilerSetting( *settings, cfg, "SDKROOT");
    AddCompilerSetting( *settings, cfg, "FRAMEWORK_SEARCH_PATHS");
    AddCompilerSetting( *settings, cfg, "USE_HEADERMAP");

    AddCompilerSetting( *settings, cfg, "OTHER_CPLUSPLUSFLAGS");

    AddLinkerSetting( *settings, cfg, "DEAD_CODE_STRIPPING");
    AddLinkerSetting( *settings, cfg, "PREBINDING");
//    AddLinkerSetting( *settings, cfg, "ZERO_LINK");
    if (cfg.m_rtType == SConfigInfo::rtMultiThreadedDebugDLL ||
        cfg.m_rtType == SConfigInfo::rtMultiThreadedDLL) {
        AddString( *settings, "STANDARD_C_PLUS_PLUS_LIBRARY_TYPE", "dynamic");
    } else {
        AddString( *settings, "STANDARD_C_PLUS_PLUS_LIBRARY_TYPE", "static");
    }
}

void CMacProjectGenerator::CreateProjectBuildSettings(
        const CProjItem& prj, const CProjectFileCollector& prj_files,
        CDict& dict_cfg, const SConfigInfo& cfg)
{
    const CMsvcMetaMakefile& metamake( GetApp().GetMetaMakefile());
    bool dll_build = GetApp().GetBuildType().GetType() == CBuildType::eDll;
    string def_lib_path(m_OutputDir + "lib/$(CONFIGURATION)");

    string libs_out(m_OutputDir + "lib/$(CONFIGURATION)");
    string libs_out_install(m_OutputDir + "lib/$(CONFIGURATION)");
// CreateProjectCopyBinScript takes care of copying them into bin; CXX-1609
//    string bins_out(m_OutputDir + "bin/$(CONFIGURATION)");
    string bins_out(m_OutputDir + "lib/$(CONFIGURATION)");
    string bins_out_install(m_OutputDir + "bin/$(CONFIGURATION)");

    string proj_dir("$(PROJECT_DIR)/");

//    string temp_dir("$(BUILD_DIR)/$(CONFIGURATION)");
    string temp_dir("$(OBJROOT)/$(CONFIGURATION)");
    string objroot(m_OutputDir + "build");

    CRef<CArray> lib_paths(new CArray);
    AddString(*lib_paths, def_lib_path);
    list<string> prj_lib_dirs;
    if (prj_files.GetLibraryDirs(prj_lib_dirs, cfg)) {
        ITERATE ( list<string>, f, prj_lib_dirs) {
            AddString( *lib_paths, GetRelativePath( *f));
        }
    }
    
    CRef<CDict> settings( AddDict( dict_cfg, "buildSettings"));
    AddString( dict_cfg, "isa", "XCBuildConfiguration");
    AddString( dict_cfg, "name", cfg.m_Name);

    if (prj.m_ProjType == CProjKey::eLib) {

        AddLibrarianSetting( *settings, cfg, "GCC_ENABLE_SYMBOL_SEPARATION");
        AddLibrarianSetting( *settings, cfg, "GCC_INLINES_ARE_PRIVATE_EXTERN");
        AddLibrarianSetting( *settings, cfg, "GCC_SYMBOLS_PRIVATE_EXTERN");

        AddString( *settings, "CONFIGURATION_BUILD_DIR", libs_out);
        AddString( *settings, "CONFIGURATION_TEMP_DIR", temp_dir);
        AddString( *settings, "INSTALL_PATH", proj_dir + libs_out_install);
        AddString( *settings, "OBJROOT", objroot);

    } else if (prj.m_ProjType == CProjKey::eDll) {

        string bld_out(libs_out);
        string bld_out_install(libs_out_install);
        if (prj.m_IsBundle) {
            bld_out = bins_out;
            bld_out_install = bins_out_install;
        }
        AddLinkerSetting( *settings, cfg, "GCC_INLINES_ARE_PRIVATE_EXTERN");
        AddLinkerSetting( *settings, cfg, "GCC_SYMBOLS_PRIVATE_EXTERN");

        AddString( *settings, "CONFIGURATION_BUILD_DIR", bld_out);
        AddString( *settings, "CONFIGURATION_TEMP_DIR", temp_dir);
        AddString( *settings, "INSTALL_PATH", proj_dir + bld_out_install);
        AddString( *settings, "OBJROOT", objroot);

        AddArray( *settings, "LIBRARY_SEARCH_PATHS", lib_paths);
        AddString( *settings, "EXECUTABLE_PREFIX", "lib");

    } else if (prj.m_ProjType == CProjKey::eApp) {

        AddLinkerSetting( *settings, cfg, "GCC_INLINES_ARE_PRIVATE_EXTERN");
        AddLinkerSetting( *settings, cfg, "GCC_SYMBOLS_PRIVATE_EXTERN");

        AddString( *settings, "CONFIGURATION_BUILD_DIR", bins_out);
        AddString( *settings, "CONFIGURATION_TEMP_DIR", temp_dir);
        AddString( *settings, "INSTALL_PATH", proj_dir + bins_out_install);
        AddString( *settings, "OBJROOT", objroot);

        AddArray( *settings, "LIBRARY_SEARCH_PATHS", lib_paths);
    }
    if (prj.m_ProjType != CProjKey::eDataSpec && prj.m_ProjType != CProjKey::eMsvc) {
        AddString( *settings, "MACH_O_TYPE", GetMachOType(prj));
    }

// library dependencies
    if (prj.m_ProjType == CProjKey::eDll || prj.m_ProjType == CProjKey::eApp) {
        string ldlib;
#if 0
        list<CProjItem> ldlibs;
        ITERATE( list<CProjKey>, d, prj.m_Depends) {
            CProjectItemsTree::TProjects::const_iterator
                dp = m_Projects_tree.m_Projects.find( *d);
            if ( dp != m_Projects_tree.m_Projects.end() &&
                (dp->first.Id() != prj.m_ID || dp->first.Type() != prj.m_ProjType) &&
                (dp->first.Type() == CProjKey::eLib || dp->first.Type() == CProjKey::eDll)) {

                if (dp->first.Type() == CProjKey::eLib &&
                    GetApp().GetSite().Is3PartyLib(dp->first.Id())) {
                        continue;
                }
                ldlibs.push_back(dp->second);
            }
        }
        ldlibs.sort(s_ProjItem_less);
        ITERATE( list<CProjItem>, d, ldlibs) {
            ldlib += string(" -l") + GetTargetName(*d);
        }
#endif
        string add;
        add = prj_files.GetProjectContext().GetMsvcProjectMakefile().GetLinkerOpt("OTHER_LDFLAGS",cfg);
        if (!add.empty()) {
            ldlib += " " + add;
        }
        add = prj_files.GetProjectContext().AdditionalLinkerOptions(cfg);
        if (!add.empty()) {
            ldlib += " " + add;
        }
        add = metamake.GetLinkerOpt("OTHER_LDFLAGS", cfg);
        if (!add.empty()) {
            ldlib += " " + add;
        }
        AddString( *settings, "OTHER_LDFLAGS", ldlib);
    }

    AddString( *settings, "PRODUCT_NAME", GetTargetName(prj));
    if (prj.m_ProjType == CProjKey::eDataSpec || prj.m_ProjType == CProjKey::eMsvc) {
        return;
    }

    CRef<CArray> inc_dirs( AddArray( *settings, "HEADER_SEARCH_PATHS"));
    list<string> prj_inc_dirs;
    if (prj_files.GetIncludeDirs(prj_inc_dirs, cfg)) {
        ITERATE ( list<string>, f, prj_inc_dirs) {
/*
            if (CSymResolver::HasDefine(*f)) {
                continue;
            }
*/
            AddString( *inc_dirs, GetRelativePath( *f));
        }
    }

// preprocessor definitions    
    list<string> tmp_list = prj.m_Defines;
    tmp_list = prj_files.GetProjectContext().Defines(cfg);
    string tmp_str = metamake.GetCompilerOpt("GCC_PREPROCESSOR_DEFINITIONS", cfg);
    NStr::Split(tmp_str, LIST_SEPARATOR, tmp_list);
    if (dll_build) {
        tmp_str = GetApp().GetConfig().Get(CMsvc7RegSettings::GetMsvcSection(), "DllBuildDefine");
        NStr::Split(tmp_str, LIST_SEPARATOR, tmp_list);
    }

    tmp_list.sort();
    tmp_list.unique();
    CRef<CArray> preproc( AddArray( *settings, "GCC_PREPROCESSOR_DEFINITIONS"));
    ITERATE( list<string>, a, tmp_list) {
        AddString( *preproc, *a);
    }

// precompiled header
    SConfigInfo cfg_tmp;
    if (prj_files.GetProjectContext().IsPchEnabled(cfg_tmp)) {
        string nofile = CDirEntry::ConcatPath(prj.m_SourcesBaseDir,"aanofile");
        string pch_name = prj_files.GetProjectContext().GetPchHeader(
            prj.m_ID, nofile, GetApp().GetProjectTreeInfo().m_Src, cfg_tmp);
        if (!pch_name.empty()) {
            string pch_path;
            // find header (MacOS requires? path)
            if (prj_files.GetIncludeDirs(prj_inc_dirs, cfg)) {
                ITERATE ( list<string>, f, prj_inc_dirs) {
                    if (pch_path.empty()) {
                        string t = CDirEntry::ConcatPath( *f, pch_name);
                        if (CDirEntry(t).IsFile()) {
                            pch_path = t;
                            break;
                        }
                    }
                }
            }
            if (pch_path.empty()) {
                pch_path = CDirEntry::ConcatPath(
                    GetApp().GetProjectTreeInfo().m_Include, pch_name);
            }
            AddString( *settings, "GCC_PRECOMPILE_PREFIX_HEADER", "YES");
            AddString( *settings, "GCC_PREFIX_HEADER", GetRelativePath( pch_path));
// for some unknown reason, when I define NCBI_USE_PCH
// xutil library does not compile
            string tmp(metamake.GetPchUsageDefine());
            if (!tmp.empty()) {
                AddString( *preproc, tmp);
            }
        }
    }
}

void CMacProjectGenerator::CreateAggregateBuildSettings(
    const string& target_name, CDict& dict_cfg, const SConfigInfo& cfg)
{
    CRef<CDict> settings( AddDict( dict_cfg, "buildSettings"));
    AddString( dict_cfg, "isa", "XCBuildConfiguration");
    AddString( dict_cfg, "name", cfg.m_Name);
    AddString( *settings, "PRODUCT_NAME", target_name);
}

string CMacProjectGenerator::AddAggregateTarget(
    const string& target_name, CDict& dict_objects, CRef<CArray>& dependencies)
{
#if USE_VERBOSE_NAMES
    string proj_target( target_name + "_target");
#else
    string proj_target( GetUUID());
#endif
    string configs_prj(
        CreateAggregateBuildConfigurations( target_name, dict_objects));
    CRef<CDict> dict_target( AddDict( dict_objects, proj_target));
    AddString( *dict_target, "buildConfigurationList", configs_prj);
    AddArray(  *dict_target, "buildPhases");
    AddString( *dict_target, "comments", NStr::NumericToString(dependencies->Get().size()) + " targets");
    AddArray(  *dict_target, "dependencies", dependencies);
    AddString( *dict_target, "isa", "PBXAggregateTarget");
    AddString( *dict_target, "name", target_name);
    AddString( *dict_target, "productName", target_name);
    return proj_target;
}

string CMacProjectGenerator::AddPreConfigureTarget(
    CArray& targets, CDict& dict_objects, const string& root_name)
{
    string target_name("_pre_CONFIGURE");
    string proj_target( GetUUID());
    string configs_prj(
        CreateAggregateBuildConfigurations( target_name, dict_objects));

    string build_tree_root (
        CDirEntry::DeleteTrailingPathSeparator( GetRelativePath(
        CDirEntry::AddTrailingPathSeparator( CDirEntry::ConcatPath(
            GetApp().GetProjectTreeInfo().m_Compilers,
            GetApp().GetRegSettings().m_CompilersSubdir)))));
    string script("$(PROJECT_DIR)/");
    script += build_tree_root + "/precfg.sh";

    CRef<CDict> dict_target( AddDict( dict_objects, proj_target));
    AddString( *dict_target, "buildArgumentsString", script);
    AddString( *dict_target, "buildConfigurationList", configs_prj);
    AddArray(  *dict_target, "buildPhases");
    AddString( *dict_target, "buildToolPath", "/bin/sh");
    AddArray(  *dict_target, "dependencies");
    AddString( *dict_target, "isa", "PBXLegacyTarget");
    AddString( *dict_target, "name", target_name);
    AddString( *dict_target, "passBuildSettingsInEnvironment", "1");
    AddString( *dict_target, "productName", target_name);

    InsertString(targets, proj_target);

    string proj_dependency( GetUUID());
    string proj_container(  GetUUID());
    // project dependency key
    {
        CRef<CDict> dict_dep( AddDict( dict_objects, proj_dependency));
        AddString( *dict_dep, "isa", "PBXTargetDependency");
        AddString( *dict_dep, "target", proj_target);
        AddString( *dict_dep, "targetProxy", proj_container);
    }
    // project container
    {
        CRef<CDict> dict_con( AddDict( dict_objects, proj_container));
        AddString( *dict_con, "containerPortal", root_name);
        AddString( *dict_con, "isa", "PBXContainerItemProxy");
        AddString( *dict_con, "proxyType", "1");
        AddString( *dict_con, "remoteGlobalIDString", proj_target);
        AddString( *dict_con, "remoteInfo", target_name);
    }
    return proj_dependency;
}

string CMacProjectGenerator::AddConfigureTarget(
    const string& solution_name, CDict& dict_objects, bool gui,
    const string& preconf_dependency)
{
    string target_name("CONFIGURE");
    if (gui) {
        target_name += "_DIALOG";
    }
#if USE_VERBOSE_NAMES
    string proj_target( target_name + "_target");
    string proj_script( target_name + "_script");
#else
    string proj_target( GetUUID());
    string proj_script( GetUUID());
#endif

    string configs_prj(
        CreateAggregateBuildConfigurations( target_name, dict_objects));

    string script;
    script += "export PTB_PLATFORM=\"$ARCHS\"\n";
    script += "export PTB_PATH=" + m_OutputDir + "../static/bin/ReleaseDLL\n";
    script += "export SLN_PATH=" + m_OutputDir + "\"$PROJECT_NAME\""/*solution_name*/ + "\n";
    script += "export TREE_ROOT=" +
        CDirEntry::DeleteTrailingPathSeparator( GetRelativePath( GetApp().m_Root)) + "\n";
    script += "export BUILD_TREE_ROOT=" +
        CDirEntry::DeleteTrailingPathSeparator( GetRelativePath(
        CDirEntry::AddTrailingPathSeparator( CDirEntry::ConcatPath(
            GetApp().GetProjectTreeInfo().m_Compilers,
            GetApp().GetRegSettings().m_CompilersSubdir)))) + "\n";
    script += m_OutputDir + "UtilityProjects/configure_";
    if (gui) {
        script += "gui_";
    }
    script += solution_name + ".sh";
    CRef<CDict> dict_script( AddDict( dict_objects, proj_script));
    AddArray(  *dict_script, "files");
    AddArray(  *dict_script, "inputPaths");
    AddArray(  *dict_script, "outputPaths");
    AddString( *dict_script, "isa", "PBXShellScriptBuildPhase");
    AddString( *dict_script, "shellPath", "/bin/sh");
    AddString( *dict_script, "shellScript", script);
    AddString( *dict_script, "showEnvVarsInLog", "0");

    CRef<CDict> dict_target( AddDict( dict_objects, proj_target));
    AddString( *dict_target, "buildConfigurationList", configs_prj);
    CRef<CArray> build_phases(AddArray(  *dict_target, "buildPhases"));
    AddString( *build_phases, proj_script);
    CRef<CArray> dependencies(AddArray(  *dict_target, "dependencies"));
    AddString( *dependencies, preconf_dependency);
    AddString( *dict_target, "isa", "PBXAggregateTarget");
    AddString( *dict_target, "name", target_name);
    AddString( *dict_target, "productName", target_name);
    return proj_target;
}

string CMacProjectGenerator::CreateRootObject(
    const string& configs_root, CDict& dict_objects, CRef<CArray>& targets,
    const string& root_group, const string& root_name, const string& products_group)
{
    CRef<CDict> root_obj( AddDict( dict_objects, root_name));
    AddString( *root_obj, "buildConfigurationList", configs_root);
    AddString( *root_obj, "compatibilityVersion", "Xcode 3.0");
    AddString( *root_obj, "hasScannedForEncodings", "1");
    AddString( *root_obj, "isa", "PBXProject");
    AddString( *root_obj, "mainGroup", root_group);
    AddString( *root_obj, "productRefGroup", products_group);
    AddString( *root_obj, "projectDirPath", "");
    AddString( *root_obj, "projectRoot", "");
    AddArray(  *root_obj, "targets", targets);
    return root_name;
}

string CMacProjectGenerator::GetUUID(void)
{
    static Uint4 uuid = 1;
    char buffer[64];
    ::sprintf(buffer, "ABCDABCDABCDABCD%08X", uuid++);
    return buffer;
}

string CMacProjectGenerator::AddFile(CDict& dict, const string& name, bool style_objcpp)
{
    string filetype;
    CDirEntry entry(name);
    string ext = entry.GetExt();
    if ( ext == ".cpp" || ext == ".c") {
        if (style_objcpp) {
            filetype = "sourcecode.cpp.objcpp";
        } else {
            filetype = string("sourcecode") + ext + ext;
        }
    } else if (ext == ".hpp" || ext == ".inl") {
        filetype = "sourcecode.cpp.h";
    } else if (ext == ".h") {
        filetype = "sourcecode.c.h";
    } else if (ext == ".xsd") {
        filetype = "text.xml";
    } else if (ext == ".wsdl") {
        filetype = "text.xml";
    } else {
        filetype = "text";
    }
    
    string base_name = entry.GetName();
#if USE_VERBOSE_NAMES
    static size_t counter = 0;
    string name_id = "FILE" + NStr::UIntToString(counter++);
#else
    string name_id( GetUUID());
#endif
    CRef<CDict> file( AddDict( dict, name_id));
    AddString( *file, "isa", "PBXFileReference");
    if (!filetype.empty()) {
        AddString( *file, "lastKnownFileType", filetype);
    }
    AddString( *file, "name", base_name);
    AddString( *file, "path", GetRelativePath(name));
    AddString( *file, "sourceTree", "SOURCE_ROOT");
    return name_id;
}

string CMacProjectGenerator::AddFileSource(CDict& dict, const string& name)
{
#if USE_VERBOSE_NAMES
    string name_ref = "SRC_" + name;
#else
    string name_ref( GetUUID());
#endif
    CRef<CDict> file( AddDict( dict, name_ref));
    AddString( *file, "fileRef", name);
    AddString( *file, "isa", "PBXBuildFile");
    return name_ref;
}

void  CMacProjectGenerator::AddGroupDict(
    CDict& dict, const string& key, CRef<CArray>& children, const string& name)
{
    CRef<CDict> group( AddDict( dict, key));
    AddArray(  *group, "children", children);
    AddString( *group, "isa", "PBXGroup");
    AddString( *group, "name", name);
    AddString( *group, "sourceTree", "<group>");
}


void CMacProjectGenerator::AddString(CArray& ar, const string& value)
{
    CRef<CArray::C_E> e(new CArray::C_E);
    e->SetString( value);
    ar.Set().push_back(e);
}

void CMacProjectGenerator::InsertString(CArray& ar, const string& value)
{
    CRef<CArray::C_E> e(new CArray::C_E);
    e->SetString( value);
    ar.Set().push_front(e);
}
    
void CMacProjectGenerator::AddString(
    CDict& dict, const string& key, const string& value)
{
    CRef<CDict::C_E> e(new CDict::C_E);
    e->SetKey(key);
    e->SetPlistObject().SetString(value);
    dict.Set().push_back(e);
}

CRef<CArray> CMacProjectGenerator::AddArray(
    CDict& dict, const string& key)
{
    CRef<CDict::C_E> e(new CDict::C_E);
    e->SetKey(key);
    CArray& a = e->SetPlistObject().SetArray();
    dict.Set().push_back(e);
    return CRef<CArray>(&a);
}

void CMacProjectGenerator::AddArray(
    CDict& dict, const string& key, CRef<CArray>& array)
{
    CRef<CDict::C_E> e(new CDict::C_E);
    e->SetKey(key);
    e->SetPlistObject().SetArray(*array);
    dict.Set().push_back(e);
}

CRef<CDict> CMacProjectGenerator::AddDict(
    CDict& dict, const string& key)
{
    CRef<CDict::C_E> e(new CDict::C_E);
    e->SetKey(key);
    CDict& d = e->SetPlistObject().SetDict();
    dict.Set().push_back(e);
    return CRef<CDict>(&d);
}

void CMacProjectGenerator::AddCompilerSetting(CDict& settings,
    const SConfigInfo& cfg, const string& key)
{
    string value = GetApp().GetMetaMakefile().GetCompilerOpt(key,cfg);
    if (!value.empty()) {
        AddString( settings, key, value);
    }
}
void CMacProjectGenerator::AddLinkerSetting(CDict& settings,
    const SConfigInfo& cfg, const string& key)
{
    string value = GetApp().GetMetaMakefile().GetLinkerOpt(key,cfg);
    if (!value.empty()) {
        AddString( settings, key, value);
    }
}
void CMacProjectGenerator::AddLibrarianSetting(CDict& settings,
    const SConfigInfo& cfg, const string& key)
{
    string value = GetApp().GetMetaMakefile().GetLibrarianOpt(key,cfg);
    if (!value.empty()) {
        AddString( settings, key, value);
    }
}

string CMacProjectGenerator::GetRelativePath(const string& name, const string* from) const
{
    if (!from) {
        from = &m_SolutionDir;
    }
    if (GetApp().UseAbsolutePath(*from) || !SameRootDirs(name, *from)) {
        return name;
    }
    string file_path;
    try {
        file_path = CDirEntry::CreateRelativePath(*from, name);
//  On REAL MacOS, it is not needed
#ifdef PSEUDO_XCODE
        NStr::ReplaceInPlace(file_path, "\\", "/");
#endif
    } catch (CFileException&) {
        file_path = name;
    }
    return file_path;
}

string CMacProjectGenerator::GetProjId( const CProjItem& prj)
{
    string id(prj.m_ID);
    if (prj.m_ProjType == CProjKey::eLib) {
        id += "_ar";
    } else if (prj.m_ProjType == CProjKey::eDll) {
        id += "_dylib";
    } else if (prj.m_ProjType == CProjKey::eApp) {
        id += "_exe";
    }
    return id;
}
string CMacProjectGenerator::GetProjTarget(const CProjItem& prj)
{
    return GetProjId(prj) + "_target";
}
string CMacProjectGenerator::GetProjBuild(const CProjItem& prj)
{
    return GetProjId(prj) + "_build";
}
string CMacProjectGenerator::GetProjProduct(const CProjItem& prj)
{
    return GetProjId(prj) + "_product";
}
string CMacProjectGenerator::GetProjHeaders(const CProjItem& prj)
{
    return GetProjId(prj) + "_headers";
}
string CMacProjectGenerator::GetProjDependency(  const CProjItem& prj)
{
    return GetProjId(prj) + "_dependency";
}
string CMacProjectGenerator::GetProjContainer(   const CProjItem& prj)
{
    return GetProjId(prj) + "_container";
}
string CMacProjectGenerator::GetTargetName( const CProjItem& prj)
{
    if (prj.m_ProjType == CProjKey::eLib) {
        return /*string("lib") +*/ prj.m_ID;
    }
    return prj.m_ID;
}

string CMacProjectGenerator::GetMachOType(  const CProjItem& prj)
{
    if (prj.m_ProjType == CProjKey::eLib) {
        return "staticlib";
    } else if (prj.m_ProjType == CProjKey::eDll) {
/*
        if (prj.m_IsBundle) {
            return "mh_bundle";
        }
*/
        return "mh_dylib";
    } else if (prj.m_ProjType == CProjKey::eApp) {
/*
        if (prj.m_IsBundle) {
            return "mh_bundle";
        }
*/
//        return "mh_executable";
        return "mh_execute";
    }
    return "";
}

string CMacProjectGenerator::GetProductType( const CProjItem& prj)
{
    if (prj.m_ProjType == CProjKey::eLib) {
        return "com.apple.product-type.library.static";
    } else if (prj.m_ProjType == CProjKey::eDll) {
        return "com.apple.product-type.library.dynamic";
    } else if (prj.m_ProjType == CProjKey::eApp) {
        if (prj.m_IsBundle) {
            return "com.apple.product-type.application";
        }
        return "com.apple.product-type.tool";
    }
    return "";
}
string CMacProjectGenerator::GetExplicitType( const CProjItem& prj)
{
    if (prj.m_ProjType == CProjKey::eLib) {
        return "archive.ar";
    } else if (prj.m_ProjType == CProjKey::eDll) {
        return "compiled.mach-o.dylib";
    } else if (prj.m_ProjType == CProjKey::eApp) {
        if (prj.m_IsBundle) {
            return "wrapper.application";
        }
        return "compiled.mach-o.executable";
    }
    return "";
}

#endif


END_NCBI_SCOPE
