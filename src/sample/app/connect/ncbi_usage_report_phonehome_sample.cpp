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
 *   Sample CNcbiApplication-based console application implementing
 *   the Phone Home Policy, using CUsageReport as a telemetry tool.
 *
 * Phone Home policy:
 *   https://confluence.ncbi.nlm.nih.gov/pages/viewpage.action?pageId=184682676
 *
 * Because this policy can be implemented in many different ways, please note:
 *
 * 1) If your application has an installer, many aspects related to
 *    the Phone Home Policy shown in this sample can be moved into the installer.
 *
 * 2) On a first run this sample asks user if he want to opt-in for telemetry and
 *    saves his choice. User will be asked again for each new version of 
 *    the application (if he is opted-out only).
 * 
 * 3) This sample uses a single global CUsageReport-based reporter via a macro.
 *    For different CUsageReport API usage scenarios, please see
 *    ncbi_usage_report_api_sample.cpp.
 *
 * 4) Used CUsageReport API follow the rules of the Console Do Not Track standard:
 *    https://consoledonottrack.com/
 *    If DO_NOT_TRACK environment variable is set to any value other than
 *    0, FALSE, NO, or OFF (case-insensitive), it disables telemetry even if
 *    user opted in.
 *
 * 5) This sample allow to send some information, like host and OS name,
 *    that may be considered as a sensitive data. This can be changed/disabled,
 *    see CUsageReportAPI::SetDefaultParameters() call below.
 * 
 * 6) This sample is very simplified, and just show how the Phone Home Policy
 *    can be implemented, not always in optimal way for a real usage.
 *    Also, it have no error handling, for example all_TROUBLE macro usages
 *    should be replaced with a real error processing.
 *
 * This example is fully working, but it does not report anything. 
 * We set a dummy reporting URL for sample purposes, so all telemetry reports
 * will actually fail. See Init() method below.
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


// App arguments names
const char* kArg_Telemetry = "telemetry";

// Print current telemetry status
#define PRINT_TELEMETRY_STATUS cout << "Telemetry is " << (IsEnabled() ? "ON" : "OFF") << endl << endl;


/// Sample implementations for the IPhoneHomePolicy.
///
/// It is up to you how to implement each IPhoneHomePolicy method, as well as
/// where and how to store the telemetry state.
///
/// The current sample implementation uses a separate configuration file,
/// "<appname>-telemetry.ini" to store the global telemetry state and
/// the current application version. The first launch is detected by checking
/// for the presence of this .ini file and comparing the current and saved
/// application versions.
/// 
class CSamplePhoneHomePolicy : public IPhoneHomePolicy
{
public:
    // Configuration file section/parameters names
    const char* kConf_Section   = "PHONE_HOME_POLICY";
    const char* kConf_Key_Telemetry = "Telemetry";
    const char* kConf_Key_Version   = "Version";

    // Redefined IPhoneHomePolicy methods

    void Apply(CNcbiApplicationAPI* app);
    void Save();
    void Restore();
    void Print();
    void Init();
    void Finish();

public:
    CNcbiApplicationAPI* m_App;  ///< Pointer to parent app
    string m_ConfFileName;       ///< Name of the configuration file to store telemetry status
    string m_Version;            ///< Application version saved in the configuration file
    bool   m_IsReporterEnabled;  ///< status of the CUsageReport API -- set by Init()
};


void CSamplePhoneHomePolicy::Apply(CNcbiApplicationAPI* app)
{
    _ASSERT(app);
    m_App = app;

    // Define name of the configuration file to store telemetry status.
    // It will be "<appname>-telemetry.ini".
    {{
        string appname = app->GetArguments().GetProgramName(eIgnoreLinks);
#if defined(NCBI_OS_MSWIN)
        NStr::ReplaceInPlace(appname, ".exe", "");
#endif
        m_ConfFileName = appname + "-telemetry.ini";
    }}

    // Explicitly disable telemetry. It can be enabled below
    // based on a command line arguments, saved telemetry status or user input.
    SetEnabled(false);

    // Check command-line arguments, related to our Phone Home Policy sample.
    {{
        const CArgs& args = app->GetArgs();

        // Check telemetry command-line argument.
        if (args[kArg_Telemetry]) {
            // Save telemetry status
            SetEnabled(args[kArg_Telemetry].AsBoolean());
            Save();
        }
        else {
            // Telemetry argument not specified, try to restore previously saved status
            Restore();
        }
        if (!IsEnabled()  &&  (m_Version == app->GetVersion().Print())) {
            // Telemetry is disabled for a current app version, cancel policy initialization
            PRINT_TELEMETRY_STATUS;
            return;
        }
    }}

    // CUsageReport is disabled by default, and should be enabled explicitly
    {{
        // This call to Init() doesn't do any reporting, it initializes API and saves initialization status only
        Init();
        if (!m_IsReporterEnabled) {
            // CUsageReport is disabled / some wrong with an initialization.
            // Telemetry is disabled.
            SetEnabled(false);
            PRINT_TELEMETRY_STATUS;
            return;
        }
    }}
    // Check first app launch or version update, ask user to opt-in.
    {{
        // We detect first launch as:
        // - configuration file doesn't exists -- m_Version is empty, see Restore()
        // - saved version of the application doesn't match current app version -- app were updated

        if (m_Version != app->GetVersion().Print()) {
            if (!IsEnabled()) {
                // Print privacy information
                Print();
                // Read user response
                int ch = getchar();
                bool optin = (ch > 0  &&  ((char)ch == 'y' || (char)ch == 'Y'));
                if (optin) {
                    cout << "Thanks for participation!" << endl;
                }
                cout << endl;
                SetEnabled(optin);
            }
            // Save new configuration file and/or update version
            Save();
        }
    }}

    // Disable CUsageReportAPI if user opted-out of telemetry
    if (!IsEnabled()  &&  m_IsReporterEnabled) {
        CUsageReportAPI::SetEnabled(false);
        m_IsReporterEnabled = false;
    }

    // Print current telemetry status
    PRINT_TELEMETRY_STATUS;
};


