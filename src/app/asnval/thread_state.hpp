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
#include "thread_pool.hpp"

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <objmgr/object_manager.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/validator_context.hpp>
#include <objtools/edit/huge_file_process.hpp>
#include <objtools/validator/huge_file_validator.hpp>
#include <util/message_queue.hpp>
#include "formatters.hpp"
#include <functional>

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


namespace ncbi {
class IMessageHandler;
}

class CValidatorThreadPool: public CThreadPoolCore
{
public:
    using CThreadPoolCore::CThreadPoolCore;
};

class CAsnvalThreadState
{
public:

    CAsnvalThreadState(const CAppConfig&, SValidatorContext::taxupdate_func_t taxon);
    CAsnvalThreadState(const CAsnvalThreadState& other) = delete;
    ~CAsnvalThreadState();
    CThreadExitData ValidateOneFile(const string& infilename, CNcbiOstream& ostr);
    CThreadExitData ValidateOneFile(const string& infilename, IMessageHandler& msgHandler);

protected:

    void ReadClassMember(CObjectIStream& in,
            const CObjectInfo::CMemberIterator& member,
            IMessageHandler& msgHandler);

    CRef<CScope> BuildScope() const;
    unique_ptr<CObjectIStream> OpenFile(TTypeInfo& asn_info, const string& filename) const;

    void ReportReadFailure(const CException* p_exception, IMessageHandler& msgHandler);

    // batch mode processing
    void ProcessSSMReleaseFile(IMessageHandler& msgHandler);
    void ProcessBSSReleaseFile(IMessageHandler& msgHandler);

    // traditional way of processing
    void ValidateInput(TTypeInfo asninfo, IMessageHandler& msgHandler);
    void ProcessSeqEntry(CSeq_entry& se, IMessageHandler& msgHandler);
    void ProcessSeqDesc(CRef<CSerialObject> serial, IMessageHandler& msgHandler);
    void ProcessBioseqset(CRef<CSerialObject> serial, IMessageHandler& msgHandler);
    void ProcessBioseq(CRef<CSerialObject> serial, IMessageHandler& msgHandler);
    void ProcessPubdesc(CRef<CSerialObject> serial, IMessageHandler& msgHandler);
    void ProcessSeqEntry(CRef<CSerialObject> serial, IMessageHandler& msgHandler);
    void ProcessSeqSubmit(CRef<CSerialObject> serial, IMessageHandler& msgHandler);
    void ProcessSeqAnnot(CRef<CSerialObject> serial, IMessageHandler& msgHandler);
    void ProcessSeqFeat(CRef<CSerialObject> serial, IMessageHandler& msgHandler);
    void ProcessBioSource(CRef<CSerialObject> serial, IMessageHandler& msgHandler);

    bool ValidateTraditionally(TTypeInfo asninfo, IMessageHandler& msgHandler);
    bool ValidateBatchMode(TTypeInfo asninfo, IMessageHandler& msgHandler);
    void ValidateOneHugeFile(edit::CHugeFileProcess& process, IMessageHandler& msgHandler);

    void ValidateOneHugeBlob(edit::CHugeFileProcess& process, IMessageHandler& msgHandler);
    void ValidateBlobAsync(const string& loader_name, edit::CHugeFileProcess& process, IMessageHandler& msgHandler);
    void ValidateBlobSequential(const string& loader_name, edit::CHugeFileProcess& process, IMessageHandler& msgHandler);

    void ValidateAsync(
        const string& loader_name,
        CConstRef<CSubmit_block> pSubmitBlock,
        CConstRef<CSeq_id> seqid,
        IMessageHandler& msgHandler
        ) const;

    static CThreadExitData ValidateWorker(CAsnvalThreadState* _this,
        const string& loader_name, CConstRef<CSubmit_block> pSubmitBlock, CConstRef<CSeq_id> seqid,
        IMessageHandler& msgHandler);


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
    bool m_ReadFailure{false};

    size_t m_Level = 0;
    std::atomic<size_t> m_Reported {0};

    CHugeFileValidator::TGlobalInfo m_GlobalInfo;

    shared_ptr<SValidatorContext> m_pContext;
    std::list<CConstRef<CValidError>> m_eval;
};

#endif
