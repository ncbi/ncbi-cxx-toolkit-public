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
 * Author: Pavel Ivanov
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbidbg.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/request_ctx.hpp>

#include "nc_storage.hpp"
#include "error_codes.hpp"


BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X  NetCache_Storage


/// Default part of default blob timeout to be used as period for database
/// parts rotation.
static const int   kNCDefDBRotateFraction     = 10;

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
static const char* kNCStorage_BlobsBatchParam = "purge_batch_size";
static const char* kNCStorage_GCSleepParam    = "purge_batch_sleep";



/// Special exception to terminate background thread when storage is
/// destroying. Class intentionally made without inheritance from CException
/// or std::exception.
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

    m_MaxBlobSize    = reg.GetInt(section, kNCStorage_MaxBlobParam,
                                  0,   0, IRegistry::eErrPost);

    m_GCRunDelay     = reg.GetInt(section, kNCStorage_GCDelayParam,
                                  30,  0, IRegistry::eErrPost);
    m_BlobsBatchSize = reg.GetInt(section, kNCStorage_BlobsBatchParam,
                                  500, 0, IRegistry::eErrPost);
    m_GCBatchSleep   = reg.GetInt(section, kNCStorage_GCSleepParam,
                                  0,   0, IRegistry::eErrPost);
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
            CNCFileSystem::OpenNewDBFile(index_name, true);
            m_IndexDB = new CNCDBIndexFile(index_name, GetStat());
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
                CNCFileSystem::DeleteFileOnClose(it->meta_file);
                CNCFileSystem::DeleteFileOnClose(it->data_file);
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
CNCBlobStorage::x_CheckPartsMinBlobId(void)
{
    TNCBlobId prev_min_id = 1;
    NON_CONST_ITERATE(TNCDBPartsList, it, m_DBParts) {
        if (it->min_blob_id == 0) {
            if (it == m_DBParts.begin()) {
                it->min_blob_id = 1;
            }
            else {
                --it;
                TMetaFileLock metafile(this, *it);
                TNCBlobId min_id = metafile->GetLastBlobId();
                ++it;
                it->min_blob_id = (min_id == 0? prev_min_id: min_id) + 1;
            }
            m_IndexDB->SetDBPartMinBlobId(it->part_id, it->min_blob_id);
        }
        prev_min_id = it->min_blob_id;
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
        // We cannot make function-style initialization because GCC 3.0.4
        // somehow doesn't understand it here.
        string send_msg = CTime(CTime::eCurrent).AsString();
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
    CSpinGuard guard(m_IdLockMutex);
    CYieldingRWLock* rw_lock = NULL;

    TId2LocksMap::iterator it = m_IdLocks.find(blob_id);
    if (it == m_IdLocks.end()) {
        if (m_FreeLocks.empty()) {
            rw_lock = new CYieldingRWLock();
        }
        else {
            rw_lock = m_FreeLocks.back();
            m_FreeLocks.pop_back();
        }
        m_IdLocks[blob_id] = rw_lock;
    }
    else {
        rw_lock = it->second;
    }
    // Acquiring cannot be carried out of mutex because it would be race with
    // unlocking - after getting here from map and before locking it can be
    // deleted from map in UnlockBlobId().
    return rw_lock->AcquireLock(access == eRead? eReadLock: eWriteLock);
}

void
CNCBlobStorage::UnlockBlobId(TNCBlobId blob_id, TRWLockHolderRef& rw_holder)
{
    if (rw_holder.IsNull()) {
        return;
    }

    rw_holder->ReleaseLock();
    CYieldingRWLock* rw_lock = rw_holder->GetRWLock();

    m_IdLockMutex.Lock();
    if (!rw_lock->IsLocked()) {
        // By this check we avoid race when several threads are releasing
        // lock simultaneously and both execute this code with unlocked
        // rw_lock, but it should be added to m_FreeLocks only once. And
        // there's even more tough scenario when 2 threads release lock,
        // 1st removes it from map, but before 2nd comes here 3rd thread
        // acquires lock on the same blob_id again thus adding to map
        // another lock which could be accidentally deleted by 2nd thread.
        // So here we must thoroughly check that we're deleting what we do
        // really want to delete.
        TId2LocksMap::iterator it = m_IdLocks.find(blob_id);
        if (it != m_IdLocks.end()  &&  it->second == rw_lock) {
            m_IdLocks.erase(it);
            m_FreeLocks.push_back(rw_lock);
        }
    }
    m_IdLockMutex.Unlock();

    rw_holder.Reset();
}

inline void
CNCBlobStorage::x_FreeBlobIdLocks(void)
{
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
CNCBlobStorage::x_InitFilesPool(const SNCDBPartInfo& part_info)
{
    TFilesPoolPtr pool(new CNCDBFilesPool(part_info.meta_file,
                                          part_info.data_file,
                                          GetStat()));
    CFastWriteGuard guard(m_DBFilesMutex);
    m_DBFiles[part_info.part_id] = pool;
}

inline void
CNCBlobStorage::x_CreateNewDBStructure(TNCDBPartId part_id)
{
    TMetaFileLock metafile(this, part_id);
    metafile->CreateDatabase();
    TDataFileLock datafile(this, part_id);
    datafile->CreateDatabase();
}

inline void
CNCBlobStorage::x_SwitchCurrentDBPart(SNCDBPartInfo* part_info)
{
    CFastWriteGuard guard(m_DBPartsLock);

    m_LastBlobLock.Lock();
    part_info->min_blob_id = ++m_LastBlob.blob_id;
    m_LastBlob.part_id = part_info->part_id;
    m_LastBlobLock.Unlock();

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

    // After x_InitFilesPool() in case of exception pool will be left alive,
    // but we intentionally will not clean it because it's not a big
    // deal and cleaning just harm code clearness.
    x_InitFilesPool(part_info);
    x_CreateNewDBStructure(part_id);
    CNCFileSystem::SetFileInitialized(meta_name);
    CNCFileSystem::SetFileInitialized(data_name);

    // No need of mutex on using index database because it is used only in one
    // background thread (after using in constructor).
    m_IndexDB->CreateDBPart(&part_info);
    x_SwitchCurrentDBPart(&part_info);
    try {
        m_IndexDB->SetDBPartMinBlobId(part_id, part_info.min_blob_id);
    }
    catch (CException& ex) {
        ERR_POST_X(11, "Error updating blob id in index database: " << ex);
        // Though index is not updated database parts are already switched
        // and we can work further - we will be able to recover in case of
        // restart.
    }
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

inline void
CNCBlobStorage::x_RotateDBParts(bool force_rotate /* = false */)
{
    SNCDBPartInfo& last_part = m_DBParts.back();
    if (!force_rotate
        &&  int(time(NULL)) - last_part.create_time < m_DBRotatePeriod)
    {
        return;
    }

    bool need_create_new = true;
    if (!force_rotate) {
        try {
            TMetaFileLock metafile(this, last_part);
            need_create_new = metafile->HasAnyBlobs();
        }
        catch (exception& ex) {
            CQuickStrStream msg;
            msg << "BG: Error reading information from part "
                << last_part.part_id;
            x_MonitorError(ex, msg);
        }
    }
    if (need_create_new) {
        try {
            x_CreateNewDBPart();
        }
        catch (exception& ex) {
            x_MonitorError(ex, "BG: Unable to rotate database");
        }
    }
    else {
        try {
            m_IndexDB->UpdateDBPartCreated(&last_part);
        }
        catch (exception& ex) {
            x_MonitorError(ex,
                "BG: Index database does not update date created");
        }
    }
}

inline void
CNCBlobStorage::x_ReadLastBlobCoords(void)
{
    SNCDBPartInfo& last_info = m_DBParts.back();
    m_LastBlob.part_id = last_info.part_id;
    TMetaFileLock last_meta(this, last_info);
    m_LastBlob.blob_id = 0;
    try {
        m_LastBlob.blob_id = last_meta->GetLastBlobId();
    }
    catch (exception& ex) {
        x_MonitorError(ex, "Error reading information from last part");
        x_RotateDBParts(true);
    }
    if (m_LastBlob.blob_id == 0) {
        // min_blob_id is guaranteed to be correctly set because of
        // x_CheckPartsMinBlobId()
        m_LastBlob.blob_id = last_info.min_blob_id;
    }
}

CNCBlobStorage::CNCBlobStorage(const IRegistry& reg,
                               CTempString      section,
                               EReinitMode      reinit_mode)
    : m_Monitor(NULL),
      m_Flags(0),
      m_Stopped(false),
      m_StopTrigger(0, 100),
      m_AllDBPartsCached(false),
      m_CurDBSize(0),
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
    x_CheckPartsMinBlobId();

    m_LastDeadTime = int(time(NULL));
    if (m_DBParts.empty()) {
        x_CreateNewDBPart();
    }
    else {
        ITERATE(TNCDBPartsList, it, m_DBParts) {
            x_InitFilesPool(*it);
        }
        x_ReadLastBlobCoords();
    }
    x_SetNotCachedPartId(m_LastBlob.blob_id == 0? -1: m_LastBlob.part_id);

    m_BGThread = NewBGThread(this, &CNCBlobStorage::x_DoBackgroundWork);
    m_BGThread->Run();

}

CNCBlobStorage::~CNCBlobStorage(void)
{
    m_Stopped = true;
    // To be on a safe side let's post 100
    m_StopTrigger.Post(100);
    m_BGThread->Join();

    m_DBFiles.clear();
    m_IndexDB.reset();

    x_FreeBlobIdLocks();
    if (!IsReadOnly()) {
        x_UnlockInstanceGuard();
    }
}

inline void
CNCBlobStorage::x_CheckStopped(void)
{
    if (m_Stopped) {
        throw CNCStorage_StoppedException();
    }
}

inline bool
CNCBlobStorage::x_ReadBlobCoordsFromCache(SNCBlobIdentity* identity)
{
    CFastReadGuard guard(m_KeysCacheLock);
    TKeyIdMap::const_iterator it = m_KeysCache.find(*identity);
    if (it != m_KeysCache.end()) {
        identity->AssignCoords(*it);
        return true;
    }
    return false;
}

inline bool
CNCBlobStorage::x_CreateBlobInCache(const SNCBlobIdentity& identity,
                                    SNCBlobCoords*         new_coords)
{
    {{
        CFastWriteGuard guard(m_KeysCacheLock);
        pair<TKeyIdMap::iterator, bool> ins_pair
                                              = m_KeysCache.insert(identity);
        if (!ins_pair.second) {
            new_coords->AssignCoords(*ins_pair.first);
            GetStat()->AddCreateHitExisting();
            return false;
        }
    }}
    {{
        CFastWriteGuard guard(m_IdsCacheLock);
        m_IdsCache.insert(identity);
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
        // Blob will not be found if somebody else has already deleted it.
        // But it should not ever occur because deleting is mutually
        // exclusive by blob locking.
        _ASSERT(it != m_IdsCache.end());
        identity = *it;
        m_IdsCache.erase(it);
    }}
    {{
        CFastWriteGuard guard(m_KeysCacheLock);
        m_KeysCache.erase(identity);
    }}
}

inline void
CNCBlobStorage::x_FillBlobIdsFromCache(const string& key, TNCIdsList* id_list)
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
        metafile->GetBlobIdsList(dead_after, last_id,
                                 numeric_limits<int>::max(),
                                 m_BlobsBatchSize, &id_list);
        ITERATE(TNCIdsList, id_it, id_list) {
            try {
                TNCBlobLockHolderRef blob_lock
                             = x_GetBlobAccess(eRead, part_id, *id_it, false);
                GetStat()->AddGCLockRequest();
                if (blob_lock->IsLockAcquired()) {
                    // There's non-zero possibility of race here as a result
                    // of which statistics will not be completely accurate but
                    // it's not a big deal for us.
                    GetStat()->AddGCLockAcquired();
                }
                // On acquiring of the lock information will be automatically
                // added to the cache. If lock is not acquired then somebody
                // else has acquired it and so he has already added info to
                // the cache.
                blob_lock->ReleaseLock();
            }
            catch (exception& ex) {
                x_MonitorError(ex, "Error while caching blob with id="
                                   + NStr::Int8ToString(*id_it));
            }
        }
    }
    while (!id_list.empty());
}

