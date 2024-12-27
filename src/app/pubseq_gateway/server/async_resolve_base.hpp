#ifndef PSGS_ASYNCRESOLVEBASE__HPP
#define PSGS_ASYNCRESOLVEBASE__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: base class for processors which need to resolve seq_id
 *                   asynchronously
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_fetch.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "cass_processor_base.hpp"
#include "pubseq_gateway_utils.hpp"
#include "psgs_seq_id_utils.hpp"

#include <objects/seqloc/Seq_id.hpp>
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_IDBLOB_SCOPE;

#include <functional>

using TSeqIdResolutionFinishedCB =
                function<void(SBioseqResolution &&  async_bioseq_resolution)>;
using TSeqIdResolutionErrorCB =
                function<void(CRequestStatus::ECode  status,
                              int  code,
                              EDiagSev  severity,
                              const string &  message,
                              EPSGS_LoggingFlag  logging_flag)>;
using TSeqIdResolutionStartProcessingCB =
                function<void(void)>;

using TContinueResolveCB =
                function<void(void)>;


// Used to accumulate errors during the resolution of multiple seq_ids
class CPSGSResolveErrors
{
public:
    void AppendError(const string &  msg, CRequestStatus::ECode  code);

    bool HasErrors(void) const
    {
        return !m_Errors.empty();
    }

    string GetCombinedErrorMessage(const list<SPSGSeqId> &  seq_id_to_resolve) const;
    CRequestStatus::ECode GetCombinedErrorCode(void) const;

private:
    vector<SResolveInputSeqIdError>     m_Errors;
};


class CPSGS_AsyncResolveBase : virtual public CPSGS_CassProcessorBase
{
public:
    CPSGS_AsyncResolveBase();
    CPSGS_AsyncResolveBase(shared_ptr<CPSGS_Request> request,
                           shared_ptr<CPSGS_Reply> reply,
                           TContinueResolveCB  continue_resolve_cb,
                           TSeqIdResolutionFinishedCB finished_cb,
                           TSeqIdResolutionErrorCB error_cb,
                           TSeqIdResolutionStartProcessingCB  start_processing_cb);
    virtual ~CPSGS_AsyncResolveBase();

public:
    using CPSGS_CassProcessorBase::Process;

    void Process(int16_t               effective_version,
                 int16_t               effective_seq_id_type,
                 list<string> &&       secondary_id_list,
                 string &&             primary_seq_id,
                 bool                  composed_ok,
                 bool                  seq_id_resolve,
                 SBioseqResolution &&  bioseq_resolution);

private:
    enum EPSGS_ResolveStage {
        eInit,                          // Initial stage; nothing has been done yet
        ePrimaryBioseq,                 // BIOSEQ_INFO (primary) request issued

        eSecondarySi2csi,               // loop over all secondary seq_id in SI2CSI
                                        // and then switch to eSecondaryAsIs
        eSecondaryAsIs,

        eFinished,

        ePostSi2Csi                 // Special case for seq_id like gi|156232
    };

protected:
    int16_t GetEffectiveVersion(const CTextseq_id *  text_seq_id);
    SPSGS_ResolveRequest::TPSGS_BioseqIncludeData GetBioseqInfoFields(void);
    bool NonKeyBioseqInfoFieldsRequested(void);
    bool CanSkipBioseqInfoRetrieval(const CBioseqInfoRecord &  bioseq_info_record);
    SPSGS_RequestBase::EPSGS_AccSubstitutioOption
                                GetAccessionSubstitutionOption(void);
    EPSGS_AccessionAdjustmentResult
                AdjustBioseqAccession(SBioseqResolution &  bioseq_resolution);
    void SetupSeqIdToResolve(void);
    void SetupSeqIdToResolve(const string &  seq_id, int16_t  seq_id_type);
    string GetCouldNotResolveMessage(void) const;

protected:
    psg_time_point_t GetAsyncResolutionStartTimestamp(void) const
    {
        return m_AsyncCassResolutionStart;
    }

    void SetAsyncResolutionStartTimestamp(const psg_time_point_t &  ts)
    {
        m_AsyncCassResolutionStart = ts;
    }

    bool MoveToNextSeqId(void);
    bool GetSeqIdResolve(void);
    bool OptimizationPrecondition(const string &  primary_id,
                                  int16_t  effective_seq_id_type) const;

private:
    void x_Process(void);

private:
    string x_GetRequestSeqId(void);
    int16_t x_GetRequestSeqIdType(void);
    string x_GetSeqIdsToResolveList(void) const;

private:
    void x_PreparePrimaryBioseqInfoQuery(const CBioseqInfoRecord::TAccession &  seq_id,
                                         CBioseqInfoRecord::TVersion  version,
                                         CBioseqInfoRecord::TSeqIdType  seq_id_type,
                                         CBioseqInfoRecord::TGI  gi,
                                         bool  with_seq_id_type);
    void x_PrepareSecondarySi2csiQuery(void);
    void x_PrepareSecondaryAsIsSi2csiQuery(void);
    void x_PrepareSi2csiQuery(const string &  secondary_id,
                              int16_t  effective_seq_id_type);

public:
    void x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records);
    void x_OnBioseqInfoWithoutSeqIdType(vector<CBioseqInfoRecord>&&  records);
    void x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity, const string &  message);
    void x_OnSi2csiRecord(vector<CSI2CSIRecord> &&  records);
    void x_OnSi2csiError(CRequestStatus::ECode  status, int  code,
                         EDiagSev  severity, const string &  message);

private:
    void x_OnSeqIdAsyncResolutionFinished(
                        SBioseqResolution &&  async_bioseq_resolution);
    void x_SignalStartProcessing(void);

protected:
    TContinueResolveCB                  m_ContinueResolveCB;
    TSeqIdResolutionFinishedCB          m_FinishedCB;
    TSeqIdResolutionErrorCB             m_ErrorCB;
    TSeqIdResolutionStartProcessingCB   m_StartProcessingCB;

    EPSGS_ResolveStage                  m_ResolveStage;
    bool                                m_ComposedOk;
    bool                                m_SeqIdResolve;
    string                              m_PrimarySeqId;
    int16_t                             m_EffectiveVersion;
    int16_t                             m_EffectiveSeqIdType;
    size_t                              m_SecondaryIndex;
    list<string>                        m_SecondaryIdList;
    SBioseqResolution                   m_BioseqResolution;

    CCassFetch *                        m_CurrentFetch;
    CCassFetch *                        m_NoSeqIdTypeFetch;

    CBioseqInfoRecord::TAccession       m_BioseqInfoRequestedAccession;
    CBioseqInfoRecord::TVersion         m_BioseqInfoRequestedVersion;
    CBioseqInfoRecord::TSeqIdType       m_BioseqInfoRequestedSeqIdType;
    CBioseqInfoRecord::TGI              m_BioseqInfoRequestedGI;

    psg_time_point_t                    m_BioseqInfoStart;
    psg_time_point_t                    m_Si2csiStart;
    psg_time_point_t                    m_AsyncCassResolutionStart;

    bool                                m_StartProcessingCalled;

    // In case of ID/get_na request a seq_id (and possibly seq_id_type) can be
    // selected from the provided seq_id/seq_id_type and other_seq_ids list.
    // So the selected part is stored in the variables below regardless what
    // kind of request it was.
    list<SPSGSeqId>                     m_SeqIdsToResolve;
    list<SPSGSeqId>::const_iterator     m_CurrentSeqIdToResolve;
    CPSGSResolveErrors                  m_ResolveErrors;
};

#endif  // PSGS_ASYNCRESOLVEBASE__HPP

