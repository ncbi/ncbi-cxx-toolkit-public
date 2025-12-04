#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER_IMPL__HPP

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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: PSG data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objtools/data_loaders/genbank/psg_loader.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <util/incr_time.hpp>
#include <memory>
#include <vector>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_SCOPE

class CThreadPool;

BEGIN_SCOPE(objects)

BEGIN_NAMESPACE(psgl);

struct SPsgBioseqInfo;
struct SPsgBlobInfo;
class CPSGCaches;
class CPSGL_Dispatcher;
class CPSGL_QueueGuard;
class CPSGL_ResultGuard;
class CPSGL_Processor;

END_NAMESPACE(psgl);


class CPSGDataLoader_Impl : public CObject
{
public:
    explicit CPSGDataLoader_Impl(const CGBLoaderParams& params);
    ~CPSGDataLoader_Impl(void) override;

    typedef CDataLoader::TIds TIds;
    typedef CDataLoader::TBulkIds TBulkIds;
    typedef CDataLoader::TGis TGis;
    typedef CDataLoader::TTaxIds TTaxIds;
    typedef CDataLoader::TSequenceLengths TSequenceLengths;
    typedef CDataLoader::TSequenceTypes TSequenceTypes;
    typedef CDataLoader::TLabels TLabels;
    typedef CDataLoader::TSequenceStates TSequenceStates;
    typedef CDataLoader::TSequenceHashes TSequenceHashes;
    typedef CDataLoader::THashKnown THashKnown;
    typedef CDataLoader::TLoaded TLoaded;
    typedef CDataLoader::TTSE_LockSets TTSE_LockSets;

