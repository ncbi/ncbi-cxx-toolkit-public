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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbidbg.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/request_ctx.hpp>

#include "nc_storage.hpp"


BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X  NetCache_Storage


static const int    kNCDefDBRotateFraction    = 10;

static const char* kNCStorage_DBFileExt       = ".db";
static const char* kNCStorage_IndexFileSuffix = ".index";
static const char* kNCStorage_MetaFileSuffix  = ".meta.";
static const char* kNCStorage_DataFileSuffix  = ".data.";
static const char* kNCStorage_StartedFileName = "__ncbi_netcache_started__";

static const char* kNCStorage_PathParam       = "path";
static const char* kNCStorage_FileNameParam   = "name";
static const char* kNCStorage_ReadOnlyParam   = "read_only";
static const char* kNCStorage_DefTTLParam     = "timeout";
static const char* kNCStorage_MaxTTLParam     = "max_timeout";
static const char* kNCStorage_TimestampParam  = "timestamp";
static const char* kNCStorage_TS_OnRead       = "onread";
static const char* kNCStorage_DropDirtyParam  = "drop_if_dirty";
static const char* kNCStorage_MaxBlobParam    = "max_blob_size";
static const char* kNCStorage_DBRotateParam   = "db_rotate_period";
static const char* kNCStorage_GCDelayParam    = "purge_thread_delay";
static const char* kNCStorage_GCBatchParam    = "purge_batch_size";
static const char* kNCStorage_GCSleepParam    = "purge_batch_sleep";
//static const char* kNCStorage_GCShrinkParam   = "purge_shrink_step";



typedef void (CNCBlobStorage::*TGCMethod)(void);

/// Thread running garbage collector and trash cleaner for the storage
class CNCStorage_BGThread : public CThread
{
public:
    CNCStorage_BGThread(CNCBlobStorage* storage, TGCMethod method)
        : m_Storage(storage), m_Method(method)
    {}

    virtual ~CNCStorage_BGThread(void)
    {}

protected:
    virtual void* Main(void)
    {
        (m_Storage->*m_Method)();
        return NULL;
    }

private:
    CNCBlobStorage* m_Storage;
    TGCMethod       m_Method;
};

///
class CNCStorage_StoppedException
{};



const char*
CNCBlobStorageException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
    case eUnknown:        return "eUnknown";
    case eWrongFileName:  return "eWrongFileName";
    case eReadOnlyAccess: return "eReadOnlyAccess";
    default:              return CException::GetErrCodeString();
    }
}



inline void
CNCBlobStorage::x_ParseTimestampParam(const string& timestamp)
{
    list<string> ts_opts;
    NStr::Split(timestamp, " \t", ts_opts);
    ITERATE(list<string>, it, ts_opts) {
        const string& opt_value = *it;
        if (NStr::CompareNocase(opt_value, kNCStorage_TS_OnRead) == 0) {
            m_Flags |= fChangeTimeOnRead;
            continue;
        }
        ERR_POST_X(2, Warning 
                      << "Unknown timeout policy parameter: '"
                      << opt_value << "'");
    }
}

inline void
CNCBlobStorage::x_ReadStorageParams(const IRegistry& reg, CTempString section)
{
    m_Path = reg.Get(section, kNCStorage_PathParam);
    m_Name = reg.Get(section, kNCStorage_FileNameParam);
    if (m_Path.empty()  ||  m_Name.empty()) {
        NCBI_THROW(CNCBlobStorageException, eWrongFileName,
                   "Incorrect parameters for file name in '"
                   + string(section) + "' section: path='" + m_Path
                   + "', name='" + m_Name + "'");
    }

    CDir dir(m_Path);
    if (!dir.Exists()) {
        dir.Create();
    }
    m_GuardName = CDirEntry::MakePath(m_Path,
                                      kNCStorage_StartedFileName,
                                      m_Name);

    if (reg.GetBool(section, kNCStorage_ReadOnlyParam,
                    false, 0, IRegistry::eErrPost))
    {
        m_Flags |= fReadOnly;
    }
    m_DefBlobTTL = reg.GetInt(section, kNCStorage_DefTTLParam,
                              3600, 0, IRegistry::eErrPost);
    m_MaxBlobTTL = reg.GetInt(section, kNCStorage_MaxTTLParam,
                              0,    0, IRegistry::eErrPost);
    if (m_MaxBlobTTL < m_DefBlobTTL) {
        m_MaxBlobTTL = m_DefBlobTTL;
    }
    m_DBRotatePeriod = reg.GetInt(section, kNCStorage_DBRotateParam,
                                  0, 0, IRegistry::eErrPost);
    if (m_DBRotatePeriod == 0) {
        m_DBRotatePeriod = m_DefBlobTTL / kNCDefDBRotateFraction;
    }

    x_ParseTimestampParam(reg.Get(section, kNCStorage_TimestampParam));

    m_MaxBlobSize  = reg.GetInt(section, kNCStorage_MaxBlobParam,
                                0, 0, IRegistry::eErrPost);

    m_GCRunDelay   = reg.GetInt(section, kNCStorage_GCDelayParam,
                                30, 0, IRegistry::eErrPost);
    m_GCBatchSize  = reg.GetInt(section, kNCStorage_GCBatchParam,
                                500, 0, IRegistry::eErrPost);
    m_GCBatchSleep = reg.GetInt(section, kNCStorage_GCSleepParam,
                                0, 0, IRegistry::eErrPost);
/*    m_DBShrinkStep = static_cast<size_t>(
                     reg.GetInt(section, kNCStorage_GCShrinkParam,
                                3276800, 0, IRegistry::eErrPost));*/
}

