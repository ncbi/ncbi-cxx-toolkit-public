#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER__HPP

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
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp> // for HAVE_PSG_CLIENT


#if defined(HAVE_PSG_LOADER) && !defined(HAVE_PSG_CLIENT)
#  undef HAVE_PSG_LOADER
#endif

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CPSGDataLoader_Impl;

class NCBI_XLOADER_GENBANK_EXPORT CPsgBlobId : public CBlobId
{
public:
    explicit CPsgBlobId(const string& id);
    CPsgBlobId(const string& id, bool is_dead);
    CPsgBlobId(const string& id, const string& id2_info);
    virtual ~CPsgBlobId();
    
    const string& ToPsgId() const
    {
        return m_Id;
    }
    
    virtual string ToString(void) const override;
    virtual bool operator<(const CBlobId& id) const override;
    virtual bool operator==(const CBlobId& id) const override;

    const string& GetId2Info() const
    {
        return m_Id2Info;
    }
    void SetId2Info(const string& id2_info)
    {
        m_Id2Info = id2_info;
    }

    const string& GetTSEName() const
    {
        return m_TSEName;
    }
    bool HasTSEName() const
    {
        return !m_TSEName.empty();
    }
    void SetTSEName(const string& name)
    {
        m_TSEName = name;
    }
    
    bool HasBioseqIsDead() const
    {
        return m_HasBioseqIsDead;
    }
    bool BioseqIsDead() const
    {
        return m_BioseqIsDead;
    }
    void ResetBioseqIsDead()
    {
        m_HasBioseqIsDead = false;
        m_BioseqIsDead = false;
    }
    void SetBioseqIsDead(bool is_dead = true)
    {
        m_HasBioseqIsDead = true;
        m_BioseqIsDead = is_dead;
    }
    
    bool GetSatSatkey(int& sat, int& satkey) const;

    static CConstRef<CPsgBlobId> GetPsgBlobId(const CBlobId& blob_id);

private:
    string m_Id; // main blob id
    string m_Id2Info; // needed to load chunks
    string m_TSEName; // needed to restore TSE annot name on re-load by blob id
    bool m_HasBioseqIsDead = false; // needed to restore 'dead' flag on re-load by blob id
    bool m_BioseqIsDead = false;
};


class NCBI_XLOADER_GENBANK_EXPORT CPSGDataLoader : public CGBDataLoader
{
public:
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const CGBLoaderParams& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);

    TBlobId GetBlobIdFromSatSatKey(int sat,
                                   int sat_key,
                                   int sub_sat) const override;
    TBlobId GetBlobId(const CSeq_id_Handle& idh) override;
    TBlobId GetBlobIdFromString(const string& str) const override;

    bool CanGetBlobById(void) const override;
    TTSE_Lock GetBlobById(const TBlobId& blob_id) override;
    vector<TTSE_Lock> GetBlobsById(const vector<TBlobId>& blob_ids) override;

    TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice) override;

    TTSE_LockSet GetOrphanAnnotRecordsNA(const TSeq_idSet& seq_ids,
                                         const SAnnotSelector* sel,
                                         TProcessedNAs* processed_nas) override;
    TTSE_LockSet GetExternalAnnotRecordsNA(const CBioseq_Info& bioseq,
                                           const SAnnotSelector* sel,
                                           TProcessedNAs* processed_nas) override;

    void GetChunk(TChunk chunk) override;
    void GetChunks(const TChunkSet& chunks) override;

    void GetBlobs(TTSE_LockSets& tse_sets) override;

    void GetCDDAnnots(const TBioseq_InfoSet& seq_set, TLoaded& loaded, TCDD_Locks& ret) override;

    void GetIds(const CSeq_id_Handle& idh, TIds& ids) override;
    SAccVerFound GetAccVerFound(const CSeq_id_Handle& idh) override;
    SGiFound GetGiFound(const CSeq_id_Handle& idh) override;
    TTaxId GetTaxId(const CSeq_id_Handle& idh) override;
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh) override;
    SHashFound GetSequenceHashFound(const CSeq_id_Handle& idh) override;
    STypeFound GetSequenceTypeFound(const CSeq_id_Handle& idh) override;
    int GetSequenceState(const CSeq_id_Handle& idh) override;

    void DropTSE(CRef<CTSE_Info> tse_info) override;

    TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh) override;
    TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh,
                                             const string& named_acc) override;
    bool HaveCache(TCacheType) override
    {
        return false;
    }
    void PurgeCache(TCacheType /*cache_type*/,
                    time_t     /*access_timeout*/ = 0) override {}
    void CloseCache(void) override {}

    void GetBulkIds(const TIds& ids, TLoaded& loaded, TBulkIds& ret) override;
    void GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret) override;
    void GetGis(const TIds& ids, TLoaded& loaded, TGis& ret) override;
    void GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret) override;
    void GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret) override;
    void GetSequenceStates(const TIds& ids, TLoaded& loaded, TSequenceStates& ret) override;
    void GetSequenceHashes(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known) override;
    void GetSequenceLengths(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret) override;
    void GetSequenceTypes(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret) override;

    // Global default SNP scale limit.
    static CSeq_id::ESNPScaleLimit GetSNP_Scale_Limit(void);
    static void SetSNP_Scale_Limit(CSeq_id::ESNPScaleLimit value);

private:
    friend class CGBLoaderMaker<CPSGDataLoader>;

    TRealBlobId x_GetRealBlobId(const TBlobId& blob_id) const override;

    // default constructor
    CPSGDataLoader(void);
    // parametrized constructor
    CPSGDataLoader(const string& loader_name, const CGBLoaderParams& params);
    ~CPSGDataLoader(void);

    CRef<CPSGDataLoader_Impl> m_Impl;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER__HPP