    void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    void GetIdsOnce(const CSeq_id_Handle& idh, TIds& ids);
    CDataLoader::SGiFound GetGi(const CSeq_id_Handle& idh);
    CDataLoader::SGiFound GetGiOnce(const CSeq_id_Handle& idh);
    CDataLoader::SAccVerFound GetAccVer(const CSeq_id_Handle& idh);
    CDataLoader::SAccVerFound GetAccVerOnce(const CSeq_id_Handle& idh);
    TTaxId GetTaxId(const CSeq_id_Handle& idh);
    TTaxId GetTaxIdOnce(const CSeq_id_Handle& idh);
    void GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret);
    void GetTaxIdsOnce(const TIds& ids, TLoaded& loaded, TTaxIds& ret);
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    TSeqPos GetSequenceLengthOnce(const CSeq_id_Handle& idh);
    CDataLoader::SHashFound GetSequenceHash(const CSeq_id_Handle& idh);
    CDataLoader::SHashFound GetSequenceHashOnce(const CSeq_id_Handle& idh);
    CDataLoader::STypeFound GetSequenceType(const CSeq_id_Handle& idh);
    CDataLoader::STypeFound GetSequenceTypeOnce(const CSeq_id_Handle& idh);
    int GetSequenceState(CDataSource* data_source, const CSeq_id_Handle& idh);
    int GetSequenceStateOnce(CDataSource* data_source, const CSeq_id_Handle& idh);

    CDataLoader::TTSE_LockSet GetRecords(CDataSource* data_source,
                                         const CSeq_id_Handle& idh,
                                         CDataLoader::EChoice choice);
    CDataLoader::TTSE_LockSet GetRecordsOnce(CDataSource* data_source,
                                             const CSeq_id_Handle& idh,
                                             CDataLoader::EChoice choice);
    CConstRef<CPsgBlobId> GetBlobId(const CSeq_id_Handle& idh);
    CConstRef<CPsgBlobId> GetBlobIdOnce(const CSeq_id_Handle& idh);
    CTSE_Lock GetBlobById(CDataSource* data_source,
                          const CPsgBlobId& blob_id);
    CTSE_Lock GetBlobByIdOnce(CDataSource* data_source,
                              const CPsgBlobId& blob_id);
    void LoadChunk(CDataSource* data_source,
                   CTSE_Chunk_Info& chunk_info);
    void LoadChunks(CDataSource* data_source,
                    const CDataLoader::TChunkSet& chunks);
    void LoadChunksOnce(CDataSource* data_source,
                        const CDataLoader::TChunkSet& chunks);

    void GetBlobs(CDataSource* data_source, TTSE_LockSets& tse_sets);
    typedef set<CSeq_id_Handle> TLoadedSeqIds;
    void GetBlobsOnce(CDataSource* data_source, TLoadedSeqIds& loaded, TTSE_LockSets& tse_sets);

    typedef CDataLoader::TSeqIdSets TSeqIdSets;
    typedef CDataLoader::TCDD_Locks TCDD_Locks;
    void GetCDDAnnots(CDataSource* data_source,
                      const TSeqIdSets& id_sets, TLoaded& loaded, TCDD_Locks& ret);
    void GetCDDAnnotsOnce(CDataSource* data_source,
                          const TSeqIdSets& id_sets, TLoaded& loaded, TCDD_Locks& ret);

    CDataLoader::TTSE_LockSet GetAnnotRecordsNA(CDataSource* data_source,
                                                const TIds& ids,
                                                const SAnnotSelector* sel,
                                                CDataLoader::TProcessedNAs* processed_nas);
    CDataLoader::TTSE_LockSet GetAnnotRecordsNAOnce(CDataSource* data_source,
                                                    const TIds& ids,
                                                    const SAnnotSelector* sel,
                                                    CDataLoader::TProcessedNAs* processed_nas);

    void DropTSE(const CPsgBlobId& blob_id);

    void GetBulkIds(const TIds& ids, TLoaded& loaded, TBulkIds& ret);
    void GetBulkIdsOnce(const TIds& ids, TLoaded& loaded, TBulkIds& ret);
    void GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret);
    void GetAccVersOnce(const TIds& ids, TLoaded& loaded, TIds& ret);
    void GetGis(const TIds& ids, TLoaded& loaded, TGis& ret);
    void GetGisOnce(const TIds& ids, TLoaded& loaded, TGis& ret);
    void GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret);
    void GetLabelsOnce(const TIds& ids, TLoaded& loaded, TLabels& ret);
    void GetSequenceLengths(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret);
    void GetSequenceLengthsOnce(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret);
    void GetSequenceTypes(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret);
    void GetSequenceTypesOnce(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret);
    void GetSequenceStates(CDataSource* data_source, const TIds& ids, TLoaded& loaded, TSequenceStates& ret);
    void GetSequenceStatesOnce(CDataSource* data_source, const TIds& ids, TLoaded& loaded, TSequenceStates& ret);
    void GetSequenceHashes(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known);
    void GetSequenceHashesOnce(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known);

    static void NCBI_XLOADER_GENBANK_EXPORT SetGetBlobByIdShouldFail(bool value);
    static bool NCBI_XLOADER_GENBANK_EXPORT GetGetBlobByIdShouldFail();

    template<class Call>
    typename std::invoke_result<Call>::type
    CallWithRetry(Call&& call,
                  const char* name,
                  int retry_count = 0);
    