inline string
CNCBlobStorage::x_GetIndexFileName(void)
{
    string file_name(m_Name);
    file_name += kNCStorage_IndexFileSuffix;
    return CDirEntry::MakePath(m_Path, file_name, kNCStorage_DBFileExt);
}

inline string
CNCBlobStorage::x_GetMetaFileName(TNCDBPartId part_id)
{
    string file_name(m_Name);
    file_name += kNCStorage_MetaFileSuffix;
    file_name += NStr::Int8ToString(part_id);
    return CDirEntry::MakePath(m_Path, file_name, kNCStorage_DBFileExt);
}

inline string
CNCBlobStorage::x_GetDataFileName(TNCDBPartId part_id)
{
    string file_name(m_Name);
    file_name += kNCStorage_DataFileSuffix;
    file_name += NStr::Int8ToString(part_id);
    return CDirEntry::MakePath(m_Path, file_name, kNCStorage_DBFileExt);
}

inline bool
CNCBlobStorage::x_LockInstanceGuard(void)
{
    bool guard_existed = CFile(m_GuardName).Exists();

    if (!guard_existed) {
        // Just create the file with read and write permissions
        CFileWriter tmp_writer(m_GuardName, CFileWriter::eOpenAlways);
        CFile guard_file(m_GuardName);
        CFile::TMode user_mode, grp_mode, oth_mode;

        guard_file.GetMode(&user_mode, &grp_mode, &oth_mode);
        user_mode |= CFile::fRead;
        guard_file.SetMode(user_mode, grp_mode, oth_mode);
    }

    m_GuardLock = new CFileLock(m_GuardName,
                                CFileLock::fDefault,
                                CFileLock::eExclusive);
    if (guard_existed  &&  CFile(m_GuardName).GetLength() == 0) {
        guard_existed = false;
    }

    CFileWriter writer(m_GuardLock->GetFileHandle());
    string pid = NStr::UIntToString(CProcess::GetCurrentPid());
    writer.Write(pid.c_str(), pid.size());

    return guard_existed;
}

inline void
CNCBlobStorage::x_UnlockInstanceGuard(void)
{
    _ASSERT(m_GuardLock.get());

    m_GuardLock.reset();
    CFile(m_GuardName).Remove();
}

inline void
CNCBlobStorage::x_OpenIndexDB(EReinitMode* reinit_mode)
{
    // Will be at most 2 repeats
    for (;;) {
        string index_name = x_GetIndexFileName();
        try {
            m_IndexDB = new CNCDB_IndexFile(index_name, GetStat());
            m_IndexDB->CreateDatabase();
            m_IndexDB->GetAllDBParts(&m_DBParts);
            break;
        }
        catch (CSQLITE_Exception& ex) {
            if (IsReadOnly()  ||  *reinit_mode == eNoReinit) {
                throw;
            }
            m_IndexDB.reset();
            ERR_POST_X(10, "Index file " << index_name << " is broken ("
                           << ex << "). Reinitializing storage.");
            CFile(index_name).Remove();
            *reinit_mode = eNoReinit;
            continue;
        }
    }
}

