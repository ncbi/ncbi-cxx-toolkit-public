#ifndef GBLOADER__HPP_INCLUDED
#define GBLOADER__HPP_INCLUDED

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

#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objtools/data_loaders/genbank/gbload_util.hpp>
#include <objtools/data_loaders/genbank/cache_manager.hpp>

#include <util/cache/icache.hpp>

#define GENBANK_NEW_READER_WRITER

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CReadDispatcher;
class CReader;
class CWriter;
class CSeqref;
class CBlob_id;
class CHandleRange;
class CSeq_entry;
class CLoadInfoSeq_ids;
class CLoadInfoBlob_ids;
class CLoadInfoBlobState;
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
// GBDataLoader
//

class CGBReaderRequestResult;


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


// Parameter names used by loader factory

class NCBI_XLOADER_GENBANK_EXPORT CGBLoaderParams
{
public:
    // possible parameters
    typedef string TReaderName;
    typedef CReader* TReaderPtr;
    typedef TPluginManagerParamTree TParamTree;
    typedef const TPluginManagerParamTree* TParamTreePtr;
    enum EPreopenConnection {
        ePreopenNever,
        ePreopenAlways,
        ePreopenByConfig
    };

    // no options
    CGBLoaderParams(void);
    // one option
    CGBLoaderParams(const string& reader_name);
    CGBLoaderParams(TReaderPtr reader_ptr);
    CGBLoaderParams(TParamTreePtr param_tree);
    CGBLoaderParams(EPreopenConnection preopen);

    ~CGBLoaderParams(void);
    // copy
    CGBLoaderParams(const CGBLoaderParams&);
    CGBLoaderParams& operator=(const CGBLoaderParams&);

    void SetReaderName(const string& reader_name)
        {
            m_ReaderName = reader_name;
        }
    const string& GetReaderName(void) const
        {
            return m_ReaderName;
        }

    void SetReaderPtr(CReader* reader_ptr);
    CReader* GetReaderPtr(void) const;

    void SetParamTree(const TParamTree* params);
    const TParamTree* GetParamTree(void) const
        {
            return m_ParamTree;
        }
    
    void SetPreopenConnection(EPreopenConnection preopen = ePreopenAlways)
        {
            m_Preopen = preopen;
        }
    EPreopenConnection GetPreopenConnection(void) const
        {
            return m_Preopen;
        }

private:
    string m_ReaderName;
    CRef<CReader> m_ReaderPtr;
    const TParamTree* m_ParamTree;
    EPreopenConnection m_Preopen;
};

class NCBI_XLOADER_GENBANK_EXPORT CGBDataLoader : public CDataLoader
{
public:
    // typedefs from CReader
    typedef unsigned                  TConn;
    typedef CBlob_id                  TRealBlobId;
    typedef set<string>               TNamedAnnotNames;

    virtual ~CGBDataLoader(void);

    virtual void DropTSE(CRef<CTSE_Info> tse_info);

    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    virtual CSeq_id_Handle GetAccVer(const CSeq_id_Handle& idh);
    virtual TGi GetGi(const CSeq_id_Handle& idh);
    virtual string GetLabel(const CSeq_id_Handle& idh);
    virtual int GetTaxId(const CSeq_id_Handle& idh);
    virtual int GetSequenceState(const CSeq_id_Handle& idh);

    virtual void GetAccVers(const TIds& ids, TLoaded& loader, TIds& ret);
    virtual void GetGis(const TIds& ids, TLoaded& loader, TGis& ret);
    virtual void GetLabels(const TIds& ids, TLoaded& loader, TLabels& ret);
    virtual void GetTaxIds(const TIds& ids, TLoaded& loader, TTaxIds& ret);
    //virtual void GetSequenceStates(const TIds& ids, TLoaded& loader,
    //                               TSequenceStates& ret);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);
    virtual TTSE_LockSet GetDetailedRecords(const CSeq_id_Handle& idh,
                                            const SRequestDetails& details);
    virtual TTSE_LockSet GetExternalRecords(const CBioseq_Info& bioseq);
    virtual TTSE_LockSet GetExternalAnnotRecords(const CSeq_id_Handle& idh,
                                                 const SAnnotSelector* sel);
    virtual TTSE_LockSet GetExternalAnnotRecords(const CBioseq_Info& bioseq,
                                                 const SAnnotSelector* sel);
    virtual TTSE_LockSet GetOrphanAnnotRecords(const CSeq_id_Handle& idh,
                                               const SAnnotSelector* sel);

    virtual void GetChunk(TChunk chunk);
    virtual void GetChunks(const TChunkSet& chunks);

    virtual void GetBlobs(TTSE_LockSets& tse_sets);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobId GetBlobIdFromString(const string& str) const;
    TBlobId GetBlobIdFromSatSatKey(int sat,
                                   int sat_key,
                                   int sub_sat = 0) const;

    virtual TBlobVersion GetBlobVersion(const TBlobId& id);
    bool CanGetBlobById(void) const;
    TTSE_Lock GetBlobById(const TBlobId& id);

    // Create GB loader and register in the object manager if
    // no loader with the same name is registered yet.
    typedef SRegisterLoaderInfo<CGBDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CReader* reader = 0,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(CReader* reader = 0);

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
    static string GetLoaderNameFromArgs(const string& reader_name);

