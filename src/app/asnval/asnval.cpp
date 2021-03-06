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
#include "message_queue.hpp"
#include <objtools/validator/huge_file_validator.hpp>

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;
USING_SCOPE(objects);
USING_SCOPE(validator);
USING_SCOPE(edit);

#define USE_XMLWRAPP_LIBS

namespace
{

class CAutoRevoker
{
public:
    template<class TLoader>
    CAutoRevoker(struct SRegisterLoaderInfo<TLoader>& info)
        : m_loader{info.GetLoader()}  {}
    ~CAutoRevoker()
    {
        CObjectManager::GetInstance()->RevokeDataLoader(*m_loader);
    }
private:
    CDataLoader* m_loader = nullptr;
};

static const set<TTypeInfo> s_known_types {
    CSeq_submit::GetTypeInfo(), CSeq_entry::GetTypeInfo(), CSeq_annot::GetTypeInfo(),
    CSeq_feat::GetTypeInfo(),   CBioSource::GetTypeInfo(), CPubdesc::GetTypeInfo(),
    CBioseq_set::GetTypeInfo(), CBioseq::GetTypeInfo(),    CSeqdesc::GetTypeInfo(),
};

};

class CValXMLStream;

class CAsnvalApp : public CReadClassMemberHook, public CNcbiApplication
{
public:
    CAsnvalApp();
    ~CAsnvalApp();

    void Init() override;
    int Run() override;

    // CReadClassMemberHook override
    void ReadClassMember(CObjectIStream& in,
        const CObjectInfo::CMemberIterator& member) override;

private:

    void Setup(const CArgs& args);
    void x_AliasLogFile();

    unique_ptr<CObjectIStream> OpenFile(const string& fname, TTypeInfo& asn_info);

    CConstRef<CValidError> ProcessSeqEntry(CSeq_entry& se);
    CConstRef<CValidError> ProcessSeqEntry();
    CConstRef<CValidError> ProcessSeqSubmit();
    CConstRef<CValidError> ProcessSeqAnnot();
    CConstRef<CValidError> ProcessSeqFeat();
    CConstRef<CValidError> ProcessBioSource();
    CConstRef<CValidError> ProcessPubdesc();
    CConstRef<CValidError> ProcessBioseqset();
    CConstRef<CValidError> ProcessBioseq();
    CConstRef<CValidError> ProcessSeqDesc();

    CConstRef<CValidError> ValidateInput(TTypeInfo asninfo);
    void ValidateOneDirectory(string dir_name, bool recurse);
    void ValidateOneFile(const string& fname);
    void ValidateOneHugeFile(const string& loader_name, bool use_mt);
    CConstRef<CValidError> x_ValidateAsync(const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid, CRef<CSeq_entry> pEntry);

    template<class _Type>
    CConstRef<CValidError> x_ReadAny(CRef<_Type>& obj);

    void ProcessBSSReleaseFile();
    void ProcessSSMReleaseFile();

    void ConstructOutputStreams();
    void DestroyOutputStreams();

    /// @param p_exception
    ///   Pointer to the exception with more info on the read failure or nullptr if not available
    /// @return
    ///   The error(s) to add to describe the read failure
    CRef<CValidError> ReportReadFailure(const CException* p_exception);

    CRef<CScope> BuildScope();

    void PrintValidError(CConstRef<CValidError> errors);

    enum EVerbosity {
        eVerbosity_Normal = 1,
        eVerbosity_Spaced = 2,
        eVerbosity_Tabbed = 3,
        eVerbosity_XML = 4,
        eVerbosity_min = 1, eVerbosity_max = 4
    };

    void PrintValidErrItem(const CValidErrItem& item);

    CRef<CObjectManager> m_ObjMgr;
    unique_ptr<CObjectIStream> m_In;
    unsigned int m_Options;
    bool m_HugeFile = false;
    bool m_Continue;
    bool m_OnlyAnnots;
    bool m_Quiet;
    double m_Longest;
    string m_CurrentId;
    string m_LongestId;
    size_t m_NumFiles;
    size_t m_NumRecords;

    size_t m_Level;
    std::atomic<size_t> m_Reported;
    EDiagSev m_ReportLevel;

    bool m_DoCleanup;
    CCleanup m_Cleanup;

    EDiagSev m_LowCutoff;
    EDiagSev m_HighCutoff;

    EVerbosity m_verbosity;
    bool m_batch = false;
    string m_OnlyError;

    unique_ptr<edit::CHugeFileProcess> m_pHugeFileProcess;
    CHugeFileValidator::TGlobalInfo m_GlobalInfo;
    CNcbiOstream* m_ValidErrorStream;

