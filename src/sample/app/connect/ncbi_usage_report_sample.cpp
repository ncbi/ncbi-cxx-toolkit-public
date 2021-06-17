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
 *   Sample for logging usage information via CUsageReport.
 *
 * This example is working, but not log anything to the Applog,
 * we set wrong dummy reporting URL, redefining real one for the sample purposes.
 * So, all reports here will fail actually.
 *
 * Also, see ncbi_usage_report_sample.ini in the same directory for allowed
 * configuration options, or check CUsageReportAPI class, that have 
 * corresponding options.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_usage_report.hpp>

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  Sample NCBI application

class CUsageReportSampleApp : public CNcbiApplication
{
public:
    CUsageReportSampleApp();
    virtual void Init();
    virtual int Run();

    /// Global API configuration example
    void ConfigureAPI();

    // Below are examples of 3 different usage patterns,
    // all can be used separately or in conjunction with each other.

    /// Simplest way to report -- use provided macro
    void Pattern_1_SimpleMacro();
    /// Custom reporters
    void Pattern_2_MultipleReporters();
    /// Advanced reporting method with custom jobs and reporting status control
    void Pattern_3_StatusControl();
};


CUsageReportSampleApp::CUsageReportSampleApp()
{
    // If application have version, it will be automatically reported
    // alongside with host, OS name and application name.
    //
    SetVersion(CVersionInfo(1,2,3));
}


void CUsageReportSampleApp::Init()
{
    // Some application initialization ("Warning" for test purposes only)
    SetDiagPostLevel(eDiag_Warning);
}


int CUsageReportSampleApp::Run()
{
    // API configuration example
    ConfigureAPI();

    // Usage pattern
    Pattern_1_SimpleMacro();
    Pattern_2_MultipleReporters();
    Pattern_3_StatusControl();

    // Global reporter: 
    // Wait until all requests are reported. Optional.
    NCBI_REPORT_USAGE_WAIT;
    // Gracefully terminate reporting API. Optional.
    NCBI_REPORT_USAGE_FINISH;

    return 0;
}


void CUsageReportSampleApp::ConfigureAPI()
{
    // Global API configuration.
    //
    // Each call here is optional. 
    // Any parameter can be set via configuration file or environment variable as well.

    // Set application name and version.
    // New values will NOT redefine CUsageReportSampleApp name and version
    // if specified (as we did, see constructor and Init()). But can be used
    // for any non-CNcbiApplication-based applications.
    //
    CUsageReportAPI::SetAppName("usage_report_sample");
    CUsageReportAPI::SetAppVersion(CVersionInfo(4, 5, 6));
    // or set version as string
    CUsageReportAPI::SetAppVersion("4.5.6");
    // Note again, all 3 calls above doesn't have any effect in our case.

    // Set what should be reported by default with any report
    // (if not changed OS name will be reported by default also)
    CUsageReportAPI::SetDefaultParameters(CUsageReportAPI::fAppName |
                                          CUsageReportAPI::fAppVersion |
                                          CUsageReportAPI::fHost);

    // Set non-standard global URL for reporting
    // Think twice why you need to change default URL.
    CUsageReportAPI::SetURL("http://dummy/cgi");

    // Change queue size for storing jobs reported in background.
    CUsageReportAPI::SetMaxQueueSize(20);

    // And finally enable reporting API (global flag).
    CUsageReportAPI::SetEnabled();
}


void CUsageReportSampleApp::Pattern_1_SimpleMacro()
{
    // Simplest way to report -- use provided macro.
    // All macro use global API reporter and can be used from any thread, MT safe.
    // No any control over reporting or checking status.

    // Enable reporting.
    // Can be enabled/disabled via configuration file or environment variable.
    // Same as CUsageReportAPI::Enable() above -- we already enabled API there.
    NCBI_REPORT_USAGE_START;

    // First parameter automatically sets to "jsevent=eventname".
    // Below are some reports with variable number of parameters.
    // All default parameters like application name, version and etc 
    // will be automatically added during sending data to server.
    //
    NCBI_REPORT_USAGE("eventname");
    NCBI_REPORT_USAGE("eventname", .Add("tool_name", "ABC"));
    NCBI_REPORT_USAGE("eventname", .Add("tool_name", "DEF").Add("tool_version", 1));
    NCBI_REPORT_USAGE("eventname",
                                   .Add("tool_name", "XYZ")
                                   .Add("tool_version", 2)
                                   .Add("one_more_parameter", 123.45));

    // Next 2 call are optional and should be done before 
    // an application exit.

    // Wait until all requests are reported. 
    NCBI_REPORT_USAGE_WAIT;
    // Gracefully terminate global reporting API.
    // Commented because we will use global reporter again below.
    //NCBI_REPORT_USAGE_FINISH;
}


void CUsageReportSampleApp::Pattern_2_MultipleReporters()
{
    // Custom local reporters.
    //
    // We still recommend do use single global reporter, 
    // but if you want to report to different URLs at the same time you may 
    // construct your own reporter. Newly created reporters inherit all 
    // configuration parameters from CUsageReportAPI.

    // First reporter will not use any default reporting parameters
    CUsageReport reporter1(CUsageReport::fNone, "http://url_1");
    // Second will report to another URL and report all defaults specified
    // for CUsageReportAPI at the moment.
    CUsageReport reporter2(CUsageReport::fDefault, "http://url_2");

    CUsageReportParameters params1, params2;

    // Report some parameters to both
    params1.Add("p1", "v1");
    reporter1.Send(params1);
    params2.Add("p2", "v2");
    reporter2.Send(params2);

    // Change parameter and report again to 1st only
    params1.Add("p1", "new value for p1");
    reporter1.Send(params1);

    // ... 

    // Wait and finish. Better to call them once per reporter 
    // before finishing reporting and application exit, or when 
    // you don't need them anymore.

    reporter1.Wait(); reporter1.Finish();
    reporter2.Wait(); reporter2.Finish();
}


void CUsageReportSampleApp::Pattern_3_StatusControl()
{
    // Advanced method with custom jobs and control for reporting process.

    /// Example of a simple custom reporting job
    class CMyJob : public CUsageReportJob
    {
    public:
        /// Our example will have some integer ID to distinguish jobs
        CMyJob(int id = 0) : m_ID(id) {};

        /// Override OnStateChange() if want to control reporting status
        virtual void OnStateChange(EState state) 
        {
            string state_str;
            switch (state) {
            case CUsageReportJob::eQueued:
                // Job has added to queue
                state_str = "Queued";
                break;
            case CUsageReportJob::eRunning:
                // Job extracted from queue and going to be send to server
                state_str = "Running";
                break;
            case CUsageReportJob::eCompleted:
                // No error for reporting this job
                break;
            case CUsageReportJob::eFailed:
                // Reporting failed, you can print error message for example
                ERR_POST(Warning << "Reporting failed for job " << m_ID);
                // Or disable future reporting for current reporter at all:
                //CUsageReportAPI::Disable();
                state_str = "Failed";
                break;
            case CUsageReportJob::eCanceled:
                // Reporting canceled for this job due CUsageReport:: ClearQueue() / Finish() / Disable()
                state_str = "Canceled";
                break;
            case CUsageReportJob::eRejected:
                // Too many requests, queue is full, job cannot be processed
                state_str = "Rejected";
                break;
            default:
                _TROUBLE;
            }
            cout << "callback: job " << m_ID << " -> " << state_str << endl << flush;
        };

        /// Copy constructor.
        /// For trivial classes like this it is not really necessary,
        /// default copy constructor can be used. But if you have pointers
        /// or any other data types that cannot be just copied, you need 
        /// to implement it.
        ///
        CMyJob(const CMyJob& other) : CUsageReportJob(other) { 
            // Copy members:
            m_ID = other.m_ID;
        };

    protected:
        int m_ID;   ///< some job ID for example
    };

    // We will use global reporter here, but this can be applied to any reporter.
    CUsageReport& reporter = CUsageReport::Instance(); 

    // Because we set dummy URL for our reporter, bigger half of jobs will fails,
    // and remaining part rejected, because current queue size is 20 only.
    // The reporter will unable to process all, we add jobs faster.
    // 
    for (int i = 1; i < 40; i++) {
        // Create custom job with ID equal to 'i'
        CMyJob job(i);
        // Add some extra parameters if necessary
        job.Add("p1", i*2); 
        job.Add("p2", "v" + std::to_string(i)); 
        // Send (add to asynchronous reporting queue)
        reporter.Send(job);
    }
    // Waiting and finishing -- not yet, see Run()
    //reporter.Wait();
    //reporter.Finish();
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    // Each application have a name. It can be specified as last parameter
    // in arguments to AppMain, or extracted from the executable file name.
    // So it can be automatically used for reporting.

    return CUsageReportSampleApp().AppMain(argc, argv, 0, eDS_Default, "", "SampleAppName");
}
