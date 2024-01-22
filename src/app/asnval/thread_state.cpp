/* $Id$
* ========================================================================== =
*
*PUBLIC DOMAIN NOTICE
* National Center for Biotechnology Information
*
* This software / database is a "United States Government Work" under the
* terms of the United States Copyright Act.It was written as part of
* the author's official duties as a United States Government employee and
* thus cannot be copyrighted.This software / database is freely available
* to the public for use.The National Library of Medicineand the U.S.
* Government have not placed any restriction on its use or reproduction.
*
*Although all reasonable efforts have been taken to ensure the accuracy
* andreliability of the software and data, the NLMand the U.S.
* Government do not and cannot warrant the performance or results that
* may be obtained by using this software or data.The NLM and the U.S.
* Government disclaim all warranties, express or implied, including
* warranties of performance, merchantability or fitness for any particular
* purpose.
*
* Please cite the author in any work or product based on this material.
*
*========================================================================== =
*
*Author:  Frank Ludwig
*
* File Description :
*validator
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
#include "app_config.hpp"
#include "thread_state.hpp"
#include <objtools/validator/huge_file_validator.hpp>
#include <objtools/readers/objhook_lambdas.hpp>

using namespace ncbi;
USING_SCOPE(objects);
USING_SCOPE(validator);
USING_SCOPE(edit);

namespace
{

    class CAutoRevoker
    {
    public:
        template<class TLoader>
        CAutoRevoker(struct SRegisterLoaderInfo<TLoader>& info)
            : m_loader{ info.GetLoader() } {}
        ~CAutoRevoker()
        {
            CObjectManager::GetInstance()->RevokeDataLoader(*m_loader);
        }
    private:
        CDataLoader* m_loader = nullptr;
    };
}


const set<TTypeInfo> s_known_types{
    CSeq_submit::GetTypeInfo(), CSeq_entry::GetTypeInfo(), CSeq_annot::GetTypeInfo(),
    CSeq_feat::GetTypeInfo(),   CBioSource::GetTypeInfo(), CPubdesc::GetTypeInfo(),
    CBioseq_set::GetTypeInfo(), CBioseq::GetTypeInfo(),    CSeqdesc::GetTypeInfo(),
};


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

    return entry;
}

//  ============================================================================
CAsnvalThreadState::CAsnvalThreadState(const CAppConfig& appConfig, SValidatorContext::taxupdate_func_t taxon) :
//  ============================================================================
    mAppConfig(appConfig)
{
    m_Options = appConfig.m_Options;
    m_pContext.reset(new SValidatorContext());
    m_ObjMgr = CObjectManager::GetInstance();
    m_pContext->m_taxon_update = taxon;
}

CAsnvalThreadState::~CAsnvalThreadState()
{
}

void CAsnvalThreadState::ReadClassMember
(CObjectIStream& in,
    const CObjectInfo::CMemberIterator& member)
{
    m_Level++;

    if (m_Level == 1) {
        size_t n = 0;
        // Read each element separately to a local TSeqEntry,
        // process it somehow, and... not store it in the container.
        for (CIStreamContainerIterator i(in, member); i; ++i) {
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
                    if (!mAppConfig.mQuiet) {
                        LOG_POST_XX(Corelib_App, 1, m_CurrentId);
                    }
                }

                if (mAppConfig.mDoCleanup) {
                    m_Cleanup.SetScope(scope);
                    m_Cleanup.BasicCleanup(*se);
                }

                if (mAppConfig.mOnlyAnnots) {
                    for (CSeq_annot_CI ni(seh); ni; ++ni) {
                        const CSeq_annot_Handle& sah = *ni;
                        CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
                        m_NumRecords++;
                        if (eval) {
                            PrintValidError(eval);
                        }
                    }
                }
                else {
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
                    if (eval) {
                        PrintValidError(eval);
                    }
                }
                scope->RemoveTopLevelSeqEntry(seh);
                scope->ResetHistory();
                n++;
            }
            catch (exception&) {
                if (!mAppConfig.mContinue) {
                    throw;
                }
                // should we issue some sort of warning?
            }
        }
    }
    else {
        in.ReadClassMember(member);
    }
    m_Level--;
}



//  ============================================================================
CRef<CScope> CAsnvalThreadState::BuildScope() const
//  ============================================================================
{
    CRef<CScope> scope(new CScope(*m_ObjMgr));
    scope->AddDefaults();
    return scope;
}


//  ============================================================================
unique_ptr<CObjectIStream> CAsnvalThreadState::OpenFile(TTypeInfo& asn_info, const string& fname) const
//  ============================================================================
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
        objectStream.reset(CObjectIStream::Open(format == CFormatGuess::eBinaryASN ? eSerial_AsnBinary : eSerial_AsnText, *InputStream, own));
        hold_stream.release();
        asn_info = contentInfo.mInfoGenbank.mTypeInfo;
        break;
    default:
        break;
    }
    return objectStream;
}

void CAsnvalThreadState::PrintValidError(CConstRef<CValidError> errors)
{
    if (errors)
        m_eval.push_back(errors);
}


CRef<CValidError> CAsnvalThreadState::ReportReadFailure(const CException* p_exception) const
{
    CRef<CValidError> errors(new CValidError());

    CNcbiOstrstream os;
    os << "Unable to read invalid ASN.1";

    const CSerialException* p_serial_exception = dynamic_cast<const CSerialException*>(p_exception);
    if (p_serial_exception && mAppConfig.mVerbosity != CAppConfig::eVerbosity_XML) {
        if (mpIstr) {
            os << ": " << mpIstr->GetPosition();
        }
        if (p_serial_exception->GetErrCode() == CSerialException::eEOF) {
            os << ": unexpected end of file";
        }
        else if (mAppConfig.mVerbosity != CAppConfig::eVerbosity_XML) {
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


void CAsnvalThreadState::ProcessSSMReleaseFile()
{
    CRef<CSeq_submit> submit(new CSeq_submit);

    auto hook = [this](CObjectIStream& in, const CObjectInfo::CMemberIterator& member) {ReadClassMember(in, member); };
    CObjectTypeInfo info = CType<CBioseq_set>();
    SetLocalReadHook(info.FindMember("seq-set"), *mpIstr, hook);

    // Read the CSeq_submit, it will call the hook object each time we
    // encounter a Seq-entry
    try {
        *mpIstr >> *submit;
    }
    catch (const CException&) {
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is not a batch Seq-submit, do not use -a u to process.");
        ++m_Reported;
    }
}


void CAsnvalThreadState::ProcessBSSReleaseFile()
{
    CRef<CBioseq_set> seqset(new CBioseq_set);

    // Register the Seq-entry hook
    auto hook = [this](CObjectIStream& in, const CObjectInfo::CMemberIterator& member) {ReadClassMember(in, member); };
    CObjectTypeInfo info = CType<CBioseq_set>();
    SetLocalReadHook(info.FindMember("seq-set"), *mpIstr, hook);


    // Read the CBioseq_set, it will call the hook object each time we
    // encounter a Seq-entry
    try {
        *mpIstr >> *seqset;
    }
    catch (const CException&) {
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is not a batch Bioseq-set, do not use -a t to process.");
        ++m_Reported;
    }
}


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqEntry(CSeq_entry& se)
{
    // Validate Seq-entry
    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    if (mAppConfig.mDoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(se);
    }
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(se);
    CBioseq_CI bi(seh);
    if (bi) {
        m_CurrentId = "";
        bi->GetId().front().GetSeqId()->GetLabel(&m_CurrentId);
        if (!mAppConfig.mQuiet) {
            LOG_POST_XX(Corelib_App, 1, m_CurrentId);
        }
    }

    if (mAppConfig.mOnlyAnnots) {
        for (CSeq_annot_CI ni(seh); ni; ++ni) {
            const CSeq_annot_Handle& sah = *ni;
            CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
            m_NumRecords++;
            if (eval) {
                PrintValidError(eval);
            }
        }
        return CConstRef<CValidError>();
    }
    CConstRef<CValidError> eval = validator.Validate(se, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqDesc(CRef<CSerialObject> serial)
{
    CRef<CSeqdesc> sd(CTypeConverter<CSeqdesc>::SafeCast(serial));

    CRef<CSeq_entry> ctx = s_BuildGoodSeq();

    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*ctx);

    CConstRef<CValidError> eval = validator.Validate(*sd, *ctx);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ProcessBioseq(CRef<CSerialObject> serial)
{
    // Get seq-entry to validate
    CRef<CBioseq> bioseq(CTypeConverter<CBioseq>::SafeCast(serial));

    auto se = Ref(new CSeq_entry);
    se->SetSeq(*bioseq);

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}

CConstRef<CValidError> CAsnvalThreadState::ProcessBioseqset(CRef<CSerialObject> serial)
{
    // Get seq-entry to validate
    CRef<CBioseq_set> bioseqset(CTypeConverter<CBioseq_set>::SafeCast(serial));

    auto se = Ref(new CSeq_entry);
    se->SetSet(*bioseqset);

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}


CConstRef<CValidError> CAsnvalThreadState::ProcessPubdesc(CRef<CSerialObject> serial)
{
    CRef<CPubdesc> pd(CTypeConverter<CPubdesc>::SafeCast(serial));

    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    m_NumRecords++;
    return validator.Validate(*pd, scope, m_Options);
}


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqEntry(CRef<CSerialObject> serial)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(CTypeConverter<CSeq_entry>::SafeCast(serial));

    return ProcessSeqEntry(*se);
}


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqSubmit(CRef<CSerialObject> serial)
{
    CConstRef<CValidError> eval;

    CRef<CSeq_submit> ss(CTypeConverter<CSeq_submit>::SafeCast(serial));

    m_CurrentId = "";
    // Validate Seq-submit
    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        for (auto& se: ss->SetData().SetEntrys() ) {
            auto teh = scope->AddTopLevelSeqEntry(*se);
            if (teh) {
                CBioseq_CI bi(teh);
                if (bi) {
                    bi->GetId().front().GetSeqId()->GetLabel(&m_CurrentId);
                }
            }
        }
    }
    if (mAppConfig.mDoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(*ss);
    }

    if (!mAppConfig.mQuiet) {
        LOG_POST_XX(Corelib_App, 1, m_CurrentId);
    }
    eval = validator.Validate(*ss, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqAnnot(CRef<CSerialObject> serial)
{
    CRef<CSeq_annot> sa(CTypeConverter<CSeq_annot>::SafeCast(serial));

    // Validate Seq-annot
    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    if (mAppConfig.mDoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(*sa);
    }
    CSeq_annot_Handle sah = scope->AddSeq_annot(*sa);
    CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqFeat(CRef<CSerialObject> serial)
{
    CRef<CSeq_feat> feat(CTypeConverter<CSeq_feat>::SafeCast(serial));

    CRef<CScope> scope = BuildScope();
    if (mAppConfig.mDoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(*feat);
    }

    CValidator validator(*m_ObjMgr, m_pContext);
    m_NumRecords++;
    return validator.Validate(*feat, scope, m_Options);
}


CConstRef<CValidError> CAsnvalThreadState::ProcessBioSource(CRef<CSerialObject> serial)
{
    CRef<CBioSource> src(CTypeConverter<CBioSource>::SafeCast(serial));

    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    m_NumRecords++;
    return validator.Validate(*src, scope, m_Options);
}


CConstRef<CValidError> CAsnvalThreadState::ValidateInput(TTypeInfo asninfo)
{
    // Process file based on its content
    if (asninfo==nullptr) {
        auto content = mpIstr->GuessDataType(s_known_types);
        if (content.size() == 1) {
            asninfo = *content.begin();
        }
    }
    if (asninfo==nullptr) {
        NCBI_THROW(CException, eUnknown, "Unrecognized data type");
    }

    CRef<CSerialObject> serial;
    try
    {
        auto obj_info = mpIstr->Read(asninfo);
        serial.Reset(static_cast<CSerialObject*>(obj_info.GetObjectPtr()));
    }
    catch (CException& e) {
        if (mAppConfig.mVerbosity != CAppConfig::eVerbosity_XML) {
            ERR_POST(Error << e);
        }
        return ReportReadFailure(&e);
    }

    string asn_type = asninfo->GetName();

    try
    {
        if (asn_type == "Seq-submit") {                 // Seq-submit
            return ProcessSeqSubmit(serial);
        }
        if (asn_type == "Seq-entry") {           // Seq-entry
            return ProcessSeqEntry(serial);
        }
        if (asn_type == "Seq-annot") {           // Seq-annot
            return ProcessSeqAnnot(serial);
        }
        if (asn_type == "Seq-feat") {            // Seq-feat
            return ProcessSeqFeat(serial);
        }
        if (asn_type == "BioSource") {           // BioSource
            return ProcessBioSource(serial);
        }
        if (asn_type == "Pubdesc") {             // Pubdesc
            return ProcessPubdesc(serial);
        }
        if (asn_type == "Bioseq-set") {          // Bioseq-set
            return ProcessBioseqset(serial);
        }
        if (asn_type == "Bioseq") {              // Bioseq
            return ProcessBioseq(serial);
        }
        if (asn_type == "Seqdesc") {             // Seq-desc
            return ProcessSeqDesc(serial);
        }
    }
    catch(const CException& e)
    {
        if (NStr::StartsWith(e.GetMsg(), "duplicate Bioseq id", NStr::eNocase)) {
            auto errors = Ref(new CValidError);
            string errstr = e.GetMsg();
            errstr = NStr::Replace(errstr, "\n", " * ");
            errstr = NStr::Replace(errstr, " *   ", " * ");
            errors->AddValidErrItem(eDiag_Critical, eErr_GENERIC_DuplicateIDs, errstr);
            ++m_Reported;
            return errors;
            //PrintValidError(errors);
        }
        throw;
    }

    NCBI_THROW(CException, eUnknown, "Unhandled type " + asn_type);
}


CConstRef<CValidError> CAsnvalThreadState::ValidateAsync(
    const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid) const
{
    CRef<CSeq_entry> pEntry;
    CRef<CScope> scope = BuildScope();
    if (!loader_name.empty())
        scope->AddDataLoader(loader_name);

    CConstRef<CValidError> eval;

    CValidator validator(*m_ObjMgr, m_pContext);

    CSeq_entry_Handle top_h;
    auto seq_id_h = CSeq_id_Handle::GetHandle(*seqid);
    if (scope->Exists(seq_id_h)) {
        if (auto bioseq_h = scope->GetBioseqHandle(seq_id_h); bioseq_h) {
            top_h = bioseq_h.GetTopLevelEntry();
            if (top_h) {
                pEntry = Ref(const_cast<CSeq_entry*>(top_h.GetCompleteSeq_entry().GetPointer()));
            }
        }
    }

    if (top_h) {

        if (mAppConfig.mDoCleanup) {
            CCleanup cleanup;
            cleanup.SetScope(scope);
            cleanup.BasicCleanup(*pEntry);
        }

        if (pSubmitBlock) {
            auto pSubmit = Ref(new CSeq_submit());
            pSubmit->SetSub().Assign(*pSubmitBlock);
            pSubmit->SetData().SetEntrys().push_back(pEntry);
            eval = validator.Validate(*pSubmit, scope, m_Options);
        }
        else {
            eval = validator.Validate(*pEntry, scope, m_Options);
        }
    }

    return eval;
}

void CAsnvalThreadState::ValidateOneHugeBlob(edit::CHugeFileProcess& process)
{
    string loader_name = CDirEntry::CreateAbsolutePath(process.GetFile().m_filename);
    bool use_mt = true;
    #ifdef _DEBUG
        use_mt = false;
    #endif

    auto& reader = process.GetReader();

    auto info = edit::CHugeAsnDataLoader::RegisterInObjectManager(
        *m_ObjMgr, loader_name, &reader, CObjectManager::eNonDefault, 1); //CObjectManager::kPriority_Local);

    CAutoRevoker autorevoker(info);
    CHugeFileValidator hugeFileValidator(reader, m_Options);
    hugeFileValidator.UpdateValidatorContext(m_GlobalInfo, *m_pContext);

    if (!mAppConfig.mQuiet) {
        if (const auto& topIds = reader.GetTopIds(); !topIds.empty()) {
            m_CurrentId.clear();
            topIds.front()->GetLabel(&m_CurrentId);
            LOG_POST_XX(Corelib_App, 1, m_CurrentId);
        }
    }

    if (m_pContext->PreprocessHugeFile) {
        CRef<CValidError> pEval;
        hugeFileValidator.ReportGlobalErrors(m_GlobalInfo, pEval);
        if (pEval) {
            PrintValidError(pEval);
        }
    }

    if (use_mt) {
        ValidateBlobAsync(loader_name, process);
    } else {
        ValidateBlobSequential(loader_name, process);
    }

    CRef<CValidError> pEval;
    hugeFileValidator.ReportPostErrors(m_GlobalInfo, pEval, *m_pContext);
    if (pEval) {
        PrintValidError(pEval);
    }
}

void CAsnvalThreadState::ValidateOneHugeFile(edit::CHugeFileProcess& process)
{
    while (true)
    {
        m_GlobalInfo.Clear();

        try {
            if (!process.ReadNextBlob())
                break;

        }
        catch (const edit::CHugeFileException& e) {
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
        catch (const CException& e) {
            auto errors = ReportReadFailure(&e);
            PrintValidError(errors);
            return;
        }

        m_NumRecords++;
        ValidateOneHugeBlob(process);
    }
}

CThreadExitData CAsnvalThreadState::ValidateWorker(
    CAsnvalThreadState* _this,
    const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid)
{
    CThreadExitData exit_data;
    try
    {
        CStopWatch sw(CStopWatch::eStart);
        exit_data.mEval.push_back(_this->ValidateAsync(loader_name, pSubmitBlock, seqid));
        double elapsed = sw.Elapsed();
        exit_data.mLongest = elapsed;
    }
    catch (const CException& e) {
        string errstr = e.GetMsg();
        errstr = NStr::Replace(errstr, "\n", " * ");
        errstr = NStr::Replace(errstr, " *   ", " * ");
        CRef<CValidError> eval(new CValidError());
        eval->AddValidErrItem(eDiag_Fatal, eErr_INTERNAL_Exception, errstr);
        ERR_POST(e);
        ++_this->m_Reported;
        exit_data.mEval.push_back(eval);
    }
    return exit_data;
}


void CAsnvalThreadState::ValidateBlobAsync(const string& loader_name, edit::CHugeFileProcess& process)
{
    auto& reader = process.GetReader();

    CMessageQueue<std::future<CThreadExitData>> val_queue{ 10 };
    // start a loop in a separate thread
    auto topids_task = std::async(std::launch::async, [this, &val_queue, &loader_name, &reader]()
        {
            auto pSubmitBlock = reader.GetSubmitBlock();
            for (auto seqid : reader.GetTopIds())
            {
                auto fut = std::async(std::launch::async, ValidateWorker, this, loader_name, pSubmitBlock, seqid);
                // std::future is not copiable, so passing it for move constructor
                val_queue.push_back(std::move(fut));
            }

            val_queue.push_back({});
        });

    while (true)
    {
        auto result = val_queue.pop_front();
        if (!result.valid())
            break;
        auto exit_data = result.get();
        if (!exit_data.mEval.empty())
            PrintValidError(exit_data.mEval.front());

        if (exit_data.mLongest > m_Longest) {
            m_Longest = exit_data.mLongest;
            m_LongestId = m_CurrentId; //exit_data.mLongestId;
        }

    }

    topids_task.wait();
}

void CAsnvalThreadState::ValidateBlobSequential(const string& loader_name, edit::CHugeFileProcess& process)
{
    auto& reader = process.GetReader();

    for (auto seqid : reader.GetTopIds())
    {
        auto pSubmitBlock = reader.GetSubmitBlock();
        CConstRef<CValidError> eval;
        eval = ValidateAsync(loader_name, pSubmitBlock, seqid);
        PrintValidError(eval);
    }
}

bool CAsnvalThreadState::ValidateTraditionally(TTypeInfo asninfo)
{
    CStopWatch sw(CStopWatch::eStart);

    CConstRef<CValidError> eval = ValidateInput(asninfo);

    double elapsed = sw.Elapsed();
    if (elapsed > m_Longest) {
        m_Longest = elapsed;
        m_LongestId = m_CurrentId;
    }

    if (eval) {
        PrintValidError(eval);
        if (eval->IsCatastrophic()) {
            return false;
        }
    }
    return true;
}

bool CAsnvalThreadState::ValidateBatchMode(TTypeInfo asninfo)
{
    if (asninfo == CBioseq_set::GetTypeInfo()) {
        ProcessBSSReleaseFile();
        return true;
    }
    else
    if (asninfo == CSeq_submit::GetTypeInfo()) {
        const auto commandLineOptions = m_Options;
        m_Options |= CValidator::eVal_seqsubmit_parent;
        try {
            ProcessSSMReleaseFile();
        }
        catch (CException&) {
            m_Options = commandLineOptions;
            throw;
        }
        m_Options = commandLineOptions;
        return true;
    }
    else {
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is neither a Seq-submit nor Bioseq-set; do not use -batch to process.");
        return false;
    }
}

CThreadExitData CAsnvalThreadState::ValidateOneFile(const std::string& filename)
{
    if (!mAppConfig.mQuiet) {
        LOG_POST_XX(Corelib_App, 1, filename);
    }

    TTypeInfo asninfo = nullptr;
    unique_ptr<edit::CHugeFileProcess> mpHugeFileProcess;

    if (filename.empty())
        mpIstr = OpenFile(asninfo, filename);
    else {
        mpHugeFileProcess.reset(new edit::CHugeFileProcess(new CValidatorHugeAsnReader(m_GlobalInfo)));
        try {
            mpHugeFileProcess->Open(filename, &s_known_types);
            asninfo = mpHugeFileProcess->GetFile().m_content;
        }
        catch (const CObjReaderParseException&) {
            mpHugeFileProcess.reset();
            throw;
        }

        if (asninfo) {
            if (!mAppConfig.mHugeFile || mAppConfig.mBatch || !edit::CHugeFileProcess::IsSupported(asninfo)) {
                mpIstr = mpHugeFileProcess->GetReader().MakeObjStream(0);
            }
        }
        else {
            mpIstr = OpenFile(asninfo, filename);
        }
    }

    if (mAppConfig.mHugeFile && !mpIstr) {
        ValidateOneHugeFile(*mpHugeFileProcess);
    } else {

        bool proceed = true;

        do {
            if (!asninfo) {
                PrintValidError(ReportReadFailure(nullptr));
                LOG_POST_XX(Corelib_App, 1, "FAILURE: Unable to process invalid ASN.1 file " + filename);
                break;
            }

            try {
                if (mAppConfig.mBatch) {
                    if (!ValidateBatchMode(asninfo))
                        proceed = ValidateTraditionally(asninfo);
                }
                else
                    proceed = ValidateTraditionally(asninfo);

                if (mpIstr->EndOfData()) // force to SkipWhiteSpace
                    break;
                else {
                    auto types = mpIstr->GuessDataType(s_known_types);
                    asninfo = types.empty() ? nullptr : *types.begin();
                }
            }
            catch (const CException& e) {
                string errstr = e.GetMsg();
                errstr = NStr::Replace(errstr, "\n", " * ");
                errstr = NStr::Replace(errstr, " *   ", " * ");
                CRef<CValidError> eval(new CValidError());
                eval->AddValidErrItem(eDiag_Fatal, eErr_INTERNAL_Exception, errstr);
                PrintValidError(eval);
                ++m_Reported;
                ERR_POST(e);
            }
        }
        while (proceed);
    }

    mpIstr.reset();

    return { m_NumRecords, m_Longest, m_LongestId, m_Reported, m_eval };
}

void CAsnvalOutput::FinishXML()
{
    if (m_ostr_xml)
    {
        m_ostr_xml.reset();
        *m_file << "</asnvalidate>" << "\n";
    }
}

CAsnvalOutput::CAsnvalOutput(const CAppConfig& config, const string& in_filename)
: mAppConfig{config}
{
    string path;
    if (in_filename.empty()) {
        path = "stdin.val";
    }
    else {
        size_t pos = NStr::Find(in_filename, ".", NStr::eNocase, NStr::eReverseSearch);
        if (pos != NPOS)
            path = in_filename.substr(0, pos);
        else
            path = in_filename;

        path.append(".val");
    }

    m_own_file.reset(new ofstream(path));
    m_file = m_own_file.get();

    StartXML();
}

CAsnvalOutput::CAsnvalOutput(const CAppConfig& config, std::ostream* file)
: mAppConfig{config},
  m_file{file}
{
    StartXML();
}

CAsnvalOutput::~CAsnvalOutput()
{
    FinishXML();
}

size_t CAsnvalOutput::Write(const std::list<CConstRef<CValidError>>& eval)
{
    size_t reported = 0;
    for (auto errors: eval) {
        if (errors.Empty() || errors->TotalSize() == 0) {
            continue;
        }

        for (CValidError_CI vit(*errors); vit; ++vit) {
            if (vit->GetSeverity() >= mAppConfig.mReportLevel) {
                ++reported;
            }
            if (vit->GetSeverity() < mAppConfig.mLowCutoff || vit->GetSeverity() > mAppConfig.mHighCutoff) {
                continue;
            }
            if (!mAppConfig.mOnlyError.empty() && !(NStr::EqualNocase(mAppConfig.mOnlyError, vit->GetErrCode()))) {
                continue;
            }
            PrintValidErrItem(*vit);
        }
    }
    return reported;
}

void CAsnvalOutput::PrintValidErrItem(const CValidErrItem& item)
{
    CNcbiOstream& os = *m_file;
    switch (mAppConfig.mVerbosity) {
        case CAppConfig::eVerbosity_Normal:
        os << s_GetSeverityLabel(item.GetSeverity())
            << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() << "] "
            << item.GetMsg();
        if (item.IsSetObjDesc()) {
            os << " " << item.GetObjDesc();
        }
        os << "\n";
        break;
    case CAppConfig::eVerbosity_Spaced:
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
    case CAppConfig::eVerbosity_Tabbed:
        os << item.GetAccnver() << "\t"
            << s_GetSeverityLabel(item.GetSeverity()) << "\t"
            << item.GetErrGroup() << "_" << item.GetErrCode() << "\n";
        break;
    case CAppConfig::eVerbosity_XML:
    {
        m_ostr_xml->Print(item);
    }
    break;
    }
}

void CAsnvalOutput::StartXML()
{
    if (!m_file  ||  mAppConfig.mVerbosity != CAppConfig::eVerbosity_XML) {
        return;
    }
    m_ostr_xml.reset(new CValXMLStream(*m_file, eNoOwnership));
    m_ostr_xml->SetEncoding(eEncoding_UTF8);
    m_ostr_xml->SetReferenceDTD(false);
    m_ostr_xml->SetEnforcedStdXml(true);
    m_ostr_xml->WriteFileHeader(CValidErrItem::GetTypeInfo());
    m_ostr_xml->SetUseIndentation(true);
    m_ostr_xml->Flush();

    *m_file << "\n" << "<asnvalidate version=\"" << "3." << NCBI_SC_VERSION_PROXY << "."
        << NCBI_TEAMCITY_BUILD_NUMBER_PROXY << "\" severity_cutoff=\""
        << s_GetSeverityLabel(mAppConfig.mLowCutoff, true) << "\">" << "\n";
}
