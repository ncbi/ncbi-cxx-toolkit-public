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
#include "netcached.hpp"
#include "nc_stat.hpp"
#include "error_codes.hpp"
#include "message_handler.hpp"


BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X  NetCache_Storage


static const char* kNCStorage_DBFileExt       = ".db";
static const char* kNCStorage_IndexFileSuffix = ".index";
static const char* kNCStorage_MetaFileSuffix  = ".meta.";
static const char* kNCStorage_DataFileSuffix  = ".data.";
static const char* kNCStorage_StartedFileName = "__ncbi_netcache_started__";

static const char* kNCStorage_RegSection        = "storage";
static const char* kNCStorage_PathParam         = "path";
static const char* kNCStorage_FilePrefixParam   = "prefix";
static const char* kNCStorage_PartsCntParam     = "parts_cnt";
static const char* kNCStorage_GCDelayParam      = "gc_delay";
static const char* kNCStorage_GCBatchParam      = "gc_batch_size";
static const char* kNCStorage_MaxMetaSizeParam  = "max_metafile_size";
static const char* kNCStorage_MaxDataSizeParam  = "max_datafile_size";
static const char* kNCStorage_MinMetaSizeParam  = "min_metafile_size";
static const char* kNCStorage_MinDataSizeParam  = "min_datafile_size";
static const char* kNCStorage_MinUsfulMetaParam = "min_useful_meta_pct";
static const char* kNCStorage_MinUsfulDataParam = "min_useful_data_pct";
static const char* kNCStorage_ExtraGCOnParam    = "db_limit_del_old_on";
static const char* kNCStorage_ExtraGCOffParam   = "db_limit_del_old_off";
static const char* kNCStorage_ExtraGCStepParam  = "delete_old_time_step";
static const char* kNCStorage_StopWriteOnParam  = "db_limit_stop_write_on";
static const char* kNCStorage_StopWriteOffParam = "db_limit_stop_write_off";
static const char* kNCStorage_MinFreeDiskParam  = "disk_free_limit";



/// Special exception to terminate background thread when storage is
/// destroying. Class intentionally made without inheritance from CException
/// or std::exception.
class CNCStorage_StoppedException
{};

///
class CNCStorage_BlockedException
{};

///
class CNCStorage_RecacheException
{};



const char*
CNCBlobStorageException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
    case eUnknown:      return "eUnknown";
    case eWrongConfig:  return "eWrongConfig";
    case eCorruptedDB:  return "eCorruptedDB";
    case eWrongBlock:   return "eWrongBlock";
    default:            return CException::GetErrCodeString();
    }
}



inline void
CNCBlobStorage::x_EnsureDirExist(const string& dir_name)
{
    CDir dir(dir_name);
    if (!dir.Exists()) {
        dir.Create();
    }
}

void
CNCBlobStorage::x_ReadVariableParams(void)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    m_GCRunDelay  = reg.GetInt(kNCStorage_RegSection, kNCStorage_GCDelayParam,
                               30,  0, IRegistry::eErrPost);
    m_GCBatchSize = reg.GetInt(kNCStorage_RegSection, kNCStorage_GCBatchParam,
                               500, 0, IRegistry::eErrPost);

    string max_size_str_m = reg.GetString(kNCStorage_RegSection,
                                          kNCStorage_MaxMetaSizeParam, "3Mb");
    string max_size_str_d = reg.GetString(kNCStorage_RegSection,
                                          kNCStorage_MaxDataSizeParam, "300Mb");
    string min_size_str_m = reg.GetString(kNCStorage_RegSection,
                                          kNCStorage_MinMetaSizeParam, "1Mb");
    string min_size_str_d = reg.GetString(kNCStorage_RegSection,
                                          kNCStorage_MinDataSizeParam, "100Mb");
    try {
        m_MaxFileSize[eNCMeta] = NStr::StringToUInt8_DataSize(max_size_str_m);
        m_MaxFileSize[eNCData] = NStr::StringToUInt8_DataSize(max_size_str_d);
        m_MinFileSize[eNCMeta] = NStr::StringToUInt8_DataSize(min_size_str_m);
        m_MinFileSize[eNCData] = NStr::StringToUInt8_DataSize(min_size_str_d);
    }
    catch (CStringException& ex) {
        ERR_POST_X(21, "Error in meta/data min/max size parameters: " << ex);
        m_MaxFileSize[eNCMeta] = 3000000;
        m_MaxFileSize[eNCData] = 300000000;
        m_MinFileSize[eNCMeta] = 1000000;
        m_MinFileSize[eNCData] = 100000000;
    }
    m_MinUsefulPct[eNCMeta] = double(reg.GetInt(kNCStorage_RegSection,
                                                kNCStorage_MinUsfulMetaParam,
                                                50, 0, IRegistry::eErrPost)) / 100;
    m_MinUsefulPct[eNCData] = double(reg.GetInt(kNCStorage_RegSection,
                                                kNCStorage_MinUsfulDataParam,
                                                50, 0, IRegistry::eErrPost)) / 100;

    m_ExtraGCOnSize  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_ExtraGCOnParam, "50Gb"));
    m_ExtraGCOffSize = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_ExtraGCOffParam, "45Gb"));
    m_ExtraGCStep    = reg.GetInt(kNCStorage_RegSection,
                                  kNCStorage_ExtraGCStepParam, 900);
    m_StopWriteOnSize  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_StopWriteOnParam, "100Gb"));
    m_StopWriteOffSize = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_StopWriteOffParam, "90Gb"));
    m_DiskFreeLimit  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_MinFreeDiskParam, "5Gb"));

    int parts_cnt = reg.GetInt(kNCStorage_RegSection, kNCStorage_PartsCntParam,
                               5, 0, IRegistry::eErrPost);
    if (parts_cnt <= 0) {
        NCBI_THROW(CNCBlobStorageException, eWrongConfig,
                   "Number of database parts cannot be less or equal to 0");
    }
    m_PartsPaths.resize(parts_cnt);
    string path_param_prefix(kNCStorage_PathParam);
    path_param_prefix += "_";
    for (int i = 0; i < parts_cnt; ++i) {
        string param(path_param_prefix);
        param += NStr::IntToString(i + 1);
        string this_path(reg.GetString(kNCStorage_RegSection, param, m_MainPath));
        m_PartsPaths[i] = this_path;
        x_EnsureDirExist(this_path);
    }
    m_CurFiles[eNCMeta].resize(parts_cnt);
    m_CurFiles[eNCData].resize(parts_cnt);
}

void
CNCBlobStorage::x_ReadStorageParams(void)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    m_MainPath = reg.Get(kNCStorage_RegSection, kNCStorage_PathParam);
    m_Prefix   = reg.Get(kNCStorage_RegSection, kNCStorage_FilePrefixParam);
    if (m_MainPath.empty()  ||  m_Prefix.empty()) {
        NCBI_THROW_FMT(CNCBlobStorageException, eWrongConfig,
                       "Incorrect parameters for " << kNCStorage_PathParam
                       << " and " << kNCStorage_FilePrefixParam
                       << " in the section '" << kNCStorage_RegSection << "': '"
                       << m_MainPath << "' and '" << m_Prefix << "'");
    }
    x_EnsureDirExist(m_MainPath);
    m_GuardName = CDirEntry::MakePath(m_MainPath,
                                      kNCStorage_StartedFileName,
                                      m_Prefix);

    x_ReadVariableParams();
}

