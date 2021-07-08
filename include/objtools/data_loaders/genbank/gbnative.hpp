#ifndef GBNATIVE__HPP_INCLUDED
#define GBNATIVE__HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko,
*          Anton Butanayev
*
*  File Description:
*   Data loader base class for object manager
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbi_param.hpp>

#if !defined(NDEBUG) && defined(DEBUG_SYNC)
// for GBLOG_POST()
# include <corelib/ncbithr.hpp>
#endif

#include <objmgr/data_loader.hpp>

#include <util/mutex_pool.hpp>

#include <objtools/data_loaders/genbank/impl/gbload_util.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objtools/data_loaders/genbank/impl/cache_manager.hpp>

#include <util/cache/icache.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CReadDispatcher;
class CReader;
class CWriter;
class CSeqref;
class CBlob_id;
class CHandleRange;
class CSeq_entry;
class CLoadInfoBlob;

#if !defined(NDEBUG) && defined(DEBUG_SYNC)
#  if defined(NCBI_THREADS)
#    define GBLOG_POST(x) LOG_POST(setw(3) << CThread::GetSelf() << ":: " << x)
#    define GBLOG_POST_X(err_subcode, x)   \
         LOG_POST_X(err_subcode, setw(3) << CThread::GetSelf() << ":: " << x)
#  else
#    define GBLOG_POST(x) LOG_POST("0:: " << x)
#    define GBLOG_POST_X(err_subcode, x) LOG_POST_X(err_subcode, "0:: " << x)
#  endif
#else
#  ifdef DEBUG_SYNC
#    undef DEBUG_SYNC
#  endif
#  define GBLOG_POST(x)
#  define GBLOG_POST_X(err_subcode, x)
#endif

/////////////////////////////////////////////////////////////////////////////////
//
// GBDataLoader_Native
//

class CGBReaderRequestResult;
class CGBInfoManager;


class NCBI_XLOADER_GENBANK_EXPORT CGBReaderCacheManager : public CReaderCacheManager
{
public:
    CGBReaderCacheManager(void) {}

    virtual void RegisterCache(ICache& cache, ECacheType cache_type);
    virtual TCaches& GetCaches(void) { return m_Caches; }
    virtual ICache* FindCache(ECacheType cache_type,
                              const TCacheParams* params);
private:
    TCaches m_Caches;
};


class NCBI_XLOADER_GENBANK_EXPORT CGBDataLoader_Native : public CGBDataLoader
{
public:
    // typedefs from CReader
    typedef unsigned                  TConn;
    typedef CBlob_id                  TRealBlobId;
    typedef set<string>               TNamedAnnotNames;

    virtual ~CGBDataLoader_Native(void);

