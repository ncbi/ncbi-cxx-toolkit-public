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
#include "msvc_prj_utils.hpp"
#include "proj_builder_app.hpp"
#include "msvc_prj_defines.hpp"
#include "ptb_err_codes.hpp"
#include <corelib/ncbi_system.hpp>

#ifdef NCBI_COMPILER_MSVC
#  include <serial/objostrxml.hpp>
#  include <serial/objistr.hpp>
#  include <serial/serial.hpp>

#  include <objbase.h>
#endif



BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC

CVisualStudioProject* LoadFromXmlFile(const string& file_path)
{
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_Xml, 
                                                    file_path, 
                                                    eSerial_StdWhenAny));
    if ( in->fail() )
        NCBI_THROW(CProjBulderAppException, eFileOpen, file_path);
    
    auto_ptr<CVisualStudioProject> prj(new CVisualStudioProject());
    in->Read(prj.get(), prj->GetThisTypeInfo());
    return prj.release();
}


void SaveToXmlFile(const string& file_path, const CSerialObject& project)
{
    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(file_path, &dir);
    CDir project_dir(dir);
    if ( !project_dir.Exists() ) {
        CDir(dir).CreatePath();
    }

    CNcbiOfstream ofs(file_path.c_str(), 
                      IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    CObjectOStreamXml xs(ofs, false);
    if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc1000) {
        xs.SetReferenceSchema();
        xs.SetUseSchemaLocation(false);
    } else {
        xs.SetReferenceDTD(false);
    }
    xs.SetEncoding(eEncoding_UTF8);
    xs.SetDefaultStringEncoding(eEncoding_UTF8);

    xs << project;
}

void SaveIfNewer(const string& file_path, const CSerialObject& project)
{
    // If no such file then simple write it
    if ( !CDirEntry(file_path).Exists() ) {
        SaveToXmlFile(file_path, project);
        GetApp().RegisterGeneratedFile( file_path );
        PTB_WARNING_EX(file_path, ePTB_FileModified,
                       "Project created");
        return;
    }

    // Save new file to tmp path.
    string candidate_file_path = file_path + ".candidate";
    SaveToXmlFile(candidate_file_path, project);
    if (PromoteIfDifferent(file_path, candidate_file_path)) {
        PTB_WARNING_EX(file_path, ePTB_FileModified,
                       "Project updated");
    } else {
        PTB_TRACE("Left intact: " << file_path);
    }
}

void SaveIfNewer    (const string&        file_path, 
                     const CSerialObject& project,
                     const string& ignore)
{
    string candidate_file_path = file_path + ".candidate";
    SaveToXmlFile(candidate_file_path, project);
    if (!PromoteIfDifferent(file_path, candidate_file_path, ignore)) {
        PTB_TRACE("Left intact: " << file_path);
    }
}

#endif //NCBI_COMPILER_MSVC

bool PromoteIfDifferent(const string& present_path, 
                        const string& candidate_path,
                        const string& ignore)
{
    CNcbiIfstream ifs_present(present_path.c_str(), 
                              IOS_BASE::in | IOS_BASE::binary);
    if (ifs_present.is_open()) {
        CNcbiIfstream ifs_new (candidate_path.c_str(), 
                                  IOS_BASE::in | IOS_BASE::binary);
        if ( !ifs_new ) {
            NCBI_THROW(CProjBulderAppException, eFileOpen, candidate_path);
        }
        string str_present, str_new;
        bool eol_present=false, eol_new = false;
        for (;;) {
            eol_present=false;
            for (;;) {
                if (!NcbiGetlineEOL(ifs_present, str_present) ) {
                    break;
                }
                NStr::TruncateSpacesInPlace(str_present);
                if (!NStr::StartsWith(str_present, ignore)) {
                    eol_present=true;
                    break;
                }
            }
            eol_new = false;
            for (;;) {
                if (!NcbiGetlineEOL(ifs_new, str_new) ) {
                    break;
                }
                NStr::TruncateSpacesInPlace(str_new);
                if (!NStr::StartsWith(str_new, ignore)) {
                    eol_new=true;
                    break;
                }
            }
            if (!eol_present && !eol_new) {
                ifs_new.close();
                CDirEntry(candidate_path).Remove();
                return false;
            }
            if (NStr::CompareCase(str_present, str_new) != 0) {
                break;
            }
        }
        ifs_present.close();
    }
    CDirEntry(present_path).Remove();
    for (int a=0; a<2 && !CDirEntry(candidate_path).Rename(present_path); ++a)
        SleepSec(1);
    GetApp().RegisterGeneratedFile( present_path );
    return true;
}

bool PromoteIfDifferent(const string& present_path, 
                        const string& candidate_path)
{
    // Open both files
    CNcbiIfstream ifs_present(present_path.c_str(), 
                              IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs_present ) {
        CDirEntry(present_path).Remove();
        for (int a=0; a<2 && !CDirEntry(candidate_path).Rename(present_path); ++a)
            SleepSec(1);
        GetApp().RegisterGeneratedFile( present_path );
        return true;
    }

    ifs_present.seekg(0, ios::end);
    size_t file_length_present = ifs_present.tellg() - streampos(0);

    ifs_present.seekg(0, ios::beg);

    CNcbiIfstream ifs_new (candidate_path.c_str(), 
                              IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs_new ) {
        NCBI_THROW(CProjBulderAppException, eFileOpen, candidate_path);
    }

    ifs_new.seekg(0, ios::end);
    size_t file_length_new = ifs_new.tellg() - streampos(0);
    ifs_new.seekg(0, ios::beg);

    if (file_length_present != file_length_new) {
        ifs_present.close();
        ifs_new.close();
        CDirEntry(present_path).Remove();
        for (int a=0; a<2 && !CDirEntry(candidate_path).Rename(present_path); ++a)
            SleepSec(1);
        GetApp().RegisterGeneratedFile( present_path );
        return true;
    }

    // Load both to memory
    typedef AutoPtr<char, ArrayDeleter<char> > TAutoArray;
    TAutoArray buf_present = TAutoArray(new char [file_length_present]);
    TAutoArray buf_new     = TAutoArray(new char [file_length_new]);

    ifs_present.read(buf_present.get(), file_length_present);
    ifs_new.read    (buf_new.get(),     file_length_new);

    ifs_present.close();
    ifs_new.close();

    // If candidate file is not the same as present file it'll be a new file
    if (memcmp(buf_present.get(), buf_new.get(), file_length_present) != 0) {
        CDirEntry(present_path).Remove();
        for (int a=0; a<2 && !CDirEntry(candidate_path).Rename(present_path); ++a)
            SleepSec(1);
        GetApp().RegisterGeneratedFile( present_path );
        return true;
    } else {
        CDirEntry(candidate_path).Remove();
    }
    return false;
}

//-----------------------------------------------------------------------------

class CGuidGenerator
{
public:
    CGuidGenerator(void);
    ~CGuidGenerator(void);

    string DoGenerateSlnGUID();
    bool Insert(const string& guid, const string& path);
    const string& GetGuidUser(const string& guid) const;

private:
    string Generate12Chars(void);