string
CNCBlobStorage::x_GetIndexFileName(void)
{
    string file_name(m_Prefix);
    file_name += kNCStorage_IndexFileSuffix;
    file_name = CDirEntry::MakePath(m_MainPath, file_name, kNCStorage_DBFileExt);
    return CDirEntry::CreateAbsolutePath(file_name);
}

string
CNCBlobStorage::x_GetFileName(ENCDBFileType file_type,
                              unsigned int  part_num,
                              TNCDBFileId   file_id)
{
    string file_name(m_Prefix);
    file_name += (file_type == eNCMeta? kNCStorage_MetaFileSuffix
                                      : kNCStorage_DataFileSuffix);
    file_name += NStr::UIntToString(file_id);
    file_name = CDirEntry::MakePath(m_PartsPaths[part_num], file_name,
                                    kNCStorage_DBFileExt);
    return CDirEntry::CreateAbsolutePath(file_name);
}

void
CNCBlobStorage::Block(void)
{
/*
    CSpinGuard guard(m_HoldersPoolLock);

    if (m_Blocked) {
        NCBI_THROW(CNCBlobStorageException, eWrongBlock,
                   "Only one blocking can occur at a time");
    }

    _ASSERT(m_CntLocksToWait == 0);
    m_CntLocksToWait = m_CntUsedHolders;
    m_Blocked = true;
*/
    abort();
}

void
CNCBlobStorage::Unblock(void)
{
/*
    _ASSERT(m_Blocked);

    CSpinGuard guard(m_HoldersPoolLock);

    if (m_NotCachedFileId != -1)
        m_NeedRecache = true;
    m_Blocked = false;
    while (m_UsedAccessors) {
        CNCBlobAccessor* holder = m_UsedAccessors;
        _ASSERT(!holder->IsInitialized());
        holder->RemoveFromList(m_UsedAccessors);
        x_InitializeAccessor(holder);
    }
*/
    abort();
}

void
CNCBlobStorage::x_LockInstanceGuard(void)
{
    m_CleanStart = !CFile(m_GuardName).Exists();

    if (m_CleanStart) {
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
    if (!m_CleanStart  &&  CFile(m_GuardName).GetLength() == 0)
        m_CleanStart = true;
    if (!m_CleanStart) {
        INFO_POST("NetCache wasn't finished cleanly in previous run. "
                  "Will try to work with storage as is.");
    }

    CFileWriter writer(m_GuardLock->GetFileHandle());
    string pid(NStr::UIntToString(CProcess::GetCurrentPid()));
    writer.Write(pid.c_str(), pid.size());
}

inline void
CNCBlobStorage::x_UnlockInstanceGuard(void)
{
    _ASSERT(m_GuardLock.get());

    m_GuardLock.reset();
    CFile(m_GuardName).Remove();
}

void
CNCBlobStorage::x_OpenIndexDB(void)
{
    string index_name = x_GetIndexFileName();
    for (int i = 0; i < 2; ++i) {
        try {
            CNCFileSystem::OpenNewDBFile(index_name, true);
            m_IndexDB = new CNCDBIndexFile(index_name);
            m_IndexDB->CreateDatabase(eNCIndex);
            m_IndexDB->GetAllDBFiles(&m_DBFiles);
            return;
        }
        catch (CSQLITE_Exception& ex) {
            m_IndexDB.reset();
            ERR_POST_X(1, "Index file is broken, reinitializing storage. " << ex);
            CFile(index_name).Remove();
        }
    }
    NCBI_THROW(CNCBlobStorageException, eCorruptedDB,
               "Cannot open or create index file for the storage");
}

void
CNCBlobStorage::x_CleanDatabase(void)
{
    INFO_POST("Reinitializing storage " << m_Prefix << " at " << m_MainPath);

    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        CNCFileSystem::DeleteFileOnClose(it->second->file_name);
        delete it->second;
    }
    m_DBFiles.clear();
    m_IndexDB->DeleteAllDBFiles();
}

void
CNCBlobStorage::x_ReinitializeStorage(void)
{
    try {
        x_CleanDatabase();
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(1, "Error in soft reinitialization, trying harder. " << ex);
        m_IndexDB.reset();
        // GCC 3.0.4 doesn't allow to merge the following 2 lines
        CFile file_obj(x_GetIndexFileName());
        file_obj.Remove();
        x_OpenIndexDB();
    }
}

void
CNCBlobStorage::x_CreateNewFile(ENCDBFileType file_type, unsigned int part_num)
{
    // No need in mutex because m_LastBlob is changed only here and will be
    // executed only from one thread.
    TNCDBFileId file_id = m_LastFileId + 1;
    string file_name = x_GetFileName(file_type, part_num, file_id);

    try {
        // Make sure that there's no such files from some previous runs.
        // As long as we didn't make any connections to databases we can do
        // the deletion safely.
        CFile file_obj(file_name);
        file_obj.Remove();

        TNCDBFilePtr file;
        if (file_type == eNCMeta) {
            file.reset(new CNCDBMetaFile(file_name));
        }
        else {
            file.reset(new CNCDBDataFile(file_name));
        }
        file->CreateDatabase(file_type);
        CNCFileSystem::SetFileInitialized(file_name);

        // No need of mutex on using index database because it is used only
        // in one background thread (after using in constructor).
        {{
            CFastMutexGuard guard(m_IndexLock);
            m_IndexDB->NewDBFile(file_type, file_id, file_name);
        }}
        SNCDBFileInfo* file_info = new SNCDBFileInfo;
        file_info->file_id      = file_id;
        file_info->file_name    = file_name;
        file_info->create_time  = int(time(NULL));
        file_info->file_obj     = file;
        file_info->ref_cnt      = 0;
        file_info->useful_blobs = file_info->useful_size = 0;
        file_info->garbage_blobs = file_info->garbage_size = 0;
        {{
            CSpinWriteGuard guard(m_DBFilesLock);
            m_DBFiles[file_id] = file_info;
            m_LastFileId = file_id;
        }}

        m_CurFiles[file_type][part_num] = file_id;
    }
    catch (CException& ex) {
        ERR_POST_X(1, "Error while creating new database part: " << ex);
        CNCFileSystem::DeleteFileOnClose(file_name);

        // Re-throw exception after cleanup to break application if storage
        // cannot create database files during initialization.
        throw;
    }
}

void
CNCBlobStorage::x_CreateNewCurFiles(void)
{
    for (unsigned int i = 0; i < m_CurFiles[0].size(); ++i) {
        x_CreateNewFile(eNCMeta, i);
        x_CreateNewFile(eNCData, i);
    }
}

void
CNCBlobStorage::x_DeleteDBFile(TNCDBFilesMap::iterator files_it)
{
    TNCDBFileId  file_id  = files_it->first;
    TNCDBFilePtr file_ptr = files_it->second->file_obj;
    try {
        CFastMutexGuard guard(m_IndexLock);
        m_IndexDB->DeleteDBFile(file_id);
    }
    catch (CException& ex) {
        ERR_POST_X(1, "Index database does not delete rows: " << ex);
    }
    CNCFileSystem::DeleteFileOnClose(files_it->second->file_name);

    CSpinWriteGuard guard(m_DBFilesLock);
    delete files_it->second;
    m_DBFiles.erase(files_it);
}

void
CNCBlobStorage::x_ReadLastBlobCoords(void)
{
    ERASE_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        CNCDBFile* file = it->second->file_obj.get();
        if (file->GetType() != eNCMeta)
            continue;

        try {
            CNCDBMetaFile* metafile = static_cast<CNCDBMetaFile*>(file);
            m_LastBlob.blob_id = max(m_LastBlob.blob_id, metafile->GetLastBlobId());
            m_LastChunkId = max(m_LastChunkId, metafile->GetLastChunkId());
        }
        catch (CException& ex) {
            ERR_POST_X(1, "Error reading last blob id from file '"
                          << file->GetFileName() << "'. File will be deleted. "
                          << ex);
            x_DeleteDBFile(it);
        }
    }
}

