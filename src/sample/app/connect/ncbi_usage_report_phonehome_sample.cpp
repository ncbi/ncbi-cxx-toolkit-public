/*  $Id$
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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *   Sample of CNcbiApplication-based console application implementing
 *   Phone Home Policy, using CUsageReport as a telemetry tool.
 * 
 *   Phone Home policy:
 *   https://confluence.ncbi.nlm.nih.gov/pages/viewpage.action?pageId=184682676
 *
 * If your application have an installer, many aspects related to Phone Home Policy
 * and shown in this sample can be moved into the installer.
 * 
 * This sample use a single global CUsageReport-based reporter via macro. 
 * For different CUsageReport API usage scenarios please see 
 * ncbi_usage_report_api_sample.cpp.
 *
 * This example is working, but it does not report anything, we set a dummy 
 * reporting URL for the sample purposes, so all reports here will fail actually.
 *
  * NOTE: This application is interactive, do not run it in the automated mode.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/phone_home_policy.hpp>
#include <connect/ncbi_usage_report.hpp>


USING_NCBI_SCOPE;

/// Sample implementations for the IPhoneHomePolicy. 
/// It is up to you how to implement each IPhoneHomePolicy method, and where
/// and how to store a tracking state.
/// 
/// The current sample implementation uses separate configuration file "<appname>-phone-home.ini",
/// to store global tracking state and current application version. First launch is detected
/// by checking presence of this ini-file and matching current and saved versions for the app. 
/// So, each new version will Print() privacy information again.
/// 
class CSamplePhoneHomePolicy : public IPhoneHomePolicy
{
public:
    const char* kSection  = "PHONE_HOME_POLICY";
    const char* kTracking = "tracking";
    const char* kVersion  = "version";

    // Redefined IPhoneHomePolicy methods

    void Apply(CNcbiApplicationAPI* app);
    void Save();
    void Restore();
    void Print();
    void Init();
    void Finish();

public:
    string m_ConfFileName;  ///< Name of the configuration file to store tracking status
    string m_Version;       ///< Application version saved in the configuration file
};


void CSamplePhoneHomePolicy::Apply(CNcbiApplicationAPI* app)
{
    _ASSERT(app);

    // Define name of the configuration file to store tracking status.
    // It will be "<appname>-phone-home.ini".
    {{
        string appname = app->GetArguments().GetProgramName(eIgnoreLinks);
        #if defined(NCBI_OS_MSWIN)
            NStr::ReplaceInPlace(appname, ".exe", "");
        #endif
        m_ConfFileName = appname + "-phone-home.ini";
    }}

    // Explicitly set "enabled" state. It can be disabled below,
    // based on command line arguments or saved tracking information.
    SetEnabled();

    // Check command-line arguments, related to our Phone Home Policy sample.
    {{
        const CArgs& args = app->GetArgs();

        // Check "-disable-tracking" to enable/disable tracking.
        // If not specifies, try to restore last saved state.
        if ( args["tracking"] ) {
            SetEnabled( args["tracking"].AsBoolean() );
            m_Version = app->GetVersion().Print();
            Save();
        }
        else {
            Restore();
        }
        if (!IsEnabled()) {
            // tracking is permanently disabled, cancel policy initialization
            return;
        }
        // Check "-do-not-track"
        if (args["do-not-track"]) {
            SetEnabled(false);
            // tracking is temporary disabled, cancel policy initialization
            return;
        }
    }}
    // CUsageReport is disabled by default, and should be enabled explicitly
    {{
        // These call don't do any reporting, it initializes API only and saves initialization status
        Init();
        if (!IsEnabled()) {
            return;
        }
    }}
    // Check first launch and app version
    {{
        // We detect first launch as:
        // - configuration file doesn't exists -- m_Version is empty, see Restore()
        // - saved version of the application doesn't match current app version -- app were updated
        if (m_Version != app->GetVersion().Print()) {
            // Print privacy information
            Print();
            // Save new configuration file
            m_Version = app->GetVersion().Print();
            Save();
        }
    }}
};

void CSamplePhoneHomePolicy::Print()
{
    // Printed mesage is just for a sample purposes, please provide your own.
    cout << R"(
To improve this program and make it better we collect some data/metrics and sent it to NCBI:
    - application name: appname=<appname>;
    - application version: version=<appversion>;
    - host name (computer name) where it executes: host=<hostname>;
    - OS name of the host, like: LINUX, FREEBSD, MSWIN: os=LINUX;
    - Some measured usage metric: jsevent=eventname, key=value.

We do not collect any personal data or personally identifiable information.

To disable all usage reporting please set OS environment variable DO_NOT_TRACK to any value
other than 0, FALSE, NO, or OFF (case-insensitive). Or use command-line flag:

    -do-not-track

this flag works for a current application run only. To permanently disable/reenable tracking,
you can run the application once with:

    -tracking off
    -tracking on

NLM/NCBI privacy policy:
https://www.nlm.nih.gov/web_policies.html

Press ENTER to agree and continue, or Ctrl+C to exit.
)"s << endl;
    getchar();
};

void CSamplePhoneHomePolicy::Save()
{
    // All _TROUBLE macro should be replaced with a real error processing
 
    CNcbiRegistry reg;
    try {
        // Read configuration file first
        CNcbiIfstream is(m_ConfFileName);
        reg.Read(is);
        is.close();

        // Set new values
        if (!reg.Set(kSection, kTracking, IsEnabled() ? "on" : "off", CNcbiRegistry::fPersistent)) {
            _TROUBLE;
        };
        if (!reg.Set(kSection, kVersion, m_Version, CNcbiRegistry::fPersistent)) {
            _TROUBLE;
        };
        // Rewrite configuration file with new values
        CNcbiOfstream os(m_ConfFileName);
        if (!reg.Write(os)) {
            _TROUBLE;
        }
    }
    catch (CRegistryException&) {
        _TROUBLE;
    }
};

void CSamplePhoneHomePolicy::Restore()
{
    // We don't check file existance here (first launch case).
    // if configuration file doesn't exists -- treat tracking state
    // as "enabled". We will Print() privacy notes later anyway,
    // because saved version is empty for this case and do not match
    // current app version.

    CNcbiIfstream is(m_ConfFileName);
    try {
        CNcbiRegistry reg;
        reg.Read(is);
        SetEnabled( reg.GetBool(kSection, kTracking, true /*enabled*/ ));
        m_Version = reg.GetString(kSection, kVersion, string());
    }
    catch (CRegistryException&) {
        _TROUBLE;
    }
};