    const string root_guid; // root GUID for MSVC solutions
    const string guid_base;
    unsigned int m_Seed;
    map<string,string> m_Trace;
};

CGuidGenerator guid_gen;

string GenerateSlnGUID(void)
{
    return guid_gen.DoGenerateSlnGUID();
}

string IdentifySlnGUID(const string& source_dir, const CProjKey& proj)
{
    string vcproj;
    if (proj.Type() == CProjKey::eMsvc) {
        vcproj = source_dir;
    } else {
        vcproj = GetApp().GetProjectTreeInfo().m_Compilers;
        vcproj = CDirEntry::ConcatPath(vcproj, 
            GetApp().GetRegSettings().m_CompilersSubdir);
        vcproj = CDirEntry::ConcatPath(vcproj, 
            GetApp().GetBuildType().GetTypeStr());
        vcproj = CDirEntry::ConcatPath(vcproj,
            GetApp().GetRegSettings().m_ProjectsSubdir);
        vcproj = CDirEntry::ConcatPath(vcproj, 
            CDirEntry::CreateRelativePath(
                GetApp().GetProjectTreeInfo().m_Src, source_dir));
        vcproj = CDirEntry::AddTrailingPathSeparator(vcproj);
        vcproj += CreateProjectName(proj);
        vcproj += CMsvc7RegSettings::GetVcprojExt();
    }
    string guid;
    if ( CDirEntry(vcproj).Exists() ) {
        char   buf[1024];
        CNcbiIfstream is(vcproj.c_str(), IOS_BASE::in);
        if (is.is_open()) {
            while ( !is.eof() ) {
                is.getline(buf, sizeof(buf));
                buf[sizeof(buf)-1] = char(0);
                string data(buf);
                string::size_type start, end;
                start = data.find("ProjectGUID");
                if (start == string::npos) {
                    start = data.find("ProjectGuid");
                }
                if (start != string::npos) {
                    start = data.find('{',start);
                    if (start != string::npos) {
                        end = data.find('}',++start);
                        if (end != string::npos) {
                            guid = data.substr(start,end-start);
                        }
                    }
                    break;
                }
            }
        }
    }
    if (!guid.empty() && !guid_gen.Insert(guid,vcproj)) {
        PTB_WARNING_EX(vcproj, ePTB_ConfigurationError,
                     "MSVC Project GUID already in use by "
                     << guid_gen.GetGuidUser(guid));
        if (proj.Type() != CProjKey::eMsvc) {
            guid.erase();
        }
    }
    if (!guid.empty()) {
        return  "{" + guid + "}";
    }
    return GenerateSlnGUID();
}


CGuidGenerator::CGuidGenerator(void)
    :root_guid("8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"),
     guid_base("8BC9CEB8-8B4A-11D0-8D11-"),
     m_Seed(0)
{
}


CGuidGenerator::~CGuidGenerator(void)
{
}


string CGuidGenerator::Generate12Chars(void)
{
    CNcbiOstrstream ost;
    ost.unsetf(ios::showbase);
    ost.setf  (ios::uppercase);
    ost << hex  << setw(12) << setfill('A') << m_Seed++ << ends << flush;
    return ost.str();
}

bool CGuidGenerator::Insert(const string& guid, const string& path)
{
    if (!guid.empty() && guid != root_guid) {
        if (m_Trace.find(guid) == m_Trace.end()) {
            m_Trace[guid] = path;
            return true;
        }
        if (!path.empty()) {
            return m_Trace[guid] == path;
        }
    }
    return false;
}

const string& CGuidGenerator::GetGuidUser(const string& guid) const
{
    map<string,string>::const_iterator i = m_Trace.find(guid);
    if (i != m_Trace.end()) {
       return i->second;
    }
    return kEmptyStr;
}

string CGuidGenerator::DoGenerateSlnGUID(void)
{
    for ( ;; ) {
        //GUID prototype
        string proto;// = guid_base + Generate12Chars();
#if NCBI_COMPILER_MSVC
        GUID guid;
        if (SUCCEEDED(CoCreateGuid(&guid))) {
            CNcbiOstrstream out;
            out.fill('0');
            out.flags(ios::hex | ios::uppercase);
            out << setw(8) << guid.Data1 << '-'
                << setw(4) << guid.Data2 << '-'
                << setw(4) << guid.Data3 << '-'
                << setw(2) << (unsigned int)guid.Data4[0]
                << setw(2) << (unsigned int)guid.Data4[1] << '-'
                << setw(2) << (unsigned int)guid.Data4[2]
                << setw(2) << (unsigned int)guid.Data4[3]
                << setw(2) << (unsigned int)guid.Data4[4]
                << setw(2) << (unsigned int)guid.Data4[5]
                << setw(2) << (unsigned int)guid.Data4[6]
                << setw(2) << (unsigned int)guid.Data4[7];
            proto = CNcbiOstrstreamToString(out);
        } else {
            proto = guid_base + Generate12Chars();
        }
#else
        proto = guid_base + Generate12Chars();
#endif
        if (Insert(proto,kEmptyStr)) {
            return "{" + proto + "}";
        }
    }
}

 
string SourceFileExt(const string& file_path)
{
    string ext;
    CDirEntry::SplitPath(file_path, NULL, NULL, &ext);
    
    bool explicit_c   = NStr::CompareNocase(ext, ".c"  )== 0;
    if (explicit_c  &&  CFile(file_path).Exists()) {
        return ".c";
    }
    bool explicit_cpp = NStr::CompareNocase(ext, ".cpp")== 0;
    if (explicit_cpp  &&  CFile(file_path).Exists()) {
        return ".cpp";
    }
    string file = file_path + ".cpp";
    if ( CFile(file).Exists() ) {
        return ".cpp";
    }
    file += ".in";
    if ( CFile(file).Exists() ) {
        return ".cpp.in";
    }
    file = file_path + ".c";
    if ( CFile(file).Exists() ) {
        return ".c";
    }
    file += ".in";
    if ( CFile(file).Exists() ) {
        return ".c.in";
    }
    return "";
}


//-----------------------------------------------------------------------------
SConfigInfo::SConfigInfo(void)
    :m_Debug(false), m_VTuneAddon(false), m_Unicode(false), m_rtType(rtUnknown)
{
}

SConfigInfo::SConfigInfo(const string& name, 
                         bool debug, 
                         const string& runtime_library)
    :m_Name          (name),
     m_RuntimeLibrary(runtime_library),
     m_Debug         (debug),
     m_VTuneAddon(false),
     m_Unicode(false),
     m_rtType(rtUnknown)
{
    DefineRtType();
}

void SConfigInfo::SetRuntimeLibrary(const string& lib)
{
    m_RuntimeLibrary = CMsvcMetaMakefile::TranslateOpt(lib, "Configuration", "RuntimeLibrary");
    DefineRtType();
}

