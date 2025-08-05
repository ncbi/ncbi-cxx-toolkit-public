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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko
 *
 * File Description:
 *   validator
 *
 */

#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/objistr.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Objects includes
#include <objects/taxon3/taxon3.hpp>
#include <objtools/validator/valid_cmdargs.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <objtools/edit/remote_updater.hpp>
#include "app_config.hpp"
#include "thread_state.hpp"
#include "message_handler.hpp"
#include <util/message_queue.hpp>
#include <future>

#include "thread_pool.hpp"

#if __has_include(<multicall/multicall.hpp>)
    #include <multicall/multicall.hpp>
#else
    #define asnvalidate_app_main main
#endif

using namespace ncbi;
USING_SCOPE(objects);
USING_SCOPE(validator);
USING_SCOPE(edit);

class CAsnvalApp : public CNcbiApplication
{

public:
    CAsnvalApp();
    ~CAsnvalApp();

    void Init() override;
    int Run() override;

private:
    void Setup(const CArgs& args);
    void x_AliasLogFile();

    CThreadExitData xValidate(const string& filename, CNcbiOstream& ostr);
    CThreadExitData xValidateSeparateOutputs(const string& filename);

    CThreadExitData xValidateThreadSeparateOutputs(const string& filename);
    CThreadExitData xValidateThreadSingleOutput(const string& filename, CAsyncMessageHandler& msgHandler);
    CThreadExitData xCombinedStatsTask();

    using TMainPoolEx = TThreadPoolLimitedEx<CThreadExitData>;

    size_t ValidateOneDirectory(string dir_name,
                                bool recurse,
                                CAsyncMessageHandler* pMessageHandler=nullptr); // returns the number of processed files

    TMainPoolEx m_queue{8};
    CValidatorThreadPool m_thread_pool1{4};
    CValidatorThreadPool m_thread_pool2{8};

    unique_ptr<CAppConfig> mAppConfig;
    unique_ptr<edit::CTaxonomyUpdater> mTaxUpdater;
    atomic<size_t> m_NumFiles{0};
    string m_InputDir;
    string m_OutputDir;
};


static unique_ptr<CNcbiOstream> s_MakeOstream(const string& in_filename,
        const string& inputDir=kEmptyStr,
        const string& outputDir=kEmptyStr)
{
    string path;
    if (in_filename.empty()) {
        path = "stdin.val";
    } else {
        size_t pos = NStr::Find(in_filename, ".", NStr::eNocase, NStr::eReverseSearch);
        if (pos != NPOS)
            path = in_filename.substr(0, pos);
        else
            path = in_filename;

        path.append(".val");

        if (! outputDir.empty()) {
            _ASSERT(!inputDir.empty());
            NStr::ReplaceInPlace(path, inputDir, outputDir, 0, 1);
        }
    }

    return unique_ptr<CNcbiOstream>(new ofstream(path));
}

CThreadExitData CAsnvalApp::xValidate(const string& filename, CNcbiOstream& ostr)
{
    CAsnvalThreadState mContext(*mAppConfig, mTaxUpdater->GetUpdateFunc());
    return  mContext.ValidateOneFile(filename, ostr);
}

CThreadExitData CAsnvalApp::xValidateSeparateOutputs(const string& filename)
{
    auto pOstr = s_MakeOstream(filename, m_InputDir, m_OutputDir);
    return xValidate(filename, *pOstr);
}

CThreadExitData CAsnvalApp::xValidateThreadSeparateOutputs(const string& filename)
{
    return xValidateSeparateOutputs(filename);
}


CThreadExitData CAsnvalApp::xValidateThreadSingleOutput(const string& filename, CAsyncMessageHandler& msgHandler)
{
    CAsnvalThreadState mContext(*mAppConfig, mTaxUpdater->GetUpdateFunc());
    return mContext.ValidateOneFile(filename, msgHandler);
}


string s_GetSeverityLabel(EDiagSev sev, bool is_xml)
{
    static const string str_sev[] = {
        "NOTE", "WARNING", "ERROR", "REJECT", "FATAL", "MAX"
    };
    if (sev < 0 || sev > eDiagSevMax) {
        return "NONE";
    }
    if (sev == 0 && is_xml) {
        return "INFO";
    }

    return str_sev[sev];
}


CAsnvalApp::CAsnvalApp()
{
    const CVersionInfo vers(3, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY);
    SetVersion(vers);
}


CAsnvalApp::~CAsnvalApp()
{
}


