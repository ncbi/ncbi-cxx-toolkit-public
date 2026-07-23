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
BOOST_AUTO_TEST_CASE(Blast_usage_report_timeout)
{
	CUsageReportAPI::SetURL("http://iwebdev/staff/fongah2/blast_test/sleep_15s.cgi");
	CStopWatch sw(CStopWatch::eStart);
	{
	    std::stringstream test_buffer;
	    std::streambuf* cerr_buffer = std::cerr.rdbuf(test_buffer.rdbuf());
		CBlastUsageReport * report(new CBlastUsageReport());
		std::cerr.rdbuf(cerr_buffer);
		//BOOST_REQUIRE_EQUAL(test_buffer.str(), CBlastPhoneHomePolicy::kPrivacyNotice);
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
		//BOOST_REQUIRE_EQUAL(test_buffer.str(), CBlastPhoneHomePolicy::kPrivacyNotice);
		report->AddParam(CBlastUsageReport::eNumThreads, 1);
		delete report;
	}
	t = sw.Elapsed();
	BOOST_REQUIRE(t < 10);
}

class CBlastPhoneHomeTestFixture {
public:
    CBlastPhoneHomeTestFixture() : m_IniFilePath(kEmptyStr) { Backup();}
    static constexpr string_view backup_ext = ".unit_test_backup";

    void Backup() {
        const string home = CDir::GetHome();
        string path = CDirEntry::MakePath(home, ".blast-usage-report.ini");
        CFile optInFile (path);
        if (optInFile.Exists()){
            string backup = path + backup_ext;
            if (optInFile.Copy(backup, CFile::fCF_Overwrite | CFile::fCF_PreserveAll)) {
                m_IniFilePath = path;
                if (!CDirEntry(path).RemoveEntry()) {
                    ERR_POST(Warning << "Failed to remove backup file: " << backup );
                }
            }
            else {
                NCBI_THROW(CBlastException, eSystem , "Failed to backup blast usage ini file");
            }

        }
        x_BackupConfigs();
    }

    void Restore() {
        if (m_IniFilePath != kEmptyStr) {
            string backup = m_IniFilePath + backup_ext;
            CFile backupFile (backup);
            if (backupFile.Copy(m_IniFilePath, CFile::fCF_Overwrite | CFile::fCF_PreserveAll)) {
                m_IniFilePath = kEmptyStr;
                if (!CDirEntry(backup).RemoveEntry()) {
                    ERR_POST(Warning << "Failed to remove backup file: " << backup );
                }
            }
            else {
                NCBI_THROW(CBlastException, eSystem , "Failed to restore blast usage ini file");
            }
        }
        x_RestoreConfigs();
    }

    ~CBlastPhoneHomeTestFixture() { Restore(); }

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

    void x_BackupConfigs();
    void x_RestoreConfigs();
    string m_IniFilePath;
    vector<SConfig> m_ConfigBackup;

};

void CBlastPhoneHomeTestFixture::x_BackupConfigs()
{
    _ASSERT(m_ConfigBackup.size() == 0);
    CNcbiEnvironment env;
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
    if(m_ConfigBackup.size()> 0) {
        CNcbiEnvironment env;
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
}

BOOST_FIXTURE_TEST_CASE(Check_usage_configurations, CBlastPhoneHomeTestFixture)
{
    CBlastPhoneHomePolicy policy;
    policy.Restore();

    // Default is disable
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);

    CNcbiEnvironment env;
    // The present of Do_NOT_TRACK env means disable usage report
    env.Set(CBlastPhoneHomePolicy::kDoNotTrackEnv, NStr::BoolToString(false));
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);
    env.Set(CBlastPhoneHomePolicy::kDoNotTrackEnv, NStr::BoolToString(true));
    policy.CheckBlastUsageConfigurations();
    policy.UpdatePhoneHomeStatus();
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);

    // DO_NOT_TRACK has higher priority
    env.Set(CBlastPhoneHomePolicy::kUsageReportEnv, NStr::BoolToString(true));
    policy.CheckBlastUsageConfigurations();
    policy.UpdatePhoneHomeStatus();
    env.Unset(CBlastPhoneHomePolicy::kDoNotTrackEnv);
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);

    // NCBI_USAGE_REPORT_ENABLED (TRUE)
    policy.CheckBlastUsageConfigurations();
    policy.UpdatePhoneHomeStatus();
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), true);

    env.Set(CBlastPhoneHomePolicy::kBlastUsageReportEnv, NStr::BoolToString(false));
    policy.CheckBlastUsageConfigurations();
    policy.UpdatePhoneHomeStatus();
    env.Unset(CBlastPhoneHomePolicy::kUsageReportEnv);
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), true);

    // BLAST_USAGE_REPORT (FALSE)
    policy.CheckBlastUsageConfigurations();
    policy.UpdatePhoneHomeStatus();
    env.Unset(CBlastPhoneHomePolicy::kBlastUsageReportEnv);
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), false);

    policy.CheckBlastUsageConfigurations();
    policy.UpdatePhoneHomeStatus();

    CMetaRegistry::SEntry sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    CRef<IRWRegistry> registry = sentry.registry;
    registry->SetValue(CBlastPhoneHomePolicy::kBlastUsageReportRegistry ,
                       CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam,
                       true, IRegistry::fOverride | IRegistry::fPersistent);
    policy.CheckBlastUsageConfigurations();
    policy.UpdatePhoneHomeStatus();
    registry->Unset(CBlastPhoneHomePolicy::kBlastUsageReportRegistry,
                    CBlastPhoneHomePolicy::kBlastUsageReportRegistryParam);
    BOOST_REQUIRE_EQUAL(policy.IsEnabled(), true);

}

BOOST_FIXTURE_TEST_CASE(Check_usage_status_report, CBlastPhoneHomeTestFixture)
{
    const string enable = "TRUE";
    const string disable = "FALSE";

    CMetaRegistry::SEntry sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    CRef<IRWRegistry> registry = sentry.registry;
    registry->SetValue(CBlastPhoneHomePolicy::kNCBIUsageReportRegistry ,
                       CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam,
                       true, IRegistry::fOverride | IRegistry::fPersistent);

    CBlastPhoneHomePolicy phone_home;
    phone_home.Restore();
    string status_report = phone_home.PhoneHomeStatusReport();
    registry->Unset(CBlastPhoneHomePolicy::kNCBIUsageReportRegistry ,
                    CBlastPhoneHomePolicy::kNCBIUsageReportRegistryParam);
    BOOST_REQUIRE_NE(status_report.find(CBlastPhoneHomePolicy::kNCBIUsageReportRegistry), string::npos);

    CNcbiEnvironment env;
    env.Set(CBlastPhoneHomePolicy::kBlastUsageReportEnv, enable);
    status_report = phone_home.PhoneHomeStatusReport();
    env.Unset(CBlastPhoneHomePolicy::kBlastUsageReportEnv);
    BOOST_REQUIRE_NE(status_report.find(CBlastPhoneHomePolicy::kBlastUsageReportEnv), string::npos);

}
BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
