#ifndef LDS2_DATALOADER_HPP__
#define LDS2_DATALOADER_HPP__

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
 * Author: Aleksey Grichenko
 *
 * File Description: Object manager compliant data loader for local data
 *                   storage v.2 (LDS2).
 *
 */

#include <objmgr/data_loader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/lds2/lds2_db.hpp>
#include <objtools/lds2/lds2_handlers.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////
//
// CLDS2_DataLoader.
// CDataLoader implementation for LDS2.
//

// Parameter names used by loader factory

#define kCFParam_LDS2_DbPath     "DbPath"     // = string
#define kCFParam_LDS2_FastaFlags "FastaFlags" // = int
#define kCFParam_LDS2_LockMode   "LockMode"   // = lock | nolock


class NCBI_XLOADER_LDS2_EXPORT CLDS2_DataLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CLDS2_DataLoader> TRegisterLoaderInfo;

    /// Database lock control. By default the database is locked so
    /// that no other process can access it while the data loader
    /// exists. The default mode can be changed throug env/ini
    /// value LDS2_DATALOADER_LOCK = {lock | nolock}. If an explicit
    /// mode is set in the code, the env/ini parameter is ignored.
    /// NOTE: To set locks the database must be writable.
    enum ELockMode {
        eDefaultLockMode,   ///< Use LDS2_DATALOADER_LOCK parameter,
                            ///< lock by default.
        eLockDatabase,      ///< Lock the database.
        eDoNotLockDatabase  ///< Do not lock the database.
    };

    /// Argument-less loader - for compatibility only, unusable.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager&            om,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(void);

    /// Use the specified database file.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager&            om,
        const string&              db_path,
        CFastaReader::TFlags       fasta_flags = -1,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet,
        ELockMode lock_mode = eDefaultLockMode);
    static string GetLoaderNameFromArgs(const string& db_path);

    /// Use the existing LDS2 database object and optional fasta flags.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager&            om,
        CLDS2_Database&            lds_db,
        CFastaReader::TFlags       fasta_flags = -1,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet,
        ELockMode lock_mode = eDefaultLockMode);
    static string GetLoaderNameFromArgs(CLDS2_Database& lds_db);

    // Public constructor not to break CSimpleClassFactoryImpl code
    CLDS2_DataLoader(void);

    virtual ~CLDS2_DataLoader(void);

    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice               choice);

    virtual TTSE_LockSet GetExternalRecords(const CBioseq_Info& bioseq);

    virtual TTSE_LockSet GetExternalAnnotRecords(const CSeq_id_Handle& idh,
                                                 const SAnnotSelector* sel);
    virtual TTSE_LockSet GetExternalAnnotRecords(const CBioseq_Info& bioseq,
                                                 const SAnnotSelector* sel);

    virtual TTSE_Lock ResolveConflict(const CSeq_id_Handle& id,
                                      const TTSE_LockSet&   tse_set);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);

    virtual bool CanGetBlobById(void) const;

    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

    virtual void GetIds(const CSeq_id_Handle& idh, TIds& ids);

    virtual void GetChunk(TChunk chunk_info);

    typedef CSimpleLoaderMaker<CLDS2_DataLoader>                 TSimpleMaker;
    typedef CParamLoaderMaker<CLDS2_DataLoader, CLDS2_Database&> TDbMaker;
    friend class CSimpleLoaderMaker<CLDS2_DataLoader>;
    friend class CParamLoaderMaker<CLDS2_DataLoader, CLDS2_Database&>;

    /// Register URL handler. The default handlers "file" and "gzipfile"
    /// for local files are registered automatically.
    void RegisterUrlHandler(CLDS2_UrlHandler_Base* handler);

private:
    class CLDS2_LoaderMaker : public TSimpleMaker
    {
    public:
        CLDS2_LoaderMaker(const string&        db_path,
                          CFastaReader::TFlags fasta_flags,
                          CLDS2_DataLoader::ELockMode lock_mode);
        CLDS2_LoaderMaker(CLDS2_Database&      db,
                          CFastaReader::TFlags fasta_flags,
                          CLDS2_DataLoader::ELockMode lock_mode);
        virtual CDataLoader* CreateLoader(void) const;
    private:
        mutable CRef<CLDS2_Database> m_Db;
        string                       m_DbPath;
        CFastaReader::TFlags         m_FastaFlags;
        CLDS2_DataLoader::ELockMode  m_LockMode;
    };
    friend class CLDS2_LoaderMaker;

    typedef CLDS2_Database::TBlobSet TBlobSet;

    // Load data for the blobs
    void x_LoadBlobs(const TBlobSet& blobs,
                     TTSE_LockSet&   locks);
    void x_LoadTSE(CTSE_LoadLock& load_lock, const SLDS2_Blob& blob);
    CRef<CSeq_entry> x_LoadFastaTSE(CNcbiIstream&     in,
                                    const SLDS2_Blob& blob);

    CLDS2_UrlHandler_Base* x_GetUrlHandler(const SLDS2_File& info);

    // to prevent copying
    CLDS2_DataLoader(const CLDS2_DataLoader&);
    void operator==(const CLDS2_DataLoader&);

    // Provide only name - required by TSimpleMaker
    CLDS2_DataLoader(const string& dl_name);

    // Construct dataloader, attach the LDS2 manager
    CLDS2_DataLoader(const string&        dl_name,
                     CLDS2_Database&      lds_db,
                     CFastaReader::TFlags fasta_flags);

    typedef map<string, CRef<CLDS2_UrlHandler_Base> > THandlers;

    CRef<CLDS2_Database>  m_Db; // Reference on the LDS2 database
    CFastaReader::TFlags  m_FastaFlags;
    THandlers             m_Handlers;
};


END_SCOPE(objects)


extern NCBI_XLOADER_LDS2_EXPORT const string kDataLoader_LDS2_DriverName;

extern "C"
{

NCBI_XLOADER_LDS2_EXPORT
void NCBI_EntryPoint_DataLoader_LDS2(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_LDS2_EXPORT
void NCBI_EntryPoint_xloader_lds2(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif // LDS2_DATALOADER_HPP__