    shared_ptr<SValidatorContext> m_pContext;
    unique_ptr<edit::CRemoteUpdater> m_remote_updater;

#ifdef USE_XMLWRAPP_LIBS
    unique_ptr<CValXMLStream> m_ostr_xml;
#endif
};

class CValXMLStream : public CObjectOStreamXml
{
public:
    CValXMLStream(CNcbiOstream& out, EOwnership deleteOut) : CObjectOStreamXml(out, deleteOut){};
    void Print(const CValidErrItem& item);
};


// constructor
CAsnvalApp::CAsnvalApp() :
    m_ObjMgr(), m_Options(0), m_Continue(false), m_OnlyAnnots(false),
    m_Quiet(false), m_Longest(0.0), m_CurrentId(), m_LongestId(), m_NumFiles(0),
    m_NumRecords(0), m_Level(0), m_Reported(0), m_verbosity(eVerbosity_min),
    m_ValidErrorStream(nullptr)
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
    CArgAllow* v_constraint = new CArgAllow_Integers(eVerbosity_min, eVerbosity_max);
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


CConstRef<CValidError> CAsnvalApp::ValidateInput(TTypeInfo asninfo)
{
    // Process file based on its content
    CConstRef<CValidError> eval;

    string asn_type = m_In->ReadFileHeader();
    if (asninfo)
        asn_type = asninfo->GetName();

    if (asn_type == "Seq-submit") {                 // Seq-submit
        eval = ProcessSeqSubmit();
    } else if (asn_type == "Seq-entry") {           // Seq-entry
        eval = ProcessSeqEntry();
    } else if (asn_type == "Seq-annot") {           // Seq-annot
        eval = ProcessSeqAnnot();
    } else if (asn_type == "Seq-feat") {            // Seq-feat
        eval = ProcessSeqFeat();
    } else if (asn_type == "BioSource") {           // BioSource
        eval = ProcessBioSource();
    } else if (asn_type == "Pubdesc") {             // Pubdesc
        eval = ProcessPubdesc();
    } else if (asn_type == "Bioseq-set") {          // Bioseq-set
        eval = ProcessBioseqset();
    } else if (asn_type == "Bioseq") {              // Bioseq
        eval = ProcessBioseq();
    } else if (asn_type == "Seqdesc") {             // Seq-desc
        eval = ProcessSeqDesc();
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + asn_type);
    }

    return eval;
}

CConstRef<CValidError> CAsnvalApp::x_ValidateAsync(const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid, CRef<CSeq_entry> pEntry)
{
    CRef<CScope> scope = BuildScope();
    if (!loader_name.empty())
        scope->AddDataLoader(loader_name);

    CConstRef<CValidError> eval;

    CValidator validator(*m_ObjMgr, m_pContext);

    CSeq_entry_Handle top_h;
    if (pEntry) {
        top_h = scope->AddTopLevelSeqEntry(*pEntry);
    } else {
        auto seq_id_h = CSeq_id_Handle::GetHandle(*seqid);
        if (scope->Exists(seq_id_h)) {
#ifdef _DEBUG
            std::cerr << "Taxid for " << scope->GetLabel(seq_id_h) << " : " << scope->GetTaxId(seq_id_h, CScope::fDoNotRecalculate) << "\n";
#endif
            auto bioseq_h = scope->GetBioseqHandle(seq_id_h);
            if (bioseq_h)
                top_h = bioseq_h.GetTopLevelEntry();
            if (top_h)
                pEntry = Ref((CSeq_entry*)(void*)top_h.GetCompleteSeq_entry().GetPointer());
        }
    }

    if (top_h) {

        if (m_DoCleanup) {
            CCleanup cleanup;
            cleanup.SetScope(scope);
            cleanup.BasicCleanup(*pEntry);
        }

        if (pSubmitBlock) {
            auto pSubmit = Ref(new CSeq_submit());
            pSubmit->SetSub().Assign(*pSubmitBlock);
            pSubmit->SetData().SetEntrys().push_back(pEntry);
            eval = validator.Validate(*pSubmit, scope, m_Options);
        } else {
            eval = validator.Validate(*pEntry, scope, m_Options);
        }
    }

    scope->ResetDataAndHistory();
    return eval;
}

void CAsnvalApp::ValidateOneHugeFile(const string& loader_name, bool use_mt)
{
    auto& reader = m_pHugeFileProcess->GetReader();


    while(true)
    {
        m_GlobalInfo.Clear();
        try
        {
            if (!reader.GetNextBlob())
                break;
        }
        catch(const CException& e)
        {
            auto errors = ReportReadFailure(&e);
            PrintValidError(errors);
            return;
        }

        m_NumRecords++;

        try
        {
            reader.FlattenGenbankSet();
        }
        catch(const edit::CHugeFileException& e)
        {
            if (e.GetErrCode() == edit::CHugeFileException::eDuplicateSeqIds)
            {
                auto errors = Ref(new CValidError);
                errors->AddValidErrItem(eDiag_Critical, eErr_GENERIC_DuplicateIDs, e.GetMsg());
                PrintValidError(errors);
                ++m_Reported;
                continue;
            }
            throw;
        }


        auto info = edit::CHugeAsnDataLoader::RegisterInObjectManager(
                *m_ObjMgr, loader_name, &reader, CObjectManager::eDefault, 1); //CObjectManager::kPriority_Local);

        CAutoRevoker autorevoker(info);
        CHugeFileValidator hugeFileValidator(reader, m_Options);
        hugeFileValidator.UpdateValidatorContext(m_GlobalInfo, *m_pContext);

        if (m_pContext->PreprocessHugeFile) {
            CRef<CValidError> pEval;
            hugeFileValidator.ReportGlobalErrors(m_GlobalInfo, pEval);
            if (pEval) {
                PrintValidError(pEval);
            }
        }

        if (use_mt)
        {
            CMessageQueue<std::future<CConstRef<CValidError>>, CMessageQueueSizeLimit<10>> val_queue;
            // start a loop in a separate thread
            auto topids_task = std::async(std::launch::async, [this, &val_queue, &loader_name, &reader]()
            {
                auto pSubmitBlock = reader.GetSubmitBlock();
                for (auto seqid: reader.GetTopIds())
                {
                    auto params = std::async(std::launch::async,
                        [this, seqid, &loader_name, pSubmitBlock] () -> CConstRef<CValidError>
                        {
                            try
                            {
                                return x_ValidateAsync(loader_name, pSubmitBlock, seqid, {});
                            } catch (const CException& e) {
                                string errstr = e.GetMsg();
                                errstr = NStr::Replace(errstr, "\n", " * ");
                                errstr = NStr::Replace(errstr, " *   ", " * ");
                                CRef<CValidError> eval(new CValidError());
                                eval->AddValidErrItem(eDiag_Fatal, eErr_INTERNAL_Exception, errstr);
                                ERR_POST(e);
                                ++m_Reported;
                                return eval;
                            }
                        });
                    // std::future is not copiable, so passing it for move constructor
                    val_queue.PostMessage(std::move(params));
                }

                val_queue.PostMessage({});
            });

            while(true)
            {
                auto result = val_queue.RetrieveMessage();
                if (!result.valid())
                    break;
                auto eval = result.get();
                PrintValidError(eval);
            }

            topids_task.wait();

        } else {
            for (auto seqid: reader.GetTopIds())
            {
                auto pSubmitBlock = reader.GetSubmitBlock();
                CConstRef<CValidError> eval;
#ifdef _DEBUG1
                cerr << MSerial_AsnText << "Validating: " << seqid->AsFastaString() << "\n";
#endif
                auto seqid_str = seqid->AsFastaString();
                eval = x_ValidateAsync(loader_name, pSubmitBlock, seqid, {});
                PrintValidError(eval);
            }
        }
    }
}

void CAsnvalApp::ValidateOneFile(const string& fname)
{
    const CArgs& args = GetArgs();

    if (! args["quiet"] ) {
        LOG_POST_XX(Corelib_App, 1, fname);
    }

    m_pContext.reset(new SValidatorContext());
    m_pContext->m_taxon_update = m_remote_updater->GetUpdateFunc();

    unique_ptr<CNcbiOfstream> local_stream;

    bool close_error_stream = false;

    try {
        if (!m_ValidErrorStream) {
            string path;
            if (fname.empty()) {
                path = "stdin.val";
            } else {
                size_t pos = NStr::Find(fname, ".", NStr::eNocase, NStr::eReverseSearch);
                if (pos != NPOS)
                    path = fname.substr(0, pos);
                else
                    path = fname;

                path.append(".val");
            }

            local_stream.reset(new CNcbiOfstream(path));
            m_ValidErrorStream = local_stream.get();

            ConstructOutputStreams();
            close_error_stream = true;
        }
    }
    catch (const CException&) {
    }

    TTypeInfo asninfo = nullptr;
    if (fname.empty())// || true)
        m_In = OpenFile(fname, asninfo);
    else {
        m_pHugeFileProcess.reset(new CHugeFileProcess(new CValidatorHugeAsnReader(m_GlobalInfo)));
        try
        {
            m_pHugeFileProcess->Open(fname, &s_known_types);
            asninfo = m_pHugeFileProcess->GetFile().RecognizeContent(0);
        }
        catch(const CObjReaderParseException&)
        {
            m_pHugeFileProcess.reset();
        }

        if (asninfo)
        {
            if (!m_HugeFile || m_batch || !edit::CHugeFileProcess::IsSupported(asninfo))
            {
                m_In = m_pHugeFileProcess->GetReader().MakeObjStream(0);
            }
        } else {
            m_In = OpenFile(fname, asninfo);
        }

    }

    if (!asninfo)
    {
        PrintValidError(ReportReadFailure(nullptr));
        if (close_error_stream) {
            DestroyOutputStreams();
        }
        // NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Unable to open invalid ASN.1 file " + fname);
    }

    if (asninfo)
    {
        try {
            if (m_batch) {
                if (asninfo == CBioseq_set::GetTypeInfo()) {
                    ProcessBSSReleaseFile();
                } else
                if (asninfo == CSeq_submit::GetTypeInfo()) {
                    const auto commandLineOptions = m_Options;
                    m_Options |= CValidator::eVal_seqsubmit_parent;
                    try {
                        ProcessSSMReleaseFile();
                    }
                    catch (CException& e) {
                        m_Options = commandLineOptions;
                        throw;
                    }
                    m_Options = commandLineOptions;

                } else {
                    LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is neither a Seq-submit nor Bioseq-set; do not use -batch to process.");
                }
            } else {
                size_t num_validated = 0;
                bool doloop = true;
                while (doloop) {
                    CStopWatch sw(CStopWatch::eStart);
                    try {
                        if (m_In) {
                            CConstRef<CValidError> eval = ValidateInput(asninfo);
                            if (eval)
                                PrintValidError(eval);

                            if (!m_In->EndOfData()) { // force to SkipWhiteSpace
                                try
                                {
                                    auto types = m_In->GuessDataType(s_known_types);
                                    asninfo = types.empty() ? nullptr : *types.begin();
                                }
                                catch(const CException& e)
                                {
                                    eval = ReportReadFailure(&e);
                                    if (eval) {
                                        PrintValidError(eval);
                                        doloop = false;
                                    } else
                                        throw;
                                }
                            }
                        } else {
                            string loader_name =  CDirEntry::ConvertToOSPath(fname);
                            bool use_mt = true;
                            #ifdef _DEBUG
                                // multitheading in DEBUG mode is not convinient
                                use_mt = false;
                            #endif
                            ValidateOneHugeFile(loader_name, use_mt);
                            doloop = false;
                        }

                        num_validated++;
                    }
                    catch (const CEofException&) {
                        break;
                    }
                    catch (const CException& e) {
                        if (num_validated == 0) {
                            throw(e);
                        }
                        else {
                            break;
                        }
                    }
                    double elapsed = sw.Elapsed();
                    if (elapsed > m_Longest) {
                        m_Longest = elapsed;
                        m_LongestId = m_CurrentId;
                    }
                }
            }
        } catch (const CException& e) {
            string errstr = e.GetMsg();
            errstr = NStr::Replace(errstr, "\n", " * ");
            errstr = NStr::Replace(errstr, " *   ", " * ");
            CRef<CValidError> eval(new CValidError());
            if (NStr::StartsWith(errstr, "duplicate Bioseq id", NStr::eNocase)) {
                eval->AddValidErrItem(eDiag_Critical, eErr_GENERIC_DuplicateIDs, errstr);
                PrintValidError(eval);
                // ERR_POST(e);
            } else {
                eval->AddValidErrItem(eDiag_Fatal, eErr_INTERNAL_Exception, errstr);
                PrintValidError(eval);
                ERR_POST(e);
            }
            ++m_Reported;
        }
    }
    m_NumFiles++;
    if (close_error_stream) {
        DestroyOutputStreams();
    }
    m_In.reset();
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
            ValidateOneFile(fpath);
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


int CAsnvalApp::Run()
{
    const CArgs& args = GetArgs();
    Setup(args);

    m_remote_updater.reset(new edit::CRemoteUpdater(nullptr)); //m_logger));

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
        m_ValidErrorStream = &(args["o"].AsOutputFile());
    }

    if (args["D"]) {
        string lat_lon_path = args["D"].AsString();
        if (! lat_lon_path.empty()) {
            SetEnvironment( "NCBI_LAT_LON_DATA_PATH", lat_lon_path );
        }
    }

    // note - the C Toolkit uses 0 for SEV_NONE, but the C++ Toolkit uses 0 for SEV_INFO
    // adjust here to make the inputs to asnvalidate match asnval expectations
    m_ReportLevel = static_cast<EDiagSev>(args["R"].AsInteger() - 1);
    m_LowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    m_HighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);

    m_DoCleanup = args["cleanup"] && args["cleanup"].AsBoolean();
    m_verbosity = static_cast<EVerbosity>(args["v"].AsInteger());
    if (args["batch"]) {
        m_batch = true;
    }

    m_Quiet = args["quiet"] && args["quiet"].AsBoolean();

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    m_Reported = 0;

    string m_obj_type = args["a"].AsString();

    if (!m_obj_type.empty()) {
        if (m_obj_type == "t" || m_obj_type == "u") {
            m_batch = true;
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
        ConstructOutputStreams();

        if (args["indir"]) {
            ValidateOneDirectory(args["indir"].AsString(), args["u"]);
        } else if (args["p"]) {
            // -p is deprecated in favor of -indir
            ValidateOneDirectory(args["p"].AsString(), args["u"]);
        } else if (args["i"]) {
            ValidateOneFile(args["i"].AsString());
        } else {
            ValidateOneFile("");
        }
    } catch (CException& e) {
        ERR_POST(Error << e);
        exception_caught = true;
    }
    if (m_NumFiles == 0) {
        ERR_POST("No matching files found");
    }

    time_t stop_time = time(NULL);
    if (! m_Quiet ) {
        LOG_POST_XX(Corelib_App, 1, "Finished in " << stop_time - start_time << " seconds");
        LOG_POST_XX(Corelib_App, 1, "Longest processing time " << m_Longest << " seconds on " << m_LongestId);
        LOG_POST_XX(Corelib_App, 1, "Total number of records " << m_NumRecords);
    }

    DestroyOutputStreams();

    if (m_Reported > 0 || exception_caught) {
        return 1;
    } else {
        return 0;
    }
}


CRef<CScope> CAsnvalApp::BuildScope()
{
    CRef<CScope> scope(new CScope(*m_ObjMgr));
    scope->AddDefaults();

    return scope;
}


void CAsnvalApp::ReadClassMember
(CObjectIStream& in,
 const CObjectInfo::CMemberIterator& member)
{
    m_Level++;

    if ( m_Level == 1 ) {
        size_t n = 0;
        // Read each element separately to a local TSeqEntry,
        // process it somehow, and... not store it in the container.
        for ( CIStreamContainerIterator i(in, member); i; ++i ) {
            try {
                // Get seq-entry to validate
                CRef<CSeq_entry> se(new CSeq_entry);
                i >> *se;

                // Validate Seq-entry
                CValidator validator(*m_ObjMgr, m_pContext);
                CRef<CScope> scope = BuildScope();
                CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*se);

                CBioseq_CI bi(seh);
                if (bi) {
                    m_CurrentId = "";
                    bi->GetId().front().GetSeqId()->GetLabel(&m_CurrentId);
                    if (! m_Quiet ) {
                        LOG_POST_XX(Corelib_App, 1, m_CurrentId);
                    }
                }

                if (m_DoCleanup) {
                    m_Cleanup.SetScope(scope);
                    m_Cleanup.BasicCleanup(*se);
                }

                if ( m_OnlyAnnots ) {
                    for (CSeq_annot_CI ni(seh); ni; ++ni) {
                        const CSeq_annot_Handle& sah = *ni;
                        CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
                        m_NumRecords++;
                        if ( eval ) {
                            PrintValidError(eval);
                        }
                    }
                } else {
                    // CConstRef<CValidError> eval = validator.Validate(*se, &scope, m_Options);
                    CStopWatch sw(CStopWatch::eStart);
                    CConstRef<CValidError> eval = validator.Validate(seh, m_Options);
                    m_NumRecords++;
                    double elapsed = sw.Elapsed();
                    if (elapsed > m_Longest) {
                        m_Longest = elapsed;
                        m_LongestId = m_CurrentId;
                    }
                    //if (m_ValidErrorStream) {
                    //    *m_ValidErrorStream << "Elapsed = " << sw.Elapsed() << "\n";
                    //}
                    if ( eval ) {
                        PrintValidError(eval);
                    }
                }
                scope->RemoveTopLevelSeqEntry(seh);
                scope->ResetHistory();
                n++;
            } catch (exception&) {
                if ( !m_Continue ) {
                    throw;
                }
                // should we issue some sort of warning?
            }
        }
    } else {
        in.ReadClassMember(member);
    }

    m_Level--;
}


void CAsnvalApp::ProcessBSSReleaseFile()
{
    CRef<CBioseq_set> seqset(new CBioseq_set);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CBioseq_set>();
    set_type.FindMember("seq-set").SetLocalReadHook(*m_In, this);

    // Read the CBioseq_set, it will call the hook object each time we
    // encounter a Seq-entry
    try {
        *m_In >> *seqset;
    }
    catch (const CException&) {
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is not a batch Bioseq-set, do not use -a t to process.");
        ++m_Reported;
    }
}