CNCBlobStorage::CNCBlobStorage(bool do_reinit)
    : m_Stopped(false),
      m_StopTrigger(0, 100),
      m_FreeAccessors(NULL),
      m_UsedAccessors(NULL),
      m_CntUsedHolders(0),
      m_GCBlockWaiter(0, 1),
      m_Blocked(false),
      m_CntLocksToWait(0),
      m_IsStopWrite(false)
{
    x_ReadStorageParams();

    x_LockInstanceGuard();
    x_OpenIndexDB();
    if (do_reinit) {
        x_ReinitializeStorage();
        m_CleanStart = false;
    }

    m_LastDeadTime = int(time(NULL));
    m_LastBlob.meta_id = m_LastBlob.data_id = 0;
    m_LastBlob.blob_id = 0;
    m_LastChunkId = kNCMinChunkId;
    m_BlobGeneration = 1;
    m_LastFileId = 0;
    if (!m_DBFiles.empty()) {
        m_LastFileId = m_DBFiles.rbegin()->first;
        ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            CNCFileSystem::SetFileInitialized(it->second->file_name);
        }
        x_ReadLastBlobCoords();
    }
    x_CreateNewCurFiles();

    m_GCAccessor = new CNCBlobAccessor();

    m_BGThread = NewBGThread(this, &CNCBlobStorage::x_DoBackgroundWork);
    m_BGThread->Run();
}

CNCBlobStorage::~CNCBlobStorage(void)
{
    m_Stopped = true;
    // To be on a safe side let's post 100
    m_StopTrigger.Post(100);
    m_BGThread->Join();

    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        delete it->second;
    }
    m_DBFiles.clear();
    m_IndexDB.reset();

    x_UnlockInstanceGuard();
}

void
CNCBlobStorage::PackBlobKey(string*      packed_key,
                            CTempString  cache_name,
                            CTempString  blob_key,
                            CTempString  blob_subkey)
{
    packed_key->clear();
    packed_key->reserve(cache_name.size() + blob_key.size()
                        + blob_subkey.size() + 2);
    packed_key->append(cache_name.data(), cache_name.size());
    packed_key->append(1, '\1');
    packed_key->append(blob_key.data(), blob_key.size());
    packed_key->append(1, '\1');
    packed_key->append(blob_subkey.data(), blob_subkey.size());
}

void
CNCBlobStorage::UnpackBlobKey(const string& packed_key,
                              string&       cache_name,
                              string&       key,
                              string&       subkey)
{
    // cache
    const char* cache_str = packed_key.data();
    size_t cache_size = packed_key.find('\1', 0);
    cache_name.assign(cache_str, cache_size);
    // key
    const char* key_str = cache_str + cache_size + 1;
    size_t key_size = packed_key.find('\1', cache_size + 1)
                      - cache_size - 1;
    key.assign(key_str, key_size);
    // subkey
    const char* subkey_str = key_str + key_size + 1;
    size_t subkey_size = packed_key.size() - cache_size - key_size - 2;
    subkey.assign(subkey_str, subkey_size);
}

string
CNCBlobStorage::UnpackKeyForLogs(const string& packed_key)
{
    // cache
    const char* cache_name = packed_key.data();
    size_t cache_size = packed_key.find('\1', 0);
    // key
    const char* key = cache_name + cache_size + 1;
    size_t key_size = packed_key.find('\1', cache_size + 1) - cache_size - 1;
    // subkey
    const char* subkey = key + key_size + 1;
    size_t subkey_size = packed_key.size() - cache_size - key_size - 2;

    string result;
    result.append("'");
    result.append(key, key_size);
    result.append("'");
    if (cache_size != 0  ||  subkey_size != 0) {
        result.append(", '");
        result.append(subkey, subkey_size);
        result.append("' from cache '");
        result.append(cache_name, cache_size);
        result.append("'");
    }
    return result;
}

inline void
CNCBlobStorage::x_WaitForGC(void)
{
    while (m_GCInWork) {
        NCBI_SCHED_YIELD();
    }
}

void
CNCBlobStorage::Reconfigure(void)
{
    x_WaitForGC();

    CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();
    // Non-changeable parameters
    reg.Set(kNCStorage_RegSection, kNCStorage_PathParam, m_MainPath,
            IRegistry::fPersistent | IRegistry::fOverride,
            reg.GetComment(kNCStorage_RegSection, kNCStorage_PathParam));
    reg.Set(kNCStorage_RegSection, kNCStorage_FilePrefixParam, m_Prefix,
            IRegistry::fPersistent | IRegistry::fOverride,
            reg.GetComment(kNCStorage_RegSection, kNCStorage_FilePrefixParam));
    // Change all others
    x_ReadVariableParams();
}

void
CNCBlobStorage::ReinitializeCache(const string& cache_key)
{
/*
    _ASSERT(IsBlockActive());

    x_WaitForGC();
    m_SlotsCache.Clear();

    string max_key(cache_key);
    size_t ind = max_key.find('\1');
    max_key.resize(ind + 1);
    max_key[ind] = '\2';
    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        CNCDBFile* file = it->second.file_obj.get();
        if (file->GetType() == eNCMeta) {
            CNCDBMetaFile* metafile = static_cast<CNCDBMetaFile*>(file);
            try {
                metafile->DeleteAllBlobInfos(cache_key, max_key);
            }
            catch (CSQLITE_Exception& ex) {
                ERR_POST("Error cleaning cache in the file '" << file->GetFileName()
                         << "': " << ex);
            }
        }
        file->ResetBlobsCounts();
    }

    m_NotCachedFileId = m_LastFileId;
    m_CachingTrigger.Reset();
    _VERIFY(m_CachingTrigger.StartWorking(NULL) == eNCSuccessNoBlock);
    _ASSERT(m_CachingTrigger.GetState() == eNCOpInProgress);
*/
    abort();
}

void
CNCBlobStorage::x_CheckStopped(void)
{
    if (m_Stopped) {
        throw CNCStorage_StoppedException();
    }
    else if (m_Blocked) {
        throw CNCStorage_BlockedException();
    }
/*    else if (m_NeedRecache) {
        m_NeedRecache = false;
        throw CNCStorage_RecacheException();
    }*/
}

CNCBlobAccessor*
CNCBlobStorage::x_GetAccessor(bool* need_initialize)
{
    CSpinGuard guard(m_HoldersPoolLock);

    CNCBlobAccessor* holder = m_FreeAccessors;
    if (holder) {
        holder->RemoveFromList(m_FreeAccessors);
    }
    else {
        holder = new CNCBlobAccessor();
    }
    ++m_CntUsedHolders;

    *need_initialize = !m_Blocked;
    return holder;
}

void
CNCBlobStorage::ReturnAccessor(CNCBlobAccessor* holder)
{
    CSpinGuard guard(m_HoldersPoolLock);

    holder->AddToList(m_FreeAccessors);
    --m_CntUsedHolders;
    if (m_Blocked  &&  holder->IsInitialized())
        --m_CntLocksToWait;
}