inline void
CNCBlobStorage::x_CheckDBInitialized(bool        guard_existed,
                                     EReinitMode reinit_mode,
                                     bool        reinit_dirty)
{
    // Will be at most 2 repeats
    for (;;) {
        if (reinit_mode == eForceReinit) {
            LOG_POST_X(7, "Reinitializing storage " << m_Name
                          << " at " << m_Path);

            ITERATE(TNCDBPartsList, it, m_DBParts) {
                CSQLITE_Connection(it->meta_file).DeleteDatabase();
                CSQLITE_Connection(it->data_file).DeleteDatabase();
            }
            m_DBParts.clear();
            m_IndexDB->DeleteAllDBParts();
        }
        else if (guard_existed) {
            if (reinit_dirty)
            {
                LOG_POST_X(3, "Database was closed uncleanly.");
                reinit_mode = eForceReinit;
                continue;
            }
            else {
                ERR_POST_X(4, Warning
                              << "NetCache wasn't finished cleanly"
                                 " in previous run. Will try to work"
                                 " with storage " << m_Name
                              << " at " << m_Path << " as is.");
            }
        }
        break;
    }
}

inline void
CNCBlobStorage::x_ReadLastBlobCoords(void)
{
    SNCDBPartInfo& last_info = m_DBParts.back();
    m_LastBlob.part_id = last_info.part_id;
    TMetaFileLock last_meta(this, last_info);
    m_LastBlob.blob_id = last_meta->GetLastBlobId();

    if (last_info.min_blob_id == 0) {
        TNCBlobId min_id = 0;
        for (TNCDBPartsList::reverse_iterator
                it = ++m_DBParts.rbegin();
                            min_id == 0 && it != m_DBParts.rend(); ++it)
        {
            TMetaFileLock pre_last(this, *it);
            min_id = pre_last->GetLastBlobId();
        }
        ++min_id;
        m_IndexDB->UpdateDBPartBlobId(m_LastBlob.part_id, min_id);
        last_info.min_blob_id = min_id;
    }
    if (m_LastBlob.blob_id == 0) {
        m_LastBlob.blob_id = last_info.min_blob_id;
    }
}

void
CNCBlobStorage::x_MonitorPost(CTempString msg, bool do_trace /* = true */)
{
    if (!x_IsMonitored()) {
        return;
    }

    if (do_trace) {
        LOG_POST(Trace << msg);
    }
    if (m_Monitor  &&  m_Monitor->IsMonitorActive()) {
        string send_msg(CTime(CTime::eCurrent).AsString());
        send_msg += "\n\t";
        send_msg += msg;
        send_msg += "\n";

        m_Monitor->SendString(send_msg);
    }
}

void
CNCBlobStorage::x_MonitorError(exception& ex, CTempString msg)
{
    string send_msg(msg);
    send_msg += " (storage ";
    send_msg += m_Name;
    send_msg += " at ";
    send_msg += m_Path;
    send_msg += "): ";
    send_msg += ex.what();
    ERR_POST_X(6, send_msg);
    x_MonitorPost(send_msg, false);
}

TRWLockHolderRef
CNCBlobStorage::LockBlobId(TNCBlobId blob_id, ENCBlobAccess access)
{
    CFastMutexGuard guard(m_IdLockMutex);
    CYieldingRWLock* rwlock = NULL;

    TId2LocksMap::iterator it = m_IdLocks.find(blob_id);
    if (it == m_IdLocks.end()) {
        if (m_FreeLocks.empty()) {
            rwlock = new CYieldingRWLock();
        }
        else {
            rwlock = m_FreeLocks.back();
            m_FreeLocks.pop_back();
        }
        m_IdLocks[blob_id] = rwlock;
    }
    else {
        rwlock = it->second;
    }

    return rwlock->AcquireLock(access == eRead? eReadLock: eWriteLock);
}

void
CNCBlobStorage::UnlockBlobId(TNCBlobId blob_id, TRWLockHolderRef& rw_holder)
{
    if (rw_holder.IsNull()) {
        return;
    }

    rw_holder->ReleaseLock();
    CYieldingRWLock* rw_lock = rw_holder->GetRWLock();
    {{
        CFastMutexGuard guard(m_IdLockMutex);

        if (!rw_lock->IsLocked()  &&  m_IdLocks.erase(blob_id)) {
            m_FreeLocks.push_back(rw_lock);
        }
    }}

    rw_holder.Reset();
}

inline void
CNCBlobStorage::x_FreeBlobIdLocks(void)
{
    CFastMutexGuard guard(m_IdLockMutex);

    // This iteration is just to quickly find if something is wrong.
    // If everything is ok it should be no-op.
    ITERATE(TId2LocksMap, it, m_IdLocks) {
        delete it->second;
    }
    ITERATE(TLocksList, it, m_FreeLocks) {
        delete *it;
    }
}

