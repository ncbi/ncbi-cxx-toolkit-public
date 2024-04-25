#ifndef WGS_CLIENT__HPP
#define WGS_CLIENT__HPP

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
 * File Description: client for loading data from WGS
 *
 */

#include "psgs_request.hpp"
#include "timing.hpp"
#include <util/thread_nonstop.hpp>
#include <util/limited_size_map.hpp>
#include <objects/general/general__.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/sra/vdbcache.hpp>


BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);
class CSeq_id;
class CSeq_entry;
class CID2_Blob_Id;
class CID2S_Split_Info;
class CWGSResolver;
class CAsnBinData;
END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(wgs);


struct SWGSProcessor_Config
{
    enum ECompressData {
        eCompressData_never,
        eCompressData_some, // if it's benefitial
        eCompressData_always
    };

    size_t m_CacheSize = 100;
    unsigned m_IndexUpdateDelay = 600;
    unsigned m_FileReopenTime = 3600;
    unsigned m_FileRecheckTime = 600;
    ECompressData m_CompressData = eCompressData_some;
};


struct SWGSData
{
    typedef SPSGS_ResolveRequest::TPSGS_BioseqIncludeData TBioseqInfoFlags;

    enum EPSGBioseqState {
        eDead     =  0,
        eSought   =  1,
        eReserved =  5,
        eMerged   =  7,
        eLive     = 10
    };
    
    int GetID2BlobState(void) const { return m_Id2BlobState; }
    int GetPSGBioseqState() const;
    static bool IsForbidden(int id2_blob_state);
    bool IsForbidden() const { return IsForbidden(m_Id2BlobState); }

    string                          m_BlobId;
    CRef<objects::CID2_Blob_Id>     m_Id2BlobId;
    int                             m_Id2BlobState = 0;
    TBioseqInfoFlags                m_BioseqInfoFlags = 0;
    shared_ptr<CBioseqInfoRecord>   m_BioseqInfo;
    CRef<objects::CAsnBinData>      m_Data;
    int                             m_SplitVersion;
    bool                            m_Excluded = false;
    bool                            m_Compress = false;
    psg_time_point_t                m_Start = psg_clock_t::now();
};


class CWGSClient
{
public:
    class CWGSDbInfo : public CObject {
    public:
        objects::CWGSDb m_WGSDb;
    };
    typedef CVDBCacheWithExpiration TWGSDbCache;
    typedef vector<string> TBlobIds;

    struct SWGSSeqInfo {
        SWGSSeqInfo(void)
            : m_IsWGS(false),
              m_ValidWGS(false),
              m_NoRootSeq(false),
              m_SeqType('\0'),
              m_RowDigits(0),
              m_RowId(0),
              m_Version(-1)
            {
            }

        DECLARE_OPERATOR_BOOL(m_ValidWGS);

        bool IsContig(void) const { return m_SeqType == '\0' && m_RowId != 0; }
        bool IsMaster(void) const { return m_SeqType == '\0' && m_RowId == 0; }
        bool IsScaffold(void) const { return m_SeqType == 'S'; }
        bool IsProtein(void) const { return m_SeqType == 'P'; }

        void SetMaster(void) { m_SeqType = '\0'; m_RowId = 0; }
        void SetContig(void) { m_SeqType = '\0'; }
        void SetScaffold(void) { m_SeqType = 'S'; }
        void SetProtein(void) { m_SeqType = 'P'; }

        // parameters
        string                          m_WGSAcc;
        bool                            m_IsWGS;
        bool                            m_ValidWGS;
        bool                            m_NoRootSeq;
        char                            m_SeqType;
        Uint1                           m_RowDigits;
        objects::TVDBRowId              m_RowId;
        int                             m_Version; // for contigs
        // cached objects
        objects::CWGSDb                 m_WGSDb;
        objects::CWGSSeqIterator        m_ContigIter;
        objects::CWGSScaffoldIterator   m_ScaffoldIter;
        objects::CWGSProteinIterator    m_ProteinIter;
        CRef<objects::CID2_Blob_Id>     m_BlobId;
        AutoPtr<SWGSSeqInfo>            m_RootSeq;
    };

    CWGSClient(const SWGSProcessor_Config& config);
    ~CWGSClient(void);

    CRef<objects::CWGSResolver> GetWGSResolver(void);

    bool CanProcessRequest(CPSGS_Request& request);
    shared_ptr<SWGSData> ResolveSeqId(const objects::CSeq_id& seq_id);
    shared_ptr<SWGSData> GetBlobBySeqId(const objects::CSeq_id& seq_id, const TBlobIds& excluded);
    shared_ptr<SWGSData> GetBlobByBlobId(const string& blob_id);
    shared_ptr<SWGSData> GetChunk(const string& id2info, int64_t chunk_id);

public:
    enum EEnabledFlags {
        fEnabledWGS = 1<<0,
        fEnabledSNP = 1<<1,
        fEnabledCDD = 1<<2,
        fEnabledAllAnnot = fEnabledSNP | fEnabledCDD,
        fEnabledAll = fEnabledWGS | fEnabledSNP | fEnabledCDD
    };
    typedef int TEnabledFlags;
    typedef int TID2SplitVersion;
    typedef int TID2BlobVersion;
    struct SParsedId2Info
    {
        DECLARE_OPERATOR_BOOL_REF(tse_id);