void CSamplePhoneHomePolicy::Print()
{
    // Printed mesage is just for a sample purposes, please provide your own.
    cout << R"(
Help us improve the Application:

If you choose to opt in, the application will automatically send non-sensitive
usage information (such as feature usage and performance data) to NCBI. This helps
us understand how the application is used and improve its quality and reliability.

We would like to collect certain data/metrics:

    - Application name;
    - Application version;
    - Host name (computer name) where it is executed;
    - Host operating system name, such as LINUX, FREEBSD, MSWIN;
    - Neasured usage metrics: 
        - jsevent=eventname 
        - key=value

No personal or sensitive information will be collected.

You can change your preference at any time in the settings, running the application
once with on/off for a telemetry command-line argument:

    -telemetry on
    -telemetry off

NLM/NCBI privacy policy:
https://www.nlm.nih.gov/web_policies.html

Would you like to participate (Y/N)?
)"s << endl;
};


void CSamplePhoneHomePolicy::Save()
{
    // All _TROUBLE macro usages should be replaced with a real error processing

    // App version will be saved with the telemetry status
    m_Version = m_App->GetVersion().Print();

    CNcbiRegistry reg;
    try {
        // Read configuration file first
        CNcbiIfstream is(m_ConfFileName);
        reg.Read(is);
        is.close();

        // Set new values
        if (!reg.Set(kConf_Section, kConf_Key_Telemetry, IsEnabled() ? "on" : "off", CNcbiRegistry::fPersistent)) {
            _TROUBLE;
        };
        if (!reg.Set(kConf_Section, kConf_Key_Version, m_Version, CNcbiRegistry::fPersistent)) {
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
    // if configuration file doesn't exists -- user will be prompted anyway,
    // because saved version is empty and doesn't match current app version.

    CNcbiIfstream is(m_ConfFileName);
    try {
        CNcbiRegistry reg;
        reg.Read(is);
        SetEnabled(reg.GetBool(kConf_Section, kConf_Key_Telemetry, false /*disabled*/));
        m_Version = reg.GetString(kConf_Section, kConf_Key_Version, string());
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
    CUsageReportAPI::SetDefaultParameters(
        CUsageReportAPI::fAppName |
        CUsageReportAPI::fAppVersion |
        CUsageReportAPI::fHost |
        CUsageReportAPI::fOS);

    // CUsageReport is disabled by default, and should be enabled explicitly
    CUsageReportAPI::SetEnabled();

    // Last call to CUsageReportAPI doesn't guaranty that it really enables reporting.
    // For example, DO_NOT_TRACK environment variable can override this, so we need 
    // to check CUsageReportAPI status back.

    m_IsReporterEnabled = CUsageReportAPI::IsEnabled();
};


void CSamplePhoneHomePolicy::Finish()
{
    // If you changed enable/disable status for CUsageReportAPI during the application
    // execution, it is better to call Wait/Finish to be sure that everything has really
    // done. Otherwise, like in our case, we can skip reporting API deinitialization if 
    // the telemetry is disabled.

    if (m_IsReporterEnabled) {
        // Wait until all requests are reported and gracefully terminate CUsageReport API.
        NCBI_REPORT_USAGE_WAIT;
        //NCBI_REPORT_USAGE_WAIT_ALWAYS;
        //NCBI_REPORT_USAGE_WAIT_IF_SUCCESS;
        //NCBI_REPORT_USAGE_WAIT_TIMEOUT(CTimeout(sec,msec));
        NCBI_REPORT_USAGE_FINISH;
    }
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
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
        "Sample implementation for Phone Home policy using CUsageReport");

    arg_desc->AddOptionalKey (kArg_Telemetry, "status",
        "Permanently enable/disable telemetry (keep status for later app usages)",
        CArgDescriptions::eBoolean);

    SetupArgDescriptions(arg_desc.release());

    // Set phone home policy
    SetPhoneHomePolicy(new CSamplePhoneHomePolicy(), eTakeOwnership);
}

int CUsageReportPhoneHomeSampleApp::Run()
{
    // Or, you can use local policy directly in the Run(), instead of Init(),
    // that makes calls to check policy status even shorter:
    // 
    //     CSamplePhoneHomePolicy telemetry;
    //     SetPhoneHomePolicy(&telemetry);
    //     if (telemetry.IsEnabled()) {...}

    // ... application code ...

    if (GetPhoneHomePolicy() && GetPhoneHomePolicy()->IsEnabled()) {
        ERR_POST(Info << "Reporting some data (1)");
        NCBI_REPORT_USAGE("eventname", .Add("key", "value1"));
    }

    // Or, with CUsageReport, it can be done using:
    if (CUsageReportAPI::IsEnabled()) {
        ERR_POST(Info << "Reporting some data (2)");
        NCBI_REPORT_USAGE("eventname", .Add("key", "value2"));
    }

    // Or, just use CUsageReport API directly. It is disabled by default
    // end should be enabled first to be able to report anything. You should
    // not enable it explicitly, only the Phone Home Policy should be used
    // to manage "enabled" state (see CSamplePhoneHomePolicy above).
    NCBI_REPORT_USAGE("eventname", .Add("key", "value3"));

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
