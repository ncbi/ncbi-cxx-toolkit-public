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

#if !defined(NDEBUG) && defined(DEBUG_SYNC)
// for GBLOG_POST()
# include <corelib/ncbithr.hpp>
#endif

#include <objmgr/data_loader.hpp>

#include <objmgr/impl/mutex_pool.hpp>

#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objtools/data_loaders/genbank/gbload_util.hpp>

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
class CLoadInfoBlob;

#if !defined(NDEBUG) && defined(DEBUG_SYNC)
#  if defined(NCBI_THREADS)
#    define GBLOG_POST(x) LOG_POST(setw(3) << CThread::GetSelf() << ":: " << x)
#  else
#    define GBLOG_POST(x) LOG_POST("0:: " << x)
#  endif
#else
#  ifdef DEBUG_SYNC
#    undef DEBUG_SYNC
#  endif
#  define GBLOG_POST(x)
#endif

/////////////////////////////////////////////////////////////////////////////////
//
// GBDataLoader
//

class CGBReaderRequestResult;

// Parameter names used by loader factory

class NCBI_XLOADER_GENBANK_EXPORT CGBDataLoader : public CDataLoader
{
public:
    // typedefs from CReader
    typedef unsigned                  TConn;

    virtual ~CGBDataLoader(void);

    virtual void DropTSE(CRef<CTSE_Info> tse_info);

    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);
    virtual TTSE_LockSet GetDetailedRecords(const CSeq_id_Handle& idh,
                                            const SRequestDetails& details);
    virtual TTSE_LockSet GetExternalRecords(const CBioseq_Info& bioseq);

    virtual void GetChunk(TChunk chunk);
    virtual void GetChunks(const TChunkSet& chunks);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobVersion GetBlobVersion(const TBlobId& id);
    CBlob_id GetBlobId(const TBlobId& blob_id) const;
    bool CanGetBlobById(void) const;
    TTSE_Lock GetBlobById(const TBlobId& id);

    // Create GB loader and register in the object manager if
    // no loader with the same name is registered yet.
    typedef SRegisterLoaderInfo<CGBDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CReader* driver = 0,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
        static string GetLoaderNameFromArgs(CReader* driver = 0);

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

    virtual CConstRef<CSeqref> GetSatSatkey(const CSeq_id_Handle& idh);
    CConstRef<CSeqref> GetSatSatkey(const CSeq_id& id);

    bool LessBlobId(const TBlobId& id1, const TBlobId& id2) const;
    string BlobIdToString(const TBlobId& id) const;

    virtual TTSE_Lock ResolveConflict(const CSeq_id_Handle& handle,
                                      const TTSE_LockSet& tse_set);

    virtual void GC(void);

    CBlob_id GetBlobId(const CTSE_Info& tse_info) const;

    CRef<CLoadInfoSeq_ids> GetLoadInfoSeq_ids(const string& key);
    CRef<CLoadInfoSeq_ids> GetLoadInfoSeq_ids(const CSeq_id_Handle& key);
    CRef<CLoadInfoBlob_ids> GetLoadInfoBlob_ids(const CSeq_id_Handle& key);

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

protected:
    friend class CGBReaderRequestResult;

    TBlobContentsMask x_MakeContentMask(EChoice choice) const;
    TBlobContentsMask x_MakeContentMask(const SRequestDetails& details) const;

    TTSE_LockSet x_GetRecords(const CSeq_id_Handle& idh,
                              TBlobContentsMask sr_mask);

private:
    typedef CParamLoaderMaker<CGBDataLoader, CReader*>      TReaderPtrMaker;
    typedef CParamLoaderMaker<CGBDataLoader, const string&> TReaderNameMaker;
    typedef CParamLoaderMaker<CGBDataLoader, const TParamTree&> TParamMaker;
    friend class CParamLoaderMaker<CGBDataLoader, CReader*>;
    friend class CParamLoaderMaker<CGBDataLoader, const string&>;
    friend class CParamLoaderMaker<CGBDataLoader, const TParamTree&>;

    CGBDataLoader(const string&     loader_name,
                  CReader*          driver);
    CGBDataLoader(const string&     loader_name,
                  const string&     reader_name);
    CGBDataLoader(const string&     loader_name,
                  const TParamTree& params);

    // Find GB loader params in the tree or return the original tree.
    const TParamTree* x_GetLoaderParams(const TParamTree* params) const;
    // Get reader name from the GB loader params.
    string x_GetReaderName(const TParamTree* params) const;

    typedef CLoadInfoMap<string, CLoadInfoSeq_ids>            TLoadMapSeq_ids;
    typedef CLoadInfoMap<CSeq_id_Handle, CLoadInfoSeq_ids>    TLoadMapSeq_ids2;
    typedef CLoadInfoMap<CSeq_id_Handle, CLoadInfoBlob_ids>   TLoadMapBlob_ids;

    CInitMutexPool          m_MutexPool;

    CRef<CReadDispatcher>   m_Dispatcher;

    TLoadMapSeq_ids         m_LoadMapSeq_ids;
    TLoadMapSeq_ids2        m_LoadMapSeq_ids2;
    TLoadMapBlob_ids        m_LoadMapBlob_ids;

    CTimer                  m_Timer;

    //
    // private code
    //

    void x_CreateDriver(CReader* reader);
    void x_CreateDriver(const string& reader_name);
    void x_CreateDriver(const TParamTree* params);

    bool x_CreateReaders(const TParamTree* params);
    void x_CreateWriters(const TParamTree* params);
    bool x_CreateReaders(const string& str, const TParamTree* params);
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