        CRef<CID2_Blob_Id> tse_id;
        TID2SplitVersion split_version;
    };
    static CRef<CID2_Blob_Id> ParsePSGBlobId(const SPSGS_BlobId& blob_id);
    static SParsedId2Info ParsePSGId2Info(const string& idsss2_info);
    static bool CanBeWGS(int seq_id_type, const string& seq_id);
    static string GetPSGBlobId(const CID2_Blob_Id& blob_id);
    static void SetSeqId(CSeq_id& id, int seq_id_type, const string& seq_id);
    static bool IsOSGBlob(const CID2_Blob_Id& blob_id);

private:
    enum EAllowSeqType {
        fAllow_master   = 1<<0,
        fAllow_contig   = 1<<1,
        fAllow_scaffold = 1<<2,
        fAllow_protein  = 1<<3,
        fAllow_na       = fAllow_contig|fAllow_scaffold,
        fAllow_aa       = fAllow_protein
    };
    typedef int TAllowSeqType;

    objects::CWGSDb GetWGSDb(const string& prefix);
    objects::CWGSDb& GetWGSDb(SWGSSeqInfo& seq);

    // WGS iterators
    void ResetIteratorCache(SWGSSeqInfo& seq);
    objects::CWGSSeqIterator& GetContigIterator(SWGSSeqInfo& seq);
    objects::CWGSScaffoldIterator& GetScaffoldIterator(SWGSSeqInfo& seq);
    objects::CWGSProteinIterator& GetProteinIterator(SWGSSeqInfo& seq);

    SWGSSeqInfo& GetRootSeq(SWGSSeqInfo& seq0);
    bool IsValidRowId(SWGSSeqInfo& seq);
    bool IsCorrectVersion(SWGSSeqInfo& seq, int version);

    bool HasSpecialState(SWGSSeqInfo& seq, NCBI_gb_state special_state);
    bool HasMigrated(SWGSSeqInfo& seq);

    SWGSSeqInfo Resolve(const objects::CSeq_id& id, bool skip_lookup = false);
    SWGSSeqInfo ResolveGeneral(const objects::CDbtag& dbtag, bool skip_lookup = false);
    SWGSSeqInfo ResolveGi(TGi gi, bool skip_lookup = false);
    SWGSSeqInfo ResolveAcc(const objects::CTextseq_id& id, bool skip_lookup = false);
    SWGSSeqInfo ResolveProtAcc(const objects::CTextseq_id& id, bool skip_lookup = false);
    SWGSSeqInfo ResolveWGSAcc(const string& acc,
                              const objects::CTextseq_id& id,
                              TAllowSeqType allow_seq_type,
                              bool skip_lookup = false);
    
    SWGSSeqInfo ResolveBlobId(const objects::CID2_Blob_Id& id,
                              bool skip_lookup = false);
    objects::CID2_Blob_Id& GetBlobId(SWGSSeqInfo& id);
    int GetID2BlobState(SWGSSeqInfo& seq);
    NCBI_gb_state GetGBState(SWGSSeqInfo& seq0);

    CRef<objects::CSeq_id> GetAccVer(SWGSSeqInfo& seq);
    CRef<objects::CSeq_id> GetGeneral(SWGSSeqInfo& seq);
    TGi GetGi(SWGSSeqInfo& seq);
    void GetSeqIds(SWGSSeqInfo& seq, list<CRef<objects::CSeq_id> >& ids);

    void GetBioseqInfo(shared_ptr<SWGSData>& data, SWGSSeqInfo& seq);
    void GetWGSData(shared_ptr<SWGSData>& data, SWGSSeqInfo& seq0);

    bool GetCompress(SWGSProcessor_Config::ECompressData comp,
                     const SWGSSeqInfo& seq,
                     const objects::CAsnBinData& data) const;

    void x_RegisterTiming(psg_time_point_t start,
                          EPSGOperation operation,
                          EPSGOperationStatus status);
    
    SWGSProcessor_Config m_Config;
    objects::CVDBMgr m_Mgr;
    CFastMutex m_ResolverMutex;
    CRef<objects::CWGSResolver> m_Resolver;
    TWGSDbCache m_WGSDbCache;
    CRef<CThreadNonStop> m_IndexUpdateThread;
};


END_NAMESPACE(wgs);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // CDD_CLIENT__HPP
