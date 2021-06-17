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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CPSGDataLoader_Impl;

class NCBI_XLOADER_GENBANK_EXPORT CPSGDataLoader : public CGBDataLoader
{
public:
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const CGBLoaderParams& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const TParamTree& param_tree,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh) override;
    virtual TBlobId GetBlobIdFromString(const string& str) const override;

    virtual bool CanGetBlobById(void) const override;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id) override;

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice) override;

    virtual TTSE_LockSet GetOrphanAnnotRecordsNA(const CSeq_id_Handle& idh,
        const SAnnotSelector* sel,
        TProcessedNAs* processed_nas) override;
    virtual TTSE_LockSet GetExternalAnnotRecordsNA(const CSeq_id_Handle& idh,
        const SAnnotSelector* sel,
        TProcessedNAs* processed_nas) override;
    virtual TTSE_LockSet GetExternalAnnotRecordsNA(const CBioseq_Info& bioseq,
        const SAnnotSelector* sel,
        TProcessedNAs* processed_nas) override;

    virtual void GetChunk(TChunk chunk) override;
    virtual void GetChunks(const TChunkSet& chunks) override;

    virtual void GetBlobs(TTSE_LockSets& tse_sets) override;

    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids) override;
    virtual SAccVerFound GetAccVerFound(const CSeq_id_Handle& idh) override;
    virtual SGiFound GetGiFound(const CSeq_id_Handle& idh) override;
    virtual TTaxId GetTaxId(const CSeq_id_Handle& idh) override;
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh) override;
    virtual SHashFound GetSequenceHashFound(const CSeq_id_Handle& idh) override;
    virtual STypeFound GetSequenceTypeFound(const CSeq_id_Handle& idh) override;
    virtual int GetSequenceState(const CSeq_id_Handle& idh) override;

    virtual void DropTSE(CRef<CTSE_Info> tse_info) override;

    virtual TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh) override;
    virtual TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh,
        const string& named_acc) override;
    virtual bool HaveCache(TCacheType cache_type = fCache_Any) override
    {
        return false;
    }
    virtual void PurgeCache(TCacheType            cache_type,
        time_t                access_timeout = 0) override {}
    virtual void CloseCache(void) override {}

    virtual void GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret) override;
    virtual void GetGis(const TIds& ids, TLoaded& loaded, TGis& ret) override;
    /*
    void GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret) override;
    void GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret) override;
    void GetSequenceStates(const TIds& ids, TLoaded& loaded, TSequenceStates& ret) override;
    void GetSequenceHashes(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known) override;
    void GetSequenceLengths(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret) override;
    void GetSequenceTypes(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret) override;
    */

private:
    typedef CParamLoaderMaker<CPSGDataLoader, CGBLoaderParams> TMaker;
    friend class CParamLoaderMaker<CPSGDataLoader, CGBLoaderParams>;

    static TRegisterLoaderInfo ConvertRegInfo(const TMaker::TRegisterInfo& info);

    // default constructor
    CPSGDataLoader(void);
    // parametrized constructor
    CPSGDataLoader(const string& loader_name, const CGBLoaderParams& params);
    ~CPSGDataLoader(void);

    CRef<CPSGDataLoader_Impl> m_Impl;
};


END_SCOPE(objects)

END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER__HPP
