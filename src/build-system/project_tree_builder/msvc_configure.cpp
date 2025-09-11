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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <ncbi_pch.hpp>
#include "msvc_configure.hpp"
#include "proj_builder_app.hpp"
#include "util/random_gen.hpp"

#include "ptb_err_codes.hpp"
#ifdef NCBI_XCODE_BUILD
  #include <sys/utsname.h>
  #include <unistd.h>
#endif

#include <corelib/ncbiexec.hpp>
#include <util/xregexp/regexp.hpp>


BEGIN_NCBI_SCOPE


CMsvcConfigure::CMsvcConfigure(void)
    : m_HaveBuildVer(false), m_HaveRevision(false), m_HaveSignature(false)
{
}


CMsvcConfigure::~CMsvcConfigure(void)
{
}


void s_ResetLibInstallKey(const string& dir, 
                          const string& lib)
{
    string key_file_name(lib);
    NStr::ToLower(key_file_name);
    key_file_name += ".installed";
    string key_file_path = CDirEntry::ConcatPath(dir, key_file_name);
    if ( CDirEntry(key_file_path).Exists() ) {
        CDirEntry(key_file_path).Remove();
    }
}


static void s_CreateThirdPartyLibsInstallMakefile
                                            (CMsvcSite&   site, 
                                             const list<string> libs_to_install,
                                             const SConfigInfo& config,
                                             const CBuildType&  build_type)
{
    // Create makefile path
    string makefile_path = GetApp().GetProjectTreeInfo().m_Compilers;
    makefile_path = 
        CDirEntry::ConcatPath(makefile_path, 
                              GetApp().GetRegSettings().m_CompilersSubdir);

    makefile_path = CDirEntry::ConcatPath(makefile_path, build_type.GetTypeStr());
    makefile_path = CDirEntry::ConcatPath(makefile_path, config.GetConfigFullName());
    makefile_path = CDirEntry::ConcatPath(makefile_path, 
                                          "Makefile.third_party.mk");

    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(makefile_path, &dir);
    CDir makefile_dir(dir);
    if ( !makefile_dir.Exists() ) {
        CDir(dir).CreatePath();
    }

    CNcbiOfstream ofs(makefile_path.c_str(), 
                      IOS_BASE::out | IOS_BASE::trunc );
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, makefile_path);

    GetApp().RegisterGeneratedFile( makefile_path );
    ITERATE(list<string>, n, libs_to_install) {
        const string& lib = *n;
        SLibInfo lib_info;
        site.GetLibInfo(lib, config, &lib_info);
        if ( !lib_info.m_LibPath.empty() ) {
            string bin_dir = lib_info.m_LibPath;
            string bin_path = lib_info.m_BinPath;
            if (bin_path.empty()) {
                bin_path = site.GetThirdPartyLibsBinSubDir();
            }
            bin_dir = 
                CDirEntry::ConcatPath(bin_dir, bin_path);
            bin_dir = CDirEntry::NormalizePath(bin_dir);
            if ( CDirEntry(bin_dir).Exists() ) {
                //
                string key(lib);
                NStr::ToUpper(key);
                key += site.GetThirdPartyLibsBinPathSuffix();

                ofs << key << " = " << bin_dir << "\n";

                s_ResetLibInstallKey(dir, lib);
                site.SetThirdPartyLibBin(lib, bin_dir);
            } else {
                PTB_WARNING_EX(bin_dir, ePTB_PathNotFound,
                               lib << "|" << config.GetConfigFullName()
                               << " disabled, path not found");
            }
        } else {
            PTB_WARNING_EX(kEmptyStr, ePTB_PathNotFound,
                           lib << "|" << config.GetConfigFullName()
                           << ": no LIBPATH specified");
        }
    }
}