inline void
CNCBlobStorage::x_InitFilePools(const SNCDBPartInfo& part_info)
{
    TMetaFilePoolPtr meta_pool(new TMetaFilePool(TMetaFileFactory(
                                            part_info.meta_file, GetStat())));
    TDataFilePoolPtr data_pool(new TDataFilePool(TDataFileFactory(
                                            part_info.data_file, GetStat())));

    CFastWriteGuard guard(m_FileMapsMutex);
    m_MetaFiles[part_info.part_id] = meta_pool;
    m_DataFiles[part_info.part_id] = data_pool;
}

inline void
CNCBlobStorage::x_CreateNewDBStructure(const SNCDBPartInfo& part_info)
{
    try {
        TMetaFileLock metafile(this, part_info);
        metafile->CreateDatabase();
        TDataFileLock datafile(this, part_info);
        datafile->CreateDatabase();
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(11, "Creating new database part structure failed: " << ex);
        {{
            CFastWriteGuard maps_guard(m_FileMapsMutex);
            m_MetaFiles.erase(part_info.part_id);
            m_DataFiles.erase(part_info.part_id);
        }}
        throw;
    }
}

inline void
CNCBlobStorage::x_SwitchCurrentDBPart(SNCDBPartInfo* part_info)
{
    CFastWriteGuard guard(m_DBPartsLock);
    {{
        CFastMutexGuard id_guard(m_LastCoordsMutex);
        part_info->min_blob_id = ++m_LastBlob.blob_id;
        m_LastBlob.part_id = part_info->part_id;
    }}
    m_DBParts.push_back(*part_info);
}

inline void
CNCBlobStorage::x_CopyPartsList(TNCDBPartsList* parts_list)
{
    CFastReadGuard guard(m_DBPartsLock);
    *parts_list = m_DBParts;
}

void
CNCBlobStorage::x_CreateNewDBPart(void)
{
    // No need in mutex because m_LastPartId is changed only here and will be
    // executed only from one thread.
    TNCDBPartId part_id = m_LastBlob.part_id + 1;
    string meta_name = x_GetMetaFileName(part_id);
    string data_name = x_GetDataFileName(part_id);

    // Make sure that there's no such files from some previous runs
    CFile(meta_name).Remove();
    CFile(data_name).Remove();

    SNCDBPartInfo part_info;
    part_info.part_id = part_id;
    part_info.meta_file = meta_name;
    part_info.data_file = data_name;

    x_InitFilePools(part_info);
    x_CreateNewDBStructure(part_info);
    // No need of mutex on using index database because it is used only in one
    // background thread (after using in constructor).
    m_IndexDB->CreateDBPartRow(&part_info);
    x_SwitchCurrentDBPart(&part_info);
    m_IndexDB->UpdateDBPartBlobId(part_id, part_info.min_blob_id);
}

inline TNCDBPartId
CNCBlobStorage::x_GetNotCachedPartId(void)
{
    if (m_AllDBPartsCached)
        return -1;

    CFastReadGuard guard(m_NotCachedPartIdLock);
    return m_NotCachedPartId;
}

inline void
CNCBlobStorage::x_SetNotCachedPartId(TNCDBPartId part_id)
{
    CFastWriteGuard guard(m_NotCachedPartIdLock);
    m_NotCachedPartId = part_id;
    if (part_id == -1)
        m_AllDBPartsCached = true;
}

CNCBlobStorage::CNCBlobStorage(const IRegistry& reg,
                               CTempString      section,
                               EReinitMode      reinit_mode)
    : m_Monitor(NULL),
      m_Flags(0),
      m_Stopped(false),
      m_StopTrigger(0, 100),
      m_AllDBPartsCached(false),
      m_LockHoldersPool(TNCBlobLockFactory(this)),
      m_BlobsPool(TNCBlobsFactory(this))
{
    x_ReadStorageParams(reg, section);

    bool guard_existed = false;
    if (IsReadOnly()) {
        if (reinit_mode != eNoReinit) {
            ERR_POST_X(12, Warning
                       << "Read-only storage is asked to be re-initialized. "
                          "Ignoring this.");
            reinit_mode = eNoReinit;
        }
    }
    else {
        guard_existed = x_LockInstanceGuard();
    }

    x_OpenIndexDB(&reinit_mode);
    x_CheckDBInitialized(guard_existed, reinit_mode,
                         reg.GetBool(section, kNCStorage_DropDirtyParam,
                                     false, 0, IRegistry::eErrPost));

    m_LastDeadTime = int(time(NULL));
    if (m_DBParts.empty()) {
        x_CreateNewDBPart();
    }
    else {
        ITERATE(TNCDBPartsList, it, m_DBParts) {
            x_InitFilePools(*it);
        }
        x_ReadLastBlobCoords();
    }
    x_SetNotCachedPartId(m_LastBlob.blob_id == 0? -1: m_LastBlob.part_id);

    m_BGThread.Reset(new CNCStorage_BGThread(this,
                                             &CNCBlobStorage::RunBackground));
    m_BGThread->Run();

}

