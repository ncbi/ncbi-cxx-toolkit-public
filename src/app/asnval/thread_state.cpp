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
#include "message_queue.hpp"
#include "xml_val_stream.hpp"
#include "thread_state.hpp"
#include <objtools/validator/huge_file_validator.hpp>

using namespace ncbi;
USING_SCOPE(objects);
USING_SCOPE(validator);
USING_SCOPE(edit);

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
    const CAsnvalThreadState& other)
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
    m_pTaxon = other.m_pTaxon;
#ifdef USE_XMLWRAPP_LIBS
    m_ostr_xml.reset();
#endif
};

//  ============================================================================
CAsnvalThreadState::CAsnvalThreadState(
    CRef<CObjectManager> objMgr,
    unsigned int options,
    bool hugeFile,
    bool continue_,
    bool onlyAnnots,
    bool quiet,
    double longest,
    const string& currentId,
    const string& longestId,
    size_t numFiles,
    size_t numRecords,
    size_t level,
    EDiagSev reportLevel,
    bool doCleanup,
    EDiagSev lowCutoff,
    EDiagSev highCutoff,
    EVerbosity verbosity,
    bool batch,
    const string& onlyError,
    CNcbiOstream* validErrorStream,
    shared_ptr<CTaxon3> taxon,
    const string& filename)
//  ============================================================================
{
    mFilename = filename;
    mpIstr.reset();
    mpHugeFileProcess.reset();
    m_ObjMgr = objMgr;
    m_Options = options;
    m_HugeFile = hugeFile;
    m_Continue = continue_;
    m_OnlyAnnots = onlyAnnots;
    m_Quiet = quiet;
    m_Longest = longest;
    m_CurrentId = currentId;
    m_LongestId = longestId;
    m_NumFiles = numFiles;
    m_NumRecords = numRecords;

    m_Level = level;
    m_Reported = 0;
    m_ReportLevel = reportLevel;

    m_DoCleanup = doCleanup;

    m_LowCutoff = lowCutoff;
    m_HighCutoff = highCutoff;

    m_verbosity = verbosity;
    m_batch = batch;
    m_OnlyError = onlyError;

    m_ValidErrorStream = validErrorStream;

    m_pContext.reset(new SValidatorContext());
    m_pTaxon = taxon;
#ifdef USE_XMLWRAPP_LIBS
    m_ostr_xml.reset();
#endif
};