void SConfigInfo::DefineRtType()
{
    if (m_RuntimeLibrary == "0" || m_RuntimeLibrary == "MultiThreaded") {
        m_rtType = rtMultiThreaded;
    } else if (m_RuntimeLibrary == "1" || m_RuntimeLibrary == "MultiThreadedDebug") {
        m_rtType = rtMultiThreadedDebug;
    } else if (m_RuntimeLibrary == "2" || m_RuntimeLibrary == "MultiThreadedDLL") {
        m_rtType = rtMultiThreadedDLL;
    } else if (m_RuntimeLibrary == "3" || m_RuntimeLibrary == "MultiThreadedDebugDLL") {
        m_rtType = rtMultiThreadedDebugDLL;
    } else if (m_RuntimeLibrary == "4") {
        m_rtType = rtSingleThreaded;
    } else if (m_RuntimeLibrary == "5") {
        m_rtType = rtSingleThreadedDebug;
    }
}

string SConfigInfo::GetConfigFullName(void) const
{
    if (m_VTuneAddon && m_Unicode) {
        return string("VTune_Unicode_") + m_Name;
    } else if (m_VTuneAddon) {
        return string("VTune_") + m_Name;
    } else if (m_Unicode) {
        return string("Unicode_") + m_Name;
    } else {
        return m_Name;
    }
}

bool SConfigInfo::operator== (const SConfigInfo& cfg) const
{
    return
        m_Name == cfg.m_Name &&
        m_Debug == cfg.m_Debug &&
        m_VTuneAddon == cfg.m_VTuneAddon &&
        m_Unicode == cfg.m_Unicode &&
        m_rtType == cfg.m_rtType;
}

bool s_Config_less(const SConfigInfo& x, const SConfigInfo& y)
{
    return NStr::CompareNocase(x.GetConfigFullName(), y.GetConfigFullName()) < 0;
}

void LoadConfigInfoByNames(const CNcbiRegistry& registry, 
                           const list<string>&  config_names, 
                           list<SConfigInfo>*   configs)
{
    ITERATE(list<string>, p, config_names) {

        const string& config_name = *p;
        SConfigInfo config;
        config.m_Name  = config_name;
        config.m_Debug = registry.GetString(config_name, 
                                            "debug",
                                            "FALSE") != "FALSE";
        config.SetRuntimeLibrary( registry.GetString(config_name, 
                                  "runtimeLibraryOption","0"));
        configs->push_back(config);
#if 0
        if (( config.m_Debug && GetApp().m_TweakVTuneD) ||
            (!config.m_Debug && GetApp().m_TweakVTuneR))
        {
            config.m_VTuneAddon = true;
            configs->push_back(config);
            config.m_VTuneAddon = false;
        }
#else
        if (GetApp().m_AddUnicode) {
            config.m_Unicode = true;
            configs->push_back(config);
        }
        if (GetApp().m_TweakVTuneR) {
            if (!config.m_Debug /*&& config.m_rtType == SConfigInfo::rtMultiThreadedDLL*/)
            {
                config.m_Unicode = false;
                config.m_VTuneAddon = true;
                configs->push_back(config);
                if (GetApp().m_AddUnicode) {
                    config.m_Unicode = true;
                    configs->push_back(config);
                }
            }
        }
#endif
    }
    configs->sort(s_Config_less);
}


//-----------------------------------------------------------------------------
#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eXCode30;
string CMsvc7RegSettings::sm_MsvcVersionName = "30";

CMsvc7RegSettings::EMsvcPlatform CMsvc7RegSettings::sm_MsvcPlatform =
    CMsvc7RegSettings::eXCode;
string CMsvc7RegSettings::sm_MsvcPlatformName = "ppc";

#elif defined(NCBI_COMPILER_MSVC)

#if _MSC_VER >= 1800
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
CMsvc7RegSettings::eMsvc1200;
string CMsvc7RegSettings::sm_MsvcVersionName = "1200";
#elif _MSC_VER >= 1700
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvc1100;
string CMsvc7RegSettings::sm_MsvcVersionName = "1100";
#elif _MSC_VER >= 1600
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvc1000;
string CMsvc7RegSettings::sm_MsvcVersionName = "1000";
#elif _MSC_VER >= 1500
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvc900;
string CMsvc7RegSettings::sm_MsvcVersionName = "900";
#elif _MSC_VER >= 1400
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvc800;
string CMsvc7RegSettings::sm_MsvcVersionName = "800";
#else
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvc710;
string CMsvc7RegSettings::sm_MsvcVersionName = "710";
#endif

#ifdef _WIN64
CMsvc7RegSettings::EMsvcPlatform CMsvc7RegSettings::sm_MsvcPlatform =
    CMsvc7RegSettings::eMsvcX64;
string CMsvc7RegSettings::sm_MsvcPlatformName = "x64";
#else
CMsvc7RegSettings::EMsvcPlatform CMsvc7RegSettings::sm_MsvcPlatform =
    CMsvc7RegSettings::eMsvcWin32;
string CMsvc7RegSettings::sm_MsvcPlatformName = "Win32";
#endif

#else // NCBI_COMPILER_MSVC

CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvcNone;
string CMsvc7RegSettings::sm_MsvcVersionName = "none";

CMsvc7RegSettings::EMsvcPlatform CMsvc7RegSettings::sm_MsvcPlatform =
    CMsvc7RegSettings::eUnix;
string CMsvc7RegSettings::sm_MsvcPlatformName = "Unix";

#endif // NCBI_COMPILER_MSVC
string CMsvc7RegSettings::sm_RequestedArchs = "";

void CMsvc7RegSettings::IdentifyPlatform()
{
#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
/*
    string native( CNcbiApplication::Instance()->GetEnvironment().Get("HOSTTYPE"));
    if (!native.empty()) {
        sm_MsvcPlatformName = native;
    }
    string xcode( CNcbiApplication::Instance()->GetEnvironment().Get("XCODE_VERSION_MAJOR"));
    if (xcode >= "0300") {
        sm_MsvcVersion = eXCode30;
        sm_MsvcVersionName = "30";
    }
    sm_MsvcPlatform = eXCode;
*/
//    sm_RequestedArchs = sm_MsvcPlatformName;

    int ide = GetApp().m_Ide;
    if (ide != 0) {
        int i = ide;
        if (i == 30) {
            sm_MsvcVersion = eXCode30;
            sm_MsvcVersionName = "30";
        } else {
            NCBI_THROW(CProjBulderAppException, eBuildConfiguration, "Unsupported IDE version");
        }
    }    
    if (!GetApp().m_Arch.empty()) {
        string a = GetApp().m_Arch;
        sm_MsvcPlatform = eXCode;
        sm_RequestedArchs = a;
        string tmp;
        NStr::SplitInTwo(a, " ", sm_MsvcPlatformName, tmp);
    }
#elif defined(NCBI_COMPILER_MSVC)
    int ide = GetApp().m_Ide;
    if (ide != 0) {
        switch (ide) {
        case 710:
            sm_MsvcVersion = eMsvc710;
            sm_MsvcVersionName = "710";
            break;
        case 800:
            sm_MsvcVersion = eMsvc800;
            sm_MsvcVersionName = "800";
            break;
        case 900:
            sm_MsvcVersion = eMsvc900;
            sm_MsvcVersionName = "900";
            break;
        case 1000:
            sm_MsvcVersion = eMsvc1000;
            sm_MsvcVersionName = "1000";
            break;
        case 1100:
            sm_MsvcVersion = eMsvc1100;
            sm_MsvcVersionName = "1100";
            break;
        case 1200:
            sm_MsvcVersion = eMsvc1200;
            sm_MsvcVersionName = "1200";
            break;
        default:
            NCBI_THROW(CProjBulderAppException, eBuildConfiguration, "Unsupported IDE version");
            break;
        }
    }    
    if (!GetApp().m_Arch.empty()) {
        string a = GetApp().m_Arch;
        if (a == "Win32") {
            sm_MsvcPlatform = eMsvcWin32;
            sm_RequestedArchs = sm_MsvcPlatformName = "Win32";
        } else if (a == "x64") {
            sm_MsvcPlatform = eMsvcX64;
            sm_RequestedArchs = sm_MsvcPlatformName = "x64";
        } else {
            NCBI_THROW(CProjBulderAppException, eBuildConfiguration, "Unsupported build platform");
        }
    }
#endif
}