CNCBlobStorage::SSlotCache*
CNCBlobStorage::x_GetSlotCache(Uint2 slot)
{
    m_BigCacheLock.ReadLock();
    TSlotCacheMap::const_iterator it = m_SlotsCache.find(slot);
    if (it != m_SlotsCache.end()) {
        SSlotCache* cache = it->second;
        m_BigCacheLock.ReadUnlock();
        return cache;
    }
    m_BigCacheLock.ReadUnlock();
    m_BigCacheLock.WriteLock();
    SSlotCache* cache = m_SlotsCache[slot];
    if (cache == NULL) {
        cache = new SSlotCache(slot);
        m_SlotsCache[slot] = cache;
    }
    m_BigCacheLock.WriteUnlock();
    return cache;
}

void
CNCBlobStorage::x_InitializeAccessor(CNCBlobAccessor* acessor)
{
    const string& key = acessor->GetBlobKey();
    Uint2 slot = acessor->GetBlobSlot();
    SSlotCache* cache = x_GetSlotCache(slot);
    SNCCacheData* data = NULL;
    if (acessor->GetAccessType() == eNCCreate
        ||  acessor->GetAccessType() == eNCCopyCreate)
    {
        /*
        data = new SNCCacheData();
        SNCCacheData* cache_data = NULL;
        if (!cache->key_map.PutOrGet(key, data,
                            TKeysCacheMap::eGetActiveAndPassive, cache_data))
        {
            delete data;
            data = cache_data;
        }
        */
        cache->lock.WriteLock();
        TNCBlobSumList::iterator it = cache->key_map.find(key);
        if (it == cache->key_map.end()) {
            data = new SNCCacheData();
            cache->key_map[key] = data;
        }
        else {
            data = it->second;
        }
        cache->lock.WriteUnlock();
        _ASSERT(data);
    }
/*    else if (!cache->key_map.Get(key, data)) {
        data = NULL;
    }*/
    else {
        cache->lock.ReadLock();
        TNCBlobSumList::const_iterator it = cache->key_map.find(key);
        if (it == cache->key_map.end()) {
            data = NULL;
        }
        else {
            data = it->second;
        }
        cache->lock.ReadUnlock();
    }
    acessor->Initialize(data);
}

CNCBlobAccessor*
CNCBlobStorage::GetBlobAccess(ENCAccessType access,
                              const string& key,
                              const string& password,
                              Uint2         slot)
{
    bool need_initialize;
    CNCBlobAccessor* accessor(x_GetAccessor(&need_initialize));
    accessor->Prepare(key, password, slot, access);
    if (need_initialize)
        x_InitializeAccessor(accessor);
    else {
        CSpinGuard guard(m_HoldersPoolLock);
        accessor->AddToList(m_UsedAccessors);
    }
    return accessor;
}

void
CNCBlobStorage::GetNewBlobCoords(SNCBlobCoords* coords)
{
    m_LastBlobLock.Lock();
    if ((++m_LastBlob.blob_id & kNCMaxBlobId) == 0)
        m_LastBlob.blob_id = 1;
    x_IncFilePartNum(m_LastBlob.meta_id);
    x_IncFilePartNum(m_LastBlob.data_id);
    coords->blob_id = m_LastBlob.blob_id;
    coords->meta_id = m_CurFiles[eNCMeta][m_LastBlob.meta_id];
    coords->data_id = m_CurFiles[eNCData][m_LastBlob.data_id];
    m_LastBlobLock.Unlock();

    m_DBFilesLock.ReadLock();
    SNCDBFileInfo* meta_info = m_DBFiles[coords->meta_id];
    SNCDBFileInfo* data_info = m_DBFiles[coords->data_id];
    m_DBFilesLock.ReadUnlock();

    meta_info->cnt_lock.Lock();
    ++meta_info->ref_cnt;
    meta_info->cnt_lock.Unlock();
    data_info->cnt_lock.Lock();
    ++data_info->ref_cnt;
    data_info->cnt_lock.Unlock();
}

void
CNCBlobStorage::DeleteBlobKey(Uint2 slot, const string& key)
{
    SSlotCache* cache = x_GetSlotCache(slot);
    //_VERIFY(cache->key_map.PassivateKey(key));
    /*cache->lock.WriteLock();
    cache->key_map.erase(key);
    cache->lock.WriteUnlock();*/
    cache->deleter.AddElement(key);
}

void
CNCBlobStorage::RestoreBlobKey(Uint2 slot,
                               const string& key,
                               SNCCacheData* cache_data)
{
    /*SSlotCache* cache = x_GetSlotCache(slot);
    //_VERIFY(cache->key_map.ActivateKey(key));
    cache->lock.WriteLock();
    cache->key_map[key] = cache_data;
    cache->lock.WriteUnlock();*/
}

bool
CNCBlobStorage::IsBlobExists(Uint2 slot, const string& key)
{
    SSlotCache* cache = x_GetSlotCache(slot);
    /*SNCCacheData* cache_data = NULL;
    return cache->key_map.Get(key, cache_data)
           &&  cache_data->blob_id != 0;*/
    CFastReadGuard guard(cache->lock);
    TNCBlobSumList::const_iterator it = cache->key_map.find(key);
    return it != cache->key_map.end()  &&  it->second->blob_id != 0;
}

bool
CNCBlobStorage::ReadBlobInfo(SNCBlobVerData* ver_data)
{
    _ASSERT(ver_data->coords.blob_id != 0);
    try {
        TMetaFileLock metafile(this, ver_data->coords.meta_id);
        if (metafile->ReadBlobInfo(ver_data))
            return true;
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(1, "Error reading blob information: " << ex);
    }
    return false;
}

void
CNCBlobStorage::WriteBlobInfo(const string& blob_key, SNCBlobVerData* ver_data)
{
    ver_data->old_dead_time = ver_data->dead_time;
    ver_data->generation    = m_BlobGeneration;
    if (ver_data->create_time == 0)
        abort();
    {{
        TMetaFileLock metafile(this, ver_data->coords.meta_id);
        metafile->WriteBlobInfo(blob_key, ver_data);
    }}
    m_DBFilesLock.ReadLock();
    SNCDBFileInfo* file_info = m_DBFiles[ver_data->coords.meta_id];
    m_DBFilesLock.ReadUnlock();

    CSpinGuard guard(file_info->cnt_lock);
    if (!ver_data->need_meta_blob)
        abort();
    ++file_info->useful_blobs;
    ver_data->need_meta_blob = false;
    if (file_info->ref_cnt < file_info->useful_blobs)
        abort();
}

bool
CNCBlobStorage::x_UpdBlobInfoNoMove(const string&   blob_key,
                                    SNCBlobVerData* ver_data)
{
    try {
        TMetaFileLock metafile(this, ver_data->coords.meta_id);
        metafile->UpdateBlobInfo(ver_data);
        return true;
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST("Cannot update blob's meta-information: " << ex);
    }
    return false;
}

