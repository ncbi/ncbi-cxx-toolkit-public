#ifndef SRA__LOADER__WGS__IMPL__ID2WGS__HPP
#define SRA__LOADER__WGS__IMPL__ID2WGS__HPP
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
 *   Processor of ID2 requests for WGS data
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_config.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <util/limited_size_map.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <sra/data_loaders/wgs/id2wgs.hpp>
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
class CID2_Reply_Data;
class CID2WGSContext;
class CDataLoader;
class CWGSResolver;

class NCBI_ID2PROC_WGS_EXPORT CID2WGSProcessor_Impl : public CObject
{
public:
    explicit
    CID2WGSProcessor_Impl(const CConfig::TParamTree* params = 0,
                          const string& driver_name = kEmptyStr);
    ~CID2WGSProcessor_Impl(void);

    struct SWGSSeqInfo {
        SWGSSeqInfo(void)
            : m_IsWGS(false),
              m_ValidWGS(false),
              m_NoRootSeq(false),
              m_SeqType('\0'),
              m_RowDigits(0),
              m_RowId(0)
            {
            }

        DECLARE_OPERATOR_BOOL(m_ValidWGS);

        bool IsContig(void) const {
            return m_SeqType == '\0' && m_RowId != 0;
        }
        bool IsMaster(void) const {
            return m_SeqType == '\0' && m_RowId == 0;
        }
        bool IsScaffold(void) const {
            return m_SeqType == 'S';
        }
        bool IsProtein(void) const {
            return m_SeqType == 'P';
        }

        void SetMaster(void) {
            m_SeqType = '\0';
            m_RowId = 0;
        }
        void SetContig(void) {
            m_SeqType = '\0';
        }
        void SetScaffold(void) {
            m_SeqType = 'S';
        }
        void SetProtein(void) {
            m_SeqType = 'P';
        }

        // parameters
        string m_WGSAcc;
        bool m_IsWGS;
        bool m_ValidWGS;
        bool m_NoRootSeq;
        char m_SeqType;
        Uint1 m_RowDigits;
        TVDBRowId m_RowId;
        // cached objects
        CWGSDb m_WGSDb;
        CWGSSeqIterator m_ContigIter;
        CWGSScaffoldIterator m_ScaffoldIter;
        CWGSProteinIterator m_ProteinIter;
        CRef<CID2_Blob_Id> m_BlobId;
        AutoPtr<SWGSSeqInfo> m_RootSeq;
    };

    typedef vector<CRef<CID2_Reply> > TReplies;
    TReplies ProcessSomeRequests(CID2WGSContext& context,
                                 CID2_Request_Packet& packet,
                                 CID2ProcessorResolver* resolver);

    bool ProcessRequest(CID2WGSContext& context,
                        TReplies& replies,
                        CID2_Request& request,
                        CID2ProcessorResolver* resolver);

    const CID2WGSContext& GetInitialContext(void) const {
        return m_InitialContext;
    }
    void InitContext(CID2WGSContext& context,
                     const CID2_Request& main_request);

    bool ProcessGetSeqId(CID2WGSContext& context,
                         TReplies& replies,
                         CID2_Request& main_request,
                         CID2ProcessorResolver* resolver,
                         CID2_Request_Get_Seq_id& request);
    bool ProcessGetBlobId(CID2WGSContext& context,
                          TReplies& replies,
                          CID2_Request& main_request,
                          CID2ProcessorResolver* resolver,
                          CID2_Request_Get_Blob_Id& request);
    bool ProcessGetBlobInfo(CID2WGSContext& context,
                            TReplies& replies,
                            CID2_Request& main_request,
                            CID2ProcessorResolver* resolver,
                            CID2_Request_Get_Blob_Info& request);
    bool ProcessGetChunks(CID2WGSContext& context,
                          TReplies& replies,
                          CID2_Request& main_request,
                          CID2ProcessorResolver* resolver,
                          CID2S_Request_Get_Chunks& request);

protected:
    SWGSSeqInfo Resolve(TReplies& replies,
                        CID2_Request& main_request,
                        CID2ProcessorResolver* resolver,
                        CID2_Request_Get_Seq_id& request);
    SWGSSeqInfo Resolve(CID2WGSContext& context,
                        TReplies& replies,
                        CID2_Request& main_request,
                        CID2ProcessorResolver* resolver,
                        CID2_Request_Get_Blob_Id& request);

    // lookup
    SWGSSeqInfo ResolveGeneral(const CDbtag& dbtag);
    SWGSSeqInfo ResolveGi(TGi gi,
                          CID2ProcessorResolver* resolver);
    SWGSSeqInfo ResolveAcc(const CTextseq_id& id,
                           CID2ProcessorResolver* resolver);
    SWGSSeqInfo Resolve(const CSeq_id& id,
                        CID2ProcessorResolver* resolver);

