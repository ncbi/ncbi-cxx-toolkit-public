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
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/error_codes.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectio.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Objects includes
#include <objects/taxon3/taxon3.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objtools/validator/validator.hpp>
#include <objtools/validator/valid_cmdargs.hpp>
#include <objtools/validator/validator_context.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include <objects/seqset/Bioseq_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <serial/objostrxml.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <util/compress/stream_util.hpp>
#include <objtools/readers/format_guess_ex.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objtools/edit/huge_file_process.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/edit/huge_asn_loader.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/edit/remote_updater.hpp>
#include <future>
#include <util/message_queue.hpp>
#include "xml_val_stream.hpp"
#include "thread_state.hpp"
#include <objtools/validator/huge_file_validator.hpp>

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;
USING_SCOPE(objects);
USING_SCOPE(validator);
USING_SCOPE(edit);

#define USE_XMLWRAPP_LIBS

class CFileReaderThread;

class CAsnvalApp : public CNcbiApplication
{
    friend class CAsnvalThreadState;
    friend class CFileReaderThread;

public:
    CAsnvalApp();
    ~CAsnvalApp();

    void Init() override;
    int Run() override;

    // CReadClassMemberHook override
    //void ReadClassMember(CObjectIStream& in,
    //    const CObjectInfo::CMemberIterator& member) override;

private:

    void Setup(const CArgs& args);
    void x_AliasLogFile();

    void ValidateOneDirectory(string dir_name, bool recurse);
    bool FinishReaderThread(CFileReaderThread&, bool blocking);

    CAsnvalThreadState mThreadState;
    unique_ptr<edit::CRemoteUpdater> mRemoteUpdater;
};

struct CThreadExitData
{
    size_t mNumRecords;
    double mLongest;
    string mLongestId;
};

class CFileReaderThread : public CThread
{
public:
    CFileReaderThread(
        CAsnvalThreadState& context) : mContext(context)
    {};
    bool mDone = false;
    CAsnvalThreadState mContext;
protected:
    void* Main() override
    {
        mContext.ValidateOneFile();
        this->mDone = true;
        CThreadExitData* pExitData = new CThreadExitData();
        pExitData->mNumRecords = mContext.m_NumRecords;
        pExitData->mLongest = mContext.m_Longest;
        pExitData->mLongestId = mContext.m_LongestId;
        return pExitData;
    }
};


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



// constructor
CAsnvalApp::CAsnvalApp() :
    mThreadState()
{
    const CVersionInfo vers(3, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY);
    SetVersion(vers);
}


// destructor
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


