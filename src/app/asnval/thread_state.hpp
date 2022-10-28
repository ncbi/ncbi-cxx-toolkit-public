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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   validator
 *
 */

#ifndef ASNVAL_THREAD_STATE_HPP
#define ASNVAL_THREAD_STATE_HPP

#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <objmgr/object_manager.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/validator_context.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/huge_file_process.hpp>
#include <objtools/validator/huge_file_validator.hpp>
#include "xml_val_stream.hpp"

using namespace ncbi;
USING_SCOPE(objects);
USING_SCOPE(validator);
USING_SCOPE(edit);

extern const set<TTypeInfo> s_known_types;
string s_GetSeverityLabel(EDiagSev, bool = false);
class CAppConfig;

struct CThreadExitData
{
    size_t mNumRecords = 0;
    double mLongest    = 0;
    string mLongestId;
    size_t mReported   = 0;
    std::list<CConstRef<CValidError>> mEval;
};

class CAsnvalOutput
{
public:
    CAsnvalOutput(const CAppConfig& config, const string& in_filename);
    CAsnvalOutput(const CAppConfig& config, std::ostream* file);
    ~CAsnvalOutput();
    size_t Write(const std::list<CConstRef<CValidError>>& eval);
private:
    void StartXML();
    void FinishXML();
    void PrintValidErrItem(const CValidErrItem& item);
    const CAppConfig& mAppConfig;
    std::unique_ptr<std::ostream> m_own_file;
    std::ostream* m_file = nullptr;
    unique_ptr<CValXMLStream> m_ostr_xml;
};

class CAsnvalThreadState
{
public:

    CAsnvalThreadState(const CAppConfig&, SValidatorContext::taxupdate_func_t taxon);
    CAsnvalThreadState(const CAsnvalThreadState& other) = delete;
    ~CAsnvalThreadState();
    CThreadExitData ValidateOneFile(const std::string& filename);

protected:

    void ReadClassMember(CObjectIStream& in, const CObjectInfo::CMemberIterator& member);

    CRef<CScope> BuildScope();
    unique_ptr<CObjectIStream> OpenFile(TTypeInfo& asn_info);

    void PrintValidError(CConstRef<CValidError> errors);
    void PrintValidErrItem(const CValidErrItem& item);
    CRef<CValidError> ReportReadFailure(const CException* p_exception);

    CConstRef<CValidError> ReadAny(CRef<CBioseq>& obj);
    CConstRef<CValidError> ReadAny(CRef<CSeq_entry>& obj);
    CConstRef<CValidError> ReadAny(CRef<CBioseq_set>& obj);
    CConstRef<CValidError> ReadAny(CRef<CSeq_feat>& obj);
    CConstRef<CValidError> ReadAny(CRef<CBioSource>& obj);
    CConstRef<CValidError> ReadAny(CRef<CPubdesc>& obj);
    CConstRef<CValidError> ReadAny(CRef<CSeq_submit>& obj);

    void ProcessSSMReleaseFile();
    void ProcessBSSReleaseFile();

    CConstRef<CValidError> ValidateInput(TTypeInfo asninfo);
    CConstRef<CValidError> ProcessSeqEntry(CSeq_entry& se);
    CConstRef<CValidError> ProcessSeqDesc();
    CConstRef<CValidError> ProcessBioseqset();
    CConstRef<CValidError> ProcessBioseq();
    CConstRef<CValidError> ProcessPubdesc();
    CConstRef<CValidError> ProcessSeqEntry();
    CConstRef<CValidError> ProcessSeqSubmit();
    CConstRef<CValidError> ProcessSeqAnnot();
    CConstRef<CValidError> ProcessSeqFeat();
    CConstRef<CValidError> ProcessBioSource();

    CConstRef<CValidError> ValidateAsync(
        const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid, CRef<CSeq_entry> pEntry);
    void ValidateOneHugeFile(const string& loader_name, bool use_mt);

    const CAppConfig& mAppConfig;

    string mFilename;
    unique_ptr<CObjectIStream> mpIstr;
    unique_ptr<edit::CHugeFileProcess> mpHugeFileProcess;
    ///
    CRef<CObjectManager> m_ObjMgr;
    unsigned int m_Options = 0;
    double m_Longest = 0;
    string m_CurrentId;
    string m_LongestId;
    size_t m_NumRecords = 0;

    size_t m_Level = 0;
    std::atomic<size_t> m_Reported {0};

    CCleanup m_Cleanup;

    CHugeFileValidator::TGlobalInfo m_GlobalInfo;

    shared_ptr<SValidatorContext> m_pContext;
    std::list<CConstRef<CValidError>> m_eval;
};

#endif