    enum EAllowSeqType {
        fAllow_master   = 1<<0,
        fAllow_contig   = 1<<1,
        fAllow_scaffold = 1<<2,
        fAllow_protein  = 1<<3,
        fAllow_na       = fAllow_contig|fAllow_scaffold,
        fAllow_aa       = fAllow_protein
    };
    typedef int TAllowSeqType;
    SWGSSeqInfo ResolveWGSAcc(const string& acc,
                              const CTextseq_id& id,
                              TAllowSeqType allow_seq_type);
    SWGSSeqInfo ResolveProtAcc(const CTextseq_id& id,
                               CID2ProcessorResolver* resolver);
    SWGSSeqInfo& GetRootSeq(SWGSSeqInfo& seq);
    bool IsValidRowId(SWGSSeqInfo& seq);
    bool IsCorrectVersion(SWGSSeqInfo& seq, int version);

    // get Seq-id
    CRef<CSeq_id> GetAccVer(SWGSSeqInfo& seq);
    CRef<CSeq_id> GetGeneral(SWGSSeqInfo& seq);
    TGi GetGi(SWGSSeqInfo& seq);
    void GetSeqIds(SWGSSeqInfo& seq, list<CRef<CSeq_id> >& ids);

    // get various seq info
    string GetLabel(SWGSSeqInfo& seq);
    int GetTaxId(SWGSSeqInfo& seq);
    int GetHash(SWGSSeqInfo& seq);
    TSeqPos GetLength(SWGSSeqInfo& seq);
    CSeq_inst::TMol GetType(SWGSSeqInfo& seq);
    NCBI_gb_state GetGBState(SWGSSeqInfo& seq);
    int GetID2BlobState(SWGSSeqInfo& seq);
    int GetBioseqState(SWGSSeqInfo& seq);

    typedef int TChunkId;

    CRef<CAsnBinData> GetObject(SWGSSeqInfo& seq);
    CRef<CAsnBinData> GetChunk(SWGSSeqInfo& seq0, TChunkId chunk_id);

    // conversion to/from blob id
    SWGSSeqInfo ResolveBlobId(const CID2_Blob_Id& id);
    CID2_Blob_Id& GetBlobId(SWGSSeqInfo& id);

    // opening WGS files (with caching)
    CWGSDb GetWGSDb(const string& prefix);
    CWGSDb& GetWGSDb(SWGSSeqInfo& seq);

    // WGS iterators
    void ResetIteratorCache(SWGSSeqInfo& seq);
    CWGSSeqIterator& GetContigIterator(SWGSSeqInfo& seq);
    CWGSScaffoldIterator& GetScaffoldIterator(SWGSSeqInfo& seq);
    CWGSProteinIterator& GetProteinIterator(SWGSSeqInfo& seq);
    
    void SetBlobState(CID2WGSContext& context,
                      CID2_Reply& main_reply,
                      int blob_state) const;

    bool ExcludedBlob(SWGSSeqInfo& seq,
                      const CID2_Request_Get_Blob_Info& request);
    bool GetCompress(CID2WGSContext& context,
                     const SWGSSeqInfo& seq,
                     const CSeq_entry& entry) const;
    bool GetCompress(CID2WGSContext& context,
                     const SWGSSeqInfo& seq,
                     const CID2S_Split_Info& split) const;
    bool GetCompress(CID2WGSContext& context,
                     const SWGSSeqInfo& seq,
                     TChunkId chunk_id,
                     const CID2S_Chunk& chunk) const;
    bool GetCompress(CID2WGSContext& context,
                     const SWGSSeqInfo& seq,
                     TChunkId chunk_id,
                     const CAsnBinData& obj) const;
    void WriteData(CID2_Reply_Data& data,
                   const CSerialObject& obj,
                   bool compress) const;
    void WriteData(CID2_Reply_Data& data,
                   const CAsnBinData& obj,
                   bool compress) const;
    void WriteData(CID2WGSContext& context,
                   CID2_Reply_Data& data,
                   const SWGSSeqInfo& seq,
                   TChunkId chunk_id,
                   const CAsnBinData& obj) const;
    void WriteData(CID2WGSContext& context,
                   CID2_Reply_Data& data,
                   const SWGSSeqInfo& seq,
                   const CSeq_entry& obj) const;
    void WriteData(CID2WGSContext& context,
                   CID2_Reply_Data& data,
                   const SWGSSeqInfo& seq,
                   const CID2S_Split_Info& obj) const;
    void WriteData(CID2WGSContext& context,
                   CID2_Reply_Data& data,
                   const SWGSSeqInfo& seq,
                   TChunkId chunk_id,
                   const CID2S_Chunk& obj) const;
    
    typedef limited_size_map<string, CWGSDb> TWGSDbCache;

    CRef<CWGSResolver> GetWGSResolver(CID2ProcessorResolver* resolver);

    TReplies DoProcessSomeRequests(CID2WGSContext& context,
                                   CID2_Request_Packet& packet,
                                   CID2ProcessorResolver* resolver);
private:
    CMutex m_Mutex;
    CVDBMgr m_Mgr;
    CRef<CWGSResolver> m_Resolver;
    TWGSDbCache m_WGSDbCache;
    unsigned m_UpdateDelay;
    CRef<CThreadNonStop> m_UpdateThread;
    CID2WGSContext m_InitialContext;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__LOADER__WGS__IMPL__ID2WGS__HPP
