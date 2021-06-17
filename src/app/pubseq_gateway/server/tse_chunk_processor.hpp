#ifndef PSGS_TSECHUNKPROCESSOR__HPP
#define PSGS_TSECHUNKPROCESSOR__HPP

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
 * File Description: get TSE chunk processor
 *
 */

#include "cass_blob_base.hpp"
#include "id2info.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

// Forward declaration
class CCassFetch;


class CPSGS_TSEChunkProcessor : public CPSGS_CassBlobBase
{
public:
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority  priority) const;
    virtual void Process(void);
    virtual void Cancel(void);
    virtual EPSGS_Status GetStatus(void);
    virtual string GetName(void) const;
    virtual void ProcessEvent(void);

public:
    CPSGS_TSEChunkProcessor();
    CPSGS_TSEChunkProcessor(
            shared_ptr<CPSGS_Request> request,
            shared_ptr<CPSGS_Reply> reply,
            TProcessorPriority  priority,
            shared_ptr<CPSGS_SatInfoChunksVerFlavorId2Info> sat_info_chunk_ver_id2info,
            shared_ptr<CPSGS_IdModifiedVerFlavorId2Info>    id_mod_ver_id2info);
    virtual ~CPSGS_TSEChunkProcessor();

private:
    void OnGetBlobProp(CCassBlobFetch *  fetch_details,
                       CBlobRecord const &  blob, bool is_found);
    void OnGetBlobError(CCassBlobFetch *  fetch_details,
                        CRequestStatus::ECode  status, int  code,
                        EDiagSev  severity, const string &  message);
    void OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                        CBlobRecord const &  blob,
                        const unsigned char *  chunk_data,
                        unsigned int  data_size, int  chunk_no);
    void OnGetSplitHistoryError(CCassSplitHistoryFetch *  fetch_details,
                                CRequestStatus::ECode  status,
                                int  code,
                                EDiagSev  severity,
                                const string &  message);
    void OnGetSplitHistory(CCassSplitHistoryFetch *  fetch_details,
                           vector<SSplitHistoryRecord> && result);

private:
    void x_ProcessSatInfoChunkVerId2Info(void);
    void x_ProcessIdModVerId2Info(void);

    void x_SendProcessorError(const string &  msg,
                              CRequestStatus::ECode  status,
                              int  code);
    bool x_ValidateTSEChunkNumber(
                    int64_t  requested_chunk,
                    CPSGS_SatInfoChunksVerFlavorId2Info::TChunks  total_chunks,
                    bool  need_finish);
    bool x_TSEChunkSatToKeyspace(SCass_BlobId &  blob_id);
    EPSGSId2InfoFlavor x_DetectId2InfoFlavor(
            const string &                                    id2_info,
            shared_ptr<CPSGS_SatInfoChunksVerFlavorId2Info> & sat_info_chunk_ver_id2info,
            shared_ptr<CPSGS_IdModifiedVerFlavorId2Info> &    id_mod_ver_id2info) const;
    bool x_ParseTSEChunkId2Info(
            const string &                                     info,
            unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info> &  id2_info,
            const SCass_BlobId &                               blob_id,
            bool                                               need_finish);
    bool x_TSEChunkSatToKeyspace(SCass_BlobId &  blob_id,
                                 bool  need_finish);
    void x_RequestTSEChunk(const SSplitHistoryRecord &  split_record,
                           CCassSplitHistoryFetch *  fetch_details);

private:
    void x_Peek(bool  need_wait);
    bool x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                bool  need_wait);

private:
    SPSGS_TSEChunkRequest *                             m_TSEChunkRequest;

    // NB: Only one will be populated
    shared_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>     m_SatInfoChunkVerId2Info;
    shared_ptr<CPSGS_IdModifiedVerFlavorId2Info>        m_IdModVerId2Info;
};

#endif  // PSGS_TSECHUNKPROCESSOR__HPP