void CMsvcConfigure::Configure(CMsvcSite&         site, 
                               const list<SConfigInfo>& configs,
                               const string&            root_dir)
{
    _TRACE("*** Analyzing 3rd party libraries availability ***");
    
    site.InitializeLibChoices();
    InitializeFrom(site);
    site.ProcessMacros(configs);

    if (CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        return;
    }
    _TRACE("*** Creating Makefile.third_party.mk files ***");
    // Write makefile uses to install 3-rd party dlls
    list<string> third_party_to_install;
    site.GetThirdPartyLibsToInstall(&third_party_to_install);
    // For static buid
    ITERATE(list<SConfigInfo>, p, configs) {
        const SConfigInfo& config = *p;
        s_CreateThirdPartyLibsInstallMakefile(site, 
                                              third_party_to_install, 
                                              config, 
                                              GetApp().GetBuildType());
    }
}

void CMsvcConfigure::CreateConfH(
    CMsvcSite& site,
    const list<SConfigInfo>& configs,
    const string& root_dir)
{
    CMsvc7RegSettings::EMsvcPlatform platform = CMsvc7RegSettings::GetMsvcPlatform();

    ITERATE(list<SConfigInfo>, p, configs) {
        WriteExtraDefines(site, root_dir, *p);
        // Windows and XCode only. On Unix build version header file is generated by configure, not PTB.
        if (platform != CMsvc7RegSettings::eUnix) {
            WriteBuildVer(site, root_dir, *p);
        }
    }
    if (platform == CMsvc7RegSettings::eUnix) {
        return;
    }
    _TRACE("*** Creating local ncbiconf headers ***");
    const CBuildType& build_type(GetApp().GetBuildType());
    ITERATE(list<SConfigInfo>, p, configs) {
        /*if (!p->m_VTuneAddon && !p->m_Unicode)*/ {
            AnalyzeDefines( site, root_dir, *p, build_type);
        }
    }
}


void CMsvcConfigure::InitializeFrom(const CMsvcSite& site)
{
    m_ConfigSite.clear();

    m_ConfigureDefinesPath = site.GetConfigureDefinesPath();
    if ( m_ConfigureDefinesPath.empty() ) {
        NCBI_THROW(CProjBulderAppException, 
           eConfigureDefinesPath,
           "Configure defines file name is not specified");
    }

    site.GetConfigureDefines(&m_ConfigureDefines);
    if( m_ConfigureDefines.empty() ) {
        PTB_ERROR(m_ConfigureDefinesPath,
                  "No configurable macro definitions specified.");
    } else {
        _TRACE("Configurable macro definitions: ");
        ITERATE(list<string>, p, m_ConfigureDefines) {
            _TRACE(*p);
        }
    }
}


bool CMsvcConfigure::ProcessDefine(const string& define, 
                                   const CMsvcSite& site, 
                                   const SConfigInfo& config) const
{
    if ( !site.IsDescribed(define) ) {
        PTB_ERROR_EX(kEmptyStr, ePTB_MacroUndefined,
                     define << ": Macro not defined");
        return false;
    }
    list<string> components;
    site.GetComponents(define, &components);
    ITERATE(list<string>, p, components) {
        const string& component = *p;
        if (site.IsBanned(component)) {
            PTB_WARNING_EX("", ePTB_ConfigurationError,
                            component << "|" << config.GetConfigFullName()
                            << ": " << define << " not provided, disabled");
            return false;
        }
        if (site.IsProvided( component, false)) {
            continue;
        }
        SLibInfo lib_info;
        site.GetLibInfo(component, config, &lib_info);
        if ( !site.IsLibOk(lib_info)  ||
             !site.IsLibEnabledInConfig(component, config)) {
            if (!lib_info.IsEmpty()) {
                PTB_WARNING_EX("", ePTB_ConfigurationError,
                               component << "|" << config.GetConfigFullName()
                               << ": " << define << " not satisfied, disabled");
            }
            return false;
        }
    }
    return true;
}