void CAsnvalApp::x_AliasLogFile()
{
    const CArgs& args = GetArgs();

    if (args["L"]) {
        if (args["logfile"]) {
            if (NStr::Equal(args["L"].AsString(), args["logfile"].AsString())) {
                // no-op
            } else {
                NCBI_THROW(CException, eUnknown, "Cannot specify both -L and -logfile");
            }
        } else {
            SetLogFile(args["L"].AsString());
        }
    }
}

void CAsnvalApp::Init()
{
    // Prepare command line descriptions

    // Create
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddOptionalKey
        ("indir", "Directory", "Path to ASN.1 Files. '-x' specifies the input-file suffix",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey
        ("i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);

    arg_desc->SetDependency("indir", CArgDescriptions::eExcludes, "i");

    arg_desc->AddOptionalKey(
            "outdir", "Directory", "Output directory",
            CArgDescriptions::eOutputFile);

    arg_desc->SetDependency("outdir", CArgDescriptions::eRequires, "indir");

    arg_desc->AddOptionalKey(
        "o", "OutFile", "Single Output File",
        CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey(
        "f", "Filter", "Substring Filter",
        CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey
        ("x", "String", "File Selection Substring", CArgDescriptions::eString, ".ent");
    arg_desc->AddFlag("u", "Recurse");
    arg_desc->AddDefaultKey(
        "R", "SevCount", "Severity for Error in Return Code\n\
\tinfo(1)\n\
\twarning(2)\n\
\terror(3)\n\
\tcritical(4)\n\
\tfatal(5)\n\
\ttrace(6)",
        CArgDescriptions::eInteger, "4");
    arg_desc->AddDefaultKey(
        "Q", "SevLevel", "Lowest Severity for Error to Show\n\
\tinfo(1)\n\
\twarning(2)\n\
\terror(3)\n\
\tcritical(4)\n\
\tfatal(5)\n\
\ttrace(6)",
        CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey(
        "P", "SevLevel", "Highest Severity for Error to Show\n\
\tinfo(1)\n\
\twarning(2)\n\
\terror(3)\n\
\tcritical(4)\n\
\tfatal(5)\n\
\ttrace(6)",
        CArgDescriptions::eInteger, "5");
    CArgAllow* constraint = new CArgAllow_Integers(eDiagSevMin, eDiagSevMax);
    arg_desc->SetConstraint("Q", constraint);
    arg_desc->SetConstraint("P", constraint);
    arg_desc->SetConstraint("R", constraint);
    arg_desc->AddOptionalKey(
        "E", "String", "Only Error Code to Show",
        CArgDescriptions::eString);

    arg_desc->AddDefaultKey("a", "a",
                            "ASN.1 Type\n\
\ta Automatic\n\
\tc Catenated\n\
\te Seq-entry\n\
\tb Bioseq\n\
\ts Bioseq-set\n\
\tm Seq-submit\n\
\td Seq-desc",
                            CArgDescriptions::eString,
                            "",
                            CArgDescriptions::fHidden);

    arg_desc->AddFlag("b", "Input is in binary format; obsolete",
        CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
    arg_desc->AddFlag("c", "Batch File is Compressed; obsolete",
        CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);

    arg_desc->AddFlag("quiet", "Do not log progress");

    CValidatorArgUtil::SetupArgDescriptions(arg_desc.get());
    arg_desc->AddFlag("annot", "Verify Seq-annots only");

    arg_desc->AddOptionalKey(
        "L", "OutFile", "Log File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("v", "Verbosity",
                            "Verbosity\n"
                            "\t1 Standard Report\n"
                            "\t2 Accession / Severity / Code(space delimited)\n"
                            "\t3 Accession / Severity / Code(tab delimited)\n"
                            "\t4 XML Report",
                            CArgDescriptions::eInteger, "1");
    CArgAllow* v_constraint = new CArgAllow_Integers(CAppConfig::eVerbosity_min, CAppConfig::eVerbosity_max);
    arg_desc->SetConstraint("v", v_constraint);

    arg_desc->AddFlag("cleanup", "Perform BasicCleanup before validating (to match C Toolkit); obsolete",
        CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
    arg_desc->AddFlag("batch", "Process NCBI release file (Seq-submit or Bioseq-set only)");
    arg_desc->AddFlag("huge", "Execute in huge-file mode");
    arg_desc->AddFlag("disable-huge", "Explicitly disable huge-files mode");
    arg_desc->SetDependency("disable-huge",
                            CArgDescriptions::eExcludes,
                            "huge");

    arg_desc->AddOptionalKey(
        "D", "String", "Path to lat_lon country data files",
        CArgDescriptions::eString);


    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc,
                                              CDataLoadersUtil::fDefault |
                                              CDataLoadersUtil::fGenbankOffByDefault);

    // Program description
    arg_desc->SetUsageContext("", "ASN Validator");

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

    x_AliasLogFile();
}


size_t CAsnvalApp::ValidateOneDirectory(string dir_name, bool recurse, CAsyncMessageHandler* pMsgHandler)
{
    size_t num_to_process = 0;

    const CArgs& args = GetArgs();

    CDir dir(dir_name);

    string suffix = ".ent";
    if (args["x"]) {
        suffix = args["x"].AsString();
    }
    string mask = "*" + suffix;

    CDir::TEntries files(dir.GetEntries(mask, CDir::eFile));

    bool separate_outputs = !args["o"];

    // Create output directory if it doesn't already exist
    if (!m_OutputDir.empty()) {
        string outputDirName = NStr::Replace(dir_name, m_InputDir, m_OutputDir, 0, 1);
        CDir outputDir(outputDirName);
        if (! outputDir.Exists()) {
            outputDir.Create();
        }
    }

    if (separate_outputs) {
        for (CDir::TEntry ii : files) {
            string fname = ii->GetName();
            if (ii->IsFile() &&
                (!args["f"] || NStr::Find(fname, args["f"].AsString()) != NPOS)) {
                string fpath = CDirEntry::MakePath(dir_name, fname);

                // start thread, it will post results to the queue itself
                auto f = [this, fpath]() -> CThreadExitData
                    { return xValidateThreadSeparateOutputs(fpath); };
                m_queue.run(f);

                ++num_to_process;
            }
        }
    } else {
        for (CDir::TEntry ii : files) {
            string fname = ii->GetName();
            if (ii->IsFile() &&
                (!args["f"] || NStr::Find(fname, args["f"].AsString()) != NPOS)) {
                string fpath = CDirEntry::MakePath(dir_name, fname);

                // start thread, it will post results to the queue itself
                auto f = [this, fpath, pMsgHandler]() -> CThreadExitData
                    { return xValidateThreadSingleOutput(fpath, *pMsgHandler); };
                m_queue.run(f);

                ++num_to_process;
            }
        }
    }

    if (recurse) {
        CDir::TEntries subdirs(dir.GetEntries("", CDir::eDir));
        for (CDir::TEntry ii : subdirs) {
            string subdir = ii->GetName();
            if (ii->IsDir() && !NStr::Equal(subdir, ".") && !NStr::Equal(subdir, "..")) {
                string subname = CDirEntry::MakePath(dir_name, subdir);
                num_to_process += ValidateOneDirectory(subname, recurse, pMsgHandler);
            }
        }
    }

    return num_to_process;
}

CThreadExitData CAsnvalApp::xCombinedStatsTask()
{
    CThreadExitData combined_exit_data;

    while(true)
    {
        auto fut = m_queue.GetNext();
        if (!fut.has_value())
            break;

        auto exit_data = *fut;

        m_NumFiles++;

        combined_exit_data.mReported += exit_data.mReported;

        combined_exit_data.mNumRecords += exit_data.mNumRecords;
        if (exit_data.mLongest > combined_exit_data.mLongest) {
            combined_exit_data.mLongest = exit_data.mLongest;
            combined_exit_data.mLongestId = exit_data.mLongestId;
        }
    }
    return combined_exit_data;
}


int CAsnvalApp::Run()
{
    const CArgs& args = GetArgs();
    Setup(args);
    if (args["indir"]) {
        m_InputDir = args["indir"].AsString();
    }

    if (args["outdir"]) {
        m_OutputDir = args["outdir"].AsString();
    }

    mTaxUpdater.reset(new edit::CTaxonomyUpdater(nullptr)); //m_logger));

    CTime expires = GetFullVersion().GetBuildInfo().GetBuildTime();
    if (!expires.IsEmpty())
    {
        expires.AddYear();
        if (CTime(CTime::eCurrent) > expires)
        {
            NcbiCerr << "This copy of " << GetProgramDisplayName()
                     << " is more than 1 year old. Please download the current version if it is newer." << endl;
        }
    }

    time_t start_time = time(NULL);

    std::ostream* ValidErrorStream = nullptr;
    if (args["o"]) { // combined output
        ValidErrorStream = &args["o"].AsOutputFile();
    }

    if (args["D"]) {
        string lat_lon_path = args["D"].AsString();
        if (! lat_lon_path.empty()) {
            SetEnvironment( "NCBI_LAT_LON_DATA_PATH", lat_lon_path );
        }
    }

    if (args["b"]) {
        cerr << "Warning: -b is deprecated; do not use" << endl;
    }

    if (args["cleanup"]) {
        cerr << "Warning: -cleanup is deprecated; do not use" << endl;
    }

    CThreadExitData exit_data;
    bool exception_caught = false;
    try {
        if (!m_InputDir.empty()) {

            if (ValidErrorStream) { // '-o' specified
                CAsyncMessageHandler msgHandler(*mAppConfig, *ValidErrorStream);
                msgHandler.SetInvokeWrite(false); // don't invoke write inside ValidateOneDirectory()

                auto writer_task = std::async([this, &msgHandler] { msgHandler.Write(); });

                auto combination_task = std::async([this, &exit_data]()
                {
                    while(true) {
                        auto fut = m_queue.GetNext();
                        if (fut.has_value()) {
                            auto exitData = *fut;
                            exit_data.mReported += exitData.mReported;
                            exit_data.mNumRecords += exitData.mNumRecords;
                            if (exitData.mLongest > exit_data.mLongest) {
                                exit_data.mLongest = exitData.mLongest;
                                exit_data.mLongestId = exitData.mLongestId;
                            }
                            ++m_NumFiles;
                        } else {
                            break;
                        }
                    }
                });

                ValidateOneDirectory(m_InputDir, args["u"], &msgHandler);
                m_queue.request_stop();
                m_queue.wait();
                combination_task.wait();
                msgHandler.RequestStop();
                writer_task.wait();
                exit_data.mReported += msgHandler.GetNumReported();
            }
            else { // write to separate files
                auto writer_task = std::async([this, ValidErrorStream] { return xCombinedStatsTask(); });

                ValidateOneDirectory(m_InputDir, args["u"]);
                m_queue.request_stop();
                m_queue.wait();
                exit_data = writer_task.get(); // this will wait writer task completion
            }

        } else {
            string in_filename = (args["i"]) ? args["i"].AsString() : "";

            if (ValidErrorStream) {
                exit_data = xValidate(in_filename, *ValidErrorStream);
            } else {
                auto pOstr = s_MakeOstream(in_filename);
                exit_data = xValidate(in_filename, *pOstr);
            }
            m_NumFiles++;
        }
    } catch (CException& e) {
        ERR_POST(Error << e);
        exception_caught = true;
    }
    if (m_NumFiles == 0) {
        ERR_POST("No matching files found");
    }

    time_t stop_time = time(NULL);
    if (! mAppConfig->mQuiet ) {
        LOG_POST_XX(Corelib_App, 1, "Finished in " << stop_time - start_time << " seconds");
        LOG_POST_XX(Corelib_App, 1, "Longest processing time " << exit_data.mLongest << " seconds on " << exit_data.mLongestId);
        LOG_POST_XX(Corelib_App, 1, "Total number of records " << exit_data.mNumRecords);
    }

    return (exit_data.mReported > 0 || exception_caught);
}


void CAsnvalApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    // CORE_SetLOG(LOG_cxx2c());
    // CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    mAppConfig.reset(new CAppConfig(args, GetConfig()));
    mAppConfig->m_thread_pool1 = &m_thread_pool1;
    mAppConfig->m_thread_pool2 = &m_thread_pool2;

    // Create object manager
    CDataLoadersUtil::SetupObjectManager(args, *CObjectManager::GetInstance(),
                                         CDataLoadersUtil::fDefault |
                                         CDataLoadersUtil::fGenbankOffByDefault);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int asnvalidate_app_main(int argc, const char* argv[])
{
    // this code converts single argument into multiple, just to simplify testing
    list<string>        split_args;
    vector<const char*> new_argv;

    if (argc==2 && argv && argv[1] && strchr(argv[1], ' '))
    {
        NStr::Split(argv[1], " ", split_args, NStr::fSplit_CanEscape | NStr::fSplit_CanQuote | NStr::fSplit_Truncate | NStr::fSplit_MergeDelimiters);

        argc = 1 + split_args.size();
        new_argv.reserve(argc);
        new_argv.push_back(argv[0]);
        for (auto& s : split_args) {
            new_argv.push_back(s.c_str());
            #ifdef _DEBUG
            std::cerr << s.c_str() << " ";
            #endif
        }
        std::cerr << "\n";

        argv = new_argv.data();
    }

    return CAsnvalApp().AppMain(argc, argv);
}