bool
CNCBlobStorage::x_UpdBlobInfoSingleChunk(const string&   blob_key,
                                         SNCBlobVerData* ver_data)
{
    ver_data->generation = m_BlobGeneration;
    SNCBlobVerData new_ver(ver_data);
    GetNewBlobCoords(&new_ver.coords);
    m_DBFilesLock.ReadLock();
    SNCDBFileInfo* new_meta_info = m_DBFiles[new_ver.coords.meta_id];
    SNCDBFileInfo* new_data_info = m_DBFiles[new_ver.coords.data_id];
    SNCDBFileInfo* old_meta_info = m_DBFiles[ver_data->coords.meta_id];
    SNCDBFileInfo* old_data_info = m_DBFiles[ver_data->coords.data_id];
    m_DBFilesLock.ReadUnlock();
    try {
        TDataFileLock datafile(this, new_ver.coords.data_id);
        datafile->WriteChunkData(new_ver.coords.blob_id, ver_data->data);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST("Cannot move data for single-chunked blob: " << ex);
        new_data_info->cnt_lock.Lock();
        --new_data_info->ref_cnt;
        new_data_info->cnt_lock.Unlock();
        return false;
    }
    try {
        TMetaFileLock metafile(this, new_ver.coords.meta_id);
        metafile->WriteBlobInfo(blob_key, &new_ver);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST("Cannot move meta-data for single-chunked blob: " << ex);
        new_meta_info->cnt_lock.Lock();
        --new_meta_info->ref_cnt;
        new_meta_info->cnt_lock.Unlock();
        new_data_info->cnt_lock.Lock();
        ++new_data_info->garbage_blobs;
        new_data_info->garbage_size += ver_data->size;
        new_data_info->cnt_lock.Unlock();
        return false;
    }
    new_data_info->cnt_lock.Lock();
    ++new_data_info->useful_blobs;
    new_data_info->useful_size += ver_data->size;
    new_data_info->cnt_lock.Unlock();
    new_meta_info->cnt_lock.Lock();
    ++new_meta_info->useful_blobs;
    new_meta_info->cnt_lock.Unlock();

    swap(ver_data->coords, new_ver.coords);
    try {
        TMetaFileLock metafile(this, new_ver.coords.meta_id);
        metafile->DeleteBlobInfo(new_ver.coords.blob_id);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST("Cannot delete old blob's meta-information: " << ex);
    }
    old_meta_info->cnt_lock.Lock();
    if (--old_meta_info->useful_blobs + 1 == 0)
        abort();
    --old_meta_info->ref_cnt;
    old_meta_info->cnt_lock.Unlock();
    old_data_info->cnt_lock.Lock();
    if (--old_data_info->useful_blobs + 1 == 0)
        abort();
    ++old_data_info->garbage_blobs;
    old_data_info->useful_size -= ver_data->size;
    old_data_info->garbage_size += ver_data->size;
    --old_data_info->ref_cnt;
    old_data_info->cnt_lock.Unlock();
    return true;
}

bool
CNCBlobStorage::x_UpdBlobInfoMultiChunk(const string&   blob_key,
                                        SNCBlobVerData* ver_data)
{
    ver_data->generation = m_BlobGeneration;
    SNCBlobVerData new_ver(ver_data);
    GetNewBlobCoords(&new_ver.coords);
    CNCDBDataFile* old_datafile;
    CNCDBDataFile* new_datafile;
    m_DBFilesLock.ReadLock();
    SNCDBFileInfo* new_meta_info = m_DBFiles[new_ver.coords.meta_id];
    SNCDBFileInfo* new_data_info = m_DBFiles[new_ver.coords.data_id];
    SNCDBFileInfo* old_meta_info = m_DBFiles[ver_data->coords.meta_id];
    SNCDBFileInfo* old_data_info = m_DBFiles[ver_data->coords.data_id];
    m_DBFilesLock.ReadUnlock();
    old_datafile = (CNCDBDataFile*)old_data_info->file_obj.get();
    new_datafile = (CNCDBDataFile*)new_data_info->file_obj.get();
    Uint8 data_written = 0;
    try {
        CNCBlobBuffer buffer;
        for (size_t i = 0; i < ver_data->chunks.size(); ++i) {
            TNCChunkId chunk_id = ver_data->chunks[i];
            {{
                TDataFileLock file_lock(old_datafile);
                old_datafile->ReadChunkData(chunk_id, &buffer);
            }}
            {{
                TDataFileLock file_lock(new_datafile);
                new_datafile->WriteChunkData(chunk_id, &buffer);
            }}
            data_written += buffer.GetSize();
        }
        {{
            TMetaFileLock metafile(this, new_ver.coords.meta_id);
            for (size_t i = 0; i < ver_data->chunks.size(); ++i) {
                metafile->CreateChunk(new_ver.coords.blob_id, ver_data->chunks[i]);
            }
            metafile->WriteBlobInfo(blob_key, &new_ver);
        }}
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST("Cannot move multi-chunked blob: " << ex);
        new_meta_info->cnt_lock.Lock();
        --new_meta_info->ref_cnt;
        new_meta_info->cnt_lock.Unlock();
        new_data_info->cnt_lock.Lock();
        ++new_data_info->garbage_blobs;
        new_data_info->garbage_size += data_written;
        --new_data_info->ref_cnt;
        new_data_info->cnt_lock.Unlock();
        return false;
    }
    new_data_info->cnt_lock.Lock();
    ++new_data_info->useful_blobs;
    new_data_info->useful_size += ver_data->size;
    new_data_info->cnt_lock.Unlock();
    new_meta_info->cnt_lock.Lock();
    ++new_meta_info->useful_blobs;
    new_meta_info->cnt_lock.Unlock();

    swap(ver_data->coords, new_ver.coords);
    try {
        TMetaFileLock metafile(this, new_ver.coords.meta_id);
        metafile->DeleteBlobInfo(new_ver.coords.blob_id);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST("Cannot delete old blob's meta-information: " << ex);
    }
    old_meta_info->cnt_lock.Lock();
    if (--old_meta_info->useful_blobs + 1 == 0)
        abort();
    ++old_meta_info->garbage_blobs;
    --old_meta_info->ref_cnt;
    old_meta_info->cnt_lock.Unlock();
    old_data_info->cnt_lock.Lock();
    if (--old_data_info->useful_blobs + 1 == 0)
        abort();
    ++old_data_info->garbage_blobs;
    old_data_info->useful_size -= ver_data->size;
    old_data_info->garbage_size += ver_data->size;
    --old_data_info->ref_cnt;
    old_data_info->cnt_lock.Unlock();
    return true;
}

bool
CNCBlobStorage::UpdateBlobInfo(const string&   blob_key,
                               SNCBlobVerData* ver_data)
{
    if (m_BlobGeneration - ver_data->generation <= m_CurFiles[eNCMeta].size()) {
        return x_UpdBlobInfoNoMove(blob_key, ver_data);
    }
    else if (ver_data->data) {
        return x_UpdBlobInfoSingleChunk(blob_key, ver_data);
    }
    else if (ver_data->data_trigger.GetState() != eNCOpCompleted
             ||  ver_data->chunks.size() == 0)
    {
        return x_UpdBlobInfoNoMove(blob_key, ver_data);
    }
    else {
        return x_UpdBlobInfoMultiChunk(blob_key, ver_data);
    }
}

void
CNCBlobStorage::DeleteBlobInfo(const SNCBlobVerData* ver_data)
{
    if (!ver_data->need_meta_blob) {
        if (ver_data->create_time == 0)
            abort();
        TMetaFileLock metafile(this, ver_data->coords.meta_id);
        try {
            metafile->DeleteBlobInfo(ver_data->coords.blob_id);
        }
        catch (CException& ex) {
            ERR_POST("Error deleting blob from file "
                     << metafile->GetFileName() << ": " << ex);
        }
    }
    m_DBFilesLock.ReadLock();
    SNCDBFileInfo* meta_info = m_DBFiles[ver_data->coords.meta_id];
    SNCDBFileInfo* data_info = m_DBFiles[ver_data->coords.data_id];
    m_DBFilesLock.ReadUnlock();
    meta_info->cnt_lock.Lock();
    if (!ver_data->need_meta_blob) {
        if (ver_data->create_time == 0)
            abort();
        if (--meta_info->useful_blobs + 1 == 0)
            abort();
    }
    --meta_info->ref_cnt;
    meta_info->cnt_lock.Unlock();
    data_info->cnt_lock.Lock();
    if (!ver_data->need_data_blob  &&  ver_data->size != 0) {
        if (ver_data->disk_size == 0)
            abort();
        if (--data_info->useful_blobs + 1 == 0)
            abort();
        ++data_info->garbage_blobs;
    }
    data_info->useful_size -= ver_data->disk_size;
    data_info->garbage_size += ver_data->disk_size;
    --data_info->ref_cnt;
    data_info->cnt_lock.Unlock();
}

