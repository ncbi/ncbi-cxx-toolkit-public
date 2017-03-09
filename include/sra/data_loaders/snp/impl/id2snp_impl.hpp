#ifndef SRA__LOADER__SNP__IMPL__ID2SNP__HPP
#define SRA__LOADER__SNP__IMPL__ID2SNP__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Processor of ID2 requests for SNP data
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_config.hpp>
#include <sra/readers/sra/snpread.hpp>
#include <util/limited_size_map.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <sra/data_loaders/snp/id2snp.hpp>
#include <vector>

BEGIN_NCBI_NAMESPACE;

class CThreadNonStop;

BEGIN_NAMESPACE(objects);

class CSeq_id;
class CDbtag;
class CID2_Request;
class CID2_Request_Packet;
class CID2_Request_Get_Seq_id;
class CID2_Request_Get_Blob_Id;
class CID2_Request_Get_Blob_Info;
class CID2S_Request_Get_Chunks;
class CID2_Blob_Id;
class CID2_Reply;
class CID2_Reply_Get_Seq_id;
class CID2_Reply_Get_Blob_Id;
class CID2_Reply_Data;
class CID2SNPContext;
class CID2ProcessorResolver;
class CID2SNPProcessorParams;
class CID2SNPProcessorState;

class CID2SNPProcessorContext : public CID2ProcessorContext
{
public:
    CID2SNPContext m_Context;
};


struct SSNPDbTrackInfo {
    SSNPDbTrackInfo(void)
        : m_NAIndex(0),
          m_NAVersion(0),
          m_FilterIndex(0)
        {
        }
    
    size_t m_NAIndex;
    size_t m_NAVersion;
    size_t m_FilterIndex;
};


class CID2SNPProcessorPacketContext : public CID2ProcessorPacketContext
{
public:
    typedef int TSerialNumber;
    typedef vector< CRef<CID2_Request> > TRequests;
    typedef vector< CRef<CID2_Reply> > TReplies;
    
    struct SRequestInfo {
        SRequestInfo()
            : m_OriginalSeqIdType(0),
              m_SentBlobIds(false)
            {
            }
        
        TReplies m_AddedReplies;
        // blob-id resolution
        vector<SSNPDbTrackInfo> m_SNPTracks;
        // seq-id resolution
        int m_OriginalSeqIdType; // seq-id type requested by client
        string m_SeqAcc; // known qualified text Seq-id (acc.ver)
        bool m_SentBlobIds;
    };
    typedef map<TSerialNumber, SRequestInfo> TSNPRequests;
    TSNPRequests m_SNPRequests;
};


class CID2SNPProcessor_Impl : public CObject
{
public:
    explicit
    CID2SNPProcessor_Impl(const CConfig::TParamTree* params = 0,
                          const string& driver_name = kEmptyStr);
    ~CID2SNPProcessor_Impl(void);

    typedef vector<CRef<CID2_Reply> > TReplies;
    
    CRef<CID2SNPProcessorParams> CreateParameters(void);
    CRef<CID2SNPProcessorState> ProcessPacket(CID2_Request_Packet& packet,
                                              CID2SNPProcessorParams& params,
                                              TReplies& replies);
    void ProcessReply(CID2_Reply& reply,
                      CID2SNPProcessorParams& params,
                      CID2SNPProcessorState& state,
                      TReplies& replies);
    
    CRef<CID2SNPProcessorContext> CreateContext(void);
    CRef<CID2SNPProcessorPacketContext> ProcessPacket(CID2SNPProcessorContext* context,
                                                      CID2_Request_Packet& packet,
                                                      TReplies& replies);
    void ProcessReply(CID2SNPProcessorContext* context,
                      CID2SNPProcessorPacketContext* packet_context,
                      CID2_Reply& reply,
                      TReplies& replies);

    const CID2SNPContext& GetInitialContext(void) const {
        return m_InitialContext;
    }
    void InitContext(CID2SNPContext& context,
                     const CID2_Request& main_request);

    struct SSNPEntryInfo {
        SSNPEntryInfo(void)
            : m_Valid(false),
              m_SeqIndex(0)
            {
            }