void CMsvcConfigure::WriteExtraDefines(CMsvcSite& site, const string& root_dir, const SConfigInfo& config)
{
    string cfg = CMsvc7RegSettings::GetConfigNameKeyword();
    string cfg_root_inc(root_dir);
    if (!cfg.empty()) {
        NStr::ReplaceInPlace(cfg_root_inc,cfg,config.GetConfigFullName());
    }
    string extra = site.GetConfigureEntry("ExtraDefines");
    string filename = CDirEntry::ConcatPath(cfg_root_inc, extra);
    int random_count = NStr::StringToInt(site.GetConfigureEntry("RandomValueCount"), NStr::fConvErr_NoThrow);

    if (extra.empty() || random_count <= 0 || CFile(filename).Exists()) {
        return;
    }

    string dir;
    CDirEntry::SplitPath(filename, &dir);
    CDir(dir).CreatePath();

    CNcbiOfstream ofs(filename.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs ) {
	    NCBI_THROW(CProjBulderAppException, eFileCreation, filename);
    }
    WriteNcbiconfHeader(ofs);

    CRandom rnd(CRandom::eGetRand_Sys);
    string prefix("#define NCBI_RANDOM_VALUE_");
    ofs << prefix << "TYPE   Uint4" << endl;
    ofs << prefix << "MIN    0" << endl;
    ofs << prefix << "MAX    "  << std::hex 
        << std::showbase << rnd.GetMax() << endl << endl;
    for (int i = 0; i < random_count; ++i) {
        ofs << prefix  << std::noshowbase << i 
            << setw(16) << std::showbase << rnd.GetRand() << endl;
    }
    ofs << endl;
    GetApp().RegisterGeneratedFile( filename );
}