void
CNCBlobStorage::ReadChunkIds(SNCBlobVerData* ver_data)
{
    TMetaFileLock metafile(this, ver_data->coords.meta_id);
    try {
        metafile->GetChunkIds(ver_data->coords.blob_id, &ver_data->chunks);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(1, "Error reading chunk ids from file "
                      << metafile->GetFileName() << ": " << ex);
    }
}

bool
CNCBlobStorage::ReadChunkData(TNCDBFileId    file_id,
                              TNCChunkId     chunk_id,
                              CNCBlobBuffer* buffer)
{
    TDataFileLock datafile(this, file_id);
    try {
        if (datafile->ReadChunkData(chunk_id, buffer)) {
            CNCStat::AddChunkRead(buffer->GetSize());
            return true;
        }
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(1, "Error reading chunk data from file "
                      << datafile->GetFileName() << ": " << ex);
    }
    return false;
}

void
CNCBlobStorage::x_WriteChunkData(TNCDBFileId     file_id,
                                 TNCChunkId      chunk_id,
                                 const CNCBlobBuffer* data,
                                 SNCBlobVerData* ver_data,
                                 bool            add_blobs_cnt)
{
    {{
        TDataFileLock datafile(this, file_id);
        datafile->WriteChunkData(chunk_id, data);
    }}
    ver_data->disk_size += data->GetSize();
    CNCStat::AddChunkWritten(data->GetSize());

    m_DBFilesLock.ReadLock();
    SNCDBFileInfo* file_info = m_DBFiles[file_id];
    m_DBFilesLock.ReadUnlock();

    CSpinGuard guard(file_info->cnt_lock);
    if (add_blobs_cnt) {
        if (!ver_data->need_data_blob)
            abort();
        ++file_info->useful_blobs;
        ver_data->need_data_blob = false;
        if (file_info->ref_cnt < file_info->useful_blobs)
            abort();
    }
    file_info->useful_size += data->GetSize();
}

struct SNCTempBlobInfo
{
    string  key;
    Uint8   create_time;
    Uint8   create_server;
    TNCBlobId create_id;
    int     dead_time;
    int     ver_expire;

    SNCTempBlobInfo(const string& blob_key, SNCCacheData* cache_info)
        : key(blob_key)
    {
        create_time = cache_info->create_time;
        create_server = cache_info->create_server;
        create_id = cache_info->create_id;
        dead_time = cache_info->dead_time;
        ver_expire = cache_info->ver_expire;
    }
};

void
CNCBlobStorage::GetFullBlobsList(Uint2 slot, TNCBlobSumList& blobs_lst)
{
    blobs_lst.clear();
    SSlotCache* cache = x_GetSlotCache(slot);
    //cache->key_map.GetContents(blobs_lst, TKeysCacheMap::eGetOnlyActive);

    cache->lock.WriteLock();

    Uint8 cnt_blobs = cache->key_map.size();
    void* big_block = malloc(size_t(cnt_blobs * sizeof(SNCTempBlobInfo)));
    SNCTempBlobInfo* info_ptr = (SNCTempBlobInfo*)big_block;

    ITERATE(TNCBlobSumList, it, cache->key_map) {
        new (info_ptr) SNCTempBlobInfo(it->first, it->second);
        ++info_ptr;
    }

    cache->lock.WriteUnlock();

    info_ptr = (SNCTempBlobInfo*)big_block;
    for (Uint8 i = 0; i < cnt_blobs; ++i) {
        SNCCacheData* blob_sum = new SNCCacheData();
        blob_sum->create_id = info_ptr->create_id;
        blob_sum->create_server = info_ptr->create_server;
        blob_sum->create_time = info_ptr->create_time;
        blob_sum->dead_time = info_ptr->dead_time;
        blob_sum->ver_expire = info_ptr->ver_expire;
        blobs_lst[info_ptr->key] = blob_sum;
        info_ptr->~SNCTempBlobInfo();
        ++info_ptr;
    }

    free(big_block);
}

void
CNCBlobStorage::PrintStat(CPrintTextProxy& proxy)
{
    Int8 size_meta = 0, size_data = 0;
    unsigned int num_meta = 0, num_data = 0;
    Uint8 useful_cnt_meta = 0, garbage_cnt_meta = 0;
    Uint8 useful_cnt_data = 0, garbage_cnt_data = 0;
    Int8 useful_size_data = 0, garbage_size_data = 0;
    {{
        CSpinReadGuard guard(m_DBFilesLock);

        ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            SNCDBFileInfo* file_info = it->second;
            CNCDBFile* file = file_info->file_obj.get();
            Int8 size = file->GetFileSize();
            file_info->cnt_lock.Lock();
            Uint8 useful_cnt = file_info->useful_blobs;
            Uint8 useful_size = file_info->useful_size;
            Uint8 garbage_cnt = file_info->garbage_blobs;
            Uint8 garbage_size = file_info->garbage_size;
            file_info->cnt_lock.Unlock();
            if (file->GetType() == eNCMeta) {
                size_meta += size;
                useful_cnt_meta += useful_cnt;
                garbage_cnt_meta += garbage_cnt;
                ++num_meta;
            }
            else {
                size_data += size;
                useful_cnt_data += useful_cnt;
                garbage_cnt_data += garbage_cnt;
                useful_size_data += useful_size;
                garbage_size_data += garbage_size;
                ++num_data;
            }
        }
    }}
/*
    proxy << "Now in cache - " << m_SlotsCache.CountValues() << " values, "
                               << m_SlotsCache.CountNodes()  << " nodes, "
                               << m_SlotsCache.TreeHeight()  << " height" << endl;
*/
    proxy << "Now in db    - "
                    << (size_meta + size_data) << " bytes, "
                    << num_meta << " metas of "
                    << size_meta << " bytes ("
                    << size_meta / num_meta << " avg), "
                    << num_data << " datas of "
                    << size_data << " bytes ("
                    << size_data / num_data << " avg)" << endl
          << "Metas blobs  - useful: "
                    << useful_cnt_meta << " cnt ("
                    << useful_cnt_meta / num_meta << " avg); garbage: "
                    << garbage_cnt_meta << " cnt ("
                    << garbage_cnt_meta / num_meta << " avg)" << endl
          << "Datas blobs  - useful: "
                    << useful_cnt_data << " cnt ("
                    << useful_cnt_data / num_data << " avg) of "
                    << useful_size_data << " bytes ("
                    << useful_size_data / num_data << " avg); garbage: "
                    << garbage_cnt_data << " cnt ("
                    << garbage_cnt_data / num_data << " avg) of "
                    << garbage_size_data << " bytes ("
                    << garbage_size_data / num_data << " avg)" << endl;
}

void
CNCBlobStorage::OnBlockedOpFinish(void)
{
    m_GCBlockWaiter.Post();
}

void
CNCBlobStorage::x_GC_DeleteExpired(const SNCBlobListInfo& blob_info,
                                   int dead_before)
{
    m_GCAccessor->Prepare(blob_info.key, kEmptyStr, blob_info.slot, eNCGCDelete);
    x_InitializeAccessor(m_GCAccessor);
    while (m_GCAccessor->ObtainMetaInfo(this) == eNCWouldBlock) {
        m_GCBlockWaiter.Wait();
    }
    if (m_GCAccessor->IsBlobExists()
        &&  m_GCAccessor->GetCurBlobDeadTime() < dead_before)
    {
        m_GCAccessor->DeleteBlob();
        ++m_GCDeleted;
    }
    else
        abort();
    m_GCAccessor->Deinitialize();
}