string CMsvc7RegSettings::GetMsvcRegSection(void)
{
    if (GetMsvcPlatform() == eUnix) {
        return UNIX_REG_SECTION;
    } else if (GetMsvcPlatform() == eXCode) {
        return XCODE_REG_SECTION;
    }
    return MSVC_REG_SECTION;
}

string CMsvc7RegSettings::GetMsvcSection(void)
{
    string s(GetMsvcRegSection());
    if (GetMsvcPlatform() == eUnix) {
        return s;
    } else if (GetMsvcPlatform() == eXCode) {
        s += GetMsvcVersionName();
        return s;
    }
    s += GetMsvcVersionName();
    if (GetMsvcPlatform() != eMsvcWin32) {
        s += "." + GetMsvcPlatformName();
    }
    return s;
}

CMsvc7RegSettings::CMsvc7RegSettings(void)
{
}

string CMsvc7RegSettings::GetProjectFileFormatVersion(void)
{
    if (GetMsvcVersion() == eMsvc710) {
        return "7.10";
    } else if (GetMsvcVersion() == eMsvc800) {
        return "8.00";
    } else if (GetMsvcVersion() == eMsvc900) {
        return "9.00";
    } else if (GetMsvcVersion() == eMsvc1000 ||
               GetMsvcVersion() == eMsvc1100 ||
               GetMsvcVersion() == eMsvc1200) {
        return "10.0.30319.1";
    }
    return "";
}
string CMsvc7RegSettings::GetSolutionFileFormatVersion(void)
{
    if (GetMsvcVersion() == eMsvc710) {
        return "8.00";
    } else if (GetMsvcVersion() == eMsvc800) {
        return "9.00";
    } else if (GetMsvcVersion() == eMsvc900) {
        return "10.00\n# Visual Studio 2008";
    } else if (GetMsvcVersion() == eMsvc1000) {
        return "11.00\n# Visual Studio 2010";
    } else if (GetMsvcVersion() == eMsvc1100) {
        return "12.00\n# Visual Studio 2012";
    } else if (GetMsvcVersion() == eMsvc1200) {
        return "12.00\n# Visual Studio 2013";
    }
    return "";
}

string CMsvc7RegSettings::GetConfigNameKeyword(void)
{
    if (GetMsvcPlatform() < eUnix) {
        if (GetMsvcVersion() < eMsvc1000) {
            return MSVC_CONFIGNAME;
        } else {
            return "$(Configuration)";
        }
    } else if (GetMsvcPlatform() == eXCode) {
        return XCODE_CONFIGNAME;
    }
    return "";
}

string CMsvc7RegSettings::GetVcprojExt(void)
{
    if (GetMsvcPlatform() < eUnix) {
        if (GetMsvcVersion() < eMsvc1000) {
            return  MSVC_PROJECT_FILE_EXT;
        } else {
            return ".vcxproj";
        }
    } else if (GetMsvcPlatform() == eXCode) {
        return ".xcodeproj";
    }
    return "";
}

string CMsvc7RegSettings::GetTopBuilddir(void)
{
    string section(CMsvc7RegSettings::GetMsvcRegSection());
    string top( GetApp().GetConfig().GetString(section, "TopBuilddir", ""));
    if (!top.empty()) {
        top = CDirEntry::ConcatPath(CDirEntry(GetApp().m_Solution).GetDir(), top);
        if (!CFile(top).Exists()) {
            top.clear();
        }

    }
    return top;
}

bool IsSubdir(const string& abs_parent_dir, const string& abs_dir)
{
    return NStr::StartsWith(abs_dir, abs_parent_dir);
}


string GetOpt(const CPtbRegistry& registry, 
              const string& section, 
              const string& opt, 
              const string& config)
{
    string section_spec = section + '.' + config;
    string val_spec = registry.Get(section_spec, opt);
    if ( !val_spec.empty() )
        return val_spec;

    return registry.Get(section, opt);
}


string GetOpt(const CPtbRegistry& registry, 
              const string&        section, 
              const string&        opt, 
              const SConfigInfo&   config)
{
    const string& version = CMsvc7RegSettings::GetMsvcVersionName();
    const string& platform = CMsvc7RegSettings::GetMsvcPlatformName();
    string build = GetApp().GetBuildType().GetTypeStr();
    string spec = config.m_Debug ? "debug": "release";
    string cfgName(config.m_Name), cfgFullName(config.GetConfigFullName());
    string value, s;

    if (cfgName != cfgFullName) {
        s.assign(section).append(1,'.').append(build).append(1,'.').append(spec).append(1,'.').append(cfgFullName);
        value = registry.Get(s, opt); if (!value.empty()) { return value;}

        s.assign(section).append(1,'.').append(spec).append(1,'.').append(cfgFullName);
        value = registry.Get(s, opt); if (!value.empty()) { return value;}
    }
    s.assign(section).append(1,'.').append(build).append(1,'.').append(spec).append(1,'.').append(cfgName);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(spec).append(1,'.').append(cfgName);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(build).append(1,'.').append(spec);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(version).append(1,'.').append(spec);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(spec);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(build);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(platform);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(version);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}
    return value;
}



string ConfigName(const string& config)
{
    return config +'|'+ CMsvc7RegSettings::GetMsvcPlatformName();
}


//-----------------------------------------------------------------------------
#if NCBI_COMPILER_MSVC

CSrcToFilterInserterWithPch::CSrcToFilterInserterWithPch
                                        (const string&            project_id,
                                         const list<SConfigInfo>& configs,
                                         const string&            project_dir)
    : m_ProjectId  (project_id),
      m_Configs    (configs),
// see __USE_DISABLED_CFGS__ in msvc_prj_generator.cpp
#if 1
      m_AllConfigs (GetApp().GetRegSettings().m_ConfigInfo),
#else
      m_AllConfigs (configs),
#endif
      m_ProjectDir (project_dir)
{
}


CSrcToFilterInserterWithPch::~CSrcToFilterInserterWithPch(void)
{
}


