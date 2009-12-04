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
/// Default number of volumes in each newly created database part
static const int   kNCDefCntVolumes           = 4;

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
static const char* kNCStorage_CntVolumesParam = "rr_volumes";
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
    if (m_DBRotatePeriod <= 0) {
        m_DBRotatePeriod = m_DefBlobTTL / kNCDefDBRotateFraction;
    }
    int cnt_volumes  = reg.GetInt(section, kNCStorage_CntVolumesParam,
                                  0, 0, IRegistry::eErrPost);
    m_DBCntVolumes = (cnt_volumes <= 0? kNCDefCntVolumes: cnt_volumes);

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

inline string
CNCBlobStorage::x_GetVolumeName(const string& prefix,
                                TNCDBVolumeId vol_id)
{
    string name(prefix);
    name += NStr::Int8ToString(vol_id);
    return name;
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

void
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
            x_MonitorError(ex, "Index file  is broken, reinitializing storage");
            CFile(index_name).Remove();
            *reinit_mode = eNoReinit;
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
                TNCDBVolumeId cnt_vols  = (*it)->cnt_volumes;
                const string& meta_name = (*it)->meta_file;
                const string& data_name = (*it)->data_file;
                if (cnt_vols == 0) {
                    CNCFileSystem::DeleteFileOnClose(meta_name);
                    CNCFileSystem::DeleteFileOnClose(data_name);
                }
                else {
                    for (TNCDBVolumeId i = 1; i < cnt_vols; ++i) {
                        CNCFileSystem::DeleteFileOnClose(
                                                x_GetVolumeName(meta_name, i));
                        CNCFileSystem::DeleteFileOnClose(
                                                x_GetVolumeName(data_name, i));
                    }
                }
            }
            m_DBParts.clear();
            try {
                m_IndexDB->DeleteAllDBParts();
            }
            catch (CSQLITE_Exception& ex) {
                x_MonitorError(ex, "Error in soft reinitialization, "
                                   "trying harder");
                m_IndexDB.reset();
                // GCC 3.0.4 doesn't allow to merge the following 2 lines
                CFile file_obj(x_GetIndexFileName());
                file_obj.Remove();
                reinit_mode = eNoReinit;
                x_OpenIndexDB(&reinit_mode);
            }
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

CNCBlobStorage::TPartFilesMap&
CNCBlobStorage::x_InitFilePools(const SNCDBPartInfo& part_info)
{
    TPartFilesMap temp_map;
    if (part_info.cnt_volumes == 0) {
        temp_map[0] = new CNCDBFilesPool(part_info.meta_file,
                                         part_info.data_file,
                                         GetStat());
    }
    else {
        for (TNCDBVolumeId i = 1; i <= part_info.cnt_volumes; ++i) {
            temp_map[i] = new CNCDBFilesPool(
                                      x_GetVolumeName(part_info.meta_file, i),
                                      x_GetVolumeName(part_info.data_file, i),
                                      GetStat());
        }
    }

    CFastWriteGuard guard(m_DBFilesMutex);

    TPartFilesMap& result_map = m_DBFiles[part_info.part_id];
    result_map.swap(temp_map);
    return result_map;
}

inline void
CNCBlobStorage::x_PrepFilePoolsToDelete(TNCDBPartId    part_id,
                                        TPartFilesMap* pools_map)
{
    CFastWriteGuard guard(m_DBFilesMutex);

    TFilesMap::iterator it = m_DBFiles.find(part_id);
    _ASSERT(it != m_DBFiles.end());
    pools_map->swap(it->second);
    m_DBFiles.erase(it);
}

inline void
CNCBlobStorage::x_CreateNewDBStructure(const SNCDBPartInfo& part_info)
{
    for (TNCDBVolumeId i = 1; i <= part_info.cnt_volumes; ++i) {
        TMetaFileLock metafile(this, part_info.part_id, i);
        metafile->CreateDatabase();
        TDataFileLock datafile(this, part_info.part_id, i);
        datafile->CreateDatabase();
    }
}

inline void
CNCBlobStorage::x_SwitchCurrentDBPart(SNCDBPartInfo* part_info)
{
    CFastWriteGuard guard(m_DBPartsLock);

    m_LastBlobLock.Lock();
    ++m_LastBlob.blob_id;
    m_LastPart             = part_info;
    m_LastBlob.part_id     = part_info->part_id;
    m_LastBlob.volume_id   = x_GetMinVolumeId(*part_info);
    m_DBParts.push_back(part_info);
    m_LastBlobLock.Unlock();
}