private:
    typedef psgl::SPsgBlobInfo SPsgBlobInfo;
    typedef psgl::SPsgBioseqInfo SPsgBioseqInfo;
    typedef psgl::CPSGCaches CPSGCaches;
    typedef psgl::CPSGL_Dispatcher CPSGL_Dispatcher;
    typedef psgl::CPSGL_QueueGuard CPSGL_QueueGuard;
    typedef psgl::CPSGL_ResultGuard CPSGL_ResultGuard;
    typedef psgl::CPSGL_Processor CPSGL_Processor;

    struct SReplyResult {
        CTSE_Lock lock;
        string blob_id;
        shared_ptr<SPsgBlobInfo> blob_info;
    };

    shared_ptr<SPsgBioseqInfo> x_GetBioseqInfo(const CSeq_id_Handle& idh);
    shared_ptr<SPsgBlobInfo> x_GetBlobInfo(CDataSource* data_source, const string& blob_id);
    typedef pair<shared_ptr<SPsgBioseqInfo>,
                 pair<shared_ptr<SPsgBlobInfo>, EPSG_Status>> TBioseqAndBlobInfo;
    TBioseqAndBlobInfo x_GetBioseqAndBlobInfo(CDataSource* data_source, const CSeq_id_Handle& idh);

    typedef vector<shared_ptr<SPsgBioseqInfo>> TBioseqInfos;
    typedef vector<TBioseqAndBlobInfo> TBioseqAndBlobInfos;

    // returns pair(number of loaded infos, first failed processor)
    pair<size_t, CRef<CPSGL_Processor>>
    x_GetBulkBioseqInfo(const TIds& ids,
                        const TLoaded& loaded,
                        TBioseqInfos& ret);
    pair<size_t, CRef<CPSGL_Processor>>
    x_GetBulkBioseqAndBlobInfo(CDataSource* data_source,
                               const TIds& ids,
                               const TLoaded& loaded,
                               TBioseqAndBlobInfos& ret);

    bool x_ReadCDDChunk(CDataSource* data_source,
                        CDataLoader::TChunk chunk,
                        const CPSG_BlobInfo& blob_info,
                        const CPSG_BlobData& blob_data);
    bool x_CheckAnnotCache(const string& name,
                           const TIds& ids,
                           CDataSource* data_source,
                           CDataLoader::TProcessedNAs* processed_nas,
                           CDataLoader::TTSE_LockSet& locks);
    TTaxId x_GetIpgTaxId(const CSeq_id_Handle& idh);
    pair<size_t, CRef<CPSGL_Processor>>
    x_GetIpgTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret);
    int x_MakeSequenceState(const TBioseqAndBlobInfo& info);
    void x_AdjustBlobState(TBioseqAndBlobInfo& info);

    enum EProcessResult {
        eFailed,
        eQueued,
        eCompleted
    };
    // x_CreateBioseqAndBlobInfoRequests() returns:
    //   eCompleted if the data was fully processed from cache
    //   eQueued if requests were created
    EProcessResult x_CreateBioseqAndBlobInfoRequests(CPSGL_QueueGuard& queue,
                                                     const CSeq_id_Handle& idh,
                                                     TBioseqAndBlobInfo& ret,
                                                     size_t id_index = 0);
    // x_ProcessBioseqAndBlobInfoResult() returns:
    //   eCompleted -  processing was successful and complete
    //   eQueued    -  processing was successful but a new request was queued
    //   eFailed    -  processing was unsuccessful, check processor for errors
    EProcessResult x_ProcessBioseqAndBlobInfoResult(CPSGL_QueueGuard& queue,
                                                    const CSeq_id_Handle& idh,
                                                    TBioseqAndBlobInfo& ret,
                                                    const CPSGL_ResultGuard& result);

    class CGetRequests;
    
    CPSG_Request_Biodata::EIncludeData m_TSERequestMode = CPSG_Request_Biodata::eSmartTSE;
    CPSG_Request_Biodata::EIncludeData m_TSERequestModeBulk = CPSG_Request_Biodata::eWholeTSE;
    bool m_AddWGSMasterDescr = true;
    CRef<CPSGL_Dispatcher> m_Dispatcher;
    CRef<CRequestContext> m_RequestContext;
    unique_ptr<CPSGCaches> m_Caches;
    class CPSG_PrefetchCDD_Task;
    CRef<CPSG_PrefetchCDD_Task> m_CDDPrefetchTask;
    bool m_IpgTaxIdEnabled;
    unsigned int m_RetryCount;
    unsigned int m_BulkRetryCount;
    CIncreasingTime m_WaitTime;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER_IMPL__HPP