bool
CNCBlobStorage::ReadBlobCoords(SNCBlobIdentity* identity)
{
    identity->blob_id = 0;
    // dead_time should be read before check_part_id to avoid races when
    // caching has already finished and dead_time is already changing but we
    // don't know about it yet
    int dead_time = m_LastDeadTime;
    TNCDBPartId check_part_id = x_GetNotCachedPartId();
    if (x_ReadBlobCoordsFromCache(identity))
        return true;

    if (check_part_id != -1) {
        TNCDBPartsList parts;
        x_CopyPartsList(&parts);
        bool do_check = false;
        // With REVERSE_ITERATE code should look more ugly to be compilable
        // in WorkShop.
        NON_CONST_REVERSE_ITERATE(TNCDBPartsList, it, parts) {
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
CNCBlobStorage::CreateBlob(const SNCBlobIdentity& identity,
                           SNCBlobCoords*         new_coords)
{
    if (x_GetNotCachedPartId() != -1) {
        // In case if some databases were not cached we need first to check if
        // there is such key in some not cached databases.
        SNCBlobIdentity temp_ident(identity);
        temp_ident.part_id = 0;
        temp_ident.blob_id = 0;
        if (ReadBlobCoords(&temp_ident)) {
            new_coords->AssignCoords(temp_ident);
            GetStat()->AddCreateHitExisting();
            return false;
        }
    }
    if (!x_CreateBlobInCache(identity, new_coords))
        return false;
    TMetaFileLock metafile(this, identity.part_id);
    metafile->CreateBlobKey(identity);
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
        if (identity->part_id != 0) {
            part_id = identity->part_id;
        }
        else {
            CFastReadGuard guard(m_DBPartsLock);
            // With REVERSE_ITERATE code should look more ugly to be
            // compilable in WorkShop.
            NON_CONST_REVERSE_ITERATE(TNCDBPartsList, it, m_DBParts) {
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
        }
        if (part_id != 0) {
            TMetaFileLock metafile(this, part_id);
            if (metafile->ReadBlobKey(identity, dead_time)) {
                identity->part_id = part_id;
                // NB: There is no race here for adding to cache because
                // GC deletes blobs only after obtaining lock on them and this
                // method is called only when lock is already acquired. Also
                // there's no race with background adding to cache because it
                // will be just repeating of the same work without any harm.
                // So even if call to x_CreateBlobInCache() found blob in
                // cache it will not harm identity structure because blob will
                // have the same coordinates.
                x_CreateBlobInCache(*identity, identity);
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
    // Just hide this blob from GC - pretend that it was already expired.
    metafile->SetBlobDeadTime(coords.blob_id, m_LastDeadTime - 1);
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
        x_FillBlobIdsFromCache(key, id_list);
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
    GetStat()->AddExistenceCheck();
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
        // With REVERSE_ITERATE code should look more ugly to be compilable
        // in WorkShop.
        NON_CONST_REVERSE_ITERATE(TNCDBPartsList, it, parts) {
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

void
CNCBlobStorage::PrintStat(CPrintTextProxy& proxy)
{
    TNCDBPartsList parts;
    x_CopyPartsList(&parts);

    proxy << "Usage statistics for storage '"
                    << m_Name << "' at " << m_Path << ":" << endl
          << endl
          << "Currently in database - "
                    << parts.size() << " parts with "
                    << m_CurDBSize << " bytes (diff for ids "
                    << parts.back().part_id - parts.front().part_id << ")" << endl
          << "List of database parts:" << endl;
    NON_CONST_ITERATE(TNCDBPartsList, it, parts) {
        CTime create_time(time_t(it->create_time));
        create_time.ToLocalTime();
        proxy << it->part_id << " - " << create_time << " created, "
                                      << it->min_blob_id << " blob id" << endl;
    }
    proxy << endl;
    m_Stat.Print(proxy);
}

inline bool
CNCBlobStorage::x_GC_DeleteExpired(TNCDBPartId part_id, TNCBlobId blob_id)
{
    try {
        TNCBlobLockHolderRef blob_lock
                           = x_GetBlobAccess(eWrite, part_id, blob_id, false);
        GetStat()->AddGCLockRequest();
        if (blob_lock->IsLockAcquired()) {
            GetStat()->AddGCLockAcquired();
            if (blob_lock->IsBlobExists()  &&  blob_lock->IsBlobExpired())
            {
                if (x_IsMonitored()) {
                    CQuickStrStream msg;
                    msg << "BG: deleting blob key='"
                        << blob_lock->GetBlobKey() << "', subkey='"
                        << blob_lock->GetBlobSubkey() << "', version="
                        << blob_lock->GetBlobVersion();
                    x_MonitorPost(msg);
                }
                // Let's avoid access to database file and will not call
                // DeleteBlob() on lock holder.
                //blob_lock->DeleteBlob();
                SNCBlobCoords coords(part_id, blob_id);
                x_DeleteBlobFromCache(coords);
            }
            blob_lock->ReleaseLock();
        }
        else {
            // Lock has not been acquired - we will need to try once more.
            return false;
        }
    }
    catch (exception& ex) {
        CQuickStrStream msg;
        msg << "BG: Cannot delete expired blob " << blob_id
            << " in part " << part_id;
        x_MonitorError(ex, msg);
    }
    return true;
}

inline bool
CNCBlobStorage::x_GC_IsMetafileEmpty(TNCDBPartId          part_id,
                                     const TMetaFileLock& metafile,
                                     int                  dead_after)
{
    bool is_empty = true;
    try {
        is_empty = !metafile->HasLiveBlobs(dead_after);
    }
    catch (exception& ex) {
        CQuickStrStream msg;
        msg << "BG: Error accessing meta file " << part_id
            << " - assuming it is empty";
        x_MonitorError(ex, msg);
        if (part_id == m_LastBlob.part_id) {
            try {
                x_CreateNewDBPart();
            }
            catch (exception&) {
                // We already tried hard to keep working normally. At this
                // point we can only re-init storage which perhaps will be
                // implemented later.
            }
        }
    }
    return is_empty;
}

inline void
CNCBlobStorage::x_GC_PrepFilesPoolToDelete(TNCDBPartId    part_id,
                                           TFilesPoolPtr* pool_ptr)
{
    CFastWriteGuard guard(m_DBFilesMutex);

    TFilesMap::iterator it = m_DBFiles.find(part_id);
    _ASSERT(it != m_DBFiles.end());
    *pool_ptr = it->second;
    m_DBFiles.erase(it);
}

inline void
CNCBlobStorage::x_GC_DeleteDBPart(TNCDBPartsList::iterator part_it)
{
    TNCDBPartId part_id = part_it->part_id;
    string meta_name    = part_it->meta_file;
    string data_name    = part_it->data_file;
    {{
        CFastWriteGuard guard(m_DBPartsLock);
        m_DBParts.erase(part_it);
    }}
    try {
        m_IndexDB->DeleteDBPart(part_id);
    }
    catch (exception& ex) {
        x_MonitorError(ex, "BG: Index database does not delete rows");
    }

    TFilesPoolPtr pool;
    x_GC_PrepFilesPoolToDelete(part_id, &pool);
    try {
        // First schedule the deletion
        CNCFileSystem::DeleteFileOnClose(meta_name);
        CNCFileSystem::DeleteFileOnClose(data_name);
        // Then close all connections.
        pool.reset();
    }
    catch (exception& ex) {
        // Something really bad happening
        CQuickStrStream msg;
        msg << "BG: Database part " << part_id << " cannot be deleted";
        x_MonitorError(ex, msg);
    }
}

inline bool
CNCBlobStorage::x_GC_CleanDBPart(TNCDBPartsList::iterator part_it,
                                 int                      dead_before)
{
    bool can_shift = true;
    bool can_delete = false;
    try {
        TMetaFileLock metafile(this, *part_it);
        TNCIdsList ids;
        int dead_after = const_cast<int&>(m_LastDeadTime);
        TNCBlobId last_id = 0;
        do {
            x_CheckStopped();
            x_MonitorPost("BG: Starting next GC batch");
            metafile->GetBlobIdsList(dead_after, last_id, dead_before,
                                     m_BlobsBatchSize, &ids);
            ITERATE(TNCIdsList, id_it, ids) {
                x_CheckStopped();
                can_shift &= x_GC_DeleteExpired(part_it->part_id, *id_it);
            }
            x_MonitorPost("BG: Batch processing is finished");
            m_StopTrigger.TryWait(m_GCBatchSleep / 1000,
                                  m_GCBatchSleep % 1000 * 1000000);
        }
        while (!ids.empty());

        if (can_shift) {
            can_delete = part_it->part_id != m_LastBlob.part_id
                         &&  x_GC_IsMetafileEmpty(part_it->part_id,
                                                  metafile, dead_before);
        }
    }
    catch (exception& ex) {
        CQuickStrStream msg;
        msg << "BG: Error processing part " << part_it->part_id
            << ". Part will be deleted";
        x_MonitorError(ex, msg);
        // TODO: maybe it will be better to exclude this part from storage
        // but leave it physically to look later at what happened.
        can_delete = can_shift = true;
    }

    if (can_delete) {
        x_GC_DeleteDBPart(part_it);
    }
    return can_shift;
}

inline void
CNCBlobStorage::x_GC_CollectPartsStatistics(void)
{
    // Nobody else changes m_DBParts, so working with it right away without
    // any mutexes.
    GetStat()->AddNumberOfDBParts(m_DBParts.size(),
                        m_DBParts.back().part_id - m_DBParts.front().part_id);

    Int8 total_meta = 0, total_data = 0;
    // With ITERATE code should look more ugly to be
    // compilable in WorkShop.
    NON_CONST_ITERATE(TNCDBPartsList, it, m_DBParts) {
        Int8 meta_size = CFile(it->meta_file).GetLength();
        Int8 data_size = CFile(it->data_file).GetLength();
        total_meta += meta_size;
        total_data += data_size;
        GetStat()->AddDBPartSizes(meta_size, data_size);
    }
    GetStat()->AddTotalDBSize(total_meta, total_data);
    m_CurDBSize = total_meta + total_data;
}

void
CNCBlobStorage::x_DoBackgroundWork(void)
{
    // Make background postings to be all in the same request id
    CRef<CRequestContext> diag_context(new CRequestContext());
    diag_context->SetRequestID();
    CDiagContext::SetRequestContext(diag_context);

    try {
        if (x_GetNotCachedPartId() != -1) {
            // Nobody else changes m_DBParts, so iterating right over it.
            // With REVERSE_ITERATE code should look more ugly to be
            // compilable in WorkShop.
            NON_CONST_REVERSE_ITERATE(TNCDBPartsList, part_it, m_DBParts) {
                x_SetNotCachedPartId(part_it->part_id);
                try {
                    x_FillCacheFromDBPart(part_it->part_id);
                }
                catch (exception& ex) {
                    // Try to recover from any database errors by just
                    // ignoring it and trying to work further.
                    CQuickStrStream msg;
                    msg << "BG: Database part " << part_it->part_id
                        << " was not cached";
                    x_MonitorError(ex, msg);
                }
            }
            x_SetNotCachedPartId(-1);
        }
        for (;;) {
            x_MonitorPost("BG: Starting next GC cycle");
            x_GC_CollectPartsStatistics();

            int next_dead = int(time(NULL));
            bool can_change = true;
            ERASE_ITERATE(TNCDBPartsList, part_it, m_DBParts) {
                can_change &= x_GC_CleanDBPart(part_it, next_dead);
            }
            if (can_change) {
                m_LastDeadTime = next_dead;
            }
            x_CheckStopped();
            x_RotateDBParts();

            x_MonitorPost("BG: GC cycle ended");
            m_StopTrigger.TryWait(m_GCRunDelay, 0);
        }
    }
    catch (CNCStorage_StoppedException&) {
        // Do nothing - we're already in place where we want to be
    }
    x_MonitorPost("BG: background thread exits normally");
}

END_NCBI_SCOPE
