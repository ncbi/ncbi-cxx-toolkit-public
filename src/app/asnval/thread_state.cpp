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
*Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko
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
#include "thread_state.hpp"
#include <objtools/validator/huge_file_validator.hpp>

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
};


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

CAppConfig::CAppConfig(const CArgs& args)
{
    mQuiet = args["quiet"] && args["quiet"].AsBoolean();
    mDoCleanup = args["cleanup"] && args["cleanup"].AsBoolean();
    mVerbosity = static_cast<CAppConfig::EVerbosity>(args["v"].AsInteger());
    mLowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    mHighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);
    mReportLevel = static_cast<EDiagSev>(args["R"].AsInteger() - 1);

    mBatch = args["batch"];
    string objectType = args["a"].AsString();
    if (!objectType.empty()) {
        if (objectType == "t" || objectType == "u") {
            mBatch = true;
            cerr << "Warning: -a t and -a u are deprecated; use -batch instead." << endl;
        }
        else {
            cerr << "Warning: -a is deprecated; ASN.1 type is now autodetected." << endl;
        }
    }

    if (args["E"]) {
        mOnlyError = args["E"].AsString();
    }
    mOnlyAnnots = args["annot"];
    mHugeFile = args["huge"];
}

//  ============================================================================
CAsnvalThreadState::CAsnvalThreadState(const CAppConfig& appConfig) :
//  ============================================================================
    mAppConfig(appConfig),
    m_ObjMgr(),
    m_Options(0),
    m_Longest(0.0),
    m_NumFiles(0),
    m_NumRecords(0),
    m_Level(0),
    m_Reported(0),
    m_ValidErrorStream(nullptr)
{
    m_pContext.reset(new SValidatorContext());
};

//  ============================================================================
CAsnvalThreadState::CAsnvalThreadState(
    CAsnvalThreadState& other):
//  ============================================================================
    mAppConfig(other.mAppConfig)
{
    mFilename = other.mFilename;
    mpIstr.reset();
    mpHugeFileProcess.reset();
    m_ObjMgr = other.m_ObjMgr;
    m_Options = other.m_Options;
    m_Longest = other.m_Longest;
    m_CurrentId = other.m_CurrentId;
    m_LongestId = other.m_LongestId;
    m_NumFiles = other.m_NumFiles;
    m_NumRecords = other.m_NumRecords;

    m_Level = other.m_Level;
    m_Reported = 0;

    m_ValidErrorStream = other.m_ValidErrorStream;

    m_pContext.reset(new SValidatorContext());
    m_pContext->m_taxon_update = other.m_pContext->m_taxon_update;
#ifdef USE_XMLWRAPP_LIBS
    m_ostr_xml.reset();
#endif
};


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
CRef<CScope> CAsnvalThreadState::BuildScope()
//  ============================================================================
{
    CRef<CScope> scope(new CScope(*m_ObjMgr));
    scope->AddDefaults();
    return scope;
}