CNCBlobStorage::~CNCBlobStorage(void)
{
    m_Stopped = true;
    // To be on a safe side let's post 100
    m_StopTrigger.Post(100);
    m_BGThread->Join();

    x_FreeBlobIdLocks();
    if (!IsReadOnly()) {
        x_UnlockInstanceGuard();
    }
}

inline void
CNCBlobStorage::x_CheckStopped(void)
{
    if (m_Stopped) {
        throw new CNCStorage_StoppedException();
    }
}

inline bool
CNCBlobStorage::x_ReadBlobIdFromCache(SNCBlobIdentity* identity)
{
    CFastReadGuard guard(m_KeysCacheLock);
    TKeyIdMap::const_iterator it = m_KeysCache.find(*identity);
    if (it != m_KeysCache.end()) {
        identity->AssignCoords(*it);
        return true;
    }
    return false;
}

inline void
CNCBlobStorage::x_AddBlobKeyIdToCache(const SNCBlobIdentity& identity)
{
    bool inserted = false;
    {{
        CFastWriteGuard guard(m_KeysCacheLock);
        inserted = m_KeysCache.insert(identity).second;
    }}
    if (inserted) {
        CFastWriteGuard guard(m_IdsCacheLock);
        m_IdsCache.insert(identity);
    }
}

inline bool
CNCBlobStorage::x_CreateBlobInCache(SNCBlobIdentity* identity)
{
    {{
        CFastWriteGuard guard(m_KeysCacheLock);
        pair<TKeyIdMap::iterator, bool> ins_pair
                                              = m_KeysCache.insert(*identity);
        if (!ins_pair.second) {
            identity->AssignCoords(*ins_pair.first);
            return false;
        }
    }}
    {{
        CFastWriteGuard guard(m_IdsCacheLock);
        m_IdsCache.insert(*identity);
    }}
    return true;
}

inline bool
CNCBlobStorage::x_ReadBlobKeyFromCache(SNCBlobIdentity* identity)
{
    CFastReadGuard guard(m_IdsCacheLock);
    TIdKeyMap::const_iterator it = m_IdsCache.find(*identity);
    if (it != m_IdsCache.end()) {
        *identity = *it;
        return true;
    }
    return false;
}

inline void
CNCBlobStorage::x_DeleteBlobFromCache(const SNCBlobCoords& coords)
{
    SNCBlobIdentity identity(coords);
    {{
        CFastWriteGuard guard(m_IdsCacheLock);
        TIdKeyMap::iterator it = m_IdsCache.find(identity);
        if (it == m_IdsCache.end()) {
            // This can happen if somebody else has already deleted this blob.
            // But it should not ever occur because deleting is mutually
            // exclusive by blob locking.
            return;
        }
        identity = *it;
        m_IdsCache.erase(it);
    }}
    {{
        CFastWriteGuard guard(m_KeysCacheLock);
        m_KeysCache.erase(identity);
    }}
}

inline void
CNCBlobStorage::x_FillBlobIdsFromCache(TNCIdsList* id_list, const string& key)
{
    CFastReadGuard guard(m_KeysCacheLock);

    SNCBlobIdentity identity(key);
    TKeyIdMap::const_iterator it = m_KeysCache.lower_bound(identity);
    while (it != m_KeysCache.end()  &&  it->key == key) {
        id_list->push_back(it->blob_id);
        ++it;
    }
}

inline bool
CNCBlobStorage::x_IsBlobExistsInCache(CTempString key, CTempString subkey)
{
    SNCBlobIdentity identity(key, subkey, 0);
    CFastReadGuard guard(m_KeysCacheLock);
    TKeyIdMap::const_iterator it = m_KeysCache.lower_bound(identity);
    return it != m_KeysCache.end()
           &&  it->key == key  &&  it->subkey == subkey;
}