void CSrcToFilterInserterWithPch::InsertFile(CRef<CFilter>&  filter, 
                                             const string&   rel_source_file,
                                             const string&   pch_default,
                                             const string&   enable_cfg)
{
    CRef<CFFile> file(new CFFile());
    file->SetAttlist().SetRelativePath(rel_source_file);
    //
    TPch pch_usage = DefinePchUsage(m_ProjectDir, rel_source_file, pch_default);
    //
    // For each configuration
    ITERATE(list<SConfigInfo>, iconfig, m_AllConfigs) {
        const string& config = (*iconfig).GetConfigFullName();
        CRef<CFileConfiguration> file_config(new CFileConfiguration());
        file_config->SetAttlist().SetName(ConfigName(config));
        
        if (m_Configs.size() != m_AllConfigs.size() &&
            find(m_Configs.begin(), m_Configs.end(), *iconfig) == m_Configs.end()) {
            file_config->SetAttlist().SetExcludedFromBuild("true");
        }
        else if ( !enable_cfg.empty()  &&  enable_cfg != config ) {
            file_config->SetAttlist().SetExcludedFromBuild("true");
        }

        CRef<CTool> compilerl_tool(new CTool());
        compilerl_tool->SetAttlist().SetName("VCCLCompilerTool");

        if (pch_usage.first == eCreate) {
            compilerl_tool->SetAttlist().SetPreprocessorDefinitions
                                (GetApp().GetMetaMakefile().GetPchUsageDefine());
            compilerl_tool->SetAttlist().SetUsePrecompiledHeader("1");
            compilerl_tool->SetAttlist().SetPrecompiledHeaderThrough
                                                            (pch_usage.second); 
        } else if (pch_usage.first == eUse) {
            compilerl_tool->SetAttlist().SetPreprocessorDefinitions
                                (GetApp().GetMetaMakefile().GetPchUsageDefine());
#if 0
// moved into msvc_prj_generator.cpp to become project default
            if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc800) {
                compilerl_tool->SetAttlist().SetUsePrecompiledHeader("2");
            } else {
                compilerl_tool->SetAttlist().SetUsePrecompiledHeader("3");
            }
#endif
            if (pch_usage.second != pch_default) {
                compilerl_tool->SetAttlist().SetPrecompiledHeaderThrough
                                                                (pch_usage.second);
            }
        }
        else {
            compilerl_tool->SetAttlist().SetUsePrecompiledHeader("0");
//            compilerl_tool->SetAttlist().SetPrecompiledHeaderThrough("");
        }
        file_config->SetTool(*compilerl_tool);
        file->SetFileConfiguration().push_back(file_config);
    }
    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    filter->SetFF().SetFF().push_back(ce);
    return;
}


void 
CSrcToFilterInserterWithPch::operator()(CRef<CFilter>&  filter, 
                                        const string&   rel_source_file,
                                        const string&   pch_default)
{
    if ( NStr::Find(rel_source_file, ".@config@") == NPOS ) {
        // Ordinary file
        InsertFile(filter, rel_source_file, pch_default);
    } else {
        // Configurable file

        // Exclude from build all file versions
        // except one for current configuration.
        ITERATE(list<SConfigInfo>, icfg, m_AllConfigs) {
            const string& cfg = (*icfg).GetConfigFullName();
            string source_file = NStr::Replace(rel_source_file,
                                               ".@config@", "." + cfg);
            InsertFile(filter, source_file, pch_default, cfg);
        }
    }
}

CSrcToFilterInserterWithPch::TPch 
CSrcToFilterInserterWithPch::DefinePchUsage(const string& project_dir,
                                            const string& rel_source_file,
                                            const string& pch_file)
{
    if ( pch_file.empty() )
        return TPch(eNotUse, "");

    string abs_source_file = 
        CDirEntry::ConcatPath(project_dir, rel_source_file);
    abs_source_file = CDirEntry::NormalizePath(abs_source_file);

    // .c files - not use PCH
    string ext;
    CDirEntry::SplitPath(abs_source_file, NULL, NULL, &ext);
    if ( NStr::CompareNocase(ext, ".c") == 0)
        return TPch(eNotUse, "");

/*
 MSVC documentation says:
 Although you can use only one precompiled header (.pch) file per source file,
 you can use multiple .pch files in a project.
 Apple XCode:
 Each target can have only one prefix header
*/
    if (m_PchHeaders.find(pch_file) == m_PchHeaders.end()) {
        // New PCH - this source file will create it
        m_PchHeaders.insert(pch_file);
        return TPch(eCreate, pch_file);
    } else {
        // Use PCH (previously created)
        return TPch(eUse, pch_file);
    }
}


//-----------------------------------------------------------------------------

CBasicProjectsFilesInserter::CBasicProjectsFilesInserter
                                (CVisualStudioProject*    vcproj,
                                const string&            project_id,
                                const list<SConfigInfo>& configs,
                                const string&            project_dir)
    : m_Vcproj     (vcproj),
      m_SrcInserter(project_id, configs, project_dir),
      m_Filters    (project_dir)
{
    m_Filters.Initilize();
}


CBasicProjectsFilesInserter::~CBasicProjectsFilesInserter(void)
{
}

void CBasicProjectsFilesInserter::AddSourceFile(
    const string& rel_file_path, const string& pch_default)
{
    m_Filters.AddSourceFile(m_SrcInserter, rel_file_path, pch_default);
}

void CBasicProjectsFilesInserter::AddHeaderFile(const string& rel_file_path)
{
    m_Filters.AddHeaderFile(rel_file_path);
}

void CBasicProjectsFilesInserter::AddInlineFile(const string& rel_file_path)
{
    m_Filters.AddInlineFile(rel_file_path);
}