        DECLARE_OPERATOR_BOOL(m_Valid);

        // parameters
        SSNPDbTrackInfo m_Track;
        bool m_Valid;
        unsigned m_SeqIndex;
        // cached objects
        CSNPDb m_SNPDb;
        CSNPDbSeqIterator m_SeqIter;
        CRef<CID2_Blob_Id> m_BlobId;
    };
    
  protected:
    enum EProcessStatus {
        eNotProcessed,
        eProcessed,
        eNeedReplies
    };
    EProcessStatus x_ProcessGetBlobId(CID2SNPContext& context,
                                      CID2SNPProcessorPacketContext& packet_context,
                                      TReplies& replies,
                                      CID2_Request& main_request,
                                      CID2_Request_Get_Blob_Id& request);
    EProcessStatus x_ProcessGetBlobInfo(CID2SNPContext& context,
                                        CID2SNPProcessorPacketContext& packet_context,
                                        TReplies& replies,
                                        CID2_Request& main_request,
                                        CID2_Request_Get_Blob_Info& request);
    EProcessStatus x_ProcessGetChunks(CID2SNPContext& context,
                                      CID2SNPProcessorPacketContext& packet_context,
                                      TReplies& replies,
                                      CID2_Request& main_request,
                                      CID2S_Request_Get_Chunks& request);
    void x_ProcessReplyGetSeqId(CID2SNPContext& context,
                                CID2SNPProcessorPacketContext& packet_context,
                                CID2_Reply& main_reply,
                                TReplies& replies,
                                CID2SNPProcessorPacketContext::SRequestInfo& info,
                                CID2_Reply_Get_Seq_id& reply);
    void x_ProcessReplyGetBlobId(CID2SNPContext& context,
                                 CID2SNPProcessorPacketContext& packet_context,
                                 CID2_Reply& main_reply,
                                 TReplies& replies,
                                 CID2SNPProcessorPacketContext::SRequestInfo& info,
                                 CID2_Reply_Get_Blob_Id& reply);

    
    // get various seq info

    // conversion to/from blob id
    SSNPEntryInfo x_ResolveBlobId(const CID2_Blob_Id& id);
    SSNPEntryInfo x_ResolveBlobId(const SSNPDbTrackInfo& track,
                                  const string& acc_ver);
    CID2_Blob_Id& x_GetBlobId(SSNPEntryInfo& id);

    static void x_AddSeqIdRequest(CID2_Request_Get_Seq_id& request,
                                  CID2SNPProcessorPacketContext::SRequestInfo& info);
    static bool x_GetAccVer(string& acc_ver, const CSeq_id& id);
    
    bool x_IsSNPNA(const string& s);

    // opening SNP files (with caching)
    CSNPDb GetSNPDb(const string& na);
    CSNPDb& GetSNPDb(SSNPEntryInfo& seq);

    // SNP iterators
    void ResetIteratorCache(SSNPEntryInfo& seq);
    CSNPDbSeqIterator& GetSeqIterator(SSNPEntryInfo& seq);
    
    void SetBlobState(CID2SNPContext& context,
                      CID2_Reply& main_reply,
                      int blob_state);

    bool ExcludedBlob(SSNPEntryInfo& seq,
                      const CID2_Request_Get_Blob_Info& request);
    void WriteData(CID2SNPContext& context,
                   const SSNPEntryInfo& seq,
                   CID2_Reply_Data& data,
                   const CSerialObject& obj);
    bool WorthCompressing(const SSNPEntryInfo& seq);

    CRef<CSerialObject> x_LoadBlob(CID2SNPContext& context, SSNPEntryInfo& info);
    CRef<CSerialObject> x_LoadChunk(CID2SNPContext& context, SSNPEntryInfo& entry, int chunk_id);
    
    typedef limited_size_map<string, CSNPDb> TSNPDbCache;

private:
    CMutex m_Mutex;
    CVDBMgr m_Mgr;
    TSNPDbCache m_SNPDbCache;
    CID2SNPContext m_InitialContext;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__LOADER__SNP__IMPL__ID2SNP__HPP