void
CNCBlobStorage::x_GC_CleanDBFile(CNCDBFile* metafile, int dead_before)
{
    try {
        TNCBlobsList blobs_list;
        int dead_after = m_LastDeadTime;
        TNCBlobId last_id = 0;
        do {
            x_CheckStopped();
            {{
                TMetaFileLock file_lock(metafile);
                file_lock->GetBlobsList(dead_after, last_id, dead_before,
                                        m_GCBatchSize, &blobs_list);
            }}
            ITERATE(TNCBlobsList, it, blobs_list) {
                ++m_GCRead;
                x_CheckStopped();
                x_GC_DeleteExpired(*it->get(), dead_before);
                // Avoid this thread to be the most foreground task to execute,
                // let's give others a chance to work.
                NCBI_SCHED_YIELD();
            }
        }
        while (!blobs_list.empty());
    }
    catch (CException& ex) {
        ERR_POST("Error processing file '" << metafile->GetFileName()
                 << "': " << ex);
    }
}

void
CNCBlobStorage::x_GC_CollectFilesStats(void)
{
/*
    CNCStat::AddCacheMeasurement(m_SlotsCache.CountValues(),
                                 m_SlotsCache.CountNodes(),
                                 m_SlotsCache.TreeHeight());
*/
    // Nobody else changes m_DBParts, so working with it right away without
    // any mutexes.
    Int8 size_meta = 0, size_data = 0;
    unsigned int num_meta = 0, num_data = 0;
    Uint8 useful_cnt_meta = 0, garbage_cnt_meta = 0;
    Uint8 useful_cnt_data = 0, garbage_cnt_data = 0;
    Int8 useful_size_data = 0, garbage_size_data = 0;
    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        SNCDBFileInfo* file_info = it->second;
        CNCDBFile* file = file_info->file_obj.get();
        Int8 size = file->GetFileSize();
        file_info->cnt_lock.Lock();
        Uint8 useful_cnt = file_info->useful_blobs;
        Uint8 useful_size = file_info->useful_size;
        Uint8 garbage_cnt = file_info->garbage_blobs;
        Uint8 garbage_size = file_info->garbage_size;
        file_info->cnt_lock.Unlock();
        if (file->GetType() == eNCMeta) {
            size_meta += size;
            useful_cnt_meta += useful_cnt;
            garbage_cnt_meta += garbage_cnt;
            ++num_meta;
            CNCStat::AddMetaFileMeasurement(size, useful_cnt, garbage_cnt);
        }
        else {
            size_data += size;
            useful_cnt_data += useful_cnt;
            garbage_cnt_data += garbage_cnt;
            useful_size_data += useful_size;
            garbage_size_data += garbage_size;
            ++num_data;
            CNCStat::AddDataFileMeasurement(size, useful_cnt, garbage_cnt,
                                            useful_size, garbage_size);
        }
    }
    CNCStat::AddDBMeasurement(num_meta, num_data, size_meta, size_data,
                              useful_cnt_meta, garbage_cnt_meta,
                              useful_cnt_data, garbage_cnt_data,
                              useful_size_data, garbage_size_data);
}

void
CNCBlobStorage::x_GC_HeartBeat(void)
{
    {{
        CFastReadGuard guard(m_BigCacheLock);
        ITERATE(TSlotCacheMap, it, m_SlotsCache) {
            SSlotCache* cache = it->second;
            //cache->key_map.HeartBeat();
            cache->deleter.HeartBeat();
        }
    }}

    bool next_gen = false;
    for (unsigned int i = 0; i < m_CurFiles[eNCMeta].size(); ++i) {
        TNCDBFileId file_id = m_CurFiles[eNCMeta][i];
        SNCDBFileInfo* file_info = m_DBFiles[file_id];
        CNCDBFile* file = file_info->file_obj.get();
        file_info->cnt_lock.Lock();
        Uint8 useful_cnt = file_info->useful_blobs;
        Uint8 garbage_cnt = file_info->garbage_blobs;
        file_info->cnt_lock.Unlock();
        Uint8 total = useful_cnt + garbage_cnt;
        Int8  size  = file->GetFileSize();
        if ((size >= m_MaxFileSize[eNCMeta]
             ||  (size >= m_MinFileSize[eNCMeta]  &&  total != 0
                  &&  useful_cnt / total <= m_MinUsefulPct[eNCMeta])))
        {
            x_CreateNewFile(eNCMeta, i);
            next_gen = true;
        }
    }
    for (unsigned int i = 0; i < m_CurFiles[eNCData].size(); ++i) {
        TNCDBFileId file_id = m_CurFiles[eNCData][i];
        SNCDBFileInfo* file_info = m_DBFiles[file_id];
        CNCDBFile* file = file_info->file_obj.get();
        file_info->cnt_lock.Lock();
        Uint8 useful_size = file_info->useful_size;
        Uint8 garbage_size = file_info->garbage_size;
        file_info->cnt_lock.Unlock();
        Int8 total_blobs_size = useful_size + garbage_size;
        Int8 file_size  = file->GetFileSize();
        if ((file_size >= m_MaxFileSize[eNCData]
             ||  (file_size >= m_MinFileSize[eNCData]  &&  total_blobs_size != 0
                  &&  useful_size / total_blobs_size <= m_MinUsefulPct[eNCData])))
        {
            x_CreateNewFile(eNCData, i);
            next_gen = true;
        }
    }
    if (next_gen)
        ++m_BlobGeneration;

    Uint8 db_size = 0;
    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        SNCDBFileInfo* file_info = it->second;
        CNCDBFile* file = file_info->file_obj.get();
        db_size += file->GetFileSize();
    }
    m_CurDBSize = db_size;
    if (!m_IsStopWrite) {
        if (m_StopWriteOnSize != 0  &&  db_size >= m_StopWriteOnSize)
            m_IsStopWrite = true;
    }
    else if (db_size <= m_StopWriteOffSize) {
        m_IsStopWrite = false;
    }
    if (CFileUtil::GetFreeDiskSpace(m_MainPath) <= m_DiskFreeLimit)
        m_IsStopWrite = true;
}

bool
CNCBlobStorage::x_CheckFileAging(TNCDBFileId file_id, CNCDBFile* file)
{
    ENCDBFileType file_type = file->GetType();
    const vector<TNCDBFileId>& files_list = m_CurFiles[file_type];
    unsigned int part_num = 0;
    for (; part_num < files_list.size(); ++part_num) {
        if (files_list[part_num] == file_id)
            break;
    }
    if (part_num != files_list.size()) {
        return false;
    }
    m_DBFilesLock.ReadLock();
    SNCDBFileInfo* file_info = m_DBFiles[file_id];
    m_DBFilesLock.ReadUnlock();
    CSpinGuard guard(file_info->cnt_lock);
    if (file_info->ref_cnt == 0  &&  file_info->useful_blobs != 0)
        abort();
    return file_info->ref_cnt == 0;
}