void CBasicProjectsFilesInserter::Finalize(void)
{
    if (m_Filters.m_SourceFiles->IsSetFF()) {
        m_Vcproj->SetFiles().SetFilter().push_back(m_Filters.m_SourceFiles);
    }
    if ( m_Filters.m_HeaderFilesPrivate->IsSetFF() ) {
        CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
        ce->SetFilter(*m_Filters.m_HeaderFilesPrivate);
        m_Filters.m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
    if ( m_Filters.m_HeaderFilesImpl->IsSetFF() )  {
        CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
        ce->SetFilter(*m_Filters.m_HeaderFilesImpl);
        m_Filters.m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
    if (m_Filters.m_HeaderFiles->IsSetFF()) {
        m_Vcproj->SetFiles().SetFilter().push_back(m_Filters.m_HeaderFiles);
    }
    if (m_Filters.m_InlineFiles->IsSetFF()) {
        m_Vcproj->SetFiles().SetFilter().push_back(m_Filters.m_InlineFiles);
    }
}

//-----------------------------------------------------------------------------

static bool s_IsPrivateHeader(const string& header_abs_path)
{
    string src_dir = GetApp().GetProjectTreeInfo().m_Src;
    return header_abs_path.find(src_dir) != NPOS;

}

static bool s_IsImplHeader(const string& header_abs_path)
{
    string src_trait = CDirEntry::GetPathSeparator()        +
                       GetApp().GetProjectTreeInfo().m_Impl +
                       CDirEntry::GetPathSeparator();
    return header_abs_path.find(src_trait) != NPOS;
}

//-----------------------------------------------------------------------------

CBasicProjectsFilesInserter::SFiltersItem::SFiltersItem(void)
{
}
CBasicProjectsFilesInserter::SFiltersItem::SFiltersItem
                                                    (const string& project_dir)
    :m_ProjectDir(project_dir)
{
}

void CBasicProjectsFilesInserter::SFiltersItem::Initilize(void)
{
    m_SourceFiles.Reset(new CFilter());
    m_SourceFiles->SetAttlist().SetName("Source Files");
    m_SourceFiles->SetAttlist().SetFilter
            ("cpp;c;cxx;def;odl;idl;hpj;bat;asm;asmx");

    m_HeaderFiles.Reset(new CFilter());
    m_HeaderFiles->SetAttlist().SetName("Header Files");
    m_HeaderFiles->SetAttlist().SetFilter("h;hpp;hxx;hm;inc");

    m_HeaderFilesPrivate.Reset(new CFilter());
    m_HeaderFilesPrivate->SetAttlist().SetName("Private");
    m_HeaderFilesPrivate->SetAttlist().SetFilter("h;hpp;hxx;hm;inc");

    m_HeaderFilesImpl.Reset(new CFilter());
    m_HeaderFilesImpl->SetAttlist().SetName("Impl");
    m_HeaderFilesImpl->SetAttlist().SetFilter("h;hpp;hxx;hm;inc");

    m_InlineFiles.Reset(new CFilter());
    m_InlineFiles->SetAttlist().SetName("Inline Files");
    m_InlineFiles->SetAttlist().SetFilter("inl");
}


void CBasicProjectsFilesInserter::SFiltersItem::AddSourceFile
                           (CSrcToFilterInserterWithPch& inserter_w_pch,
                            const string&                rel_file_path,
                            const string&                pch_default)
{
    inserter_w_pch(m_SourceFiles, rel_file_path, pch_default);
}

void CBasicProjectsFilesInserter::SFiltersItem::AddHeaderFile
                                                  (const string& rel_file_path)
{
    CRef< CFFile > file(new CFFile());
    file->SetAttlist().SetRelativePath(rel_file_path);

    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    
    string abs_header_path = 
        CDirEntry::ConcatPath(m_ProjectDir, rel_file_path);
    abs_header_path = CDirEntry::NormalizePath(abs_header_path);
    if ( s_IsPrivateHeader(abs_header_path) ) {
        m_HeaderFilesPrivate->SetFF().SetFF().push_back(ce);
    } else if ( s_IsImplHeader(abs_header_path) ) {
        m_HeaderFilesImpl->SetFF().SetFF().push_back(ce);
    } else {
        m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
}


void CBasicProjectsFilesInserter::SFiltersItem::AddInlineFile
                                                  (const string& rel_file_path)
{
    CRef< CFFile > file(new CFFile());
    file->SetAttlist().SetRelativePath(rel_file_path);

    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    m_InlineFiles->SetFF().SetFF().push_back(ce);
}


//-----------------------------------------------------------------------------

CDllProjectFilesInserter::CDllProjectFilesInserter
                                (CVisualStudioProject*    vcproj,
                                 const CProjKey           dll_project_key,
                                 const list<SConfigInfo>& configs,
                                 const string&            project_dir)
    :m_Vcproj        (vcproj),
     m_DllProjectKey (dll_project_key),
     m_SrcInserter   (dll_project_key.Id(), 
                      configs, 
                      project_dir),
     m_ProjectDir    (project_dir),
     m_PrivateFilters(project_dir)
{
    // Private filters initilization
    m_PrivateFilters.m_SourceFiles.Reset(new CFilter());
    m_PrivateFilters.m_SourceFiles->SetAttlist().SetName("Source Files");
    m_PrivateFilters.m_SourceFiles->SetAttlist().SetFilter
            ("cpp;c;cxx;def;odl;idl;hpj;bat;asm;asmx");

    m_PrivateFilters.m_HeaderFiles.Reset(new CFilter());
    m_PrivateFilters.m_HeaderFiles->SetAttlist().SetName("Header Files");
    m_PrivateFilters.m_HeaderFiles->SetAttlist().SetFilter("h;hpp;hxx;hm;inc");

    m_PrivateFilters.m_HeaderFilesPrivate.Reset(new CFilter());
    m_PrivateFilters.m_HeaderFilesPrivate->SetAttlist().SetName("Private");
    m_PrivateFilters.m_HeaderFilesPrivate->SetAttlist().SetFilter("h;hpp;hxx;hm;inc");
    
    m_PrivateFilters.m_HeaderFilesImpl.Reset(new CFilter());
    m_PrivateFilters.m_HeaderFilesImpl->SetAttlist().SetName("Impl");
    m_PrivateFilters.m_HeaderFilesImpl->SetAttlist().SetFilter("h;hpp;hxx;hm;inc");

    m_PrivateFilters.m_InlineFiles.Reset(new CFilter());
    m_PrivateFilters.m_InlineFiles->SetAttlist().SetName("Inline Files");
    m_PrivateFilters.m_InlineFiles->SetAttlist().SetFilter("inl");

    // Hosted Libraries filter (folder)
    m_HostedLibrariesRootFilter.Reset(new CFilter());
    m_HostedLibrariesRootFilter->SetAttlist().SetName("Hosted Libraries");
    m_HostedLibrariesRootFilter->SetAttlist().SetFilter("");
}


CDllProjectFilesInserter::~CDllProjectFilesInserter(void)
{
}


void CDllProjectFilesInserter::AddSourceFile (
    const string& rel_file_path, const string& pch_default)
{
    string abs_path = CDirEntry::ConcatPath(m_ProjectDir, rel_file_path);
    abs_path = CDirEntry::NormalizePath(abs_path);
    
    CProjKey proj_key = GetApp().GetDllFilesDistr().GetSourceLib(abs_path, m_DllProjectKey);
    
    if (proj_key == CProjKey()) {
        m_PrivateFilters.AddSourceFile(m_SrcInserter, rel_file_path, pch_default);
        return;
    }

    THostedLibs::iterator p = m_HostedLibs.find(proj_key);
    if (p != m_HostedLibs.end()) {
        TFiltersItem& filters_item = p->second;
        filters_item.AddSourceFile(m_SrcInserter, rel_file_path, pch_default);
        return;
    }

    TFiltersItem new_item(m_ProjectDir);
    new_item.Initilize();
    new_item.AddSourceFile(m_SrcInserter, rel_file_path, pch_default);
    m_HostedLibs[proj_key] = new_item;
}

void CDllProjectFilesInserter::AddHeaderFile(const string& rel_file_path)
{
    string abs_path = CDirEntry::ConcatPath(m_ProjectDir, rel_file_path);
    abs_path = CDirEntry::NormalizePath(abs_path);
    
    CProjKey proj_key = GetApp().GetDllFilesDistr().GetHeaderLib(abs_path, m_DllProjectKey);
    
    if (proj_key == CProjKey()) {
        CProjectItemsTree::TProjects::const_iterator p = 
            GetApp().GetWholeTree().m_Projects.find(m_DllProjectKey);
        if (p != GetApp().GetWholeTree().m_Projects.end()) {
            const CProjItem& proj_item = p->second;
            if (NStr::StartsWith(abs_path, proj_item.m_SourcesBaseDir, NStr::eNocase)) {
                m_PrivateFilters.AddHeaderFile(rel_file_path);
            }
        }
        return;
    }

    THostedLibs::iterator p = m_HostedLibs.find(proj_key);
    if (p != m_HostedLibs.end()) {
        TFiltersItem& filters_item = p->second;
        filters_item.AddHeaderFile(rel_file_path);
        return;
    }

    TFiltersItem new_item(m_ProjectDir);
    new_item.Initilize();
    new_item.AddHeaderFile(rel_file_path);
    m_HostedLibs[proj_key] = new_item;
}

void CDllProjectFilesInserter::AddInlineFile(const string& rel_file_path)
{
    string abs_path = CDirEntry::ConcatPath(m_ProjectDir, rel_file_path);
    abs_path = CDirEntry::NormalizePath(abs_path);
    
    CProjKey proj_key = GetApp().GetDllFilesDistr().GetInlineLib(abs_path, m_DllProjectKey);
    
    if (proj_key == CProjKey()) {
        m_PrivateFilters.AddInlineFile(rel_file_path);
        return;
    }

    THostedLibs::iterator p = m_HostedLibs.find(proj_key);
    if (p != m_HostedLibs.end()) {
        TFiltersItem& filters_item = p->second;
        filters_item.AddInlineFile(rel_file_path);
        return;
    }

    TFiltersItem new_item(m_ProjectDir);
    new_item.Initilize();
    new_item.AddInlineFile(rel_file_path);
    m_HostedLibs[proj_key] = new_item;
}


void CDllProjectFilesInserter::Finalize(void)
{
    m_Vcproj->SetFiles().SetFilter().push_back(m_PrivateFilters.m_SourceFiles);

    if ( !m_PrivateFilters.m_HeaderFilesPrivate->IsSetFF() ) {
        CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
        ce->SetFilter(*m_PrivateFilters.m_HeaderFilesPrivate);
        m_PrivateFilters.m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
    if ( !m_PrivateFilters.m_HeaderFilesImpl->IsSetFF() ) {
        CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
        ce->SetFilter(*m_PrivateFilters.m_HeaderFilesImpl);
        m_PrivateFilters.m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
    m_Vcproj->SetFiles().SetFilter().push_back(m_PrivateFilters.m_HeaderFiles);

    m_Vcproj->SetFiles().SetFilter().push_back(m_PrivateFilters.m_InlineFiles);

    NON_CONST_ITERATE(THostedLibs, p, m_HostedLibs) {

        const CProjKey& proj_key     = p->first;
        TFiltersItem&   filters_item = p->second;

        CRef<CFilter> hosted_lib_filter(new CFilter());
        hosted_lib_filter->SetAttlist().SetName(CreateProjectName(proj_key));
        hosted_lib_filter->SetAttlist().SetFilter("");

        {{
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*(filters_item.m_SourceFiles));
            hosted_lib_filter->SetFF().SetFF().push_back(ce);
        }}

        if ( filters_item.m_HeaderFilesPrivate->IsSetFF() ) {
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*filters_item.m_HeaderFilesPrivate);
            filters_item.m_HeaderFiles->SetFF().SetFF().push_back(ce);
        }
        if ( filters_item.m_HeaderFilesImpl->IsSetFF() ) {
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*filters_item.m_HeaderFilesImpl);
            filters_item.m_HeaderFiles->SetFF().SetFF().push_back(ce);
        }
        {{
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*(filters_item.m_HeaderFiles));
            hosted_lib_filter->SetFF().SetFF().push_back(ce);
        }}
        {{
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*(filters_item.m_InlineFiles));
            hosted_lib_filter->SetFF().SetFF().push_back(ce);
        }}
        {{
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*hosted_lib_filter);
            m_HostedLibrariesRootFilter->SetFF().SetFF().push_back(ce);
        }}
    }
    m_Vcproj->SetFiles().SetFilter().push_back(m_HostedLibrariesRootFilter);
}