inline void
CNCBlobStorage::x_FillCacheFromDBPart(TNCDBPartId part_id)
{
    TMetaFileLock metafile(this, part_id);
    TNCIdsList id_list;
    TNCBlobId last_id = 0;
    // Hint to the compiler that this code executes in the thread where
    // m_LastDeadTime changes.
    int dead_after = const_cast<int&>(m_LastDeadTime);
    do {
        x_CheckStopped();
        metafile->GetBlobIdsList(last_id, dead_after,
                                 numeric_limits<int>::max(),
                                 m_GCBatchSize, &id_list);
        ITERATE(TNCIdsList, id_it, id_list) {
            TNCBlobLockHolderRef blob_lock
                               = GetBlobAccess(eRead, part_id, *id_it, false);
            // On acquiring of the lock information will be automatically
            // added to the cache. If lock is not acquired then somebody else
            // has acquired it and so he has already added info to the cache.
            blob_lock->ReleaseLock();
        }
    }
    while (!id_list.empty());
}

bool
CNCBlobStorage::ReadBlobId(SNCBlobIdentity* identity)
{
    identity->blob_id = 0;
    // dead_time should be read before check_part_id to avoid races when
    // caching has already finished and dead_time is already changing but we
    // don't know about it yet
    int dead_time = m_LastDeadTime;
    TNCDBPartId check_part_id = x_GetNotCachedPartId();
    if (x_ReadBlobIdFromCache(identity))
        return true;

    if (check_part_id != -1) {
        TNCDBPartsList parts;
        x_CopyPartsList(&parts);
        bool do_check = false;
        REVERSE_ITERATE(TNCDBPartsList, it, parts) {
            if (it->part_id == check_part_id) {
                do_check = true;
            }
            if (do_check) {
                TMetaFileLock metafile(this, *it);
                if (metafile->ReadBlobId(identity, dead_time)) {
                    identity->part_id = it->part_id;
                    break;
                }
            }
        }
        // NB: We cannot add obtained information to the cache because it will
        // have race with GC which could be already started (caching ended
        // while we worked) and could already delete this blob (access to this
        // method occurs without lock on the blob).
    }
    return identity->blob_id != 0;
}

bool
CNCBlobStorage::CreateBlob(SNCBlobIdentity* identity)
{
    if (x_GetNotCachedPartId() != -1) {
        // In case if some databases were not cached we need first to check if
        // there is such key in some not cached databases.
        SNCBlobIdentity temp_ident(*identity);
        temp_ident.part_id = 0;
        temp_ident.blob_id = 0;
        if (ReadBlobId(&temp_ident)) {
            identity->AssignCoords(temp_ident);
            return false;
        }
    }
    if (!x_CreateBlobInCache(identity))
        return false;
    TMetaFileLock metafile(this, identity->part_id);
    metafile->CreateBlobKey(*identity);
    return true;
}

bool
CNCBlobStorage::ReadBlobKey(SNCBlobIdentity* identity)
{
    // dead_time should be read before check_part_id to avoid races when
    // caching has already finished and dead_time is already changing but we
    // don't know about it yet
    int dead_time = m_LastDeadTime;
    TNCDBPartId check_part_id = x_GetNotCachedPartId();
    if (x_ReadBlobKeyFromCache(identity))
        return true;
    if (check_part_id != -1) {
        TNCDBPartId part_id = 0;
        {{
            CFastReadGuard guard(m_DBPartsLock);
            REVERSE_ITERATE(TNCDBPartsList, it, m_DBParts) {
                // Find the first part having minimum blob id less than
                // requested.
                if (it->min_blob_id <= identity->blob_id) {
                    // If it was already cached then we shouldn't check
                    // database file anyway
                    if (it->part_id <= check_part_id) {
                        part_id = it->part_id;
                    }
                    break;
                }
            }
        }}
        if (part_id != 0) {
            TMetaFileLock metafile(this, part_id);
            // NB: There is no race here for adding to cache because
            // GC deletes blobs only after obtaining lock on them and this
            // method is called only when lock is already acquired.
            if (metafile->ReadBlobKey(identity, dead_time)) {
                identity->part_id = part_id;
                x_AddBlobKeyIdToCache(*identity);
                return true;
            }
        }
    }
    return false;
}

void
CNCBlobStorage::DeleteBlob(const SNCBlobCoords& coords)
{
    x_DeleteBlobFromCache(coords);

    TMetaFileLock metafile(this, coords.part_id);
    //metafile->DeleteBlobKey(coords.blob_id);
    //metafile->DeleteBlobInfo(coords.blob_id);

    metafile->UpdateBlobDeadTime(coords.blob_id, m_LastDeadTime - 1);
}