void CMsvcConfigure::WriteBuildVer(CMsvcSite& site, const string& root_dir, const SConfigInfo& config)
{
    static TXChar* filename_cache = NULL;
    
    string cfg = CMsvc7RegSettings::GetConfigNameKeyword();
    string cfg_root_inc(root_dir);
    if (!cfg.empty()) {
        NStr::ReplaceInPlace(cfg_root_inc, cfg, config.GetConfigFullName());
    }
    string extra = CDirEntry::ConvertToOSPath(site.GetConfigureEntry("BuildVerPath"));
    string filename = CDirEntry::ConcatPath(cfg_root_inc, extra);

    if (extra.empty()) {
        return;
    }

    string dir;
    CDirEntry::SplitPath(filename, &dir);
    CDir(dir).CreatePath();

    // Each file with build version information is the same for each configuration,
    // so create it once and copy to each configuration on next calls.

    if (filename_cache) {
        CFile src(_T_STDSTRING(filename_cache));
        CFile dst(filename);
        if (!dst.Exists() || src.IsNewer(dst.GetPath(),0)) {
            src.Copy(filename, CFile::fCF_Overwrite);
            GetApp().RegisterGeneratedFile( filename );
        }
        return;
    }

    // get vars
    string tc_ver   = GetApp().GetEnvironment().Get("TEAMCITY_VERSION");
    string tc_build = GetApp().GetEnvironment().Get("BUILD_NUMBER");
    string tc_build_id;
    string tc_prj   = GetApp().GetEnvironment().Get("TEAMCITY_PROJECT_NAME");
    string tc_conf  = GetApp().GetEnvironment().Get("TEAMCITY_BUILDCONF_NAME");
    string svn_rev  = GetApp().GetEnvironment().Get("SVNREV");
    string git_rev;
    string sc_ver   = GetApp().GetEnvironment().Get("SCVER");

    // Get content of the Teamcity property file, if any
    string prop_file_name = GetApp().GetEnvironment().Get("TEAMCITY_BUILD_PROPERTIES_FILE");
    string prop_file_info;
    if (!prop_file_name.empty()) {
        CNcbiIfstream is(prop_file_name.c_str(), IOS_BASE::in);
        if (is.good()) {
            NcbiStreamToString(&prop_file_info, is);
            // teamcity.build.id
            if (!prop_file_info.empty()) {
                CRegexp re("teamcity.build.id=(\\d+)");
                tc_build_id = re.GetMatch(prop_file_info, 0, 1);
            }
        }
    }
    string tree_root = GetApp().GetEnvironment().Get("TREE_ROOT");
    if (tree_root.empty()) {
        tree_root = GetApp().GetProjectTreeInfo().m_Root;
    }
    if (!tree_root.empty()) {
        // Get VCS info
        string tmp_file = CFile::GetTmpName();

        string cmd = "git -C " + tree_root + " log -1 --format=%h > " + tmp_file;
        TExitCode ret = CExec::System(cmd.c_str());
        if (ret == 0) {
            CNcbiIfstream is(tmp_file.c_str(), IOS_BASE::in);
            is >> git_rev;
            CFile(tmp_file).Remove();            
        }

        cmd = "svn info " + tree_root + " > " + tmp_file;
        ret = CExec::System(cmd.c_str());
        if (ret == 0) {
            // SVN client present, parse results
            string info;
            {
                CNcbiIfstream is(tmp_file.c_str(), IOS_BASE::in);
                NcbiStreamToString(&info, is);
                CFile(tmp_file).Remove();
                if (!info.empty()) {
                    CRegexp re("Revision: (\\d+)");
                    svn_rev = re.GetMatch(info, 0, 1);
                }
            }
            cmd = "svn info " + CDir::ConcatPath(tree_root, "src/build-system") + " > " + tmp_file;
            ret = CExec::System(cmd.c_str());
            if (ret == 0) {
                CNcbiIfstream is(tmp_file.c_str(), IOS_BASE::in);
                NcbiStreamToString(&info, is);
                CFile(tmp_file).Remove();
                if (!info.empty()) {
                    CRegexp re("/production/components/[^/]*/(\\d+)");
                    sc_ver = re.GetMatch(info, 0, 1);
                }
            }
        }
        else {
            // Fallback
            if (sc_ver.empty()) {
                sc_ver = GetApp().GetEnvironment().Get("NCBI_SC_VERSION");
            }
            // try to get revision bumber from teamcity property file
            if (svn_rev.empty() && !prop_file_info.empty()) {
                CRegexp re("build.vcs.number=(\\d+)");
                svn_rev = re.GetMatch(prop_file_info, 0, 1);
            }
        }
    }
    if (tc_build.empty()) {tc_build = "0";}
    if (sc_ver.empty())   {sc_ver = "0";}
    if (svn_rev.empty())  {svn_rev = "0";}

    map<string,string> build_vars;
    build_vars["NCBI_TEAMCITY_BUILD_NUMBER"] = tc_build;
    build_vars["NCBI_TEAMCITY_BUILD_ID"] = tc_build_id;
    build_vars["NCBI_TEAMCITY_PROJECT_NAME"] = tc_prj;
    build_vars["NCBI_TEAMCITY_BUILDCONF_NAME"] = tc_conf;
    build_vars["NCBI_REVISION"]
        = (git_rev.empty()  &&  svn_rev != "0") ? svn_rev : git_rev;
    build_vars["NCBI_SUBVERSION_REVISION"] = svn_rev;
    build_vars["NCBI_SC_VERSION"] = sc_ver;
    build_vars["NCBI_SIGNATURE"] = GetSignature(config);

    m_HaveRevision = !build_vars["NCBI_REVISION"].empty();

    auto converter = [&](const string& file_in, const string& file_out, const map<string,string>& vocabulary) {
        string candidate = file_out + ".candidate";
        CNcbiOfstream ofs(candidate.c_str(), IOS_BASE::out | IOS_BASE::trunc);
        if ( ofs.is_open() ) {

            CNcbiIfstream ifs(file_in.c_str());
            if (ifs.is_open()) {
                string line;
                while( getline(ifs, line) ) {
                    for (string::size_type from = line.find("@"); from != string::npos; from = line.find("@")) {
                        string::size_type  to = line.find("@", from + 1);
                        if (to != string::npos) {
                            string key = line.substr(from+1, to-from-1);
                            if (key == "NCBI_SIGNATURE") {
                                m_HaveSignature = true;
                            }
                            if (vocabulary.find(key) != vocabulary.end()) {
                                line.replace(from, to-from+1, vocabulary.at(key));
                            } else {
                                line.replace(from, to-from+1, "");
                            }
                        }
                    }
                    ofs << line << endl;
                }
                ifs.close();
            }
            ofs.close();
            PromoteIfDifferent(file_out, candidate);
        }
    };

    string file_in = CDirEntry::ConvertToOSPath(CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, "common/ncbi_build_ver.h.in"));
    converter(file_in, filename, build_vars); 
    // Cache file name for the generated file
    filename_cache = NcbiSys_strdup(_T_XCSTRING(filename));
    m_HaveBuildVer = true;

    filename = CDirEntry::ConvertToOSPath(CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, "common/ncbi_revision.h"));
    file_in = CDirEntry::ConvertToOSPath(CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, "common/ncbi_revision.h.in"));
    converter(file_in, filename, build_vars); 
}