//-----------------------------------------------------------------------------

void AddCustomBuildFileToFilter(CRef<CFilter>&          filter, 
                                const list<SConfigInfo> configs,
                                const string&           project_dir,
                                const SCustomBuildInfo& build_info)
{
    CRef<CFFile> file(new CFFile());
    file->SetAttlist().SetRelativePath
        (CDirEntry::CreateRelativePath(project_dir, 
                                       build_info.m_SourceFile));

    ITERATE(list<SConfigInfo>, n, configs) {
        // Iterate all configurations
        const string& config = (*n).GetConfigFullName();

        CRef<CFileConfiguration> file_config(new CFileConfiguration());
        file_config->SetAttlist().SetName(ConfigName(config));

        CRef<CTool> custom_build(new CTool());
        custom_build->SetAttlist().SetName("VCCustomBuildTool");
        custom_build->SetAttlist().SetDescription(build_info.m_Description);
        custom_build->SetAttlist().SetCommandLine(build_info.m_CommandLine);
        custom_build->SetAttlist().SetOutputs(build_info.m_Outputs);
        custom_build->SetAttlist().SetAdditionalDependencies
                                      (build_info.m_AdditionalDependencies);
        file_config->SetTool(*custom_build);

        file->SetFileConfiguration().push_back(file_config);
    }
    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    filter->SetFF().SetFF().push_back(ce);
}

#endif //NCBI_COMPILER_MSVC


bool SameRootDirs(const string& dir1, const string& dir2)
{
    if ( dir1.empty() )
        return false;
    if ( dir2.empty() )
        return false;
#if NCBI_COMPILER_MSVC
        return tolower((unsigned char)(dir1[0])) == tolower((unsigned char)(dir2[0]));
//        return dir1[0] == dir2[0];
#else
    if (dir1[0] == '/' && dir2[0] == '/') {
        string::size_type n1= dir1.find_first_of('/', 1);
        string::size_type n2= dir2.find_first_of('/', 1);
        if (n1 != string::npos && n1 == n2) {
           return dir1.compare(0,n1,dir2,0,n2) == 0;
        }
    }
#endif
    return false;
}


string CreateProjectName(const CProjKey& project_id)
{
    switch (project_id.Type()) {
    case CProjKey::eApp:
        return project_id.Id() + ".exe";
    case CProjKey::eLib:
        return project_id.Id() + ".lib";
    case CProjKey::eDll:
        return project_id.Id() + ".dll";
    case CProjKey::eMsvc:
        if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
            return project_id.Id() + ".unix";
        }
        return project_id.Id();// + ".vcproj";
    case CProjKey::eDataSpec:
        return project_id.Id() + ".dataspec";
    case CProjKey::eUtility:
        return project_id.Id();
    default:
        NCBI_THROW(CProjBulderAppException, eProjectType, project_id.Id());
        return "";
    }
}

CProjKey CreateProjKey(const string& project_name)
{
    CProjKey::TProjType type = CProjKey::eNoProj;
    CDirEntry d(project_name);
    string ext(d.GetExt());
    string base(d.GetBase());
    if        (ext == ".exe") {
        type = CProjKey::eApp;
    } else if (ext == ".lib") {
        type = CProjKey::eLib;
    } else if (ext == ".dll") {
        type = CProjKey::eDll;
    } else if (ext == ".dataspec") {
        type = CProjKey::eDataSpec;
    } else {
        type = CProjKey::eMsvc;
    }
    return CProjKey(type,base);
}