void
CNCBlobStorage::x_CreateNewDBPart(void)
{
    // No need in mutex because m_LastPartId is changed only here and will be
    // executed only from one thread.
    TNCDBPartId part_id = m_LastBlob.part_id + 1;
    string meta_name = x_GetMetaFileName(part_id);
    string data_name = x_GetDataFileName(part_id);
    TNCDBVolumeId cnt_vols = m_DBCntVolumes;

    SNCDBPartInfo* part_info = new SNCDBPartInfo;
    part_info->part_id     = part_id;
    part_info->meta_file   = meta_name;
    part_info->data_file   = data_name;
    part_info->cnt_volumes = cnt_vols;
    part_info->meta_size   = part_info->data_size = 0;

    try {
        TPartFilesMap& pools_map = x_InitFilePools(*part_info);
        // Make sure that there's no such files from some previous runs.
        // As long as we didn't make any connections to databases we can do
        // the deletion safely.
        for (TNCDBVolumeId i = 1; i <= cnt_vols; ++i) {
            // GCC 3.0.4 doesn't allow to merge the following lines
            CFile meta_obj(pools_map[i]->GetMetaFileName());
            meta_obj.Remove();
            CFile data_obj(pools_map[i]->GetDataFileName());
            data_obj.Remove();
        }
        x_CreateNewDBStructure(*part_info);
        for (TNCDBVolumeId i = 1; i <= cnt_vols; ++i) {
            CNCFileSystem::SetFileInitialized(pools_map[i]->GetMetaFileName());
            CNCFileSystem::SetFileInitialized(pools_map[i]->GetDataFileName());
        }

        // No need of mutex on using index database because it is used only
        // in one background thread (after using in constructor).
        m_IndexDB->CreateDBPart(part_info);
        x_SwitchCurrentDBPart(part_info);
    }
    catch (exception& ex) {
        x_MonitorError(ex, "Error while creating new database part");

        TPartFilesMap pools_map;
        x_PrepFilePoolsToDelete(part_id, &pools_map);
        // First schedule the deletion
        ITERATE(TPartFilesMap, it, pools_map) {
            CNCFileSystem::DeleteFileOnClose(it->second->GetMetaFileName());
            CNCFileSystem::DeleteFileOnClose(it->second->GetDataFileName());
        }
        // Then close all connections.
        pools_map.clear();
        delete part_info;

        // Re-throw exception after cleanup to break application if storage
        // cannot create any database parts during initialization.
        throw;
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

void
CNCBlobStorage::x_DeleteDBPart(TNCDBPartsList::iterator part_it)
{
    TNCDBPartId part_id = (*part_it)->part_id;
    {{
        CFastWriteGuard guard(m_DBPartsLock);
        delete *part_it;
        m_DBParts.erase(part_it);
    }}
    try {
        m_IndexDB->DeleteDBPart(part_id);
    }
    catch (exception& ex) {
        x_MonitorError(ex, "Index database does not delete rows");
    }

    TPartFilesMap pools_map;
    x_PrepFilePoolsToDelete(part_id, &pools_map);
    // First schedule the deletion
    ITERATE(TPartFilesMap, it, pools_map) {
        CNCFileSystem::DeleteFileOnClose(it->second->GetMetaFileName());
        CNCFileSystem::DeleteFileOnClose(it->second->GetDataFileName());
    }
    // Then close all connections.
    pools_map.clear();
}

inline void
CNCBlobStorage::x_RotateDBParts(bool force_rotate /* = false */)
{
    SNCDBPartInfo* last_part = m_DBParts.back();
    if (!force_rotate
        &&  int(time(NULL)) - last_part->last_rot_time < m_DBRotatePeriod)
    {
        return;
    }

    bool need_create_new = false;
    if (!force_rotate) {
        try {
            for (TNCDBVolumeId i = x_GetMinVolumeId(*last_part);
                               i <= last_part->cnt_volumes; ++i)
            {
                TMetaFileLock metafile(this, last_part->part_id, i);
                need_create_new |= metafile->HasAnyBlobs();
            }
        }
        catch (exception& ex) {
            CQuickStrStream msg;
            msg << "Error reading information from part "
                << last_part->part_id;
            x_MonitorError(ex, msg);
            need_create_new = true;
        }
    }
    if (need_create_new) {
        try {
            x_CreateNewDBPart();
        }
        catch (exception& ex) {
            x_MonitorError(ex, "Cannot create new database part while rotating");
            last_part->last_rot_time = int(time(NULL));
        }
    }
    else {
        last_part->last_rot_time = int(time(NULL));
    }
}

inline void
CNCBlobStorage::x_ReadLastBlobCoords(void)
{
    while (!m_DBParts.empty()) {
        m_LastPart           = m_DBParts.back();
        m_LastBlob.part_id   = m_LastPart->part_id;
        m_LastBlob.volume_id = x_GetMinVolumeId(*m_LastPart);
        m_LastBlob.blob_id   = 0;
        bool was_error       = false;
        for (TNCDBVolumeId i = x_GetMinVolumeId(*m_LastPart);
                           i <= m_LastPart->cnt_volumes; ++i)
        {
            try {
                TMetaFileLock last_meta(this, m_LastPart->part_id, i);
                m_LastBlob.blob_id = max(m_LastBlob.blob_id,
                                         last_meta->GetLastBlobId());
            }
            catch (exception& ex) {
                CQuickStrStream msg;
                msg << "Error reading last blob id from volume " << i
                    << " of part " << m_LastBlob.part_id;
                x_MonitorError(ex, msg);
                was_error = true;
            }
        }
        if (m_LastBlob.blob_id == 0) {
            x_DeleteDBPart(--m_DBParts.end());
            continue;
        }
        if (was_error) {
            x_RotateDBParts(true);
        }
        break;
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

    if (IsReadOnly()  &&  reinit_mode != eNoReinit) {
        ERR_POST_X(12, Warning
                   << "Read-only storage is asked to be re-initialized. "
                      "Ignoring this.");
        reinit_mode = eNoReinit;
    }
    bool guard_existed = x_LockInstanceGuard();

    x_OpenIndexDB(&reinit_mode);
    x_CheckDBInitialized(guard_existed, reinit_mode,
                         reg.GetBool(section, kNCStorage_DropDirtyParam,
                                     false, 0, IRegistry::eErrPost));

    m_LastDeadTime = int(time(NULL));
    TNCDBPartId last_part_id = 0;
    if (!m_DBParts.empty()) {
        last_part_id = m_DBParts.back()->part_id;
        ITERATE(TNCDBPartsList, it, m_DBParts) {
            SNCDBPartInfo* part_info = *it;
            TPartFilesMap& files_map = x_InitFilePools(*part_info);
            for (TNCDBVolumeId i = x_GetMinVolumeId(*part_info);
                               i <= part_info->cnt_volumes; ++i)
            {
                CNCFileSystem::SetFileInitialized(files_map[i]->GetMetaFileName());
                CNCFileSystem::SetFileInitialized(files_map[i]->GetDataFileName());
            }
        }
        x_ReadLastBlobCoords();
    }
    if (m_DBParts.empty()) {
        m_LastBlob.part_id = last_part_id;
        m_LastBlob.blob_id = 0;
        x_CreateNewDBPart();
    }
    x_SetNotCachedPartId(m_LastBlob.part_id);

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

    ITERATE(TNCDBPartsList, it, m_DBParts) {
        delete *it;
    }
    m_DBParts.clear();

    x_FreeBlobIdLocks();
    x_UnlockInstanceGuard();
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
    TKeyIdMap::const_iterator it = m_KeysCache.find(identity);
    if (it != m_KeysCache.end()) {
        identity->AssignCoords(**it);
        return true;
    }
    return false;
}

inline bool
CNCBlobStorage::x_CreateBlobInCache(const SNCBlobIdentity& identity,
                                    SNCBlobCoords*         new_coords)
{
    SNCBlobIdentity* cache_ident = new SNCBlobIdentity(identity);
    {{
        CFastWriteGuard guard(m_KeysCacheLock);
        pair<TKeyIdMap::iterator, bool> ins_pair
                                            = m_KeysCache.insert(cache_ident);
        if (!ins_pair.second) {
            delete cache_ident;
            new_coords->AssignCoords(**ins_pair.first);
            return false;
        }
    }}
    {{
        CFastWriteGuard guard(m_IdsCacheLock);
        _VERIFY(m_IdsCache.insert(cache_ident).second);
    }}
    m_CntBlobs.Add(1);
    return true;
}

inline void
CNCBlobStorage::x_MoveBlobInCache(const SNCBlobIdentity& identity,
                                  const SNCBlobCoords&   new_coords)
{
    {{
        CFastWriteGuard guard(m_IdsCacheLock);
        _VERIFY(m_IdsCache.erase(const_cast<SNCBlobIdentity*>(&identity)) == 1);
    }}
    SNCBlobIdentity* stored_ident;
    {{
        // We use write guard here because later changing of coordinates can
        // have a race with x_ReadBlobCoordsFromCache() and write guard is the
        // simplest method to avoid this race without introducing additional
        // level of complexity.
        CFastWriteGuard guard(m_KeysCacheLock);
        TKeyIdMap::const_iterator it
                    = m_KeysCache.find(const_cast<SNCBlobIdentity*>(&identity));
        _ASSERT(it != m_KeysCache.end());
        stored_ident = *it;
        stored_ident->AssignCoords(new_coords);
    }}
    {{
        CFastWriteGuard guard(m_IdsCacheLock);
        _VERIFY(m_IdsCache.insert(stored_ident).second);
    }}
}

inline bool
CNCBlobStorage::x_ReadBlobKeyFromCache(SNCBlobIdentity* identity)
{
    CFastReadGuard guard(m_IdsCacheLock);
    TIdKeyMap::const_iterator it = m_IdsCache.find(identity);
    if (it != m_IdsCache.end()) {
        *identity = **it;
        return true;
    }
    return false;
}

inline void
CNCBlobStorage::x_DeleteBlobFromCache(const SNCBlobCoords& coords)
{
    SNCBlobIdentity identity(coords);
    SNCBlobIdentity* cache_ident;
    {{
        CFastWriteGuard guard(m_IdsCacheLock);
        TIdKeyMap::iterator it = m_IdsCache.find(&identity);
        // Blob will not be found if somebody else has already deleted it.
        // But it should not ever occur because deleting is mutually
        // exclusive by blob locking.
        _ASSERT(it != m_IdsCache.end());
        cache_ident = *it;
        identity = *cache_ident;
        m_IdsCache.erase(it);
    }}
    {{
        CFastWriteGuard guard(m_KeysCacheLock);
        m_KeysCache.erase(cache_ident);
    }}
    delete cache_ident;
    m_CntBlobs.Add(-1);
}

inline void
CNCBlobStorage::x_FillBlobIdsFromCache(const string& key, TNCIdsList* id_list)
{
    CFastReadGuard guard(m_KeysCacheLock);

    SNCBlobIdentity identity(key);
    TKeyIdMap::const_iterator it = m_KeysCache.lower_bound(&identity);
    while (it != m_KeysCache.end()  &&  (*it)->key == key) {
        id_list->push_back((*it)->blob_id);
        ++it;
    }
}

inline bool
CNCBlobStorage::x_IsBlobExistsInCache(CTempString key, CTempString subkey)
{
    SNCBlobIdentity identity(key, subkey, 0);
    CFastReadGuard guard(m_KeysCacheLock);
    TKeyIdMap::const_iterator it = m_KeysCache.lower_bound(&identity);
    return it != m_KeysCache.end()
           &&  (*it)->key == key  &&  (*it)->subkey == subkey;
}

inline void
CNCBlobStorage::x_FillCacheFromDBVolume(TNCDBPartId   part_id,
                                        TNCDBVolumeId vol_id)
{
    TMetaFileLock metafile(this, part_id, vol_id);
    TNCIdsList id_list;
    TNCBlobId last_id = 0;
    // Hint to the compiler that this code executes in the thread where
    // m_LastDeadTime changes (nobody changes it in background).
    int dead_after = const_cast<int&>(m_LastDeadTime);
    do {
        x_CheckStopped();
        metafile->GetBlobIdsList(dead_after, last_id,
                                 numeric_limits<int>::max(),
                                 m_BlobsBatchSize, &id_list);
        ITERATE(TNCIdsList, id_it, id_list) {
            TNCBlobLockHolderRef blob_lock
                        = x_GetBlobAccess(eRead, part_id, vol_id, *id_it);
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
    }
    while (!id_list.empty());
}

inline void
CNCBlobStorage::x_FillCacheFromDBPart(TNCDBPartId part_id)
{
    // Nothing changes m_DBFiles somewhere in background.
    TPartFilesMap& files_map = m_DBFiles[part_id];
    ITERATE(TPartFilesMap, it, files_map) {
        try {
            x_FillCacheFromDBVolume(part_id, it->first);
        }
        catch (exception& ex) {
            // Try to recover from any database errors by just
            // ignoring it and trying to work further.
            CQuickStrStream msg;
            msg << "BG: Database volume " << it->first << " of part "
                << part_id << " was not cached";
            x_MonitorError(ex, msg);
        }
    }
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
        CFastReadGuard guard(m_DBPartsLock);

        bool do_check = false;
        // With REVERSE_ITERATE code should look more ugly to be compilable
        // in WorkShop.
        NON_CONST_REVERSE_ITERATE(TNCDBPartsList, it, m_DBParts) {
            SNCDBPartInfo* part_info = *it;
            if (part_info->part_id == check_part_id) {
                do_check = true;
            }
            if (do_check) {
                for (TNCDBVolumeId i = x_GetMinVolumeId(*part_info);
                                   i <= part_info->cnt_volumes; ++i)
                {
                    try {
                        TMetaFileLock metafile(this, part_info->part_id, i);
                        if (metafile->ReadBlobId(identity, dead_time)) {
                            identity->part_id   = part_info->part_id;
                            identity->volume_id = i;
                            break;
                        }
                    }
                    catch (exception& ex) {
                        CQuickStrStream msg;
                        msg << "Error reading blob id from volume " << i
                            << " of part " << part_info->part_id;
                        x_MonitorError(ex, msg);
                        // Will try other parts - maybe blob is there.
                    }
                }
                // blob_id will be read inside ReadBlobId() and found blob is
                // the only reason we break here (and broke from inner loop).
                if (identity->blob_id)
                    break;
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
        if (ReadBlobCoords(&temp_ident)) {
            new_coords->AssignCoords(temp_ident);
            GetStat()->AddCreateHitExisting();
            return false;
        }
    }
    if (!x_CreateBlobInCache(identity, new_coords)) {
        GetStat()->AddCreateHitExisting();
        return false;
    }
    try {
        TMetaFileLock metafile(this, identity.part_id, identity.volume_id);
        metafile->CreateBlobKey(identity);
        return true;
    }
    catch (exception& ex) {
        x_MonitorError(ex, "Error creating new blob in meta file");
        x_DeleteBlobFromCache(identity);
        // Force CNCBlobLockHolder to retry creation.
        GetNextBlobCoords(new_coords);
        return false;
    }
}

bool
CNCBlobStorage::MoveBlobIfNeeded(const SNCBlobIdentity& identity,
                                 const SNCBlobCoords&   new_coords)
{
    if (x_GetNotCachedPartId() != -1
        ||  identity.part_id   == new_coords.part_id
        // Protection from errors in file system when new_coords can point
        // to some old part_id.
        ||  new_coords.part_id != m_LastPart->part_id)
    {
        return false;
    }

    SNCBlobIdentity new_ident(identity);
    new_ident.AssignCoords(new_coords);
    try {
        {{
            TMetaFileLock metafile(this, new_ident.part_id, new_ident.volume_id);
            metafile->CreateBlobKey(new_ident);
        }}
        {{
            TMetaFileLock metafile(this, identity.part_id, identity.volume_id);
            metafile->SetBlobDeadTime(identity.blob_id, m_LastDeadTime - 1);
        }}
    }
    catch (exception& ex) {
        x_MonitorError(ex, "Error attempting to move the blob");
        try {
            TMetaFileLock metafile(this, new_ident.part_id, new_ident.volume_id);
            metafile->SetBlobDeadTime(new_ident.blob_id, m_LastDeadTime - 1);
        }
        catch (exception& inner_ex) {
            x_MonitorError(inner_ex, "Error recovering after failed move. "
                                     "Database can be in inconsistent state");
        }
        return false;
    }
    x_MoveBlobInCache(identity, new_ident);
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
        try {
            TMetaFileLock metafile(this, identity->part_id, identity->volume_id);
            if (metafile->ReadBlobKey(identity, dead_time)) {
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
        catch (exception& ex) {
            CQuickStrStream msg;
            msg << "Error reading blob key from volume " << identity->volume_id
                << " of part " << identity->part_id;
            x_MonitorError(ex, msg);
        }
    }
    return false;
}

void
CNCBlobStorage::DeleteBlob(const SNCBlobCoords& coords)
{
    x_DeleteBlobFromCache(coords);

    try {
        TMetaFileLock metafile(this, coords.part_id, coords.volume_id);
        // Just hide this blob from GC - pretend that it was already expired.
        metafile->SetBlobDeadTime(coords.blob_id, m_LastDeadTime - 1);
    }
    catch (exception& ex) {
        CQuickStrStream msg;
        msg << "Error deleting blob from volume " << coords.volume_id
            << " of part " << coords.part_id;
        x_MonitorError(ex, msg);
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
        CFastReadGuard guard(m_DBPartsLock);

        bool do_check = false;
        // With REVERSE_ITERATE code should look more ugly to be compilable
        // in WorkShop.
        NON_CONST_REVERSE_ITERATE(TNCDBPartsList, it, m_DBParts) {
            SNCDBPartInfo* part_info = *it;
            if (part_info->part_id == check_part_id) {
                do_check = true;
            }
            if (do_check) {
                for (TNCDBVolumeId i = x_GetMinVolumeId(*part_info);
                                   i <= part_info->cnt_volumes; ++i)
                {
                    try {
                        TMetaFileLock metafile(this, part_info->part_id, i);
                        if (metafile->IsBlobExists(key, subkey, dead_time))
                            return true;
                    }
                    catch (exception& ex) {
                        CQuickStrStream msg;
                        msg << "Error checking blob existence in volume "
                            << i << " of part " << part_info->part_id;
                        x_MonitorError(ex, msg);
                    }
                }
            }
        }
    }
    return false;
}

void
CNCBlobStorage::PrintStat(CPrintTextProxy& proxy)
{
    CFastReadGuard guard(m_DBPartsLock);

    proxy << "Usage statistics for storage '"
                    << m_Name << "' at " << m_Path << ":" << endl
          << endl
          << "Currently in database - "
                    << m_CntBlobs.Get() << " blobs, "
                    << m_DBParts.size() << " parts with "
                    << m_CurDBSize << " bytes (diff for ids "
                    << m_DBParts.back()->part_id
                                        - m_DBParts.front()->part_id << ")" << endl
          << "List of database parts:" << endl;
    NON_CONST_ITERATE(TNCDBPartsList, it, m_DBParts) {
        SNCDBPartInfo* part_info = *it;
        CTime create_time(time_t(part_info->create_time));
        create_time.ToLocalTime();
        proxy << part_info->part_id << " - "
                        << part_info->meta_size      << " (meta), "
                        << part_info->data_size      << " (data), "
                        << part_info->meta_size_diff << " (meta diff), "
                        << part_info->data_size_diff << " (data diff), "
                        << create_time               << " (created)" << endl;
    }
    proxy << endl;
    m_Stat.Print(proxy);
}

inline bool
CNCBlobStorage::x_GC_DeleteExpired(TNCDBPartId   part_id,
                                   TNCDBVolumeId vol_id,
                                   TNCBlobId     blob_id)
{
    TNCBlobLockHolderRef blob_lock
                          = x_GetBlobAccess(eWrite, part_id, vol_id, blob_id);
    GetStat()->AddGCLockRequest();
    if (blob_lock->IsLockAcquired()) {
        GetStat()->AddGCLockAcquired();
        if (blob_lock->IsBlobExists()  &&  blob_lock->IsBlobExpired()) {
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
            SNCBlobCoords coords(part_id, vol_id, blob_id);
            x_DeleteBlobFromCache(coords);
        }
        blob_lock->ReleaseLock();
    }
    else {
        // Lock has not been acquired - we will need to try once more.
        return false;
    }
    return true;
}

inline bool
CNCBlobStorage::x_GC_IsDBPartEmpty(const SNCDBPartInfo& part_info,
                                   int                  dead_after)
{
    for (TNCDBVolumeId i = x_GetMinVolumeId(part_info);
                       i <= part_info.cnt_volumes; ++i)
    {
        try {
            TMetaFileLock metafile(this, part_info.part_id, i);
            if (metafile->HasLiveBlobs(dead_after))
                return false;
        }
        catch (exception& ex) {
            CQuickStrStream msg;
            msg << "BG: Error checking live blobs in volume " << i
                << " of part " << part_info.part_id
                << " - assuming it is empty";
            x_MonitorError(ex, msg);
        }
    }
    return true;
}

inline bool
CNCBlobStorage::x_GC_CleanDBVolume(TNCDBPartId   part_id,
                                   TNCDBVolumeId vol_id,
                                   int           dead_before)
{
    TMetaFileLock metafile(this, part_id, vol_id);
    TNCIdsList ids;
    int dead_after = const_cast<int&>(m_LastDeadTime);
    TNCBlobId last_id = 0;
    bool can_shift = true;
    do {
        x_CheckStopped();
        x_MonitorPost("BG: Starting next GC batch");
        metafile->GetBlobIdsList(dead_after, last_id, dead_before,
                                 m_BlobsBatchSize, &ids);
        ITERATE(TNCIdsList, id_it, ids) {
            x_CheckStopped();
            can_shift &= x_GC_DeleteExpired(part_id, vol_id, *id_it);
        }
        x_MonitorPost("BG: Batch processing is finished");
        m_StopTrigger.TryWait(m_GCBatchSleep / 1000,
                              m_GCBatchSleep % 1000 * 1000000);
    }
    while (!ids.empty());

    return can_shift;
}

inline bool
CNCBlobStorage::x_GC_CleanDBPart(TNCDBPartsList::iterator part_it,
                                 int                      dead_before)
{
    bool can_shift           = true;
    SNCDBPartInfo* part_info = *part_it;
    TNCDBPartId part_id      = part_info->part_id;
    for (TNCDBVolumeId i = x_GetMinVolumeId(*part_info);
                       i <= part_info->cnt_volumes; ++i)
    {
        try {
            can_shift &= x_GC_CleanDBVolume(part_id, i, dead_before);
        }
        catch (exception& ex) {
            CQuickStrStream msg;
            msg << "BG: Error processing volume " << i << " in part "
                << part_id << ". Part will be deleted";
            x_MonitorError(ex, msg);
        }
    }
    if (can_shift  &&  part_id != m_LastBlob.part_id
        &&  x_GC_IsDBPartEmpty(*part_info, dead_before))
    {
        x_DeleteDBPart(part_it);
    }
    return can_shift;
}

inline void
CNCBlobStorage::x_GC_CollectPartsStatistics(void)
{
    // Nobody else changes m_DBParts, so working with it right away without
    // any mutexes.
    GetStat()->AddNumberOfDBParts(m_DBParts.size(),
                      m_DBParts.back()->part_id - m_DBParts.front()->part_id);
    GetStat()->AddNumberOfBlobs(m_CntBlobs.Get());

    Int8 total_meta = 0, total_data = 0;
    // With ITERATE code should look more ugly to be
    // compilable in WorkShop.
    NON_CONST_ITERATE(TNCDBPartsList, it, m_DBParts) {
        SNCDBPartInfo* part_info = *it;
        Int8 meta_size = 0, max_meta = 0;
        Int8 min_meta = numeric_limits<Int8>::max();
        Int8 data_size = 0, max_data = 0;
        Int8 min_data = numeric_limits<Int8>::max();
        const TPartFilesMap& files_map = m_DBFiles[part_info->part_id];
        ITERATE(TPartFilesMap, vol_it, files_map) {
            Int8 this_meta = CFile(vol_it->second->GetMetaFileName()).GetLength();
            Int8 this_data = CFile(vol_it->second->GetDataFileName()).GetLength();
            meta_size += this_meta;
            data_size += this_data;
            min_meta = min(min_meta, this_meta);
            min_data = min(min_data, this_data);
            max_meta = max(max_meta, this_meta);
            max_data = max(max_data, this_data);
            GetStat()->AddDBFilesSizes(this_meta, this_data);
        }
        part_info->meta_size = meta_size;
        part_info->data_size = data_size;
        part_info->meta_size_diff = max_meta - min_meta;
        part_info->data_size_diff = max_data - min_data;
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
        // Nobody else changes m_DBParts, so iterating right over it.
        // With REVERSE_ITERATE code should look more ugly to be
        // compilable in WorkShop.
        NON_CONST_REVERSE_ITERATE(TNCDBPartsList, part_it, m_DBParts) {
            TNCDBPartId part_id = (*part_it)->part_id;
            x_SetNotCachedPartId(part_id);
            x_FillCacheFromDBPart(part_id);
        }
        x_SetNotCachedPartId(-1);

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
