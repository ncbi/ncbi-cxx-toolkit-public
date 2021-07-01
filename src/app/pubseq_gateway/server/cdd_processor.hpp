#ifndef CDD_PROCESSOR__HPP
#define CDD_PROCESSOR__HPP

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
 * Authors: Aleksey Grichenko
 *
 * File Description: processor for data from CDD
 *
 */

#include "ipsgs_processor.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include <objects/seq/seq_id_handle.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>
#include <thread>


BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);
class CCDD_Reply_Get_Blob_Id;
class CSeq_annot;
class CID2_Blob_Id;
END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(cdd);

class CPSGS_CDDProcessor : public IPSGS_Processor
{
public:
    CPSGS_CDDProcessor(void);
    ~CPSGS_CDDProcessor(void) override;

    IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority priority) const override;
    void Process(void) override;
    void Cancel(void) override;
    EPSGS_Status GetStatus(void) override;
    string GetName(void) const override;

    void OnGotBlobBySeqId(void);
    void OnGotBlobByBlobId(void);
    void OnGotBlobId(void);

private:
    CPSGS_CDDProcessor(shared_ptr<objects::CCDDClientPool> client_pool,
                       shared_ptr<CPSGS_Request> request,
                       shared_ptr<CPSGS_Reply> reply,
                       TProcessorPriority priority);

    bool x_CanProcessAnnotRequest(SPSGS_AnnotRequest& annot_request,
                                  TProcessorPriority priority) const;
    bool x_CanProcessBlobRequest(SPSGS_BlobBySatSatKeyRequest& blob_request) const;
    bool x_NameIncluded(const vector<string>& names) const;
    void x_Finish(EPSGS_Status status);
    void x_ProcessResolveRequest(void);
    void x_ProcessGetBlobRequest(void);
    void x_GetBlobBySeqIdAsync(void);
    void x_GetBlobIdAsync(void);
    void x_GetBlobByBlobIdAsync(void);
    void x_SendAnnotInfo(const objects::CCDD_Reply_Get_Blob_Id& blob_info);
    void x_SendAnnot(const objects::CID2_Blob_Id& id2_blob_id, CRef<objects::CSeq_annot>& annot);

    shared_ptr<objects::CCDDClientPool> m_ClientPool;
    EPSGS_Status m_Status;
    unique_ptr<thread> m_Thread;
    objects::CSeq_id_Handle m_SeqId;
    CRef<objects::CCDDClientPool::TBlobId> m_BlobId;
    objects::CCDDClientPool::SCDDBlob m_CDDBlob;
};


END_NAMESPACE(cdd);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // CDD_PROCESSOR__HPP