    virtual void DropTSE(CRef<CTSE_Info> tse_info) override;

    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids) override;
    virtual SAccVerFound GetAccVerFound(const CSeq_id_Handle& idh) override;
    virtual SGiFound GetGiFound(const CSeq_id_Handle& idh) override;
    virtual string GetLabel(const CSeq_id_Handle& idh) override;
    virtual TTaxId GetTaxId(const CSeq_id_Handle& idh) override;
    virtual int GetSequenceState(const CSeq_id_Handle& idh) override;
    virtual SHashFound GetSequenceHashFound(const CSeq_id_Handle& idh) override;
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& sih) override;
    virtual STypeFound GetSequenceTypeFound(const CSeq_id_Handle& sih) override;

    virtual void GetAccVers(const TIds& ids, TLoaded& loader, TIds& ret) override;
    virtual void GetGis(const TIds& ids, TLoaded& loader, TGis& ret) override;
    virtual void GetLabels(const TIds& ids, TLoaded& loader, TLabels& ret) override;
    virtual void GetTaxIds(const TIds& ids, TLoaded& loader, TTaxIds& ret) override;
    virtual void GetSequenceStates(const TIds& ids, TLoaded& loader,
                                   TSequenceStates& ret) override;
    virtual void GetSequenceHashes(const TIds& ids, TLoaded& loader,
                                   TSequenceHashes& ret, THashKnown& known) override;
    virtual void GetSequenceLengths(const TIds& ids, TLoaded& loader,
                                    TSequenceLengths& ret) override;
    virtual void GetSequenceTypes(const TIds& ids, TLoaded& loader,
                                  TSequenceTypes& ret) override;

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice) override;
    virtual TTSE_LockSet GetDetailedRecords(const CSeq_id_Handle& idh,
                                            const SRequestDetails& details) override;
    virtual TTSE_LockSet GetExternalRecords(const CBioseq_Info& bioseq) override;
    virtual TTSE_LockSet GetExternalAnnotRecordsNA(const CSeq_id_Handle& idh,
                                                   const SAnnotSelector* sel,
                                                   TProcessedNAs* processed_nas) override;
    virtual TTSE_LockSet GetExternalAnnotRecordsNA(const CBioseq_Info& bioseq,
                                                   const SAnnotSelector* sel,
                                                   TProcessedNAs* processed_nas) override;
    virtual TTSE_LockSet GetOrphanAnnotRecordsNA(const CSeq_id_Handle& idh,
                                                 const SAnnotSelector* sel,
                                                 TProcessedNAs* processed_nas) override;

    virtual void GetChunk(TChunk chunk) override;
    virtual void GetChunks(const TChunkSet& chunks) override;

    virtual void GetBlobs(TTSE_LockSets& tse_sets) override;

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh) override;
    virtual TBlobId GetBlobIdFromString(const string& str) const override;

    virtual TBlobVersion GetBlobVersion(const TBlobId& id) override;
    virtual bool CanGetBlobById(void) const override;
    virtual TTSE_Lock GetBlobById(const TBlobId& id) override;

    // Create GB loader and register in the object manager if
    // no loader with the same name is registered yet.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CReader* reader = 0,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);

    // Select reader by name. If failed, select default reader.
    // Reader name may be the same as in environment: PUBSEQOS, ID1 etc.
    // Several names may be separated with ":". Empty name or "*"
    // included as one of the names allows to include reader names
    // from environment and registry.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string&   reader_name,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);

    // GBLoader with HUP data included.
    // The reader will be chosen from default configuration,
    // either pubseqos or pubseqos2.
    // The default loader priority will be slightly lower than for main data.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        EIncludeHUP     include_hup,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        EIncludeHUP     include_hup,
        const string& web_cookie,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);

    // GBLoader with HUP data included.
    // The reader can be either pubseqos or pubseqos2.
    // The default loader priority will be slightly lower than for main data.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string&   reader_name, // pubseqos or pubseqos2
        EIncludeHUP     include_hup,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string&   reader_name, // pubseqos or pubseqos2
        EIncludeHUP     include_hup,
        const string& web_cookie,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);

    // Setup loader using param tree. If tree is null or failed to find params,
    // use environment or select default reader.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const TParamTree& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);

    // Setup loader using param tree. If tree is null or failed to find params,
    // use environment or select default reader.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const CGBLoaderParams& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);

    //bool LessBlobId(const TBlobId& id1, const TBlobId& id2) const;
    //string BlobIdToString(const TBlobId& id) const;

    virtual TTSE_Lock ResolveConflict(const CSeq_id_Handle& handle,
                                      const TTSE_LockSet& tse_set) override;

    virtual void GC(void) override;

    virtual TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh) override;
    virtual TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh,
                                                     const string& named_acc) override;

    CReadDispatcher& GetDispatcher(void)
        {
            return *m_Dispatcher;
        }
    CGBInfoManager& GetInfoManager(void)
        {
            return *m_InfoManager;
        }

    enum ECacheType {
        fCache_Id   = CGBReaderCacheManager::fCache_Id,
        fCache_Blob = CGBReaderCacheManager::fCache_Blob,
        fCache_Any  = CGBReaderCacheManager::fCache_Any
    };
    typedef CGBReaderCacheManager::TCacheType TCacheType;
    bool HaveCache(TCacheType cache_type = fCache_Any) override;

    void PurgeCache(TCacheType            cache_type,
                    time_t                access_timeout = 0) override;
    void CloseCache(void) override;

    virtual CObjectManager::TPriority GetDefaultPriority(void) const override;

protected:
    friend class CGBReaderRequestResult;

    TBlobContentsMask x_MakeContentMask(EChoice choice) const;
    TBlobContentsMask x_MakeContentMask(const SRequestDetails& details) const;

    TTSE_LockSet x_GetRecords(const CSeq_id_Handle& idh,
                              TBlobContentsMask sr_mask,
                              const SAnnotSelector* sel,
                              TProcessedNAs* processed_nas = 0);

private:
    friend class CGBDataLoader;
    typedef CParamLoaderMaker<CGBDataLoader_Native, const CGBLoaderParams&> TGBMaker;
    friend class CParamLoaderMaker<CGBDataLoader_Native, const CGBLoaderParams&>;

    TRealBlobId x_GetRealBlobId(const TBlobId& blob_id) const override;

    static TRegisterLoaderInfo ConvertRegInfo(const TGBMaker::TRegisterInfo& info);

    CGBDataLoader_Native(const string&     loader_name,
                         const CGBLoaderParams& params);

    // Find GB loader params in the tree or return the original tree.
    const TParamTree* x_GetLoaderParams(const TParamTree* params) const;
    // Get reader name from the GB loader params.
    string x_GetReaderName(const TParamTree* params) const;

    CInitMutexPool          m_MutexPool;

    CRef<CReadDispatcher>   m_Dispatcher;
    CRef<CGBInfoManager>    m_InfoManager;

    // Information about all available caches
    CGBReaderCacheManager   m_CacheManager;

    //
    // private code
    //

    void x_CreateDriver(const CGBLoaderParams& params);

    pair<string, string> GetReaderWriterName(const TParamTree* params) const;
    bool x_CreateReaders(const string& str,
                         const TParamTree* params,
                         CGBLoaderParams::EPreopenConnection preopen);
    void x_CreateWriters(const string& str, const TParamTree* params);
    CReader* x_CreateReader(const string& names, const TParamTree* params = 0);
    CWriter* x_CreateWriter(const string& names, const TParamTree* params = 0);

    typedef CPluginManager<CReader> TReaderManager;
    typedef CPluginManager<CWriter> TWriterManager;

    static CRef<TReaderManager> x_GetReaderManager(void);
    static CRef<TWriterManager> x_GetWriterManager(void);

private:
    CGBDataLoader_Native(const CGBDataLoader_Native&);
    CGBDataLoader_Native& operator=(const CGBDataLoader_Native&);
};


END_SCOPE(objects)

END_NCBI_SCOPE

#endif