//-----------------------------------------------------------------------------

CBuildType::CBuildType(bool dll_flag)
    :m_Type(dll_flag? eDll: eStatic)
{
    
}


CBuildType::EBuildType CBuildType::GetType(void) const
{
    return m_Type;
}
    

string CBuildType::GetTypeStr(void) const
{
    switch (m_Type) {
    case eStatic:
        return "static";
    case eDll:
        return "dll";
    }
    NCBI_THROW(CProjBulderAppException, 
               eProjectType, 
               NStr::IntToString(m_Type));
    return "";
}


//-----------------------------------------------------------------------------

CDllSrcFilesDistr::CDllSrcFilesDistr(void)
{
}

void CDllSrcFilesDistr::RegisterSource(const string&   src_file_path, 
                                       const CProjKey& dll_project_id,
                                       const CProjKey& lib_project_id)
{
    m_SourcesMap[ TDllSrcKey(src_file_path,dll_project_id) ] = lib_project_id;
}

void CDllSrcFilesDistr::RegisterHeader(const string&   hdr_file_path, 
                                       const CProjKey& dll_project_id,
                                       const CProjKey& lib_project_id)
{
    m_HeadersMap[ TDllSrcKey(hdr_file_path,dll_project_id) ] = lib_project_id;
}

void CDllSrcFilesDistr::RegisterInline(const string&   inl_file_path, 
                                       const CProjKey& dll_project_id,
                                       const CProjKey& lib_project_id)
{
    m_InlinesMap[ TDllSrcKey(inl_file_path,dll_project_id) ] = lib_project_id;
}

void CDllSrcFilesDistr::RegisterExtraFile(const string&   ex_file_path, 
                                          const CProjKey& dll_project_id,
                                          const CProjKey& lib_project_id)
{
    m_ExtraFileMap[ TDllSrcKey(ex_file_path,dll_project_id) ] = lib_project_id;
}

CProjKey CDllSrcFilesDistr::GetSourceLib(const string&   src_file_path, 
                                         const CProjKey& dll_project_id) const
{
    TDllSrcKey key(src_file_path, dll_project_id);
    TDistrMap::const_iterator p = m_SourcesMap.find(key);
    if (p != m_SourcesMap.end()) {
        const CProjKey& lib_id = p->second;
        return lib_id;
    }
    return CProjKey();
}


CProjKey CDllSrcFilesDistr::GetHeaderLib(const string&   hdr_file_path, 
                                         const CProjKey& dll_project_id) const
{
    TDllSrcKey key(hdr_file_path, dll_project_id);
    TDistrMap::const_iterator p = m_HeadersMap.find(key);
    if (p != m_HeadersMap.end()) {
        const CProjKey& lib_id = p->second;
        return lib_id;
    }
    return CProjKey();
}

CProjKey CDllSrcFilesDistr::GetInlineLib(const string&   inl_file_path, 
                                         const CProjKey& dll_project_id) const
{
    TDllSrcKey key(inl_file_path, dll_project_id);
    TDistrMap::const_iterator p = m_InlinesMap.find(key);
    if (p != m_InlinesMap.end()) {
        const CProjKey& lib_id = p->second;
        return lib_id;
    }
    return CProjKey();
}

CProjKey CDllSrcFilesDistr::GetExtraFileLib(const string&   ex_file_path, 
                                            const CProjKey& dll_project_id) const
{
    TDllSrcKey key(ex_file_path, dll_project_id);
    TDistrMap::const_iterator p = m_ExtraFileMap.find(key);
    if (p != m_ExtraFileMap.end()) {
        const CProjKey& lib_id = p->second;
        return lib_id;
    }
    return CProjKey();
}

CProjKey CDllSrcFilesDistr::GetFileLib(const string&   file_path, 
                          const CProjKey& dll_project_id) const
{
    CProjKey empty;
    if (dll_project_id.Type() != CProjKey::eDll) {
        return empty;
    }
    CProjKey test;
    test = GetSourceLib(file_path, dll_project_id);
    if (test != empty) {
        return test;
    }
    test = GetHeaderLib(file_path, dll_project_id);
    if (test != empty) {
        return test;
    }
    test = GetInlineLib(file_path, dll_project_id);
    if (test != empty) {
        return test;
    }
    test = GetExtraFileLib(file_path, dll_project_id);
    if (test != empty) {
        return test;
    }
    return empty;
}


const CDataToolGeneratedSrc*
IsProducedByDatatool(const string&    src_path_abs,
                     const CProjItem& project)
{
    if ( project.m_DatatoolSources.empty() ) {
        ITERATE( list<CProjKey>, d, project.m_Depends) {
            if (d->Type() == CProjKey::eDataSpec) {
                CProjectItemsTree::TProjects::const_iterator n = 
                               GetApp().GetWholeTree().m_Projects.find(*d);
                if (n != GetApp().GetWholeTree().m_Projects.end()) {
                    const CDataToolGeneratedSrc *pFound =
                        IsProducedByDatatool(src_path_abs, n->second);
                    if (pFound) {
                        return pFound;
                    }
                }
            }
        }
        return NULL;
    }

    string src_base;
    CDirEntry::SplitPath(src_path_abs, NULL, &src_base);

    // guess name.asn file name from name__ or name___
    string asn_base;
    if ( NStr::EndsWith(src_base, "___") ) {
        asn_base = src_base.substr(0, src_base.length() -3);
    } else if ( NStr::EndsWith(src_base, "__") ) {
        asn_base = src_base.substr(0, src_base.length() -2);
    } else {
        return NULL;
    }
    string asn_name = asn_base + ".asn";
    string dtd_name = asn_base + ".dtd";
    string xsd_name = asn_base + ".xsd";
    string wsdl_name = asn_base + ".wsdl";

    //try to find this name in datatool generated sources container
    ITERATE(list<CDataToolGeneratedSrc>, p, project.m_DatatoolSources) {
        const CDataToolGeneratedSrc& asn = *p;
        if ((asn.m_SourceFile == asn_name) ||
            (asn.m_SourceFile == dtd_name) ||
            (asn.m_SourceFile == xsd_name) ||
            (asn.m_SourceFile == wsdl_name)) {
            return &(*p);
        }
    }

    try {
        ITERATE(list<CDataToolGeneratedSrc>, p, project.m_DatatoolSources) {
            const CDataToolGeneratedSrc& asn = *p;
            string def( CDirEntry::ConcatPath(asn.m_SourceBaseDir, CDirEntry(asn.m_SourceFile).GetBase()) + ".def");
            if (CFile(def).Exists()) {
                CNcbiIfstream in(def.c_str(), IOS_BASE::in | IOS_BASE::binary);
                CNcbiRegistry reg(in);
                if (reg.GetString("-", "-oc", "") == asn_base) {
                    return &(*p);
                } 
            }
        }
    } catch (...) {
    }
    return NULL;
}

END_NCBI_SCOPE