void CAsnvalApp::ProcessSSMReleaseFile()
{
    CRef<CSeq_submit> seqset(new CSeq_submit);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CSeq_submit>();
    (*(*(*set_type.FindMember("data")).GetPointedType().FindVariant("entrys")).GetElementType().GetPointedType().FindVariant("set")).FindMember("seq-set").SetLocalReadHook(*m_In, this);

    // Read the CSeq_submit, it will call the hook object each time we
    // encounter a Seq-entry
    try {
        *m_In >> *seqset;
    }
    catch (const CException&) {
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is not a batch Seq-submit, do not use -a u to process.");
        ++m_Reported;
    }
}

CRef<CValidError> CAsnvalApp::ReportReadFailure(const CException* p_exception)
{
    CRef<CValidError> errors(new CValidError());

    CNcbiOstrstream os;
    os << "Unable to read invalid ASN.1";

    const CSerialException* p_serial_exception = dynamic_cast<const CSerialException*>(p_exception);
    if( p_serial_exception ) {
        if( m_In ) {
            os << ": " << m_In->GetPosition();
        }
        if( p_serial_exception->GetErrCode() == CSerialException::eEOF ) {
            os << ": unexpected end of file";
        } else {
            // manually call ReportAll(0) because what() includes a lot of info
            // that's not of interest to the submitter such as stacktraces and
            // GetMsg() doesn't include enough info.
            os << ": " + p_exception->ReportAll(0);
        }
    }

    string errstr = CNcbiOstrstreamToString(os);
    // newlines don't play well with XML
    errstr = NStr::Replace(errstr, "\n", " * ");
    errstr = NStr::Replace(errstr, " *   ", " * ");

    errors->AddValidErrItem(eDiag_Critical, eErr_GENERIC_InvalidAsn, errstr);
    return errors;
}

template<class _Type>
CConstRef<CValidError> CAsnvalApp::x_ReadAny(CRef<_Type>& obj)
{
    try {
        if (!obj)
            obj.Reset(new _Type);
        m_In->Read(obj, _Type::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        return {};
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }
}

CConstRef<CValidError> CAsnvalApp::ProcessBioseq()
{
    // Get seq-entry to validate
    CRef<CBioseq> bioseq;

    auto eval = x_ReadAny(bioseq);
    if (eval)
        return eval;

    auto se = Ref(new CSeq_entry);
    se->SetSeq(*bioseq);

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}

CConstRef<CValidError> CAsnvalApp::ProcessBioseqset()
{
    // Get seq-entry to validate
    CRef<CBioseq_set> bioseqset;

    auto eval = x_ReadAny(bioseqset);
    if (eval)
        return eval;

    auto se = Ref(new CSeq_entry);
    se->SetSet(*bioseqset);

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqEntry()
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se;
    auto eval = x_ReadAny(se);
    if (eval)
        return eval;

#if 0
    try
    {
        return ProcessSeqEntry(*se);
    }
    catch (const CObjMgrException& om_ex)
    {
        if (om_ex.GetErrCode() == CObjMgrException::eAddDataError)
            se->ReassignConflictingIds();
    }
#endif

    // try again
    return ProcessSeqEntry(*se);
}

CConstRef<CValidError> CAsnvalApp::ProcessSeqEntry(CSeq_entry& se)
{
    // Validate Seq-entry
    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(se);
    }
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(se);
    CBioseq_CI bi(seh);
    if (bi) {
        m_CurrentId = "";
        bi->GetId().front().GetSeqId()->GetLabel(&m_CurrentId);
        if (! m_Quiet ) {
            LOG_POST_XX(Corelib_App, 1, m_CurrentId);
        }
    }

    if ( m_OnlyAnnots ) {
        for (CSeq_annot_CI ni(seh); ni; ++ni) {
            const CSeq_annot_Handle& sah = *ni;
            CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
            m_NumRecords++;
            if ( eval ) {
                PrintValidError(eval);
            }
        }
        return CConstRef<CValidError>();
    }
    CConstRef<CValidError> eval = validator.Validate(se, scope, m_Options);
    m_NumRecords++;
    return eval;
}

CConstRef<CValidError> CAsnvalApp::ProcessSeqFeat()
{
    CRef<CSeq_feat> feat;
    auto eval = x_ReadAny(feat);
    if (eval)
        return eval;

    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(*feat);
    }

    CValidator validator(*m_ObjMgr, m_pContext);
    eval = validator.Validate(*feat, scope, m_Options);
    m_NumRecords++;
    return eval;
}

CConstRef<CValidError> CAsnvalApp::ProcessBioSource()
{
    CRef<CBioSource> src;
    auto eval = x_ReadAny(src);
    if (eval)
        return eval;

    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    eval = validator.Validate(*src, scope, m_Options);
    m_NumRecords++;
    return eval;
}