void
CNCBlobStorage::x_DoBackgroundWork(void)
{
    // Make background postings to be all in the same request id
    CRef<CRequestContext> diag_context(new CRequestContext());
    diag_context->SetRequestID();
    CDiagContext::SetRequestContext(diag_context);

    try {
        for (;;) {
            try {
                INFO_POST("Caching of blobs info is started");
                // Nobody else changes m_DBFiles, so iterating right over it.
                //    * With REVERSE_ITERATE code should look more ugly to be
                //    * compilable in WorkShop.
                NON_CONST_REVERSE_ITERATE(TNCDBFilesMap, file_it, m_DBFiles) {
                    SNCDBFileInfo* file_info = file_it->second;
                    CNCDBFile* file = file_info->file_obj.get();
                    if (file->GetType() != eNCMeta)
                        continue;
                    TNCDBFileId file_id = file_it->first;

                    Uint4 cnt_blobs = 0;
                    try {
                        TNCBlobsList blobs_list;
                        TNCBlobId last_id = 0;
                        int dead_after = m_LastDeadTime;
                        do {
                            x_CheckStopped();
                            {{
                                TMetaFileLock file_lock(file);
                                file_lock->GetBlobsList(dead_after, last_id,
                                                        numeric_limits<int>::max(),
                                                        m_GCBatchSize, &blobs_list);
                            }}
                            ITERATE(TNCBlobsList, blob_it, blobs_list) {
                                SNCBlobListInfo* blob_info = blob_it->get();
                                blob_info->meta_id = file_id;
                                SNCCacheData* new_data = new SNCCacheData(blob_info);
                                SNCCacheData* cache_data;
                                SSlotCache* cache = m_SlotsCache[blob_info->slot];
                                if (!cache) {
                                    cache = new SSlotCache(blob_info->slot);
                                    m_SlotsCache[blob_info->slot] = cache;
                                }
                                //TKeysCacheMap* key_map = &cache->key_map;
                                //bool was_added = key_map->PutOrGet(blob_info->key, new_data, TKeysCacheMap::eGetActiveAndPassive, cache_data);
                                TNCBlobSumList* key_map = &cache->key_map;
                                TNCBlobSumList::iterator key_it = key_map->find(blob_info->key);
                                //if (!was_added) {
                                bool was_added = false;
                                if (key_it != key_map->end()) {
                                    cache_data = key_it->second;
                                    if (cache_data->isOlder(*new_data)) {
                                        //key_map->Put(blob_info->key, new_data);
                                        key_it->second = new_data;
                                        was_added = true;
                                        SNCDBFileInfo* cache_d_info = m_DBFiles[cache_data->data_id];
                                        cache_d_info->useful_size -= cache_data->size;
                                        --cache_d_info->useful_blobs;
                                        --cache_d_info->ref_cnt;
                                        if (cache_data->meta_id == file_id)
                                            --cnt_blobs;
                                        else {
                                            SNCDBFileInfo* cache_m_info = m_DBFiles[cache_data->meta_id];
                                            --cache_m_info->useful_blobs;
                                            --cache_m_info->ref_cnt;
                                        }
                                        delete cache_data;
                                    }
                                    else {
                                        delete new_data;
                                    }
                                }
                                else {
                                    (*key_map)[blob_info->key] = new_data;
                                    was_added = true;
                                }
                                if (was_added) {
                                    ++cnt_blobs;
                                    SNCDBFileInfo* data_info = m_DBFiles[blob_info->data_id];
                                    ++data_info->useful_blobs;
                                    data_info->useful_size += blob_info->size;
                                    ++data_info->ref_cnt;
                                }
                            }
                        }
                        while (!blobs_list.empty());
                    }
                    catch (CException& ex) {
                        // Try to recover from any database errors by just
                        // ignoring it and trying to work further.
                        ERR_POST("Database file " << file_it->second->file_name
                                 << " was not cached properly: " << ex);
                    }
                    file_info->useful_blobs = cnt_blobs;
                    file_info->ref_cnt = cnt_blobs;
                }

                INFO_POST("Caching of blobs info is finished");
                CNetCacheServer::CachingCompleted();

                bool do_extra_gc = false;
                Uint4 extra_gc_time = 0;
                for (;;) {
                    time_t start_time = time(NULL);
                    do {
                        m_StopTrigger.TryWait(1, 0);
                        x_GC_HeartBeat();
                        try {
                            x_CheckStopped();
                        }
                        catch (CNCStorage_BlockedException&) {
                            // If storage is blocked heart beat still can work
                        }
                    }
                    while (time(NULL) - start_time < m_GCRunDelay);

                    m_GCInWork = true;

                    int next_dead = int(time(NULL));
                    if (do_extra_gc) {
                        if (m_CurDBSize <= m_ExtraGCOffSize) {
                            do_extra_gc = false;
                        }
                        else {
                            extra_gc_time += m_ExtraGCStep;
                            next_dead += extra_gc_time;
                        }
                    }
                    else if (m_ExtraGCOnSize != 0
                             &&  m_CurDBSize >= m_ExtraGCOnSize)
                    {
                        do_extra_gc = true;
                        extra_gc_time = m_ExtraGCStep;
                        next_dead += extra_gc_time;
                    }

                    CRef<CRequestContext> ctx(new CRequestContext());
                    ctx->SetRequestID();
                    GetDiagContext().SetRequestContext(ctx);
                    if (g_NetcacheServer->IsLogCmds()) {
                        CDiagContext_Extra extra = GetDiagContext().PrintRequestStart();
                        extra.Print("_type", "gc");
                        if (do_extra_gc)
                            extra.Print("force_delete", extra_gc_time);
                        extra.Flush();
                    }
                    ctx->SetRequestStatus(CNCMessageHandler::eStatus_OK);
                    m_GCRead = m_GCDeleted = 0;

                    try {
                        x_CheckStopped();

                        x_GC_CollectFilesStats();

                        ERASE_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
                            CNCDBFile* file = it->second->file_obj.get();
                            if (file->GetType() == eNCMeta) {
                                x_GC_CleanDBFile(file, next_dead);
                            }
                            if (x_CheckFileAging(it->second->file_id, file)) {
                                x_DeleteDBFile(it);
                            }
                        }
                        m_LastDeadTime = next_dead;
                    }
                    catch (CNCStorage_BlockedException&) {
                        // Do nothing - we're already in place where we want to be
                        INFO_POST("Storage block is in progress");
                    }

                    ctx->SetBytesRd(Int8(m_GCRead));
                    ctx->SetBytesWr(Int8(m_GCDeleted));
                    if (g_NetcacheServer->IsLogCmds()) {
                        GetDiagContext().PrintRequestStop();
                    }
                    ctx.Reset();
                    GetDiagContext().SetRequestContext(NULL);

                    m_GCInWork = false;
                }
            }
            catch (CNCStorage_RecacheException&) {
                // Do nothing - we're already in place where we want to be
            }
        }
    }
    catch (CNCStorage_StoppedException&) {
        // Do nothing - we're already in place where we want to be
    }
}


CNCBlobStorage::SSlotCache::SSlotCache(Uint2 slot)
    : deleter(CKeysCleaner(slot))
{}


CNCBlobStorage::CKeysCleaner::CKeysCleaner(Uint2 slot)
    : m_Slot(slot)
{}

void
CNCBlobStorage::CKeysCleaner::Delete(const string& key) const
{
    SSlotCache* cache = g_NCStorage->x_GetSlotCache(m_Slot);
    //cache->key_map.EraseIfPassive(key);
    cache->lock.WriteLock();
    TNCBlobSumList::iterator it = cache->key_map.find(key);
    if (it != cache->key_map.end()) {
        SNCCacheData* cache_data = it->second;
        if (cache_data->is_deleted) {
            delete cache_data;
            cache->key_map.erase(it);
        }
    }
    cache->lock.WriteUnlock();
}

END_NCBI_SCOPE
