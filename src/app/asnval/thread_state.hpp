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

    CRef<CScope> BuildScope() const;
    unique_ptr<CObjectIStream> OpenFile(TTypeInfo& asn_info, const string& filename) const;

    void PrintValidError(CConstRef<CValidError> errors);
    void PrintValidErrItem(const CValidErrItem& item);
    CRef<CValidError> ReportReadFailure(const CException* p_exception) const;

    // batch mode processing
    void ProcessSSMReleaseFile();
    void ProcessBSSReleaseFile();

    // traditional way of processing
    CConstRef<CValidError> ValidateInput(TTypeInfo asninfo);
    CConstRef<CValidError> ProcessSeqEntry(CSeq_entry& se);
    CConstRef<CValidError> ProcessSeqDesc(CRef<CSerialObject> serial);
    CConstRef<CValidError> ProcessBioseqset(CRef<CSerialObject> serial);
    CConstRef<CValidError> ProcessBioseq(CRef<CSerialObject> serial);
    CConstRef<CValidError> ProcessPubdesc(CRef<CSerialObject> serial);
    CConstRef<CValidError> ProcessSeqEntry(CRef<CSerialObject> serial);
    CConstRef<CValidError> ProcessSeqSubmit(CRef<CSerialObject> serial);
    CConstRef<CValidError> ProcessSeqAnnot(CRef<CSerialObject> serial);
    CConstRef<CValidError> ProcessSeqFeat(CRef<CSerialObject> serial);
    CConstRef<CValidError> ProcessBioSource(CRef<CSerialObject> serial);

    CConstRef<CValidError> ValidateAsync(
        const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid) const;

    static CThreadExitData ValidateWorker(CAsnvalThreadState* _this,
        const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid);

    bool ValidateTraditionally(TTypeInfo asninfo);
    bool ValidateBatchMode(TTypeInfo asninfo);
    void ValidateOneHugeFile(edit::CHugeFileProcess& process);
    void ValidateOneHugeBlob(edit::CHugeFileProcess& process);
    void ValidateBlobAsync(const string& loader_name, edit::CHugeFileProcess& process);
    void ValidateBlobSequential(const string& loader_name, edit::CHugeFileProcess& process);

    const CAppConfig& mAppConfig;

    unique_ptr<CObjectIStream> mpIstr;
    ///
    mutable
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
