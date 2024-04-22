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
#include <util/message_queue.hpp>
#include <future>

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;
USING_SCOPE(objects);
USING_SCOPE(validator);
USING_SCOPE(edit);

template<class _T>
class TThreadPoolLimited
{
public:
    using token_type = _T;
    TThreadPoolLimited(unsigned num_slots, unsigned queue_depth = 0):
        m_tasks_semaphose{num_slots, num_slots},
        m_product_queue{queue_depth == 0 ? num_slots : queue_depth}
    {}

    // acquire new slot to run on
    void acquire() {
        m_running++;
        m_tasks_semaphose.Wait();
    }

    // release a slot and send a product down to the queue
    void release(token_type&& token) {
        m_tasks_semaphose.Post();
        m_product_queue.push_back(std::move(token));

        size_t current = m_running.fetch_sub(1);
        if (current == 1)
        { // the last task completed, singal main thread to finish
            m_product_queue.push_back({});
        }
    }

    auto GetNext() {
        return m_product_queue.pop_front();
    }

private:
    std::atomic<size_t>   m_running = 0;      // number of started threads
    CSemaphore            m_tasks_semaphose;  //
    CMessageQueue<token_type> m_product_queue{8}; // the queue of threads products
    std::future<void>     m_consumer;
};

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

    CThreadExitData xValidate(const string& filename, bool save_output);
    void xValidateThread(const string& filename, bool save_output);
    CThreadExitData xCombinedWriterTask(std::ostream* ofile);

    using TPool = TThreadPoolLimited<std::future<CThreadExitData>>;

    void ValidateOneDirectory(string dir_name, bool recurse);
    TPool m_queue{8};

    unique_ptr<CAppConfig> mAppConfig;
    unique_ptr<edit::CRemoteUpdater> mRemoteUpdater;
    size_t m_NumFiles = 0;
};

CThreadExitData CAsnvalApp::xValidate(const string& filename, bool save_output)
{
    CAsnvalThreadState mContext(*mAppConfig, mRemoteUpdater->GetUpdateFunc());
    auto result = mContext.ValidateOneFile(filename);
    if (save_output) {
        CAsnvalOutput out(*mAppConfig, filename);
        result.mReported += out.Write(result.mEval);
        result.mEval.clear();
    }
    return result;
}

void CAsnvalApp::xValidateThread(const string& filename, bool save_output)
{
    CThreadExitData result = xValidate(filename, save_output);

    std::promise<CThreadExitData> prom;
    prom.set_value(std::move(result));
    auto fut = prom.get_future();

    m_queue.release(std::move(fut));
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
        ("indir", "Directory", "Path to ASN.1 Files",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey
        ("i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);
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
\tinfo(0)\n\
\twarning(1)\n\
\terror(2)\n\
\tcritical(3)\n\
\tfatal(4)\n\
\ttrace(5)",
        CArgDescriptions::eInteger, "4");
    arg_desc->AddDefaultKey(
        "Q", "SevLevel", "Lowest Severity for Error to Show\n\
\tinfo(0)\n\
\twarning(1)\n\
\terror(2)\n\
\tcritical(3)\n\
\tfatal(4)\n\
\ttrace(5)",
        CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey(
        "P", "SevLevel", "Highest Severity for Error to Show\n\
\tinfo(0)\n\
\twarning(1)\n\
\terror(2)\n\
\tcritical(3)\n\
\tfatal(4)\n\
\ttrace(5)",
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

    arg_desc->AddFlag("cleanup", "Perform BasicCleanup before validating (to match C Toolkit)");
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


//LCOV_EXCL_START
//unable to exercise with our test framework
void CAsnvalApp::ValidateOneDirectory(string dir_name, bool recurse)
{
    const CArgs& args = GetArgs();

    CDir dir(dir_name);

    string suffix = ".ent";
    if (args["x"]) {
        suffix = args["x"].AsString();
    }
    string mask = "*" + suffix;

    CDir::TEntries files(dir.GetEntries(mask, CDir::eFile));

    for (CDir::TEntry ii : files) {
        string fname = ii->GetName();
        if (ii->IsFile() &&
            (!args["f"] || NStr::Find(fname, args["f"].AsString()) != NPOS)) {
            string fpath = CDirEntry::MakePath(dir_name, fname);

            bool separate_outputs = !args["o"];

            m_queue.acquire();
            // start thread detached, it will post results to the queue itself
            std::thread([this, fpath, separate_outputs]()
                { xValidateThread(fpath, separate_outputs); })
                .detach();
        }
    }
    if (recurse) {
        CDir::TEntries subdirs(dir.GetEntries("", CDir::eDir));
        for (CDir::TEntry ii : subdirs) {
            string subdir = ii->GetName();
            if (ii->IsDir() && !NStr::Equal(subdir, ".") && !NStr::Equal(subdir, "..")) {
                string subname = CDirEntry::MakePath(dir_name, subdir);
                ValidateOneDirectory(subname, recurse);
            }
        }
    }

}
//LCOV_EXCL_STOP


CThreadExitData CAsnvalApp::xCombinedWriterTask(std::ostream* combined_file)
{
    CThreadExitData combined_exit_data;

    std::list<CConstRef<CValidError>> eval;

    CAsnvalOutput out(*mAppConfig, combined_file);

    while(true)
    {
        auto fut = m_queue.GetNext();
        if (!fut.valid())
            break;

        auto exit_data = fut.get();

        m_NumFiles++;

        combined_exit_data.mReported += exit_data.mReported;

        combined_exit_data.mNumRecords += exit_data.mNumRecords;
        if (exit_data.mLongest > combined_exit_data.mLongest) {
            combined_exit_data.mLongest = exit_data.mLongest;
            combined_exit_data.mLongestId = exit_data.mLongestId;
        }

        if (combined_file && !exit_data.mEval.empty()) {
            combined_exit_data.mReported += out.Write(exit_data.mEval);
        }
    }

    return combined_exit_data;
}

int CAsnvalApp::Run()
{
    const CArgs& args = GetArgs();
    Setup(args);

    mRemoteUpdater.reset(new edit::CRemoteUpdater(nullptr)); //m_logger));

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

    CThreadExitData exit_data;
    bool exception_caught = false;
    try {
        if (args["indir"]) {

            auto writer_task = std::async([this, ValidErrorStream] { return xCombinedWriterTask(ValidErrorStream); });

            ValidateOneDirectory(args["indir"].AsString(), args["u"]);

            exit_data = writer_task.get(); // this will wait writer task completion

        } else {
            string in_filename = (args["i"]) ? args["i"].AsString() : "";
            exit_data = xValidate(in_filename, ValidErrorStream==nullptr);
            m_NumFiles++;
            if (ValidErrorStream) {
                CAsnvalOutput out(*mAppConfig, ValidErrorStream);
                exit_data.mReported += out.Write({exit_data.mEval});
            }
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

    if (exit_data.mReported > 0 || exception_caught) {
        return 1;
    } else {
        return 0;
    }
}


void CAsnvalApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    // CORE_SetLOG(LOG_cxx2c());
    // CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    mAppConfig.reset(new CAppConfig(args, GetConfig()));

    // Create object manager
    CDataLoadersUtil::SetupObjectManager(args, *CObjectManager::GetInstance(),
                                         CDataLoadersUtil::fDefault |
                                         CDataLoadersUtil::fGenbankOffByDefault);
}


void CValXMLStream::Print(const CValidErrItem& item)
{
    m_Output.PutString("  <message severity=\"");
    m_Output.PutString(s_GetSeverityLabel(item.GetSeverity(), true));
    if (item.IsSetAccnver()) {
        m_Output.PutString("\" seq-id=\"");
        WriteString(item.GetAccnver(), eStringTypeVisible);
    }

    if (item.IsSetFeatureId()) {
        m_Output.PutString("\" feat-id=\"");
        WriteString(item.GetFeatureId(), eStringTypeVisible);
    }

    if (item.IsSetLocation()) {
        m_Output.PutString("\" interval=\"");
        string loc = item.GetLocation();
        if (NStr::StartsWith(loc, "(") && NStr::EndsWith(loc, ")")) {
            loc = "[" + loc.substr(1, loc.length() - 2) + "]";
        }
        WriteString(loc, eStringTypeVisible);
    }

    m_Output.PutString("\" code=\"");
    WriteString(item.GetErrGroup(), eStringTypeVisible);
    m_Output.PutString("_");
    WriteString(item.GetErrCode(), eStringTypeVisible);
    m_Output.PutString("\">");

    WriteString(item.GetMsg(), eStringTypeVisible);

    m_Output.PutString("</message>");
    m_Output.PutEol();
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    #ifdef _DEBUG
    // this code converts single argument into multiple, just to simplify testing
    list<string> split_args;
    vector<const char*> new_argv;

    if (argc==2 && argv && argv[1] && strchr(argv[1], ' '))
    {
        NStr::Split(argv[1], " ", split_args);

        auto it = split_args.begin();
        while (it != split_args.end())
        {
            auto next = it; ++next;
            if (next != split_args.end() &&
                ((it->front() == '"' && it->back() != '"') ||
                 (it->front() == '\'' && it->back() != '\'')))
            {
                it->append(" "); it->append(*next);
                next = split_args.erase(next);
            } else it = next;
        }
        for (auto& rec: split_args)
        {
            if (rec.front()=='\'' && rec.back()=='\'')
                rec=rec.substr(1, rec.length()-2);
        }
        argc = 1 + split_args.size();
        new_argv.reserve(argc);
        new_argv.push_back(argv[0]);
        for (const string& s : split_args)
        {
            new_argv.push_back(s.c_str());
            std::cerr << s.c_str() << " ";
        }
        std::cerr << "\n";


        argv = new_argv.data();
    }
    #endif
    return CAsnvalApp().AppMain(argc, argv);
}