    // Setup loader using param tree. If tree is null or failed to find params,
    // use environment or select default reader.
    typedef TPluginManagerParamTree TParamTree;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const TParamTree& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const TParamTree& params);

    // Setup loader using param tree. If tree is null or failed to find params,
    // use environment or select default reader.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const CGBLoaderParams& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const CGBLoaderParams& params);

    virtual CConstRef<CSeqref> GetSatSatkey(const CSeq_id_Handle& idh);
    CConstRef<CSeqref> GetSatSatkey(const CSeq_id& id);

    //bool LessBlobId(const TBlobId& id1, const TBlobId& id2) const;
    //string BlobIdToString(const TBlobId& id) const;

    virtual TTSE_Lock ResolveConflict(const CSeq_id_Handle& handle,
                                      const TTSE_LockSet& tse_set);

    virtual void GC(void);

    virtual TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh);
    virtual TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh,
                                                     const string& named_acc);

    const TRealBlobId& GetRealBlobId(const TBlobId& blob_id) const;
    const TRealBlobId& GetRealBlobId(const CTSE_Info& tse_info) const;

    CRef<CLoadInfoSeq_ids> GetLoadInfoSeq_ids(const string& key);
    CRef<CLoadInfoSeq_ids> GetLoadInfoSeq_ids(const CSeq_id_Handle& key);
    typedef pair<CSeq_id_Handle, string>                      TLoadKeyBlob_ids;
    CRef<CLoadInfoBlob_ids> GetLoadInfoBlob_ids(const TLoadKeyBlob_ids& key);
    CRef<CLoadInfoBlobState> GetLoadInfoBlobState(const CBlob_id& key);

    // params modifying access methods
    // argument params should be not-null
    // returned value is not null - subnode will be created if necessary
    static TParamTree* GetParamsSubnode(TParamTree* params,
                                        const string& subnode_name);
    static TParamTree* GetLoaderParams(TParamTree* params);
    static TParamTree* GetReaderParams(TParamTree* params,
                                       const string& reader_name);
    static void SetParam(TParamTree* params,
                         const string& param_name,
                         const string& param_value);
    // params non-modifying access methods
    // argument params may be null
    // returned value may be null if params argument is null
    // or if subnode is not found
    static const TParamTree* GetParamsSubnode(const TParamTree* params,
                                              const string& subnode_name);
    static const TParamTree* GetLoaderParams(const TParamTree* params);
    static const TParamTree* GetReaderParams(const TParamTree* params,
                                             const string& reader_name);
    static string GetParam(const TParamTree* params,
                           const string& param_name);

    CReadDispatcher& GetDispatcher(void);

    enum ECacheType {
        fCache_Id   = CGBReaderCacheManager::fCache_Id,
        fCache_Blob = CGBReaderCacheManager::fCache_Blob,
        fCache_Any  = CGBReaderCacheManager::fCache_Any
    };
    typedef CGBReaderCacheManager::TCacheType TCacheType;
    bool HaveCache(TCacheType cache_type = fCache_Any);
    void PurgeCache(TCacheType            cache_type,
                    time_t                access_timeout = 0,
                    ICache::EKeepVersions keep_last_ver =
                    ICache::eDropAll);
    void CloseCache(void);

    // expiration timout in seconds, must be positive
    typedef Uint4 TExpirationTimeout;
    TExpirationTimeout GetIdExpirationTimeout(void) const
        {
            return m_IdExpirationTimeout;
        }
    void SetIdExpirationTimeout(TExpirationTimeout timeout);

protected:
    friend class CGBReaderRequestResult;

    TBlobContentsMask x_MakeContentMask(EChoice choice) const;
    TBlobContentsMask x_MakeContentMask(const SRequestDetails& details) const;

    TTSE_LockSet x_GetRecords(const CSeq_id_Handle& idh,
                              TBlobContentsMask sr_mask,
                              const SAnnotSelector* sel);

private:
    typedef CParamLoaderMaker<CGBDataLoader, const CGBLoaderParams&> TGBMaker;
    friend class CParamLoaderMaker<CGBDataLoader, const CGBLoaderParams&>;

    CGBDataLoader(const string&     loader_name,
                  const CGBLoaderParams& params);

    // Find GB loader params in the tree or return the original tree.
    const TParamTree* x_GetLoaderParams(const TParamTree* params) const;
    // Get reader name from the GB loader params.
    string x_GetReaderName(const TParamTree* params) const;

    typedef CLoadInfoMap<string, CLoadInfoSeq_ids>            TLoadMapSeq_ids;
    typedef CLoadInfoMap<CSeq_id_Handle, CLoadInfoSeq_ids>    TLoadMapSeq_ids2;
    typedef CLoadInfoMap<TLoadKeyBlob_ids, CLoadInfoBlob_ids> TLoadMapBlob_ids;
    typedef CLoadInfoMap<CBlob_id, CLoadInfoBlobState>        TLoadMapBlobStates;

    CInitMutexPool          m_MutexPool;

    CRef<CReadDispatcher>   m_Dispatcher;

    TLoadMapSeq_ids         m_LoadMapSeq_ids;
    TLoadMapSeq_ids2        m_LoadMapSeq_ids2;
    TLoadMapBlob_ids        m_LoadMapBlob_ids;
    TLoadMapBlobStates      m_LoadMapBlobStates;

    //CTimer                  m_Timer;

    // Information about all available caches
    CGBReaderCacheManager   m_CacheManager;

    TExpirationTimeout      m_IdExpirationTimeout;

    //
    // private code
    //

    void x_CreateDriver(const CGBLoaderParams& params);

    string GetReaderName(const TParamTree* params) const;
    string GetWriterName(const TParamTree* params) const;
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
    CGBDataLoader(const CGBDataLoader&);
    CGBDataLoader& operator=(const CGBDataLoader&);
};


NCBI_PARAM_DECL(string, GENBANK, LOADER_METHOD);

END_SCOPE(objects)


extern "C"
{

NCBI_XLOADER_GENBANK_EXPORT
void NCBI_EntryPoint_DataLoader_GB(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_GENBANK_EXPORT
void NCBI_EntryPoint_xloader_genbank(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif
