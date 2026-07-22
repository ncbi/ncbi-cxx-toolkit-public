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
 * Author: Amelia Fong
 *
 */

/** @file blast_usage_report_app.cpp
 * Command line tool to config and report blast_usage_report setting.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/api/blast_usage_report.hpp>
#include "../blast/blast_app_util.hpp"
#include <iomanip>


#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

/// Command line argument names
static const string kArgOn("on");
static const string kArgOff("off");
static const string kArgPrivacyMsg("privacy_msg");
static const string kArgStatus("status");

/// The application class
class CBlastUsageReportApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastUsageReportApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    CBlastPhoneHomePolicy m_PhoneHomePolicy;

};

void CBlastUsageReportApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun |fHideLogfile);

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                  "BLAST Usage Report, version " + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("Configuration options");
    arg_desc->AddFlag(kArgOn, "Enable BLAST usage report", true);
    arg_desc->AddFlag(kArgOff, "Disable BLAST usage report", true);
    arg_desc->SetDependency(kArgOn, CArgDescriptions::eExcludes, kArgOff);

    arg_desc->SetCurrentGroup("General options");
    arg_desc->AddFlag(kArgPrivacyMsg, "Print BLAST privacy message", true);
    arg_desc->SetDependency(kArgPrivacyMsg, CArgDescriptions::eExcludes, kArgOn);
    arg_desc->SetDependency(kArgPrivacyMsg, CArgDescriptions::eExcludes, kArgOff);

    arg_desc->AddFlag(kArgStatus, "BLAST usage report status", true);
    arg_desc->SetDependency(kArgStatus, CArgDescriptions::eExcludes, kArgOn);
    arg_desc->SetDependency(kArgStatus, CArgDescriptions::eExcludes, kArgOff);
    arg_desc->SetDependency(kArgStatus, CArgDescriptions::eExcludes, kArgPrivacyMsg);

    arg_desc->AddDefaultKey(kArgOutput, "output_file", "Output file name",
                            CArgDescriptions::eOutputFile, "-");
    arg_desc->SetDependency(kArgOn, CArgDescriptions::eExcludes, kArgOutput);
    arg_desc->SetDependency(kArgOff, CArgDescriptions::eExcludes, kArgOutput);

    SetupArgDescriptions(arg_desc.release());
}

int CBlastUsageReportApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();
    
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostPrefix("blast_usage_report");

    try {

        if (args[kArgOn]){
		    m_PhoneHomePolicy.CheckBlastUsageConfigurations();
		    m_PhoneHomePolicy.EnableOptIn(true);
		    if (m_PhoneHomePolicy.IsEnabled() != true) {
		        ERR_POST(Warning << "Existing usage configuration override opt-in selection.\n"
		                            "Please run status option for more information.");
            }
        }
        else if (args[kArgOff]){
		    m_PhoneHomePolicy.CheckBlastUsageConfigurations();
		    m_PhoneHomePolicy.EnableOptIn(false);
		    if (m_PhoneHomePolicy.IsEnabled() != false) {
		        ERR_POST(Warning << "Existing usage configuration override opt-in selection.\n"
		                            "Please run status option for more information.");
            }
        }
        else if (args[kArgPrivacyMsg]){
            CNcbiOstream& out = args[kArgOutput].AsOutputFile();
            std::streambuf* old = std::cout.rdbuf(out.rdbuf());
            m_PhoneHomePolicy.Print();
            std::cout.rdbuf(old);
        }
        else if (args[kArgStatus]){
            CNcbiOstream& out = args[kArgOutput].AsOutputFile();
            out << m_PhoneHomePolicy.PhoneHomeStatusReport();
        }
        else {
            string options = kArgOn +", " + kArgOff + ", " + kArgPrivacyMsg + " or " + kArgStatus;
            NCBI_THROW(CInputException, eInvalidInput,
                        "Must specify: one of " + options);
        }
        
    } 
    catch (const CException& e) {
    	ERR_POST(Error << e.GetMsg());
        status = 1;
    } catch (...) {
        ERR_POST(Error << "BLAST usage report failure");
        status = 1;
    }	
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastUsageReportApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
