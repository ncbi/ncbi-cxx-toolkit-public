#ifndef ___ASN_CACHE_LOADER__HPP
#define ___ASN_CACHE_LOADER__HPP

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
 * Author: Mike DiCuccio
 *
 * File Description: Object manager compliant data loader for local data
 *                   storage (ASN-Cache).
 *
 */

#include <objmgr/data_loader.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_export.h>

BEGIN_NCBI_SCOPE

class CAsnCache;

BEGIN_SCOPE(objects)

//////////////////////////////////////////////////////////////////
//
// CAsnCache_DataLoader.
// CDataLoader implementation for local data storage.
//

// Parameter names used by loader factory

class NCBI_ASN_CACHE_EXPORT CAsnCache_DataLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CAsnCache_DataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& db_path,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);
    static string GetLoaderNameFromArgs(const string& db_path);

    // Public constructor not to break CSimpleClassFactoryImpl code
    CAsnCache_DataLoader(void);

    virtual ~CAsnCache_DataLoader(void);

    /// @name CDataLoader interface methods
    /// @{
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);

    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);

    virtual TGi GetGi(const CSeq_id_Handle& idh);
    virtual int GetTaxId(const CSeq_id_Handle& idh);
    virtual bool CanGetBlobById() const;
    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

#if NCBI_PRODUCTION_VER > 20110000
    virtual void GetGis(const TIds& ids, TLoaded& loaded, TIds& ret);
#endif

    /// @}
    
    typedef CSimpleLoaderMaker<CAsnCache_DataLoader>              TSimpleMaker;
    typedef CParamLoaderMaker<CAsnCache_DataLoader, string> TDbMaker;
    friend class CSimpleLoaderMaker<CAsnCache_DataLoader>;
    friend class CParamLoaderMaker<CAsnCache_DataLoader, string>;

private:
    CAsnCache_DataLoader(const string& dl_name);

    // Construct dataloader, attach the external local data storage database
    CAsnCache_DataLoader(const string& dl_name,
                         const string& db_path);

    // thread-specific index map
    struct SCacheInfo
    {
        SCacheInfo();
        ~SCacheInfo();

        CFastMutex      cache_mtx;
        CRef<CAsnCache> cache;
        size_t          requests;
        size_t          found;
    };

    typedef vector< AutoPtr<SCacheInfo> > TIndexMap;
    CFastMutex m_Mutex;
    TIndexMap m_IndexMap;
    string    m_DbPath;
    SCacheInfo& x_GetIndex();
};

END_SCOPE(objects)


extern const string kDataLoader_AsnCache_DriverName;

extern "C"
{

void NCBI_EntryPoint_DataLoader_AsnCache(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

void NCBI_EntryPoint_xloader_asncache(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif
