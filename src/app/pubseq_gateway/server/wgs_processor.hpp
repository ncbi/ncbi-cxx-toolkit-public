#ifndef WGS_PROCESSOR__HPP
#define WGS_PROCESSOR__HPP

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
 * Authors: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description: processor for data from WGS
 *
 */

#include "ipsgs_processor.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "timing.hpp"
#include "wgs_client.hpp"
#include <objects/seq/seq_id_handle.hpp>


BEGIN_NCBI_NAMESPACE;

class CThreadPool;

BEGIN_NAMESPACE(objects);
class CID2_Blob_Id;
class CID2_Reply_Data;
class CWGSResolver;
class CSeq_id;
class CAsnBinData;
END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(wgs);


const string    kWGSProcessorEvent = "WGS";

class CPSGS_WGSProcessor : public IPSGS_Processor
{
public:
    CPSGS_WGSProcessor(void);
    ~CPSGS_WGSProcessor(void) override;

    virtual bool CanProcess(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply) const override;
    IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                     shared_ptr<CPSGS_Reply> reply,
                                     TProcessorPriority priority) const override;

    void Process(void) override;
    void Cancel(void) override;
    EPSGS_Status GetStatus(void) override;
    string GetName(void) const override;
    string GetGroupName(void) const override;

    void ResolveSeqId(void);
    void OnResolvedSeqId(void);

    void GetBlobBySeqId(void);
    void OnGotBlobBySeqId(void);

    void GetBlobByBlobId(void);
    void OnGotBlobByBlobId(void);

    void GetChunk(void);
    void OnGotChunk(void);

public:
    static string GetPSGId2Info(const CID2_Blob_Id& tse_id,
                                CWGSClient::TID2SplitVersion split_version);

private:
    CPSGS_WGSProcessor(const shared_ptr<CWGSClient>& client,
                       shared_ptr<ncbi::CThreadPool> thread_pool,
                       shared_ptr<CPSGS_Request> request,
                       shared_ptr<CPSGS_Reply> reply,
                       TProcessorPriority priority);

    void x_LoadConfig(void);
    bool x_IsEnabled(CPSGS_Request& request) const;
    void x_InitClient(void) const;

    void x_ProcessResolveRequest(void);
    void x_ProcessBlobBySeqIdRequest(void);
    void x_ProcessBlobBySatSatKeyRequest(void);
    void x_ProcessTSEChunkRequest(void);

    typedef SPSGS_ResolveRequest::EPSGS_OutputFormat EOutputFormat;
    typedef SPSGS_ResolveRequest::TPSGS_BioseqIncludeData TBioseqInfoFlags;
    typedef int TID2ChunkId;
    typedef vector<string> TBlobIds;

    void x_RegisterTiming(psg_time_point_t start,
                          EPSGOperation operation,
                          EPSGOperationStatus status,
                          size_t blob_size);
    void x_RegisterTimingFound(psg_time_point_t start,
                               EPSGOperation operation,
                               const objects::CID2_Reply_Data& data);
    void x_RegisterTimingNotFound(EPSGOperation operation);
    EOutputFormat x_GetOutputFormat(void);
    void x_SendResult(const string& data_to_send, EOutputFormat output_format);
    void x_SendBioseqInfo(void);
    void x_SendBlobProps(const string& psg_blob_id, CBlobRecord& blob_props);
    void x_SendBlobForbidden(const string& psg_blob_id);
    void x_SendBlobData(const string& psg_blob_id, const objects::CID2_Reply_Data& data);
    void x_SendChunkBlobProps(const string& id2_info,
                              TID2ChunkId chunk_id,
                              CBlobRecord& blob_props);
    void x_SendChunkBlobData(const string& id2_info,
                             TID2ChunkId chunk_id,
                             const objects::CID2_Reply_Data& data);
    void x_SendSplitInfo(void);
    void x_SendMainEntry(void);
    void x_SendExcluded(void);
    void x_SendForbidden(void);
    void x_SendBlob(void);
    void x_SendChunk(void);
    void x_WriteData(objects::CID2_Reply_Data& data,
                     const objects::CAsnBinData& obj,
                     bool compress) const;

    static void x_SendError(shared_ptr<CPSGS_Reply> reply,
                            const string& msg);
    static void x_SendError(shared_ptr<CPSGS_Reply> reply,
                            const string& msg, const exception& exc);
    void x_SendError(const string& msg);
    void x_SendError(const string& msg, const exception& exc);

    template<class C> static int x_GetBlobState(const C& obj) {
        return obj.IsSetBlob_state() ? obj.GetBlob_state() : 0;
    }

    void x_UnlockRequest(void);
    void x_WaitForOtherProcessors(void);
    void x_Finish(EPSGS_Status status);
    bool x_IsCanceled();
    bool x_SignalStartProcessing();

    CFastMutex m_Mutex;
    shared_ptr<SWGSProcessor_Config> m_Config;
    mutable shared_ptr<CWGSClient> m_Client;
    psg_time_point_t m_Start;
    EPSGS_Status m_Status;
    bool m_Canceled;
    CRef<objects::CSeq_id> m_SeqId; // requested seq-id
    string m_PSGBlobId; // requested blob-id
    string m_Id2Info; // requested id2-info
    int64_t m_ChunkId; // requested chunk-id
    TBlobIds m_ExcludedBlobs;
    shared_ptr<SWGSData> m_WGSData;
    string m_WGSDataError;
    EOutputFormat m_OutputFormat;
    bool m_Unlocked;
    shared_ptr<ncbi::CThreadPool> m_ThreadPool;
};


END_NAMESPACE(wgs);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // CDD_PROCESSOR__HPP