CConstRef<CValidError> CAsnvalApp::ProcessPubdesc()
{
    CRef<CPubdesc> pd;
    auto eval = x_ReadAny(pd);
    if (eval)
        return eval;

    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    eval = validator.Validate(*pd, scope, m_Options);
    m_NumRecords++;
    return eval;
}

CConstRef<CValidError> CAsnvalApp::ProcessSeqSubmit()
{

    CRef<CSeq_submit> ss;
    auto eval = x_ReadAny(ss);
    if (eval)
        return eval;

    // Validate Seq-submit
    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, se, ss->SetData().SetEntrys()) {
            scope->AddTopLevelSeqEntry(**se);
        }
    }
    if (m_DoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(*ss);
    }

    eval = validator.Validate(*ss, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqAnnot()
{
    CRef<CSeq_annot> sa(new CSeq_annot);

    // Get seq-annot to validate
    try {
        m_In->Read(ObjectInfo(*sa), CObjectIStream::eNoFileHeader);
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    // Validate Seq-annot
    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(*sa);
    }
    CSeq_annot_Handle sah = scope->AddSeq_annot(*sa);
    CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
    m_NumRecords++;
    return eval;
}

static CRef<objects::CSeq_entry> s_BuildGoodSeq()
{
    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry());
    entry->SetSeq().SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    entry->SetSeq().SetInst().SetLength(60);

    CRef<objects::CSeq_id> id(new objects::CSeq_id());
    id->SetLocal().SetStr("good");
    entry->SetSeq().SetId().push_back(id);

    CRef<objects::CSeqdesc> mdesc(new objects::CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_genomic);
    entry->SetSeq().SetDescr().Set().push_back(mdesc);

    /*
    AddGoodSource (entry);
    AddGoodPub(entry);
    */

    return entry;
}

CConstRef<CValidError> CAsnvalApp::ProcessSeqDesc()
{
    CRef<CSeqdesc> sd(new CSeqdesc);

    try {
        m_In->Read(ObjectInfo(*sd), CObjectIStream::eNoFileHeader);
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    CRef<CSeq_entry> ctx = s_BuildGoodSeq();

    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*ctx);

    CConstRef<CValidError> eval = validator.Validate(*sd, *ctx);
    m_NumRecords++;
    return eval;
}


void CAsnvalApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    // CORE_SetLOG(LOG_cxx2c());
    // CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    m_ObjMgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *m_ObjMgr,
                                         CDataLoadersUtil::fDefault |
                                         CDataLoadersUtil::fGenbankOffByDefault);

    m_OnlyAnnots = args["annot"];
    m_HugeFile = args["huge"];
    if (args["E"])
        m_OnlyError = args["E"].AsString();

    // Set validator options
    m_Options = CValidatorArgUtil::ArgsToValidatorOptions(args);


}


unique_ptr<CObjectIStream> CAsnvalApp::OpenFile(const string& fname, TTypeInfo& asn_info)
{
    ENcbiOwnership own = eNoOwnership;
    unique_ptr<CNcbiIstream> hold_stream;
    CNcbiIstream* InputStream = &NcbiCin;

    if (!fname.empty()) {
        own = eTakeOwnership;
        hold_stream = make_unique<CNcbiIfstream>(fname, ios::binary);
        InputStream = hold_stream.get();
    }

    CCompressStream::EMethod method;

    CFormatGuessEx FG(*InputStream);
    CFileContentInfo contentInfo;
    FG.SetRecognizedGenbankTypes(s_known_types);
    CFormatGuess::EFormat format = FG.GuessFormatAndContent(contentInfo);
    switch (format)
    {
        case CFormatGuess::eGZip:  method = CCompressStream::eGZipFile;  break;
        case CFormatGuess::eBZip2: method = CCompressStream::eBZip2;     break;
        case CFormatGuess::eLzo:   method = CCompressStream::eLZO;       break;
        default:                   method = CCompressStream::eNone;      break;
    }
    if (method != CCompressStream::eNone)
    {
        CDecompressIStream* decompress(new CDecompressIStream(*InputStream, method, CCompressStream::fDefault, own));
        hold_stream.release();
        hold_stream.reset(decompress);
        InputStream = hold_stream.get();
        own = eTakeOwnership;
        CFormatGuessEx fg(*InputStream);
        format = fg.GuessFormatAndContent(contentInfo);
    }

    unique_ptr<CObjectIStream> objectStream;
    switch (format)
    {
        case CFormatGuess::eBinaryASN:
        case CFormatGuess::eTextASN:
            objectStream.reset(CObjectIStream::Open(format==CFormatGuess::eBinaryASN ? eSerial_AsnBinary : eSerial_AsnText, *InputStream, own));
            hold_stream.release();
            asn_info = contentInfo.mInfoGenbank.mTypeInfo;
            break;
        default:
            break;
    }
    return objectStream;
}


