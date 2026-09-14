/*
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
 * Authors: Amelia Fong
 */

/** @file blast_usage_report_unit_test.cpp
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbireg.hpp>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/api/blast_usage_report.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <vector>

#include "ensure_enough_corelib.hpp"

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);

BOOST_AUTO_TEST_SUITE(BLAST_USAGE_REPORT)

static const string kNcbiConfigOverridesEnv("NCBI_CONFIG_OVERRIDES");


static void s_WriteFile(const string& path, const string& contents)
{
    CNcbiOfstream out(path.c_str(), IOS_BASE::out | IOS_BASE::trunc |
                      IOS_BASE::binary);
    BOOST_REQUIRE(out.is_open());
    out << contents;
    out.close();
    BOOST_REQUIRE(out.good());
}


static string s_GetRegistryValue(CMemoryRegistry& registry,
                                 const string& section,
                                 const string& name)
{
    BOOST_REQUIRE(registry.HasEntry(section, name, IRegistry::fPersistent));
    return registry.Get(section, name, IRegistry::fPersistent);
}


static size_t s_CountOccurrences(const string& text, const string& pattern)
{
    size_t count = 0;
    string::size_type pos = text.find(pattern);
    while (pos != NPOS) {
        ++count;
        pos = text.find(pattern, pos + pattern.size());
    }
    return count;
}


static void s_CheckInherits(const string& inherits,
                            const vector<string>& expected)
{
    ITERATE(vector<string>, iter, expected) {
        BOOST_REQUIRE_NE(inherits.find(*iter), string::npos);
    }
}


class CBlastPhoneHomeTestFixture {
public:
    CBlastPhoneHomeTestFixture()
        : m_NcbiEnvFound(false), m_NcbiConfigOverridesFound(false)
    {
        Backup();
    }
    ~CBlastPhoneHomeTestFixture() { Restore(); }
    static constexpr string_view backup_ext = ".unit_test_backup";

    void Backup() {
        CDir home(CDir::GetHome());
        if (!home.Exists() && !home.CreatePath()) {
            NCBI_THROW(CBlastException, eSystem,
                       "Failed to create test home " + home.GetPath());
        }
        x_BackupFile(CBlastPhoneHomePolicy::GetLocalNcbiConfigFilePath());
        x_BackupConfigs();
    }

    void BackupFile(const string& path) { x_BackupFile(path); }

    void Restore() {
        ITERATE(vector<SFileBackup>, iter, m_FileBackup) {
            x_RestoreFile(*iter);
        }
        m_FileBackup.clear();
        x_RestoreConfigs();
    }


private:
    enum class Config_type {
        e_DoNotTrack,
        e_NCBIUsageReportEnv,
        e_BlastUsageReportEnv,
        e_NCBIUsageReportRegistry,
        e_BlastUsageReportRegistry
    };

    struct SConfig {
        SConfig(Config_type t, const string v):type(t), value(v){}
        const Config_type type;
        const string value;
    };

    struct SFileBackup {
        SFileBackup(const string& p, bool e):path(p), existed(e){}
        const string path;
        const bool existed;
    };

    void x_BackupConfigs();
    void x_RestoreConfigs();
    void x_BackupFile(const string& path);
    void x_RestoreFile(const SFileBackup& backup);
    vector<SFileBackup> m_FileBackup;
    vector<SConfig> m_ConfigBackup;
    bool m_NcbiEnvFound;
    bool m_NcbiConfigOverridesFound;
    string m_NcbiEnvValue;
    string m_NcbiConfigOverridesValue;

};

void CBlastPhoneHomeTestFixture::x_BackupFile(const string& path)
{
    const string backup = path + string(backup_ext);
    if (CFile(backup).Exists()) {
        CDirEntry(backup).RemoveEntry();
    }

    CFile config_file(path);
    if (config_file.Exists()) {
        if (!config_file.Copy(backup,
                              CFile::fCF_Overwrite |
                              CFile::fCF_PreserveAll)) {
            NCBI_THROW(CBlastException, eSystem,
                       "Failed to backup " + path);
        }
        if (!CDirEntry(path).RemoveEntry()) {
            NCBI_THROW(CBlastException, eSystem,
                       "Failed to remove " + path);
        }
        m_FileBackup.push_back(SFileBackup(path, true));
    }
    else {
        m_FileBackup.push_back(SFileBackup(path, false));
    }
}

void CBlastPhoneHomeTestFixture::x_RestoreFile(const SFileBackup& backup)
{
    const string backup_path = backup.path + string(backup_ext);
    if (backup.existed) {
        CFile backup_file(backup_path);
        if (backup_file.Exists() &&
            !backup_file.Copy(backup.path,
                              CFile::fCF_Overwrite |
                              CFile::fCF_PreserveAll)) {
            NCBI_THROW(CBlastException, eSystem,
                       "Failed to restore " + backup.path);
        }
        if (CFile(backup_path).Exists() &&
            !CDirEntry(backup_path).RemoveEntry()) {
            ERR_POST(Warning << "Failed to remove backup file: "
                     << backup_path);
        }
    }
    else if (CFile(backup.path).Exists() &&
             !CDirEntry(backup.path).RemoveEntry()) {
        ERR_POST(Warning << "Failed to remove test file: " << backup.path);
    }
}

void CBlastPhoneHomeTestFixture::x_BackupConfigs()
{
    _ASSERT(m_ConfigBackup.size() == 0);
    CNcbiEnvironment env;
    m_NcbiEnvValue = env.Get(CBlastPhoneHomePolicy::kNcbiEnv,
                             &m_NcbiEnvFound);
    env.Unset(CBlastPhoneHomePolicy::kNcbiEnv);
    m_NcbiConfigOverridesValue = env.Get(kNcbiConfigOverridesEnv,
                                         &m_NcbiConfigOverridesFound);
    env.Unset(kNcbiConfigOverridesEnv);

    string do_not_track_env = env.Get(CBlastPhoneHomePolicy::kDoNotTrackEnv);
    if(!do_not_track_env.empty()){
        SConfig c(Config_type::e_DoNotTrack, do_not_track_env);
        m_ConfigBackup.push_back(c);
        env.Unset(CBlastPhoneHomePolicy::kDoNotTrackEnv);
    }
    string usage_report_env = env.Get(CBlastPhoneHomePolicy::kUsageReportEnv);
    if(!usage_report_env.empty() ){
        SConfig c(Config_type::e_NCBIUsageReportEnv, usage_report_env);
        m_ConfigBackup.push_back(c);
        env.Unset(CBlastPhoneHomePolicy::kUsageReportEnv);
    }
    string blast_usage_env = env.Get(CBlastPhoneHomePolicy::kBlastUsageReportEnv);
    if(!blast_usage_env.empty()){
        SConfig c(Config_type::e_BlastUsageReportEnv, blast_usage_env);
        m_ConfigBackup.push_back(c);
        env.Unset(CBlastPhoneHomePolicy::kBlastUsageReportEnv);
    }
    CNcbiIstrstream empty_stream(kEmptyStr);
    CRef<CNcbiRegistry> registry(new CNcbiRegistry(empty_stream, IRegistry::fWithNcbirc));
    if (registry->HasEntry(CBlastPhoneHomePolicy::kNCBIUsageReportRegistry, CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam)) {
        SConfig c(Config_type::e_NCBIUsageReportRegistry,
                  registry->Get(CBlastPhoneHomePolicy::kNCBIUsageReportRegistry, CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam));
        m_ConfigBackup.push_back(c);
        registry->Unset(CBlastPhoneHomePolicy::kNCBIUsageReportRegistry, CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam);
    }
    if (registry->HasEntry(CBlastPhoneHomePolicy::kBlastUsageReportRegistry, CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam)) {
        SConfig c(Config_type::e_BlastUsageReportRegistry,
                  registry->Get(CBlastPhoneHomePolicy::kBlastUsageReportRegistry, CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam));
        m_ConfigBackup.push_back(c);
        registry->Unset(CBlastPhoneHomePolicy::kBlastUsageReportRegistry, CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam);
    }
}

void CBlastPhoneHomeTestFixture::x_RestoreConfigs()
{
    CNcbiEnvironment env;
    if(m_ConfigBackup.size()> 0) {
        CNcbiIstrstream empty_stream(kEmptyStr);
        CRef<CNcbiRegistry> registry(new CNcbiRegistry(empty_stream, IRegistry::fWithNcbirc));
        for(size_t i=0; i < m_ConfigBackup.size(); i++) {
            switch (m_ConfigBackup[i].type)
            {
                case Config_type::e_DoNotTrack:
                    env.Set(CBlastPhoneHomePolicy::kDoNotTrackEnv, m_ConfigBackup[i].value);
                    break;
                case Config_type::e_NCBIUsageReportEnv:
                    env.Set(CBlastPhoneHomePolicy::kUsageReportEnv, m_ConfigBackup[i].value);
                    break;
                case Config_type::e_BlastUsageReportEnv:
                    env.Set(CBlastPhoneHomePolicy::kBlastUsageReportEnv, m_ConfigBackup[i].value);
                    break;
                case Config_type::e_NCBIUsageReportRegistry:
                    registry->Set(CBlastPhoneHomePolicy::kNCBIUsageReportRegistry, CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam, m_ConfigBackup[i].value);
                    break;
                case Config_type::e_BlastUsageReportRegistry:
                    registry->Set(CBlastPhoneHomePolicy::kBlastUsageReportRegistry, CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam, m_ConfigBackup[i].value);
                    break;
                default:
                    NCBI_THROW(CException, eUnknown, "Invalid usage configuration type");
                    break;
            }
        }
    }

    if (m_NcbiEnvFound) {
        env.Set(CBlastPhoneHomePolicy::kNcbiEnv, m_NcbiEnvValue);
    }
    else {
        env.Unset(CBlastPhoneHomePolicy::kNcbiEnv);
    }
    if (m_NcbiConfigOverridesFound) {
        env.Set(kNcbiConfigOverridesEnv, m_NcbiConfigOverridesValue);
    }
    else {
        env.Unset(kNcbiConfigOverridesEnv);
    }
}

BOOST_FIXTURE_TEST_CASE(Blast_usage_report_timeout, CBlastPhoneHomeTestFixture)
{
    CUsageReportAPI::SetURL("http://iwebdev/staff/fongah2/blast_test/sleep_15s.cgi");
    CStopWatch sw(CStopWatch::eStart);
    {
        std::stringstream test_buffer;
        std::streambuf* cerr_buffer = std::cerr.rdbuf(test_buffer.rdbuf());
        CBlastUsageReport * report(new CBlastUsageReport());
        std::cerr.rdbuf(cerr_buffer);
        BOOST_REQUIRE_EQUAL(test_buffer.str(), CBlastPhoneHomePolicy::kPrivacyNotice);
        report->AddParam(CBlastUsageReport::eNumThreads, 1);
        delete report;
    }
    double t = sw.Elapsed();
    BOOST_REQUIRE(t < 15);
    sw.Restart();
    CUsageReportAPI::SetURL("http://iwebdev/staff/fongah2/blast_test/invalid");
    {
        std::stringstream test_buffer;
        std::streambuf* cerr_buffer = std::cerr.rdbuf(test_buffer.rdbuf());
        CBlastUsageReport * report(new CBlastUsageReport());
        std::cerr.rdbuf(cerr_buffer);
        BOOST_REQUIRE_EQUAL(test_buffer.str(), CBlastPhoneHomePolicy::kPrivacyNotice);
        report->AddParam(CBlastUsageReport::eNumThreads, 1);
        delete report;
    }
    t = sw.Elapsed();
    BOOST_REQUIRE(t < 10);
}

BOOST_FIXTURE_TEST_CASE(Check_usage_configurations, CBlastPhoneHomeTestFixture)
{
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_policy.ini");
    BackupFile(config_path);

    CBlastPhoneHomePolicy policy(config_path);
    policy.Restore();

    // Default is disable
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);

    CNcbiEnvironment env;
    // DO_NOT_TRACK=true disables usage report.
    env.Set(CBlastPhoneHomePolicy::kDoNotTrackEnv, NStr::BoolToString(true));
    policy.Restore();
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);

    // DO_NOT_TRACK with an invalid value silently means do not track.
    env.Set(CBlastPhoneHomePolicy::kDoNotTrackEnv, "dummy");
    env.Set(CBlastPhoneHomePolicy::kUsageReportEnv, NStr::BoolToString(true));
    policy.Restore();
    env.Unset(CBlastPhoneHomePolicy::kDoNotTrackEnv);
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);

    // NCBI_USAGE_REPORT_ENABLED (TRUE)
    policy.Restore();
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), true);

    env.Set(CBlastPhoneHomePolicy::kBlastUsageReportEnv, NStr::BoolToString(false));
    policy.Restore();
    env.Unset(CBlastPhoneHomePolicy::kUsageReportEnv);
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), true);

    // BLAST_USAGE_REPORT (FALSE)
    policy.Restore();
    env.Unset(CBlastPhoneHomePolicy::kBlastUsageReportEnv);
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);
    policy.SetUserUsageReportPreference(true);
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), true);

}

BOOST_FIXTURE_TEST_CASE(Save_usage_configuration_to_ncbirc,
                        CBlastPhoneHomeTestFixture)
{
    CNcbiEnvironment env;
    const string ncbi_dir = CDirEntry::MakePath(CDir::GetHome(), "ncbi_env");
    env.Set(CBlastPhoneHomePolicy::kNcbiEnv, ncbi_dir);
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_save.ini");
    BackupFile(config_path);

    CBlastPhoneHomePolicy policy(config_path);
    policy.SetUserUsageReportPreference(false);

    BOOST_REQUIRE(CFile(config_path).Exists());

    CMemoryRegistry registry;
    CBlastPhoneHomePolicy::ReadRegistryFile(config_path, registry);
    const vector<string> expected_inherits =
        CBlastPhoneHomePolicy::GetDefaultNcbiInherits();
    const string inherits =
        s_GetRegistryValue(registry,
                           CBlastPhoneHomePolicy::kNcbiRegistrySection,
                           CBlastPhoneHomePolicy::kNcbiInheritsParam);
    s_CheckInherits(inherits, expected_inherits);
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue
                            (registry,
                             CBlastPhoneHomePolicy::kBlastUsageReportRegistry,
                             CBlastPhoneHomePolicy::kBlastUsageReportKey),
                        "false");

    CBlastPhoneHomePolicy restored(config_path);
    restored.Restore();
    BOOST_REQUIRE(restored.HasUserUsageReportPreference());
}

BOOST_FIXTURE_TEST_CASE(Save_creates_missing_parent_directory,
                        CBlastPhoneHomeTestFixture)
{
    const string config_dir = CDirEntry::GetTmpNameEx
        (CDir::GetHome(), "blast_usage_report_missing_parent_",
         CDirEntry::eTmpFileGetName);
    const string config_path =
        CDirEntry::MakePath(config_dir,
                            CBlastPhoneHomePolicy::GetLocalNcbiConfigFileName());
    BackupFile(config_path);

    BOOST_REQUIRE(!CDir(config_dir).Exists());

    CBlastPhoneHomePolicy policy(config_path);
    policy.SetUserUsageReportPreference(false);

    BOOST_REQUIRE(CDir(config_dir).Exists());
    BOOST_REQUIRE(CFile(config_path).Exists());

    CMemoryRegistry registry;
    CBlastPhoneHomePolicy::ReadRegistryFile(config_path, registry);
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue
                            (registry,
                             CBlastPhoneHomePolicy::kBlastUsageReportRegistry,
                             CBlastPhoneHomePolicy::kBlastUsageReportKey),
                        "false");

    BOOST_REQUIRE(CDir(config_dir).Remove());
}

BOOST_FIXTURE_TEST_CASE(Check_inheritance_configuration_parameters,
                        CBlastPhoneHomeTestFixture)
{
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_inherit_params.ini");
    BackupFile(config_path);

    vector<string> expected_inherits;
    expected_inherits.push_back
        ("-" + CDirEntry::MakePath("$" + CBlastPhoneHomePolicy::kNcbiEnv,
             CBlastPhoneHomePolicy::GetLocalNcbiConfigFileName()));
#if defined(NCBI_OS_MSWIN)
    CNcbiEnvironment env;
    const string system_root = env.Get("SYSTEMROOT");
    if (!system_root.empty()) {
        expected_inherits.push_back
            ("-" + CDirEntry::MakePath
                (system_root,
                 CBlastPhoneHomePolicy::GetLocalNcbiConfigFileName()));
    }
#else
    expected_inherits.push_back
        ("-" + CDirEntry::MakePath("/etc", CNcbiRegistry::sm_SysRegName));
#endif

    const vector<string> default_inherits =
        CBlastPhoneHomePolicy::GetDefaultNcbiInherits();
    BOOST_REQUIRE_EQUAL_COLLECTIONS(default_inherits.begin(),
                                    default_inherits.end(),
                                    expected_inherits.begin(),
                                    expected_inherits.end());

    CBlastPhoneHomePolicy policy(config_path);
    policy.SetUserUsageReportPreference(false);

    CMemoryRegistry registry;
    CBlastPhoneHomePolicy::ReadRegistryFile(config_path, registry);
    BOOST_REQUIRE(registry.HasEntry
                      (CBlastPhoneHomePolicy::kNcbiRegistrySection,
                       CBlastPhoneHomePolicy::kNcbiInheritsParam,
                       IRegistry::fPersistent));

    const string inherits =
        s_GetRegistryValue(registry,
                           CBlastPhoneHomePolicy::kNcbiRegistrySection,
                           CBlastPhoneHomePolicy::kNcbiInheritsParam);
    BOOST_REQUIRE_EQUAL(s_CountOccurrences(inherits, ","),
                        expected_inherits.empty() ? size_t(0) :
                        expected_inherits.size() - 1);
    ITERATE(vector<string>, iter, expected_inherits) {
        BOOST_REQUIRE(NStr::StartsWith(*iter, "-"));
        BOOST_REQUIRE_EQUAL(s_CountOccurrences(inherits, *iter), size_t(1));
    }
}

BOOST_FIXTURE_TEST_CASE(Save_does_not_duplicate_default_inherits,
                        CBlastPhoneHomeTestFixture)
{
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_default_inherits.ini");
    BackupFile(config_path);

    const vector<string> expected_inherits =
        CBlastPhoneHomePolicy::GetDefaultNcbiInherits();
    string inherits;
    ITERATE(vector<string>, iter, expected_inherits) {
        if (!inherits.empty()) {
            inherits += ", ";
        }
        inherits += *iter;
    }

    s_WriteFile(config_path,
                string("[") +
                CBlastPhoneHomePolicy::kNcbiRegistrySection + "]\n" +
                CBlastPhoneHomePolicy::kNcbiInheritsParam + "=" +
                inherits + "\n");

    CBlastPhoneHomePolicy policy(config_path);
    policy.SetUserUsageReportPreference(false);

    CMemoryRegistry registry;
    CBlastPhoneHomePolicy::ReadRegistryFile(config_path, registry);
    inherits =
        s_GetRegistryValue(registry,
                           CBlastPhoneHomePolicy::kNcbiRegistrySection,
                           CBlastPhoneHomePolicy::kNcbiInheritsParam);
    ITERATE(vector<string>, iter, expected_inherits) {
        BOOST_REQUIRE_EQUAL(s_CountOccurrences(inherits, *iter), size_t(1));
    }
}

BOOST_FIXTURE_TEST_CASE(Environment_override_preserves_user_preference,
                        CBlastPhoneHomeTestFixture)
{
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_env_override.ini");
    BackupFile(config_path);

    CBlastPhoneHomePolicy policy(config_path);
    policy.SetUserUsageReportPreference(true);

    CNcbiEnvironment env;
    env.Set(CBlastPhoneHomePolicy::kBlastUsageReportEnv,
            NStr::BoolToString(false));
    policy.Restore();
    BOOST_REQUIRE(!policy.IsEnabled());

    CMemoryRegistry registry;
    CBlastPhoneHomePolicy::ReadRegistryFile(config_path, registry);
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue
                            (registry,
                             CBlastPhoneHomePolicy::kBlastUsageReportRegistry,
                             CBlastPhoneHomePolicy::kBlastUsageReportKey),
                        "true");

    env.Unset(CBlastPhoneHomePolicy::kBlastUsageReportEnv);
    policy.Restore();
    BOOST_REQUIRE(policy.IsEnabled());

    CMemoryRegistry updated_registry;
    CBlastPhoneHomePolicy::ReadRegistryFile(config_path, updated_registry);
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue
                            (updated_registry,
                             CBlastPhoneHomePolicy::kBlastUsageReportRegistry,
                             CBlastPhoneHomePolicy::kBlastUsageReportKey),
                        "true");
}

BOOST_FIXTURE_TEST_CASE(Save_preserves_existing_ncbirc,
                        CBlastPhoneHomeTestFixture)
{
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_preserve.ini");
    BackupFile(config_path);
    const string custom_inherit =
        CBlastPhoneHomePolicy::MakeOptionalNcbiInheritPath
            ("/custom", CNcbiRegistry::sm_SysRegName);
    s_WriteFile(config_path,
                string("[") +
                CBlastPhoneHomePolicy::kNcbiRegistrySection + "]\n" +
                CBlastPhoneHomePolicy::kNcbiInheritsParam + "=" +
                custom_inherit + "\n"
                "Other=true\n"
                "\n"
                "[" + CBlastPhoneHomePolicy::kBlastUsageReportRegistry + "]\n"
                "BLASTDB=/tmp/blastdb\n"
                "\n"
                "[OTHER]\n"
                "Value=1\n");

    CBlastPhoneHomePolicy policy(config_path);
    policy.SetUserUsageReportPreference(true);

    CMemoryRegistry registry;
    CBlastPhoneHomePolicy::ReadRegistryFile(config_path, registry);
    const vector<string> expected_inherits =
        CBlastPhoneHomePolicy::GetDefaultNcbiInherits();
    string inherits =
        s_GetRegistryValue(registry,
                           CBlastPhoneHomePolicy::kNcbiRegistrySection,
                           CBlastPhoneHomePolicy::kNcbiInheritsParam);
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue
                            (registry,
                             CBlastPhoneHomePolicy::kNcbiRegistrySection,
                             "Other"),
                        "true");
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue
                            (registry,
                             CBlastPhoneHomePolicy::kBlastUsageReportRegistry,
                             "BLASTDB"),
                        "/tmp/blastdb");
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue(registry, "OTHER", "Value"),
                        "1");
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue
                            (registry,
                             CBlastPhoneHomePolicy::kBlastUsageReportRegistry,
                             CBlastPhoneHomePolicy::kBlastUsageReportKey),
                        "true");
    BOOST_REQUIRE_NE(inherits.find(custom_inherit), string::npos);
    s_CheckInherits(inherits, expected_inherits);
    policy.SetUserUsageReportPreference(false);
    CMemoryRegistry updated_registry;
    CBlastPhoneHomePolicy::ReadRegistryFile(config_path, updated_registry);
    inherits =
        s_GetRegistryValue(updated_registry,
                           CBlastPhoneHomePolicy::kNcbiRegistrySection,
                           CBlastPhoneHomePolicy::kNcbiInheritsParam);
    BOOST_REQUIRE_EQUAL(s_GetRegistryValue
                            (updated_registry,
                             CBlastPhoneHomePolicy::kBlastUsageReportRegistry,
                             CBlastPhoneHomePolicy::kBlastUsageReportKey),
                        "false");
    ITERATE(vector<string>, iter, expected_inherits) {
        BOOST_REQUIRE_EQUAL(s_CountOccurrences(inherits, *iter), size_t(1));
    }
}

BOOST_FIXTURE_TEST_CASE(Check_usage_status_report, CBlastPhoneHomeTestFixture)
{
    const string enable = "TRUE";
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_status.ini");
    BackupFile(config_path);

    CBlastPhoneHomePolicy phone_home(config_path);
    phone_home.SetUserUsageReportPreference(false);
    string status_report = phone_home.PhoneHomeStatusReport();
    BOOST_REQUIRE_NE(status_report.find("Local NCBI config file"),
                     string::npos);
    BOOST_REQUIRE_NE(status_report.find
        (CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam),
        string::npos);

    CNcbiEnvironment env;
    env.Set(CBlastPhoneHomePolicy::kBlastUsageReportEnv, enable);
    status_report = phone_home.PhoneHomeStatusReport();
    env.Unset(CBlastPhoneHomePolicy::kBlastUsageReportEnv);
    BOOST_REQUIRE_NE(status_report.find(CBlastPhoneHomePolicy::kBlastUsageReportEnv), string::npos);

}

BOOST_FIXTURE_TEST_CASE(Check_invalid_usage_configurations,
                        CBlastPhoneHomeTestFixture)
{
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_invalid.ini");
    BackupFile(config_path);

    CNcbiEnvironment env;
    env.Set(kNcbiConfigOverridesEnv, config_path);

    s_WriteFile(config_path,
                string("[") +
                CBlastPhoneHomePolicy::kNCBIUsageReportRegistry + "]\n"
                "Enable=dummy\n");

    CBlastPhoneHomePolicy policy(config_path);
    policy.Restore();
    BOOST_REQUIRE(!policy.IsUsageConfigured());
    BOOST_REQUIRE(!policy.HasUserUsageReportPreference());
    BOOST_REQUIRE(!policy.IsEnabled());

    s_WriteFile(config_path,
                string("[") +
                CBlastPhoneHomePolicy::kNCBIUsageReportRegistry + "]\n" +
                CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam +
                "=dummy\n");
    policy.Restore();
    BOOST_REQUIRE(!policy.IsUsageConfigured());
    BOOST_REQUIRE(!policy.HasUserUsageReportPreference());
    BOOST_REQUIRE(!policy.IsEnabled());

    s_WriteFile(config_path,
                string("[") +
                CBlastPhoneHomePolicy::kBlastUsageReportRegistry + "]\n" +
                CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam +
                "=Nevermind\n");
    policy.Restore();
    BOOST_REQUIRE(!policy.IsUsageConfigured());
    BOOST_REQUIRE(!policy.HasUserUsageReportPreference());
    BOOST_REQUIRE(!policy.IsEnabled());

    env.Unset(kNcbiConfigOverridesEnv);
}

BOOST_FIXTURE_TEST_CASE(Check_inherited_ncbi_configuration_hierarchy,
                        CBlastPhoneHomeTestFixture)
{
    const string inherited_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_unit_test.ini");
    const string config_path =
        CDirEntry::MakePath(CDir::GetHome(),
                            "blast_usage_report_inherits.ini");
    BackupFile(inherited_path);
    BackupFile(config_path);

    s_WriteFile(inherited_path,
                string("[") +
                CBlastPhoneHomePolicy::kBlastUsageReportRegistry + "]\n" +
                CBlastPhoneHomePolicy::kBlastUsageReportKey + "=False\n");

    s_WriteFile(config_path,
                string("[") +
                CBlastPhoneHomePolicy::kNcbiRegistrySection + "]\n" +
                CBlastPhoneHomePolicy::kNcbiInheritsParam + "=" +
                inherited_path + "\n\n"
                "[" + CBlastPhoneHomePolicy::kNCBIUsageReportRegistry + "]\n" +
                CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam +
                "=TRUE\n");

    CNcbiEnvironment env;
    env.Set(kNcbiConfigOverridesEnv, config_path);

    CBlastPhoneHomePolicy policy(config_path);
    policy.Restore();
    BOOST_REQUIRE(policy.IsEnabled());
    const string status_report = policy.PhoneHomeStatusReport();
    BOOST_REQUIRE_NE(status_report.find
        ("[" + CBlastPhoneHomePolicy::kNCBIUsageReportRegistry + "] " +
         CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam),
        string::npos);
    BOOST_REQUIRE_NE(status_report.find
        ("[" + CBlastPhoneHomePolicy::kBlastUsageReportRegistry + "] " +
         CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam),
        string::npos);
    BOOST_REQUIRE_NE(status_report.find("true*"), string::npos);
    BOOST_REQUIRE_NE(status_report.find("false"), string::npos);
    env.Unset(kNcbiConfigOverridesEnv);
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
