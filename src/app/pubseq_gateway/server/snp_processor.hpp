#ifndef SNP_PROCESSOR__HPP
#define SNP_PROCESSOR__HPP

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
 * File Description: processor for data from SNP
 *
 */

#include "ipsgs_processor.hpp"
#include "resolve_base.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "snp_client.hpp"
#include <objects/seq/seq_id_handle.hpp>
#include <thread>


BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);
class CID2_Reply_Data;
END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(snp);


struct SSNPProcessor_Config;
class CSNPClient;

const string    kSNPProcessorEvent = "SNP";

class CPSGS_SNPProcessor :
    virtual public CPSGS_CassProcessorBase,
    virtual public CPSGS_ResolveBase
{
public:
    CPSGS_SNPProcessor(void);
    ~CPSGS_SNPProcessor(void) override;

    IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                     shared_ptr<CPSGS_Reply> reply,
                                     TProcessorPriority priority) const override;

    void Process(void) override;
    void Cancel(void) override;
    EPSGS_Status GetStatus(void) override;
    string GetName(void) const override;
    string GetGroupName(void) const override;

    void GetAnnotation(void);
    void OnGotAnnotation(void);

    void GetBlobByBlobId(void);
    void OnGotBlobByBlobId(void);

    void GetChunk(void);
    void OnGotChunk(void);

    // Seq-id pre-resolving
    virtual void ProcessEvent(void);

private:
    CPSGS_SNPProcessor(const shared_ptr<CSNPClient>& client,
                       shared_ptr<CPSGS_Request> request,
                       shared_ptr<CPSGS_Reply> reply,
                       TProcessorPriority priority);


    void x_LoadConfig(void);
    bool x_IsEnabled(CPSGS_Request& request) const;
    void x_InitClient(void) const;

    void x_ProcessAnnotationRequest(void);
    void x_ProcessBlobBySatSatKeyRequest(void);
    void x_ProcessTSEChunkRequest(void);

    // Seq-id pre-resolving
    void x_OnSeqIdResolveFinished(SBioseqResolution&& bioseq_resolution);
    void x_OnSeqIdResolveError(CRequestStatus::ECode  status,
                               int  code,
                               EDiagSev  severity,
                               const string& message);
    void x_OnResolutionGoodData(void);
    void x_Peek(bool  need_wait);
    bool x_Peek(unique_ptr<CCassFetch>& fetch_details, bool  need_wait);

    void x_WriteData(objects::CID2_Reply_Data& data, const CSerialObject& obj) const;
    void x_SendAnnotInfo(void);
    void x_SendBlob(void);
    void x_SendChunk(void);
    void x_SendSplitInfo(const SSNPData& data);
    void x_SendMainEntry(const SSNPData& data);
    void x_SendBlobProps(const string& psg_blob_id, CBlobRecord& blob_props);
    void x_SendBlobData(const string& psg_blob_id, const objects::CID2_Reply_Data& data);
    void x_SendChunkBlobProps(const string& id2_info, int chunk_id, CBlobRecord& blob_props);
    void x_SendChunkBlobData(const string& id2_info, int chunk_id, const objects::CID2_Reply_Data& data);

    void x_UnlockRequest(void);
    void x_Finish(EPSGS_Status status);
    bool x_IsCanceled();
    bool x_SignalStartProcessing();

    shared_ptr<SSNPProcessor_Config> m_Config;
    mutable shared_ptr<CSNPClient> m_Client;

    EPSGS_Status m_Status = ePSGS_NotFound;
    bool m_Canceled = false;
    vector<CSeq_id_Handle> m_SeqIds;
    string m_PSGBlobId; // requested blob-id
    string m_Id2Info;   // requested id2-info
    int m_ChunkId;  // requested chunk-id
    vector<SSNPData> m_SNPData;
    bool m_Unlocked;
};


END_NAMESPACE(snp);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // CDD_PROCESSOR__HPP