void CAsnvalApp::PrintValidError
(CConstRef<CValidError> errors)
{
    if ( errors.Empty() || errors->TotalSize() == 0 ) {
        return;
    }

    for ( CValidError_CI vit(*errors); vit; ++vit) {
        if (vit->GetSeverity() >= m_ReportLevel) {
            ++m_Reported;
        }
        if ( vit->GetSeverity() < m_LowCutoff || vit->GetSeverity() > m_HighCutoff) {
            continue;
        }
        if (!m_OnlyError.empty() && !(NStr::EqualNocase(m_OnlyError, vit->GetErrCode()))) {
            continue;
        }
        PrintValidErrItem(*vit);
    }
    //m_ValidErrorStream->flush();
}


static string s_GetSeverityLabel(EDiagSev sev, bool is_xml = false)
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


void CAsnvalApp::PrintValidErrItem(const CValidErrItem& item)
{
    CNcbiOstream& os = *m_ValidErrorStream;
    switch (m_verbosity) {
    case eVerbosity_Normal:
        os << s_GetSeverityLabel(item.GetSeverity())
            << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() << "] "
            << item.GetMsg();
        if (item.IsSetObjDesc()) {
            os << " " << item.GetObjDesc();
        }
        os << "\n";
        break;
    case eVerbosity_Spaced:
    {
        string spacer = "                    ";
        string msg = item.GetAccnver() + spacer;
        msg = msg.substr(0, 15);
        msg += s_GetSeverityLabel(item.GetSeverity());
        msg += spacer;
        msg = msg.substr(0, 30);
        msg += item.GetErrGroup() + "_" + item.GetErrCode();
        os << msg << "\n";
    }
    break;
    case eVerbosity_Tabbed:
        os << item.GetAccnver() << "\t"
            << s_GetSeverityLabel(item.GetSeverity()) << "\t"
            << item.GetErrGroup() << "_" << item.GetErrCode() << "\n";
        break;
#ifdef USE_XMLWRAPP_LIBS
    case eVerbosity_XML:
    {
        m_ostr_xml->Print(item);
    }
#else
    case eVerbosity_XML:
    {
        string msg = NStr::XmlEncode(item.GetMsg());
        if (item.IsSetFeatureId()) {
            os << "  <message severity=\"" << s_GetSeverityLabel(item.GetSeverity())
                << "\" seq-id=\"" << item.GetAccnver()
                << "\" feat-id=\"" << item.GetFeatureId()
                << "\" code=\"" << item.GetErrGroup() << "_" << item.GetErrCode()
                << "\">" << msg << "</message>" << "\n";
        } else {
            os << "  <message severity=\"" << s_GetSeverityLabel(item.GetSeverity())
                << "\" seq-id=\"" << item.GetAccnver()
                << "\" code=\"" << item.GetErrGroup() << "_" << item.GetErrCode()
                << "\">" << msg << "</message>" << "\n";
        }
    }
#endif
    break;
    }
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

void CAsnvalApp::ConstructOutputStreams()
{
    if (m_ValidErrorStream && m_verbosity == eVerbosity_XML)
    {
#ifdef USE_XMLWRAPP_LIBS
        m_ostr_xml.reset(new CValXMLStream(*m_ValidErrorStream, eNoOwnership));
        m_ostr_xml->SetEncoding(eEncoding_UTF8);
        m_ostr_xml->SetReferenceDTD(false);
        m_ostr_xml->SetEnforcedStdXml(true);
        m_ostr_xml->WriteFileHeader(CValidErrItem::GetTypeInfo());
        m_ostr_xml->SetUseIndentation(true);
        m_ostr_xml->Flush();

        *m_ValidErrorStream << "\n" << "<asnvalidate version=\"" << "3." << NCBI_SC_VERSION_PROXY << "."
        << NCBI_TEAMCITY_BUILD_NUMBER_PROXY << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << "\n";
        m_ValidErrorStream->flush();
#else
        *m_ValidErrorStream << "<asnvalidate version=\"" << "3." << NCBI_SC_VERSION_PROXY << "."
        << NCBI_TEAMCITY_BUILD_NUMBER_PROXY << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << "\n";
#endif
    }
}

void CAsnvalApp::DestroyOutputStreams()
{
#ifdef USE_XMLWRAPP_LIBS
    if (m_ostr_xml)
    {
        m_ostr_xml.reset();
        *m_ValidErrorStream << "</asnvalidate>" << "\n";
    }
#endif
    m_ValidErrorStream = nullptr;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CAsnvalApp().AppMain(argc, argv);
}