//  ============================================================================
unique_ptr<CObjectIStream> CAsnvalThreadState::OpenFile(TTypeInfo& asn_info)
//  ============================================================================
{
    ENcbiOwnership own = eNoOwnership;
    unique_ptr<CNcbiIstream> hold_stream;
    CNcbiIstream* InputStream = &NcbiCin;
    auto& fname = mFilename;

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


//  ============================================================================
void CAsnvalThreadState::DestroyOutputStreams()
//  ============================================================================
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


//  ============================================================================
void CAsnvalThreadState::ConstructOutputStreams()
//  ============================================================================
{
    if (!m_ValidErrorStream  ||  mAppConfig.mVerbosity != CAppConfig::eVerbosity_XML) {
        return;
    }
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
        << s_GetSeverityLabel(mAppConfig.mLowCutoff, true) << "\">" << "\n";
    m_ValidErrorStream->flush();
#else
    * context.m_ValidErrorStream << "<asnvalidate version=\"" << "3." << NCBI_SC_VERSION_PROXY << "."
        << NCBI_TEAMCITY_BUILD_NUMBER_PROXY << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << "\n";
#endif
}


void CAsnvalThreadState::PrintValidErrItem(const CValidErrItem& item)
{
    CNcbiOstream& os = *m_ValidErrorStream;
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
#ifdef USE_XMLWRAPP_LIBS
    case CAppConfig::eVerbosity_XML:
    {
        m_ostr_xml->Print(item);
    }
#else
    case CAppConfig::eVerbosity_XML:
    {
        string msg = NStr::XmlEncode(item.GetMsg());
        if (item.IsSetFeatureId()) {
            os << "  <message severity=\"" << s_GetSeverityLabel(item.GetSeverity())
                << "\" seq-id=\"" << item.GetAccnver()
                << "\" feat-id=\"" << item.GetFeatureId()
                << "\" code=\"" << item.GetErrGroup() << "_" << item.GetErrCode()
                << "\">" << msg << "</message>" << "\n";
        }
        else {
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


void CAsnvalThreadState::PrintValidError(
    CConstRef<CValidError> errors)
{
    if (errors.Empty() || errors->TotalSize() == 0) {
        return;
    }

    for (CValidError_CI vit(*errors); vit; ++vit) {
        if (vit->GetSeverity() >= mAppConfig.mReportLevel) {
            ++m_Reported;
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


CRef<CValidError> CAsnvalThreadState::ReportReadFailure(const CException* p_exception)
{
    CRef<CValidError> errors(new CValidError());

    CNcbiOstrstream os;
    os << "Unable to read invalid ASN.1";

    const CSerialException* p_serial_exception = dynamic_cast<const CSerialException*>(p_exception);
    if (p_serial_exception) {
        if (mpIstr) {
            os << ": " << mpIstr->GetPosition();
        }
        if (p_serial_exception->GetErrCode() == CSerialException::eEOF) {
            os << ": unexpected end of file";
        }
        else {
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


CConstRef<CValidError> CAsnvalThreadState::ReadAny(CRef<CBioseq>& obj)
{
    try {
        if (!obj)
            obj.Reset(new CBioseq);
        mpIstr->Read(obj, CBioseq::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        return {};
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }
}

CConstRef<CValidError> CAsnvalThreadState::ReadAny(CRef<CBioseq_set>& obj)
{
    try {
        if (!obj)
            obj.Reset(new CBioseq_set);
        mpIstr->Read(obj, CBioseq_set::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        return {};
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }
}


CConstRef<CValidError> CAsnvalThreadState::ReadAny(CRef<CSeq_entry>& obj)
{
    try {
        if (!obj)
            obj.Reset(new CSeq_entry);
        mpIstr->Read(obj, CSeq_entry::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        return {};
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }
}


CConstRef<CValidError> CAsnvalThreadState::ReadAny(CRef<CSeq_feat>& obj)
{
    try {
        if (!obj)
            obj.Reset(new CSeq_feat);
        mpIstr->Read(obj, CSeq_feat::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        return {};
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }
}


CConstRef<CValidError> CAsnvalThreadState::ReadAny(CRef<CBioSource>& obj)
{
    try {
        if (!obj)
            obj.Reset(new CBioSource);
        mpIstr->Read(obj, CBioSource::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        return {};
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }
}


CConstRef<CValidError> CAsnvalThreadState::ReadAny(CRef<CPubdesc>& obj)
{
    try {
        if (!obj)
            obj.Reset(new CPubdesc);
        mpIstr->Read(obj, CPubdesc::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        return {};
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }
}


CConstRef<CValidError> CAsnvalThreadState::ReadAny(CRef<CSeq_submit>& obj)
{
    try {
        if (!obj)
            obj.Reset(new CSeq_submit);
        mpIstr->Read(obj, CSeq_submit::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        return {};
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }
}


void CAsnvalThreadState::ProcessSSMReleaseFile()
{
    CRef<CSeq_submit> seqset(new CSeq_submit);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CSeq_submit>();
    (*(*(*set_type.FindMember("data")).GetPointedType().FindVariant("entrys")).GetElementType().GetPointedType().FindVariant("set")).FindMember("seq-set").SetLocalReadHook(
        *mpIstr, this);

    // Read the CSeq_submit, it will call the hook object each time we
    // encounter a Seq-entry
    try {
        *mpIstr >> *seqset;
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
    CObjectTypeInfo set_type = CType<CBioseq_set>();
    set_type.FindMember("seq-set").SetLocalReadHook(*mpIstr, this);

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


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqDesc()
{
    CRef<CSeqdesc> sd(new CSeqdesc);

    try {
        mpIstr->Read(ObjectInfo(*sd), CObjectIStream::eNoFileHeader);
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


CConstRef<CValidError> CAsnvalThreadState::ProcessBioseq()
{
    // Get seq-entry to validate
    CRef<CBioseq> bioseq;

    auto eval = ReadAny(bioseq);
    if (eval)
        return eval;

    auto se = Ref(new CSeq_entry);
    se->SetSeq(*bioseq);

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}

CConstRef<CValidError> CAsnvalThreadState::ProcessBioseqset()
{
    // Get seq-entry to validate
    CRef<CBioseq_set> bioseqset;

    auto eval = ReadAny(bioseqset);
    if (eval)
        return eval;

    auto se = Ref(new CSeq_entry);
    se->SetSet(*bioseqset);

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}


CConstRef<CValidError> CAsnvalThreadState::ProcessPubdesc()
{
    CRef<CPubdesc> pd;
    auto eval = ReadAny(pd);
    if (eval)
        return eval;

    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    eval = validator.Validate(*pd, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqEntry()
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se;
    auto eval = ReadAny(se);
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


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqSubmit()
{

    CRef<CSeq_submit> ss;
    auto eval = ReadAny(ss);
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
    if (mAppConfig.mDoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(*ss);
    }

    eval = validator.Validate(*ss, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqAnnot()
{
    CRef<CSeq_annot> sa(new CSeq_annot);

    // Get seq-annot to validate
    try {
        mpIstr->Read(ObjectInfo(*sa), CObjectIStream::eNoFileHeader);
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

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


CConstRef<CValidError> CAsnvalThreadState::ProcessSeqFeat()
{
    CRef<CSeq_feat> feat;
    auto eval = ReadAny(feat);
    if (eval)
        return eval;

    CRef<CScope> scope = BuildScope();
    if (mAppConfig.mDoCleanup) {
        m_Cleanup.SetScope(scope);
        m_Cleanup.BasicCleanup(*feat);
    }

    CValidator validator(*m_ObjMgr, m_pContext);
    eval = validator.Validate(*feat, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ProcessBioSource()
{
    CRef<CBioSource> src;
    auto eval = ReadAny(src);
    if (eval)
        return eval;

    CValidator validator(*m_ObjMgr, m_pContext);
    CRef<CScope> scope = BuildScope();
    eval = validator.Validate(*src, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ValidateInput(TTypeInfo asninfo)
{
    // Process file based on its content
    CConstRef<CValidError> eval;

    string asn_type = mpIstr->ReadFileHeader();
    if (asninfo)
        asn_type = asninfo->GetName();

    if (asn_type == "Seq-submit") {                 // Seq-submit
        eval = ProcessSeqSubmit();
    }
    else if (asn_type == "Seq-entry") {           // Seq-entry
        eval = ProcessSeqEntry();
    }
    else if (asn_type == "Seq-annot") {           // Seq-annot
        eval = ProcessSeqAnnot();
    }
    else if (asn_type == "Seq-feat") {            // Seq-feat
        eval = ProcessSeqFeat();
    }
    else if (asn_type == "BioSource") {           // BioSource
        eval = ProcessBioSource();
    }
    else if (asn_type == "Pubdesc") {             // Pubdesc
        eval = ProcessPubdesc();
    }
    else if (asn_type == "Bioseq-set") {          // Bioseq-set
        eval = ProcessBioseqset();
    }
    else if (asn_type == "Bioseq") {              // Bioseq
        eval = ProcessBioseq();
    }
    else if (asn_type == "Seqdesc") {             // Seq-desc
        eval = ProcessSeqDesc();
    }
    else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + asn_type);
    }

    return eval;
}


CConstRef<CValidError> CAsnvalThreadState::ValidateAsync(
    const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid, CRef<CSeq_entry> pEntry)
{
    CRef<CScope> scope = BuildScope();
    if (!loader_name.empty())
        scope->AddDataLoader(loader_name);

    CConstRef<CValidError> eval;

    CValidator validator(*m_ObjMgr, m_pContext);

    CSeq_entry_Handle top_h;
    if (pEntry) {
        top_h = scope->AddTopLevelSeqEntry(*pEntry);
    }
    else {
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

    scope->ResetDataAndHistory();
    return eval;
}


void CAsnvalThreadState::ValidateOneHugeFile(const string& loader_name, bool use_mt)
{
    auto& reader = mpHugeFileProcess->GetReader();

    while (true)
    {
        m_GlobalInfo.Clear();
        try {
            if (!reader.GetNextBlob())
                break;
        }
        catch (const CException& e) {
            auto errors = ReportReadFailure(&e);
            PrintValidError(errors);
            return;
        }
        m_NumRecords++;

        try {
            reader.FlattenGenbankSet();
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
            CMessageQueue<std::future<CConstRef<CValidError>>> val_queue{ 10 };
            // start a loop in a separate thread
            auto topids_task = std::async(std::launch::async, [this, &val_queue, &loader_name, &reader]()
                {
                    auto pSubmitBlock = reader.GetSubmitBlock();
                    for (auto seqid : reader.GetTopIds())
                    {
                        auto params = std::async(std::launch::async,
                            [this, seqid, &loader_name, pSubmitBlock]() -> CConstRef<CValidError>
                            {
                                try
                                {
                                    return ValidateAsync(loader_name, pSubmitBlock, seqid, {});
                                }
                                catch (const CException& e) {
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
                        val_queue.push_back(std::move(params));
                    }

                    val_queue.push_back({});
                });

            while (true)
            {
                auto result = val_queue.pop_front();
                if (!result.valid())
                    break;
                auto eval = result.get();
                PrintValidError(eval);
            }

            topids_task.wait();

        }
        else {
            for (auto seqid : reader.GetTopIds())
            {
                auto pSubmitBlock = reader.GetSubmitBlock();
                CConstRef<CValidError> eval;
#ifdef _DEBUG1
                cerr << MSerial_AsnText << "Validating: " << seqid->AsFastaString() << "\n";
#endif
                auto seqid_str = seqid->AsFastaString();
                eval = ValidateAsync(loader_name, pSubmitBlock, seqid, {});
                PrintValidError(eval);
            }
        }
    }
}


void CAsnvalThreadState::ValidateOneFile()
{
    if (!mAppConfig.mQuiet) {
        LOG_POST_XX(Corelib_App, 1, mFilename);
    }

    unique_ptr<CNcbiOfstream> local_stream;
    //DebugBreak();
    bool close_error_stream = false;
    try {
        if (!m_ValidErrorStream) {
            string path;
            if (mFilename.empty()) {
                path = "stdin.val";
            }
            else {
                size_t pos = NStr::Find(mFilename, ".", NStr::eNocase, NStr::eReverseSearch);
                if (pos != NPOS)
                    path = mFilename.substr(0, pos);
                else
                    path = mFilename;

                path.append(".val");
            }

            local_stream.reset(new CNcbiOfstream(path));
            m_ValidErrorStream = local_stream.get();

            ConstructOutputStreams();
            close_error_stream = true;
        }
    }
    catch (CException) {
    }

    TTypeInfo asninfo = nullptr;
    //DebugBreak();
    if (mFilename.empty())// || true)
        mpIstr = OpenFile(asninfo);
    else {
        mpHugeFileProcess.reset(new edit::CHugeFileProcess);
        try {
            mpHugeFileProcess->Open(mFilename, &s_known_types);
            asninfo = mpHugeFileProcess->GetFile().RecognizeContent(0);
        }
        catch (const CObjReaderParseException&) {
            mpHugeFileProcess.reset();
        }

        if (asninfo) {
            if (!mAppConfig.mHugeFile || mAppConfig.mBatch || !edit::CHugeFileProcess::IsSupported(asninfo)) {
                mpIstr = mpHugeFileProcess->GetReader().MakeObjStream(0);
            }
        }
        else {
            mpIstr = OpenFile(asninfo);
        }

    }

    if (!asninfo) {
        PrintValidError(ReportReadFailure(nullptr));
        if (close_error_stream) {
            DestroyOutputStreams();
        }
        // NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Unable to open invalid ASN.1 file " + mFilename);
    }

    //cerr << "CAsnvalApp::ValidateOneFile() check3 " << context.mFilename << "\n";
    if (asninfo) {
        try {
            if (mAppConfig.mBatch) {
                if (asninfo == CBioseq_set::GetTypeInfo()) {
                    ProcessBSSReleaseFile();
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

                    }
                    else {
                        LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is neither a Seq-submit nor Bioseq-set; do not use -batch to process.");
                    }
            }
            else {
                size_t num_validated = 0;
                bool doloop = true;
                while (doloop) {
                    CStopWatch sw(CStopWatch::eStart);
                    try {
                        if (mpIstr) {
                            CConstRef<CValidError> eval = ValidateInput(asninfo);
                            if (eval)
                                PrintValidError(eval);

                            if (!mpIstr->EndOfData()) { // force to SkipWhiteSpace
                                try {
                                    auto types = mpIstr->GuessDataType(s_known_types);
                                    asninfo = types.empty() ? nullptr : *types.begin();
                                }
                                catch (const CException& e) {
                                    eval = ReportReadFailure(&e);
                                    if (eval) {
                                        PrintValidError(eval);
                                        doloop = false;
                                    }
                                    else
                                        throw;
                                }
                            }
                        }
                        else {
                            string loader_name = CDirEntry::ConvertToOSPath(mFilename);
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
                    double elapsed = sw.Elapsed();
                    if (elapsed > m_Longest) {
                        m_Longest = elapsed;
                        m_LongestId = m_CurrentId;
                    }
                }
            }
        }
        catch (const CException& e) {
            string errstr = e.GetMsg();
            errstr = NStr::Replace(errstr, "\n", " * ");
            errstr = NStr::Replace(errstr, " *   ", " * ");
            CRef<CValidError> eval(new CValidError());
            if (NStr::StartsWith(errstr, "duplicate Bioseq id", NStr::eNocase)) {
                eval->AddValidErrItem(eDiag_Critical, eErr_GENERIC_DuplicateIDs, errstr);
                PrintValidError(eval);
                // ERR_POST(e);
            }
            else {
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
    mpIstr.reset();
}