void CSamplePhoneHomePolicy::Init()
{
    // Set wrong dummy reporting URL, redefining real one for the sample purposes only!
    // So, all reports here will fail to report actually.
    CUsageReportAPI::SetURL("http://dummy/cgi");

    // Set what should be reported by default with any report
    CUsageReportAPI::SetDefaultParameters(CUsageReportAPI::fAppName |
                                          CUsageReportAPI::fAppVersion |
                                          CUsageReportAPI::fHost |
                                          CUsageReportAPI::fOS);

    // CUsageReport is disabled by default, and should be enabled explicitly
    CUsageReportAPI::SetEnabled();

    // Last call to CUsageReportAPI doesn't guaranty that it really enables reporting.
    // For exqmple, DO_NOT_TRACK environment variable can override this, so we need 
    // to check CUsageReportAPI status back and set policy enabled state accordingly.
    SetEnabled( CUsageReportAPI::IsEnabled() );
};

void CSamplePhoneHomePolicy::Finish()
{
    // Wait until all requests are reported and gracefully terminate CUsageReport API.
    NCBI_REPORT_USAGE_WAIT;
    NCBI_REPORT_USAGE_FINISH;
}



/////////////////////////////////////////////////////////////////////////////
//  Sample NCBI application

class CUsageReportPhoneHomeSampleApp : public CNcbiApplication
{
public:
    CUsageReportPhoneHomeSampleApp();
    void Init();
    int  Run();
    void Exit();
};

CUsageReportPhoneHomeSampleApp::CUsageReportPhoneHomeSampleApp()
{
    // If application have version, it will be automatically reported
    // alongside with host, OS name and application name.
    // For this implementation we expect that the application have version.
    //
    NCBI_APP_SET_VERSION(1, 2, 3);  // Extended variant of SetVersion(CVersionInfo(1,2,3));
}

void CUsageReportPhoneHomeSampleApp::Init()
{
    SetDiagPostLevel(eDiag_Info);

    // Arguments
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Sample implementation for Phone Home policy using CUsageReport");

    arg_desc->AddFlag
        ("do-not-track",
         "Disable tracking for a current run of the application");

    arg_desc->AddOptionalKey
        ("tracking", "status",
         "Permanently disable/reenable tracking (keep status for later app usages)",
         CArgDescriptions::eBoolean);

    SetupArgDescriptions(arg_desc.release());

    // Set phone home policy
    SetPhoneHomePolicy(new CSamplePhoneHomePolicy(), eTakeOwnership);
}

int CUsageReportPhoneHomeSampleApp::Run()
{
    // Or, you can use local policy directly in the Run(), that makes calls 
    // to check policy status even shorter:
    // 
    //     CSamplePhoneHomePolicy policy;
    //     SetPhoneHomePolicy(&policy);
    //     if (policy.IsEnabled()) {...}
    
    // ... application code ...

    ERR_POST(Info << "Reporting some data, please wait...");

    if (GetPhoneHomePolicy()  &&  GetPhoneHomePolicy()->IsEnabled()) {
        NCBI_REPORT_USAGE("eventname", .Add("key", "value1"));
    }
    // Or, just use CUsageReport API directly. It is disabled by default
    // end should be enabled first to be able to report anything. You should
    // not enable it explicitly, only the Phone Home Policy should be used to manage 
    // "enabled" state (see CSamplePhoneHomePolicy above).
    NCBI_REPORT_USAGE("eventname", .Add("key", "value2"));

    // ... application code ...

    return 0;
}

void CUsageReportPhoneHomeSampleApp::Exit()
{
    // Deactivates policy. Automatically calls Finish(), that in our sample
    // gracefully waits all reportings to finish and deinitializes reporting API.
    SetPhoneHomePolicy(NULL);
    ERR_POST(Info << "Done.");
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    // Each application have a name. It can be specified as last parameter
    // in arguments to AppMain, or extracted from the executable file name.
    // So it can be automatically used for reporting.

    return CUsageReportPhoneHomeSampleApp().AppMain(argc, argv, 0, eDS_Default, "", "PhoneHomeSampleApp");
}