void
CNCBlobStorage::FillBlobIds(const string& key, TNCIdsList* id_list)
{
    id_list->clear();
    // dead_time should be read before check_part_id to avoid races when
    // caching has already finished and dead_time is already changing but we
    // don't know about it yet
    int dead_time = m_LastDeadTime;
    if (x_GetNotCachedPartId() == -1) {
        x_FillBlobIdsFromCache(id_list, key);
    }
    else {
        TNCDBPartsList parts;
        x_CopyPartsList(&parts);
        ITERATE(TNCDBPartsList, it, parts) {
            TMetaFileLock metafile(this, *it);
            metafile->FillBlobIds(key, dead_time, id_list);
        }
    }
}

bool
CNCBlobStorage::IsBlobExists(CTempString key,
                             CTempString subkey /* = kEmptyStr */)
{
    // dead_time should be read before check_part_id to avoid races when
    // caching has already finished and dead_time is already changing but we
    // don't know about it yet
    int dead_time = m_LastDeadTime;
    TNCDBPartId check_part_id = x_GetNotCachedPartId();
    if (x_IsBlobExistsInCache(key, subkey))
        return true;
    if (check_part_id != -1) {
        TNCDBPartsList parts;
        x_CopyPartsList(&parts);
        bool do_check = false;
        REVERSE_ITERATE(TNCDBPartsList, it, parts) {
            if (it->part_id == check_part_id) {
                do_check = true;
            }
            if (do_check) {
                TMetaFileLock metafile(this, *it);
                if (metafile->IsBlobExists(key, subkey, dead_time))
                    return true;
            }
        }
    }
    return false;
}

inline bool
CNCBlobStorage::x_GC_DeleteExpired(TNCDBPartId part_id, TNCBlobId blob_id)
{
    try {
        TNCBlobLockHolderRef blob_lock
                             = GetBlobAccess(eWrite, part_id, blob_id, false);
        if (blob_lock->IsLockAcquired()) {
            if (blob_lock->IsBlobExists()  &&  blob_lock->IsBlobExpired())
            {
                if (x_IsMonitored()) {
                    CQuickStrStream msg;
                    msg << "GC: deleting blob key='"
                        << blob_lock->GetBlobKey() << "', subkey='"
                        << blob_lock->GetBlobSubkey() << "', version="
                        << blob_lock->GetBlobVersion();
                    x_MonitorPost(msg);
                }
                //blob_lock->DeleteBlob();
                SNCBlobCoords coords(part_id, blob_id);
                x_DeleteBlobFromCache(coords);
            }
            blob_lock->ReleaseLock();
        }
        else {
            return false;
        }
    }
    catch (exception& ex) {
        CQuickStrStream msg;
        msg << "GC: Cannot delete expired blob "
            << blob_id << " in part " << part_id;
        x_MonitorError(ex, msg);
    }
    return true;
}

inline bool
CNCBlobStorage::x_GC_IsMetafileEmpty(TNCDBPartId          part_id,
                                     const TMetaFileLock& metafile,
                                     int                  dead_time)
{
    bool is_empty = true;
    try {
        is_empty = metafile->IsEmpty(dead_time);
    }
    catch (exception& ex) {
        CQuickStrStream msg;
        msg << "GC: Error accessing meta file " << part_id
            << " - assuming it is empty";
        x_MonitorError(ex, msg);
        if (part_id == m_LastBlob.part_id) {
            try {
                x_CreateNewDBPart();
            }
            catch (exception&) {
                // We already tried hard to keep working normally. At this
                // point we can only re-init storage which maybe will be
                // implemented later.
            }
        }
    }
    return is_empty;
}

inline void
CNCBlobStorage::x_GC_PrepFilePoolsToDelete(TNCDBPartId       part_id,
                                           TMetaFilePoolPtr* meta_pool,
                                           TDataFilePoolPtr* data_pool)
{
    CFastWriteGuard guard(m_FileMapsMutex);

    TMetaFilesMap::iterator meta_it = m_MetaFiles.find(part_id);
    _ASSERT(meta_it != m_MetaFiles.end());
    *meta_pool = meta_it->second;
    m_MetaFiles.erase(meta_it);

    TDataFilesMap::iterator data_it = m_DataFiles.find(part_id);
    _ASSERT(data_it != m_DataFiles.end());
    *data_pool = data_it->second;
    m_DataFiles.erase(data_it);
}

