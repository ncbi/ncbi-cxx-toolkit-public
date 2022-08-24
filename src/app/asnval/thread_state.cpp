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
#include "xml_val_stream.hpp"
#include "thread_state.hpp"
#include <objtools/validator/huge_file_validator.hpp>

using namespace ncbi;
USING_SCOPE(objects);
USING_SCOPE(validator);
USING_SCOPE(edit);

const set<TTypeInfo> s_known_types{
    CSeq_submit::GetTypeInfo(), CSeq_entry::GetTypeInfo(), CSeq_annot::GetTypeInfo(),
    CSeq_feat::GetTypeInfo(),   CBioSource::GetTypeInfo(), CPubdesc::GetTypeInfo(),
    CBioseq_set::GetTypeInfo(), CBioseq::GetTypeInfo(),    CSeqdesc::GetTypeInfo(),
};


//  ============================================================================
CAsnvalThreadState::CAsnvalThreadState() :
//  ============================================================================
    m_ObjMgr(),
    m_Options(0),
    m_Continue(false),
    m_OnlyAnnots(false),
    m_Quiet(false),
    m_Longest(0.0),
    m_NumFiles(0),
    m_NumRecords(0),
    m_Level(0),
    m_Reported(0),
    m_verbosity(eVerbosity_min),
    m_ValidErrorStream(nullptr)
{};

//  ============================================================================
CAsnvalThreadState::CAsnvalThreadState(
    CAsnvalThreadState& other)
//  ============================================================================
{
    mFilename = other.mFilename;
    mpIstr.reset();
    mpHugeFileProcess.reset();
    m_ObjMgr = other.m_ObjMgr;
    m_Options = other.m_Options;
    m_HugeFile = other.m_HugeFile;
    m_Continue = other.m_Continue;
    m_OnlyAnnots = other.m_OnlyAnnots;
    m_Quiet = other.m_Quiet;
    m_Longest = other.m_Longest;
    m_CurrentId = other.m_CurrentId;
    m_LongestId = other.m_LongestId;
    m_NumFiles = other.m_NumFiles;
    m_NumRecords = other.m_NumRecords;

    m_Level = other.m_Level;
    m_Reported = 0;
    m_ReportLevel = other.m_ReportLevel;

    m_DoCleanup = other.m_DoCleanup;
    //CCleanup m_Cleanup;

    m_LowCutoff = other.m_LowCutoff;
    m_HighCutoff = other.m_HighCutoff;

    m_verbosity = other.m_verbosity;
    m_batch = other.m_batch;
    m_OnlyError = other.m_OnlyError;

    m_ValidErrorStream = other.m_ValidErrorStream;

    m_pContext.reset(new SValidatorContext());
#ifdef USE_XMLWRAPP_LIBS
    m_ostr_xml.reset();
#endif
};


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
    * context.m_ValidErrorStream << "<asnvalidate version=\"" << "3." << NCBI_SC_VERSION_PROXY << "."
        << NCBI_TEAMCITY_BUILD_NUMBER_PROXY << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << "\n";
#endif
}