void CMsvcConfigure::AnalyzeDefines(
    CMsvcSite& site, const string& root_dir,
    const SConfigInfo& config, const CBuildType&  build_type)
{
    string cfg_root_inc = NStr::Replace(root_dir,
        CMsvc7RegSettings::GetConfigNameKeyword(),config.GetConfigFullName());
    string filename =
        CDirEntry::ConcatPath(cfg_root_inc, m_ConfigureDefinesPath);
    string dir;
    CDirEntry::SplitPath(filename, &dir);

    _TRACE("Configuration " << config.m_Name << ":");

    m_ConfigSite.clear();

    ITERATE(list<string>, p, m_ConfigureDefines) {
        const string& define = *p;
        if( ProcessDefine(define, site, config) ) {
            _TRACE("Macro definition Ok  " << define);
            m_ConfigSite[define] = '1';
        } else {
            PTB_WARNING_EX(kEmptyStr, ePTB_MacroUndefined,
                           "Macro definition not satisfied: " << define);
            m_ConfigSite[define] = '0';
        }
    }
    if (m_HaveBuildVer) {
        // Add define that we have build version info file
        m_ConfigSite["HAVE_COMMON_NCBI_BUILD_VER_H"] = '1';
    }
    if (m_HaveRevision) {
        m_ConfigSite["HAVE_NCBI_REVISION"] = '1';
    }

    string signature;
    if (!m_HaveSignature) {
        signature = GetSignature(config);
    }
    string candidate_path = filename + ".candidate";
    CDirEntry::SplitPath(filename, &dir);
    CDir(dir).CreatePath();
    WriteNcbiconfMsvcSite(candidate_path, signature);
    if (PromoteIfDifferent(filename, candidate_path)) {
        PTB_WARNING_EX(filename, ePTB_FileModified,
                       "Configuration file modified");
    } else {
        PTB_INFO_EX(filename, ePTB_NoError,
                    "Configuration file unchanged");
    }
}

string CMsvcConfigure::GetSignature(const SConfigInfo& config)
{
    string signature;
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        signature = "MSVC";
    } else if (CMsvc7RegSettings::GetMsvcPlatform() > CMsvc7RegSettings::eUnix) {
        signature = "XCODE";
    } else {
        signature = CMsvc7RegSettings::GetMsvcPlatformName();
    }
    signature += "_";
    signature += CMsvc7RegSettings::GetMsvcVersionName();
    signature += "-" + config.GetConfigFullName();
    if (config.m_rtType == SConfigInfo::rtMultiThreadedDLL ||
        config.m_rtType == SConfigInfo::rtMultiThreadedDebugDLL) {
        signature += "MT";
    }
#ifdef NCBI_XCODE_BUILD
    string tmp = CMsvc7RegSettings::GetRequestedArchs();
    NStr::ReplaceInPlace(tmp, " ", "_");
    signature += "_" + tmp;
    signature += "--";
    struct utsname u;
    if (uname(&u) == 0) {
//        signature += string(u.machine) + string("-apple-") + string(u.sysname) + string(u.release);
        signature +=
            GetApp().GetSite().GetPlatformInfo(u.sysname, "arch", u.machine) +
            string("-apple-") +
            GetApp().GetSite().GetPlatformInfo(u.sysname, "os", u.sysname) +
            string(u.release);
    } else {
        signature += HOST;
    }
    signature += "-";
    {
        char hostname[255];
        string tmp1, tmp2;
        if (0 == gethostname(hostname, 255))
            NStr::SplitInTwo(hostname, ".", tmp1, tmp2);
        signature += tmp1;
    }
#else
    signature += "--";
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        signature += "i386-pc-";
        signature += CMsvc7RegSettings::GetRequestedArchs();
    } else {
        signature += HOST;
    }
    signature += "-";
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        signature += GetApp().GetEnvironment().Get("COMPUTERNAME");
    }
#endif
    return signature;
}

CNcbiOfstream& CMsvcConfigure::WriteNcbiconfHeader(CNcbiOfstream& ofs) const
{
    ofs <<"/* $" << "Id" << "$" << endl;
    ofs <<"* ===========================================================================" << endl;
    ofs <<"*" << endl;
    ofs <<"*                            PUBLIC DOMAIN NOTICE" << endl;
    ofs <<"*               National Center for Biotechnology Information" << endl;
    ofs <<"*" << endl;
    ofs <<"*  This software/database is a \"United States Government Work\" under the" << endl;
    ofs <<"*  terms of the United States Copyright Act.  It was written as part of" << endl;
    ofs <<"*  the author's official duties as a United States Government employee and" << endl;
    ofs <<"*  thus cannot be copyrighted.  This software/database is freely available" << endl;
    ofs <<"*  to the public for use. The National Library of Medicine and the U.S." << endl;
    ofs <<"*  Government have not placed any restriction on its use or reproduction." << endl;
    ofs <<"*" << endl;
    ofs <<"*  Although all reasonable efforts have been taken to ensure the accuracy" << endl;
    ofs <<"*  and reliability of the software and data, the NLM and the U.S." << endl;
    ofs <<"*  Government do not and cannot warrant the performance or results that" << endl;
    ofs <<"*  may be obtained by using this software or data. The NLM and the U.S." << endl;
    ofs <<"*  Government disclaim all warranties, express or implied, including" << endl;
    ofs <<"*  warranties of performance, merchantability or fitness for any particular" << endl;
    ofs <<"*  purpose." << endl;
    ofs <<"*" << endl;
    ofs <<"*  Please cite the author in any work or product based on this material." << endl;
    ofs <<"*" << endl;
    ofs <<"* ===========================================================================" << endl;
    ofs <<"*" << endl;
    ofs <<"* Author:  ......." << endl;
    ofs <<"*" << endl;
    ofs <<"* File Description:" << endl;
    ofs <<"*   ......." << endl;
    ofs <<"*" << endl;
    ofs <<"* ATTENTION:" << endl;
    ofs <<"*   Do not edit or commit this file into SVN as this file will" << endl;
    ofs <<"*   be overwritten (by PROJECT_TREE_BUILDER) without warning!" << endl;
    ofs <<"*/" << endl;
    ofs << endl;
    ofs << endl;
    return ofs;
}

void CMsvcConfigure::WriteNcbiconfMsvcSite(
    const string& full_path, const string& signature) const
{
    CNcbiOfstream  ofs(full_path.c_str(), 
                       IOS_BASE::out | IOS_BASE::trunc );
    if ( !ofs )
	    NCBI_THROW(CProjBulderAppException, eFileCreation, full_path);

    WriteNcbiconfHeader(ofs);

    ITERATE(TConfigSite, p, m_ConfigSite) {
        if (p->second == '1') {
            ofs << "#define " << p->first << " " << p->second << endl;
        } else {
            ofs << "/* #undef " << p->first << " */" << endl;
        }
    }
    ofs << endl;
    if (!signature.empty()) {
        ofs << "#define NCBI_SIGNATURE \\" << endl << "  \"" << signature << "\"" << endl;
    }

    list<string> customH;
    GetApp().GetCustomConfH(&customH);
    ITERATE(list<string>, c, customH) {
        string file (CDirEntry::CreateRelativePath(GetApp().m_Root, *c));
        NStr::ReplaceInPlace(file, "\\", "/");
        ofs << endl << "/*"
            << endl << "* ==========================================================================="
            << endl << "* Included contents of " << file
            << endl << "*/"
            << endl;

            CNcbiIfstream is(c->c_str(), IOS_BASE::in | IOS_BASE::binary);
            if ( !is ) {
                continue;
            }
            char   buf[1024];
            while ( is ) {
                is.read(buf, sizeof(buf));
                ofs.write(buf, is.gcount());
            }
    }
}


END_NCBI_SCOPE