inline void
CNCBlobStorage::x_GC_DeleteDBPart(TNCDBPartsList::iterator part_it)
{
    TNCDBPartId part_id = part_it->part_id;
    {{
        CFastWriteGuard guard(m_DBPartsLock);
        m_DBParts.erase(part_it);
    }}
    try {
        m_IndexDB->DeleteDBPartRow(part_id);
    }
    catch (exception& ex) {
        x_MonitorError(ex, "GC: Index database does not delete rows");
    }

    TMetaFilePoolPtr meta_pool;
    TDataFilePoolPtr data_pool;
    x_GC_PrepFilePoolsToDelete(part_id, &meta_pool, &data_pool);
    // Save exactly one connection of each type to delete
    // database files.
    TMetaFilePtr metafile(meta_pool->Get());
    TDataFilePtr datafile(data_pool->Get());
    // All the rest should be gone before deleting.
    meta_pool.reset();
    data_pool.reset();
    // Now we can delete
    metafile->DeleteDatabase();
    datafile->DeleteDatabase();
}

inline bool
CNCBlobStorage::x_GC_CleanDBPart(TNCDBPartsList::iterator part_it,
                                 int                      dead_before)
{
    bool can_shift = true;
    bool can_delete = false;
    {{
        TMetaFileLock metafile(this, *part_it);
        TNCIdsList ids;
        int dead_after = const_cast<int&>(m_LastDeadTime);
        TNCBlobId last_id = 0;
        do {
            x_CheckStopped();
            x_MonitorPost("GC: Starting next batch");
            metafile->GetBlobIdsList(last_id, dead_after, dead_before,
                                     m_GCBatchSize, &ids);
            ITERATE(TNCIdsList, id_it, ids) {
                x_CheckStopped();
                can_shift &= x_GC_DeleteExpired(part_it->part_id, *id_it);
            }
            x_MonitorPost("GC: Batch processing is finished");
            m_StopTrigger.TryWait(m_GCBatchSleep / 1000,
                                  m_GCBatchSleep % 1000 * 1000000);
        }
        while (!ids.empty());

        if (can_shift) {
            can_delete = part_it->part_id != m_LastBlob.part_id
                         &&  x_GC_IsMetafileEmpty(part_it->part_id,
                                                  metafile, dead_before);
        }
    }}
    if (can_delete) {
        x_GC_DeleteDBPart(part_it);
    }
    return can_shift;
}

inline void
CNCBlobStorage::x_GC_RotateDBParts(void)
{
    SNCDBPartInfo& last_part = m_DBParts.back();
    if (int(time(NULL)) - last_part.create_time < m_DBRotatePeriod)
        return;

    TDataFileLock datafile(this, last_part);
    if (datafile->IsEmpty()) {
        try {
            m_IndexDB->UpdateDBPartCreated(&last_part);
        }
        catch (exception& ex) {
            x_MonitorError(ex,
                           "GC: Index database does not update date created");
        }
    }
    else {
        try {
            x_CreateNewDBPart();
        }
        catch (exception& ex) {
            x_MonitorError(ex, "GC: Unable to rotate database");
        }
    }
}

void
CNCBlobStorage::RunBackground(void)
{
    // Make background postings to be all in the same request id
    CRef<CRequestContext> diag_context(new CRequestContext());
    diag_context->SetRequestID();
    CDiagContext::SetRequestContext(diag_context);

    try {
        if (x_GetNotCachedPartId() != -1) {
            // Nobody else changes m_DBParts
            REVERSE_ITERATE(TNCDBPartsList, part_it, m_DBParts) {
                x_SetNotCachedPartId(part_it->part_id);
                x_FillCacheFromDBPart(part_it->part_id);
            }
            x_SetNotCachedPartId(-1);
        }
        for (;;) {
            x_MonitorPost("GC: Starting next cycle");

            int next_dead = int(time(NULL));
            bool can_change = true;
            ERASE_ITERATE(TNCDBPartsList, part_it, m_DBParts) {
                can_change &= x_GC_CleanDBPart(part_it, next_dead);
            }
            if (can_change) {
                m_LastDeadTime = next_dead;
            }
            x_CheckStopped();
            x_GC_RotateDBParts();

            x_MonitorPost("GC: Work cycle ended");
            m_StopTrigger.TryWait(m_GCRunDelay, 0);
        }
    }
    catch (CNCStorage_StoppedException&) {
        // Do nothing - we're already in place where we want to be
    }
    x_MonitorPost("GC: background thread exits normally");
}

END_NCBI_SCOPE
