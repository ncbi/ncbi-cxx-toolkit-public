#ifndef __LOADER_PATCHER_HPP__
#define __LOADER_PATCHER_HPP__

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
*  Author: Maxim Didenko
*
*  File Description:
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/impl/tse_assigner.hpp>
#include <objmgr/edit_saver.hpp>
#include <objmgr/edits_db_engine.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////////
//
// CDataLoaderPatcher
//

// Parameter names used by loader factory

const string kCFParam_DLP_DataLoader =    "DataLoader"; 
const string kCFParam_DLP_EditsDBEngine = "EdtisDBEngine"; 
const string kCFParam_DLP_EditSaver =     "EditSaver"; 

class NCBI_XLOADER_PATCHER_EXPORT CDataLoaderPatcher : public CDataLoader
{
    
public:

    struct SParam
    {
        SParam(CRef<CDataLoader>    data_loader,
               CRef<IEditsDBEngine> db_engine,
               CRef<IEditSaver>     saver) 
            : m_DataLoader(data_loader), m_DBEngine(db_engine),
              m_EditSaver(saver)
        {}
        
        CRef<CDataLoader>    m_DataLoader;
        CRef<IEditsDBEngine> m_DBEngine;
        CRef<IEditSaver>     m_EditSaver;

    };

    typedef SRegisterLoaderInfo<CDataLoaderPatcher> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CRef<CDataLoader>,
        CRef<IEditsDBEngine>,
        CRef<IEditSaver> saver = CRef<IEditSaver>(),
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);

    static string GetLoaderNameFromArgs(const SParam& param);
    static string GetLoaderNameFromArgs(CRef<CDataLoader>    data_loader,
                                        CRef<IEditsDBEngine> db_engine,
                                        CRef<IEditSaver>     saver)
    {
        return GetLoaderNameFromArgs(SParam(data_loader, db_engine, saver));
    }
    
    virtual ~CDataLoaderPatcher();

    /// Request from a datasource using handles and ranges instead of seq-loc
    /// The TSEs loaded in this call will be added to the tse_set.
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);
    
    /// Request from a datasource using handles and ranges instead of seq-loc
    /// The TSEs loaded in this call will be added to the tse_set.
    /// Default implementation will call GetRecords().
    virtual TTSE_LockSet GetDetailedRecords(const CSeq_id_Handle& idh,
                                            const SRequestDetails& details);
    

    /// Request from a datasource set of blobs with external annotations.
    /// CDataLoader has reasonable default implementation.
    virtual TTSE_LockSet GetExternalRecords(const CBioseq_Info& bioseq);

    //virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);

    // blob operations
    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobVersion GetBlobVersion(const TBlobId& id);

    virtual bool CanGetBlobById(void) const;
    virtual TBlobId GetBlobIdFromString(const string& str) const;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    virtual void GetChunk(TChunk chunk_info);
    virtual void GetChunks(const TChunkSet& chunks);


    virtual TEditSaver GetEditSaver() const;

    /*
    virtual SRequestDetails ChoiceToDetails(EChoice choice) const;
    virtual EChoice DetailsToChoice(const SRequestDetails::TAnnotSet& annots) const; 
    virtual EChoice DetailsToChoice(const SRequestDetails& details) const;
    */
   
    
private:

    CRef<CDataLoader>    m_DataLoader;
    CRef<IEditsDBEngine> m_DBEngine;
    CRef<IEditSaver>     m_EditSaver;

    typedef CParamLoaderMaker<CDataLoaderPatcher, SParam> TMaker;
    friend class CParamLoaderMaker<CDataLoaderPatcher, SParam>;
    
    void x_PatchLockSet(const TTSE_LockSet& orig_locks, 
                      TTSE_LockSet& new_locks);

    TTSE_Lock   x_PatchLock(const TTSE_Lock& lock);

    bool x_IsPatchNeeded(const CTSE_Info& tse);
    void x_ApplyPatches(CTSE_Info& tse);

    CDataLoaderPatcher(const string& loader_name,
                       const SParam& param);
    
    /// Prevent automatic copy constructor generation
    CDataLoaderPatcher(const CDataLoaderPatcher &);
    
    /// Prevent automatic assignment operator generation
    CDataLoaderPatcher & operator=(const CDataLoaderPatcher &);

};

END_SCOPE(objects)

extern NCBI_XLOADER_PATCHER_EXPORT const string kDataLoader_Patcher_DriverName;

extern "C"
{

NCBI_XLOADER_PATCHER_EXPORT
void NCBI_EntryPoint_DataLoader_Patcher(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_PATCHER_EXPORT
void NCBI_EntryPoint_xloader_Patcher(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif  // __LOADER_PATCHER_HPP__