bool CAsnvalApp::FinishReaderThread(
    CFileReaderThread& thread,
    bool blocking)
{
    if (!blocking &&  !thread.mDone) {
        return false;
    }
    void* ppExitData = nullptr;
    thread.Join(&ppExitData);
    CThreadExitData* pExitData = static_cast<CThreadExitData*>(ppExitData);
    mThreadState.m_NumFiles++;
    mThreadState.m_NumRecords += pExitData->mNumRecords;
    if (pExitData->mLongest > mThreadState.m_Longest) {
        mThreadState.m_Longest = pExitData->mLongest;
        mThreadState.m_LongestId = pExitData->mLongestId;
    }
    delete pExitData;
    return true;
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
        ("p", "Directory", "Deprecated Path to ASN.1 Files",
        CArgDescriptions::eInputFile, CArgDescriptions::fHidden);

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
    CArgAllow* v_constraint = new CArgAllow_Integers(CAsnvalThreadState::eVerbosity_min, CAsnvalThreadState::eVerbosity_max);
    arg_desc->SetConstraint("v", v_constraint);

    arg_desc->AddFlag("cleanup", "Perform BasicCleanup before validating (to match C Toolkit)");
    arg_desc->AddFlag("batch", "Process NCBI release file (Seq-submit or Bioseq-set only)");
    arg_desc->AddFlag("huge", "Execute in huge-file mode");

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
    list<CFileReaderThread*> threadList;

    const size_t maxThreadCount = 8;
    size_t threadCount = 0;

    for (CDir::TEntry ii : files) {
        while (threadCount >= maxThreadCount) {
            for (auto* pThread : threadList) {
                if (FinishReaderThread(*pThread, false)) {
                    threadList.remove(pThread);
                    --threadCount;
                    break;
                }
            }
            SleepMilliSec(100);
        }
        string fname = ii->GetName();
        if (ii->IsFile() &&
            (!args["f"] || NStr::Find(fname, args["f"].AsString()) != NPOS)) {
            string fpath = CDirEntry::MakePath(dir_name, fname);

            //CAsnvalThreadState context(*this, fpath);
            CAsnvalThreadState context(mThreadState);
            context.mFilename = fpath;
            CFileReaderThread* pThread(new CFileReaderThread(context));
            pThread->Run();
            threadList.push_back(pThread);
            ++threadCount;
        }
    }
    for (auto* pThread : threadList) {
        FinishReaderThread(*pThread, true);
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

    if (args["o"]) {
        mThreadState.m_ValidErrorStream = &(args["o"].AsOutputFile());
    }

    if (args["D"]) {
        string lat_lon_path = args["D"].AsString();
        if (! lat_lon_path.empty()) {
            SetEnvironment( "NCBI_LAT_LON_DATA_PATH", lat_lon_path );
        }
    }

    // note - the C Toolkit uses 0 for SEV_NONE, but the C++ Toolkit uses 0 for SEV_INFO
    // adjust here to make the inputs to asnvalidate match asnval expectations
    mThreadState.m_ReportLevel = static_cast<EDiagSev>(args["R"].AsInteger() - 1);
    mThreadState.m_LowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    mThreadState.m_HighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);

    mThreadState.m_DoCleanup = args["cleanup"] && args["cleanup"].AsBoolean();
    mThreadState.m_verbosity = static_cast<CAsnvalThreadState::EVerbosity>(args["v"].AsInteger());
    if (args["batch"]) {
        mThreadState.m_batch = true;
    }

    mThreadState.m_Quiet = args["quiet"] && args["quiet"].AsBoolean();

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    mThreadState.m_Reported = 0;

    string m_obj_type = args["a"].AsString();

    if (!m_obj_type.empty()) {
        if (m_obj_type == "t" || m_obj_type == "u") {
            mThreadState.m_batch = true;
            cerr << "Warning: -a t and -a u are deprecated; use -batch instead." << endl;
        } else {
            cerr << "Warning: -a is deprecated; ASN.1 type is now autodetected." << endl;
        }
    }

    if (args["b"]) {
        cerr << "Warning: -b is deprecated; do not use" << endl;
    }

    bool exception_caught = false;
    try {
        mThreadState.ConstructOutputStreams();

        if (args["indir"]) {
            ValidateOneDirectory(args["indir"].AsString(), args["u"]);
        } else if (args["p"]) {
            // -p is deprecated in favor of -indir
            ValidateOneDirectory(args["p"].AsString(), args["u"]);
        } else {
            mThreadState.mFilename = (args["i"]) ? args["i"].AsString() : "";
            mThreadState.m_pContext->m_taxon_update = mRemoteUpdater->GetUpdateFunc();
            mThreadState.ValidateOneFile();
        }
    } catch (CException& e) {
        ERR_POST(Error << e);
        exception_caught = true;
    }
    if (mThreadState.m_NumFiles == 0) {
        ERR_POST("No matching files found");
    }

    time_t stop_time = time(NULL);
    if (! mThreadState.m_Quiet ) {
        LOG_POST_XX(Corelib_App, 1, "Finished in " << stop_time - start_time << " seconds");
        LOG_POST_XX(Corelib_App, 1, "Longest processing time " << mThreadState.m_Longest << " seconds on " << mThreadState.m_LongestId);
        LOG_POST_XX(Corelib_App, 1, "Total number of records " << mThreadState.m_NumRecords);
    }

    mThreadState.DestroyOutputStreams();

    if (mThreadState.m_Reported > 0 || exception_caught) {
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

    // Create object manager
    mThreadState.m_ObjMgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *mThreadState.m_ObjMgr,
                                         CDataLoadersUtil::fDefault |
                                         CDataLoadersUtil::fGenbankOffByDefault);

    mThreadState.m_OnlyAnnots = args["annot"];
    mThreadState.m_HugeFile = args["huge"];
    if (args["E"])
        mThreadState.m_OnlyError = args["E"].AsString();

    // Set validator options
    mThreadState.m_Options = CValidatorArgUtil::ArgsToValidatorOptions(args);
}


void CValXMLStream::Print(const CValidErrItem& item)
{
#if 0
    TTypeInfo info = item.GetThisTypeInfo();
    WriteObject(&item, info);
#else
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
#endif
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CAsnvalApp().AppMain(argc, argv);
}
